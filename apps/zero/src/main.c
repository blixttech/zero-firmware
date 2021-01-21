#include "networking.h"
#include "services.h"
#include <kernel.h>
#include <bcb_coap.h>

#define LOG_LEVEL CONFIG_ZERO_APP_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(zero_app);

void main()
{
    LOG_INF("Zero APP (built " __DATE__ " " __TIME__ ")");

    networking_init();
    services_init();

    while(1) {
        k_sleep(K_MSEC(1000));
    }
}