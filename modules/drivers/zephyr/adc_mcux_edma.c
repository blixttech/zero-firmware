#define DT_DRV_COMPAT nxp_kinetis_adc16_edma


#include <adc_mcux_edma.h>
#include <fsl_common.h>
#include <fsl_adc16.h>
#include <fsl_dmamux.h>
#include <fsl_edma.h>
#include <fsl_ftm.h>
#include <fsl_clock.h>
#include <drivers/clock_control.h>

#include <fsl_rcm.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(adc_mcux_edma);

#define FTM_MAX_CHANNELS    ARRAY_SIZE(FTM0->CONTROLS)

#define ADC_MCUX_CH_MASK    (0x1FU)
#define ADC_MCUX_CH_SHIFT   (0U)
#define ADC_MCUX_CH(x)      (((uint8_t)(((uint8_t)(x)) << ADC_MCUX_CH_SHIFT)) &  ADC_MCUX_CH_MASK)   
#define ADC_MCUX_GET_CH(x)  (((uint8_t)(x) & ADC_MCUX_CH_MASK) >> ADC_MCUX_CH_SHIFT)

#define ADC_MCUX_ALT_CH_MASK    (0x20U)
#define ADC_MCUX_ALT_CH_SHIFT   (5U)
#define ADC_MCUX_ALT_CH(x)      (((uint8_t)(((uint8_t)(x)) << ADC_MCUX_ALT_CH_SHIFT)) &  ADC_MCUX_ALT_CH_MASK)
#define ADC_MCUX_GET_ALT_CH(x)  (((uint8_t)(x) & ADC_MCUX_ALT_CH_MASK) >> ADC_MCUX_ALT_CH_SHIFT)

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
    uint8_t dma_src_result;     /* DMA request source used for transfering ADC result */
    uint8_t dma_src_ch;         /* DMA request source used for changing ADC main channel */
};

/**
 * @brief ADC channel configuration block.
 * 
 * This structure maps ADC registers starting from SC1A to CFG2.
 * SC1A specifies the main ADC channel while CFG2 specifies the alternate channel. 
 * Though we don't use SC1B, CFG1 registers when changing the ADC channel, they have 
 * to be there to the DMA tranfer in a single minor loop.
 */
typedef struct adc_ch_config_block {
  uint32_t SC1A;
  uint32_t SC1B;
  uint32_t CFG1;                             
  uint32_t CFG2;
} adc_ch_config_block_t;

struct adc_mcux_data {
    uint32_t ftm_ticks_per_sec;
    edma_handle_t dma_h_result;
    edma_handle_t dma_h_ch;
    adc16_config_t adc_config;
    struct adc_ch_config_block* ch_mux_cfg;
    uint8_t* ch_cfg;
    volatile void* buffer;
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

#if 1
static void adc_mcux_edma_adc_irq_handler(void *arg)
{
/* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
 * exception return operation might vector to incorrect interrupt 
 */
#if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
#endif
}
#endif


static void adc_mcux_edma_callback(edma_handle_t* handle, void* param, 
                                    bool transfer_done, uint32_t tcds)
{
    struct device* dev = (struct device *)param;
    const struct adc_mcux_config* config = dev->config_info;
    struct adc_mcux_data* data = dev->driver_data;

    LOG_DBG("DMA ch %" PRIu8 ", done: %" PRIu8 "---------------------", 
            handle->channel, transfer_done);

    if (handle->channel == config->dma_ch_ch) {
        int i;
        volatile uint16_t* buffer = data->buffer;
        for(i = 0; i < data->seq_len; i++) {
            LOG_DBG("adc buffer[%" PRId32 "]: %" PRIu16, i, buffer[i]);
        }
    }
}

static void set_channel_mux_config(const struct adc_mcux_config* config, 
                                    struct adc_mcux_data* data)
{
    int i;
    uint32_t reg_masked;
    for (i = 0; i < data->seq_len - 1; i++) {
        reg_masked = config->adc_base->SC1[0] & ~ADC_SC1_ADCH_MASK;
        data->ch_mux_cfg[i].SC1A = reg_masked | ADC_SC1_ADCH(ADC_MCUX_GET_CH(data->ch_cfg[i + 1]));
        data->ch_mux_cfg[i].SC1B = config->adc_base->SC1[1];
        data->ch_mux_cfg[i].CFG1 = config->adc_base->CFG1;
        reg_masked = config->adc_base->CFG2 & ~ADC_CFG2_MUXSEL_MASK;
        data->ch_mux_cfg[i].CFG2 = reg_masked | ADC_CFG2_MUXSEL(ADC_MCUX_GET_ALT_CH(data->ch_cfg[i + 1]));
    }

