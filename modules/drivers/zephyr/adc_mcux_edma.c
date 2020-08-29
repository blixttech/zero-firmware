#define DT_DRV_COMPAT nxp_kinetis_adc16_edma

#include <adc_mcux_edma.h>
#include <fsl_common.h>
#include <fsl_adc16.h>
#include <fsl_dmamux.h>
#include <fsl_edma.h>

#define LOG_LEVEL CONFIG_INPUT_CAPTURE_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(adc_mcux_edma);

struct adc_mcux_config {
    ADC_Type* adc_base;
    FTM_Type* ftm_base;
    DMA_Type* dma_base;
    DMAMUX_Type* dma_mux_base;
    uint8_t max_channels;
    uint8_t dma_ch0;
    uint8_t dma_ch1;
    uint8_t dma_ch2;
};

struct adc_mcux_data {
    adc16_config_t adc_config;
    uint8_t* sc1n_lsb;
    uint8_t* cfg2_lsb;
    void* buffer;
    size_t buffer_size;
    uint8_t seq_last_idx;
};


static void configure_channel_mux_lsb(const struct adc_mcux_config* config, 
                                    struct adc_mcux_data* data)
{
    int i;
    uint8_t lsb_masked;
    for(i = 0; i < config->max_channels; i++) {
        lsb_masked = data->sc1n_lsb[i] & ADC_SC1_ADCH_MASK;
        data->sc1n_lsb[i] = lsb_masked | (uint8_t)config->adc_base->SC1[0];
        lsb_masked = data->cfg2_lsb[i] & ADC_CFG2_MUXSEL_MASK;
        data->cfg2_lsb[i] = lsb_masked | (uint8_t)config->adc_base->CFG2;
    }
}

static int adc_mcux_channel_setup_impl(struct device* dev, uint8_t seq_idx, 
                                    const struct adc_mcux_channel_config* ch_cfg)
{
    const struct adc_mcux_config* config = dev->config_info;
    struct adc_mcux_data* data = dev->driver_data;
    if (seq_idx > (config->max_channels - 1)) {
        LOG_ERR("Invalid sequence index");
        return -EINVAL;
    }
    /* Modify only the channel select field */
    uint8_t lsb_masked = data->sc1n_lsb[seq_idx] & ~ADC_SC1_ADCH_MASK;
    data->sc1n_lsb[seq_idx] = lsb_masked | (uint8_t)(ADC_SC1_ADCH(ch_cfg->channel));
    /* Modify only the alternate channel select field */
    lsb_masked = data->cfg2_lsb[seq_idx] & ~ADC_CFG2_MUXSEL_MASK;
    data->cfg2_lsb[seq_idx] = lsb_masked | (uint8_t)(ADC_CFG2_MUXSEL(ch_cfg->alt_channel));
    return 0;
}

static int adc_mcux_set_sequence_last_impl(struct device* dev, uint8_t seq_idx)
{
    const struct adc_mcux_config* config = dev->config_info;
    struct adc_mcux_data* data = dev->driver_data;
    if (seq_idx > (config->max_channels - 1)) {
        LOG_ERR("Invalid sequence index");
        return -EINVAL;
    }
    data->seq_last_idx = seq_idx;
    return 0;
}

static uint8_t adc_mcux_get_sequence_last_impl(struct device* dev)
{
    struct adc_mcux_data* data = dev->driver_data;
    return data->seq_last_idx;
}

static int adc_mcux_read_impl(struct device* dev, 
                        const struct adc_mcux_sequence_config* seq_cfg, bool single)
{
    const struct adc_mcux_config* config = dev->config_info;
    struct adc_mcux_data* data = dev->driver_data;

    uint8_t adc_ref;
    if (seq_cfg->reference == ADC_MCUX_REF_INTERNAL) {
        adc_ref = kADC16_ReferenceVoltageSourceValt;
    } else {
        adc_ref = kADC16_ReferenceVoltageSourceVref;
    }

    if (data->adc_config.referenceVoltageSource != adc_ref) {
        /* Reference has been changed. Reinitialise and do a recalibration  */
        data->adc_config.referenceVoltageSource = adc_ref;
        ADC16_Init(config->adc_base, &data->adc_config);
        ADC16_DoAutoCalibration(config->adc_base);
    }

    if (data->seq_last_idx == 0) {
        LOG_ERR("No channels to sample");
        return -EINVAL;
    }

    configure_channel_mux_lsb(config, data);

    return 0;
}

