#include <lib/bcb.h>
#include <lib/bcb_msmnt.h>
#include <lib/bcb_msmnt_calib.h>
#include <lib/bcb_sw.h>
#include <lib/bcb_zd.h>
#include <lib/bcb_tc_def.h>
#include <stdlib.h>
#include <zephyr.h>
#include <device.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_shell);

struct bcb_shell_data {
	struct bcb_sw_callback sw_callback;
};

static struct bcb_shell_data shell_data;

static int cmd_open_handler(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	bcb_open();
	return 0;
}

static int cmd_close_handler(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	bcb_close();
	return 0;
}

static int cmd_ocp_trigger_handler(const struct shell *shell, size_t argc, char **argv)
{
	int direction = BCB_OCP_DIRECTION_POSITIVE;
	if (argc > 1) {
		switch ((int)argv[1][0]) {
		case 'p':
		case 'P':
			direction = BCB_OCP_DIRECTION_POSITIVE;
			break;
		case 'n':
		case 'N':
			direction = BCB_OCP_DIRECTION_NEGATIVE;
			break;
		default:
			direction = BCB_OCP_DIRECTION_POSITIVE;
		}
	} else {
		direction = BCB_OCP_DIRECTION_POSITIVE;
	}

	bcb_ocp_test_trigger(direction, true);
	return 0;
}

static int cmd_temp_handler(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_print(shell, "Invalid sensor");
		shell_print(shell, "Available sensors:");
		shell_print(shell, "    %d: PWR_IN", BCB_TEMP_SENSOR_PWR_IN);
		shell_print(shell, "    %d: PWR_OUT", BCB_TEMP_SENSOR_PWR_OUT);
		shell_print(shell, "    %d: AMB", BCB_TEMP_SENSOR_AMB);
		shell_print(shell, "    %d: MCU", BCB_TEMP_SENSOR_MCU);
		return -1;
	}
	bcb_temp_sensor_t sensor = (bcb_temp_sensor_t)(argv[1][0] - '0');
	shell_print(shell, "%d C", bcb_msmnt_get_temp(sensor));
	return 0;
}

static int cmd_voltage_handler(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "%" PRId32 " mV", bcb_msmnt_get_voltage());

	return 0;
}

static int cmd_current_handler(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell,
		    "%06" PRId32 " mA, [h: %06" PRId32 " mA, %" PRIu16 "|l: %06" PRId32
		    " mA, %" PRIu16 "]",
		    bcb_msmnt_get_current(), bcb_msmnt_get_current_high_gain(),
		    bcb_msmnt_get_raw(BCB_MSMNT_TYPE_I_HIGH_GAIN), bcb_msmnt_get_current_low_gain(),
		    bcb_msmnt_get_raw(BCB_MSMNT_TYPE_I_LOW_GAIN));

	return 0;
}

static int cmd_frequency_handler(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	uint32_t frequency = bcb_zd_get_frequency();
	shell_print(shell, "%" PRIu32 ".%03" PRIu32 " Hz", frequency / 1000, frequency % 1000);

	return 0;
}

static int cmd_calib_adc_handler(const struct shell *shell, size_t argc, char **argv)
{
	int r;
	uint16_t samples;

	if (argc < 2) {
		shell_error(shell, "%s - number of samples not given", argv[0]);
		shell_print(shell, "%s - <samples>", argv[0]);
		return -EINVAL;
	}

	samples = (uint16_t)atoi(argv[1]);

	shell_print(shell, "Starting ADC calibration with %" PRIu16 " samples", samples);
	r = bcb_msmnt_calib_adc(samples);
	shell_print(shell, "ADC calibration completed: %d", r);

	return 0;
}

