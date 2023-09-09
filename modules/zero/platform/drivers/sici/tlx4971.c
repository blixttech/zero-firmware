#define DT_DRV_COMPAT infineon_tlx4971

#include <zephyr/kernel.h>
#include <zephyr/drivers/sici.h>
#include <zephyr/drivers/tlx4971.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/logging/log.h>
#include <stdint.h>

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
	uint16_t config_regs[3];
};

static int tlx4971_get_config_impl(const struct device *dev, struct tlx4971_config *config)
{
	return 0;
}

static int tlx4971_set_config_impl(const struct device *dev, const struct tlx4971_config *config)
{
	return 0;
}

static int tlx4971_init(const struct device *dev)
{
	const struct tlx4971_drv_config *config = dev->config;
	struct tlx4971_drv_data *data = dev->data;
	uint16_t out = 0;

	if (sici_enable(config->sici, true)) {
		LOG_ERR("unable to enable interface");
		return -EIO;
	}

	/* Power down ISM */
	sici_transfer(config->sici, 0x8250, &out);
	sici_transfer(config->sici, 0x8000, &out);

	/* Disable failure indication */
	sici_transfer(config->sici, 0x8010, &out);
	sici_transfer(config->sici, 0x0000, &out);

	/* Read EEPROM registers 0x40 to 0x42 */
	sici_transfer(config->sici, 0x0400, &out);
	sici_transfer(config->sici, 0x0410, &data->config_regs[0]);
	sici_transfer(config->sici, 0x0420, &data->config_regs[1]);
	sici_transfer(config->sici, 0xFFFF, &data->config_regs[2]);

	LOG_DBG("config_reg[0]: 0x%" PRIx16, data->config_regs[0]);
	LOG_DBG("config_reg[1]: 0x%" PRIx16, data->config_regs[1]);
	LOG_DBG("config_reg[2]: 0x%" PRIx16, data->config_regs[2]);

	k_msleep(2);

	sici_enable(config->sici, false);

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