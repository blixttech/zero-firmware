#include <lib/bcb_tc_def_msm.h>
#include <lib/bcb_tc_def_csom_mod.h>
#include <lib/bcb_tc.h>
#include <lib/bcb_config.h>
#include <lib/bcb_sw.h>
#include <lib/bcb_zd.h>
#include <logging/log.h>
#include <init.h>
#include <string.h>

// clang-format off
#define CONFIG_OFFSET                       CONFIG_BCB_LIB_PERSISTENT_CONFIG_OFFSET_TC_DEF_MSM
#define SUPPLY_WORK_TIMEOUT                 CONFIG_BCB_TRIP_CURVE_DEFAULT_SUPPLY_TIMER_TIMEOUT
#define RECOVERY_WORK_TIMEOUT               CONFIG_BCB_TRIP_CURVE_DEFAULT_RECOVERY_TIMER_TIMEOUT
#define RECOVERY_RESET_WORK_TIMEOUT         CONFIG_BCB_TRIP_CURVE_DEFAULT_RECOVERY_RESET_TIMER_TIMEOUT
#define RECOVERY_RESET_WORK_TIMEOUT_MAX     CONFIG_BCB_TRIP_CURVE_DEFAULT_RECOVERY_RESET_TIMER_TIMEOUT_MAX
#define RECOVERY_RESET_WORK_TIMEOUT_MIN     CONFIG_BCB_TRIP_CURVE_DEFAULT_RECOVERY_RESET_TIMER_TIMEOUT_MIN
#define ZD_COUNT_SUPPLY_WAIT                CONFIG_BCB_TRIP_CURVE_DEFAULT_SUPPLY_ZD_COUNT_MIN
#define LOG_LEVEL                           CONFIG_BCB_TRIP_CURVE_DEFAULT_LOG_LEVEL
// clang-format on

LOG_MODULE_REGISTER(bcb_tc_def_msm);

#define MSM_EV_FILTER_ADD(var, ev)                                                                 \
	do {                                                                                       \
		(var) = ((var) | (1 << ev));                                                       \
	} while (0)

#define MSM_EV_FILTER_REM(var, ev)                                                                 \
	do {                                                                                       \
		(var) = ((var) & ~(1 << ev));                                                      \
	} while (0)

#define MSM_EV_FILTERED(var, ev) ((var) & (1 << ev))

struct tc_def_msm_data {
	bcb_tc_def_msm_config_t config;
	bcb_tc_def_msm_state_t state;
	bcb_tc_def_msm_cause_t cause;
	bcb_tc_def_msm_csom_t csom;
	uint8_t zd_count;
	bool is_ac_supply;
	uint32_t ev_filter;
	uint16_t recovery_remaining;
	struct k_work *notify_work;
	struct k_delayed_work supply_detect_work;
	struct k_delayed_work recovery_work;
	struct k_delayed_work recovery_reset_work;
};

static struct tc_def_msm_data msm_data;

static inline void msm_csom_cleanup(void);

static void set_cause_from_sw_cause(bcb_sw_cause_t sw_cause)
{
	switch (sw_cause) {
	case BCB_SW_CAUSE_OCP:
		msm_data.cause = BCB_TC_DEF_MSM_CAUSE_OCP;
		break;
	case BCB_SW_CAUSE_EXT:
		/* Cause should be already set by the trip curve */
		break;
	default:
		msm_data.cause = BCB_TC_DEF_MSM_CAUSE_OTHER;
		break;
	}
}

static inline void msm_on_cmd_close_at_opened(void)
{
	LOG_INF("close");
	k_delayed_work_cancel(&msm_data.supply_detect_work);
	msm_data.zd_count = 0;
	msm_data.state = BCB_TC_DEF_MSM_STATE_SUPPLY_WAIT;
	msm_data.cause = BCB_TC_DEF_MSM_CAUSE_EXT;
	msm_data.csom = BCB_TC_DEF_MSM_CSOM_NONE;
	msm_data.recovery_remaining = msm_data.config.rec_attempts;
	MSM_EV_FILTER_REM(msm_data.ev_filter, BCB_TC_DEF_EV_ZD_V);
	k_delayed_work_submit(&msm_data.supply_detect_work, K_MSEC(SUPPLY_WORK_TIMEOUT));
}

static inline void msm_on_cmd_open_at_supply_wait(void)
{
	LOG_INF("open");
	k_delayed_work_cancel(&msm_data.supply_detect_work);
	MSM_EV_FILTER_ADD(msm_data.ev_filter, BCB_TC_DEF_EV_ZD_V);
	msm_data.state = BCB_TC_DEF_MSM_STATE_OPENED;
	msm_data.cause = BCB_TC_DEF_MSM_CAUSE_EXT;
}

