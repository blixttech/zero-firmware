#include <bcb_msmnt.h>
#include <adc_mcux_edma.h>
#include <device.h>
#include <devicetree.h>
#include <arm_math.h>
#include <math.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_msmnt);

#define BCB_MSMNT_RMS_SAMPLES           ((1U) << CONFIG_BCB_LIB_MSMNT_RMS_SAMPLES)
#define BCB_MSMNT_RMS_SAMPLES_SHIFT     (CONFIG_BCB_LIB_MSMNT_RMS_SAMPLES) 

#define BCB_MSMNT_CAL_SAMPLES           ((1U) << 10U)
#define BCB_MSMNT_CAL_SAMPLES_SHIFT     (10U)


#define BCB_MSMNT_ADC_SEQ_ADD(ds, dt_node, ch_name)                                                 \
    do {                                                                                            \
        struct adc_mcux_channel_config cfg;                                                         \
        cfg.channel = DT_PHA_BY_NAME(DT_NODELABEL(dt_node), io_channels, ch_name, input);           \
        cfg.alt_channel = DT_PHA_BY_NAME(DT_NODELABEL(dt_node), io_channels, ch_name, muxsel);      \
        struct device* dev = device_get_binding(                                                    \
                        DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(dt_node), io_channels, ch_name))); \
        if (dev == (ds)->dev_adc_0) {                                                               \
            (ds)->raw_##ch_name = &((ds)->buffer_adc_0[(ds)->seq_len_adc_0]);                       \
            adc_mcux_channel_setup((ds)->dev_adc_0, ((ds)->seq_len_adc_0++), &cfg);                 \
        } else if(dev == (ds)->dev_adc_1) {                                                         \
            (ds)->raw_##ch_name = &((ds)->buffer_adc_1[(ds)->seq_len_adc_1]);                       \
            adc_mcux_channel_setup((ds)->dev_adc_1, ((ds)->seq_len_adc_1++), &cfg);                 \
        } else {                                                                                    \
            LOG_ERR("Unknown ADC device");                                                          \
        }                                                                                           \
    } while (0)

struct bcb_msmnt_data {
    /* ADC channel 0 */
    struct device           *dev_adc_0;
    uint8_t                 seq_len_adc_0;
    volatile uint16_t       *buffer_adc_0;
    size_t                  buffer_size_adc_0;
    /* ADC channel 1 */
    struct device           *dev_adc_1;
    uint8_t                 seq_len_adc_1;
    volatile uint16_t       *buffer_adc_1;
    size_t                  buffer_size_adc_1;
    /* ADC0 channel values */
    volatile uint16_t       *raw_i_low_gain;
    volatile uint16_t       *raw_i_high_gain;
    volatile uint16_t       *raw_v_mains;
    /* ADC1 channel values */
    volatile uint16_t       *raw_t_mosfet_in;
    volatile uint16_t       *raw_t_mosfet_out;
    volatile uint16_t       *raw_t_ambient;
    volatile uint16_t       *raw_t_mcu;
    volatile uint16_t       *raw_hw_rev_in;
    volatile uint16_t       *raw_hw_rev_out;
    volatile uint16_t       *raw_hw_rev_ctrl;
    volatile uint16_t       *raw_oc_test_adj;
    volatile uint16_t       *raw_ref_1v5;
    /* offset errors */
    uint16_t                i_low_gain_raw_zero;
    uint16_t                i_high_gain_raw_zero;
    uint16_t                v_mains_raw_zero;

    uint32_t                diff_i_low_gain_2_acc;
    uint32_t                diff_i_high_gain_2_acc;
    uint32_t                diff_v_mains_2_acc;
    uint8_t                 n_samples_rms;

    uint32_t                diff_i_low_gain_2_acc_last;
    uint32_t                diff_i_high_gain_2_acc_last;
    uint32_t                diff_v_mains_2_acc_last;

