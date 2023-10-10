#define DT_DRV_COMPAT infineon_tlx4971

#include <zephyr/kernel.h>
#include <zephyr/drivers/sici.h>
#include <zephyr/drivers/tlx4971.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>
#include <zephyr/timing/timing.h>
#include <math.h>

// #define LOG_LEVEL CONFIG_SICI_LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DBG
LOG_MODULE_REGISTER(tlx4971);

/* clang-format off */
#define CMD_RW_MASK			(0x8000U)
#define CMD_RW_SHIFT			(15U)
#define CMD_RW(x)			(((uint16_t)(((uint16_t)(x)) << CMD_RW_SHIFT)) & CMD_RW_MASK)
#define CMD_AC0_MASK			(0x1000U)
#define CMD_AC0_SHIFT			(12U)
#define CMD_AC0(x)			(((uint16_t)(((uint16_t)(x)) << CMD_AC0_SHIFT)) & CMD_AC0_MASK)
#define CMD_AC1_MASK			(0x2000U)
#define CMD_AC1_SHIFT			(13U)
#define CMD_AC1(x)			(((uint16_t)(((uint16_t)(x)) << CMD_AC1_SHIFT)) & CMD_AC1_MASK)
#define CMD_AD_MASK			(0x0FF0U)
#define CMD_AD_SHIFT			(4U)
#define CMD_AD(x)			(((uint16_t)(((uint16_t)(x)) << CMD_AD_SHIFT)) & CMD_AD_MASK)
#define READ_COMMAND(addr)		(CMD_RW(0) | CMD_AD(addr))
#define WRITE_COMMAND(addr)		(CMD_RW(1) | CMD_AD(addr))

#define REG0_MEAS_RANGE_MASK		(0x001FU)
#define REG0_MEAS_RANGE_SHIFT		(0U)
#define REG0_MEAS_RANGE(x)		(((uint16_t)(((uint16_t)(x)) << REG0_MEAS_RANGE_SHIFT)) & REG0_MEAS_RANGE_MASK)
#define REG0_OP_MODE_MASK		(0x0060U)
#define REG0_OP_MODE_SHIFT		(5U)
#define REG0_OP_MODE(x)			(((uint16_t)(((uint16_t)(x)) << REG0_OP_MODE_SHIFT)) & REG0_OP_MODE_MASK)
#define REG0_OCD1_DEGLITCH_MASK		(0x0380U)
#define REG0_OCD1_DEGLITCH_SHIFT	(7U)
#define REG0_OCD1_DEGLITCH(x)   	(((uint16_t)(((uint16_t)(x)) << REG0_OCD1_DEGLITCH_SHIFT)) & REG0_OCD1_DEGLITCH_MASK)
#define REG0_OCD2_DEGLITCH_MASK		(0x3C00U)
#define REG0_OCD2_DEGLITCH_SHIFT	(10U)
#define REG0_OCD2_DEGLITCH(x)   	(((uint16_t)(((uint16_t)(x)) << REG0_OCD2_DEGLITCH_SHIFT)) & REG0_OCD2_DEGLITCH_MASK)
#define REG0_OCD1_EN_MASK		(0x4000U)
#define REG0_OCD1_EN_SHIFT		(14U)
#define REG0_OCD1_EN(x)			(((uint16_t)(((uint16_t)(x)) << REG0_OCD1_EN_SHIFT)) & REG0_OCD1_EN_MASK)
#define REG0_OCD2_EN_MASK		(0x8000U)
#define REG0_OCD2_EN_SHIFT		(15U)
#define REG0_OCD2_EN(x)			(((uint16_t)(((uint16_t)(x)) << REG0_OCD2_EN_SHIFT)) & REG0_OCD2_EN_MASK)

