#include "bcb.h"
#include "bcb_config.h"
#include "bcb_msmnt.h"
#include "bcb_trip_curve.h"
#include <errno.h>

#define LOG_LEVEL CONFIG_BCB_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb);

enum bcb_state { BCB_STATE_NONE = 0x00, BCB_STATE_ON = 0x01, BCB_STATE_OFF = 0x02 };

typedef struct __attribute__((packed)) bcb_config_data {
	uint8_t state;
} bcb_config_data_t;

struct bcb_data {
	const struct bcb_trip_curve *trip_curve;
	sys_slist_t callback_list;
	bcb_config_data_t config;
};

static struct bcb_data bcb_data;

static inline void load_default_config(void)
{
	bcb_data.config.state = BCB_STATE_ON;
}

static void trip_curve_callback(const struct bcb_trip_curve *curve, bcb_trip_cause_t type, uint8_t limit)
{
	sys_snode_t *node;
	SYS_SLIST_FOR_EACH_NODE (&bcb_data.callback_list, node) {
		struct bcb_trip_callback *callback = (struct bcb_trip_callback *)node;
		if (callback && callback->handler) {
			callback->handler(curve, type, limit);
		}
	}
}

int bcb_init()
{
	int r;

	memset(&bcb_data, 0, sizeof(bcb_data));
	sys_slist_init(&bcb_data.callback_list);

	r = bcb_config_load(CONFIG_BCB_LIB_PERSISTENT_CONFIG_OFFSET_BCB,
			    (uint8_t *)&bcb_data.config, sizeof(bcb_data.config));
	if (r) {
		load_default_config();
	}
	return 0;
}

int bcb_close(void)
{
	int r;

	if (!bcb_data.trip_curve) {
		LOG_ERR("Trip cruve not set");
		return -EINVAL;
	}

	bcb_data.config.state = BCB_STATE_ON;
	r = bcb_config_store(CONFIG_BCB_LIB_PERSISTENT_CONFIG_OFFSET_BCB,
			     (uint8_t *)&bcb_data.config, sizeof(bcb_data.config));

	if (r) {
		LOG_WRN("Cannot save configuration: %d", r);
	}

	return bcb_data.trip_curve->close();
}

int bcb_open(void)
{
	int r;

	if (!bcb_data.trip_curve) {
		LOG_ERR("Trip cruve not set");
		return -EINVAL;
	}

	bcb_data.config.state = BCB_STATE_OFF;
	r = bcb_config_store(CONFIG_BCB_LIB_PERSISTENT_CONFIG_OFFSET_BCB,
			     (uint8_t *)&bcb_data.config, sizeof(bcb_data.config));

	if (r) {
		LOG_WRN("Cannot save configuration: %d", r);
	}

	return bcb_data.trip_curve->open();
}

bool bcb_is_closed()
{
	return bcb_data.config.state == BCB_STATE_ON;
}

int bcb_set_trip_curve(const struct bcb_trip_curve *curve)
{
	int r;
	if (!curve) {
		LOG_ERR("Invalid trip cruve");
		return -EINVAL;
	}

	if (bcb_data.trip_curve) {
		if ((r = bcb_data.trip_curve->shutdown())) {
			LOG_WRN("Trip curve shutdown failed: %d", r);
		}
	}

	bcb_data.trip_curve = curve;
	r = bcb_data.trip_curve->init();
	if (r) {
		LOG_WRN("Cannot initialise curve: %d", r);
	}

	bcb_data.trip_curve->set_callback(trip_curve_callback);

	if (bcb_is_closed()) {
		return bcb_data.trip_curve->close();
	} else {
		return bcb_data.trip_curve->open();
	}

	return 0;
}

int bcb_add_trip_callback(struct bcb_trip_callback *callback)
{
	if (!callback || !callback->handler) {
		return -ENOTSUP;
	}


	sys_slist_append(&bcb_data.callback_list, &callback->node);

	return 0;
}