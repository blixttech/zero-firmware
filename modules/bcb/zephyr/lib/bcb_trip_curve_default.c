#include <lib/bcb_trip_curve_default.h>
#include <lib/bcb_trip_curve.h>
#include <lib/bcb_common.h>
#include <lib/bcb_config.h>
#include <lib/bcb.h>
#include <lib/bcb_sw.h>
#include <lib/bcb_zd.h>
#include <lib/bcb_msmnt.h>
#include <init.h>
#include <stdbool.h>

#define TRANSIENT_WORK_TIMEOUT CONFIG_BCB_TRIP_CURVE_DEFAULT_TRANSIENT_WORK_TIMEOUT
#define MONITOR_WORK_INTERVAL CONFIG_BCB_TRIP_CURVE_DEFAULT_MONITOR_INTERVAL
#define MAX_CURVE_POINTS CONFIG_BCB_TRIP_CURVE_DEFAULT_MAX_POINTS

#define SCALE_DURATION(d) ((uint16_t)(((d)*1000) / MONITOR_WORK_INTERVAL))

#define LOG_LEVEL CONFIG_BCB_TRIP_CURVE_DEFAULT_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_tc_default);

typedef enum {
	CURVE_STATE_UNDEFINED = 0,
	CURVE_STATE_OPENED,
	CURVE_STATE_CLOSING,
	CURVE_STATE_CLOSED,
	CURVE_STATE_OPENING,
} curve_state_t;

typedef struct __attribute__((packed)) persistent_curve_point {
	uint16_t i; /**< Current (A) */
	uint16_t d; /**< Duration */
} persistent_curve_point_t;

typedef struct __attribute__((packed)) persistent_params {
	uint16_t recovery_attempts; /**< Maximum number of recovery attempts.  */
	uint8_t limit_hw; /**< Hardware current limit. */
	uint8_t num_points; /**< Number of points of the trip curve. */
	persistent_curve_point_t points[MAX_CURVE_POINTS];
} persistent_params_t;

struct curve_data {
	bcb_trip_curve_callback_t callback;
	struct bcb_zd_callback zd_callback;
	struct bcb_sw_callback sw_callback;
	volatile curve_state_t state;
	volatile bool initialized;
	bcb_trip_cause_t cause;
	persistent_params_t params;
	uint32_t spent_duration[MAX_CURVE_POINTS];
	uint16_t recovery_remaining;
	struct k_delayed_work transient_work;
	struct k_delayed_work monitor_work;
	struct k_work callback_work;
};

static struct curve_data curve_data;
const struct bcb_trip_curve trip_curve_default;

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

static void do_transient_work(void)
{
	if (curve_data.state == CURVE_STATE_CLOSING) {
		int r = bcb_sw_on();
		if (r) {
			/* Immediate error while trying to turn of the switch.
			   This is not an over current situation.
			   Over temperature or something else. 
			 */
			set_cause_from_sw_cause();
		}
	} else if (curve_data.state == CURVE_STATE_OPENING) {
		bcb_sw_off();
	} else {
		/* We don't care other states in here. */
	}
}

static void on_voltage_zero_detect(void)
{
	k_delayed_work_cancel(&curve_data.transient_work);
	do_transient_work();
}

static void on_switch_event(bool is_closed, bcb_sw_cause_t cause)
{
	k_delayed_work_cancel(&curve_data.transient_work);

	if (is_closed) {
		LOG_INF("closed: %d", cause);
		curve_data.state = CURVE_STATE_CLOSED;
		curve_data.cause = BCB_TRIP_CAUSE_EXT;
	} else {
		LOG_INF("opened: %d", cause);
		curve_data.state = CURVE_STATE_OPENED;
		set_cause_from_sw_cause();

		if (curve_data.cause == BCB_TRIP_CAUSE_OCP_SW) {
			k_work_submit(&curve_data.callback_work);
			return;
		}

		if (curve_data.cause != BCB_TRIP_CAUSE_OCP_HW) {
			/* Recovery is available only for hardware based over current situations */
			return;
		}

		if (!curve_data.recovery_remaining) {
			k_work_submit(&curve_data.callback_work);
			return;
		}

		curve_data.recovery_remaining--;
		curve_data.state = CURVE_STATE_CLOSING;
		k_delayed_work_submit(&curve_data.transient_work, K_MSEC(TRANSIENT_WORK_TIMEOUT));
	}
}