#define REG1_OCD_COMP_HYST_MASK		(0x000FU)
#define REG1_OCD_COMP_HYST_SHIFT	(0U)
#define REG1_OCD_COMP_HYST(x)		(((uint16_t)(((uint16_t)(x)) << REG1_OCD_COMP_HYST_SHIFT)) & REG1_OCD_COMP_HYST_MASK)
#define REG1_OCD1_THRSH_MASK		(0x03F0U)
#define REG1_OCD1_THRSH_SHIFT		(4U)
#define REG1_OCD1_THRSH(x)		(((uint16_t)(((uint16_t)(x)) << REG1_OCD1_THRSH_SHIFT)) & REG1_OCD1_THRSH_MASK)
#define REG1_OCD2_THRSH_MASK		(0xFC00U)
#define REG1_OCD2_THRSH_SHIFT		(10U)
#define REG1_OCD2_THRSH(x)		(((uint16_t)(((uint16_t)(x)) << REG1_OCD2_THRSH_SHIFT)) & REG1_OCD2_THRSH_MASK)

#define REG2_CRC_MASK			(0x00FFU)
#define REG2_CRC_SHIFT			(0U)
#define REG2_CRC(x)			(((uint16_t)(((uint16_t)(x)) << REG2_CRC_SHIFT)) & REG2_CRC_MASK)
#define REG2_VREF_EXT_MASK		(0x0700U)
#define REG2_VREF_EXT_SHIFT		(8U)
#define REG2_VREF_EXT(x)		(((uint16_t)(((uint16_t)(x)) << REG2_VREF_EXT_SHIFT)) & REG2_VREF_EXT_MASK)
#define REG2_QV1V5_MASK			(0x1000U)
#define REG2_QV1V5_SHIFT		(12U)
#define REG2_QV1V5(x)			(((uint16_t)(((uint16_t)(x)) << REG2_QV1V5_SHIFT)) & REG2_QV1V5_MASK)
#define REG2_OCD2F_MASK			(0x2000U)
#define REG2_OCD2F_SHIFT		(13U)
#define REG2_OCD2F(x)			(((uint16_t)(((uint16_t)(x)) << REG2_OCD2F_SHIFT)) & REG2_OCD2F_MASK)
#define REG2_RATIO_GAIN_MASK		(0x4000U)
#define REG2_RATIO_GAIN_SHIFT		(14U)
#define REG2_RATIO_GAIN(x)		(((uint16_t)(((uint16_t)(x)) << REG2_RATIO_GAIN_SHIFT)) & REG2_RATIO_GAIN_MASK)
#define REG2_RATIO_GAIN_OFF_MASK	(0x8000U)
#define REG2_RATIO_GAIN_OFF_SHIFT	(15U)
#define REG2_RATIO_GAIN_OFF(x)		(((uint16_t)(((uint16_t)(x)) << REG2_RATIO_GAIN_OFF_SHIFT)) & REG2_RATIO_GAIN_OFF_MASK)
/* clang-format on */

/**
 *
 * r = R*f*[1-(h/t)]
 * t = a*f + b
 * Where:
 *     r    : Release point (in A)
 *     R    : Full-scale range (in A)
 *     f    : Threshold factor
 *     t    : Value of OCD_thrsh register field
 *     h    : Value of OCD_comp_hyst register field
 *     a, b : Parameters of fitted f vs t curve
 *
 * 0 = t^2 - [b+h+(r*a/R)]*t + b*h
 * t = [l +- square_root(n)] / (2)
 * Where:
 *     l = b + h + (r*a/R)
 *     m = b*h
 *     n = l^2 - 4*m
 */

struct ocd_curve_item {
	float a;
	float b;
	uint8_t min;
	uint8_t max;
};

struct ocd_calc_info {
	float a;
	float b;
	float R;
	float h;
	uint16_t r;
	uint8_t t;
	uint8_t t_min;
	uint8_t t_max;
};

struct tlx4971_drv_config {
	const struct device *sici;
	struct gpio_dt_spec vref_en_pin;
	struct gpio_dt_spec ocd1_pin;
	struct gpio_dt_spec ocd2_pin;
	const struct pinctrl_dev_config *pincfg;
};

