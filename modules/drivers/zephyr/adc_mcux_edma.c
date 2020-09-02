#define DT_DRV_COMPAT nxp_kinetis_adc16_edma


#include <adc_mcux_edma.h>
#include <fsl_common.h>
#include <fsl_adc16.h>
#include <fsl_dmamux.h>
#include <fsl_edma.h>
#include <fsl_ftm.h>
#include <fsl_clock.h>
#include <drivers/clock_control.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(adc_mcux_edma);

#define FTM_MAX_CHANNELS    ARRAY_SIZE(FTM0->CONTROLS)

struct adc_mcux_config {
    ADC_Type* adc_base;
    FTM_Type* ftm_base;
    DMA_Type* dma_base;
    DMAMUX_Type* dma_mux_base;
    ftm_clock_source_t ftm_clock_source;
    ftm_clock_prescale_t ftm_prescale;
    uint8_t ftm_channel;
    uint8_t max_channels;       /* Maximum number of ADC channels supported by the driver */
    uint8_t dma_ch_result;      /* DMA channel used for transfering ADC result */
    uint8_t dma_ch_ch;          /* DMA channel used for changing ADC main channel */
    uint8_t dma_ch_alt_ch;      /* DMA channel used for changing ADC alternate channel */
    uint8_t dma_src_result;     /* DMA request source used for transfering ADC result */
    uint8_t dma_src_ch;         /* DMA request source used for changing ADC main channel */
    uint8_t dma_src_alt_ch;     /* DMA request source used for changing ADC alternate channel */
};

struct adc_mcux_data {
    uint32_t ftm_ticks_per_sec;
    edma_handle_t dma_h_result;
    edma_handle_t dma_h_ch;
    edma_handle_t dma_h_alt_ch;
    adc16_config_t adc_config;
    uint8_t* sc1n_lsb;
    uint8_t* cfg2_lsb;
    void* buffer;
    size_t buffer_size;
    uint8_t seq_len;
};

static void adc_mcux_edma_dma_irq_handler(void *arg)
{
    edma_handle_t* handle = (edma_handle_t*)arg;
    EDMA_HandleIRQ(handle);
/* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
 * exception return operation might vector to incorrect interrupt 
 */
#if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
#endif
}

static void adc_mcux_edma_adc_irq_handler(void *arg)
{
    struct device* dev = (struct device *)arg;
    const struct adc_mcux_config* config = dev->config_info;

    uint16_t adc_val = ADC16_GetChannelConversionValue(config->adc_base, 0);
    LOG_DBG("ADC irq val %" PRIu16, adc_val);
/* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
 * exception return operation might vector to incorrect interrupt 
 */
#if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
#endif
}


static void adc_mcux_edma_callback(edma_handle_t* handle, void* param, 
                                    bool transfer_done, uint32_t tcds)
{
    LOG_DBG("DMA ch %" PRIu8 ", done: %" PRIu8, handle->channel, transfer_done);
    if (transfer_done) {
        EDMA_StartTransfer(handle);
    }
}

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
    /* Modify only the channel selection field */
    uint8_t lsb_masked = data->sc1n_lsb[seq_idx] & ~ADC_SC1_ADCH_MASK;
    data->sc1n_lsb[seq_idx] = lsb_masked | (uint8_t)(ADC_SC1_ADCH(ch_cfg->channel));
    /* Modify only the alternate channel selection field */
    lsb_masked = data->cfg2_lsb[seq_idx] & ~ADC_CFG2_MUXSEL_MASK;
    data->cfg2_lsb[seq_idx] = lsb_masked | (uint8_t)(ADC_CFG2_MUXSEL(ch_cfg->alt_channel));
    return 0;
}

static int adc_mcux_set_sequence_len_impl(struct device* dev, uint8_t seq_len)
{
    const struct adc_mcux_config* config = dev->config_info;
    struct adc_mcux_data* data = dev->driver_data;
    if (seq_len > config->max_channels) {
        LOG_ERR("Invalid sequence index");
        return -EINVAL;
    }
    data->seq_len = seq_len;
    return 0;
}

static uint8_t adc_mcux_get_sequence_len_impl(struct device* dev)
{
    struct adc_mcux_data* data = dev->driver_data;
    return data->seq_len;
}

