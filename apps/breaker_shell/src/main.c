/*
 * Copyright (c) 2015 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <version.h>
#include <logging/log.h>
#include <stdlib.h>

#include <bcb.h>

LOG_MODULE_REGISTER(app);


static void on_ocp(int32_t duration)
{
	printk("\nOn OCP: duration %d\n", duration);
}

static int cmd_off_params(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "Turning off");

	bcb_off();

	return 0;
}

static int cmd_on_params(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "Turning on");

	bcb_on();

	return 0;
}

static int cmd_ocp_trigger_params(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	int direction = BCB_OCP_TEST_TGR_DIR_P;
	if (argc > 0) {
		switch ((int)argv[0]) {
			case 'p':
			case 'P':
				direction = BCB_OCP_TEST_TGR_DIR_P;
				break;
			case 'n':
			case 'N':
				direction = BCB_OCP_TEST_TGR_DIR_N;
				break;
			default:
				direction = BCB_OCP_TEST_TGR_DIR_P;
		}
	} else {
		direction = BCB_OCP_TEST_TGR_DIR_P;
	}
	shell_print(shell, 
				"Triggering OCP direction : %c", 
				(direction == BCB_OCP_TEST_TGR_DIR_P ? 'P' : 'N'));

	bcb_ocp_test_trigger(direction);

	return 0;
}

static int cmd_version(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "Zephyr version %s", KERNEL_VERSION_STRING);

	bcb_off();

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(breaker_sub,
	SHELL_CMD(on, NULL, "Turn on.", cmd_on_params),
	SHELL_CMD(off, NULL, "Turn off.", cmd_off_params),
	SHELL_CMD(ocp_trigger, NULL, "Trigger OCP.", cmd_ocp_trigger_params),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);
SHELL_CMD_REGISTER(breaker, &breaker_sub, "Breaker commands", NULL);

SHELL_CMD_ARG_REGISTER(version, NULL, "Show kernel version", cmd_version, 1, 0);

void main(void)
{
	printk("\nInitializing breaker..\n");
	bcb_set_ocp_callback(on_ocp);
}
