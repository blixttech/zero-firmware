#include "services.h"
#include "bcb_user_if.h"
#include "bcb.h"
#include <stdint.h>
#include <mgmt/smp_udp.h>
#include <os_mgmt/os_mgmt.h>
#include <img_mgmt/img_mgmt.h>
#include <img_mgmt/image.h>
#include <bcb_coap.h>

#define LOG_LEVEL CONFIG_ZERO_SERVICES_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(zero_services);

struct services_data {
	struct bcb_user_button_callback button_callback;
};

static struct services_data services_data;

static void button_callback_handler(bool is_pressed)
{
	if (is_pressed) {
		bcb_toggle();
	}
}

int services_init()
{
	memset(&services_data, 0, sizeof(services_data));

#if CONFIG_MCUMGR_CMD_OS_MGMT
	os_mgmt_register_group();
#endif

#if CONFIG_MCUMGR_CMD_IMG_MGMT
	{
		int r;
		struct image_version version;
		img_mgmt_register_group();
		r = img_mgmt_my_version(&version);
		if (!r) {
			LOG_INF("Firmware version %" PRIu8 ".%" PRIu8 ".%" PRIu16 " (%" PRIu32 ")",
				version.iv_major, version.iv_minor, version.iv_revision,
				version.iv_build_num);
		}
	}
#endif

#if CONFIG_MCUMGR_SMP_UDP
	{
		int r = smp_udp_open();
		if (r < 0) {
			LOG_ERR("Cannot not open smp udp %d", r);
			return r;
		}
	}
#endif

#if CONFIG_BCB_COAP
	{
		int r = bcb_coap_start_server();
		if (r < 0) {
			LOG_ERR("Cannot start CoAP server %d", r);
			return r;
		}
	}
#endif

	services_data.button_callback.handler = button_callback_handler;
	bcb_user_button_add_callback(&services_data.button_callback);

	return 0;
}