    uint16_t                diff_i_low_gain_rms;
    uint16_t                diff_i_high_gain_rms;
    uint16_t                diff_v_mains_rms;

    volatile uint16_t                cal_n_samples;
    volatile uint32_t                cal_i_lg_acc;
    volatile uint32_t                cal_i_hg_acc;
    volatile uint32_t                cal_v_acc;

    struct k_timer timer_stat;
    struct k_timer timer_rms;
    struct k_timer timer_cal;
#if CONFIG_BCB_LIB_ADC_DUMP_ENABLE
    struct k_timer timer_adc_dump;
#endif // CONFIG_BCB_LIB_ADC_DUMP_ENABLE
};

typedef struct bcb_msmnt_ntc_tbl {
    uint32_t r; /* Resistance in milliohm */
    uint32_t b;
} bcb_msmnt_ntc_tbl_t;

static uint16_t buffer_adc_0[DT_PROP(DT_NODELABEL(adc0), max_channels)] __attribute__ ((aligned (2)));
static uint16_t buffer_adc_1[DT_PROP(DT_NODELABEL(adc1), max_channels)] __attribute__ ((aligned (2)));

static struct bcb_msmnt_data bcb_msmnt_data = {
    /* Initialise channel 0. */
    .dev_adc_0 = NULL,
    .seq_len_adc_0 = 0,
    .buffer_adc_0 = buffer_adc_0,
    .buffer_size_adc_0 = sizeof(buffer_adc_0),
    /* Initialise channel 1. */
    .dev_adc_1 = NULL,
    .seq_len_adc_1 = 0,
    .buffer_adc_1 = buffer_adc_1,
    .buffer_size_adc_1 = sizeof(buffer_adc_1),
    /* offset errors */
    .i_low_gain_raw_zero = 0,
    .i_high_gain_raw_zero = 0,
    .v_mains_raw_zero = 0,
    /* RMS calculation */
    .diff_i_low_gain_2_acc = 0,
    .diff_i_high_gain_2_acc = 0,
    .diff_v_mains_2_acc = 0,
    .n_samples_rms = 0,
    .diff_i_low_gain_rms = 0,
    .diff_i_high_gain_rms = 0,
    .diff_v_mains_rms = 0,
    /* Calibration */
    .cal_n_samples = 0,
};

static bcb_msmnt_ntc_tbl_t bcb_msmnt_ntc_tbl_data[] = {
    {1135000U,   4075U}, /* -20C */
    {355600U,    4133U}, /* 0C */
    {127000U,    4185U}, /* 20C */
    {100000U,    4197U}, /* 25C */
    {50680U,     4230U}, /* 40C */
    {22220U,     4269U}, /* 60C */
    {10580U,     4301U}, /* 80C */
    {5410U,      4327U}, /* 100C */
};

static int32_t get_temp(uint32_t adc_ntc) 
{
    /* ADC is referenced to 3V (3000 millivolt). */
    uint32_t v_ntc = (3000 * adc_ntc) >> 16;
    /* NTC is connected to 3V3 (3300 millivolt) rail via a 56k (56000 milliohm) resistor. */
    uint32_t r_tnc = v_ntc * 56000 / (3300 - v_ntc);
    uint32_t b_ntc = 4197U;
    int i;
    /* Find the best value for beta (B) */
    for (i = 0; i < sizeof(bcb_msmnt_ntc_tbl_data)/sizeof(bcb_msmnt_ntc_tbl_t); i++) {
        if (r_tnc > bcb_msmnt_ntc_tbl_data[i].r) {
            b_ntc = bcb_msmnt_ntc_tbl_data[i].b;
            break;
        }
    }
    /* 
     * T = (T_0 * B)/(B + (T_0 * ln(R/R_0)))
     * ln(R/R_0) = ln(R) - ln(R_0)
     *
     * T_0 = 298.15 K (25C)
     * R_0 = 100000 ohm
     * ln(R_0) = 11.5130
     */
    float temp_k = (298.15f * ((float)b_ntc));
    temp_k /= ((float)b_ntc) + (298.15f * (logf(r_tnc) - 11.5130f));
    /* Convert the value back to celsius and into integer */
    return (int32_t)((temp_k - 273.15f) * 100.0f); 
}

