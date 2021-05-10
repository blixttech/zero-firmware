#include <lib/bcb_msmnt.h>
#include <lib/bcb_config.h>
#include <drivers/adc_dma.h>
#include <device.h>
#include <devicetree.h>
#include <arm_math.h>
#include <string.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_msmnt);

#define BCB_MSMNT_RMS_SAMPLES ((1U) << CONFIG_BCB_LIB_MSMNT_RMS_SAMPLES)
#define BCB_MSMNT_RMS_SAMPLES_SHIFT (CONFIG_BCB_LIB_MSMNT_RMS_SAMPLES)

#define BCB_MSMNT_ADC_SEQ_ADD(ds, dt_node, ch_name)                                                \
	do {                                                                                       \
		struct adc_dma_channel_config cfg;                                                 \
		cfg.channel = DT_PHA_BY_NAME(DT_NODELABEL(dt_node), io_channels, ch_name, input);  \
		cfg.alt_channel =                                                                  \
			DT_PHA_BY_NAME(DT_NODELABEL(dt_node), io_channels, ch_name, muxsel);       \
		struct device *dev = device_get_binding(DT_LABEL(                                  \
			DT_PHANDLE_BY_NAME(DT_NODELABEL(dt_node), io_channels, ch_name)));         \
		if (dev == (ds)->dev_adc_0) {                                                      \
			(ds)->raw_##ch_name = &((ds)->buffer_adc_0[(ds)->seq_len_adc_0]);          \
			adc_dma_channel_setup((ds)->dev_adc_0, ((ds)->seq_len_adc_0++), &cfg);     \
		} else if (dev == (ds)->dev_adc_1) {                                               \
			(ds)->raw_##ch_name = &((ds)->buffer_adc_1[(ds)->seq_len_adc_1]);          \
			adc_dma_channel_setup((ds)->dev_adc_1, ((ds)->seq_len_adc_1++), &cfg);     \
		} else {                                                                           \
			LOG_ERR("Unknown ADC device");                                             \
		}                                                                                  \
	} while (0)

typedef struct __attribute__((packed)) bcb_msmnt_config_data {
	uint16_t i_low_gain_cal_a;
	uint16_t i_low_gain_cal_b;
	uint16_t i_high_gain_cal_a;
	uint16_t i_high_gain_cal_b;
	uint16_t v_mains_cal_a;
	uint16_t v_mains_cal_b;
} bcb_msmnt_config_data_t;

struct bcb_msmnt_data {
	/* ADC0 */
	struct device *dev_adc_0;
	uint8_t seq_len_adc_0;
	volatile uint16_t *buffer_adc_0;
	size_t buffer_size_adc_0;
	/* ADC1 */
	struct device *dev_adc_1;
	uint8_t seq_len_adc_1;
	volatile uint16_t *buffer_adc_1;
	size_t buffer_size_adc_1;
	/* ADC0 channel values */
	volatile uint16_t *raw_i_low_gain;
	volatile uint16_t *raw_i_high_gain;
	volatile uint16_t *raw_v_mains;
	/* ADC1 channel values */
	volatile uint16_t *raw_t_mosfet_in;
	volatile uint16_t *raw_t_mosfet_out;
	volatile uint16_t *raw_t_ambient;
	volatile uint16_t *raw_t_mcu;
	volatile uint16_t *raw_hw_rev_in;
	volatile uint16_t *raw_hw_rev_out;
	volatile uint16_t *raw_hw_rev_ctrl;
	volatile uint16_t *raw_oc_test_adj;
	volatile uint16_t *raw_ref_1v5;
	/* Configuration related */
	bcb_msmnt_config_data_t config;
	/* RMS calculation related */
	uint64_t i_low_gain_diff_sqrd_acc;
	uint64_t i_high_gain_diff_sqrd_acc;
	uint64_t v_mains_diff_sqrd_acc;
	uint32_t i_low_gain_rms;
	uint32_t i_high_gain_rms;
	uint32_t v_mains_rms;
	uint8_t rms_samples;
	struct k_timer timer_rms;
};

typedef struct bcb_msmnt_ntc_tbl {
	uint32_t r; /* Resistance in ohms */
	uint32_t b;
} bcb_msmnt_ntc_tbl_t;

static struct bcb_msmnt_data bcb_msmnt_data;

