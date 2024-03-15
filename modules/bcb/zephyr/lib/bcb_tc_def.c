#include <lib/bcb_tc_def.h>
#include <lib/bcb_tc.h>
#include <lib/bcb_tc_def_msm.h>
#include <lib/bcb_common.h>
#include <lib/bcb_config.h>
#include <lib/bcb.h>
#include <lib/bcb_sw.h>
#include <lib/bcb_zd.h>
#include <lib/bcb_msmnt.h>
#include <init.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// clang-format off
#define CONFIG_OFFSET		CONFIG_BCB_LIB_PERSISTENT_CONFIG_OFFSET_TC_DEF
#define MONITOR_WORK_INTERVAL	CONFIG_BCB_TRIP_CURVE_DEFAULT_MONITOR_INTERVAL
#define MAX_CURVE_POINTS	CONFIG_BCB_TRIP_CURVE_DEFAULT_MAX_POINTS
#define LOG_LEVEL 		CONFIG_BCB_TRIP_CURVE_DEFAULT_LOG_LEVEL
// clang-format on

#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_tc_default);

typedef struct tc_def_config {
	bcb_tc_pt_t points[MAX_CURVE_POINTS];
	uint8_t num_points; /**< Number of points of the trip curve. */
	uint8_t limit_hw; /**< Hardware current limit. */
} tc_def_config_t;

struct curve_data {
	bool initialized;
	tc_def_config_t config;
	uint32_t spent_duration[MAX_CURVE_POINTS];
	struct k_delayed_work monitor_work;
	struct k_work callback_work;
	bcb_tc_callback_handler_t callback;
	struct bcb_zd_callback zd_callback;
	struct bcb_sw_callback sw_callback;
};

static struct curve_data curve_data;
const struct bcb_tc trip_curve_default;

static inline void curve_reset()
{
	int i;

	for (i = 0; i < curve_data.config.num_points; i++) {
		curve_data.spent_duration[i] = 0;
	}
}

static void on_monitor_work(struct k_work *work)
{
	int i;
	bool should_trip = false;
	uint32_t current = bcb_msmnt_get_current_rms();

	if (!curve_data.config.num_points) {
		goto shedule_monitor;
	}

	if (current < curve_data.config.points[0].i) {
		goto shedule_monitor;
	}

	for (i = 0; i < curve_data.config.num_points; i++) {
		if (current >= curve_data.config.points[i].i) {
			curve_data.spent_duration[i] += MONITOR_WORK_INTERVAL;
			if (curve_data.spent_duration[i] >= curve_data.config.points[i].d) {
				should_trip = true;
			}
		} else {
			curve_data.spent_duration[i] = 0;
		}
	}

	if (should_trip) {
		bcb_tc_def_msm_event(BCB_TC_DEF_EV_OCD, NULL);
		return;
	} else {
		/* Nothing to be done in here. */
	}

shedule_monitor:
	k_delayed_work_submit(&curve_data.monitor_work, K_MSEC(MONITOR_WORK_INTERVAL));
}

static inline void load_default_config(void)
{
	LOG_INF("loading default config");

	/* Current limit values are for a 16 A class B */
	/* Hardware over current limit */
	curve_data.config.limit_hw = 60;
	/* Number of points of the trip curve */
#if MAX_CURVE_POINTS == 16
	curve_data.config.num_points = 16;
	curve_data.config.points[0].i = 18000;
	curve_data.config.points[0].d = 5400000;
	curve_data.config.points[1].i = 20000;
	curve_data.config.points[1].d = 1600000;
	curve_data.config.points[2].i = 22000;
	curve_data.config.points[2].d = 500000;
	curve_data.config.points[3].i = 24000;
	curve_data.config.points[3].d = 180000;
	curve_data.config.points[4].i = 26000;
	curve_data.config.points[4].d = 90000;
	curve_data.config.points[5].i = 28000;
	curve_data.config.points[5].d = 50000;
	curve_data.config.points[6].i = 30000;
	curve_data.config.points[6].d = 30000;
	curve_data.config.points[7].i = 32000;
	curve_data.config.points[7].d = 20000;
	curve_data.config.points[8].i = 34000;
	curve_data.config.points[8].d = 13000;
	curve_data.config.points[9].i = 36000;
	curve_data.config.points[9].d = 8500;
	curve_data.config.points[10].i = 38000;
	curve_data.config.points[10].d = 6000;
	curve_data.config.points[11].i = 40000;
	curve_data.config.points[11].d = 4200;
	curve_data.config.points[12].i = 42000;
	curve_data.config.points[12].d = 2800;
	curve_data.config.points[13].i = 44000;
	curve_data.config.points[13].d = 1900;
	curve_data.config.points[14].i = 46000;
	curve_data.config.points[14].d = 1400;
	curve_data.config.points[15].i = 48000;
	curve_data.config.points[15].d = 1000;
#else
#warning The default trip curve with 16-points is disabled
	curve_data.params.num_points = 0;
#endif
}

