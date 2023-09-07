#define DT_DRV_COMPAT infineon_sici

#include <zephyr/kernel.h>
#include <zephyr/drivers/sici.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/logging/log.h>
#include <stdint.h>

// #define LOG_LEVEL CONFIG_SICI_LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DBG
LOG_MODULE_REGISTER(sici);

#define PINCTRL_STATE_ACTIVATION    PINCTRL_STATE_PRIV_START
#define PINCTRL_STATE_COMMUNICATION (PINCTRL_STATE_PRIV_START + 1U)

#define T_EN	 150
#define T_EN_MAX 400
#define T_LOW	 20
#define T_CMD	 3000

#define CMD_ENTER_IF 0xdcba

struct sici_config {
	const struct device *uart;
	struct gpio_dt_spec pwr_en_pin;
	struct gpio_dt_spec com_en_pin;
	struct gpio_dt_spec com_pin;
	const struct pinctrl_dev_config *pincfg;
	uint32_t baud_bit;
	uint8_t byte_sbit0;
	uint8_t byte_sbit1;
	uint8_t byte_rbit;
};

struct sici_data {
	bool enabled;
	struct uart_config uart_cfg;
	uint32_t bit_timeout;
};

static inline int sici_tx_rx_bit(const struct device *dev, const uint8_t tx_bit, uint8_t *rx_bit)
{
	const struct sici_config *config = dev->config;
	const struct sici_data *data = dev->data;
	uint64_t end;
	uint8_t dummy;
	uint8_t tx_bytes[2];
	uint8_t rx_bytes[2] = {0};

	tx_bytes[0] = tx_bit ? config->byte_sbit1 : config->byte_sbit0;
	tx_bytes[1] = config->byte_rbit;

	end = sys_clock_timeout_end_calc(K_USEC(data->bit_timeout));

	while (uart_poll_in(config->uart, &dummy) == 0) {
		/* poll in any buffered data */
	}

	for (int i = 0; i < 2; i++) {
		int r;

		uart_poll_out(config->uart, tx_bytes[0]);

		do {
			r = uart_poll_in(config->uart, &rx_bytes[0]);
		} while (r != 0 && end > k_uptime_ticks());

		if (r) {
			return r;
		}
	}

	return 0;
}

static int sici_power_impl(const struct device *dev, bool on)
{
	const struct sici_config *config;
	int r;

	if (!dev) {
		return -ENODEV;
	}

	config = dev->config;

	if (on) {
		r = gpio_pin_set_dt(&config->pwr_en_pin, 1);
	} else {
		r = gpio_pin_set_dt(&config->pwr_en_pin, 0);
	}

	return r;
}

static int sici_transfer_impl(const struct device *dev, uint16_t in, uint16_t *out)
{

	const struct sici_config *config;
	struct sici_data *data;
	uint8_t tx_bit, rx_bit = 0;

	if (!dev) {
		return -ENODEV;
	}

	config = dev->config;
	data = dev->data;

	for (int i = 0; i < 16; i++) {
		tx_bit = (in & (1U << i)) ? 1U : 0;
		if (sici_tx_rx_bit(dev, tx_bit, &rx_bit)) {
			return -EIO;
		}

		*out |= (uint16_t)(rx_bit != 0) << i;
	}

	return 0;
}

static int sici_enable_impl(const struct device *dev, bool enable)
{
	const struct sici_config *config;
	struct sici_data *data;

	if (!dev) {
		return -ENODEV;
	}

	config = dev->config;
	data = dev->data;

	if (enable) {
		uint16_t rx_data = 0xffff;

		if (data->enabled) {
			return 0;
		}
		/*
		 * Power down device.
		 * Apply interface activation pin configurations (set to GPIO mode).
		 * Pulldown com pin.
		 * Enable communication pin (if applicable).
		 * Wait 100 us.
		 * Power on device.
		 * Wait T_EN + T_LOW us.
		 * Apply communication pin configurations (set UART mode).
		 * Wait T_MAX-T_EN-T_LOW us.
		 * Send interface enable command.
		 */

		gpio_pin_set_dt(&config->pwr_en_pin, 0);
		pinctrl_apply_state(config->pincfg, PINCTRL_STATE_ACTIVATION);
		gpio_pin_configure_dt(&config->com_pin, GPIO_OUTPUT);
		gpio_pin_set_dt(&config->com_pin, 0);

		if (config->com_en_pin.port) {
			gpio_pin_set_dt(&config->com_en_pin, 0);
		}

		k_usleep(100);
		gpio_pin_set_dt(&config->pwr_en_pin, 1);
		k_usleep(T_EN + T_LOW);
		pinctrl_apply_state(config->pincfg, PINCTRL_STATE_COMMUNICATION);
		k_usleep(T_EN_MAX - T_EN - T_LOW);

		if (sici_transfer_impl(dev, CMD_ENTER_IF, &rx_data)) {
			LOG_ERR("cannot send ENTER_IF");
			return -EIO;
		}

		if (!rx_data) {
			LOG_ERR("invalid response to ENTER_IF: %" PRIu16, rx_data);
			return -EIO;
		}

	} else {
		if (!data->enabled) {
			return 0;
		}

		/**
		 * Pulldown com pin.
		 * Wait 6ms.
		 * Disable communication pin (if applicable).
		 * Apply default pin configurations.
		 */
		pinctrl_apply_state(config->pincfg, PINCTRL_STATE_ACTIVATION);
		gpio_pin_configure_dt(&config->com_pin, GPIO_OUTPUT);
		gpio_pin_set_dt(&config->com_pin, 0);
		k_msleep(6);

		if (config->com_en_pin.port) {
			gpio_pin_set_dt(&config->com_en_pin, 0);
		}

		pinctrl_apply_state(config->pincfg, PINCTRL_STATE_DEFAULT);
	}

	return 0;
}