static int adc_mcux_read_impl(struct device* dev, 
                        const struct adc_mcux_sequence_config* seq_cfg, bool single)
{
    const struct adc_mcux_config* config = dev->config_info;
    struct adc_mcux_data* data = dev->driver_data;

    /* Stop the ADC trigger timer before changes.  */
    FTM_StopTimer(config->ftm_base);
    /* TODO: Not sure if we need to abort any ongoing DMA transfers as well. */
    //EDMA_AbortTransfer(&data->dma_h_result);
    //EDMA_AbortTransfer(&data->dma_h_ch);
    //EDMA_AbortTransfer(&data->dma_h_alt_ch);

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

        adc16_channel_config_t adc_ch_cfg;
        adc_ch_cfg.channelNumber = 0; /* Set to channel zero by default. */
        adc_ch_cfg.enableDifferentialConversion = false;
        adc_ch_cfg.enableInterruptOnConversionCompleted = false;
        ADC16_SetChannelConfig(config->adc_base, 0, &adc_ch_cfg);

        ADC16_EnableHardwareTrigger(config->adc_base, false);
        ADC16_EnableDMA(config->adc_base, false);

        ADC16_DoAutoCalibration(config->adc_base);

        //adc_ch_cfg.enableInterruptOnConversionCompleted = true;
        //ADC16_SetChannelConfig(config->adc_base, 0, &adc_ch_cfg);

        ADC16_EnableHardwareTrigger(config->adc_base, true);
        ADC16_EnableDMA(config->adc_base, true);
    }

    if (data->seq_len == 0) {
        LOG_ERR("No channels to sample");
        return -EINVAL;
    }

    LOG_DBG("ADC seq len: %" PRIu8 "", data->seq_len);

    if (seq_cfg->buffer_size < data->seq_len * sizeof(uint16_t)) {
        LOG_ERR("Not enough memory to store samples");
        return -EINVAL;        
    }

    LOG_DBG("ADC buffer size: %" PRIu8 "", seq_cfg->buffer_size);


#if 1
    configure_channel_mux_lsb(config, data);

    edma_transfer_config_t tran_cfg_result;
    EDMA_PrepareTransfer(&tran_cfg_result, 
                        (uint32_t*)(config->adc_base->R),   /* Source Address (ADCx_RA) */
                        sizeof(uint16_t),                   /* Source width (2 bytes) */
                        data->buffer,                       /* Destination buffer */
                        sizeof(uint16_t),                   /* Destination width (2 bytes) */
                        sizeof(uint16_t),                   /* Bytes to transfer each minor loop (2 bytes) */
                        data->seq_len * sizeof(uint16_t),   /* Total of bytes to transfer (12*2 bytes) */
                        kEDMA_PeripheralToMemory);          /* From ADC to Memory */
    EDMA_SubmitTransfer(&data->dma_h_result, &tran_cfg_result);

    edma_transfer_config_t tran_cfg_ch;
    EDMA_PrepareTransfer(&tran_cfg_ch, 
                        data->sc1n_lsb,                     /* Source Address */
                        sizeof(uint8_t),                    /* Source width (1 byte) */
                        (uint32_t*)(config->adc_base->SC1), /* Destination buffer */
                        sizeof(uint8_t),                    /* Destination width (1 byte) */
                        sizeof(uint8_t),                    /* Bytes to transfer each minor loop (1 byte) */
                        data->seq_len * sizeof(uint8_t),    /* Total of bytes to transfer (data->seq_len bytes) */
                        kEDMA_MemoryToPeripheral);          /* From ADC channels array to ADCH register */
    EDMA_SubmitTransfer(&data->dma_h_ch, &tran_cfg_ch);

    edma_transfer_config_t tran_cfg_alt_ch;
    EDMA_PrepareTransfer(&tran_cfg_alt_ch, 
                        data->cfg2_lsb,                      /* Source Address */
                        sizeof(uint8_t),                     /* Source width (1 byte) */
                        (uint32_t*)(config->adc_base->CFG2), /* Destination buffer */
                        sizeof(uint8_t),                     /* Destination width (1 byte) */
                        sizeof(uint8_t),                     /* Bytes to transfer each minor loop (1 byte) */
                        data->seq_len * sizeof(uint8_t),     /* Total of bytes to transfer (data->seq_len bytes) */
                        kEDMA_MemoryToPeripheral);           /* From ADC channels array to MUXSEL register */
    EDMA_SubmitTransfer(&data->dma_h_alt_ch, &tran_cfg_alt_ch);


    /* Adjust the offset to be added to the last source/destination addresses after 
     * the major loop ends.  
     */
    config->dma_base->TCD[config->dma_ch_ch].SLAST = -1 * data->seq_len;
    config->dma_base->TCD[config->dma_ch_alt_ch].SLAST = -1 * data->seq_len;
    config->dma_base->TCD[config->dma_ch_result].DLAST_SGA = -1 * sizeof(uint16_t) * data->seq_len;

    /* Start transferring the ADC channel configuration of the first channel in the sequence.  */
    EDMA_StartTransfer(&data->dma_h_result);
