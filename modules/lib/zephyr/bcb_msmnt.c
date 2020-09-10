#include <bcb_msmnt.h>
#include <adc_mcux_edma.h>
#include <device.h>
#include <devicetree.h>

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
            (ds)->val_##ch_name = &((ds)->buffer_adc_0[(ds)->seq_len_adc_0]);                       \
            adc_mcux_channel_setup((ds)->dev_adc_0, ((ds)->seq_len_adc_0++), &cfg);                 \
        } else if(dev == (ds)->dev_adc_1) {                                                         \
            (ds)->val_##ch_name = &((ds)->buffer_adc_1[(ds)->seq_len_adc_1]);                       \
            adc_mcux_channel_setup((ds)->dev_adc_1, ((ds)->seq_len_adc_0++), &cfg);                 \
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
    /* ADC channel values */
    volatile uint16_t*      val_i_low_gain;
    volatile uint16_t*      val_i_high_gain;
    volatile uint16_t*      val_v_mains;
};

static uint16_t buffer_adc_0[DT_PROP(DT_NODELABEL(adc0), max_channels)];
static uint16_t buffer_adc_1[DT_PROP(DT_NODELABEL(adc1), max_channels)];

static struct bcb_msmnt_data bcb_msmnt_data = {
    /* Initialise channel 0. */
    .dev_adc_0 = NULL,
    .seq_len_adc_0 = 0,
    .buffer_adc_0 = buffer_adc_0,
    .buffer_size_adc_0 = sizeof(buffer_adc_0),
    /* Initialise channel 1. */
    .dev_adc_1 = NULL,
    .seq_len_adc_1 = 0,
    .buffer_adc_0 = buffer_adc_1,
    .buffer_size_adc_0 = sizeof(buffer_adc_1),
    /* Initialise channel value pointers. */
    .val_i_low_gain = NULL,
    .val_i_high_gain = NULL,
    .val_v_mains = NULL,
};

static int bcb_msmnt_init()
{
    bcb_msmnt_data.dev_adc_0 = device_get_binding(DT_LABEL(DT_NODELABEL(adc0)));
    if (bcb_msmnt_data.dev_adc_0 == NULL) {
        LOG_ERR("Could not get ADC device 0"); 
        return -EINVAL;
    }

    bcb_msmnt_data.dev_adc_1 = device_get_binding(DT_LABEL(DT_NODELABEL(adc1)));
    if (bcb_msmnt_data.dev_adc_1 == NULL) {
        LOG_ERR("Could not get ADC device 0"); 
        return -EINVAL;
    }

    BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, i_low_gain);
    //BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, i_high_gain);
    BCB_MSMNT_ADC_SEQ_ADD(&bcb_msmnt_data, aread, v_mains);


    struct adc_mcux_sequence_config adc_seq_cfg;

    adc_mcux_set_sequence_len(bcb_msmnt_data.dev_adc_0, bcb_msmnt_data.seq_len_adc_0);
    adc_seq_cfg.interval_us = 1e6;
    adc_seq_cfg.buffer = bcb_msmnt_data.buffer_adc_0;
    adc_seq_cfg.buffer_size = bcb_msmnt_data.buffer_size_adc_0;
    adc_seq_cfg.reference = ADC_MCUX_REF_EXTERNAL;
    adc_mcux_read(bcb_msmnt_data.dev_adc_0, &adc_seq_cfg, false);

#if 0
    adc_mcux_set_sequence_len(bcb_msmnt_data.dev_adc_1, bcb_msmnt_data.seq_len_adc_1);
    adc_seq_cfg.interval_us = 1e6;
    adc_seq_cfg.buffer = bcb_msmnt_data.buffer_adc_1;
    adc_seq_cfg.buffer_size = bcb_msmnt_data.buffer_size_adc_1;
    adc_seq_cfg.reference = ADC_MCUX_REF_EXTERNAL;
    adc_mcux_read(bcb_msmnt_data.dev_adc_1, &adc_seq_cfg, false);
#endif

    return 0;
}

SYS_INIT(bcb_msmnt_init, APPLICATION, CONFIG_BCB_LIB_MSMNT_INIT_PRIORITY);