static uint16_t buffer_adc_0[DT_PROP(DT_NODELABEL(adc0), max_channels)] __attribute__((aligned(2)));
static uint16_t buffer_adc_1[DT_PROP(DT_NODELABEL(adc1), max_channels)] __attribute__((aligned(2)));

static bcb_msmnt_ntc_tbl_t bcb_msmnt_ntc_tbl_data[] = {
	{ 1135000U, 4075U }, /* -20C */
	{ 355600U, 4133U }, /* 0C */
	{ 127000U, 4185U }, /* 20C */
	{ 100000U, 4197U }, /* 25C */
	{ 50680U, 4230U }, /* 40C */
	{ 22220U, 4269U }, /* 60C */
	{ 10580U, 4301U }, /* 80C */
	{ 5410U, 4327U }, /* 100C */
};

static void bcb_msmnt_on_rms_timer(struct k_timer *timer);

static int32_t get_temp_adc(uint32_t adc_ntc)
{
	/* ADC is referenced to 3V (3000 millivolt). */
	uint32_t v_ntc = (3000 * adc_ntc) >> 16;
	/* NTC is connected to 3V3 (3300 millivolt) rail via a 56k resistor. */
	uint32_t r_tnc = v_ntc * 56000 / (3300 - v_ntc);
	uint32_t b_ntc = 4197U;
	int i;
	/* Find the best value for beta (B) */
	for (i = 0; i < sizeof(bcb_msmnt_ntc_tbl_data) / sizeof(bcb_msmnt_ntc_tbl_t); i++) {
		if (r_tnc > bcb_msmnt_ntc_tbl_data[i].r) {
			b_ntc = bcb_msmnt_ntc_tbl_data[i].b;
			break;
		}
	}
	/* 
	 *           T  ⋅ β      
	 *            0          
	 * T = ──────────────────
	 *         ⎛        ⎛ R⎞⎞
	 *     β + ⎜T  ⋅ ln ⎜──⎟⎟
	 *         ⎜ 0      ⎜R ⎟⎟
	 *         ⎝        ⎝ 0⎠⎠
	 *
	 *    ⎛ R⎞                  
	 * ln ⎜──⎟ = ln(R) - ln ⎛R ⎞
	 *    ⎜R ⎟              ⎝ 0⎠
	 *    ⎝ 0⎠
	 *
	 * T  = 298.15 K (25C)     
	 *  0               
	 * R  = 100000 ohms     
	 *  0               
	 * ln ⎛R ⎞ = 11.5130
	 *    ⎝ 0⎠
	 */
	float temp_k = (298.15f * ((float)b_ntc));
	temp_k /= ((float)b_ntc) + (298.15f * (logf(r_tnc) - 11.5130f));
	/* Convert the value back to celsius and into integer */
	//return (int32_t)((temp_k - 273.15f) * 100.0f);
	return (int32_t)(temp_k - 273.15f);
}

static int32_t get_temp_mcu(uint32_t adc_ntc)
{
	uint32_t v_ntc = (3000U * adc_ntc) >> 16;
	//return (int32_t)((25.0f - (((float)v_ntc - 716.0f) / 1.62f)) * 100.0f);
	return (int32_t)(25.0f - (((float)v_ntc - 716.0f) / 1.62f));
}

/**
 * @brief   Returns the temperature read from a sensor
 * 
 * @param sensor    Type of the sensor. 
 * @return int32_t  Temperature in centigrade.
 */
int32_t bcb_msmnt_get_temp(bcb_temp_sensor_t sensor)
{
	switch (sensor) {
	case BCB_TEMP_SENSOR_PWR_IN:
		return (int32_t)get_temp_adc(*(bcb_msmnt_data.raw_t_mosfet_in));
	case BCB_TEMP_SENSOR_PWR_OUT:
		return (int32_t)get_temp_adc(*(bcb_msmnt_data.raw_t_mosfet_out));
	case BCB_TEMP_SENSOR_AMB:
		return (int32_t)get_temp_adc(*(bcb_msmnt_data.raw_t_ambient));
	case BCB_TEMP_SENSOR_MCU:
		return (int32_t)get_temp_mcu(*(bcb_msmnt_data.raw_t_mcu));
	default:
		LOG_ERR("Invalid sensor");
	}
	return 0;
}

