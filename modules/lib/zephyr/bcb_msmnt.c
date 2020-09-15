#include <bcb_msmnt.h>
#include <adc_mcux_edma.h>
#include <device.h>
#include <devicetree.h>
#include <arm_math.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_msmnt);

#define BCB_MSMNT_RMS_SAMPLES           ((1U) << CONFIG_BCB_LIB_MSMNT_RMS_SAMPLES)
#define BCB_MSMNT_RMS_SAMPLES_SHIFT     (CONFIG_BCB_LIB_MSMNT_RMS_SAMPLES) 

#define BCB_MSMNT_ADC_SEQ_ADD(ds, dt_node, ch_name)                                                 \
    do {                                                                                            \
        struct adc_mcux_channel_config cfg;                                                         \
        cfg.channel = DT_PHA_BY_NAME(DT_NODELABEL(dt_node), io_channels, ch_name, input);           \
        cfg.alt_channel = DT_PHA_BY_NAME(DT_NODELABEL(dt_node), io_channels, ch_name, muxsel);      \
        struct device* dev = device_get_binding(                                                    \
                        DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(dt_node), io_channels, ch_name))); \
        if (dev == (ds)->dev_adc_0) {                                                               \
            (ds)->val_##ch_name = &((ds)->buffer_adc_0[(ds)->seq_len_adc_0]);                       \
            adc_mcux_channel_setup((ds)->dev_adc_0, ((ds)->seq_len_adc_0++), &cfg);                 \
        } else if(dev == (ds)->dev_adc_1) {                                                         \
            (ds)->val_##ch_name = &((ds)->buffer_adc_1[(ds)->seq_len_adc_1]);                       \
            adc_mcux_channel_setup((ds)->dev_adc_1, ((ds)->seq_len_adc_1++), &cfg);                 \
        } else {                                                                                    \
            LOG_ERR("Unknown ADC device");                                                          \
        }                                                                                           \
    } while (0)

struct bcb_msmnt_data {
    /* ADC channel 0 */
    struct device*          dev_adc_0;
    uint8_t                 seq_len_adc_0;
    volatile uint16_t*       buffer_adc_0;
    size_t                  buffer_size_adc_0;
    /* ADC channel 1 */
    struct device*          dev_adc_1;
    uint8_t                 seq_len_adc_1;
    volatile uint16_t*      buffer_adc_1;
    size_t                  buffer_size_adc_1;
    /* ADC0 channel values */
    volatile uint16_t*      val_i_low_gain;
    volatile uint16_t*      val_i_high_gain;
    volatile uint16_t*      val_v_mains;
    /* ADC1 channel values */
    volatile uint16_t*      val_t_mosfet_in;
    volatile uint16_t*      val_t_mosfet_out;
    volatile uint16_t*      val_t_ambient;
    volatile uint16_t*      val_t_mcu;
    volatile uint16_t*      val_hw_rev_in;
    volatile uint16_t*      val_hw_rev_out;
    volatile uint16_t*      val_hw_rev_ctrl;
    volatile uint16_t*      val_oc_test_adj;
    volatile uint16_t*      val_ref_1v5;
    /* offset errors */
    uint16_t                val_i_low_gain_zero;
    uint16_t                val_i_high_gain_zero;
    uint16_t                val_v_mains_zero;

    uint32_t                val_i_low_gain_acc;
    uint32_t                val_i_high_gain_acc;
    uint32_t                val_v_mains_acc;
    uint8_t                 n_samples_rms;

    uint16_t                val_i_low_gain_rms;
    uint16_t                val_i_high_gain_rms;
    uint16_t                val_v_mains_rms;

    struct k_timer timer_stat;
    struct k_timer timer_rms;
};

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
    .val_i_low_gain_zero = 0,
    .val_i_high_gain_zero = 0,
    .val_v_mains_zero = 0,
    /* RMS calculation */
    .val_i_low_gain_acc = 0,
    .val_i_high_gain_acc = 0,
    .val_v_mains_acc = 0,
    .n_samples_rms = 0,
    .val_i_low_gain_rms = 0,
    .val_i_high_gain_rms = 0,
    .val_v_mains_rms = 0,
};

static float get_temp(uint32_t adc_ntc) 
{
    uint32_t v_ntc = (3000U * adc_ntc) >> 16;
    uint32_t r_tnc = v_ntc * 56000U / (3300 - v_ntc);

    float temp_k = (298.15f * 4308.0f);
    temp_k /= 4308.0f + (298.15f * (log((float)r_tnc) - log((100000.0f))));
    return (temp_k - 273.15f) * 100.0f; 
}