static int32_t get_temp_mcu(uint32_t adc_ntc)
{
    uint32_t v_ntc = (3000U * adc_ntc) >> 16;
    return (int32_t)((25.0f - (((float)v_ntc - 716.0f) / 1.62f)) * 100.0f);
}

int32_t bcb_msmnt_get_current_l()
{
  /**
   * Low gain amplification G = 20.
   * Current shunt resistance R_SHUNT = 2 mOhm.
   * ADC is 16-bit and reference voltage V_REF = 3 V
   * So current I = ADC_DIFF * (V_REF/2^16) / (R_SHUNT * G)
   * I (in mA) = ADC_DIFF * 3 * 10^5 / 2^18
   */
    int16_t adc_diff = (int16_t)*(bcb_msmnt_data.raw_i_low_gain)
                        - (int16_t)(bcb_msmnt_data.i_low_gain_raw_zero); 
                        //- (int64_t)*(bcb_msmnt_data.ref_1v5_raw);
    return (int32_t)(((int64_t)adc_diff * 300000LL) >> 18ULL);
}

int32_t bcb_msmnt_get_current_h()
{
  /**
   * Low gain amplification G = 200.
   * Current shunt resistance R_SHUNT = 2 mOhm.
   * ADC is 16-bit and reference voltage V_REF = 3 V
   * So current I = ADC_DIFF * (V_REF/2^16) / (R_SHUNT * G)
   * I (in mA) = ADC_DIFF * 3 * 10^4 / 2^18
   */
    int64_t adc_diff = (int64_t)*(bcb_msmnt_data.raw_i_high_gain)
                        - (int64_t)(bcb_msmnt_data.i_high_gain_raw_zero); 
                        //- (int64_t)*(bcb_msmnt_data.ref_1v5_raw);
    return (int32_t)((adc_diff * 30000) >> 18);
}

int32_t bcb_msmnt_get_voltage()
{
#if 0
    /* TODO: Explain how the calculation is done. */
    int64_t adc_diff = (int64_t)*(bcb_msmnt_data.v_mains_raw)
                        - (int64_t)(bcb_msmnt_data.v_mains_raw_zero);
                        //- (int64_t)*(bcb_msmnt_data.ref_1v5_raw);
    return (int32_t)(((adc_diff * 3 * 2000360) / 360) >> 16);
#else
    int64_t adc_diff = (int64_t)*(bcb_msmnt_data.raw_v_mains)
                        - (int64_t)(bcb_msmnt_data.v_mains_raw_zero);
    return (int32_t)(((adc_diff * 3000 * 4000710) / 20 / 710) >> 16);
#endif

    
}

int32_t bcb_msmnt_get_current_l_rms()
{
  /**
   * Low gain amplification G = 20.
   * Current shunt resistance R_SHUNT = 2 mOhm.
   * ADC is 16-bit and reference voltage V_REF = 3 V
   * So current I = ADC_DIFF * (V_REF/2^16) / (R_SHUNT * G)
   * I (in mA) = ADC_DIFF * 3 * 10^5 / 2^18
   */
    return (int32_t)(((uint64_t)bcb_msmnt_data.diff_i_low_gain_rms * 300000ULL) >> 18ULL);
}