uint16_t bcb_msmnt_get_raw(bcb_msmnt_type_t type)
{
	switch (type) {
	case BCB_MSMNT_TYPE_I_LOW_GAIN:
		return (*bcb_msmnt_data.raw_i_low_gain);
	case BCB_MSMNT_TYPE_I_HIGH_GAIN:
		return (*bcb_msmnt_data.raw_i_high_gain);
	case BCB_MSMNT_TYPE_V_MAINS:
		return (*bcb_msmnt_data.raw_v_mains);
	case BCB_MSMNT_TYPE_T_PWR_IN:
		return (*bcb_msmnt_data.raw_t_mosfet_in);
	case BCB_MSMNT_TYPE_T_PWR_OUT:
		return (*bcb_msmnt_data.raw_t_mosfet_out);
	case BCB_MSMNT_TYPE_T_AMB:
		return (*bcb_msmnt_data.raw_t_ambient);
	case BCB_MSMNT_TYPE_T_MCU:
		return (*bcb_msmnt_data.raw_t_mcu);
	case BCB_MSMNT_TYPE_REV_IN:
		return (*bcb_msmnt_data.raw_hw_rev_in);
	case BCB_MSMNT_TYPE_REV_OUT:
		return (*bcb_msmnt_data.raw_hw_rev_out);
	case BCB_MSMNT_TYPE_REV_CTRL:
		return (*bcb_msmnt_data.raw_hw_rev_ctrl);
	case BCB_MSMNT_TYPE_OCP_TEST_ADJ:
		return (*bcb_msmnt_data.raw_oc_test_adj);
	case BCB_MSMNT_TYPE_V_REF_1V5:
		return (*bcb_msmnt_data.raw_ref_1v5);
	default:
		LOG_ERR("Invalid measurement type");
	}
	return 0;
}

int bcb_msmnt_set_calib_param_a(bcb_msmnt_type_t type, uint16_t a)
{
	if (!a) {
		LOG_ERR("Invalid calibration value");
		return -EINVAL;
	}

	switch (type) {
	case BCB_MSMNT_TYPE_I_LOW_GAIN:
		bcb_msmnt_data.config.i_low_gain_cal_a = a;
		break;
	case BCB_MSMNT_TYPE_I_HIGH_GAIN:
		bcb_msmnt_data.config.i_high_gain_cal_a = a;
		break;
	case BCB_MSMNT_TYPE_V_MAINS:
		bcb_msmnt_data.config.v_mains_cal_a = a;
		break;
	default:
		LOG_ERR("Invalid measurement type");
		return -EINVAL;
	}

	return 0;
}

int bcb_msmnt_set_calib_param_b(bcb_msmnt_type_t type, uint16_t b)
{
	if (!b) {
		LOG_ERR("Invalid calibration value");
		return -EINVAL;
	}

	switch (type) {
	case BCB_MSMNT_TYPE_I_LOW_GAIN:
		bcb_msmnt_data.config.i_low_gain_cal_b = b;
		break;
	case BCB_MSMNT_TYPE_I_HIGH_GAIN:
		bcb_msmnt_data.config.i_high_gain_cal_b = b;
		break;
	case BCB_MSMNT_TYPE_V_MAINS:
		bcb_msmnt_data.config.v_mains_cal_b = b;
		break;
	default:
		LOG_ERR("Invalid measurement type");
		return -EINVAL;
	}

	return 0;
}

int bcb_msmnt_get_calib_param_a(bcb_msmnt_type_t type, uint16_t *a)
{
	switch (type) {
	case BCB_MSMNT_TYPE_I_LOW_GAIN:
		*a = bcb_msmnt_data.config.i_low_gain_cal_a;
		break;
	case BCB_MSMNT_TYPE_I_HIGH_GAIN:
		*a = bcb_msmnt_data.config.i_high_gain_cal_a;
		break;
	case BCB_MSMNT_TYPE_V_MAINS:
		*a = bcb_msmnt_data.config.v_mains_cal_a;
		break;
	default:
		LOG_ERR("Invalid measurement type");
		return -EINVAL;
	}

	return 0;
}