static void transient_work(struct k_work *work)
{
	/* This work got executed since no voltage zero crossing detected.
	   Most likely, the mains voltage is DC. 
	 */
	do_transient_work();
}

static void monitor_work(struct k_work *work)
{
	int i;
	bool should_trip = false;
	uint16_t current = (uint16_t)(bcb_msmnt_get_current_rms() / 1000); /* Convert to amperes. */

	if (!curve_data.params.num_points) {
		goto shedule_monitor;
	}

	if (current < curve_data.params.points[0].i) {
		goto shedule_monitor;
	}

	for (i = 0; i < curve_data.params.num_points; i++) {
		if (current >= curve_data.params.points[i].i) {
			curve_data.spent_duration[i]++;
		} else {
			curve_data.spent_duration[i] = 0;
		}

		if (curve_data.spent_duration[i] >= curve_data.params.points[i].d) {
			should_trip = true;
		}
	}

	if (should_trip) {
		curve_data.recovery_remaining = 0;
		curve_data.state = CURVE_STATE_OPENING;
		curve_data.cause = BCB_TRIP_CAUSE_OCP_SW;
		k_delayed_work_cancel(&curve_data.transient_work);
		k_delayed_work_submit(&curve_data.transient_work, K_MSEC(TRANSIENT_WORK_TIMEOUT));
		/* We are going to open the switch. So no need to schedule monitor work. */
		return;
	} else {
		/* Nothing to be done in here. */
	}

shedule_monitor:
	k_delayed_work_submit(&curve_data.monitor_work, K_MSEC(MONITOR_WORK_INTERVAL));
}

static void callback_work(struct k_work *work)
{
	if (!curve_data.callback) {
		return;
	}

	curve_data.callback(&trip_curve_default, curve_data.cause);
}

static inline void load_default_params(void)
{
	LOG_INF("loading default params");
	/* Current limit values are for a 16 A class B */

	/* Hardware over current limit */
	curve_data.params.limit_hw = 60;
	/* No recovery attempts (disabled) */
	curve_data.params.recovery_attempts = 0;
	/* Number of points of the trip curve */
#if MAX_CURVE_POINTS == 16
	curve_data.params.num_points = 16;
	/* Duration values are calculated in multiples of 100ms */
	curve_data.params.points[0].i = 18;
	curve_data.params.points[0].d = SCALE_DURATION(5400.0f);
	curve_data.params.points[1].i = 20;
	curve_data.params.points[1].d = SCALE_DURATION(1600.0f);
	curve_data.params.points[2].i = 22;
	curve_data.params.points[2].d = SCALE_DURATION(500.0f);
	curve_data.params.points[3].i = 24;
	curve_data.params.points[3].d = SCALE_DURATION(180.0f);
	curve_data.params.points[4].i = 26;
	curve_data.params.points[4].d = SCALE_DURATION(90.0f);
	curve_data.params.points[5].i = 28;
	curve_data.params.points[5].d = SCALE_DURATION(50.0f);
	curve_data.params.points[6].i = 30;
	curve_data.params.points[6].d = SCALE_DURATION(30.0f);
	curve_data.params.points[7].i = 32;
	curve_data.params.points[7].d = SCALE_DURATION(20.0f);
	curve_data.params.points[8].i = 34;
	curve_data.params.points[8].d = SCALE_DURATION(13.0f);
	curve_data.params.points[9].i = 36;
	curve_data.params.points[9].d = SCALE_DURATION(8.5f);
	curve_data.params.points[10].i = 38;
	curve_data.params.points[10].d = SCALE_DURATION(6.0f);
	curve_data.params.points[11].i = 40;
	curve_data.params.points[11].d = SCALE_DURATION(4.2f);
	curve_data.params.points[12].i = 42;
	curve_data.params.points[12].d = SCALE_DURATION(2.8f);
	curve_data.params.points[13].i = 44;
	curve_data.params.points[13].d = SCALE_DURATION(1.9f);
	curve_data.params.points[14].i = 46;
	curve_data.params.points[14].d = SCALE_DURATION(1.4f);
	curve_data.params.points[15].i = 48;
	curve_data.params.points[15].d = SCALE_DURATION(1.0f);
#else
#warning The default trip curve with 16-points is disabled
	curve_data.params.num_points = 0;
#endif
}

