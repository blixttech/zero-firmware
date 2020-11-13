#include <zephyr.h>
#include <bcb_leds.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

void main(void)
{
	while (1) {
		bcb_leds_toggle(BCB_LEDS_RED);
		k_msleep(SLEEP_TIME_MS);
	}
}