int32_t bcb_msmnt_get_current_h_rms()
{
  /**
   * Low gain amplification G = 200.
   * Current shunt resistance R_SHUNT = 2 mOhm.
   * ADC is 16-bit and reference voltage V_REF = 3 V
   * So current I = ADC_DIFF * (V_REF/2^16) / (R_SHUNT * G)
   * I (in mA) = ADC_DIFF * 3 * 10^4 / 2^18
   */
    //int64_t adc_diff = (int64_t)bcb_msmnt_data.diff_i_high_gain_rms
    //                    - (int64_t)(bcb_msmnt_data.i_high_gain_raw_zero); 
                        //- (int64_t)*(bcb_msmnt_data.ref_1v5_raw);
    //return (int32_t)((adc_diff * 30000) >> 18);

    return (int32_t)(((uint64_t)bcb_msmnt_data.diff_i_high_gain_rms * 30000ULL) >> 18ULL);
}

int32_t bcb_msmnt_get_voltage_rms()
{
    /* TODO: Explain how the calculation is done. */
    int64_t adc_diff = (int64_t)bcb_msmnt_data.diff_v_mains_rms
                        //- (int64_t)(bcb_msmnt_data.v_mains_raw_zero); 
                        - (int64_t)*(bcb_msmnt_data.raw_ref_1v5);
    return (int32_t)(((adc_diff * 3 * 2000360) / 360) >> 16);
}

void on_stat_timer_expired(struct k_timer* timer)
{

#if 0
    LOG_DBG("T_MOS_IN: %" PRId32 ", T_MOS_OUT %" PRId32 ", T_AMB %" PRId32 " T_MCU %" PRId32, 
        bcb_msmnt_get_temp(BCB_MSMNT_TEMP_PWR_IN),
        bcb_msmnt_get_temp(BCB_MSMNT_TEMP_PWR_OUT),
        bcb_msmnt_get_temp(BCB_MSMNT_TEMP_AMB),
        bcb_msmnt_get_temp(BCB_MSMNT_TEMP_MCU));
#endif

#if 0
    LOG_DBG("current: l: %06" PRId32 "[%06" PRId32 "], h: %06" PRId32 "[%06" PRId32 "], v %06" PRId32 "[%06" PRId32 "]", 
        bcb_msmnt_get_current_l(),
        bcb_msmnt_get_current_l_rms(), 
        bcb_msmnt_get_current_h(),
        bcb_msmnt_get_current_h_rms(),
        bcb_msmnt_get_voltage(),
        bcb_msmnt_get_voltage_rms());
#endif 

#if 0
    LOG_DBG("ref_1v5: %" PRIu16, *(bcb_msmnt_data.ref_1v5_raw));

    int16_t adc_diff;
    adc_diff = (int16_t)((int32_t)*(bcb_msmnt_data.i_low_gain_raw)
                //- (int32_t)(bcb_msmnt_data.i_low_gain_raw_zero)); 
                - (int32_t)*(bcb_msmnt_data.ref_1v5_raw));
    LOG_DBG("adc_diff c_lg: %" PRId16, adc_diff);

    adc_diff = (int16_t)((int32_t)*(bcb_msmnt_data.i_high_gain_raw)
                //- (int32_t)(bcb_msmnt_data.i_high_gain_raw_zero)); 
                - (int32_t)*(bcb_msmnt_data.ref_1v5_raw));
    LOG_DBG("adc_diff c_hg: %" PRId16, adc_diff);

    adc_diff = (int16_t)((int32_t)*(bcb_msmnt_data.v_mains_raw)
                //- (int32_t)(bcb_msmnt_data.v_mains_raw_zero)); 
                - (int32_t)*(bcb_msmnt_data.ref_1v5_raw));
    LOG_DBG("adc_diff v: %" PRId16, adc_diff);

#endif 

#if 1
    LOG_DBG("%" PRIu16 ", %" PRIu16 ", %" PRIu16, 
        *bcb_msmnt_data.raw_v_mains,
        *bcb_msmnt_data.raw_i_low_gain,
        *bcb_msmnt_data.raw_i_high_gain);
#endif
}