    reg_masked = config->adc_base->SC1[0] & ~ADC_SC1_ADCH_MASK;
    data->ch_mux_cfg[i].SC1A = reg_masked | ADC_SC1_ADCH(ADC_MCUX_GET_CH(data->ch_cfg[0]));
    data->ch_mux_cfg[i].SC1B = config->adc_base->SC1[1];
    data->ch_mux_cfg[i].CFG1 = config->adc_base->CFG1;
    reg_masked = config->adc_base->CFG2 & ~ADC_CFG2_MUXSEL_MASK;
    data->ch_mux_cfg[i].CFG2 = reg_masked | ADC_CFG2_MUXSEL(ADC_MCUX_GET_ALT_CH(data->ch_cfg[0]));

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

    data->ch_cfg[seq_idx] = ADC_MCUX_CH(ch_cfg->channel) | ADC_MCUX_ALT_CH(ch_cfg->alt_channel);

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

    if (data->seq_len == 0) {
        LOG_ERR("No channels to sample");
        return -EINVAL;
    }

    LOG_DBG("ADC seq len: %" PRIu8 "", data->seq_len);

    if (seq_cfg->buffer_size < data->seq_len * sizeof(uint16_t)) {
        LOG_ERR("Not enough memory to store samples");
        return -EINVAL;        
    }

    data->buffer = seq_cfg->buffer;

    LOG_DBG("ADC buffer size: %" PRIu8 "", seq_cfg->buffer_size);

    /* Stop the ADC trigger timer before changes.  */
    FTM_StopTimer(config->ftm_base);
    /* TODO: Not sure if we need to abort any ongoing DMA transfers as well. */
    //EDMA_AbortTransfer(&data->dma_h_result);
    //EDMA_AbortTransfer(&data->dma_h_ch);

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
        /* Need to disable interrupts, HW trigger and DMA before the calibration */
        config->adc_base->SC1[0] &= ~ADC_SC1_AIEN_MASK;
        ADC16_EnableHardwareTrigger(config->adc_base, false);
        ADC16_EnableDMA(config->adc_base, false);
        /* Do recalibration */
        ADC16_DoAutoCalibration(config->adc_base);
        // config->adc_base->SC1[0] |= ADC_SC1_AIEN_MASK;
        ADC16_EnableHardwareTrigger(config->adc_base, true);
        ADC16_EnableDMA(config->adc_base, true);
    }

    set_channel_mux_config(config, data);

    EDMA_CreateHandle(&data->dma_h_result, config->dma_base, config->dma_ch_result);
    EDMA_CreateHandle(&data->dma_h_ch, config->dma_base, config->dma_ch_ch);

    EDMA_SetCallback(&data->dma_h_result, adc_mcux_edma_callback, dev);
    EDMA_SetCallback(&data->dma_h_ch, adc_mcux_edma_callback, dev);

