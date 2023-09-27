#define DT_DRV_COMPAT infineon_tlx4971

#include <zephyr/kernel.h>
#include <zephyr/drivers/sici.h>
#include <zephyr/drivers/tlx4971.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>
#include <zephyr/timing/timing.h>

// #define LOG_LEVEL CONFIG_SICI_LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DBG
LOG_MODULE_REGISTER(tlx4971);

struct tlx4971_drv_config {
	const struct device *sici;
	struct gpio_dt_spec vref_en_pin;
	struct gpio_dt_spec ocd1_pin;
	struct gpio_dt_spec ocd2_pin;
	const struct pinctrl_dev_config *pincfg;
};

struct tlx4971_drv_data {
	uint16_t regs[3]; /**< Holds values of configurable registers. */
	uint8_t crc_ro;	  /**< Holds CRC for the read-only registers. */
};

static uint8_t crc8_reg(uint16_t data, uint8_t crc)
{
	data = (data >> 8) | (data << 8);
	return crc8((const uint8_t *)&data, sizeof(data), 0x1d, crc, false);
}

static inline uint8_t crc8_byte(uint8_t data, uint8_t crc)
{
	return crc8(&data, 1, 0x1d, crc, false);
}

static uint8_t crc8_all(const struct device *dev)
{
	struct tlx4971_drv_data *data = dev->data;
	uint8_t crc = data->crc_ro;

	crc = crc8_reg(data->regs[0], crc);
	crc = crc8_reg(data->regs[1], crc);
	crc = crc8_byte((data->regs[2] >> 8), crc);
	crc = crc ^ 0xff;

	return crc;
}

/**
 * @brief Fetch EEPROM registers.
 *	  This function takes about 90 ms to complete.
 */
static int fetch_regs(const struct device *dev)
{
	const struct tlx4971_drv_config *config = dev->config;
	struct tlx4971_drv_data *data = dev->data;
	uint16_t out = 0;
	int r = 0;

	if (sici_enable(config->sici, true)) {
		LOG_ERR("unable to enable interface");
		return -EIO;
	}

	/* Power down ISM - write 0x8000 to the address 0x25 */
	sici_transfer(config->sici, 0x8250, &out);
	sici_transfer(config->sici, 0x8000, &out);

	/* Disable failure indication - write 0x0000 to the address 0x01 */
	sici_transfer(config->sici, 0x8010, &out);
	sici_transfer(config->sici, 0x0000, &out);

	/* Read all EEPROM lines 0x40 to 0x51 to calculate CRC (x^8 + x^4 + x^3 + x^2 + 1).
	 * Only store the configurable registers 0x40 to 0x42.
	 */
	data->crc_ro = 0xaa; /* Initialise with seed value */

	sici_transfer(config->sici, 0x0400, &out);

	for (uint8_t reg_idx = 0; reg_idx < 17; reg_idx++) {
		uint16_t next_addr = 0x0410 + (reg_idx << 4);

		if (reg_idx < 3) {
			sici_transfer(config->sici, next_addr, &data->regs[reg_idx]);
		} else {
			sici_transfer(config->sici, next_addr, &out);
			data->crc_ro = crc8_reg(out, data->crc_ro);
		}
	}

	sici_transfer(config->sici, 0xFFFF, &out);
	data->crc_ro = crc8_reg(out, data->crc_ro);

	/* Power on ISM - write 0x0000 to the address 0x25 */
	sici_transfer(config->sici, 0x8250, &out);
	sici_transfer(config->sici, 0x0000, &out);

	sici_enable(config->sici, false);

	if ((data->regs[2] & 0xff) != crc8_all(dev)) {
		LOG_ERR("CRC mistach");
		r = -EIO;
	}

	return r;
}