static inline void msm_on_zd_v_at_supply_wait(void)
{
	msm_data.zd_count++;
}

static inline void msm_on_supply_timer_at_supply_wait(void)
{
	uint8_t zd_count = msm_data.zd_count;
	msm_data.zd_count = 0;
	msm_data.state = BCB_TC_DEF_MSM_STATE_CLOSE_WAIT;

	if (zd_count < ZD_COUNT_SUPPLY_WAIT) {
		/* We have a DC supply */
		int r;

		msm_data.is_ac_supply = false;
		MSM_EV_FILTER_ADD(msm_data.ev_filter, BCB_TC_DEF_EV_ZD_V);

		r = bcb_sw_on();
		if (r) {
			bcb_sw_cause_t sw_cause = bcb_sw_get_cause();
			set_cause_from_sw_cause(sw_cause);
		}
	} else {
		/* We have an AC supply */
		msm_data.is_ac_supply = true;
	}

	LOG_INF("ac: %d", (uint8_t)msm_data.is_ac_supply);
}

static void msm_on_cmd_open_before_open_wait(void)
{
	LOG_INF("open");

	if (!bcb_sw_is_on()) {
		/* CSOM has already opened the switch. */
		msm_data.state = BCB_TC_DEF_MSM_STATE_OPENED;
		msm_data.cause = BCB_TC_DEF_MSM_CAUSE_EXT;
		MSM_EV_FILTER_ADD(msm_data.ev_filter, BCB_TC_DEF_EV_ZD_V);
		return;
	}

	msm_data.state = BCB_TC_DEF_MSM_STATE_OPEN_WAIT;
	msm_data.cause = BCB_TC_DEF_MSM_CAUSE_EXT;

	if (!msm_data.is_ac_supply) {
		bcb_sw_off();
	}
}

static inline void msm_on_zd_v_rec_timer_at_close_wait(void)
{
	k_delayed_work_cancel(&msm_data.recovery_work);

	if (bcb_sw_on()) {
		set_cause_from_sw_cause(bcb_sw_get_cause());
	}
}

static inline void msm_on_sw_closed_at_close_wait(void)
{
	msm_data.state = BCB_TC_DEF_MSM_STATE_CLOSED;
	if (msm_data.recovery_remaining < msm_data.config.rec_attempts) {
		k_delayed_work_submit(&msm_data.recovery_reset_work,
				      K_MSEC(msm_data.config.rec_reset_timeout));
	}
}

static inline void msm_on_ocd_at_closed(void)
{
	msm_data.state = BCB_TC_DEF_MSM_STATE_OPEN_WAIT;
	msm_data.cause = BCB_TC_DEF_MSM_CAUSE_OCD;

	if (!msm_data.is_ac_supply) {
		bcb_sw_off();
	}
}

static inline void msm_on_sw_opened_at_closed(void)
{
	k_delayed_work_cancel(&msm_data.recovery_reset_work);

	bcb_sw_cause_t sw_cause = bcb_sw_get_cause();
	if (sw_cause == BCB_SW_CAUSE_EXT) {
		/* Opened by CSOM */
		return;
	}

	if (sw_cause != BCB_SW_CAUSE_OCP) {
		MSM_EV_FILTER_ADD(msm_data.ev_filter, BCB_TC_DEF_EV_ZD_V);
		msm_data.state = BCB_TC_DEF_MSM_STATE_OPENED;
		set_cause_from_sw_cause(sw_cause);
		k_work_submit(msm_data.notify_work);
		return;
	}

	if (!msm_data.recovery_remaining) {
		MSM_EV_FILTER_ADD(msm_data.ev_filter, BCB_TC_DEF_EV_ZD_V);
		msm_data.state = BCB_TC_DEF_MSM_STATE_OPENED;
		set_cause_from_sw_cause(sw_cause);
		k_work_submit(msm_data.notify_work);
		return;
	}

	msm_data.recovery_remaining--;
	msm_data.state = BCB_TC_DEF_MSM_STATE_CLOSE_WAIT;

	if (!msm_data.is_ac_supply) {
		k_delayed_work_submit(&msm_data.recovery_work, K_USEC(msm_data.config.rec_delay));
	}
}

static inline void msm_on_zd_v_at_open_wait(void)
{
	bcb_sw_off();
}

static inline void msm_on_sw_opened_at_open_wait(void)
{
	msm_data.state = BCB_TC_DEF_MSM_STATE_OPENED;
	MSM_EV_FILTER_ADD(msm_data.ev_filter, BCB_TC_DEF_EV_ZD_V);
}

