#include "services.h"
#include <zephyr/kernel.h>

#ifdef CONFIG_MCUMGR_SMP_UDP
#include <zephyr/mgmt/mcumgr/smp_udp.h>
#endif

#ifdef CONFIG_MCUMGR_CMD_OS_MGMT
#include "os_mgmt/os_mgmt.h"
#endif
#ifdef CONFIG_MCUMGR_CMD_IMG_MGMT
#include "img_mgmt/img_mgmt.h"
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(services);

int services_init(void)
{
#ifdef CONFIG_MCUMGR_CMD_OS_MGMT
	os_mgmt_register_group();
#endif

#ifdef CONFIG_MCUMGR_CMD_IMG_MGMT
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

#ifdef CONFIG_MCUMGR_SMP_UDP
	{
		int r = smp_udp_open();
		if (r < 0) {
			LOG_ERR("Cannot not open smp udp %d", r);
			return r;
		}
	}
#endif

	return 0;
}