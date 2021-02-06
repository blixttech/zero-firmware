#include "bcb_trip_curve.h"
#include "bcb.h"
#include "bcb_leds.h"
#include "bcb_sw.h"
#include "bcb_zd.h"
#include <init.h>

#define LOG_LEVEL CONFIG_BCB_TRIP_CURVE_DEFAULT_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_tc_default);

typedef enum {
	CURVE_STATE_NONE = 0,
	CURVE_STATE_ON_WAITING,
	CURVE_STATE_ON,
	CURVE_STATE_OFF_WAITING,
	CURVE_STATE_OFF,
} curve_state_t;

struct curve_data {
	sys_slist_t callback_list;
	struct bcb_zd_callback zd_callback;
	volatile bool is_on_requested;
	volatile bool is_off_requested;
	volatile uint8_t state;
};

static struct curve_data curve_data;

static void on_voltage_zero_detect(void)
{
	if (curve_data.is_off_requested) {
		curve_data.is_off_requested = false;
		bcb_sw_off();
	} else if (curve_data.is_on_requested) {
		curve_data.is_on_requested = false;
		bcb_sw_on();
	}
}

static int trip_curve_init(void)
{
	LOG_INF("init");
	bcb_zd_voltage_add_callback(&curve_data.zd_callback);
	return 0;
}

static int trip_curve_shutdown(void)
{
	LOG_INF("shutdown");
	bcb_zd_voltage_remove_callback(&curve_data.zd_callback);
	return 0;
}

static int trip_curve_on(void)
{
	curve_data.is_on_requested = true;
	return 0;
}

static int trip_curve_off(void)
{
	curve_data.is_off_requested = true;
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

static void trip_curve_set_callback(bcb_trip_curve_callback_t callback)
{
}

const struct bcb_trip_curve trip_curve_default = {
	.init = trip_curve_init,
	.shutdown = trip_curve_shutdown,
	.on = trip_curve_on,
	.off = trip_curve_off,
	.set_limit = trip_curve_set_current_limit,
	.get_limit = trip_curve_get_current_limit,
	.set_callback = trip_curve_set_callback,
};

static int trip_curve_system_init()
{
	curve_data.zd_callback.handler = on_voltage_zero_detect;
	bcb_set_trip_curve(&trip_curve_default);
	return 0;
}

SYS_INIT(trip_curve_system_init, APPLICATION, CONFIG_BCB_LIB_TRIP_CURVE_DEFAULT_INIT_PRIORITY);