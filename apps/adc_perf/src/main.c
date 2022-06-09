#include "networking.h"
#include "services.h"
#include "adc_perf.h"
#include <kernel.h>
#include <lib/bcb.h>
#include <lib/bcb_trip_curve_default.h>

#define LOG_LEVEL CONFIG_ZERO_APP_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(adc_perf_app);

void main()
{
	LOG_INF("ADC Perf APP (built " __DATE__ " " __TIME__ ")");
	LOG_INF("system reset - SRS0: 0x%02" PRIx8 ", SRS1: 0x%02" PRIx8, RCM->SRS0, RCM->SRS1);

	networking_init();
	services_init();
	bcb_set_trip_curve(bcb_trip_curve_get_default());
	adc_pef_init();

	while (1) {
		k_sleep(K_MSEC(1000));
	}
}