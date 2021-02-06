#include "bcb.h"
#include "bcb_sw.h"
#include "bcb_msmnt.h"
#include "bcb_trip_curve.h"
#include <errno.h>

#define LOG_LEVEL CONFIG_BCB_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb);

struct bcb_data {
	const struct bcb_trip_curve *trip_curve;
};

static struct bcb_data bcb_data;

int bcb_init()
{
	memset(&bcb_data, 0, sizeof(bcb_data));
	return 0;
}

int bcb_on(void)
{
	if (!bcb_data.trip_curve) {
		LOG_ERR("Trip cruve not set");
		return -EINVAL;
	}
	return bcb_data.trip_curve->on();
}

int bcb_off(void)
{
	if (!bcb_data.trip_curve) {
		LOG_ERR("Trip cruve not set");
		return -EINVAL;
	}
	return bcb_data.trip_curve->off();
}

bool bcb_is_on()
{
	return bcb_sw_is_on();
}

int32_t bcb_get_temp(bcb_temp_sensor_t sensor)
{
	return bcb_msmnt_get_temp(sensor);
}

int32_t bcb_get_voltage(void)
{
	return bcb_msmnt_get_voltage();
}

int32_t bcb_get_current(void)
{
	return bcb_msmnt_get_current();
}

uint32_t bcb_get_voltage_rms(void)
{
	return bcb_msmnt_get_voltage_rms();
}

uint32_t bcb_get_current_rms(void)
{
	return bcb_msmnt_get_voltage_rms();
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

	return bcb_data.trip_curve->init();
}