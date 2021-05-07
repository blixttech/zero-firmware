#include "bcb_tc_def.h"
#include "bcb_config.h"
#include "bcb_sw.h"
#include "bcb_zd.h"
#include <init.h>

#define LOG_LEVEL CONFIG_BCB_TRIP_CURVE_DEFAULT_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_tc_def);

typedef struct __attribute__((packed)) persistent_config {
	bcb_tc_limit_config_t limit_config;
	bcb_tc_inrush_config_t inrush_config;
} persistent_config_t;

typedef enum {
	CURVE_STATE_UNDEFINED = 0,
	CURVE_STATE_OPENED,
	CURVE_STATE_CLOSING,
	CURVE_STATE_CLOSED,
	CURVE_STATE_OPENING,
} curve_state_t;

struct curve_data {
	persistent_config_t config;
	volatile curve_state_t state;
	volatile bool initialized;
	bcb_tc_callback_t tc_callback;
	struct bcb_zd_callback zd_callback;
	struct bcb_sw_callback sw_callback;
};

static struct curve_data curve_data;

static void on_voltage_zero_detect(void)
{
}

static void on_switch_event(bool is_closed, bcb_sw_cause_t cause)
{
}

static void restore_default_config(void)
{
	curve_data.config.limit_config.inst_limit = 50; /* In amperes */
	curve_data.config.limit_config.long_limit = 16; /* In amperes */
	curve_data.config.limit_config.short_limit = 30; /* In amperes */
	curve_data.config.limit_config.long_delay = 86400000U; /* In milliseconds (24 hours) */
	curve_data.config.limit_config.short_delay = 10000U; /* In milliseconds (10 seconds) */

	curve_data.config.inrush_config.rec_enable = false;
	curve_data.config.inrush_config.max_attemps = 100;
	curve_data.config.inrush_config.wait = 50; /* In milliseconds */
}

static int trip_curve_init(void)
{
	int r;

	r = bcb_config_load(CONFIG_BCB_LIB_PERSISTENT_CONFIG_OFFSET_TRIP_CURVE_DEFAULT,
			    (uint8_t *)&curve_data.config, sizeof(persistent_config_t));
	if (r) {
		LOG_ERR("config loading error: %d", r);
		restore_default_config();
	}

	r = bcb_ocp_set_limit(curve_data.config.limit_config.inst_limit);
	if (r) {
		LOG_ERR("cannot set hw current limit: %d", r);
	}

	bcb_zd_add_callback(BCB_ZD_TYPE_VOLTAGE, &curve_data.zd_callback);
	bcb_sw_add_callback(&curve_data.sw_callback);

	if (bcb_sw_is_on()) {
		curve_data.state = CURVE_STATE_CLOSED;
	} else {
		curve_data.state = CURVE_STATE_OPENED;
	}

	LOG_INF("initialised");

	return 0;
}

static int trip_curve_shutdown(void)
{
	bcb_zd_remove_callback(BCB_ZD_TYPE_VOLTAGE, &curve_data.zd_callback);
	bcb_sw_remove_callback(&curve_data.sw_callback);
	curve_data.tc_callback = NULL;
	curve_data.state = CURVE_STATE_UNDEFINED;

	LOG_INF("shutdown");

	return 0;
}

static int trip_curve_close(void)
{
	if (curve_data.state == CURVE_STATE_UNDEFINED) {
		return -ENOTSUP;
	}

	LOG_INF("closing");

	return 0;
}

static int trip_curve_open(void)
{
	if (curve_data.state == CURVE_STATE_UNDEFINED) {
		return -ENOTSUP;
	}

	LOG_INF("opening");

	return 0;
}

static bcb_tc_state_t trip_curve_get_state(void)
{
	switch (curve_data.state) {
	case CURVE_STATE_UNDEFINED:
		return BCB_TC_STATE_UNDEFINED;
	case CURVE_STATE_OPENED:
		return BCB_TC_STATE_OPENED;
	case CURVE_STATE_CLOSED:
		return BCB_TC_STATE_CLOSED;
	default:
		return BCB_TC_STATE_TRANSIENT;
	}

	return BCB_TC_STATE_UNDEFINED;
}

static bcb_tc_cause_t trip_curve_get_cause(void)
{
	return BCB_TC_CAUSE_NONE;
}

static int trip_curve_set_limit_config(const bcb_tc_limit_config_t *limit)
{
	int r;

	if (!limit) {
		return -EINVAL;
	}

	/* TODO: Validate new limits */

	if (limit->inst_limit != curve_data.config.limit_config.inst_limit) {
		r = bcb_ocp_set_limit(curve_data.config.limit_config.inst_limit);
		if (r) {
			LOG_ERR("cannot set hw current limit: %d", r);
			return r;
		}
	}

	memcpy(&curve_data.config.limit_config, limit, sizeof(bcb_tc_limit_config_t));

	r = bcb_config_store(CONFIG_BCB_LIB_PERSISTENT_CONFIG_OFFSET_TRIP_CURVE_DEFAULT,
			    (uint8_t *)&curve_data.config, sizeof(persistent_config_t));
	if (r) {
		LOG_ERR("config storing error: %d", r);
	}

	return r;
}

static int trip_curve_get_limit_config(bcb_tc_limit_config_t *limit)
{
	if (!limit) {
		return -EINVAL;
	}

	memcpy(limit, &curve_data.config.limit_config, sizeof(bcb_tc_limit_config_t));

	return 0;
}

static int trip_curve_set_inrush_config(const bcb_tc_inrush_config_t *config)
{
	if (!config) {
		return -EINVAL;
	}

	/* TODO: Validate new config */

	return 0;
}

static int trip_curve_get_inrush_config(bcb_tc_inrush_config_t *config)
{
	if (!config) {
		return -EINVAL;
	}

	memcpy(config, &curve_data.config.inrush_config, sizeof(bcb_tc_inrush_config_t));

	return 0;
}

static int trip_curve_set_callback(bcb_tc_callback_t callback)
{
	if (!callback) {
		return -EINVAL;
	}

	curve_data.tc_callback = callback;

	return 0;
}

const bcb_tc_t tc_def = {
	.init = trip_curve_init,
	.shutdown = trip_curve_shutdown,
	.close = trip_curve_close,
	.open = trip_curve_open,
	.get_state = trip_curve_get_state,
	.get_cause = trip_curve_get_cause,
	.set_limit_config = trip_curve_set_limit_config,
	.get_limit_config = trip_curve_get_limit_config,
	.set_inrush_config = trip_curve_set_inrush_config,
	.get_inrush_config = trip_curve_get_inrush_config,
	.set_callback = trip_curve_set_callback,
};

const bcb_tc_t *bcb_tc_get_default(void)
{
	return &tc_def;
}

static int trip_curve_system_init()
{
	memset(&curve_data, 0, sizeof(curve_data));
	curve_data.sw_callback.handler = on_switch_event;
	curve_data.zd_callback.handler = on_voltage_zero_detect;

	return 0;
}

SYS_INIT(trip_curve_system_init, APPLICATION, CONFIG_BCB_LIB_TRIP_CURVE_DEFAULT_INIT_PRIORITY);