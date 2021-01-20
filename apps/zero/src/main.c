#include "networking.h"
#include "services.h"
#include <kernel.h>
#include <bcb_coap.h>

#define LOG_LEVEL CONFIG_ZERO_APP_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(zero_app);

void main()
{
    LOG_INF("Zero APP:" __DATE__ " " __TIME__);

    networking_init();
    services_init();
    bcb_coap_start_server();
    /* TODO: Implement thread-based reading from the CoAP port */
    do {
        bcb_coap_process();
    } while(1);
}