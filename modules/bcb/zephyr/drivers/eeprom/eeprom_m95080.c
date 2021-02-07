#define DT_DRV_COMPAT st_m95080

#include <drivers/eeprom.h>
#include <drivers/gpio.h>
#include <drivers/spi.h>
#include <zephyr.h>

#define LOG_LEVEL CONFIG_EEPROM_M95080_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(eeprom_m95080);

/* M95080 instruction set */
#define EEPROM_M95080_WREN	0x06U	/* Write Enable */
#define EEPROM_M95080_WRDI	0x04U	/* Write Disable */
#define EEPROM_M95080_RDSR	0x05U	/* Read Status Register */
#define EEPROM_M95080_WRSR	0x01U	/* Write Status Register */
#define EEPROM_M95080_READ	0x03U	/* Read from Memory Array */
#define EEPROM_M95080_WRITE	0x02U	/* Write to Memory Array */

/* Only for the M95080-D device */
#define EEPROM_M95080_RDID	0x83U	/* Read Identification Page */
#define EEPROM_M95080_WRID	0x82U	/* Write Identification Page */
#define EEPROM_M95080_RDLS	0x83U	/* Reads the Identification Page lock status */
#define EEPROM_M95080_LID	0x82U	/* Locks the Identification page in read-only mode */

/* M95080 status register bits */
#define EEPROM_M95080_STATUS_WIP	BIT(0) /* Write-In-Process   (RO) */
#define EEPROM_M95080_STATUS_WEL	BIT(1) /* Write Enable Latch (RO) */
#define EEPROM_M95080_STATUS_BP0	BIT(2) /* Block Protection 0 (RW) */
#define EEPROM_M95080_STATUS_BP1	BIT(3) /* Block Protection 1 (RW) */

struct eeprom_m95080_config {
	const char *spi_dev_name;
	const char *cs_dev_name;
	const char *wp_dev_name;
	uint32_t frequency;
	uint16_t bus_address;
	int cs_pin;
	int wp_pin;
	gpio_dt_flags_t cs_pin_flags;
	gpio_dt_flags_t wp_pin_flags;
	size_t size;
	size_t pagesize;
	uint8_t address_width;
	uint16_t timeout;
	bool readonly;
};

struct eeprom_m95080_data {
	struct device *spi_dev;
	struct spi_config spi_cfg;
	struct spi_cs_control spi_cs;
	struct device *wp_dev;
	struct k_mutex lock;
};

static int eeprom_m95080_rdsr(const struct device *dev, uint8_t *status)
{
	struct eeprom_m95080_data *data = dev->driver_data;
	uint8_t rdsr[2] = { EEPROM_M95080_RDSR, 0 };
	uint8_t sr[2];
	int err;
	const struct spi_buf tx_buf = {
		.buf = rdsr,
		.len = sizeof(rdsr),
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1,
	};
	const struct spi_buf rx_buf = {
		.buf = sr,
		.len = sizeof(sr),
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 1,
	};

	err = spi_transceive(data->spi_dev, &data->spi_cfg, &tx, &rx);
	if (!err) {
		*status = sr[1];
	}

	return err;
}

static int eeprom_m95080_wait_for_idle(const struct device *dev)
{
	const struct eeprom_m95080_config *config = dev->config_info;
	int64_t timeout;
	uint8_t status;
	int err;

	timeout = k_uptime_get() + config->timeout;
	while (1) {
		int64_t now = k_uptime_get();
		err = eeprom_m95080_rdsr(dev, &status);
		if (err) {
			LOG_ERR("Could not read status register (err %d)", err);
			return err;
		}

		if (!(status & EEPROM_M95080_STATUS_WIP)) {
			return 0;
		}
		if (now > timeout) {
			break;
		}
		k_sleep(K_MSEC(1));
	}

	return -EBUSY;
}

static inline int eeprom_m95080_write_enable(const struct device *dev, bool enable)
{
	const struct eeprom_m95080_config *config = dev->config_info;
	struct eeprom_m95080_data *data = dev->driver_data;

	if (!data->wp_dev) {
		return 0;
	}

	return gpio_pin_set(data->wp_dev, config->wp_pin, enable ? 0 : 1);
}

static inline int eeprom_m95080_read_spi(const struct device *dev, off_t offset, void *buf,
					 size_t len)
{
	const struct eeprom_m95080_config *config = dev->config_info;
	struct eeprom_m95080_data *data = dev->driver_data;
	size_t cmd_len = 1 + config->address_width / 8;
	uint8_t cmd[4] = { EEPROM_M95080_READ, 0, 0, 0 };
	uint8_t *paddr;
	int err;
	const struct spi_buf tx_buf = {
		.buf = cmd,
		.len = cmd_len,
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1,
	};
	const struct spi_buf rx_bufs[2] = {
		{
			.buf = NULL,
			.len = cmd_len,
		},
		{
			.buf = buf,
			.len = len,
		},
	};
	const struct spi_buf_set rx = {
		.buffers = rx_bufs,
		.count = ARRAY_SIZE(rx_bufs),
	};

	if (!len) {
		return 0;
	}

	if ((offset + len) > config->size) {
		LOG_WRN("attempt to read past device boundary");
		return -EINVAL;
	}

	paddr = &cmd[1];
	switch (config->address_width) {
	case 24:
		*paddr++ = offset >> 16;
	case 16:
		*paddr++ = offset >> 8;
	case 8:
		*paddr++ = offset;
		break;
	default:
		__ASSERT(0, "invalid address width");
	}

	err = eeprom_m95080_wait_for_idle(dev);
	if (err) {
		LOG_ERR("EEPROM idle wait failed (err %d)", err);
		k_mutex_unlock(&data->lock);
		return err;
	}

	err = spi_transceive(data->spi_dev, &data->spi_cfg, &tx, &rx);
	if (err < 0) {
		return err;
	}

	return len;
}

