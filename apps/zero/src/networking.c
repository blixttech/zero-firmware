#include "networking.h"
#include <stdint.h>
#include <kernel.h>
#include <net/net_mgmt.h>
#include <net/net_event.h>
#include <net/net_conn_mgr.h>
#include <net/net_if.h>
#include <net/dhcpv4.h>
#include <sys/_stdint.h>

#define LOG_LEVEL CONFIG_ZERO_NETWORKING_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(zero_networking);

#ifndef CONFIG_NET_CONFIG_AUTO_INIT

struct networking_data {
	struct net_mgmt_event_callback callback;
	struct k_work dhcp_work;
	struct net_if *current_iface;
	uint32_t current_event;
};

static struct networking_data networking_data;

static void networking_dhcp_work(struct k_work *work)
{
	if (networking_data.current_event == NET_EVENT_IF_UP) {
		net_dhcpv4_start(networking_data.current_iface);
	} else if (networking_data.current_event == NET_EVENT_IF_DOWN) {
		net_dhcpv4_stop(networking_data.current_iface);
	}
}

static void networking_event_handler(struct net_mgmt_event_callback *cb,
				     uint32_t event, struct net_if *iface)
{
	if (event == NET_EVENT_IF_UP) {
		networking_data.current_event = NET_EVENT_IF_UP;
		networking_data.current_iface = iface;
		k_work_submit(&networking_data.dhcp_work);
	} else if (event == NET_EVENT_IF_DOWN) {
		networking_data.current_event = NET_EVENT_IF_DOWN;
		networking_data.current_iface = iface;
		k_work_submit(&networking_data.dhcp_work);
	} else {
	}
}

#endif // CONFIG_NET_CONFIG_AUTO_INIT

int networking_init()
{
#ifndef CONFIG_NET_CONFIG_AUTO_INIT
	k_work_init(&networking_data.dhcp_work, networking_dhcp_work);
	net_mgmt_init_event_callback(&networking_data.callback,
				     networking_event_handler,
				     NET_EVENT_IF_UP | NET_EVENT_IF_DOWN);
	net_mgmt_add_event_callback(&networking_data.callback);
#endif // CONFIG_NET_CONFIG_AUTO_INIT
	return 0;
}