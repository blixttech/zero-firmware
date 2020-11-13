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
    LOG_INF("[OCP] on->off duration: %" PRIu32 " ticks", (uint32_t)duration);
}

static void on_ocpt(uint64_t reponse_time, int direction)
{
    LOG_INF("[OCPT] direction %c, response time: %" PRIu32 " ns",
            (direction == BCB_OCP_TEST_TGR_DIR_P ? 'P' : 'N'),  
            (uint32_t)(BCB_ONOFF_TICKS_TO_NS(reponse_time)));
}

static void on_cal_done()
{

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

static int cmd_calibrate_params(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    bcb_msmnt_cal_start(on_cal_done);

    return 0;
}

#if CONFIG_BCB_LIB_ADC_DUMP_ENABLE
static void on_adc_dump_done()
{

}

static int cmd_adc_dump_params(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (argc > 1) {
        switch ((int)argv[1][0]) {
            case 's':
            case 'S':
                bcb_msmnt_adc_dump_sstart(on_adc_dump_done);
                break;
            case 'd':
            case 'D':
                bcb_msmnt_adc_dump_dstart(on_adc_dump_done);
                break;
            case 'q':
            case 'Q':
                bcb_msmnt_adc_dump_stop();
                break;
            default:
                bcb_msmnt_adc_dump_stop();
        }
    } else {
        bcb_msmnt_adc_dump_stop();
    }

    return 0;
}
#endif // CONFIG_BCB_LIB_ADC_DUMP_ENABLE

SHELL_STATIC_SUBCMD_SET_CREATE(breaker_sub,
    SHELL_CMD(on, NULL, "Turn on.", cmd_on_params),
    SHELL_CMD(off, NULL, "Turn off.", cmd_off_params),
    SHELL_CMD(ocpt, NULL, "Trigger OCP.", cmd_ocp_trigger_params),
    SHELL_CMD(cal, NULL, "Calibrate", cmd_calibrate_params),
#if CONFIG_BCB_LIB_ADC_DUMP_ENABLE
    SHELL_CMD(adc_dump, NULL, "ADC Dump", cmd_adc_dump_params),
#endif // CONFIG_BCB_LIB_ADC_DUMP_ENABLE
    SHELL_SUBCMD_SET_END /* Array terminated. */
);
SHELL_CMD_REGISTER(breaker, &breaker_sub, "Breaker commands", NULL);

static int init_breaker_shell()
{
    bcb_set_ocp_callback(on_ocp);
    bcb_set_ocpt_callback(on_ocpt);

    shell_execute_cmd(shell_backend_uart_get_ptr(), "shell colors off");

    return 0;
}

SYS_INIT(init_breaker_shell, APPLICATION, CONFIG_BCB_LIB_SHELL_INIT_PRIORITY);