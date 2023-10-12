#include <zephyr/shell/shell.h>
#include <zephyr/drivers/tlx4971.h>

static struct tlx4971_config config;

static const char *range_labels[] = {"A_120", "A_100", "A_75", "A_50", "A_37_5", "A_25"};

static const char *opmode_labels[] = {"SD_BID", "FD", "SD_UNI", "SE"};

static const char *deglitch_labels[] = {
	"NS_0",	   "NS_500",  "NS_1000", "NS_1500", "NS_2000", "NS_2500", "NS_3000", "NS_3500",
	"NS_4000", "NS_4500", "NS_5000", "NS_5500", "NS_6000", "NS_6500", "NS_7000", "NS_7500"};

static const char *vref_ext_labels[] = {"V_1_65", "V_1_2", "V_1_5", "V_1_8"};

static void show_config(const struct shell *shell)
{
	shell_print(shell, "range         : %s", range_labels[config.range]);
	shell_print(shell, "opmode        : %s", opmode_labels[config.opmode]);
	shell_print(shell, "ocd1_level    : %" PRIu16, config.ocd1_level);
	shell_print(shell, "ocd2_level    : %" PRIu16, config.ocd2_level);
	shell_print(shell, "ocd1_en       : %" PRIu8, config.ocd1_en);
	shell_print(shell, "ocd2_en       : %" PRIu8, config.ocd2_en);
	shell_print(shell, "ocd1_deglitch : %s", deglitch_labels[config.ocd1_deglitch]);
	shell_print(shell, "ocd2_deglitch : %s", deglitch_labels[config.ocd2_deglitch]);
	shell_print(shell, "vref_ext      : %s", vref_ext_labels[config.vref_ext]);
	shell_print(shell, "");
}

static int cmd_read(const struct shell *shell, size_t argc, char **argv)
{
	const struct device *dev;
	int r;

	dev = device_get_binding(argv[1]);
	if (!dev) {
		shell_error(shell, "TLX4971 device not found");
		return -EINVAL;
	}

	r = tlx4971_get_config(dev, &config);
	if (r) {
		shell_error(shell, "cannot get TLX4971 device config");
		return -EINVAL;
	}

	show_config(shell);

	return 0;
}

static int cmd_program(const struct shell *shell, size_t argc, char **argv)
{
	const struct device *dev;
	int r;

	dev = device_get_binding(argv[1]);
	if (!dev) {
		shell_error(shell, "TLX4971 device not found");
		return -EINVAL;
	}

	r = tlx4971_set_config(dev, &config);
	if (r) {
		shell_error(shell, "cannot set TLX4971 device config");
		return -EINVAL;
	}

	return 0;
}

static int cmd_range(const struct shell *shell, size_t argc, char **argv, void *data)
{
	config.range = (enum tlx4971_range)data;
	return 0;
}

static int cmd_opmode(const struct shell *shell, size_t argc, char **argv, void *data)
{
	config.opmode = (enum tlx4971_opmode)data;
	return 0;
}

static int cmd_vref_ext(const struct shell *shell, size_t argc, char **argv, void *data)
{
	config.vref_ext = (enum tlx4971_vref_ext)data;
	return 0;
}

static int cmd_ocd_enable(const struct shell *shell, size_t argc, char **argv)
{
	/* E.g. tlx4971 set ocd1 enable */
	if (argv[-1][3] == '1') {
		config.ocd1_en = true;
	} else if (argv[-1][3] == '2') {
		config.ocd2_en = true;
	} else {
		shell_error(shell, "invalid ocd channel : %c", argv[-2][3]);
		return -EINVAL;
	}

	return 0;
}

static int cmd_ocd_disable(const struct shell *shell, size_t argc, char **argv)
{
	/* E.g. tlx4971 set ocd1 disable */
	if (argv[-1][3] == '1') {
		config.ocd1_en = false;
	} else if (argv[-1][3] == '2') {
		config.ocd2_en = false;
	} else {
		shell_error(shell, "invalid ocd channel : %c", argv[-2][3]);
		return -EINVAL;
	}

	return 0;
}

static int cmd_ocd_level(const struct shell *shell, size_t argc, char **argv)
{
	/* E.g. tlx4971 set ocd1 level 100 */
	uint16_t level;

	if (argc != 2) {
		shell_help(shell);
		return -EINVAL;
	}

	level = strtoul(argv[1], NULL, 0);

	if (argv[-1][3] == '1') {
		config.ocd1_level = level;
	} else if (argv[-1][3] == '2') {
		config.ocd2_level = level;
	} else {
		shell_error(shell, "invalid ocd channel : %c", argv[-2][3]);
		return -EINVAL;
	}

	return 0;
}

static int cmd_ocd_deglitch(const struct shell *shell, size_t argc, char **argv, void *data)
{
	/* E.g. tlx4971 set ocd1 deglitch DEGLITCH_0 */
	if (argv[-2][3] == '1') {
		config.ocd1_deglitch = (enum tlx4971_deglitch)data;
	} else if (argv[-2][3] == '2') {
		config.ocd2_deglitch = (enum tlx4971_deglitch)data;
	} else {
		shell_error(shell, "invalid ocd channel : %c", argv[-2][3]);
		return -EINVAL;
	}

	return 0;
}

