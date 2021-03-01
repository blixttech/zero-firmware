#include "bcb_sw.h"
#include "bcb_macros.h"
#include "bcb_config.h"
#include "bcb_msmnt.h"
#include "bcb_etime.h"
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/input_capture.h>
#include <drivers/pwm.h>
#include <drivers/dac.h>

#define LOG_LEVEL CONFIG_BCB_OCP_OTP_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_sw);

#define BCB_GPIO_DEV(pin_name) (sw_data.dev_gpio_##pin_name)
#define BCB_IC_DEV(ch_name) (sw_data.dev_ic_##ch_name)
#define BCB_PWM_DEV(ch_name) (sw_data.dev_pwm_##ch_name)
#define BCB_DAC_DEV(ch_name) (sw_data.dev_dac_##ch_name)

struct bcb_sw_data {
	struct device *dev_gpio_on_off;
	struct device *dev_gpio_on_off_status;
	struct device *dev_gpio_ocp_otp_reset;
	struct device *dev_gpio_ocp_test_tr_n;
	struct device *dev_gpio_ocp_test_tr_p;
	struct device *dev_ic_on_off_status_r;
	struct device *dev_ic_on_off_status_f;
	struct device *dev_ic_ocp_test_tr_n;
	struct device *dev_ic_ocp_test_tr_p;
	struct device *dev_pwm_ocp_test_adj;
	struct device *dev_dac_ocp_limit_adj;
	volatile bcb_ocp_direction_t ocp_test_direction;
	volatile bool ocp_test_active;
	volatile bcb_sw_cause_t cause;
	volatile uint64_t etime_on;
	volatile uint64_t etime_off;
	volatile uint32_t on_off_duration;
	volatile uint32_t ocp_test_duration;
	uint32_t etime_frequency;
	uint32_t ic_frequency;
	struct gpio_callback on_off_status_callback;
	sys_slist_t callback_list;
};

static struct bcb_sw_data sw_data;

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
	if (sw_data.etime_frequency != sw_data.ic_frequency) {
		etime *= sw_data.ic_frequency;
		ic *= sw_data.etime_frequency;
	}

	return etime < ic ? -1 : etime > ic;
}

/**
 * @brief Get the time duration between on and off events.
 *
 * @return uint64_t Time duration in nano seconds
 */
static inline uint32_t get_on_off_duration(void)
{
	uint32_t ic_on;
	uint32_t ic_off;
	uint32_t ic_duration;
	uint64_t etime_duration;
	uint64_t duration;

	ic_on = BCB_IC_VALUE(itimestamp, on_off_status_r);
	ic_off = BCB_IC_VALUE(itimestamp, on_off_status_f);
	ic_duration = ic_on > ic_off ? BCB_IC_COUNTER_MAX(on_off_status_r) - ic_on + ic_off :
				       ic_off - ic_on;

	etime_duration = sw_data.etime_on > sw_data.etime_off ?
				 UINT64_MAX - sw_data.etime_on + sw_data.etime_off :
				 sw_data.etime_off - sw_data.etime_on;

	if (etime_ic_ticks_compare(etime_duration, BCB_IC_COUNTER_MAX(on_off_status_r)) < 0) {
		duration = (uint64_t)ic_duration * (uint64_t)1e9 /
			   (uint64_t)BCB_IC_FREQUENCY(on_off_status_f);

	} else {
		duration = etime_duration * (uint64_t)1e9 / (uint64_t)bcb_etime_get_frequency();
	}

	return duration > UINT32_MAX ? UINT32_MAX : (uint32_t)duration;
}

static inline uint32_t get_ocp_test_duration()
{
	uint32_t start;
	uint32_t end;
	uint64_t duration;

	if (sw_data.ocp_test_direction == BCB_OCP_DIRECTION_POSITIVE) {
		start = BCB_IC_VALUE(itimestamp, ocp_test_tr_p);
	} else {
		start = BCB_IC_VALUE(itimestamp, ocp_test_tr_n);
	}

	end = BCB_IC_VALUE(itimestamp, on_off_status_f);
	duration = start > end ? BCB_IC_COUNTER_MAX(on_off_status_f) - start + end : end - start;
	duration = duration * (uint64_t)1e9 / (uint64_t)BCB_IC_FREQUENCY(on_off_status_f);

	return duration > UINT32_MAX ? UINT32_MAX : (uint32_t)duration;
}