int bcb_msmnt_get_calib_param_b(bcb_msmnt_type_t type, uint16_t *b)
{
	switch (type) {
	case BCB_MSMNT_TYPE_I_LOW_GAIN:
		*b = bcb_msmnt_data.config.i_low_gain_cal_b;
		break;
	case BCB_MSMNT_TYPE_I_HIGH_GAIN:
		*b = bcb_msmnt_data.config.i_high_gain_cal_b;
		break;
	case BCB_MSMNT_TYPE_V_MAINS:
		*b = bcb_msmnt_data.config.v_mains_cal_b;
		break;
	default:
		LOG_ERR("Invalid measurement type");
		return -EINVAL;
	}

	return 0;
}

static void bcb_msmnt_on_rms_timer(struct k_timer *timer)
{
	int32_t adc_diff;

	adc_diff = (int32_t)*bcb_msmnt_data.raw_i_low_gain -
		   (int32_t)bcb_msmnt_data.config.i_low_gain_cal_b;
	bcb_msmnt_data.i_low_gain_diff_sqrd_acc += (uint64_t)(adc_diff * adc_diff);

	adc_diff = (int32_t)*bcb_msmnt_data.raw_i_high_gain -
		   (int32_t)bcb_msmnt_data.config.i_high_gain_cal_b;
	bcb_msmnt_data.i_high_gain_diff_sqrd_acc += (uint64_t)(adc_diff * adc_diff);

	adc_diff =
		(int32_t)*bcb_msmnt_data.raw_v_mains - (int32_t)bcb_msmnt_data.config.v_mains_cal_b;
	bcb_msmnt_data.v_mains_diff_sqrd_acc += (uint64_t)(adc_diff * adc_diff);

	bcb_msmnt_data.rms_samples++;

	if (bcb_msmnt_data.rms_samples == BCB_MSMNT_RMS_SAMPLES) {
		uint32_t diff_rms;
		uint32_t a;

		a = bcb_msmnt_data.config.i_low_gain_cal_a;
		bcb_msmnt_data.i_low_gain_diff_sqrd_acc =
			bcb_msmnt_data.i_low_gain_diff_sqrd_acc >> BCB_MSMNT_RMS_SAMPLES_SHIFT;
		diff_rms = (uint32_t)sqrtl((double)bcb_msmnt_data.i_low_gain_diff_sqrd_acc);
		bcb_msmnt_data.i_low_gain_rms = (diff_rms * 1000) / a;

		a = bcb_msmnt_data.config.i_high_gain_cal_a;
		bcb_msmnt_data.i_high_gain_diff_sqrd_acc =
			bcb_msmnt_data.i_high_gain_diff_sqrd_acc >> BCB_MSMNT_RMS_SAMPLES_SHIFT;
		diff_rms = (uint32_t)sqrtl((double)bcb_msmnt_data.i_high_gain_diff_sqrd_acc);
		bcb_msmnt_data.i_high_gain_rms = (diff_rms * 1000) / a;

		a = bcb_msmnt_data.config.v_mains_cal_a;
		bcb_msmnt_data.v_mains_diff_sqrd_acc =
			bcb_msmnt_data.v_mains_diff_sqrd_acc >> BCB_MSMNT_RMS_SAMPLES_SHIFT;
		diff_rms = (uint32_t)sqrtl((double)bcb_msmnt_data.v_mains_diff_sqrd_acc);
		bcb_msmnt_data.v_mains_rms = (diff_rms * 1000) / a;

		bcb_msmnt_data.rms_samples = 0;
		bcb_msmnt_data.i_low_gain_diff_sqrd_acc = 0;
		bcb_msmnt_data.i_high_gain_diff_sqrd_acc = 0;
		bcb_msmnt_data.v_mains_diff_sqrd_acc = 0;
	}
}

void bcb_msmnt_rms_start(uint8_t interval)
{
	bcb_msmnt_data.rms_samples = 0;
	bcb_msmnt_data.i_low_gain_diff_sqrd_acc = 0;
	bcb_msmnt_data.i_high_gain_diff_sqrd_acc = 0;
	bcb_msmnt_data.v_mains_diff_sqrd_acc = 0;
	bcb_msmnt_data.i_low_gain_rms = 0;
	bcb_msmnt_data.i_high_gain_rms = 0;
	bcb_msmnt_data.v_mains_rms = 0;
	k_timer_start(&bcb_msmnt_data.timer_rms, K_MSEC(interval), K_MSEC(interval));
}