const static struct ocd_curve_item ocd1_curves[] = {
	{.a = 19.669f, .b = -5.420f, .min = 0x13, .max = 0x27},
	{.a = 16.251f, .b = -5.314f, .min = 0x0f, .max = 0x1f},
	{.a = 12.168f, .b = -5.543f, .min = 0x0a, .max = 0x16},
	{.a = 20.999f, .b = -6.248f, .min = 0x14, .max = 0x29},
	{.a = 16.006f, .b = -7.010f, .min = 0x0d, .max = 0x1d},
	{.a = 10.663f, .b = -6.160f, .min = 0x07, .max = 0x12},
};

const static struct ocd_curve_item ocd2_curves[] = {
	{.a = 40.554f, .b = -6.109f, .min = 0x0e, .max = 0x2d},
	{.a = 33.555f, .b = -5.610f, .min = 0x0b, .max = 0x24},
	{.a = 25.342f, .b = -5.674f, .min = 0x07, .max = 0x1a},
	{.a = 43.436f, .b = -7.882f, .min = 0x0e, .max = 0x2e},
	{.a = 31.336f, .b = -6.419f, .min = 0x09, .max = 0x21},
	{.a = 10.112f, .b = -6.723f, .min = 0x04, .max = 0x14},
};

struct tlx4971_drv_data {
	uint16_t regs[3]; /**< Holds values of configurable registers. */
	uint8_t crc_ro;	  /**< Holds CRC for the read-only registers. */
};

static bool set_meas_range(struct tlx4971_drv_data *data, const enum tlx4971_range range)
{
	uint8_t value;

	switch (range) {
	case TLX4971_RANGE_120:
		value = 0x05;
		break;
	case TLX4971_RANGE_100:
		value = 0x06;
		break;
	case TLX4971_RANGE_75:
		value = 0x08;
		break;
	case TLX4971_RANGE_50:
		value = 0x0c;
		break;
	case TLX4971_RANGE_37_5:
		value = 0x10;
		break;
	case TLX4971_RANGE_25:
		value = 0x18;
		break;
	default:
		LOG_ERR("invalid full-scale range %" PRIu8, range);
		return false;
	}

	data->regs[0] = (data->regs[0] & (~REG0_MEAS_RANGE_MASK)) | REG0_MEAS_RANGE(value);

	return true;
}

static bool get_meas_range(const struct tlx4971_drv_data *data, enum tlx4971_range *range)
{
	uint8_t value = (data->regs[0] & REG0_MEAS_RANGE_MASK) >> REG0_MEAS_RANGE_SHIFT;

	switch (value) {
	case 0x05:
		*range = TLX4971_RANGE_120;
		break;
	case 0x06:
		*range = TLX4971_RANGE_100;
		break;
	case 0x08:
		*range = TLX4971_RANGE_75;
		break;
	case 0x0c:
		*range = TLX4971_RANGE_50;
		break;
	case 0x10:
		*range = TLX4971_RANGE_37_5;
		break;
	case 0x18:
		*range = TLX4971_RANGE_25;
		break;
	default:
		LOG_ERR("invalid meas_rng 0x%" PRIu8, value);
		return false;
	}

	return true;
}

static bool calculate_level(struct ocd_calc_info *info)
{
	if (info->t == 0 || info->a == 0.0f) {
		return false;
	}

	info->r = roundf(info->R * (info->t - info->b) * (info->t - info->h) / (info->a * info->t));

	return true;
}

