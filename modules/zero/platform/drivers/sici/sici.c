#define DT_DRV_COMPAT infineon_sici

#include <zephyr/drivers/sici.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sici, CONFIG_SICI_LOG_LEVEL);

struct sici_config {
	const struct device *uart_dev;
	const struct device *power_dev;
	const struct pinctrl_dev_config *pincfg_default;
	const struct pinctrl_dev_config *pincfg_enabled;
	uint32_t baud_enable;
	uint32_t baud_bit;
	uint8_t byte_enable;
	uint8_t byte_sbit0;
	uint8_t byte_sbit1;
	uint8_t byte_rbit;
};

struct sici_data {
};

#define SICI_INST(n)

DT_INST_FOREACH_STATUS_OKAY(ADC_FTM_TRIGGER)