static void call_callbacks(void)
{
	bcb_sw_callback_t *callback;
	SYS_SLIST_FOR_EACH_CONTAINER (&sw_data.callback_list, callback, node) {
		if (callback && callback->handler) {
			callback->handler(bcb_sw_is_on(), bcb_sw_get_cause());
		}
	}
}

static void on_off_status_changed(struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	uint64_t etime_now = bcb_etime_get_now();
	bool is_on = BCB_GPIO_PIN_GET_RAW(dctrl, on_off_status) == 1;
	bool is_on_cmd_active = BCB_GPIO_PIN_GET_RAW(dctrl, on_off) == 1;
	int32_t temp_in;
	int32_t temp_out;

	if (is_on) {
		sw_data.etime_on = etime_now;
		sw_data.cause = BCB_SW_CAUSE_EXT;
		call_callbacks();
		return;
	}

	sw_data.etime_off = etime_now;
	sw_data.on_off_duration = get_on_off_duration();

	if (!is_on_cmd_active) {
		/* The switch has been turned off via software */
		sw_data.cause = BCB_SW_CAUSE_EXT;
		call_callbacks();
		return;
	}

	if (sw_data.ocp_test_active) {
		bcb_ocp_test_trigger(BCB_OCP_DIRECTION_POSITIVE, false);
		bcb_ocp_test_trigger(BCB_OCP_DIRECTION_NEGATIVE, false);

		sw_data.ocp_test_duration = get_ocp_test_duration();
		LOG_DBG("direction %d, duration %" PRIu32 " ns", sw_data.ocp_test_direction,
			sw_data.ocp_test_duration);
		sw_data.cause = BCB_SW_CAUSE_OCP_TEST;
		call_callbacks();
		return;
	}

	/* The switch has been turned off while the turn-on command is still active.
	 * The hardware protection has been activated.
	 */

	temp_in = bcb_msmnt_get_temp(BCB_TEMP_SENSOR_PWR_IN);
	temp_out = bcb_msmnt_get_temp(BCB_TEMP_SENSOR_PWR_OUT);

	if (temp_out > 200) {
		/* Under voltage shutdown circuit pulls the temperature line of
		 * the power out board to ground.
		 */
		LOG_DBG("uvp activated");
		sw_data.cause = BCB_SW_CAUSE_UVP;
		call_callbacks();
		return;
	}

	if (temp_in > 80 || temp_out > 80) {
		/* overtemperature protection has been activated. */
		LOG_DBG("oct activated: in %" PRId32 " C, out %" PRId32 " C", temp_in, temp_out);
		sw_data.cause = BCB_SW_CAUSE_OTP;
		call_callbacks();
		return;
	}

	/* Overcurrent protection has been activated. */
	LOG_DBG("otp activated");
	sw_data.cause = BCB_SW_CAUSE_OCP;
	call_callbacks();
}