static int set_regs(const struct device *dev, bool is_temp)
{

	const struct tlx4971_drv_config *config = dev->config;
	struct tlx4971_drv_data *data = dev->data;
	uint16_t out = 0;
	int r = 0;

	if (sici_enable(config->sici, true)) {
		LOG_ERR("unable to enable interface");
		return -EIO;
	}

	/* Power down ISM - write 0x8000 to the address 0x25 */
	sici_transfer(config->sici, 0x8250, &out);
	sici_transfer(config->sici, 0x8000, &out);

	/* Disable failure indication - write 0x0000 to the address 0x01 */
	sici_transfer(config->sici, 0x8010, &out);
	sici_transfer(config->sici, 0x0000, &out);


	/* @todo Update CRC
	 * @todo Write temporary registers in selected.
	 * @todo Write to EEPROM registers otherwise.
	 */

	/* Power on ISM - write 0x0000 to the address 0x25 */
	sici_transfer(config->sici, 0x8250, &out);
	sici_transfer(config->sici, 0x0000, &out);

	sici_enable(config->sici, false);

	return r;
}

static int tlx4971_get_config_impl(const struct device *dev, struct tlx4971_config *config)
{
	struct tlx4971_drv_data *data;
	int r;

	if (!dev) {
		return -ENODEV;
	}

	data = dev->data;

	r = fetch_regs(dev);
	if (r) {
		return r;
	}

	switch (data->regs[0] & 0x1f) {
	case 0x05:
		config->range = TLX4971_RANGE_120;
		break;
	case 0x06:
		config->range = TLX4971_RANGE_100;
		break;
	case 0x08:
		config->range = TLX4971_RANGE_75;
		break;
	case 0x0c:
		config->range = TLX4971_RANGE_50;
		break;
	case 0x10:
		config->range = TLX4971_RANGE_37_5;
		break;
	case 0x18:
		config->range = TLX4971_RANGE_25;
		break;
	default:
		LOG_WRN("invalid range");
	}

	return 0;
}

static int tlx4971_set_config_impl(const struct device *dev, const struct tlx4971_config *config)
{
	return 0;
}

static int tlx4971_init(const struct device *dev)
{
	// const struct tlx4971_drv_config *config = dev->config;
	struct tlx4971_drv_data *data = dev->data;
	int r;

	r = fetch_regs(dev);
	if (r) {
		return r;
	}

	LOG_DBG("config_reg[0]: 0x%" PRIx16, data->regs[0]);
	LOG_DBG("config_reg[1]: 0x%" PRIx16, data->regs[1]);
	LOG_DBG("config_reg[2]: 0x%" PRIx16, data->regs[2]);

	return 0;
}

static const struct tlx4971_driver_api drv_api = {.get_config = tlx4971_get_config_impl,
						  .set_config = tlx4971_set_config_impl};

#define GPIO_DT_SPEC_INST_VREF_GET(n)                                                              \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, vref_gpios), (GPIO_DT_SPEC_INST_GET(n, vref_gpios)),  \
		    ({0}))

#define GPIO_DT_SPEC_INST_OCD1_GET(n)                                                              \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, ocd1_gpios), (GPIO_DT_SPEC_INST_GET(n, ocd1_gpios)),  \
		    ({0}))

#define GPIO_DT_SPEC_INST_OCD2_GET(n)                                                              \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, ocd2_gpios), (GPIO_DT_SPEC_INST_GET(n, ocd2_gpios)),  \
		    ({0}))

#define TLX4971_INST_DEFINE(n)                                                                     \
	PINCTRL_DT_INST_DEFINE(n);                                                                 \
	static struct tlx4971_drv_config drv_config_##n = {                                        \
		.sici = DEVICE_DT_GET(DT_INST_BUS(n)),                                             \
		.vref_en_pin = GPIO_DT_SPEC_INST_VREF_GET(n),                                      \
		.ocd1_pin = GPIO_DT_SPEC_INST_OCD1_GET(n),                                         \
		.ocd2_pin = GPIO_DT_SPEC_INST_OCD2_GET(n),                                         \
		.pincfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),                                       \
	};                                                                                         \
                                                                                                   \
	static struct tlx4971_drv_data drv_data_##n;                                               \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &tlx4971_init, NULL, &drv_data_##n, &drv_config_##n, POST_KERNEL, \
			      CONFIG_SICI_INIT_PRIORITY, &drv_api);

DT_INST_FOREACH_STATUS_OKAY(TLX4971_INST_DEFINE)