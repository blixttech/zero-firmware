#include "bcb_sw.h"
#include "bcb_macros.h"
#include "bcb_config.h"
#include "bcb_ocp_otp.h"
#include "bcb_msmnt.h"
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>

#define LOG_LEVEL CONFIG_BCB_OCP_OTP_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_sw);

#define BCB_GPIO_DEV(pin_name) (sw_data.dev_gpio_##pin_name)

struct bcb_sw_data {
	struct device *dev_gpio_on_off;
	struct device *dev_gpio_on_off_status;
};

static struct bcb_sw_data sw_data;

int bcb_sw_init(void)
{
	memset(&sw_data, 0, sizeof(sw_data));

	BCB_GPIO_PIN_INIT(dctrl, on_off);
	BCB_GPIO_PIN_CONFIG(dctrl, on_off, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
	BCB_GPIO_PIN_SET_RAW(dctrl, on_off, 0);

	BCB_GPIO_PIN_INIT(dctrl, on_off_status);
	BCB_GPIO_PIN_CONFIG(dctrl, on_off_status, GPIO_INPUT);

	return 0;
}

int bcb_sw_on(void)
{
	if (bcb_sw_is_on()) {
		return 0;
	}

	if ((bcb_msmnt_get_temp(BCB_TEMP_SENSOR_PWR_IN) >
	     CONFIG_BCB_LIB_SW_MAX_CLOSING_TEMPERATURE) ||
	    (bcb_msmnt_get_temp(BCB_TEMP_SENSOR_PWR_OUT) >
	     CONFIG_BCB_LIB_SW_MAX_CLOSING_TEMPERATURE)) {
		     return -EACCES;
	}

	bcb_ocp_otp_reset();
	BCB_GPIO_PIN_SET_RAW(dctrl, on_off, 1);

	return 0;
}

int bcb_sw_off(void)
{
	bcb_ocp_test_trigger(BCB_OCP_DIRECTION_P, false);
	bcb_ocp_test_trigger(BCB_OCP_DIRECTION_N, false);
	BCB_GPIO_PIN_SET_RAW(dctrl, on_off, 0);
	return 0;
}

bool bcb_sw_is_on()
{
	return BCB_GPIO_PIN_GET_RAW(dctrl, on_off_status) == 1;
}

bool bcb_sw_is_on_cmd_active()
{
	return BCB_GPIO_PIN_GET_RAW(dctrl, on_off) == 1;
}