int bcb_sw_init(void)
{
	memset(&sw_data, 0, sizeof(sw_data));

	BCB_GPIO_PIN_INIT(dctrl, on_off);
	BCB_GPIO_PIN_CONFIG(dctrl, on_off, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
	BCB_GPIO_PIN_SET_RAW(dctrl, on_off, 0);

	BCB_GPIO_PIN_INIT(dctrl, on_off_status);
	BCB_GPIO_PIN_CONFIG(dctrl, on_off_status, GPIO_INPUT);

	BCB_GPIO_PIN_INIT(dctrl, ocp_otp_reset);
	BCB_GPIO_PIN_CONFIG(dctrl, ocp_otp_reset, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
	BCB_GPIO_PIN_SET_RAW(dctrl, ocp_otp_reset, 0);

	BCB_GPIO_PIN_INIT(dctrl, ocp_test_tr_n);
	BCB_GPIO_PIN_CONFIG(dctrl, ocp_test_tr_n, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
	BCB_GPIO_PIN_SET_RAW(dctrl, ocp_test_tr_n, 0);

	BCB_GPIO_PIN_INIT(dctrl, ocp_test_tr_p);
	BCB_GPIO_PIN_CONFIG(dctrl, ocp_test_tr_p, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
	BCB_GPIO_PIN_SET_RAW(dctrl, ocp_test_tr_p, 0);

	gpio_pin_interrupt_configure(BCB_GPIO_DEV(on_off_status),
				     DT_PHA_BY_NAME(DT_NODELABEL(dctrl), gpios, on_off_status, pin),
				     GPIO_INT_EDGE_BOTH);
	gpio_init_callback(&sw_data.on_off_status_callback, on_off_status_changed,
			   BIT(DT_PHA_BY_NAME(DT_NODELABEL(dctrl), gpios, on_off_status, pin)));
	gpio_add_callback(BCB_GPIO_DEV(on_off_status), &sw_data.on_off_status_callback);

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

	return 0;
}

int bcb_sw_on(void)
{
	int32_t temp_in;
	int32_t temp_out;

	if (bcb_sw_is_on()) {
		return 0;
	}

	temp_in = bcb_msmnt_get_temp(BCB_TEMP_SENSOR_PWR_IN);
	temp_out = bcb_msmnt_get_temp(BCB_TEMP_SENSOR_PWR_OUT);

	if (temp_out > 200) {
		sw_data.cause = BCB_SW_CAUSE_UVP;
		return -EACCES;
	}

	if (temp_in > CONFIG_BCB_LIB_SW_MAX_CLOSING_TEMPERATURE ||
	    temp_out > CONFIG_BCB_LIB_SW_MAX_CLOSING_TEMPERATURE) {
		sw_data.cause = BCB_SW_CAUSE_OTP;
		return -EACCES;
	}

	BCB_GPIO_PIN_SET_RAW(dctrl, ocp_otp_reset, 0);
	BCB_GPIO_PIN_SET_RAW(dctrl, ocp_otp_reset, 1);
	BCB_GPIO_PIN_SET_RAW(dctrl, ocp_otp_reset, 0);
	sw_data.cause = BCB_SW_CAUSE_EXT;
	BCB_GPIO_PIN_SET_RAW(dctrl, on_off, 1);

	return 0;
}

int bcb_sw_off(void)
{
	bcb_ocp_test_trigger(BCB_OCP_DIRECTION_POSITIVE, false);
	bcb_ocp_test_trigger(BCB_OCP_DIRECTION_NEGATIVE, false);
	sw_data.cause = BCB_SW_CAUSE_EXT;
	BCB_GPIO_PIN_SET_RAW(dctrl, on_off, 0);

	return 0;
}

bool bcb_sw_is_on()
{
	return BCB_GPIO_PIN_GET_RAW(dctrl, on_off_status) == 1;
}

bcb_sw_cause_t bcb_sw_get_cause(void)
{
	return sw_data.cause;
}

int bcb_ocp_test_trigger(bcb_ocp_direction_t direction, bool enable)
{
	if (enable) {
		if (!bcb_sw_is_on()) {
			LOG_WRN("switch has to be turned on");
			return -EINVAL;
		}

		if (direction == BCB_OCP_DIRECTION_POSITIVE) {
			BCB_GPIO_PIN_SET_RAW(dctrl, ocp_test_tr_p, 1);
		} else if (direction == BCB_OCP_DIRECTION_NEGATIVE) {
			BCB_GPIO_PIN_SET_RAW(dctrl, ocp_test_tr_n, 1);
		} else {
			LOG_WRN("invalid direction");
			return -EINVAL;
		}

		sw_data.ocp_test_active = true;
		sw_data.ocp_test_direction = direction;

	} else {
		/* OCP test can be done only one direction at a time. So disable both anyway. */
		BCB_GPIO_PIN_SET_RAW(dctrl, ocp_test_tr_p, 0);
		BCB_GPIO_PIN_SET_RAW(dctrl, ocp_test_tr_n, 0);
		sw_data.ocp_test_active = false;
	}

	return 0;
}

bcb_ocp_direction_t bcb_ocp_test_get_direction(void)
{
	return sw_data.ocp_test_direction;
}

uint32_t bcb_sw_get_on_off_duration(void)
{
	return sw_data.on_off_duration;
}

uint32_t bcb_ocp_test_get_duration(void)
{
	return sw_data.ocp_test_duration;
}

int bcb_sw_add_callback(bcb_sw_callback_t *callback)
{
	if (!callback || !callback->handler) {
		return -ENOTSUP;
	}

	sys_slist_append(&sw_data.callback_list, &callback->node);

	return 0;
}

void bcb_sw_remove_callback(bcb_sw_callback_t *callback)
{
	if (!callback) {
		return;
	}

	sys_slist_find_and_remove(&sw_data.callback_list, &callback->node);
}