static inline void msm_csom_cleanup(void)
{
	switch (msm_data.csom) {
	case BCB_TC_DEF_MSM_CSOM_MOD: {
		bcb_tc_def_csom_mod_cleanup();
	} break;
	default: {
	} break;
	}

	if (bcb_sw_is_on()) {
		return;
	}
	/* Need to re-close the switch since we are still in the closed state. */
	msm_data.state = BCB_TC_DEF_MSM_STATE_CLOSE_WAIT;

	if (msm_data.is_ac_supply) {
		return;
	}
	/* We have a DC supply. 
	 * We have to re-close now since there will be no closing at the zero-crossing.
	 */
	if (bcb_sw_on()) {
		set_cause_from_sw_cause(bcb_sw_get_cause());
	}
}

static inline int msm_csom_mod(bcb_tc_def_event_t event, void *arg)
{
	if (!msm_data.is_ac_supply) {
		/* Modulation control is applicable only to AC supplies. */
		return -ENOTSUP;
	}

	return bcb_tc_def_csom_mod_event(event, arg);
}

static inline int msm_csom_event(bcb_tc_def_event_t event, void *arg)
{
	int r = 0;

	if (msm_data.state != BCB_TC_DEF_MSM_STATE_CLOSED) {
		return 0;
	}

	if (msm_data.csom != msm_data.config.csom) {
		/* CSOM has been changed. Cleanup and re-close the switch if needed. */
		msm_data.csom = msm_data.config.csom;
		msm_csom_cleanup();
		return 0;
	}

	switch (msm_data.csom) {
	case BCB_TC_DEF_MSM_CSOM_MOD: {
		r = msm_csom_mod(event, arg);
	} break;
	default: {
	} break;
	}

	return r;
}

static int restore_config(void)
{
	int r;

	r = bcb_config_load(CONFIG_OFFSET, (uint8_t *)&msm_data.config,
			    sizeof(bcb_tc_def_msm_config_t));
	if (r) {
		LOG_ERR("cannot restore params: %d", r);
	}

	return r;
}

static int store_config(void)
{
	int r;

	r = bcb_config_store(CONFIG_OFFSET, (uint8_t *)&msm_data.config,
			     sizeof(bcb_tc_def_msm_config_t));
	if (r) {
		LOG_ERR("cannot store params: %d", r);
	}

	return r;
}

static void load_default_config(void)
{
	LOG_INF("loading default config");

	msm_data.config.csom = BCB_TC_DEF_MSM_CSOM_NONE;
	msm_data.config.rec_enabled = false;
	msm_data.config.rec_attempts = 0;
	msm_data.config.rec_delay = 1000 * RECOVERY_WORK_TIMEOUT;
	msm_data.config.rec_reset_timeout = RECOVERY_RESET_WORK_TIMEOUT;
}

int bcb_tc_def_msm_init(struct k_work *notify_work)
{
	if (!notify_work) {
		return -EINVAL;
	}

	msm_data.notify_work = notify_work;

	if (restore_config()) {
		/* Restoring could fail while trying to load old parameters. 
		   So store default values into the config to avoid future errors.
		   NOTE: This effectively overwrites the old parameters.
		 */
		load_default_config();
		store_config();
	}

	if (bcb_sw_is_on()) {
		msm_data.state = BCB_TC_DEF_MSM_STATE_CLOSED;
	} else {
		msm_data.state = BCB_TC_DEF_MSM_STATE_OPENED;
	}

	msm_data.cause = BCB_TC_DEF_MSM_CAUSE_NONE;
	msm_data.zd_count = 0;

	bcb_tc_def_csom_mod_init();
	return 0;
}

