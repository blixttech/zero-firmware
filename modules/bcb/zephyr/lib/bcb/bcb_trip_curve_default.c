#include "bcb_trip_curve.h"
#include "bcb_common.h"
#include "bcb.h"
#include "bcb_leds.h"
#include "bcb_sw.h"
#include "bcb_zd.h"
#include "bcb_ocp_otp.h"
#include <init.h>

#define TRANSIENT_WORK_TIMEOUT 50

#define LOG_LEVEL CONFIG_BCB_TRIP_CURVE_DEFAULT_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_tc_default);

typedef enum {
	CURVE_STATE_UNDEFINED = 0,
	CURVE_STATE_OPENED,
	CURVE_STATE_CLOSING,
	CURVE_STATE_CLOSED,
	CURVE_STATE_OPENING,
	CURVE_STATE_EMERGENCY_OPENED,
} curve_state_t;

struct curve_data {
	const struct bcb_trip_curve *trip_curve;
	bcb_trip_curve_callback_t callback;
	struct bcb_zd_callback zd_callback;
	struct bcb_ocp_callback ocp_callback;
	struct bcb_otp_callback otp_callback;
	struct bcb_ocp_test_callback ocp_test_callback;
	volatile curve_state_t state;
	struct k_delayed_work transient_work;
	uint16_t events_to_wait;
};

static struct curve_data curve_data;

static void process_timed_event(void)
{
	if (curve_data.state == CURVE_STATE_OPENED) {
		/* Nothing to be done for the moment */
	} else if (curve_data.state == CURVE_STATE_CLOSING) {
		int r;

		if (curve_data.events_to_wait) {
			curve_data.events_to_wait--;
			k_delayed_work_submit(&curve_data.transient_work,
					      K_MSEC(TRANSIENT_WORK_TIMEOUT));
			return;
		}

		r = bcb_sw_on();
		if (r) {
			k_delayed_work_submit(&curve_data.transient_work,
					      K_MSEC(TRANSIENT_WORK_TIMEOUT));
		} else {
			curve_data.state = CURVE_STATE_CLOSED;
		}

	} else if (curve_data.state == CURVE_STATE_CLOSED) {
		/* Nothing to be done for the moment */
	} else if (curve_data.state == CURVE_STATE_OPENING) {
		bcb_sw_off();
		curve_data.state = CURVE_STATE_OPENED;
	} else if (curve_data.state == CURVE_STATE_EMERGENCY_OPENED) {
		/* Nothing to be done for the moment */
	} else {
	}
}

static void on_voltage_zero_detect(void)
{
	k_delayed_work_cancel(&curve_data.transient_work);
	process_timed_event();
}

static void on_ocp_activated(uint64_t on_off_duration)
{
	k_delayed_work_cancel(&curve_data.transient_work);
	curve_data.events_to_wait = 500;
	curve_data.state = CURVE_STATE_CLOSING;
	k_delayed_work_submit(&curve_data.transient_work, K_MSEC(TRANSIENT_WORK_TIMEOUT));

	if (curve_data.callback) {
		curve_data.callback(curve_data.trip_curve, BCB_TRIP_CAUSE_OCP_HW, 0);
	}
}

static void on_otp_activated(int8_t temp)
{
	k_delayed_work_cancel(&curve_data.transient_work);
	curve_data.events_to_wait = 500;
	curve_data.state = CURVE_STATE_CLOSING;
	k_delayed_work_submit(&curve_data.transient_work, K_MSEC(TRANSIENT_WORK_TIMEOUT));

	if (curve_data.callback) {
		curve_data.callback(curve_data.trip_curve, BCB_TRIP_CAUSE_OTP, 0);
	}
}

static void on_ocp_test_activated(bcb_ocp_direction_t direction, uint32_t duration)
{
	k_delayed_work_cancel(&curve_data.transient_work);
	curve_data.events_to_wait = 0;
	curve_data.state = CURVE_STATE_CLOSING;
	k_delayed_work_submit(&curve_data.transient_work, K_MSEC(TRANSIENT_WORK_TIMEOUT));
}

static void delayed_transient_work(struct k_work *work)
{
	process_timed_event();
}

static int trip_curve_init(void)
{
	LOG_INF("init");

	if (bcb_sw_is_on()) {
		curve_data.state = CURVE_STATE_CLOSED;
	} else {
		curve_data.state = CURVE_STATE_OPENED;
	}

	bcb_zd_add_callback(BCB_ZD_TYPE_VOLTAGE, &curve_data.zd_callback);
	bcb_ocp_add_callback(&curve_data.ocp_callback);
	bcb_otp_add_callback(&curve_data.otp_callback);
	bcb_ocp_test_add_callback(&curve_data.ocp_test_callback);
	return 0;
}

static int trip_curve_shutdown(void)
{
	LOG_INF("shutdown");

	bcb_zd_remove_callback(BCB_ZD_TYPE_VOLTAGE, &curve_data.zd_callback);
	bcb_ocp_remove_callback(&curve_data.ocp_callback);
	bcb_otp_remove_callback(&curve_data.otp_callback);
	bcb_ocp_test_remove_callback(&curve_data.ocp_test_callback);
	curve_data.state = CURVE_STATE_UNDEFINED;
	return 0;
}

static int trip_curve_close(void)
{
	if (curve_data.state == CURVE_STATE_OPENED) {
		curve_data.state = CURVE_STATE_CLOSING;
		k_delayed_work_submit(&curve_data.transient_work, K_MSEC(TRANSIENT_WORK_TIMEOUT));
	}

	return 0;
}

static int trip_curve_open(void)
{
	if (curve_data.state == CURVE_STATE_CLOSED) {
		curve_data.state = CURVE_STATE_OPENING;
		k_delayed_work_submit(&curve_data.transient_work, K_MSEC(TRANSIENT_WORK_TIMEOUT));
	}
	
	return 0;
}

static int trip_curve_set_current_limit(uint8_t limit)
{
	return 0;
}

static uint8_t trip_curve_get_current_limit(void)
{
	return 0;
}

static int trip_curve_set_callback(bcb_trip_curve_callback_t callback)
{
	if (callback) {
		return -ENOTSUP;
	}

	curve_data.callback = callback;

	return 0;
}

const struct bcb_trip_curve trip_curve_default = {
	.init = trip_curve_init,
	.shutdown = trip_curve_shutdown,
	.close = trip_curve_close,
	.open = trip_curve_open,
	.set_limit = trip_curve_set_current_limit,
	.get_limit = trip_curve_get_current_limit,
	.set_callback = trip_curve_set_callback,
};

static int trip_curve_system_init()
{
	memset(&curve_data, 0, sizeof(curve_data));
	k_delayed_work_init(&curve_data.transient_work, delayed_transient_work);
	curve_data.trip_curve = &trip_curve_default;
	curve_data.state = CURVE_STATE_UNDEFINED;
	curve_data.zd_callback.handler = on_voltage_zero_detect;
	curve_data.ocp_callback.handler = on_ocp_activated;
	curve_data.otp_callback.handler = on_otp_activated;
	curve_data.ocp_test_callback.handler = on_ocp_test_activated;

	bcb_set_trip_curve(&trip_curve_default);
	return 0;
}

SYS_INIT(trip_curve_system_init, APPLICATION, CONFIG_BCB_LIB_TRIP_CURVE_DEFAULT_INIT_PRIORITY);