/* Device name autocompletion support */
static void device_name_get(size_t idx, struct shell_static_entry *entry)
{
	const struct device *dev = shell_device_lookup(idx, NULL);

	entry->syntax = (dev != NULL) ? dev->name : NULL;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;
}

/* clang-format off */
SHELL_SUBCMD_DICT_SET_CREATE(sub_range, cmd_range, 
			     (A_120, TLX4971_RANGE_120, "+/-120A"),
			     (A_100, TLX4971_RANGE_100, "+/-100A"),
			     (A_75, TLX4971_RANGE_75, "+/-75A"),
			     (A_50, TLX4971_RANGE_50, "+/-50A"),
			     (A_37_5, TLX4971_RANGE_37_5, "+/-37.5A"),
			     (A_25, TLX4971_RANGE_25, "+/-25A"));

SHELL_SUBCMD_DICT_SET_CREATE(sub_opmode, cmd_opmode, 
			     (SD_BID, TLX4971_OPMODE_SD_BID, "Semi-differential (bidirectional)"),
			     (FD, TLX4971_OPMODE_FD, "Fully-differential"),
			     (SD_UNI, TLX4971_OPMODE_SD_UNI, "Semi-differential (unidirectional)"),
			     (SE, TLX4971_OPMODE_SE, "Single-ended"));

SHELL_SUBCMD_DICT_SET_CREATE(sub_deglitch, cmd_ocd_deglitch, 
			     (NS_0, TLX4971_DEGLITCH_0, "0 ns"),
			     (NS_500, TLX4971_DEGLITCH_500, "500 ns"),
			     (NS_1000, TLX4971_DEGLITCH_1000, "1000 ns"),
			     (NS_1500, TLX4971_DEGLITCH_1500, "1500 ns"),
			     (NS_2000, TLX4971_DEGLITCH_2000, "2000 ns"),
			     (NS_2500, TLX4971_DEGLITCH_2500, "2500 ns"),
			     (NS_3000, TLX4971_DEGLITCH_3000, "3000 ns"),
			     (NS_3500, TLX4971_DEGLITCH_3500, "3500 ns"),
			     (NS_4000, TLX4971_DEGLITCH_4000, "4000 ns"),
			     (NS_4500, TLX4971_DEGLITCH_4500, "4500 ns"),
			     (NS_5000, TLX4971_DEGLITCH_5000, "5000 ns"),
			     (NS_5500, TLX4971_DEGLITCH_5500, "5500 ns"),
			     (NS_6000, TLX4971_DEGLITCH_6000, "6000 ns"),
			     (NS_6500, TLX4971_DEGLITCH_6500, "6500 ns"),
			     (NS_7000, TLX4971_DEGLITCH_7000, "7000 ns"),
			     (NS_7500, TLX4971_DEGLITCH_7500, "7500 ns"));

SHELL_SUBCMD_DICT_SET_CREATE(sub_vref_ext, cmd_vref_ext, 
			     (V_1_65, TLX4971_VREF_EXT_1V65, "1.65V"),
			     (V_1_2, TLX4971_VREF_EXT_1V2, "1.2V"),
			     (V_1_5, TLX4971_VREF_EXT_1V5, "1.5V"),
			     (V_1_8, TLX4971_VREF_EXT_1V8, "1.8V"));

SHELL_STATIC_SUBCMD_SET_CREATE(sub_ocd,
			       SHELL_CMD(level, NULL, "Current level (A)", cmd_ocd_level),
			       SHELL_CMD(enable, NULL, "Output enable", cmd_ocd_enable),
			       SHELL_CMD(disable, NULL, "Output disable", cmd_ocd_disable),
			       SHELL_CMD(deglitch, &sub_deglitch, "Deglitch duration", NULL),
			       SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_set,
		               SHELL_CMD(range, &sub_range, "Measurement range", NULL),
		               SHELL_CMD(opmode, &sub_opmode, "Operational mode", NULL),
			       SHELL_CMD(vref_ext, &sub_vref_ext, "Reference voltage", NULL),
			       SHELL_CMD(ocd1, &sub_ocd, "OCD1 settings", NULL),
			       SHELL_CMD(ocd2, &sub_ocd, "OCD2 settings", NULL),
			       SHELL_SUBCMD_SET_END);

SHELL_DYNAMIC_CMD_CREATE(dsub_device_name, device_name_get);

SHELL_STATIC_SUBCMD_SET_CREATE(tlx4971_cmds,
			       SHELL_CMD_ARG(read, &dsub_device_name, "Read device",
			       		     cmd_read, 2, 0),
			       SHELL_CMD_ARG(program, &dsub_device_name, "Program device",
			       		     cmd_program, 2, 0),
			       SHELL_CMD(set, &sub_set, "Settings", NULL),
			       SHELL_SUBCMD_SET_END);
/* clang-format on */
SHELL_CMD_REGISTER(tlx4971, &tlx4971_cmds, "TLX4971 shell commands", NULL);
