#include <zephyr/kernel.h>

#ifdef CONFIG_MCUMGR_CMD_IMG_MGMT
#include "img_mgmt/img_mgmt.h"
#endif

#include "networking.h"
#include "services.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

void main(void)
{
	LOG_INF("app: zero, built: " __DATE__ " " __TIME__);
	LOG_INF("board: " CONFIG_BOARD);

#ifdef CONFIG_MCUMGR_CMD_IMG_MGMT
	{
		int r;
		struct image_version version;
		r = img_mgmt_my_version(&version);
		if (!r) {
			LOG_INF("version: %" PRIu8 ".%" PRIu8 ".%" PRIu16 " (%" PRIu32 ")",
				version.iv_major, version.iv_minor, version.iv_revision,
				version.iv_build_num);
		} else {
			LOG_WRN("cannot get version");
		}
	}
#endif

	networking_init();
	services_init();

	while (1) {
		k_sleep(K_MSEC(1000));
	}
}
