#include <zephyr/kernel.h>
#include "networking.h"
#include "services.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

void main(void)
{
	LOG_INF("Zero APP [board: " CONFIG_BOARD ", built: " __DATE__ " " __TIME__ "]");

	networking_init();
	services_init();

	while (1) {
		k_sleep(K_MSEC(1000));
	}
}
