#include <lib/bcb_tc_def_csom_mod.h>
#include <lib/bcb_tc_def_msm.h>
#include <lib/bcb_sw.h>
#include <lib/bcb_config.h>
#include <logging/log.h>
#include <string.h>

// clang-format off
#define CONFIG_OFFSET		CONFIG_BCB_LIB_PERSISTENT_CONFIG_OFFSET_TC_DEF_CSOM_MOD
#define LOG_LEVEL 		CONFIG_BCB_TRIP_CURVE_DEFAULT_LOG_LEVEL
// clang-format on

LOG_MODULE_REGISTER(bcb_tc_def_csom_mod);

typedef enum {
	TC_DEF_CSOM_MOD_STATE_DISABLED = 0,
	TC_DEF_CSOM_MOD_STATE_CLOSED,
	TC_DEF_CSOM_MOD_STATE_OPENED
} tc_def_csom_mod_mod_state_t;

struct tc_def_csom_data {
	bcb_tc_def_csom_mod_config_t config;
	tc_def_csom_mod_mod_state_t state;
	uint8_t zdc;
};

struct tc_def_csom_data csom_data;

static int restore_config(void)
{
	int r;

	r = bcb_config_load(CONFIG_OFFSET, (uint8_t *)&csom_data.config,
			    sizeof(bcb_tc_def_csom_mod_config_t));
	if (r) {
		LOG_ERR("cannot restore params: %d", r);
	}

	return r;
}

static int store_config(void)
{
	int r;

	r = bcb_config_store(CONFIG_OFFSET, (uint8_t *)&csom_data.config,
			     sizeof(bcb_tc_def_csom_mod_config_t));
	if (r) {
		LOG_ERR("cannot store params: %d", r);
	}

	return r;
}

static void load_default_config(void)
{
	LOG_INF("loading default config");

	csom_data.config.zdc_closed = 0;
	csom_data.config.zdc_period = 0;
}

int bcb_tc_def_csom_mod_init(void)
{
	if (restore_config()) {
		/* Restoring could fail while trying to load old parameters. 
		   So store default values into the config to avoid future errors.
		   NOTE: This effectively overwrites the old parameters.
		 */
		load_default_config();
		store_config();
	}

	csom_data.state = TC_DEF_CSOM_MOD_STATE_DISABLED;
	csom_data.zdc = 0;

	return 0;
}

int bcb_tc_def_csom_mod_config_set(bcb_tc_def_csom_mod_config_t *config)
{
	if (!config) {
		return -EINVAL;
	}

	if (!config->zdc_closed || (config->zdc_closed >= config->zdc_period)) {
		LOG_ERR("invalid params");
		return -EINVAL;
	}

	memcpy(&csom_data.config, config, sizeof(bcb_tc_def_csom_mod_config_t));

	return store_config();
}

int bcb_tc_def_csom_mod_config_get(bcb_tc_def_csom_mod_config_t *config)
{
	if (!config) {
		return -EINVAL;
	}

	memcpy(config, &csom_data.config, sizeof(bcb_tc_def_csom_mod_config_t));

	return 0;
}

static inline void csom_mod_on_zd_v_at_closed(void)
{
	csom_data.zdc++;
	if (csom_data.zdc >= csom_data.config.zdc_closed) {
		bcb_sw_off();
	}
	/* Stay in the same state */
}

static inline void csom_mod_on_sw_opened_at_closed(void)
{
	csom_data.state = TC_DEF_CSOM_MOD_STATE_OPENED;
}

static inline void csom_mod_on_zd_v_at_opened(void)
{
	csom_data.zdc++;
	if (csom_data.zdc >= csom_data.config.zdc_period) {
		csom_data.zdc = 0;
		bcb_sw_on();
	}
	/* Stay in the same state */
}

static inline void csom_mod_on_sw_closed_at_opened(void)
{
	csom_data.state = TC_DEF_CSOM_MOD_STATE_CLOSED;
}

static inline void csom_mod_at_disabled(void)
{
	csom_data.zdc = 0;

	if (csom_data.config.zdc_closed &&
	    (csom_data.config.zdc_closed < csom_data.config.zdc_period)) {
		csom_data.state = TC_DEF_CSOM_MOD_STATE_CLOSED;
	} else {
		csom_data.state = TC_DEF_CSOM_MOD_STATE_DISABLED;
	}
}

int bcb_tc_def_csom_mod_event(bcb_tc_def_event_t event, void *arg)
{
	switch (csom_data.state) {
	case TC_DEF_CSOM_MOD_STATE_DISABLED: {
		csom_mod_at_disabled();
	} break;
	case TC_DEF_CSOM_MOD_STATE_CLOSED: {
		switch (event) {
		case BCB_TC_DEF_EV_ZD_V: {
			csom_mod_on_zd_v_at_closed();
		} break;
		case BCB_TC_DEF_EV_SW_OPENED: {
			csom_mod_on_sw_opened_at_closed();
		} break;
		default: {
			/* Other events are ignored */
		} break;
		}
	} break;
	case TC_DEF_CSOM_MOD_STATE_OPENED: {
		switch (event) {
		case BCB_TC_DEF_EV_ZD_V: {
			csom_mod_on_zd_v_at_opened();
		} break;
		case BCB_TC_DEF_EV_SW_CLOSED: {
			csom_mod_on_sw_closed_at_opened();
		} break;
		default: {
			/* Other events are ignored */
		} break;
		}
	} break;
	default: {
		/* Other events are ignored */
	} break;
	}

	return 0;
}

void bcb_tc_def_csom_mod_cleanup(void)
{
	csom_data.zdc = 0;
	csom_data.state = TC_DEF_CSOM_MOD_STATE_DISABLED;
}