static float get_temp_mcu(uint32_t adc_ntc)
{
    uint32_t v_ntc = (3000U * adc_ntc) >> 16;
    return (25.0f - (((float)v_ntc - 716.0f) / 1.62f)) * 100.0f;
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
    int64_t adc_diff = (int64_t)*(bcb_msmnt_data.val_i_low_gain)
                        //- (int64_t)(bcb_msmnt_data.val_i_low_gain_zero); 
                        - (int64_t)*(bcb_msmnt_data.val_ref_1v5);
    return (int32_t)((adc_diff * 300000) >> 18);
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
    int64_t adc_diff = (int64_t)*(bcb_msmnt_data.val_i_high_gain)
                        //- (int64_t)(bcb_msmnt_data.val_i_high_gain_zero); 
                        - (int64_t)*(bcb_msmnt_data.val_ref_1v5);
    return (int32_t)((adc_diff * 30000) >> 18);
}

int32_t bcb_msmnt_get_voltage()
{
    /* TODO: Explain how the calculation is done. */
    int64_t adc_diff = (int64_t)*(bcb_msmnt_data.val_v_mains)
                        //- (int64_t)(bcb_msmnt_data.val_v_mains_zero)
                        - (int64_t)*(bcb_msmnt_data.val_ref_1v5);
    return (int32_t)(((adc_diff * 3 * 2000360) / 360) >> 16);
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
    int64_t adc_diff = (int64_t)bcb_msmnt_data.val_i_low_gain_rms
                        //- (int64_t)(bcb_msmnt_data.val_i_low_gain_zero); 
                        - (int64_t)*(bcb_msmnt_data.val_ref_1v5);
    return (int32_t)((adc_diff * 300000) >> 18);
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
    int64_t adc_diff = (int64_t)bcb_msmnt_data.val_i_high_gain_rms
                        //- (int64_t)(bcb_msmnt_data.val_i_high_gain_zero); 
                        - (int64_t)*(bcb_msmnt_data.val_ref_1v5);
    return (int32_t)((adc_diff * 30000) >> 18);
}

int32_t bcb_msmnt_get_voltage_rms()
{
    /* TODO: Explain how the calculation is done. */
    int64_t adc_diff = (int64_t)bcb_msmnt_data.val_v_mains_rms
                        //- (int64_t)(bcb_msmnt_data.val_v_mains_zero); 
                        - (int64_t)*(bcb_msmnt_data.val_ref_1v5);
    return (int32_t)(((adc_diff * 3 * 2000360) / 360) >> 16);
}

#if 0
static int32_t get_v_bias(uint32_t adc_voltage)
{
    return (3000 * (int32_t)adc_voltage) >> 16;
}
#endif

void on_stat_timer_expired(struct k_timer* timer)
{

#if 0
    LOG_DBG("T_MOS_IN: %" PRId32 ", T_MOS_OUT %" PRId32 ", T_AMB %" PRId32 " T_MCU %" PRId32, 
        bcb_msmnt_get_temp(BCB_MSMNT_TEMP_PWR_IN),
        bcb_msmnt_get_temp(BCB_MSMNT_TEMP_PWR_OUT),
        bcb_msmnt_get_temp(BCB_MSMNT_TEMP_AMB),
        bcb_msmnt_get_temp(BCB_MSMNT_TEMP_MCU));
#endif

    LOG_DBG("current: h: %06" PRId32 "[%06" PRId32 "], l: %06" PRId32 "[%06" PRId32 "]", 
        bcb_msmnt_get_current_l(),
        bcb_msmnt_get_current_l_rms(), 
        bcb_msmnt_get_current_h(),
        bcb_msmnt_get_current_h_rms()); 
}

static void on_rms_timer_expired(struct k_timer* timer)
{

    int32_t adc_diff = (int32_t)*(bcb_msmnt_data.val_i_low_gain)
                    - (int32_t)(bcb_msmnt_data.val_i_low_gain_zero);
                    //- (int32_t)*(bcb_msmnt_data.val_ref_1v5);
    bcb_msmnt_data.val_i_low_gain_acc += (uint32_t)(adc_diff * adc_diff);

    adc_diff = (int32_t)*(bcb_msmnt_data.val_i_high_gain)
                    - (int32_t)(bcb_msmnt_data.val_i_high_gain_zero);
                    //- (int32_t)*(bcb_msmnt_data.val_ref_1v5);
    bcb_msmnt_data.val_i_high_gain_acc += (uint32_t)(adc_diff * adc_diff);

    adc_diff = (int32_t)*(bcb_msmnt_data.val_v_mains)
                    - (int32_t)(bcb_msmnt_data.val_v_mains_zero);
                    //- (int32_t)*(bcb_msmnt_data.val_ref_1v5);
    bcb_msmnt_data.val_v_mains_acc += (uint32_t)(adc_diff * adc_diff);

    bcb_msmnt_data.n_samples_rms++;

    if (bcb_msmnt_data.n_samples_rms == BCB_MSMNT_RMS_SAMPLES) {

        bcb_msmnt_data.val_i_low_gain_acc = bcb_msmnt_data.val_i_low_gain_acc >> BCB_MSMNT_RMS_SAMPLES_SHIFT;
        bcb_msmnt_data.val_i_high_gain_acc = bcb_msmnt_data.val_i_high_gain_acc >> BCB_MSMNT_RMS_SAMPLES_SHIFT;
        bcb_msmnt_data.val_v_mains_acc = bcb_msmnt_data.val_v_mains_acc >> BCB_MSMNT_RMS_SAMPLES_SHIFT;
        
        int32_t adc_sqrt;
        arm_sqrt_q31(bcb_msmnt_data.val_i_low_gain_acc, &adc_sqrt);
        bcb_msmnt_data.val_i_low_gain_rms = (uint16_t)adc_sqrt;
        arm_sqrt_q31(bcb_msmnt_data.val_i_high_gain_acc, &adc_sqrt);
        bcb_msmnt_data.val_i_low_gain_rms = (uint16_t)adc_sqrt;
        arm_sqrt_q31(bcb_msmnt_data.val_v_mains_acc, &adc_sqrt);
        bcb_msmnt_data.val_v_mains_rms = (uint16_t)adc_sqrt;

        bcb_msmnt_data.n_samples_rms = 0;
        bcb_msmnt_data.val_i_low_gain_acc = 0;
        bcb_msmnt_data.val_i_high_gain_acc = 0;
        bcb_msmnt_data.val_v_mains_acc = 0;
    }
}