    edma_transfer_config_t tran_cfg_result;
    EDMA_PrepareTransfer(&tran_cfg_result, 
                        (uint32_t*)(config->adc_base->R),   /* Source Address (ADCx_RA) */
                        sizeof(uint16_t),                   /* Source width (2 bytes) */
                        (uint16_t*)data->buffer,            /* Destination buffer */
                        sizeof(uint16_t),                   /* Destination width (2 bytes) */
                        sizeof(uint16_t),                   /* Bytes to transfer each minor loop (2 bytes) */
                        data->seq_len * sizeof(uint16_t),   /* Total of bytes to transfer (12*2 bytes) */
                        kEDMA_PeripheralToMemory);          /* From ADC to Memory */
    EDMA_SubmitTransfer(&data->dma_h_result, &tran_cfg_result);
    EDMA_EnableAutoStopRequest(config->dma_base, config->dma_ch_result, false);

    edma_transfer_config_t tran_cfg_ch;
    EDMA_PrepareTransferConfig(&tran_cfg_ch,
        data->ch_mux_cfg,                   /* Source Address */
        sizeof(adc_ch_config_block_t),      /* Source width (16 bytes) */
        sizeof(adc_ch_config_block_t),      /* Source offset (16 bytes) */
        (uint32_t*)(config->adc_base->SC1), /* Destination address */
        sizeof(adc_ch_config_block_t),      /* Destination width (16 bytes) */
        0,                                  /* Destination offset (0 bytes) */
        sizeof(adc_ch_config_block_t),      /* Bytes to transfer each minor loop (16 bytes) */
        data->seq_len * sizeof(adc_ch_config_block_t)); /* Total of bytes to transfer (16 * data->seq_len bytes) */
    EDMA_SubmitTransfer(&data->dma_h_ch, &tran_cfg_ch);
    EDMA_EnableAutoStopRequest(config->dma_base, config->dma_ch_ch, false);

    /* Adjust the offset to be added to the last source/destination addresses after 
     * the major loop ends.  
     */
    config->dma_base->TCD[config->dma_ch_ch].SLAST = -1 * sizeof(adc_ch_config_block_t) * data->seq_len;
    config->dma_base->TCD[config->dma_ch_result].DLAST_SGA = -1 * sizeof(uint16_t) * data->seq_len;

    EDMA_SetChannelLink(config->dma_base, config->dma_ch_result, kEDMA_MinorLink, config->dma_ch_ch);
    EDMA_SetChannelLink(config->dma_base, config->dma_ch_result, kEDMA_MajorLink, config->dma_ch_ch);

    /* Enable transfer requests for the DMA channels.  */
    EDMA_EnableChannelRequest(config->dma_base, config->dma_ch_result);
    /* Disable all DMA requests as we need them. */
    EDMA_DisableChannelInterrupts(config->dma_base, 
                                    config->dma_ch_result, 
                                    (kEDMA_ErrorInterruptEnable 
                                        | kEDMA_HalfInterruptEnable 
                                        | kEDMA_MajorInterruptEnable));
    EDMA_DisableChannelInterrupts(config->dma_base, 
                                    config->dma_ch_ch, 
                                    (kEDMA_ErrorInterruptEnable 
                                        | kEDMA_HalfInterruptEnable 
                                        | kEDMA_MajorInterruptEnable));