static bool get_ocd_levels(const struct tlx4971_drv_data *data, struct tlx4971_config *config)
{
	struct ocd_calc_info info;

	info.h = (data->regs[1] & REG1_OCD_COMP_HYST_MASK) >> REG1_OCD_COMP_HYST_SHIFT;

	switch (config->range) {
	case TLX4971_RANGE_120:
		info.R = 120.0f;
		break;
	case TLX4971_RANGE_100:
		info.R = 100.0f;
		break;
	case TLX4971_RANGE_75:
		info.R = 75.0f;
		break;
	case TLX4971_RANGE_50:
		info.R = 50.0f;
		break;
	case TLX4971_RANGE_37_5:
		info.R = 37.5f;
		break;
	case TLX4971_RANGE_25:
		info.R = 25.0f;
		break;
	default:
		LOG_ERR("invalid full-scale range %" PRIu8, config->range);
		return false;
	}

	info.a = ocd1_curves[config->range].a;
	info.b = ocd1_curves[config->range].b;
	info.t = (data->regs[1] & REG1_OCD1_THRSH_MASK) >> REG1_OCD1_THRSH_SHIFT;

	if (!calculate_level(&info)) {
		LOG_ERR("ocd1 level caculation failed");
		return false;
	}

	config->ocd1_level = info.r;

	info.a = ocd2_curves[config->range].a;
	info.b = ocd2_curves[config->range].b;
	info.t = (data->regs[1] & REG1_OCD2_THRSH_MASK) >> REG1_OCD2_THRSH_SHIFT;

	if (!calculate_level(&info)) {
		LOG_ERR("ocd2 level caculation failed");
		return false;
	}

	config->ocd2_level = info.r;

	return true;
}

static bool calculate_thrsh(struct ocd_calc_info *info)
{
	float l;
	float n;
	uint8_t t1, t2;

	l = info->b + info->h + (info->r * info->a / info->R);
	n = (l * l) - (4 * info->b * info->h);

	if (n > 0.0f) {
		float sq = sqrtf(n);
		t1 = roundf((l + sq) / 2);
		t2 = roundf((l - sq) / 2);
	} else if (n == 0.0f) {
		t1 = roundf(l / 2);
		t2 = 0;
	} else {
		return false;
	}

	if (info->t_min <= t1 && info->t_max >= t1) {
		info->t = t1;
	} else if (info->t_min <= t2 && info->t_max >= t2) {
		info->t = t2;
	} else {
		return false;
	}

	return true;
}

static bool set_ocd_thrshs(struct tlx4971_drv_data *data, const struct tlx4971_config *config)
{
	struct ocd_calc_info info;

	info.h = (data->regs[1] & REG1_OCD_COMP_HYST_MASK) >> REG1_OCD_COMP_HYST_SHIFT;

	switch (config->range) {
	case TLX4971_RANGE_120:
		info.R = 120.0f;
		break;
	case TLX4971_RANGE_100:
		info.R = 100.0f;
		break;
	case TLX4971_RANGE_75:
		info.R = 75.0f;
		break;
	case TLX4971_RANGE_50:
		info.R = 50.0f;
		break;
	case TLX4971_RANGE_37_5:
		info.R = 37.5f;
		break;
	case TLX4971_RANGE_25:
		info.R = 25.0f;
		break;
	default:
		LOG_ERR("invalid full-scale range %" PRIu8, config->range);
		return false;
	}

	info.a = ocd1_curves[config->range].a;
	info.b = ocd1_curves[config->range].b;
	info.t_min = ocd1_curves[config->range].min;
	info.t_max = ocd1_curves[config->range].max;
	info.r = config->ocd1_level;

	if (!calculate_thrsh(&info)) {
		LOG_ERR("ocd1_thrsh caculation failed");
		return false;
	}

	LOG_DBG("ocd1_thrsh: 0x%" PRIx8, info.t);
	data->regs[1] = (data->regs[1] & (~REG1_OCD1_THRSH_MASK)) | REG1_OCD1_THRSH(info.t);

	info.a = ocd2_curves[config->range].a;
	info.b = ocd2_curves[config->range].b;
	info.t_min = ocd2_curves[config->range].min;
	info.t_max = ocd2_curves[config->range].max;
	info.r = config->ocd2_level;

	if (!calculate_thrsh(&info)) {
		LOG_ERR("ocd2_thrsh caculation failed");
		return false;
	}

	LOG_DBG("ocd2_thrsh: 0x%" PRIx8, info.t);
	data->regs[1] = (data->regs[1] & (~REG1_OCD2_THRSH_MASK)) | REG1_OCD2_THRSH(info.t);

	return true;
}

