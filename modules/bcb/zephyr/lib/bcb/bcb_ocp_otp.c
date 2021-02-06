#include "bcb_ocp_otp.h"
#include "bcb_macros.h"
#include "bcb_config.h"
#include "bcb_sw.h"
#include "bcb_etime.h"
#include "bcb_msmnt.h"
#include <stdbool.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/input_capture.h>
#include <drivers/pwm.h>
#include <drivers/dac.h>

#define LOG_LEVEL CONFIG_BCB_OCP_OTP_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_ocp_otp);

#define BCB_GPIO_DEV(pin_name) (ocp_otp_data.dev_gpio_##pin_name)
#define BCB_IC_DEV(ch_name) (ocp_otp_data.dev_ic_##ch_name)
#define BCB_PWM_DEV(ch_name) (ocp_otp_data.dev_pwm_##ch_name)
#define BCB_DAC_DEV(ch_name) (ocp_otp_data.dev_dac_##ch_name)

struct ocp_cal_data {
	uint32_t a; /* Unit - Amps/millivolt */
	uint8_t b; /* Unit - Amps */
};

struct ocp_otp_data {
	struct device *dev_gpio_ocp_otp_reset;
	struct device *dev_gpio_ocp_test_tr_n;
	struct device *dev_gpio_ocp_test_tr_p;
	struct device *dev_gpio_on_off_status;
	struct device *dev_ic_on_off_status_r;
	struct device *dev_ic_on_off_status_f;
	struct device *dev_ic_ocp_test_tr_n;
	struct device *dev_ic_ocp_test_tr_p;
	struct device *dev_pwm_ocp_test_adj;
	struct device *dev_dac_ocp_limit_adj;

	bcb_ocp_callback_t ocp_callback;
	bcb_otp_callback_t otp_callback;
	bcb_ocp_test_callback_t ocp_test_callback;

	volatile bool ocp_test_active;
	bcb_ocp_direction_t ocp_test_direction;

	struct gpio_callback on_off_status_callback;
	volatile uint64_t etime_on;
	volatile uint64_t etime_off;

	uint32_t etime_frequency;
	uint32_t ic_frequency;
};

static struct ocp_otp_data ocp_otp_data;

/**
 * @brief Compare the time duration represented by elapsed time ticks and input capcure time ticks
 * 
 * @param etime	Elapsed time ticks
 * @param ic 	Input capcure time ticks
 * @return int 
 * @retval -1	If the duration represented by elapsed time ticks is less than 
 *		that of input capcure time ticks.
 * @retval 0	If both ticks represent the same time duration.
 * @retval 1	If the duration represented by elapsed time ticks is higher than 
 *		that of input capcure time ticks.
 */
static inline int etime_ic_ticks_compare(uint64_t etime, uint64_t ic)
{
	if (ocp_otp_data.etime_frequency != ocp_otp_data.ic_frequency) {
		etime *= ocp_otp_data.ic_frequency;
		ic *= ocp_otp_data.etime_frequency;
	}

	return etime < ic ? -1 : etime > ic;
}

/**
 * @brief Get the time duration between on and off events.
 * 
 * @return uint64_t Time duration in nano seconds
 */
static inline uint64_t get_on_off_duration(void)
{
	uint32_t ic_on;
	uint32_t ic_off;
	uint32_t ic_duration;
	uint64_t etime_duration;

	ic_on = BCB_IC_VALUE(itimestamp, on_off_status_r);
	ic_off = BCB_IC_VALUE(itimestamp, on_off_status_f);
	ic_duration = ic_on > ic_off ? BCB_IC_COUNTER_MAX(on_off_status_r) - ic_on + ic_off :
				       ic_off - ic_on;

	etime_duration = ocp_otp_data.etime_on > ocp_otp_data.etime_off ?
				 UINT64_MAX - ocp_otp_data.etime_on + ocp_otp_data.etime_off :
				 ocp_otp_data.etime_off - ocp_otp_data.etime_on;

	if (etime_ic_ticks_compare(etime_duration, BCB_IC_COUNTER_MAX(on_off_status_r)) < 0) {
		return (uint64_t)ic_duration * (uint64_t)1e9 /
		       (uint64_t)BCB_IC_FREQUENCY(on_off_status_f);
	}

	return etime_duration * (uint64_t)1e9 / (uint64_t)bcb_etime_get_frequency();
}