void bcb_msmnt_rms_stop(void)
{
	k_timer_stop(&bcb_msmnt_data.timer_rms);
}

int32_t bcb_msmnt_get_current_low_gain(void)
{
	int32_t a = bcb_msmnt_data.config.i_low_gain_cal_a;
	int32_t b = bcb_msmnt_data.config.i_low_gain_cal_b;

	return ((((int32_t)*bcb_msmnt_data.raw_i_low_gain) - b) * 1000) / a;
}

int32_t bcb_msmnt_get_current_high_gain(void)
{
	int32_t a = bcb_msmnt_data.config.i_high_gain_cal_a;
	int32_t b = bcb_msmnt_data.config.i_high_gain_cal_b;

	return ((((int32_t)*bcb_msmnt_data.raw_i_high_gain) - b) * 1000) / a;
}

int32_t bcb_msmnt_get_voltage(void)
{
	int32_t a = bcb_msmnt_data.config.v_mains_cal_a;
	int32_t b = b = bcb_msmnt_data.config.v_mains_cal_b;

	return ((((int32_t)*bcb_msmnt_data.raw_v_mains) - b) * 1000) / a;
}

int32_t bcb_msmnt_get_current(void)
{
	if ((*bcb_msmnt_data.raw_i_high_gain < (UINT16_MAX - 100)) &&
	    (*bcb_msmnt_data.raw_i_high_gain > 100)) {
		return bcb_msmnt_get_current_high_gain();
	} else {
		/* high gain amplifer is about get saturated or already saturated. */
		return bcb_msmnt_get_current_low_gain();
	}
	return 0;
}

uint32_t bcb_msmnt_get_voltage_rms(void)
{
	return bcb_msmnt_data.v_mains_rms;
}

uint32_t bcb_msmnt_get_current_rms(void)
{
	if ((*bcb_msmnt_data.raw_i_low_gain < (UINT16_MAX - 100)) ||
	    (*bcb_msmnt_data.raw_i_low_gain > 100)) {
		return bcb_msmnt_data.i_low_gain_rms;
	} else {
		/* Low gain amplifer is about get saturated or already saturated. */
		return bcb_msmnt_data.i_high_gain_rms;
	}
}

static void bcb_msmnt_configuration_load_default(void)
{
	bcb_msmnt_data.config.i_low_gain_cal_a = 874U; /* (20*2*10^-3*2^16)/3 */
	bcb_msmnt_data.config.i_low_gain_cal_b = 32768U; /* Middle of the ADC code range */
	bcb_msmnt_data.config.i_high_gain_cal_a = 8738U; /* (200*2*10^-3*2^16)/3 */
	bcb_msmnt_data.config.i_high_gain_cal_b = 32768U; /* Middle of the ADC code range */
	bcb_msmnt_data.config.v_mains_cal_a = 79U; /* (20*2^16*360*2)/(3*(360*2+1*10^6*4)) */
	bcb_msmnt_data.config.v_mains_cal_b = 32768U; /* Middle of the ADC code range */
}

int bcb_msmnt_init(void)
{
	int r;

	memset(&bcb_msmnt_data, 0, sizeof(bcb_msmnt_data));
	k_timer_init(&bcb_msmnt_data.timer_rms, bcb_msmnt_on_rms_timer, NULL);

	bcb_msmnt_data.buffer_adc_0 = buffer_adc_0;
	bcb_msmnt_data.buffer_size_adc_0 = sizeof(buffer_adc_0);
	bcb_msmnt_data.buffer_adc_1 = buffer_adc_1;
	bcb_msmnt_data.buffer_size_adc_1 = sizeof(buffer_adc_1);

	bcb_msmnt_data.dev_adc_0 = device_get_binding(DT_LABEL(DT_NODELABEL(adc0)));
	if (bcb_msmnt_data.dev_adc_0 == NULL) {
		LOG_ERR("Could not get ADC device %s", DT_LABEL(DT_NODELABEL(adc0)));
		return -EINVAL;
	}

	bcb_msmnt_data.dev_adc_1 = device_get_binding(DT_LABEL(DT_NODELABEL(adc1)));
	if (bcb_msmnt_data.dev_adc_1 == NULL) {
		LOG_ERR("Could not get ADC device %s", DT_LABEL(DT_NODELABEL(adc1)));
		return -EINVAL;
	}

	if ((r = bcb_msmnt_config_load())) {
		LOG_WRN("Cannot load configurations: %d. Loading default", r);
		bcb_msmnt_configuration_load_default();
	}

	adc_dma_set_reference(bcb_msmnt_data.dev_adc_0, ADC_DMA_REF_EXTERNAL0);
	adc_dma_set_reference(bcb_msmnt_data.dev_adc_1, ADC_DMA_REF_EXTERNAL0);

	adc_dma_set_performance_level(bcb_msmnt_data.dev_adc_0, ADC_DMA_PERF_LEVEL_1);
	adc_dma_set_performance_level(bcb_msmnt_data.dev_adc_1, ADC_DMA_PERF_LEVEL_0);

	bcb_msmnt_start();

	return 0;
}