static void on_zd_voltage(void)
{
	bcb_tc_def_msm_event(BCB_TC_DEF_EV_ZD_V, NULL);
}

static void on_switch_changed(bool is_closed, bcb_sw_cause_t cause)
{
	if (is_closed) {
		bcb_tc_def_msm_event(BCB_TC_DEF_EV_SW_CLOSED, NULL);
	} else {
		bcb_tc_def_msm_event(BCB_TC_DEF_EV_SW_OPENED, NULL);
	}
}

static int restore_config(void)
{
	int r;

	r = bcb_config_load(CONFIG_OFFSET, (uint8_t *)&curve_data.config, sizeof(tc_def_config_t));
	if (r) {
		LOG_ERR("cannot restore params: %d", r);
	}

	return r;
}

static int store_config(void)
{
	int r;

	r = bcb_config_store(CONFIG_OFFSET, (uint8_t *)&curve_data.config, sizeof(tc_def_config_t));
	if (r) {
		LOG_ERR("cannot store params: %d", r);
	}

	return r;
}

static int trip_curve_init(void)
{
	if (restore_config()) {
		/* Restoring could fail while trying to load old parameters. 
		   So store default values into the config to avoid future errors.
		   NOTE: This effectively overwrites the old parameters.
		 */
		load_default_config();
		store_config();
	}

	bcb_ocp_set_limit(curve_data.config.limit_hw);

	curve_data.zd_callback.handler = on_zd_voltage;
	curve_data.sw_callback.handler = on_switch_changed;
	bcb_zd_add_callback(BCB_ZD_TYPE_VOLTAGE, &curve_data.zd_callback);
	bcb_sw_add_callback(&curve_data.sw_callback);

	bcb_tc_def_msm_init(&curve_data.callback_work);

	curve_data.initialized = true;

	LOG_INF("initialised");

	return 0;
}

static int trip_curve_shutdown(void)
{
	curve_data.initialized = false;
	bcb_zd_remove_callback(BCB_ZD_TYPE_VOLTAGE, &curve_data.zd_callback);
	bcb_sw_remove_callback(&curve_data.sw_callback);

	LOG_INF("shutdown");

	return 0;
}

static int trip_curve_close(void)
{
	if (!curve_data.initialized) {
		return -ENOTSUP;
	}

	curve_reset();
	bcb_tc_def_msm_event(BCB_TC_DEF_EV_CMD_CLOSE, NULL);
	k_delayed_work_submit(&curve_data.monitor_work, K_MSEC(MONITOR_WORK_INTERVAL));

	return 0;
}

static int trip_curve_open(void)
{
	if (!curve_data.initialized) {
		return -ENOTSUP;
	}

	bcb_tc_def_msm_event(BCB_TC_DEF_EV_CMD_OPEN, NULL);
	k_delayed_work_cancel(&curve_data.monitor_work);

	return 0;
}