static int sici_init(const struct device *dev)
{
	const struct sici_config *config = dev->config;
	struct sici_data *data = dev->data;

	pinctrl_apply_state(config->pincfg, PINCTRL_STATE_DEFAULT);
	gpio_pin_configure_dt(&config->pwr_en_pin, GPIO_OUTPUT);
	gpio_pin_set_dt(&config->pwr_en_pin, 1);

	if (config->com_en_pin.port) {
		gpio_pin_configure_dt(&config->com_en_pin, GPIO_OUTPUT);
		gpio_pin_set_dt(&config->com_en_pin, 0);
	}

	if (!device_is_ready(config->uart)) {
		LOG_ERR("Serial device not ready");
		return -ENODEV;
	}

	/* One start, 8 data, one stop and no parity bits (totally 10 bits). */
	data->uart_cfg.baudrate = config->baud_bit;
	data->uart_cfg.parity = UART_CFG_PARITY_NONE;
	data->uart_cfg.data_bits = UART_CFG_DATA_BITS_8;
	data->uart_cfg.stop_bits = UART_CFG_STOP_BITS_1;
	data->uart_cfg.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;

	/* Need to send two bytes over the UART to send one bit in SICI. */
	data->bit_timeout = (1e6 * 10 * 2) / data->uart_cfg.baudrate;
	//data->bit_timeout *= 2;
	LOG_DBG("bit timeout: %" PRIu32 " us", data->bit_timeout);

	if (uart_configure(config->uart, &data->uart_cfg) != 0) {
		LOG_ERR("Failed to configure UART");
		return -EINVAL;
	}

	return 0;
}

static const struct sici_driver_api sici_api = {
	.power = sici_power_impl,
	.enable = sici_enable_impl,
	.transfer = sici_transfer_impl,
};

#define GPIO_DT_SPEC_INST_COM_GET(n)                                                               \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, com_gpios), (GPIO_DT_SPEC_INST_GET(n, com_gpios)), (0))

#define GPIO_DT_SPEC_INST_COM_EN_GET(n)                                                            \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, com_en_gpios),                                        \
		    (GPIO_DT_SPEC_INST_GET(n, com_en_gpios)), (0))

#define SICI_INST_DEFINE(n)                                                                        \
	PINCTRL_DT_INST_DEFINE(n);                                                                 \
	static struct sici_config sici_config_##n = {                                              \
		.uart = DEVICE_DT_GET(DT_INST_PHANDLE(n, uart)),                                   \
		.pwr_en_pin = GPIO_DT_SPEC_INST_GET(n, pwr_en_gpios),                              \
		.com_en_pin = GPIO_DT_SPEC_INST_COM_GET(n),                                        \
		.com_pin = GPIO_DT_SPEC_INST_COM_GET(n),                                           \
		.baud_bit = DT_INST_PROP(n, baud_bit),                                             \
		.byte_sbit0 = DT_INST_PROP(n, byte_sbit0),                                         \
		.byte_sbit1 = DT_INST_PROP(n, byte_sbit1),                                         \
		.byte_rbit = DT_INST_PROP(n, byte_rbit),                                           \
		.pincfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),                                       \
	};                                                                                         \
                                                                                                   \
	static struct sici_data sici_data_##n;                                                     \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &sici_init, NULL, &sici_data_##n, &sici_config_##n, POST_KERNEL,  \
			      CONFIG_SICI_INIT_PRIORITY, &sici_api);

DT_INST_FOREACH_STATUS_OKAY(SICI_INST_DEFINE)