#endif

#if 1
    uint32_t period = (((uint64_t)seq_cfg->interval_us * (uint64_t)data->ftm_ticks_per_sec) / (uint64_t)1e6);
    LOG_DBG("FTM period: %" PRIu32 "", period);
    FTM_SetTimerPeriod(config->ftm_base, period);
    FTM_StartTimer(config->ftm_base, config->ftm_clock_source);
#endif
    return 0;
}

static int adc_mcux_init(struct device* dev)
{
    const struct adc_mcux_config* config = dev->config_info;
    struct adc_mcux_data* data = dev->driver_data;

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

    clock_name_t clock_name;
    if (config->ftm_clock_source == kFTM_SystemClock) {
        clock_name = kCLOCK_CoreSysClk;
    } else if (config->ftm_clock_source == kFTM_FixedClock) {
        clock_name = kCLOCK_McgFixedFreqClk;
    } else {
        LOG_ERR("Not supported FTM clock");
        return -EINVAL;
    }
    data->ftm_ticks_per_sec = CLOCK_GetFreq(clock_name) >> config->ftm_prescale;

    LOG_DBG("FTM ticks/s: %" PRIu32 "", data->ftm_ticks_per_sec);

    if (config->ftm_channel > FTM_MAX_CHANNELS) {
        LOG_ERR("Invalid number of FTM channels");
        return -EINVAL;
    }

    ftm_config_t ftm_config;
    FTM_GetDefaultConfig(&ftm_config);

    ftm_config.prescale = config->ftm_prescale;
    /* FTM trigger generation is only for the specified channel. */
    ftm_config.extTriggers = (1U << config->ftm_channel);

    FTM_Init(config->ftm_base, &ftm_config);
    /* Keep the channel compare value as zero and use the timer period to
     * adjust the time between consecutive triggers.
     */
    FTM_SetupOutputCompare(config->ftm_base, config->ftm_channel, kFTM_NoOutputSignal, 0);

#if 1
    /* ---------------- DMA configuration ---------------- */
    DMAMUX_SetSource(config->dma_mux_base, config->dma_ch_result, config->dma_src_result);
    DMAMUX_EnableChannel(config->dma_mux_base, config->dma_ch_result);
    DMAMUX_SetSource(config->dma_mux_base, config->dma_ch_ch, config->dma_src_ch);
    DMAMUX_EnableChannel(config->dma_mux_base, config->dma_ch_ch);
    DMAMUX_SetSource(config->dma_mux_base, config->dma_ch_alt_ch, config->dma_src_alt_ch);
    DMAMUX_EnableChannel(config->dma_mux_base, config->dma_ch_alt_ch);

    EDMA_CreateHandle(&data->dma_h_result, config->dma_base, config->dma_ch_result);
    EDMA_SetCallback(&data->dma_h_result, adc_mcux_edma_callback, dev);

    EDMA_CreateHandle(&data->dma_h_ch, config->dma_base, config->dma_ch_ch);
    EDMA_SetCallback(&data->dma_h_ch, adc_mcux_edma_callback, dev);

    EDMA_CreateHandle(&data->dma_h_alt_ch, config->dma_base, config->dma_ch_alt_ch);
    EDMA_SetCallback(&data->dma_h_alt_ch, adc_mcux_edma_callback, dev);
    /*
     * After the "dma_ch_result" DMA channel has transferred ADC, "data dma_ch_ch" DMA channel
     * changes the main ADC channel.
     * After the "dma_ch_ch" DMA channel has changed the main ADC channel 
     * "dma_ch_alt_ch" DMA channel changes the alternate ADC channel.
     */   
    EDMA_SetChannelLink(config->dma_base, config->dma_ch_result, kEDMA_MinorLink, config->dma_ch_ch);
    //EDMA_SetChannelLink(config->dma_base, config->dma_ch_ch, kEDMA_MinorLink, config->dma_ch_alt_ch);
    /* Link major loops since the final minor loop are not covered by the minor loop linking above. */
    EDMA_SetChannelLink(config->dma_base, config->dma_ch_result, kEDMA_MajorLink, config->dma_ch_ch);
    //EDMA_SetChannelLink(config->dma_base, config->dma_ch_ch, kEDMA_MajorLink, config->dma_ch_alt_ch);
    /* Enable interrupts only when the ADC conversion result of the last in the sequence 
     * has been transferred into the buffer. 
     */

    EDMA_EnableChannelInterrupts(config->dma_base, config->dma_ch_result, kEDMA_MajorInterruptEnable);
    EDMA_EnableChannelInterrupts(config->dma_base, config->dma_ch_ch, kEDMA_MajorInterruptEnable);
    EDMA_EnableChannelInterrupts(config->dma_base, config->dma_ch_alt_ch, kEDMA_MajorInterruptEnable);
#endif

    return 0;
}