static inline uint32_t get_ocp_test_duration()
{
	uint32_t start;
	uint32_t end;
	uint64_t duration;

	if (ocp_otp_data.ocp_test_direction == BCB_OCP_DIRECTION_P) {
		start = BCB_IC_VALUE(itimestamp, ocp_test_tr_p);
	} else {
		start = BCB_IC_VALUE(itimestamp, ocp_test_tr_n);
	}

	end = BCB_IC_VALUE(itimestamp, on_off_status_f);

	duration = start > end ? BCB_IC_COUNTER_MAX(on_off_status_f) - start + end : end - start;

	return (uint32_t)(duration * (uint64_t)1e9 / (uint64_t)BCB_IC_FREQUENCY(on_off_status_f));
}

static void on_off_status_changed(struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	uint64_t etime_now = bcb_etime_get_now();
	bool is_on = bcb_sw_is_on();
	bool is_on_cmd_active = bcb_sw_is_on_cmd_active();

	if (is_on) {
		ocp_otp_data.etime_on = etime_now;
	} else {
		int8_t temp;
		uint64_t on_off_duration;

		ocp_otp_data.etime_off = etime_now;

		on_off_duration = get_on_off_duration();

		if (!is_on_cmd_active) {
			/* The switch has been turned off via software */
			return;
		}

		if (ocp_otp_data.ocp_test_active) {
			bcb_ocp_test_trigger(BCB_OCP_DIRECTION_P, false);
			bcb_ocp_test_trigger(BCB_OCP_DIRECTION_N, false);

			on_off_duration = get_ocp_test_duration();
			LOG_DBG("ocp_test %" PRIu32 " ns", (uint32_t)on_off_duration);

			if (ocp_otp_data.ocp_test_callback) {
				ocp_otp_data.ocp_test_callback(ocp_otp_data.ocp_test_direction,
							       on_off_duration);
			}
			return;
		}

		/* The switch has been turned off while the turn-on command is still active. 
		 * The hardware protection has been activated. 
		 */

		/* Overcurrent or overtemperature protection has been triggered. */
		if ((temp = bcb_msmnt_get_temp(BCB_TEMP_SENSOR_PWR_IN)) > 80 ||
		    (temp = bcb_msmnt_get_temp(BCB_TEMP_SENSOR_PWR_OUT)) > 80) {
			LOG_DBG("overtemperature protection activated %" PRId8 " C", temp);
			if (ocp_otp_data.otp_callback) {
				ocp_otp_data.otp_callback(temp);
			}
			return;
		}

		LOG_DBG("overcurrent protection activated");

		if (ocp_otp_data.ocp_callback) {
			ocp_otp_data.ocp_callback(on_off_duration);
		}
	}
}