static void on_rms_timer_expired(struct k_timer* timer)
{

#if 0
    int32_t adc_diff = (int32_t)*(bcb_msmnt_data.raw_i_low_gain)
                    - (int32_t)(bcb_msmnt_data.i_low_gain_raw_zero);
                    //- (int32_t)*(bcb_msmnt_data.ref_1v5_raw);

    bcb_msmnt_data.diff_i_low_gain_2_acc += (uint32_t)(adc_diff * adc_diff);

    adc_diff = (int32_t)*(bcb_msmnt_data.raw_i_high_gain)
                    - (int32_t)(bcb_msmnt_data.i_high_gain_raw_zero);
                    //- (int32_t)*(bcb_msmnt_data.ref_1v5_raw);
    bcb_msmnt_data.diff_i_high_gain_2_acc += (uint32_t)(adc_diff * adc_diff);

    adc_diff = (int32_t)*(bcb_msmnt_data.raw_v_mains)
                    //- (int32_t)(bcb_msmnt_data.v_mains_raw_zero);
                    - (int32_t)*(bcb_msmnt_data.raw_ref_1v5);
    bcb_msmnt_data.diff_v_mains_2_acc += (uint32_t)(adc_diff * adc_diff);

    bcb_msmnt_data.n_samples_rms++;

    if (bcb_msmnt_data.n_samples_rms == BCB_MSMNT_RMS_SAMPLES) {

        bcb_msmnt_data.diff_i_low_gain_2_acc = bcb_msmnt_data.diff_i_low_gain_2_acc >> BCB_MSMNT_RMS_SAMPLES_SHIFT;
        bcb_msmnt_data.diff_i_high_gain_2_acc = bcb_msmnt_data.diff_i_high_gain_2_acc >> BCB_MSMNT_RMS_SAMPLES_SHIFT;
        bcb_msmnt_data.diff_v_mains_2_acc = bcb_msmnt_data.diff_v_mains_2_acc >> BCB_MSMNT_RMS_SAMPLES_SHIFT;
        
        bcb_msmnt_data.diff_i_low_gain_rms = (uint16_t)sqrtf((float)bcb_msmnt_data.diff_i_low_gain_2_acc);
        bcb_msmnt_data.diff_i_high_gain_rms = (uint16_t)sqrtf((float)bcb_msmnt_data.diff_i_high_gain_2_acc);
        bcb_msmnt_data.diff_v_mains_rms = (uint16_t)sqrtf((float)bcb_msmnt_data.diff_v_mains_2_acc);

        bcb_msmnt_data.n_samples_rms = 0;
        bcb_msmnt_data.diff_i_low_gain_2_acc = 0;
        bcb_msmnt_data.diff_i_high_gain_2_acc = 0;
        bcb_msmnt_data.diff_v_mains_2_acc = 0;
    }
#endif
}

static void on_cal_timer_expired(struct k_timer* timer)
{
#if 0
    if (bcb_msmnt_data.cal_n_samples > BCB_MSMNT_CAL_SAMPLES) {
        bcb_msmnt_data.i_high_gain_raw_zero = (uint16_t)(bcb_msmnt_data.cal_i_hg_acc >> BCB_MSMNT_CAL_SAMPLES_SHIFT);
        bcb_msmnt_data.i_low_gain_raw_zero = (uint16_t)(bcb_msmnt_data.cal_i_lg_acc >> BCB_MSMNT_CAL_SAMPLES_SHIFT);
        bcb_msmnt_data.v_mains_raw_zero = (uint16_t)(bcb_msmnt_data.cal_v_acc >> BCB_MSMNT_CAL_SAMPLES_SHIFT);
        LOG_DBG("cal done. i_lg %" PRId16 ", i_hg %" PRIu16 ", v %" PRIu16, 
                bcb_msmnt_data.i_low_gain_raw_zero, 
                bcb_msmnt_data.i_high_gain_raw_zero, 
                bcb_msmnt_data.v_mains_raw_zero);
        k_timer_stop(timer);
        k_timer_start(&bcb_msmnt_data.timer_stat, K_MSEC(10), K_MSEC(10));
    }

    bcb_msmnt_data.cal_i_hg_acc += (uint32_t)*(bcb_msmnt_data.raw_i_high_gain);
    bcb_msmnt_data.cal_i_lg_acc += (uint32_t)*(bcb_msmnt_data.raw_i_low_gain);
    bcb_msmnt_data.cal_v_acc += (uint32_t)*(bcb_msmnt_data.raw_v_mains);

    bcb_msmnt_data.cal_n_samples++;
#endif
}