static bcb_tc_state_t trip_curve_get_state(void)
{
	switch (bcb_tc_def_msm_get_state()) {
	case BCB_TC_DEF_MSM_STATE_UNDEFINED:
		return BCB_TC_STATE_UNDEFINED;
	case BCB_TC_DEF_MSM_STATE_OPENED:
		return BCB_TC_STATE_OPENED;
	case BCB_TC_DEF_MSM_STATE_CLOSED:
		return BCB_TC_STATE_CLOSED;
	default:
		return BCB_TC_STATE_TRANSIENT;
	}

	return BCB_TC_STATE_UNDEFINED;
}

static bcb_tc_cause_t trip_curve_get_cause(void)
{
	bcb_tc_cause_t trip_cause;

	switch (bcb_tc_def_msm_get_cause()) {
	case BCB_TC_DEF_MSM_CAUSE_NONE:
		trip_cause = BCB_TC_CAUSE_NONE;
		break;
	case BCB_TC_DEF_MSM_CAUSE_EXT:
		trip_cause = BCB_TC_CAUSE_EXT;
		break;
	case BCB_TC_DEF_MSM_CAUSE_OCP:
		trip_cause = BCB_TC_CAUSE_OCP_HW;
		break;
	case BCB_TC_DEF_MSM_CAUSE_OCD:
		trip_cause = BCB_TC_CAUSE_OCP_SW;
		break;
	default:
		switch (bcb_sw_get_cause()) {
		case BCB_SW_CAUSE_OTP:
			trip_cause = BCB_TC_CAUSE_OTP;
			break;
		case BCB_SW_CAUSE_OCP_TEST:
			trip_cause = BCB_TC_CAUSE_OCP_TEST;
			break;
		case BCB_SW_CAUSE_UVP:
			trip_cause = BCB_TC_CAUSE_UVP;
			break;
		default:
			trip_cause = BCB_TC_CAUSE_NONE;
			break;
		}
		break;
	}

	return trip_cause;
}

static int trip_curve_set_limit_hw(uint8_t limit)
{
	int r;

	r = bcb_ocp_set_limit(limit);
	if (r) {
		LOG_ERR("cannot set hw current limit: %d", r);
		return r;
	}

	curve_data.config.limit_hw = limit;

	return store_config();
}

static uint8_t trip_curve_get_limit_hw(void)
{
	return curve_data.config.limit_hw;
}

static int trip_curve_set_callback(bcb_tc_callback_handler_t callback)
{
	if (!callback) {
		return -ENOTSUP;
	}

	curve_data.callback = callback;

	return 0;
}

static int validate_points(const bcb_tc_pt_t *data, uint8_t points)
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

static int bcb_trip_curve_default_set_points(const bcb_tc_pt_t *points, uint8_t count)
{
	if (validate_points(points, count) != 0) {
		LOG_ERR("invalid curve points");
		return -ENOTSUP;
	}

	if (count > MAX_CURVE_POINTS) {
		LOG_ERR("Not enough space for curve points");
		return -ENOMEM;
	}

	curve_data.config.num_points = count;
	memcpy(&curve_data.config.points, points, sizeof(bcb_tc_pt_t) * count);

	return store_config();
}

static int bcb_trip_curve_default_get_points(bcb_tc_pt_t *points, uint8_t *count)
{
	if (*count > curve_data.config.num_points) {
		*count = curve_data.config.num_points;
	}

	memcpy(points, &curve_data.config.points[0], sizeof(bcb_tc_pt_t) * (*count));

	return 0;
}

static void on_callback_work(struct k_work *work)
{
	if (!curve_data.callback) {
		return;
	}

	curve_data.callback(&trip_curve_default, trip_curve_get_cause());
}

const struct bcb_tc trip_curve_default = {
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
};

const bcb_tc_t* bcb_tc_get_default(void)
{
	return &trip_curve_default;
}

static int trip_curve_system_init()
{
	memset(&curve_data, 0, sizeof(curve_data));
	k_delayed_work_init(&curve_data.monitor_work, on_monitor_work);
	k_work_init(&curve_data.callback_work, on_callback_work);
	return 0;
}

SYS_INIT(trip_curve_system_init, APPLICATION, CONFIG_BCB_LIB_TRIP_CURVE_DEFAULT_INIT_PRIORITY);