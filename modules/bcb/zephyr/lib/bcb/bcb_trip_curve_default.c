#include "bcb_trip_curve_default.h"
#include "bcb_trip_curve.h"
#include "bcb_common.h"
#include "bcb_config.h"
#include "bcb.h"
#include "bcb_sw.h"
#include "bcb_zd.h"
#include <init.h>

#define TRANSIENT_WORK_TIMEOUT CONFIG_BCB_TRIP_CURVE_DEFAULT_TRANSIENT_WORK_TIMEOUT
#define MONITOR_WORK_INTERVAL CONFIG_BCB_TRIP_CURVE_DEFAULT_MONITOR_INTERVAL

//#define LOG_LEVEL CONFIG_BCB_TRIP_CURVE_DEFAULT_LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_tc_default);

typedef enum {
	CURVE_STATE_UNDEFINED = 0,
	CURVE_STATE_OPENED,
	CURVE_STATE_CLOSING,
	CURVE_STATE_CLOSED,
	CURVE_STATE_OPENING,
} curve_state_t;

typedef struct __attribute__((packed)) persistent_state {
	uint32_t recovery_duration;
	bool recovery_enabled;
	uint8_t limit_hw;
	uint8_t limit_sw;
	uint8_t limit_sw_hist;
	uint32_t limit_sw_duration;
} persistent_state_t;

struct curve_data {
	const struct bcb_trip_curve *trip_curve;
	bcb_trip_curve_callback_t callback;
	struct bcb_zd_callback zd_callback;
	struct bcb_sw_callback sw_callback;
	volatile curve_state_t state;
	volatile bcb_trip_cause_t cause;
	volatile uint32_t recovery_duration;
	volatile bool recovery_enabled;
	volatile bool max_events_waiting_updated;
	volatile uint32_t max_events_waiting;
	volatile uint32_t events_waiting;
	volatile uint8_t limit_hw;
	volatile uint8_t limit_sw;
	volatile uint8_t limit_sw_hist;
	volatile uint32_t limit_sw_duration;
	volatile bool initialized;
	struct k_delayed_work transient_work;
	struct k_delayed_work monitor_work;
	struct k_work store_work;
};

static struct curve_data curve_data;

static void set_cause_from_sw_cause(void)
{
	switch (bcb_sw_get_cause()) {
	case BCB_SW_CAUSE_OCP:
		curve_data.cause = BCB_TRIP_CAUSE_OCP_HW;
		break;
	case BCB_SW_CAUSE_OTP:
		curve_data.cause = BCB_TRIP_CAUSE_OTP;
		break;
	case BCB_SW_CAUSE_OCP_TEST:
		curve_data.cause = BCB_TRIP_CAUSE_OCP_TEST;
		break;
	case BCB_SW_CAUSE_UVP:
		curve_data.cause = BCB_TRIP_CAUSE_UVP;
		break;
	default:
		if (curve_data.cause != BCB_TRIP_CAUSE_OCP_SW) {
			curve_data.cause = BCB_TRIP_CAUSE_EXT;
		}
	}
}

static void process_timed_event(void)
{
	if (curve_data.state == CURVE_STATE_CLOSING) {
		int r;

		if (curve_data.events_waiting) {
			curve_data.events_waiting--;
			k_delayed_work_submit(&curve_data.transient_work,
					      K_MSEC(TRANSIENT_WORK_TIMEOUT));
			return;
		}

		r = bcb_sw_on();

		if (r) {
			set_cause_from_sw_cause();
			k_delayed_work_submit(&curve_data.transient_work,
					      K_MSEC(TRANSIENT_WORK_TIMEOUT));
		}

	} else if (curve_data.state == CURVE_STATE_OPENING) {
		bcb_sw_off();
	} else {
		/* We don't care other states for the moment. */
	}
}

static void on_voltage_zero_detect(void)
{
	if (curve_data.recovery_enabled && !curve_data.max_events_waiting_updated) {
		curve_data.max_events_waiting = (uint64_t)curve_data.recovery_duration * 2U *
						(uint64_t)bcb_zd_get_frequency() / 1e6;
		curve_data.max_events_waiting_updated = true;
		LOG_DBG("recovery events %" PRIu32, curve_data.max_events_waiting);
	}

	k_delayed_work_cancel(&curve_data.transient_work);
	process_timed_event();
}