static uint8_t crc8_reg(uint16_t data, uint8_t crc)
{
	data = (data >> 8) | (data << 8);
	return crc8((const uint8_t *)&data, sizeof(data), 0x1d, crc, false);
}

static inline uint8_t crc8_byte(uint8_t data, uint8_t crc)
{
	return crc8(&data, 1, 0x1d, crc, false);
}

static uint8_t crc8_all(const struct device *dev)
{
	struct tlx4971_drv_data *data = dev->data;
	uint8_t crc = data->crc_ro;

	crc = crc8_reg(data->regs[0], crc);
	crc = crc8_reg(data->regs[1], crc);
	crc = crc8_byte((data->regs[2] >> 8), crc);
	crc = crc ^ 0xff;

	return crc;
}

/**
 * @brief Fetch EEPROM registers.
 *	  This function takes about 90 ms to complete.
 */
static int fetch_regs(const struct device *dev)
{
	const struct tlx4971_drv_config *config = dev->config;
	struct tlx4971_drv_data *data = dev->data;
	uint16_t out = 0;

	if (sici_enable(config->sici, true)) {
		LOG_ERR("unable to enable interface");
		return -EIO;
	}

	/* Power down ISM - write 0x8000 to the address 0x25 */
	sici_transfer(config->sici, WRITE_COMMAND(0x25), &out);
	sici_transfer(config->sici, 0x8000, &out);

	/* Disable failure indication - write 0x0000 to the address 0x01 */
	sici_transfer(config->sici, WRITE_COMMAND(0x01), &out);
	sici_transfer(config->sici, 0x0000, &out);

	/* Read all EEPROM lines 0x40 to 0x51 to calculate CRC (x^8 + x^4 + x^3 + x^2 + 1).
	 * Only store the configurable registers 0x40 to 0x42.
	 */
	data->crc_ro = 0xaa; /* Initialise with seed value */

	sici_transfer(config->sici, READ_COMMAND(0x40), &out);

	for (uint8_t reg_idx = 0; reg_idx < 17; reg_idx++) {
		uint16_t next_addr = 0x41 + reg_idx;

		if (reg_idx < 3) {
			sici_transfer(config->sici, READ_COMMAND(next_addr), &data->regs[reg_idx]);
		} else {
			sici_transfer(config->sici, READ_COMMAND(next_addr), &out);
			data->crc_ro = crc8_reg(out, data->crc_ro);
		}
	}

	sici_transfer(config->sici, 0xFFFF, &out);
	data->crc_ro = crc8_reg(out, data->crc_ro);

	/* Power on ISM - write 0x0000 to the address 0x25 */
	sici_transfer(config->sici, WRITE_COMMAND(0x25), &out);
	sici_transfer(config->sici, 0x0000, &out);

	sici_enable(config->sici, false);

	if ((data->regs[2] & REG2_CRC_MASK) != REG2_CRC(crc8_all(dev))) {
		LOG_ERR("CRC mistach");
		return -EIO;
	}

	return 0;
}