static inline void update_adc_zero()
{
    bcb_msmnt_data.i_low_gain_raw_zero = *(bcb_msmnt_data.raw_i_low_gain);
    bcb_msmnt_data.i_high_gain_raw_zero = *(bcb_msmnt_data.raw_i_high_gain);
    bcb_msmnt_data.v_mains_raw_zero = *(bcb_msmnt_data.raw_v_mains);

    LOG_DBG("low_gain_zero %" PRIu16, bcb_msmnt_data.i_low_gain_raw_zero);
    LOG_DBG("high_gain_zero %" PRIu16, bcb_msmnt_data.i_high_gain_raw_zero);
    LOG_DBG("v_mains_zero %" PRIu16, bcb_msmnt_data.v_mains_raw_zero);
}

int32_t bcb_msmnt_get_temp(bcb_msmnt_temp_t sensor)
{
    switch (sensor) {
        case BCB_MSMNT_TEMP_PWR_IN:
            return (int32_t)get_temp(*(bcb_msmnt_data.raw_t_mosfet_in));
        case BCB_MSMNT_TEMP_PWR_OUT:
            return (int32_t)get_temp(*(bcb_msmnt_data.raw_t_mosfet_out));
        case BCB_MSMNT_TEMP_AMB:
            return (int32_t)get_temp(*(bcb_msmnt_data.raw_t_ambient));
        case BCB_MSMNT_TEMP_MCU:
            return (int32_t)get_temp_mcu(*(bcb_msmnt_data.raw_t_mcu));
        default:
            LOG_ERR("Invalid sensor");
    }
    return 0;
}

uint16_t bcb_msmnt_get_temp_raw(bcb_msmnt_temp_t sensor)
{
    switch (sensor) {
        case BCB_MSMNT_TEMP_PWR_IN:
            return *(bcb_msmnt_data.raw_t_mosfet_in);
        case BCB_MSMNT_TEMP_PWR_OUT:
            return *(bcb_msmnt_data.raw_t_mosfet_out);
        case BCB_MSMNT_TEMP_AMB:
            return *(bcb_msmnt_data.raw_t_ambient);
        case BCB_MSMNT_TEMP_MCU:
            return *(bcb_msmnt_data.raw_t_mcu);
        default:
            LOG_ERR("Invalid sensor");
    }
    return 0;
}

int bcb_msmnt_cal_start(bcb_msmnt_cal_callback_t callback)
{
#if 0
    bcb_msmnt_data.cal_n_samples = 0;
    bcb_msmnt_data.cal_i_hg_acc = 0;
    bcb_msmnt_data.cal_i_lg_acc = 0;
    bcb_msmnt_data.cal_v_acc = 0;

    k_timer_stop(&bcb_msmnt_data.timer_stat);
    k_timer_start(&bcb_msmnt_data.timer_cal, K_MSEC(10), K_MSEC(2));
#endif

    return 0;
}

int bcb_msmnt_cal_stop()
{
    return 0;
}

#if CONFIG_BCB_LIB_ADC_DUMP_ENABLE

typedef enum adc_dump_state {
    ADC_DUMP_STATE_IDLE,
    ADC_DUMP_STATE_SAMPLING,
    ADC_DUMP_STATE_DUMPING
} adc_dump_state_t;

