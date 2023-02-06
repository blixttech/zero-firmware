#include "networking.h"
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_conn_mgr.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/dhcpv4.h>

#ifndef CONFIG_NET_CONFIG_AUTO_INIT
static struct net_mgmt_event_callback callback;
#endif // CONFIG_NET_CONFIG_AUTO_INIT

static void event_handler(struct net_mgmt_event_callback *cb, uint32_t event, struct net_if *iface)
{
	if (event == NET_EVENT_IF_UP) {
		net_dhcpv4_start(iface);
	} else if (event == NET_EVENT_IF_DOWN) {
		net_dhcpv4_stop(iface);
	} else {
		/* Some other event */
	}
}

int networking_init(void)
{
#ifndef CONFIG_NET_CONFIG_AUTO_INIT
	net_mgmt_init_event_callback(&callback, event_handler, NET_EVENT_IF_UP | NET_EVENT_IF_DOWN);
	net_mgmt_add_event_callback(&callback);
#endif // CONFIG_NET_CONFIG_AUTO_INIT
	return 0;
}