int bcb_msmnt_start(void)
{
	adc_dma_sequence_config_t adc_seq_cfg;

	bcb_msmnt_data.seq_len_adc_0 = 0;
	bcb_msmnt_data.seq_len_adc_1 = 0;

	adc_dma_stop(bcb_msmnt_data.dev_adc_0);
	BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, i_low_gain);
	BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, i_high_gain);
	BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, v_mains);

	adc_dma_stop(bcb_msmnt_data.dev_adc_1);
	BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, t_mosfet_in);
	BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, t_mosfet_out);
	BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, t_ambient);
	BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, t_mcu);
	BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, hw_rev_in);
	BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, hw_rev_out);
	BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, hw_rev_ctrl);
	BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, oc_test_adj);
	BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, ref_1v5);

	adc_seq_cfg.buffer = bcb_msmnt_data.buffer_adc_0;
	adc_seq_cfg.buffer_size = bcb_msmnt_data.buffer_size_adc_0;
	adc_seq_cfg.len = bcb_msmnt_data.seq_len_adc_0;
	adc_seq_cfg.samples = adc_seq_cfg.len;
	adc_seq_cfg.callback = NULL;
	adc_dma_read(bcb_msmnt_data.dev_adc_0, &adc_seq_cfg);

	adc_seq_cfg.buffer = bcb_msmnt_data.buffer_adc_1;
	adc_seq_cfg.buffer_size = bcb_msmnt_data.buffer_size_adc_1;
	adc_seq_cfg.len = bcb_msmnt_data.seq_len_adc_1;
	adc_seq_cfg.samples = adc_seq_cfg.len;
	adc_seq_cfg.callback = NULL;
	adc_dma_read(bcb_msmnt_data.dev_adc_1, &adc_seq_cfg);

	k_timer_start(&bcb_msmnt_data.timer_rms, K_MSEC(CONFIG_BCB_LIB_MSMNT_RMS_INTERVAL),
		      K_MSEC(CONFIG_BCB_LIB_MSMNT_RMS_INTERVAL));

	return 0;
}

int bcb_msmnt_stop(void)
{
	k_timer_stop(&bcb_msmnt_data.timer_rms);
	adc_dma_stop(bcb_msmnt_data.dev_adc_0);
	adc_dma_stop(bcb_msmnt_data.dev_adc_1);

	return 0;
}

int bcb_msmnt_config_load(void)
{
	int r;
	off_t buf_offset;
	size_t size_config;
	size_t size_adc_0;
	size_t size_adc_1;
	size_t size_total;
	uint8_t *buf;

	size_config = sizeof(bcb_msmnt_data.config);
	size_adc_0 = adc_dma_get_calibration_values_length(bcb_msmnt_data.dev_adc_0);
	size_adc_1 = adc_dma_get_calibration_values_length(bcb_msmnt_data.dev_adc_1);
	size_total = size_config + size_adc_0 + size_adc_1;

	buf = k_malloc(size_total);
	if (!buf) {
		return -ENOMEM;
	}

	r = bcb_config_load(CONFIG_BCB_LIB_PERSISTENT_CONFIG_OFFSET_MSMNT, buf, size_total);
	if (r) {
		LOG_ERR("configuration loading error: %d", r);
		goto cleanup;
	}

	buf_offset = 0;
	memcpy(&bcb_msmnt_data.config, buf + buf_offset, size_config);

	buf_offset += size_config;
	r = adc_dma_set_calibration_values(bcb_msmnt_data.dev_adc_0, buf + buf_offset, size_adc_0);
	if (r) {
		LOG_ERR("cannot set ADC0 configuration: %d", r);
		goto cleanup;
	}

	buf_offset += size_adc_0;
	r = adc_dma_set_calibration_values(bcb_msmnt_data.dev_adc_1, buf + buf_offset, size_adc_1);
	if (r) {
		LOG_ERR("cannot set ADC1 configuration: %d", r);
	}

cleanup:
	k_free(buf);

	return r;
}

