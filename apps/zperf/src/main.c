#include <zephyr.h>
#include <net/net_if.h>
#include <net/dhcpv4.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(main);

void main(void)
{	
    LOG_INF("Run dhcpv4 client");

    struct net_if * iface = net_if_get_default();
    net_dhcpv4_start(iface);
}