static inline void update_adc_zero()
{
    bcb_msmnt_data.val_i_low_gain_zero = *(bcb_msmnt_data.val_i_low_gain);
    bcb_msmnt_data.val_i_high_gain_zero = *(bcb_msmnt_data.val_i_high_gain);
    bcb_msmnt_data.val_v_mains_zero = *(bcb_msmnt_data.val_v_mains);
}

int32_t bcb_msmnt_get_temp(bcb_msmnt_temp_t sensor)
{
    switch (sensor) {
        case BCB_MSMNT_TEMP_PWR_IN:
            return (int32_t)get_temp(*(bcb_msmnt_data.val_t_mosfet_in));
        case BCB_MSMNT_TEMP_PWR_OUT:
            return (int32_t)get_temp(*(bcb_msmnt_data.val_t_mosfet_out));
        case BCB_MSMNT_TEMP_AMB:
            return (int32_t)get_temp(*(bcb_msmnt_data.val_t_ambient));
        case BCB_MSMNT_TEMP_MCU:
            return (int32_t)get_temp_mcu(*(bcb_msmnt_data.val_t_mcu));
        default:
            LOG_ERR("Invalid sensor");
    }
    return 0;
}

static int bcb_msmnt_init()
{
    bcb_msmnt_data.dev_adc_0 = device_get_binding(DT_LABEL(DT_NODELABEL(adc0)));
    if (bcb_msmnt_data.dev_adc_0 == NULL) {
        LOG_ERR("Could not get ADC device 0"); 
        return -EINVAL;
    }

    bcb_msmnt_data.dev_adc_1 = device_get_binding(DT_LABEL(DT_NODELABEL(adc1)));
    if (bcb_msmnt_data.dev_adc_1 == NULL) {
        LOG_ERR("Could not get ADC device 1"); 
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

    struct adc_mcux_sequence_config adc_seq_cfg;

    adc_mcux_set_sequence_len(bcb_msmnt_data.dev_adc_0, bcb_msmnt_data.seq_len_adc_0);
    adc_seq_cfg.interval_us = 50;
    adc_seq_cfg.buffer = bcb_msmnt_data.buffer_adc_0;
    adc_seq_cfg.buffer_size = bcb_msmnt_data.buffer_size_adc_0;
    adc_seq_cfg.reference = ADC_MCUX_REF_EXTERNAL;
    adc_mcux_read(bcb_msmnt_data.dev_adc_0, &adc_seq_cfg, false);

    adc_mcux_set_sequence_len(bcb_msmnt_data.dev_adc_1, bcb_msmnt_data.seq_len_adc_1);
    adc_seq_cfg.interval_us = 125;
    adc_seq_cfg.buffer = bcb_msmnt_data.buffer_adc_1;
    adc_seq_cfg.buffer_size = bcb_msmnt_data.buffer_size_adc_1;
    adc_seq_cfg.reference = ADC_MCUX_REF_EXTERNAL;
    adc_mcux_read(bcb_msmnt_data.dev_adc_1, &adc_seq_cfg, false);

    /* Wait for 1ms until ADC conversions started to measure offset errors. */
    k_usleep(1000);
    update_adc_zero();

    /* Start RMS calculation timer. */
    k_timer_init(&bcb_msmnt_data.timer_rms, on_rms_timer_expired, NULL);
    k_timer_start(&bcb_msmnt_data.timer_rms, K_MSEC(1), K_MSEC(1));

    /* Starting stat timer. */
    k_timer_init(&bcb_msmnt_data.timer_stat, on_stat_timer_expired, NULL);
    k_timer_start(&bcb_msmnt_data.timer_stat, K_SECONDS(1), K_SECONDS(1));

    return 0;
}


SYS_INIT(bcb_msmnt_init, APPLICATION, CONFIG_BCB_LIB_MSMNT_INIT_PRIORITY);