static int adc_mcux_init(struct device* dev)
{
    const struct adc_mcux_config* config = dev->config_info;
    struct adc_mcux_data* data = dev->driver_data;

    data->seq_last_idx = 0;
    data->buffer_size = 0;
    data->buffer = NULL;
    memset(data->sc1n_lsb, 0, config->max_channels);
    memset(data->cfg2_lsb, 0, config->max_channels);

    /* ---------------- ADC configuration ---------------- */
    /*
     * adc16ConfigStruct.referenceVoltageSource = kADC16_ReferenceVoltageSourceVref;
     * adc16ConfigStruct.clockSource = kADC16_ClockSourceAsynchronousClock;
     * adc16ConfigStruct.enableAsynchronousClock = true;
     * adc16ConfigStruct.clockDivider = kADC16_ClockDivider8;
     * adc16ConfigStruct.resolution = kADC16_ResolutionSE12Bit;
     * adc16ConfigStruct.longSampleMode = kADC16_LongSampleDisabled;
     * adc16ConfigStruct.enableHighSpeed = false;
     * adc16ConfigStruct.enableLowPower = false;
     * adc16ConfigStruct.enableContinuousConversion = false;
     */
    ADC16_GetDefaultConfig(&data->adc_config);
    data->adc_config.resolution = kADC16_Resolution16Bit;
    data->adc_config.referenceVoltageSource = kADC16_ReferenceVoltageSourceValt;
    ADC16_Init(config->adc_base, &data->adc_config);

    ADC16_DoAutoCalibration(config->adc_base);
    ADC16_EnableHardwareTrigger(config->adc_base, true);
    ADC16_EnableDMA(config->adc_base, true);

    adc16_channel_config_t adc_ch_cfg;
    adc_ch_cfg.channelNumber = 0; /* Set to channel zero by default. */
    adc_ch_cfg.enableInterruptOnConversionCompleted = false;
    adc_ch_cfg.enableDifferentialConversion = false;
    ADC16_SetChannelConfig(config->adc_base, 0, &adc_ch_cfg);

    configure_channel_mux_lsb(config, data);

    /* ---------------- FTM configuration ---------------- */

    /* ---------------- DMA configuration ---------------- */

    return 0;
}

static const struct adc_mcux_driver_api adc_mcux_driver_api = {
    .read = adc_mcux_read_impl,
    .channel_setup = adc_mcux_channel_setup_impl,
    .set_sequence_last = adc_mcux_set_sequence_last_impl,
    .get_sequence_last = adc_mcux_get_sequence_last_impl
};

#define ADC_MCUX_DEVICE(n)                                                      \
    static struct adc_mcux_config adc_mcux_config_##n = {                       \
        .adc_base = (ADC_Type *)DT_INST_REG_ADDR(n),                            \
        .ftm_base = (FTM_Type *)DT_REG_ADDR(DT_INST_PHANDLE(n, timer)),         \
        .dma_base = DMA0,                                                       \
        .dma_mux_base = DMAMUX0,                                                \
        .max_channels = DT_INST_PROP(n, max_channels),                          \
        .dma_ch0 = DT_INST_PROP_BY_IDX(n, dma_channels, 0),                     \
        .dma_ch1 = DT_INST_PROP_BY_IDX(n, dma_channels, 1),                     \
        .dma_ch2 = DT_INST_PROP_BY_IDX(n, dma_channels, 2),                     \
    };                                                                          \
                                                                                \
    static uint8_t adc_mcux_sc1n_lsb_##n[DT_INST_PROP(n, max_channels)];        \
    static uint8_t adc_mcux_cfg2_lsb_mux_##n[DT_INST_PROP(n, max_channels)];    \
    static struct adc_mcux_data adc_mcux_data_##n = {                           \
        .sc1n_lsb = &adc_mcux_sc1n_lsb_##n[0],                                  \
        .cfg2_lsb = &adc_mcux_cfg2_lsb_mux_##n[0],                              \
        .buffer = NULL,                                                         \
        .buffer_size = 0,                                                       \
        .seq_last_idx = 0,                                                      \
    };                                                                          \
                                                                                \
    DEVICE_AND_API_INIT(adc_mcux_##n,                                           \
                        DT_INST_LABEL(n),                                       \
                        &adc_mcux_init,                                         \
                        &adc_mcux_data_##n,                                     \
                        &adc_mcux_config_##n,                                   \
                        POST_KERNEL,                                            \
                        CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                     \
                        &adc_mcux_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ADC_MCUX_DEVICE)