static int restore_params(void)
{
	int r;

	r = bcb_config_load(CONFIG_BCB_LIB_PERSISTENT_CONFIG_OFFSET_TRIP_CURVE_DEFAULT,
			    (uint8_t *)&curve_data.params, sizeof(persistent_params_t));
	if (r) {
		LOG_ERR("cannot restore params from config: %d", r);
		load_default_params();
	}

	return r;
}

static int store_params(void)
{
	int r;

	r = bcb_config_store(CONFIG_BCB_LIB_PERSISTENT_CONFIG_OFFSET_TRIP_CURVE_DEFAULT,
			     (uint8_t *)&curve_data.params, sizeof(persistent_params_t));
	if (r) {
		LOG_ERR("cannot store params into config: %d", r);
	}

	return r;
}

static int trip_curve_init(void)
{
	int r;

	if (bcb_sw_is_on()) {
		curve_data.state = CURVE_STATE_CLOSED;
	} else {
		curve_data.state = CURVE_STATE_OPENED;
	}

	curve_data.cause = BCB_TRIP_CAUSE_EXT;
	curve_data.recovery_remaining = 0;
	bcb_zd_add_callback(BCB_ZD_TYPE_VOLTAGE, &curve_data.zd_callback);
	bcb_sw_add_callback(&curve_data.sw_callback);

	r = restore_params();
	if (r) {
		/* Restoring could fail while trying to old parameters. 
		   So store default values into the config to avoid future errors. 
		 */
		store_params();
	}

	r = bcb_ocp_set_limit(curve_data.params.limit_hw);
	if (r) {
		LOG_ERR("cannot set hw current limit: %d", r);
	}

	k_delayed_work_submit(&curve_data.transient_work, K_MSEC(TRANSIENT_WORK_TIMEOUT));
	k_delayed_work_submit(&curve_data.monitor_work, K_MSEC(MONITOR_WORK_INTERVAL));

	curve_data.initialized = true;

	LOG_INF("initialised");

	return 0;
}

static int trip_curve_shutdown(void)
{
	curve_data.initialized = false;
	bcb_zd_remove_callback(BCB_ZD_TYPE_VOLTAGE, &curve_data.zd_callback);
	bcb_sw_remove_callback(&curve_data.sw_callback);
	curve_data.state = CURVE_STATE_UNDEFINED;
	curve_data.cause = BCB_TRIP_CAUSE_NONE;

	LOG_INF("shutdown");

	return 0;
}

static int trip_curve_close(void)
{
	if (!curve_data.initialized) {
		return -ENOTSUP;
	}

	if (curve_data.state == CURVE_STATE_CLOSED) {
		return 0;
	}

	LOG_DBG("closing");

	memset(curve_data.spent_duration, 0, sizeof(curve_data.spent_duration));
	curve_data.recovery_remaining = curve_data.params.recovery_attempts;
	curve_data.state = CURVE_STATE_CLOSING;
	curve_data.cause = BCB_TRIP_CAUSE_EXT;
	k_delayed_work_cancel(&curve_data.transient_work);
	k_delayed_work_cancel(&curve_data.monitor_work);
	k_delayed_work_submit(&curve_data.transient_work, K_MSEC(TRANSIENT_WORK_TIMEOUT));
	k_delayed_work_submit(&curve_data.monitor_work, K_MSEC(MONITOR_WORK_INTERVAL));

	return 0;
}

