/*
 * Copyright (c) 2020 Blixt Tech AB
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>

#include "os_mgmt/os_mgmt.h"
#include "img_mgmt/img_mgmt.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(zero);

#include "common.h"


void main(void)
{

	/* Register the built-in mcumgr command handlers. */
	os_mgmt_register_group();
	img_mgmt_register_group();
	start_smp_udp();

	/* using __TIME__ ensure that a new binary will be built on every
	 * compile which is convient when testing firmware upgrade.
	 */
	LOG_INF("build time: " __DATE__ " " __TIME__);

	/* The system work queue handles all incoming mcumgr requests.  Let the
	 * main thread idle while the mcumgr server runs.
	 */
	while (1) {
		k_sleep(K_MSEC(1000));
	}
}