static uint16_t raw_adc_dump_v_mains[CONFIG_BCB_LIB_ADC_DUMP_SAMPLES] __attribute__ ((aligned (2)));
static uint16_t raw_adc_dump_i_low_gain[CONFIG_BCB_LIB_ADC_DUMP_SAMPLES] __attribute__ ((aligned (2)));
static uint16_t raw_adc_dump_i_high_gain[CONFIG_BCB_LIB_ADC_DUMP_SAMPLES] __attribute__ ((aligned (2)));
static uint32_t adc_dump_sample_count;
static uint32_t adc_dump_dump_count;
static uint8_t adc_dump_state;

static void on_adc_dump_timer_expired(struct k_timer* timer)
{
    switch (adc_dump_state) {
        case ADC_DUMP_STATE_IDLE: {
            k_timer_stop(timer);
            break;
        }
        case ADC_DUMP_STATE_SAMPLING: {
            if (adc_dump_sample_count >= CONFIG_BCB_LIB_ADC_DUMP_SAMPLES) {
                k_timer_stop(timer);
                LOG_DBG("Sampling completed");
            } else {
                raw_adc_dump_v_mains[adc_dump_sample_count] = *bcb_msmnt_data.raw_v_mains;
                raw_adc_dump_i_low_gain[adc_dump_sample_count] = *bcb_msmnt_data.raw_i_low_gain;
                raw_adc_dump_i_high_gain[adc_dump_sample_count] = *bcb_msmnt_data.raw_i_high_gain;
                adc_dump_sample_count++;
            }
            break;
        }
        case ADC_DUMP_STATE_DUMPING: {
            if (adc_dump_dump_count >= adc_dump_sample_count) {
                k_timer_stop(timer);
                adc_dump_state = ADC_DUMP_STATE_IDLE;
                LOG_DBG("Dumpling completed");
            } else {
                if (adc_dump_dump_count == 0) {
                    LOG_DBG("#idx, v_mains, i_low_gain, i_high_gain");
                }
                LOG_DBG("%" PRIu32 ", %" PRIu16 ", %" PRIu16 ", %" PRIu16 "",
                    adc_dump_dump_count, 
                    raw_adc_dump_v_mains[adc_dump_dump_count],
                    raw_adc_dump_i_low_gain[adc_dump_dump_count],
                    raw_adc_dump_i_high_gain[adc_dump_dump_count]);
                adc_dump_dump_count++;
            }
            break;
        }
        default: {

        }
    }
}

int bcb_msmnt_adc_dump_sstart(bcb_msmnt_adc_dump_callback_t callback)
{
    k_timer_stop(&bcb_msmnt_data.timer_adc_dump);
    adc_dump_sample_count = 0;
    adc_dump_state = ADC_DUMP_STATE_SAMPLING;
    LOG_DBG("Start sampling");
    uint32_t i;
    for (i = 0; i < CONFIG_BCB_LIB_ADC_DUMP_SAMPLES; i++) {
        raw_adc_dump_v_mains[adc_dump_sample_count] = *bcb_msmnt_data.raw_v_mains;
        raw_adc_dump_i_low_gain[adc_dump_sample_count] = *bcb_msmnt_data.raw_i_low_gain;
        raw_adc_dump_i_high_gain[adc_dump_sample_count] = *bcb_msmnt_data.raw_i_high_gain;
        adc_dump_sample_count++;
        k_busy_wait(40);   
    }
    LOG_DBG("Sampling completed");
    //k_timer_start(&bcb_msmnt_data.timer_adc_dump, K_USEC(100), K_USEC(100));
    return 0;
}

int bcb_msmnt_adc_dump_dstart(bcb_msmnt_adc_dump_callback_t callback)
{
    k_timer_stop(&bcb_msmnt_data.timer_adc_dump);
    adc_dump_state = ADC_DUMP_STATE_DUMPING;
    adc_dump_dump_count = 0;
    LOG_DBG("Start dumping");
    k_timer_start(&bcb_msmnt_data.timer_adc_dump, K_MSEC(10), K_MSEC(10));
    return 0;
}