static inline int eeprom_m95080_wren(const struct device *dev)
{
	struct eeprom_m95080_data *data = dev->driver_data;
	uint8_t cmd = EEPROM_M95080_WREN;
	const struct spi_buf tx_buf = {
		.buf = &cmd,
		.len = 1,
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1,
	};

	return spi_write(data->spi_dev, &data->spi_cfg, &tx);
}

static inline size_t eeprom_m95080_limit_write_count(const struct device *dev, off_t offset,
						     size_t len)
{
	const struct eeprom_m95080_config *config = dev->config_info;
	size_t count = len;
	off_t page_boundary;

	/* We can at most write one page at a time */
	if (count > config->pagesize) {
		count = config->pagesize;
	}

	/* Writes can not cross a page boundary */
	page_boundary = ROUND_UP(offset + 1, config->pagesize);
	if (offset + count > page_boundary) {
		count = page_boundary - offset;
	}

	return count;
}

static inline int eeprom_m95080_write_spi(const struct device *dev, off_t offset, const void *buf,
					  size_t len)
{
	const struct eeprom_m95080_config *config = dev->config_info;
	struct eeprom_m95080_data *data = dev->driver_data;
	int count = eeprom_m95080_limit_write_count(dev, offset, len);
	uint8_t cmd[4] = { EEPROM_M95080_WRITE, 0, 0, 0 };
	size_t cmd_len = 1 + config->address_width / 8;
	uint8_t *paddr;
	int err;
	const struct spi_buf tx_bufs[2] = {
		{
			.buf = cmd,
			.len = cmd_len,
		},
		{
			.buf = (void *)buf,
			.len = count,
		},
	};
	const struct spi_buf_set tx = {
		.buffers = tx_bufs,
		.count = ARRAY_SIZE(tx_bufs),
	};

	paddr = &cmd[1];
	switch (config->address_width) {
	case 24:
		*paddr++ = offset >> 16;
	case 16:
		*paddr++ = offset >> 8;
	case 8:
		*paddr++ = offset;
		break;
	default:
		__ASSERT(0, "invalid address width");
	}

	err = eeprom_m95080_wait_for_idle(dev);
	if (err) {
		LOG_ERR("EEPROM idle wait failed (err %d)", err);
		return err;
	}

	err = eeprom_m95080_wren(dev);
	if (err) {
		LOG_ERR("failed to disable write protection (err %d)", err);
		return err;
	}

	err = spi_transceive(data->spi_dev, &data->spi_cfg, &tx, NULL);
	if (err) {
		return err;
	}

	return count;
}

static int eeprom_m95080_read(struct device *dev, off_t offset, void *buf, size_t len)
{
	const struct eeprom_m95080_config *config = dev->config_info;
	struct eeprom_m95080_data *data = dev->driver_data;
	uint8_t *read_buf = buf;
	int ret;

	if (!len) {
		return 0;
	}

	if ((offset + len) > config->size) {
		LOG_WRN("attempt to read past device boundary");
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);
	while (len) {
		ret = eeprom_m95080_read_spi(dev, offset, read_buf, len);
		if (ret < 0) {
			LOG_ERR("failed to read EEPROM (err %d)", ret);
			k_mutex_unlock(&data->lock);
			return ret;
		}

		read_buf += ret;
		offset += ret;
		len -= ret;
	}

	k_mutex_unlock(&data->lock);
	return 0;
}