static int cmd_calib_param_a_handler(const struct shell *shell, size_t argc, char **argv)
{
	bcb_msmnt_type_t type;
	uint16_t samples;
	int32_t input_x;
	int r;

	if (argc != 4) {
		shell_error(shell, "invalid arguments", argv[0]);
		shell_print(shell, "%s - <l|h|v> <input-x> <samples>", argv[0]);
		shell_print(shell, " - l: current low gain");
		shell_print(shell, " - h: current high gain");
		shell_print(shell, " - v: mains voltage");
		return -EINVAL;
	}

	if (argv[1][0] == 'l' || argv[1][0] == 'L') {
		type = BCB_MSMNT_TYPE_I_LOW_GAIN;
	} else if (argv[1][0] == 'h' || argv[1][0] == 'H') {
		type = BCB_MSMNT_TYPE_I_HIGH_GAIN;
	} else if (argv[1][0] == 'v' || argv[1][0] == 'V') {
		type = BCB_MSMNT_TYPE_V_MAINS;
	} else {
		shell_error(shell, "invalid type %c", argv[0], argv[1][0]);
		return -EINVAL;
	}

	input_x = (int32_t)atoi(argv[2]);
	samples = (uint16_t)atoi(argv[3]);

	shell_print(shell, "starting parameter a calibration: samples %" PRIu16, samples);
	r = bcb_msmnt_calib_a(type, input_x, samples);
	if (r) {
		shell_error(shell, "calibration failed %d", r);
		return r;
	}
	shell_print(shell, "calbration completed");

	return 0;
}

static int cmd_calib_param_b_handler(const struct shell *shell, size_t argc, char **argv)
{
	bcb_msmnt_type_t type;
	uint16_t samples;
	int r;

	if (argc != 3) {
		shell_error(shell, "invalid arguments", argv[0]);
		shell_print(shell, "%s - <l|h|v> <samples>", argv[0]);
		shell_print(shell, " - l: current low gain");
		shell_print(shell, " - h: current high gain");
		shell_print(shell, " - v: mains voltage");
		return -EINVAL;
	}

	if (argv[1][0] == 'l' || argv[1][0] == 'L') {
		type = BCB_MSMNT_TYPE_I_LOW_GAIN;
	} else if (argv[1][0] == 'h' || argv[1][0] == 'H') {
		type = BCB_MSMNT_TYPE_I_HIGH_GAIN;
	} else if (argv[1][0] == 'v' || argv[1][0] == 'V') {
		type = BCB_MSMNT_TYPE_V_MAINS;
	} else {
		shell_error(shell, "invalid type %c", argv[0], argv[1][0]);
		return -EINVAL;
	}

	samples = (uint16_t)atoi(argv[2]);
	shell_print(shell, "starting parameter b calibration: samples %" PRIu16, samples);
	r = bcb_msmnt_calib_b(type, samples);
	if (r) {
		shell_error(shell, "calibration failed %d", r);
		return r;
	}
	shell_print(shell, "calbration completed");

	return 0;
}

static void on_swtich_event(bool is_closed, bcb_sw_cause_t cause)
{
	if (!is_closed) {
		if (cause == BCB_SW_CAUSE_OCP_TEST) {
			char direction;
			direction = bcb_ocp_test_get_direction() == BCB_OCP_DIRECTION_NEGATIVE ?
					    'N' :
					    'P';
			LOG_INF("OCP Test completed: direction %c, duration %" PRIu32 " ns",
				direction, bcb_ocp_test_get_duration());
		}
	}
}

int bcb_shell_init(void)
{
	memset(&shell_data, 0, sizeof(shell_data));
	shell_data.sw_callback.handler = on_swtich_event;

	bcb_sw_add_callback(&shell_data.sw_callback);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	calibrate_sub, SHELL_CMD(adc, NULL, "Calibrate ADCs", cmd_calib_adc_handler),
	SHELL_CMD(param_a, NULL, "Calibrate parameter a", cmd_calib_param_a_handler),
	SHELL_CMD(param_b, NULL, "Calibrate parameter b", cmd_calib_param_b_handler),
	SHELL_SUBCMD_SET_END /* Array terminated. */);

SHELL_STATIC_SUBCMD_SET_CREATE(breaker_sub,
			       SHELL_CMD(close, NULL, "Close switch.", cmd_close_handler),
			       SHELL_CMD(open, NULL, "Open switch.", cmd_open_handler),
			       SHELL_CMD(ocpt, NULL, "Trigger OCP.", cmd_ocp_trigger_handler),
			       SHELL_CMD(temp, NULL, "Get temperature.", cmd_temp_handler),
			       SHELL_CMD(voltage, NULL, "Get voltage.", cmd_voltage_handler),
			       SHELL_CMD(current, NULL, "Get current.", cmd_current_handler),
			       SHELL_CMD(frequency, NULL, "Get frequency.", cmd_frequency_handler),
			       SHELL_CMD(calibrate, &calibrate_sub, "Calibrate measurement system.",
					 NULL),
			       SHELL_SUBCMD_SET_END /* Array terminated. */
);
SHELL_CMD_REGISTER(breaker, &breaker_sub, "Breaker commands", NULL);