int bcb_ocp_otp_init(void)
{
	memset(&ocp_otp_data, 0, sizeof(ocp_otp_data));

	BCB_GPIO_PIN_INIT(dctrl, ocp_otp_reset);
	BCB_GPIO_PIN_CONFIG(dctrl, ocp_otp_reset, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
	BCB_GPIO_PIN_SET_RAW(dctrl, ocp_otp_reset, 0);

	BCB_GPIO_PIN_INIT(dctrl, ocp_test_tr_n);
	BCB_GPIO_PIN_CONFIG(dctrl, ocp_test_tr_n, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
	BCB_GPIO_PIN_SET_RAW(dctrl, ocp_test_tr_n, 0);

	BCB_GPIO_PIN_INIT(dctrl, ocp_test_tr_p);
	BCB_GPIO_PIN_CONFIG(dctrl, ocp_test_tr_p, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
	BCB_GPIO_PIN_SET_RAW(dctrl, ocp_test_tr_p, 0);

	BCB_GPIO_PIN_INIT(dctrl, on_off_status);

	gpio_pin_interrupt_configure(BCB_GPIO_DEV(on_off_status),
				     DT_PHA_BY_NAME(DT_NODELABEL(dctrl), gpios, on_off_status, pin),
				     GPIO_INT_EDGE_BOTH);
	gpio_init_callback(&ocp_otp_data.on_off_status_callback, on_off_status_changed,
			   BIT(DT_PHA_BY_NAME(DT_NODELABEL(dctrl), gpios, on_off_status, pin)));
	gpio_add_callback(BCB_GPIO_DEV(on_off_status), &ocp_otp_data.on_off_status_callback);

	BCB_IC_INIT(itimestamp, on_off_status_r);
	BCB_IC_CHANNEL_SET(itimestamp, on_off_status_r);

	BCB_IC_INIT(itimestamp, on_off_status_f);
	BCB_IC_CHANNEL_SET(itimestamp, on_off_status_f);

	BCB_IC_INIT(itimestamp, ocp_test_tr_n);
	BCB_IC_CHANNEL_SET(itimestamp, ocp_test_tr_n);

	BCB_IC_INIT(itimestamp, ocp_test_tr_p);
	BCB_IC_CHANNEL_SET(itimestamp, ocp_test_tr_p);

	BCB_PWM_INIT(actrl, ocp_test_adj);
	BCB_PWM_DUTY_CYCLE_SET(actrl, ocp_test_adj, 98);

	BCB_DAC_INIT(actrl, ocp_limit_adj);
	BCB_DAC_SET(actrl, ocp_limit_adj, 4095);

	/* TODO: Load calibration information */

	return 0;
}

int bcb_ocp_otp_reset(void)
{
	BCB_GPIO_PIN_SET_RAW(dctrl, ocp_otp_reset, 0);
	BCB_GPIO_PIN_SET_RAW(dctrl, ocp_otp_reset, 1);
	BCB_GPIO_PIN_SET_RAW(dctrl, ocp_otp_reset, 0);

	return 0;
}

void bcb_ocp_set_callback(bcb_ocp_callback_t callback)
{
	ocp_otp_data.ocp_callback = callback;
}

void bcb_otp_set_callback(bcb_otp_callback_t callback)
{
	ocp_otp_data.otp_callback = callback;
}

void bcb_otp_test_set_callback(bcb_ocp_test_callback_t callback)
{
	ocp_otp_data.ocp_test_callback = callback;
}

int bcb_ocp_set_limit(uint8_t current)
{
	return 0;
}

int bcb_ocp_test_trigger(bcb_ocp_direction_t direction, bool enable)
{
	if (enable) {
		if (!bcb_sw_is_on()) {
			LOG_WRN("Switch has to be turned on");
			return -EINVAL;
		}

		if (direction == BCB_OCP_DIRECTION_P) {
			BCB_GPIO_PIN_SET_RAW(dctrl, ocp_test_tr_p, 1);
		} else if (direction == BCB_OCP_DIRECTION_N) {
			BCB_GPIO_PIN_SET_RAW(dctrl, ocp_test_tr_n, 1);
		} else {
			LOG_WRN("Invalid direction");
			return -EINVAL;
		}

		ocp_otp_data.ocp_test_active = true;
		ocp_otp_data.ocp_test_direction = direction;

	} else {
		/* OCP test can be done only one direction at a time. So disable both anyway. */
		BCB_GPIO_PIN_SET_RAW(dctrl, ocp_test_tr_p, 0);
		BCB_GPIO_PIN_SET_RAW(dctrl, ocp_test_tr_n, 0);
		ocp_otp_data.ocp_test_active = false;
	}

	return 0;
}