int bcb_tc_def_msm_event(bcb_tc_def_event_t event, void *arg)
{
	int r = 0;

	if (MSM_EV_FILTERED(msm_data.ev_filter, event)) {
		return 0;
	}

	switch (msm_data.state) {
	case BCB_TC_DEF_MSM_STATE_UNDEFINED: {
		return -ENOTSUP;
	} break;
	case BCB_TC_DEF_MSM_STATE_OPENED: {
		switch (event) {
		case BCB_TC_DEF_EV_CMD_CLOSE: {
			msm_on_cmd_close_at_opened();
		} break;
		default: {
			/* Other events are ignored */
		} break;
		}
	} break;
	case BCB_TC_DEF_MSM_STATE_SUPPLY_WAIT: {
		switch (event) {
		case BCB_TC_DEF_EV_CMD_OPEN: {
			msm_on_cmd_open_at_supply_wait();
		} break;
		case BCB_TC_DEF_EV_ZD_V: {
			msm_on_zd_v_at_supply_wait();
		} break;
		case BCB_TC_DEF_EV_SUPPLY_TIMER: {
			msm_on_supply_timer_at_supply_wait();
		} break;
		default: {
			/* Other events are ignored */
		} break;
		}
	} break;
	case BCB_TC_DEF_MSM_STATE_CLOSE_WAIT: {
		switch (event) {
		case BCB_TC_DEF_EV_CMD_OPEN: {
			msm_on_cmd_open_before_open_wait();
		} break;
		case BCB_TC_DEF_EV_REC_TIMER:
		case BCB_TC_DEF_EV_ZD_V: {
			msm_on_zd_v_rec_timer_at_close_wait();
		} break;
		case BCB_TC_DEF_EV_SW_CLOSED: {
			msm_on_sw_closed_at_close_wait();
		} break;
		default: {
			/* Other events are ignored */
		} break;
		}
	} break;
	case BCB_TC_DEF_MSM_STATE_CLOSED: {
		switch (event) {
		case BCB_TC_DEF_EV_CMD_OPEN: {
			msm_on_cmd_open_before_open_wait();
		} break;
		case BCB_TC_DEF_EV_OCD: {
			msm_on_ocd_at_closed();
		} break;
		case BCB_TC_DEF_EV_SW_OPENED: {
			msm_on_sw_opened_at_closed();
		} break;

		case BCB_TC_DEF_EV_REC_RESET_TIMER: {
			msm_data.recovery_remaining = msm_data.config.rec_attempts;
			LOG_INF("Reset recovery attempts: %d", msm_data.recovery_remaining);
		} break;
		default: {
			/* Other events are ignored */
		} break;
		}
		r = msm_csom_event(event, arg);
	} break;
	case BCB_TC_DEF_MSM_STATE_OPEN_WAIT: {
		switch (event) {
		case BCB_TC_DEF_EV_ZD_V: {
			msm_on_zd_v_at_open_wait();
		} break;
		case BCB_TC_DEF_EV_SW_OPENED: {
			msm_on_sw_opened_at_open_wait();
		} break;
		default: {
			/* Other events are ignored */
		} break;
		}
	} break;
	default: {
		/* Other states are ignored */
	} break;
	}

	return r;
}

int bcb_tc_def_msm_config_set(bcb_tc_def_msm_config_t *config)
{
	if (!config) {
		return -EINVAL;
	}

	if (config->csom >= BCB_TC_DEF_MSM_CSOM_END) {
		LOG_ERR("invalid csom: %d", config->csom);
		return -EINVAL;
	}

	if (config->rec_reset_timeout < RECOVERY_RESET_WORK_TIMEOUT_MIN ||
	    config->rec_reset_timeout > RECOVERY_RESET_WORK_TIMEOUT_MAX) {
		return -EINVAL;
	}

	LOG_INF("Updating Reset Timeout: %d ms", config->rec_reset_timeout);

	memcpy(&msm_data.config, config, sizeof(bcb_tc_def_msm_config_t));

	return store_config();
}

int bcb_tc_def_msm_config_get(bcb_tc_def_msm_config_t *config)
{
	if (!config) {
		return -EINVAL;
	}

	memcpy(config, &msm_data.config, sizeof(bcb_tc_def_msm_config_t));
	return 0;
}

bcb_tc_def_msm_state_t bcb_tc_def_msm_get_state(void)
{
	return msm_data.state;
}

bcb_tc_def_msm_cause_t bcb_tc_def_msm_get_cause(void)
{
	return msm_data.cause;
}

static void on_supply_detect_work(struct k_work *work)
{
	bcb_tc_def_msm_event(BCB_TC_DEF_EV_SUPPLY_TIMER, NULL);
}

static void on_recovery_work(struct k_work *work)
{
	bcb_tc_def_msm_event(BCB_TC_DEF_EV_REC_TIMER, NULL);
}

static void on_recovery_reset_work(struct k_work *work)
{
	bcb_tc_def_msm_event(BCB_TC_DEF_EV_REC_RESET_TIMER, NULL);
}

static int tc_def_msm_system_init()
{
	memset(&msm_data, 0, sizeof(msm_data));
	k_delayed_work_init(&msm_data.supply_detect_work, on_supply_detect_work);
	k_delayed_work_init(&msm_data.recovery_work, on_recovery_work);
	k_delayed_work_init(&msm_data.recovery_reset_work, on_recovery_reset_work);
	return 0;
}

SYS_INIT(tc_def_msm_system_init, APPLICATION, CONFIG_BCB_LIB_TRIP_CURVE_DEFAULT_INIT_PRIORITY);