#include <bcb_msmnt.h>
#include <adc_mcux_edma.h>
#include <device.h>
#include <devicetree.h>
#include <arm_math.h>
#include <math.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_msmnt);


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
    /* ADC0 */
    struct device           *dev_adc_0;
    uint8_t                 seq_len_adc_0;
    volatile uint16_t       *buffer_adc_0;
    size_t                  buffer_size_adc_0;
    /* ADC1 */
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
};

typedef struct bcb_msmnt_ntc_tbl {
    uint32_t r; /* Resistance in milliohm */
    uint32_t b;
} bcb_msmnt_ntc_tbl_t;

static uint16_t buffer_adc_0[DT_PROP(DT_NODELABEL(adc0), max_channels)] __attribute__ ((aligned (2)));
static uint16_t buffer_adc_1[DT_PROP(DT_NODELABEL(adc1), max_channels)] __attribute__ ((aligned (2)));

static struct bcb_msmnt_data bcb_msmnt_data = {
    /* ADC0. */
    .dev_adc_0 = NULL,
    .seq_len_adc_0 = 0,
    .buffer_adc_0 = buffer_adc_0,
    .buffer_size_adc_0 = sizeof(buffer_adc_0),
    /* ADC1. */
    .dev_adc_1 = NULL,
    .seq_len_adc_1 = 0,
    .buffer_adc_1 = buffer_adc_1,
    .buffer_size_adc_1 = sizeof(buffer_adc_1),
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

static int32_t get_temp_adc(uint32_t adc_ntc) 
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
int32_t bcb_get_temp(bcb_temp_t sensor)
{
    switch (sensor) {
        case BCB_TEMP_PWR_IN:
            return (int32_t)get_temp_adc(*(bcb_msmnt_data.raw_t_mosfet_in));
        case BCB_TEMP_PWR_OUT:
            return (int32_t)get_temp_adc(*(bcb_msmnt_data.raw_t_mosfet_out));
        case BCB_TEMP_AMB:
            return (int32_t)get_temp_adc(*(bcb_msmnt_data.raw_t_ambient));
        case BCB_TEMP_MCU:
            return (int32_t)get_temp_mcu(*(bcb_msmnt_data.raw_t_mcu));
        default:
            LOG_ERR("Invalid sensor");
    }
    return 0;
}

static int bcb_msmnt_init()
{
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


    /*
    // ADCs are calibrated by the driver when initialised.
    adc_mcux_set_reference(bcb_msmnt_data.dev_adc_0, ADC_MCUX_REF_EXTERNAL);
    adc_mcux_set_perf_level(bcb_msmnt_data.dev_adc_0, ADC_MCUX_PERF_LVL_0);
    adc_mcux_calibrate(bcb_msmnt_data.dev_adc_0);

    adc_mcux_set_reference(bcb_msmnt_data.dev_adc_1, ADC_MCUX_REF_EXTERNAL);
    adc_mcux_set_perf_level(bcb_msmnt_data.dev_adc_1, ADC_MCUX_PERF_LVL_0);
    adc_mcux_calibrate(bcb_msmnt_data.dev_adc_1);
    */

    adc_mcux_sequence_config_t adc_seq_cfg;

    adc_seq_cfg.buffer = bcb_msmnt_data.buffer_adc_0;
    adc_seq_cfg.buffer_size = bcb_msmnt_data.buffer_size_adc_0;
    adc_seq_cfg.seq_len = bcb_msmnt_data.seq_len_adc_0;
    adc_seq_cfg.seq_samples = 1;
    adc_mcux_read(bcb_msmnt_data.dev_adc_0, &adc_seq_cfg);

    adc_seq_cfg.buffer = bcb_msmnt_data.buffer_adc_1;
    adc_seq_cfg.buffer_size = bcb_msmnt_data.buffer_size_adc_1;
    adc_seq_cfg.seq_len = bcb_msmnt_data.seq_len_adc_1;
    adc_seq_cfg.seq_samples = 1;
    adc_mcux_read(bcb_msmnt_data.dev_adc_1, &adc_seq_cfg);

    return 0;
}


SYS_INIT(bcb_msmnt_init, APPLICATION, CONFIG_BCB_LIB_MSMNT_INIT_PRIORITY);