static int set_regs(const struct device *dev, bool is_temp)
{
	const struct tlx4971_drv_config *config = dev->config;
	struct tlx4971_drv_data *data = dev->data;
	uint16_t out = 0;
	int r = 0;

	if (sici_enable(config->sici, true)) {
		LOG_ERR("unable to enable interface");
		return -EIO;
	}

	/* Power down ISM - write 0x8000 to the address 0x25 */
	sici_transfer(config->sici, WRITE_COMMAND(0x25), &out);
	sici_transfer(config->sici, 0x8000, &out);

	/* Disable failure indication - write 0x0000 to the address 0x01 */
	sici_transfer(config->sici, WRITE_COMMAND(0x01), &out);
	sici_transfer(config->sici, 0x0000, &out);

	/* Update CRC. */
	data->regs[2] = (data->regs[2] & (~REG2_CRC_MASK)) | REG2_CRC(crc8_all(dev));

	for (uint8_t reg_idx = 0; reg_idx < 3; reg_idx++) {
		uint16_t addr_base = is_temp ? 0x11 : 0x40;

		sici_transfer(config->sici, WRITE_COMMAND(addr_base + reg_idx), &out);
		sici_transfer(config->sici, data->regs[reg_idx], &out);

		if (!is_temp) {
			/* Send EEPROM program zeros command - write 0x024e to address 0x3e. */
			sici_transfer(config->sici, WRITE_COMMAND(0x3e), &out);
			sici_transfer(config->sici, 0x024e, &out);
			/* @todo Apply programming voltage for 30 ms. */
			k_msleep(30);
			/* Send EEPROM refresh command - write 0x024c to address 0x3e. */
			sici_transfer(config->sici, WRITE_COMMAND(0x3e), &out);
			sici_transfer(config->sici, 0x024c, &out);
			k_usleep(100);

			/* Send data again */
			sici_transfer(config->sici, WRITE_COMMAND(addr_base + reg_idx), &out);
			sici_transfer(config->sici, data->regs[reg_idx], &out);

			/* Send EEPROM program ones command - write 0x024f to address 0x3e. */
			sici_transfer(config->sici, WRITE_COMMAND(0x3e), &out);
			sici_transfer(config->sici, 0x024f, &out);
			/* @todo Apply programming voltage for 30 ms. */
			k_msleep(30);
			/* Send EEPROM refresh command - write 0x024c to address 0x3e. */
			sici_transfer(config->sici, WRITE_COMMAND(0x3e), &out);
			sici_transfer(config->sici, 0x024c, &out);
			k_usleep(100);
		}
	}

	/* Power on ISM - write 0x0000 to the address 0x25 */
	sici_transfer(config->sici, WRITE_COMMAND(0x25), &out);
	sici_transfer(config->sici, 0x0000, &out);

	sici_enable(config->sici, false);

	return r;
}

static int tlx4971_get_config_impl(const struct device *dev, struct tlx4971_config *config)
{
	struct tlx4971_drv_data *data;
	int r;

	if (!dev) {
		return -ENODEV;
	}

	data = dev->data;

	r = fetch_regs(dev);
	if (r) {
		return r;
	}

	LOG_DBG("regs[0]: 0x%" PRIx16, data->regs[0]);
	LOG_DBG("regs[1]: 0x%" PRIx16, data->regs[1]);
	LOG_DBG("regs[2]: 0x%" PRIx16, data->regs[2]);

	memset(config, 0, sizeof(struct tlx4971_config));

	if (!get_meas_range(data, &config->range)) {
		return -EINVAL;
	}

	if (!get_ocd_levels(data, config)) {
		return -EINVAL;
	}

	config->opmode = (data->regs[0] & REG0_OP_MODE_MASK) >> REG0_OP_MODE_SHIFT;
	config->ocd1_en = (data->regs[0] & REG0_OCD1_EN_MASK) >> REG0_OCD1_EN_SHIFT;
	config->ocd2_en = (data->regs[0] & REG0_OCD2_EN_MASK) >> REG0_OCD2_EN_SHIFT;
	config->ocd1_deglitch =
		(data->regs[0] & REG0_OCD1_DEGLITCH_MASK) >> REG0_OCD1_DEGLITCH_SHIFT;
	config->ocd2_deglitch =
		(data->regs[0] & REG0_OCD2_DEGLITCH_MASK) >> REG0_OCD2_DEGLITCH_SHIFT;
	config->is_temp = false;

	LOG_DBG("range: %" PRIu8, config->range);
	LOG_DBG("opmode: %" PRIu8, config->opmode);
	LOG_DBG("ocd1_level: %" PRIu16, config->ocd1_level);
	LOG_DBG("ocd2_level: %" PRIu16, config->ocd2_level);
	LOG_DBG("ocd1_en: %" PRIu8, config->ocd1_en);
	LOG_DBG("ocd2_en: %" PRIu8, config->ocd2_en);
	LOG_DBG("ocd1_deglitch: %" PRIu8, config->ocd1_deglitch);
	LOG_DBG("ocd2_deglitch: %" PRIu8, config->ocd2_deglitch);

	return 0;
}