static int eeprom_m95080_write(struct device *dev, off_t offset, const void *buf, size_t len)
{
	const struct eeprom_m95080_config *config = dev->config_info;
	struct eeprom_m95080_data *data = dev->driver_data;
	const uint8_t *pbuf = buf;
	int ret;

	if (config->readonly) {
		LOG_WRN("attempt to write to read-only device");
		return -EACCES;
	}

	if (!len) {
		return 0;
	}

	if ((offset + len) > config->size) {
		LOG_WRN("attempt to write past device boundary");
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	ret = eeprom_m95080_write_enable(dev, true);
	if (ret) {
		LOG_ERR("failed to write-enable EEPROM (err %d)", ret);
		k_mutex_unlock(&data->lock);
		return ret;
	}

	while (len) {
		ret = eeprom_m95080_write_spi(dev, offset, pbuf, len);
		if (ret < 0) {
			LOG_ERR("failed to write to EEPROM (err %d)", ret);
			eeprom_m95080_write_enable(dev, false);
			k_mutex_unlock(&data->lock);
			return ret;
		}

		pbuf += ret;
		offset += ret;
		len -= ret;
	}

	ret = eeprom_m95080_write_enable(dev, false);
	if (ret) {
		LOG_ERR("failed to write-protect EEPROM (err %d)", ret);
	}

	k_mutex_unlock(&data->lock);

	return ret;
}

static size_t eeprom_m95080_size(struct device *dev)
{
	const struct eeprom_m95080_config *config = dev->config_info;
	return config->size;
}

static int eeprom_m95080_init(struct device *dev)
{
	const struct eeprom_m95080_config *config = dev->config_info;
	struct eeprom_m95080_data *data = dev->driver_data;

	k_mutex_init(&data->lock);

	data->spi_dev = device_get_binding(config->spi_dev_name);
	if (!data->spi_dev) {
		LOG_ERR("could not get SPI device");
		return -EINVAL;
	}

	data->spi_cfg.operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8);
	data->spi_cfg.frequency = config->frequency;
	data->spi_cfg.slave = config->bus_address;

	if (config->cs_dev_name) {
		data->spi_cs.gpio_dev = device_get_binding(config->cs_dev_name);
		if (!data->spi_cs.gpio_dev) {
			LOG_ERR("could not get CS device");
			return -EINVAL;
		}
		data->spi_cs.gpio_pin = config->cs_pin;
		data->spi_cs.delay = 0;
		data->spi_cfg.cs = &data->spi_cs;
	}

	if (config->wp_dev_name) {
		int r;
		data->wp_dev = device_get_binding(config->wp_dev_name);
		if (!data->wp_dev) {
			LOG_ERR("could not get WP device");
			return -EINVAL;
		}

		r = gpio_pin_configure(data->wp_dev, config->wp_pin,
				       GPIO_OUTPUT_ACTIVE | config->wp_pin_flags);
		if (r) {
			LOG_ERR("failed to configure WP GPIO pin (err %d)", r);
			return r;
		}
	}

	return 0;
}

static const struct eeprom_driver_api eeprom_m95080_api = {
	.read = eeprom_m95080_read,
	.write = eeprom_m95080_write,
	.size = eeprom_m95080_size,
};

#define DT_INST_SPI_EEPROM_HAS_WP_GPIOS(n)		DT_INST_NODE_HAS_PROP(n, wp_gpios)
#define DT_INST_SPI_EEPROM_WP_GPIOS_LABEL(n)		DT_LABEL(DT_INST_PHANDLE_BY_IDX(n, wp_gpios, 0))
#define DT_INST_SPI_EEPROM_WP_GPIOS_PIN(n)		DT_INST_PHA_BY_IDX(n, wp_gpios, 0, pin)
#define DT_INST_SPI_EEPROM_WP_GPIOS_FLAGS(n)		DT_INST_PHA_BY_IDX(n, wp_gpios, 0, flags)

#define EEPROM_M95080_DEVICE(n)							\
	static struct eeprom_m95080_data eeprom_m95080_##n##_data = {};		\
	static struct eeprom_m95080_config eeprom_m95080_##n##_config = {	\
		.spi_dev_name = DT_INST_BUS_LABEL(n),				\
		.frequency = DT_INST_PROP(n, spi_max_frequency),		\
		.bus_address = DT_INST_REG_ADDR(n),				\
		.size = DT_INST_PROP(n, size),					\
		.pagesize = DT_INST_PROP(n, pagesize),				\
		.address_width = DT_INST_PROP(n, address_width),		\
		.timeout = DT_INST_PROP(n, timeout),				\
		.readonly = DT_INST_PROP(n, read_only),				\
		IF_ENABLED(DT_INST_SPI_DEV_HAS_CS_GPIOS(n), (			\
		.cs_dev_name = DT_INST_SPI_DEV_CS_GPIOS_LABEL(n),		\
		.cs_pin  = DT_INST_SPI_DEV_CS_GPIOS_PIN(n),			\
		.cs_pin_flags  = DT_INST_SPI_DEV_CS_GPIOS_FLAGS(n),))		\
		IF_ENABLED(DT_INST_SPI_EEPROM_HAS_WP_GPIOS(n), (		\
		.wp_dev_name = DT_INST_SPI_EEPROM_WP_GPIOS_LABEL(n),		\
		.wp_pin  = DT_INST_SPI_EEPROM_WP_GPIOS_PIN(n),			\
		.wp_pin_flags  = DT_INST_SPI_EEPROM_WP_GPIOS_FLAGS(n),))	\
	};									\
	DEVICE_AND_API_INIT(eeprom_m95080_##n,					\
			DT_INST_LABEL(n),					\
			&eeprom_m95080_init,					\
			&eeprom_m95080_##n##_data,				\
			&eeprom_m95080_##n##_config,				\
			POST_KERNEL,						\
			CONFIG_EEPROM_M95080_INIT_PRIORITY,			\
			&eeprom_m95080_api);

DT_INST_FOREACH_STATUS_OKAY(EEPROM_M95080_DEVICE)