static const struct adc_mcux_driver_api adc_mcux_driver_api = {
    .read = adc_mcux_read_impl,
    .channel_setup = adc_mcux_channel_setup_impl,
    .set_sequence_len = adc_mcux_set_sequence_len_impl,
    .get_sequence_len = adc_mcux_get_sequence_len_impl,
};


#define _DT_IRQ_BY_IDX_(node_id, idx, cell)  DT_IRQ_BY_IDX(node_id, idx, cell)
#define ADC_MUCX_DMA_IRQ(adc_inst, dma_ch)                                      \
    _DT_IRQ_BY_IDX_(DT_INST_PHANDLE_BY_NAME(adc_inst, dmas, dma_ch),            \
                    DT_INST_PHA_BY_NAME(adc_inst, dmas, dma_ch, channel),       \
                    irq)
#define ADC_MUCX_DMA_IRQ_PRIORITY(adc_inst, dma_ch)                             \
    _DT_IRQ_BY_IDX_(DT_INST_PHANDLE_BY_NAME(adc_inst, dmas, dma_ch),            \
                    DT_INST_PHA_BY_NAME(adc_inst, dmas, dma_ch, channel),       \
                    priority)

#define ADC_MCUX_SET_FTM_TRIGGER(n, ftm_base)                                   \
    do {                                                                        \
        uint32_t trg_sel = SIM->SOPT7 & ~((SIM_SOPT7_ADC ##n## TRGSEL_MASK) |   \
                                          (SIM_SOPT7_ADC ##n## ALTTRGEN_MASK)); \
        switch ((uint32_t)(ftm_base)) {                                         \
            case (uint32_t)FTM0:                                                \
                trg_sel |= SIM_SOPT7_ADC ##n## TRGSEL(0x8);                     \
                break;                                                          \
            case (uint32_t)FTM1:                                                \
                trg_sel |= SIM_SOPT7_ADC ##n## TRGSEL(0x9);                     \
                break;                                                          \
            case (uint32_t)FTM2:                                                \
                trg_sel |= SIM_SOPT7_ADC ##n## TRGSEL(0xA);                     \
                break;                                                          \
            case (uint32_t)FTM3:                                                \
                trg_sel |= SIM_SOPT7_ADC ##n## TRGSEL(0xB);                     \
                break;                                                          \
        }                                                                       \
        trg_sel |= SIM_SOPT7_ADC ##n## ALTTRGEN(0x1);                           \
        SIM->SOPT7 = trg_sel;                                                   \
    } while (0)

#define TO_FTM_PRESCALE_DIVIDE(val) _DO_CONCAT(kFTM_Prescale_Divide_, val)

#define ADC_MCUX_DEVICE(n)                                                                          \
    static struct adc_mcux_config adc_mcux_config_##n = {                                           \
        .adc_base = (ADC_Type *)DT_INST_REG_ADDR(n),                                                \
        .ftm_base = (FTM_Type *)DT_REG_ADDR(DT_INST_PHANDLE(n, triggers)),                          \
        .dma_base = DMA0,                                                                           \
        .dma_mux_base = DMAMUX0,                                                                    \
        .ftm_clock_source = DT_INST_PROP_BY_PHANDLE(n, triggers, clock_source),                     \
        .ftm_prescale = TO_FTM_PRESCALE_DIVIDE(DT_INST_PROP_BY_PHANDLE(n, triggers, prescaler)),    \
        .ftm_channel = DT_INST_PHA(n, triggers, channel),                                           \
        .max_channels = DT_INST_PROP(n, max_channels),                                              \
        .dma_ch_result = DT_INST_PHA_BY_NAME(n, dmas, result, channel),                             \
        .dma_ch_ch = DT_INST_PHA_BY_NAME(n, dmas, ch, channel),                                     \
        .dma_ch_alt_ch = DT_INST_PHA_BY_NAME(n, dmas, alt_ch, channel),                             \
        .dma_src_result = DT_INST_PHA_BY_NAME(n, dmas, result, source),                             \
        .dma_src_ch = DT_INST_PHA_BY_NAME(n, dmas, ch, source),                                     \
        .dma_src_alt_ch = DT_INST_PHA_BY_NAME(n, dmas, alt_ch, source),                             \
    };                                                                                              \
                                                                                                    \
    static uint8_t adc_mcux_sc1n_lsb_##n[DT_INST_PROP(n, max_channels)];                            \
    static uint8_t adc_mcux_cfg2_lsb_mux_##n[DT_INST_PROP(n, max_channels)];                        \
    static struct adc_mcux_data adc_mcux_data_##n = {                                               \
        .sc1n_lsb = &adc_mcux_sc1n_lsb_##n[0],                                                      \
        .cfg2_lsb = &adc_mcux_cfg2_lsb_mux_##n[0],                                                  \
    };                                                                                              \
                                                                                                    \
    static int adc_mcux_init_##n(struct device* dev);                                               \
                                                                                                    \
    DEVICE_AND_API_INIT(adc_mcux_##n,                                                               \
                        DT_INST_LABEL(n),                                                           \
                        &adc_mcux_init_##n,                                                         \
                        &adc_mcux_data_##n,                                                         \
                        &adc_mcux_config_##n,                                                       \
                        POST_KERNEL,                                                                \
                        CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                                         \
                        &adc_mcux_driver_api);                                                      \
                                                                                                    \
    static int adc_mcux_init_##n(struct device* dev)                                                \
    {                                                                                               \
        adc_mcux_init(dev);                                                                         \
        IRQ_CONNECT(ADC_MUCX_DMA_IRQ(n, result),                                                    \
                    ADC_MUCX_DMA_IRQ_PRIORITY(n, result),                                           \
                    adc_mcux_edma_dma_irq_handler,                                                  \
                    &(adc_mcux_data_##n.dma_h_result), 0);                                          \
        irq_enable(ADC_MUCX_DMA_IRQ(n, result));                                                    \
        IRQ_CONNECT(ADC_MUCX_DMA_IRQ(n, ch),                                                        \
                    ADC_MUCX_DMA_IRQ_PRIORITY(n, ch),                                               \
                    adc_mcux_edma_dma_irq_handler,                                                  \
                    &(adc_mcux_data_##n.dma_h_ch), 0);                                              \
        irq_enable(ADC_MUCX_DMA_IRQ(n, ch));                                                        \
        IRQ_CONNECT(ADC_MUCX_DMA_IRQ(n, alt_ch),                                                    \
                    ADC_MUCX_DMA_IRQ_PRIORITY(n, alt_ch),                                           \
                    adc_mcux_edma_dma_irq_handler,                                                  \
                    &(adc_mcux_data_##n.dma_h_alt_ch), 0);                                          \
        irq_enable(ADC_MUCX_DMA_IRQ(n, alt_ch));                                                    \
        IRQ_CONNECT(DT_INST_IRQ(n, irq),                                                            \
                    DT_INST_IRQ(n, priority),                                                       \
                    adc_mcux_edma_adc_irq_handler, DEVICE_GET(adc_mcux_##n), 0);                    \
        irq_enable(DT_INST_IRQ(n, irq));                                                            \
        ADC_MCUX_SET_FTM_TRIGGER(n, DT_REG_ADDR(DT_INST_PHANDLE(n, triggers)));                     \
                                                                                                    \
        return 0;                                                                                   \
    }                                                                                               

DT_INST_FOREACH_STATUS_OKAY(ADC_MCUX_DEVICE)