static void on_switch_event(bool is_closed, bcb_sw_cause_t cause)
{
	k_delayed_work_cancel(&curve_data.transient_work);

	if (is_closed) {
		LOG_DBG("closed");
		curve_data.state = CURVE_STATE_CLOSED;
		curve_data.cause = BCB_TRIP_CAUSE_NONE;
	} else {
		LOG_DBG("opened");
		curve_data.state = CURVE_STATE_OPENED;
		set_cause_from_sw_cause();

		if (curve_data.cause == BCB_TRIP_CAUSE_EXT ||
		    curve_data.cause == BCB_TRIP_CAUSE_NONE) {
			return;
		}

		if (!curve_data.recovery_enabled) {
			return;
		}

		curve_data.events_waiting = curve_data.max_events_waiting;
		curve_data.state = CURVE_STATE_CLOSING;
		k_delayed_work_submit(&curve_data.transient_work, K_MSEC(TRANSIENT_WORK_TIMEOUT));
	}
}

static void transient_work(struct k_work *work)
{
	if (curve_data.recovery_enabled && !curve_data.max_events_waiting_updated) {
		curve_data.max_events_waiting =
			curve_data.recovery_duration / TRANSIENT_WORK_TIMEOUT;
		curve_data.max_events_waiting_updated = true;
		LOG_DBG("recovery events %" PRIu32, curve_data.max_events_waiting);
	}

	process_timed_event();
}

static void monitor_work(struct k_work *work)
{
	/* TODO: Implement current monitoring function */
}

static void restore_state_default(void)
{
	curve_data.recovery_enabled = false;
	curve_data.recovery_duration = 0;
	curve_data.limit_hw = 50;
	curve_data.limit_sw = 18;
	curve_data.limit_sw_hist = 1;
	curve_data.limit_sw_duration = 7200;
}

static int restore_state(void)
{
	int r;
	persistent_state_t state;

	r = bcb_config_load(CONFIG_BCB_LIB_PERSISTENT_CONFIG_OFFSET_TRIP_CURVE_DEFAULT,
			    (uint8_t *)&state, sizeof(state));
	if (r) {
		LOG_ERR("state restroing error: %d", r);
		restore_state_default();
		return r;
	}

	curve_data.recovery_enabled = state.recovery_enabled;
	curve_data.recovery_duration = state.recovery_duration;
	curve_data.limit_hw = state.limit_hw;
	curve_data.limit_sw = state.limit_sw;
	curve_data.limit_sw_hist = state.limit_sw_hist;
	curve_data.limit_sw_duration = state.limit_sw_duration;

	r = bcb_ocp_set_limit(curve_data.limit_hw);
	if (r) {
		LOG_ERR("cannot set hw current limit: %d", r);
		return r;
	}

	return 0;
}

static void state_store_work(struct k_work *work)
{
	int r;
	persistent_state_t state;

	state.recovery_enabled = curve_data.recovery_enabled;
	state.recovery_duration = curve_data.recovery_duration;
	state.limit_hw = curve_data.limit_hw;
	state.limit_sw = curve_data.limit_sw;
	state.limit_sw_hist = curve_data.limit_sw_hist;
	state.limit_sw_duration = curve_data.limit_sw_duration;

	r = bcb_config_store(CONFIG_BCB_LIB_PERSISTENT_CONFIG_OFFSET_TRIP_CURVE_DEFAULT,
			     (uint8_t *)&state, sizeof(state));
	if (r) {
		LOG_ERR("state storing error: %d", r);
	}
}

static int trip_curve_init(void)
{
	LOG_DBG("init");

	if (bcb_sw_is_on()) {
		curve_data.state = CURVE_STATE_CLOSED;
	} else {
		curve_data.state = CURVE_STATE_OPENED;
	}

	curve_data.cause = BCB_TRIP_CAUSE_NONE;
	bcb_zd_add_callback(BCB_ZD_TYPE_VOLTAGE, &curve_data.zd_callback);
	bcb_sw_add_callback(&curve_data.sw_callback);
	curve_data.initialized = true;

	restore_state();

	k_delayed_work_submit(&curve_data.transient_work, K_MSEC(TRANSIENT_WORK_TIMEOUT));
	k_delayed_work_submit(&curve_data.monitor_work, K_MSEC(MONITOR_WORK_INTERVAL));

	return 0;
}

static int trip_curve_shutdown(void)
{
	LOG_DBG("shutdown");

	curve_data.initialized = false;
	bcb_zd_remove_callback(BCB_ZD_TYPE_VOLTAGE, &curve_data.zd_callback);
	bcb_sw_remove_callback(&curve_data.sw_callback);
	curve_data.state = CURVE_STATE_UNDEFINED;
	curve_data.cause = BCB_TRIP_CAUSE_NONE;

	return 0;
}

static int trip_curve_close(void)
{
	if (!curve_data.initialized) {
		return -ENOTSUP;
	}

	LOG_DBG("close");

	curve_data.events_waiting = 0;
	curve_data.state = CURVE_STATE_CLOSING;
	curve_data.cause = BCB_TRIP_CAUSE_EXT;
	k_delayed_work_cancel(&curve_data.transient_work);
	k_delayed_work_submit(&curve_data.transient_work, K_MSEC(TRANSIENT_WORK_TIMEOUT));

	return 0;
}