static int trip_curve_open(void)
{
	if (!curve_data.initialized) {
		return -ENOTSUP;
	}

	if (curve_data.state == CURVE_STATE_OPENED) {
		return 0;
	}

	LOG_DBG("opening: ext");

	curve_data.recovery_remaining = 0;
	curve_data.state = CURVE_STATE_OPENING;
	curve_data.cause = BCB_TRIP_CAUSE_EXT;
	k_delayed_work_cancel(&curve_data.monitor_work);
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

	curve_data.params.limit_hw = limit;

	return store_params();
}

static uint8_t trip_curve_get_limit_hw(void)
{
	return curve_data.params.limit_hw;
}

static int trip_curve_set_callback(bcb_trip_curve_callback_t callback)
{
	if (!callback) {
		return -ENOTSUP;
	}

	curve_data.callback = callback;

	return 0;
}

int bcb_trip_curve_default_set_recovery(uint16_t attempts)
{
	if (!curve_data.initialized) {
		return -ENOTSUP;
	}

	curve_data.params.recovery_attempts = attempts;

	return store_params();
}

uint16_t bcb_trip_curve_default_get_recovery(void)
{
	return curve_data.params.recovery_attempts;
}

static int validate_points(const bcb_trip_curve_point_t *data, uint16_t points)
{
	int i;

	if (!data || !points) {
		return -EINVAL;
	}

	if (points == 1) {
		return 0;
	}

	for (i = 1; i < points; i++) {
		if (data[i - 1].i > data[i].i || data[i - 1].d < data[i].d) {
			return -EINVAL;
		}
	}

	return 0;
}

static int bcb_trip_curve_default_set_points(const bcb_trip_curve_point_t *data, uint16_t points)
{
	int i;

	if (validate_points(data, points) < 0) {
		LOG_ERR("invalid curve points");
		return -ENOTSUP;
	}

	if (points > MAX_CURVE_POINTS) {
		LOG_ERR("Not enough space for curve points");
		return -ENOMEM;
	}

	curve_data.params.num_points = points;
	for (i = 0; i < points; i++) {
		curve_data.params.points[i].i = data[i].i;
		curve_data.params.points[i].d = data[i].d;
	}

	return 0;
}

static int bcb_trip_curve_default_get_points(bcb_trip_curve_point_t *data, uint16_t points)
{
	int i;

	if (!data) {
		LOG_ERR("invalid destination");
		return -ENOTSUP;
	}

	if (points > MAX_CURVE_POINTS) {
		LOG_ERR("invalid number of curve points");
		return -EINVAL;
	}

	for (i = 0; i < points; i++) {
		data[i].i = curve_data.params.points[i].i;
		data[i].d = curve_data.params.points[i].d;
	}

	return 0;
}

static uint16_t bcb_trip_curve_default_get_point_count(void)
{
	return curve_data.params.num_points;
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
	.set_callback = trip_curve_set_callback,
	.set_points = bcb_trip_curve_default_set_points,
	.get_points = bcb_trip_curve_default_get_points,
	.get_point_count = bcb_trip_curve_default_get_point_count,
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
	k_work_init(&curve_data.callback_work, callback_work);
	curve_data.zd_callback.handler = on_voltage_zero_detect;
	curve_data.sw_callback.handler = on_switch_event;
	return 0;
}

SYS_INIT(trip_curve_system_init, APPLICATION, CONFIG_BCB_LIB_TRIP_CURVE_DEFAULT_INIT_PRIORITY);