    LOG_DBG("TCD[result] SADDR 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_result].SADDR);
    LOG_DBG("TCD[result] DADDR 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_result].DADDR);
    LOG_DBG("TCD[result] SOFF 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_result].SOFF);
    LOG_DBG("TCD[result] DOFF 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_result].DOFF);
    LOG_DBG("TCD[result] NBYTES_MLNO 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_result].NBYTES_MLNO);
    LOG_DBG("TCD[result] DLAST_SGA %" PRId32, config->dma_base->TCD[config->dma_ch_result].DLAST_SGA);
    LOG_DBG("TCD[result] SLAST %" PRId32, config->dma_base->TCD[config->dma_ch_result].SLAST);
    LOG_DBG("TCD[result] CITER_ELINKYES 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_result].CITER_ELINKYES);
    LOG_DBG("TCD[result] BITER_ELINKYES 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_result].BITER_ELINKYES);

    LOG_DBG("TCD[ch] SADDR 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_ch].SADDR);
    LOG_DBG("TCD[ch] DADDR 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_ch].DADDR);
    LOG_DBG("TCD[ch] SOFF 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_ch].SOFF);
    LOG_DBG("TCD[ch] DOFF 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_ch].DOFF);
    LOG_DBG("TCD[ch] NBYTES_MLNO 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_ch].NBYTES_MLNO);
    LOG_DBG("TCD[ch] DLAST_SGA %" PRId32, config->dma_base->TCD[config->dma_ch_ch].DLAST_SGA);
    LOG_DBG("TCD[ch] SLAST %" PRId32, config->dma_base->TCD[config->dma_ch_ch].SLAST);
    LOG_DBG("TCD[ch] CITER_ELINKNO 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_ch].CITER_ELINKNO);
    LOG_DBG("TCD[ch] BITER_ELINKNO 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_ch].BITER_ELINKNO);

    uint32_t period = (((uint64_t)seq_cfg->interval_us * (uint64_t)data->ftm_ticks_per_sec) / (uint64_t)1e6);
    LOG_DBG("FTM period: %" PRIu32 "", period);
    FTM_SetTimerPeriod(config->ftm_base, period);
    FTM_StartTimer(config->ftm_base, config->ftm_clock_source);

    return 0;
}

static int adc_mcux_init(struct device* dev)
{
    const struct adc_mcux_config* config = dev->config_info;
    struct adc_mcux_data* data = dev->driver_data;

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

    /* ---------------- DMA configuration ---------------- */

    DMAMUX_SetSource(config->dma_mux_base, config->dma_ch_result, config->dma_src_result);
    DMAMUX_EnableChannel(config->dma_mux_base, config->dma_ch_result);
    DMAMUX_SetSource(config->dma_mux_base, config->dma_ch_ch, config->dma_src_ch);
    DMAMUX_EnableChannel(config->dma_mux_base, config->dma_ch_ch);

    LOG_DBG("DMA CH_RESULT: %" PRIu8 ", CH_CH: %" PRIu8, 
            config->dma_ch_result, config->dma_ch_ch);
    LOG_DBG("DMA SRC_RESULT: %" PRIu8 ", SRC_CH: %" PRIu8, 
            config->dma_src_result, config->dma_src_ch);

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
        .dma_src_result = DT_INST_PHA_BY_NAME(n, dmas, result, source),                             \
        .dma_src_ch = DT_INST_PHA_BY_NAME(n, dmas, ch, source),                                     \
    };                                                                                              \
                                                                                                    \
    static struct adc_ch_config_block adc_mcux_ch_mux_cfg_##n[DT_INST_PROP(n, max_channels)];       \
    static uint8_t adc_mcux_ch_cfg_##n[DT_INST_PROP(n, max_channels)];                              \
    static struct adc_mcux_data adc_mcux_data_##n = {                                               \
        .ch_cfg = &adc_mcux_ch_cfg_##n[0],                                                          \
        .ch_mux_cfg = &adc_mcux_ch_mux_cfg_##n[0],                                                  \
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
                                                                                                    \
        IRQ_CONNECT(DT_INST_IRQ(n, irq),                                                            \
                    DT_INST_IRQ(n, priority),                                                       \
                    adc_mcux_edma_adc_irq_handler, DEVICE_GET(adc_mcux_##n), 0);                    \
        irq_enable(DT_INST_IRQ(n, irq));                                                            \
                                                                                                    \
        ADC_MCUX_SET_FTM_TRIGGER(n, DT_REG_ADDR(DT_INST_PHANDLE(n, triggers)));                     \
                                                                                                    \
        return 0;                                                                                   \
    }                                                                                               

DT_INST_FOREACH_STATUS_OKAY(ADC_MCUX_DEVICE)