static int trip_curve_open(void)
{
	if (!curve_data.initialized) {
		return -ENOTSUP;
	}

	LOG_DBG("open");

	curve_data.events_waiting = 0;
	curve_data.state = CURVE_STATE_OPENING;
	curve_data.cause = BCB_TRIP_CAUSE_EXT;
	k_delayed_work_cancel(&curve_data.transient_work);
	k_delayed_work_submit(&curve_data.transient_work, K_MSEC(TRANSIENT_WORK_TIMEOUT));

	return 0;
}

static bcb_curve_state_t trip_curve_get_state(void)
{
	switch (curve_data.state) {
	case CURVE_STATE_UNDEFINED:
		return BCB_CURVE_STATE_UNDEFINED;
	case CURVE_STATE_OPENED:
		return BCB_CURVE_STATE_OPENED;
	case CURVE_STATE_CLOSED:
		return BCB_CURVE_STATE_CLOSED;
	default:
		return BCB_CURVE_STATE_TRANSIENT;
	}

	return BCB_CURVE_STATE_UNDEFINED;
}

static bcb_trip_cause_t trip_curve_get_cause(void)
{
	return curve_data.cause;
}

static int trip_curve_set_limit_hw(uint8_t limit)
{
	int r;

	r = bcb_ocp_set_limit(limit);
	if (r) {
		LOG_ERR("cannot set hw current limit: %d", r);
		return r;
	}

	curve_data.limit_hw = limit;
	k_work_submit(&curve_data.store_work);

	return 0;
}

static uint8_t trip_curve_get_limit_hw(void)
{
	return curve_data.limit_hw;
}

static int trip_curve_set_limit_sw(uint8_t limit, uint8_t hist, uint32_t duration)
{
	curve_data.limit_sw = limit;
	curve_data.limit_sw_duration = duration;
	curve_data.limit_sw_hist = hist;
	k_work_submit(&curve_data.store_work);

	return 0;
}

static void trip_curve_get_limit_sw(uint8_t *limit, uint8_t *hist, uint32_t *duration)
{
	*limit = curve_data.limit_sw;
	*hist = curve_data.limit_sw_hist;
	*duration = curve_data.limit_sw_duration;
}

static int trip_curve_set_callback(bcb_trip_curve_callback_t callback)
{
	if (!callback) {
		return -ENOTSUP;
	}

	curve_data.callback = callback;

	return 0;
}

int bcb_trip_curve_default_set_recovery(uint32_t duration)
{
	if (!curve_data.initialized) {
		return -ENOTSUP;
	}

	curve_data.recovery_enabled = duration > 0 ? true : false;
	curve_data.recovery_duration = duration;

	if (curve_data.recovery_enabled) {
		curve_data.max_events_waiting_updated = false;
		k_delayed_work_submit(&curve_data.transient_work, K_MSEC(TRANSIENT_WORK_TIMEOUT));
	}

	k_work_submit(&curve_data.store_work);

	return 0;
}

uint32_t bcb_trip_curve_default_get_recovery(void)
{
	return curve_data.recovery_duration;
}

const struct bcb_trip_curve trip_curve_default = {
	.init = trip_curve_init,
	.shutdown = trip_curve_shutdown,
	.close = trip_curve_close,
	.open = trip_curve_open,
	.get_state = trip_curve_get_state,
	.get_cause = trip_curve_get_cause,
	.set_limit_hw = trip_curve_set_limit_hw,
	.get_limit_hw = trip_curve_get_limit_hw,
	.set_limit_sw = trip_curve_set_limit_sw,
	.get_limit_sw = trip_curve_get_limit_sw,
	.set_callback = trip_curve_set_callback,
};

const struct bcb_trip_curve *bcb_trip_curve_get_default(void)
{
	return &trip_curve_default;
}

static int trip_curve_system_init()
{
	memset(&curve_data, 0, sizeof(curve_data));
	k_delayed_work_init(&curve_data.transient_work, transient_work);
	k_delayed_work_init(&curve_data.monitor_work, monitor_work);
	k_work_init(&curve_data.store_work, state_store_work);
	curve_data.trip_curve = &trip_curve_default;
	curve_data.zd_callback.handler = on_voltage_zero_detect;
	curve_data.sw_callback.handler = on_switch_event;

	bcb_set_trip_curve(&trip_curve_default);
	return 0;
}

SYS_INIT(trip_curve_system_init, APPLICATION, CONFIG_BCB_LIB_TRIP_CURVE_DEFAULT_INIT_PRIORITY);