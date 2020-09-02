#include <bcb_msmnt.h>
#include <adc_mcux_edma.h>
#include <device.h>
#include <devicetree.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_msmnt);

struct bcb_msmnt_data {
    struct device*  adc_0;
    struct adc_mcux_sequence_config adc_0_cfg;
    uint8_t adc_0_seq_idx;
    uint16_t adc_0_buffer[3];
};

static struct bcb_msmnt_data bcb_msmnt_data;

static int bcb_msmnt_init()
{
    bcb_msmnt_data.adc_0 = device_get_binding(DT_LABEL(DT_NODELABEL(adc0)));
    if (bcb_msmnt_data.adc_0 == NULL) {
        LOG_ERR("Could not get ADC device"); 
        return -EINVAL;
    }
    bcb_msmnt_data.adc_0_seq_idx = 0;

    struct adc_mcux_channel_config ch_cfg;

    ch_cfg.channel = DT_PHA_BY_NAME(DT_NODELABEL(aread), io_channels, i_low_gain, input);
    ch_cfg.alt_channel = DT_PHA_BY_NAME(DT_NODELABEL(aread), io_channels, i_low_gain, muxsel);
    adc_mcux_channel_setup(bcb_msmnt_data.adc_0, (bcb_msmnt_data.adc_0_seq_idx++), &ch_cfg);

    ch_cfg.channel = DT_PHA_BY_NAME(DT_NODELABEL(aread), io_channels, i_high_gain, input);
    ch_cfg.alt_channel = DT_PHA_BY_NAME(DT_NODELABEL(aread), io_channels, i_high_gain, muxsel);
    adc_mcux_channel_setup(bcb_msmnt_data.adc_0, (bcb_msmnt_data.adc_0_seq_idx++), &ch_cfg);

    adc_mcux_set_sequence_len(bcb_msmnt_data.adc_0, bcb_msmnt_data.adc_0_seq_idx);

    bcb_msmnt_data.adc_0_cfg.interval_us = 1e6;
    bcb_msmnt_data.adc_0_cfg.buffer = &bcb_msmnt_data.adc_0_buffer;
    bcb_msmnt_data.adc_0_cfg.buffer_size = sizeof(bcb_msmnt_data.adc_0_buffer);
    bcb_msmnt_data.adc_0_cfg.reference = ADC_MCUX_REF_EXTERNAL;

    adc_mcux_read(bcb_msmnt_data.adc_0, &bcb_msmnt_data.adc_0_cfg, false);
    

    return 0;
}

SYS_INIT(bcb_msmnt_init, APPLICATION, CONFIG_BCB_LIB_MSMNT_INIT_PRIORITY);