int bcb_msmnt_config_store(void)
{
	int r;
	off_t buf_offset;
	size_t size_config;
	size_t size_adc_0;
	size_t size_adc_1;
	size_t size_total;
	uint8_t *buf;

	size_config = sizeof(bcb_msmnt_data.config);
	size_adc_0 = adc_dma_get_calibration_values_length(bcb_msmnt_data.dev_adc_0);
	size_adc_1 = adc_dma_get_calibration_values_length(bcb_msmnt_data.dev_adc_1);
	size_total = size_config + size_adc_0 + size_adc_1;

	buf = k_malloc(size_total);
	if (!buf) {
		return -ENOMEM;
	}

	buf_offset = 0;
	memcpy(buf + buf_offset, &bcb_msmnt_data.config, size_config);

	buf_offset += size_config;
	r = adc_dma_get_calibration_values(bcb_msmnt_data.dev_adc_0, buf + buf_offset, size_adc_0);
	if (r) {
		LOG_ERR("cannot get ADC0 configuration: %d", r);
		goto cleanup;
	}

	buf_offset += size_adc_0;
	r = adc_dma_get_calibration_values(bcb_msmnt_data.dev_adc_1, buf + buf_offset, size_adc_1);
	if (r) {
		LOG_ERR("cannot get ADC1 configuration: %d", r);
		goto cleanup;
	}

	r = bcb_config_store(CONFIG_BCB_LIB_PERSISTENT_CONFIG_OFFSET_MSMNT, buf, size_total);
	if (r) {
		LOG_ERR("configuration storing error: %d", r);
	}

cleanup:
	k_free(buf);

	return r;
}

/*
 * Current measurement calculation
 * ===============================
 *             ⎛                 N⎞                     
 *             ⎜G    ⋅ R      ⋅ 2 ⎟                     
 *             ⎜ amp    sense     ⎟                     
 *  ADC      = ⎜──────────────────⎟ ⋅ I      + ADC      
 *     total   ⎜     V            ⎟    sense      offset
 *             ⎝      ADC_ref     ⎠                     
 *          
 * Where:                                           
 * ADC         = Total ADC reading                     
 *    total
 * -------------
 * ADC         = ADC reading when the input current is zero                     
 *    offset    
 * -------------                                  
 * G           = Gain of the amplifier (20 or 200)                         
 *  amp                                       
 * -------------
 * R           = Value of sense resistor(s) (2 milliohms)        
 *  sense                                     
 * -------------
 * V           = ADC reference voltage (3 V)           
 *  ADC_ref
 * -------------
 * I           = Measured current
 *  sense
 *
 * Voltage measurement calculation
 * ===============================
 *            ⎛                 N⎞                     
 *            ⎜G    ⋅ R      ⋅ 2 ⎟                     
 *            ⎜ amp    sense     ⎟                     
 * ADC      = ⎜──────────────────⎟ ⋅ V      + ADC      
 *    total   ⎜ V        ⋅ R     ⎟    mains      offset
 *            ⎝  ADC_ref    total⎠                     
 *
 * Where:                                           
 * ADC         = Total ADC reading                     
 *    total
 * -------------
 * ADC         = ADC reading when the input voltage is zero                     
 *    offset    
 * -------------                                  
 * G           = Gain of the amplifier (20)                         
 *  amp                                       
 * -------------
 * R           = Value of sense resistor(s) (720 ohms)        
 *  sense
 * -------------
 * R           = Total value of resistor ladder (4000720 ohms)        
 *  total                                      
 * -------------
 * V           = ADC reference voltage (3 V)           
 *  ADC_ref
 * -------------
 * V           = Measured voltage
 *  mains  
 */
