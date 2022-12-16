#include <lib/bcb.h>
#include <lib/bcb_config.h>
#include <lib/bcb_msmnt.h>
#include <lib/bcb_tc.h>
#include <errno.h>
#include <kernel.h>

#define LOG_LEVEL CONFIG_BCB_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb);

typedef struct __attribute__((packed)) bcb_state {
	bool is_closed;
} bcb_state_t;

struct bcb_data {
	const struct bcb_tc *trip_curve;
	sys_slist_t callback_list;
	bcb_state_t state;
	struct k_work bcb_work;
};

typedef enum {
	BCB_ACTION_OPEN = 0,
	BCB_ACTION_CLOSE,
	BCB_ACTION_TOGGLE,
} bcb_action_t;

struct bcb_action_item {
	bcb_action_t action;
};

K_MSGQ_DEFINE(async_action_msgq, sizeof(struct bcb_action_item), 4, 4);

static struct bcb_data bcb_data;

static inline void load_default_state(void)
{
	bcb_data.state.is_closed = false;
}

static void trip_curve_callback(const bcb_tc_t *curve, bcb_tc_cause_t type)
{
	sys_snode_t *node;
	SYS_SLIST_FOR_EACH_NODE (&bcb_data.callback_list, node) {
		bcb_tc_callback_t *callback = (bcb_tc_callback_t *)node;
		if (callback && callback->handler) {
			callback->handler(curve, type);
		}
	}
}

static void bcb_work(struct k_work *work)
{
	int r;
	struct bcb_action_item item;

	while (!k_msgq_get(&async_action_msgq, &item, K_NO_WAIT)) {
		bcb_tc_state_t curve_state;
		curve_state = bcb_data.trip_curve->get_state();

		if (item.action == BCB_ACTION_OPEN) {
			LOG_DBG("open");
			bcb_data.state.is_closed = false;
			r = bcb_data.trip_curve->open();
		} else if (item.action == BCB_ACTION_CLOSE) {
			LOG_DBG("close");
			bcb_data.state.is_closed = true;
			r = bcb_data.trip_curve->close();
		} else {
			if (curve_state != BCB_TC_STATE_OPENED &&
			curve_state != BCB_TC_STATE_CLOSED) {
				LOG_WRN("not in opened/closed state");
				continue;
			}

			LOG_DBG("toggle");
			if (curve_state == BCB_TC_STATE_OPENED) {
				bcb_data.state.is_closed = true;
				r = bcb_data.trip_curve->close();
			} else {
				bcb_data.state.is_closed = false;
				r = bcb_data.trip_curve->open();
			}
		}
	}

	r = bcb_config_store(CONFIG_BCB_LIB_PERSISTENT_CONFIG_OFFSET_BCB,
			     (uint8_t *)&bcb_data.state, sizeof(bcb_data.state));

	if (r) {
		LOG_WRN("cannot save configuration: %d", r);
	}
}

int bcb_init()
{
	int r;

	memset(&bcb_data, 0, sizeof(bcb_data));
	sys_slist_init(&bcb_data.callback_list);
	k_work_init(&bcb_data.bcb_work, bcb_work);

	r = bcb_config_load(CONFIG_BCB_LIB_PERSISTENT_CONFIG_OFFSET_BCB,
			    (uint8_t *)&bcb_data.state, sizeof(bcb_data.state));
	if (r) {
		load_default_state();
	}

	return 0;
}

int bcb_close(void)
{
	struct bcb_action_item item;
	int r;

	if (!bcb_data.trip_curve) {
		LOG_ERR("trip cruve not set");
		return -EINVAL;
	}

	item.action = BCB_ACTION_CLOSE;
	r = k_msgq_put(&async_action_msgq, &item, K_NO_WAIT);

	if (r < 0) {
		return r;
	}

	k_work_submit(&bcb_data.bcb_work);

	return 0;
}

int bcb_open(void)
{
	struct bcb_action_item item;
	int r;

	if (!bcb_data.trip_curve) {
		LOG_ERR("trip cruve not set");
		return -EINVAL;
	}

	item.action = BCB_ACTION_OPEN;
	r = k_msgq_put(&async_action_msgq, &item, K_NO_WAIT);

	if (r < 0) {
		return r;
	}

	k_work_submit(&bcb_data.bcb_work);

	return 0;
}

int bcb_toggle(void)
{
	struct bcb_action_item item;
	int r;

	if (!bcb_data.trip_curve) {
		LOG_ERR("trip cruve not set");
		return -EINVAL;
	}

	item.action = BCB_ACTION_TOGGLE;
	r = k_msgq_put(&async_action_msgq, &item, K_NO_WAIT);

	if (r < 0) {
		return r;
	}

	k_work_submit(&bcb_data.bcb_work);

	return 0;
}

bcb_tc_state_t bcb_get_state(void)
{
	if (!bcb_data.trip_curve) {
		LOG_ERR("trip cruve not set");
		return BCB_TC_STATE_UNDEFINED;
	}

	return bcb_data.trip_curve->get_state();
}

bcb_tc_cause_t bcb_get_cause(void)
{
	if (!bcb_data.trip_curve) {
		LOG_ERR("trip cruve not set");
		return BCB_TC_CAUSE_NONE;
	}

	return bcb_data.trip_curve->get_cause();
}

int bcb_set_tc(const bcb_tc_t *curve)
{
	int r;
	if (!curve) {
		LOG_ERR("invalid trip cruve");
		return -EINVAL;
	}

	if (bcb_data.trip_curve) {
		if ((r = bcb_data.trip_curve->shutdown())) {
			LOG_WRN("trip curve shutdown failed: %d", r);
		}
	}

	bcb_data.trip_curve = curve;
	r = bcb_data.trip_curve->init();
	if (r) {
		LOG_WRN("cannot initialise curve: %d", r);
	}

	bcb_data.trip_curve->set_callback(trip_curve_callback);

	if (bcb_data.state.is_closed) {
		return bcb_close();
	} else {
		return bcb_open();
	}

	return 0;
}

const struct bcb_tc * bcb_get_tc(void)
{
	return bcb_data.trip_curve;
}

int bcb_add_tc_callback(bcb_tc_callback_t *callback)
{
	if (!callback || !callback->handler) {
		return -ENOTSUP;
	}

	sys_slist_append(&bcb_data.callback_list, &callback->node);

	return 0;
}