static int tlx4971_set_config_impl(const struct device *dev, const struct tlx4971_config *config)
{

	struct tlx4971_drv_data *data;

	if (!dev) {
		return -ENODEV;
	}

	if (!config) {
		return -EINVAL;
	}

	data = dev->data;

	if (!set_meas_range(data, config->range)) {
		return -EINVAL;
	}

	if (!set_ocd_thrshs(data, config)) {
		return -EINVAL;
	}

	data->regs[0] = (data->regs[0] & (~REG0_OP_MODE_MASK)) | REG0_OP_MODE(config->opmode);
	data->regs[0] = (data->regs[0] & (~REG0_OCD1_EN_MASK)) | REG0_OCD1_EN(config->ocd1_en);
	data->regs[0] = (data->regs[0] & (~REG0_OCD2_EN_MASK)) | REG0_OCD2_EN(config->ocd2_en);
	data->regs[0] = (data->regs[0] & (~REG0_OCD1_DEGLITCH_MASK)) |
			REG0_OCD1_DEGLITCH(config->ocd1_deglitch);
	data->regs[0] = (data->regs[0] & (~REG0_OCD2_DEGLITCH_MASK)) |
			REG0_OCD2_DEGLITCH(config->ocd2_deglitch);

	return set_regs(dev, config->is_temp);
}

static int tlx4971_init(const struct device *dev)
{
	int r;
	struct tlx4971_config config;

	r = tlx4971_get_config_impl(dev, &config);
	if (r) {
		return r;
	}

	return 0;
}

static const struct tlx4971_driver_api drv_api = {.get_config = tlx4971_get_config_impl,
						  .set_config = tlx4971_set_config_impl};

#define GPIO_DT_SPEC_INST_VREF_GET(n)                                                              \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, vref_gpios), (GPIO_DT_SPEC_INST_GET(n, vref_gpios)),  \
		    ({0}))

#define GPIO_DT_SPEC_INST_OCD1_GET(n)                                                              \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, ocd1_gpios), (GPIO_DT_SPEC_INST_GET(n, ocd1_gpios)),  \
		    ({0}))

#define GPIO_DT_SPEC_INST_OCD2_GET(n)                                                              \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, ocd2_gpios), (GPIO_DT_SPEC_INST_GET(n, ocd2_gpios)),  \
		    ({0}))

#define TLX4971_INST_DEFINE(n)                                                                     \
	PINCTRL_DT_INST_DEFINE(n);                                                                 \
	static struct tlx4971_drv_config drv_config_##n = {                                        \
		.sici = DEVICE_DT_GET(DT_INST_BUS(n)),                                             \
		.vref_en_pin = GPIO_DT_SPEC_INST_VREF_GET(n),                                      \
		.ocd1_pin = GPIO_DT_SPEC_INST_OCD1_GET(n),                                         \
		.ocd2_pin = GPIO_DT_SPEC_INST_OCD2_GET(n),                                         \
		.pincfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),                                       \
	};                                                                                         \
                                                                                                   \
	static struct tlx4971_drv_data drv_data_##n;                                               \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &tlx4971_init, NULL, &drv_data_##n, &drv_config_##n, POST_KERNEL, \
			      CONFIG_SICI_INIT_PRIORITY, &drv_api);

DT_INST_FOREACH_STATUS_OKAY(TLX4971_INST_DEFINE)