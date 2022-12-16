#include "networking.h"
#include "services.h"
#include <kernel.h>
#include <lib/bcb.h>
#include <lib/bcb_tc_def.h>

#define LOG_LEVEL CONFIG_ZERO_APP_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(zero_app);

/* Uncomment following to enable custom trip settings. */
//#define CUSTOM_TRIP_SETTINGS

#ifdef CUSTOM_TRIP_SETTINGS
#define CURVE_DURATION_SECONDS(d)                                                                  \
	((uint16_t)(((d)*1000) / CONFIG_BCB_TRIP_CURVE_DEFAULT_MONITOR_INTERVAL))
#endif

void main()
{
	LOG_INF("Zero APP (built " __DATE__ " " __TIME__ ")");

	networking_init();
	services_init();

#ifdef CUSTOM_TRIP_SETTINGS
	const bcb_tc_t *curve;
	bcb_tc_pt_t curve_points[2];

	/* Current limit in amperes */
	curve_points[0].i = 10;
	/* Use CURVE_DURATION_SECONDS to specify allowed elapsed duration in seconds. */
	curve_points[0].d = CURVE_DURATION_SECONDS(10);

	curve_points[1].i = 15; /* Ditto */
	curve_points[1].d = CURVE_DURATION_SECONDS(5); /* Ditto */

	/* Set the default trip curve. */
	curve = bcb_tc_get_default();
	bcb_set_tc(curve);

	/* Hardware limit can be changed between 30 and 60 */
	curve->set_limit_hw(30);
	/* Setting points to the trip curve */
	curve->set_points(curve_points, 2);
#else
	bcb_set_tc(bcb_tc_get_default());
#endif

	while (1) {
		k_sleep(K_MSEC(1000));
	}
}