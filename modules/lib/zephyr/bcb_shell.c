#include <stdlib.h>
#include <zephyr.h>
#include <device.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <bcb.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_shell);

static void on_ocp(uint64_t duration)
{
    LOG_INF("[OCP] on->off duration: %" PRIu32, (uint32_t)duration);
}

static void on_ocpt(uint32_t reponse_time, int direction)
{
    LOG_INF("[OCPT] direction %c, response time: %" PRIu32 " ns",
            (direction == BCB_OCP_TEST_TGR_DIR_P ? 'P' : 'N'),  
            BCB_ONOFF_TICKS_TO_NS(reponse_time));
}

static int cmd_off_params(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    bcb_off();
    return 0;
}

static int cmd_on_params(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    bcb_on();
    return 0;
}

static int cmd_ocp_trigger_params(const struct shell *shell, size_t argc, char **argv)
{
    int direction = BCB_OCP_TEST_TGR_DIR_P;
    if (argc > 1) {
        switch ((int)argv[1][0]) {
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

    bcb_ocpt_trigger(direction, true);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(breaker_sub,
    SHELL_CMD(on, NULL, "Turn on.", cmd_on_params),
    SHELL_CMD(off, NULL, "Turn off.", cmd_off_params),
    SHELL_CMD(ocpt, NULL, "Trigger OCP.", cmd_ocp_trigger_params),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);
SHELL_CMD_REGISTER(breaker, &breaker_sub, "Breaker commands", NULL);

static int init_breaker_shell()
{
    bcb_set_ocp_callback(on_ocp);
    bcb_set_ocpt_callback(on_ocpt);

    return 0;
}

SYS_INIT(init_breaker_shell, APPLICATION, CONFIG_BCB_LIB_SHELL_INIT_PRIORITY);