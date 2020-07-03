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

#include <lib/blixt-breaker.h>

LOG_MODULE_REGISTER(app);

static int cmd_off_params(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "Turning off");

	breaker_off();

	return 0;
}

static int cmd_on_params(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "Turning on");

	breaker_on();

	return 0;
}

static int cmd_ocp_trigger_params(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	int direction = BREAKER_OCP_TRIGGER_DIR_P;
	if (argc > 0) {
		switch ((int)argv[0]) {
			case 'p':
			case 'P':
				direction = BREAKER_OCP_TRIGGER_DIR_P;
				break;
			case 'n':
			case 'N':
				direction = BREAKER_OCP_TRIGGER_DIR_N;
				break;
			default:
				direction = BREAKER_OCP_TRIGGER_DIR_P;
		}
	} else {
		direction = BREAKER_OCP_TRIGGER_DIR_P;
	}
	shell_print(shell, 
				"Triggering OCP direction : %c", 
				(direction == BREAKER_OCP_TRIGGER_DIR_P ? 'P' : 'N'));

	breaker_ocp_trigger(direction);

	return 0;
}

static int cmd_version(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "Zephyr version %s", KERNEL_VERSION_STRING);

	breaker_off();

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
}