int bcb_msmnt_adc_dump_stop()
{
    k_timer_stop(&bcb_msmnt_data.timer_adc_dump);
    adc_dump_state = ADC_DUMP_STATE_IDLE;
    return 0;
}

#endif // CONFIG_BCB_LIB_ADC_DUMP_ENABLE

static int bcb_msmnt_init()
{
#if 1
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

    /* ADC0 */
    BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, i_low_gain);
    BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, i_high_gain);
    BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, v_mains);
    /* ADC1 */
    BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, t_mosfet_in);
    BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, t_mosfet_out);
    BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, t_ambient);
    BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, t_mcu);
    BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, hw_rev_in);
    BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, hw_rev_out);
    BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, hw_rev_ctrl);
    BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, oc_test_adj);
    BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, ref_1v5);


    adc_mcux_set_reference(bcb_msmnt_data.dev_adc_0, ADC_MCUX_REF_EXTERNAL);
    adc_mcux_set_perf_level(bcb_msmnt_data.dev_adc_0, ADC_MCUX_PERF_LVL_0);
    adc_mcux_calibrate(bcb_msmnt_data.dev_adc_0);

    adc_mcux_set_reference(bcb_msmnt_data.dev_adc_1, ADC_MCUX_REF_EXTERNAL);
    adc_mcux_set_perf_level(bcb_msmnt_data.dev_adc_1, ADC_MCUX_PERF_LVL_0);
    adc_mcux_calibrate(bcb_msmnt_data.dev_adc_1);

    k_timer_init(&bcb_msmnt_data.timer_cal, on_cal_timer_expired, NULL);
    k_timer_init(&bcb_msmnt_data.timer_rms, on_rms_timer_expired, NULL);
    k_timer_init(&bcb_msmnt_data.timer_stat, on_stat_timer_expired, NULL);
#if CONFIG_BCB_LIB_ADC_DUMP_ENABLE
    k_timer_init(&bcb_msmnt_data.timer_adc_dump, on_adc_dump_timer_expired, NULL);
#endif // CONFIG_BCB_LIB_ADC_DUMP_ENABLE


    adc_mcux_set_perf_level(bcb_msmnt_data.dev_adc_0, ADC_MCUX_PERF_LVL_3);

    struct adc_mcux_sequence_config adc_seq_cfg;

    adc_seq_cfg.interval_us = 10;
    adc_seq_cfg.buffer = bcb_msmnt_data.buffer_adc_0;
    adc_seq_cfg.buffer_size = bcb_msmnt_data.buffer_size_adc_0;
    adc_seq_cfg.seq_len = bcb_msmnt_data.seq_len_adc_0;
    adc_mcux_read(bcb_msmnt_data.dev_adc_0, &adc_seq_cfg);

    adc_seq_cfg.interval_us = 1000;
    adc_seq_cfg.buffer = bcb_msmnt_data.buffer_adc_1;
    adc_seq_cfg.buffer_size = bcb_msmnt_data.buffer_size_adc_1;
    adc_seq_cfg.seq_len = bcb_msmnt_data.seq_len_adc_1;
    //adc_mcux_read(bcb_msmnt_data.dev_adc_1, &adc_seq_cfg);

    /* Wait for 10ms until ADC conversions started to measure offset errors. */
    //k_msleep(10);
    //update_adc_zero();


    /* Start RMS calculation timer. */
    //k_timer_start(&bcb_msmnt_data.timer_rms, K_MSEC(1), K_MSEC(1));

    /* Starting stat timer. */
    //k_timer_start(&bcb_msmnt_data.timer_stat, K_SECONDS(1), K_SECONDS(1));

#endif

    return 0;
}


SYS_INIT(bcb_msmnt_init, APPLICATION, CONFIG_BCB_LIB_MSMNT_INIT_PRIORITY);