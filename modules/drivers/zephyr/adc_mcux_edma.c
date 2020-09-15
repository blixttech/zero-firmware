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

#define ADC_MCUX_CH_MASK        (0x1FU)
#define ADC_MCUX_CH_SHIFT       (0U)
#define ADC_MCUX_CH(x)          (((uint8_t)(((uint8_t)(x)) << ADC_MCUX_CH_SHIFT)) &  ADC_MCUX_CH_MASK)   
#define ADC_MCUX_GET_CH(x)      (((uint8_t)(x) & ADC_MCUX_CH_MASK) >> ADC_MCUX_CH_SHIFT)

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
 * @brief   ADC channel configuration block.
 * 
 * This structure maps ADC registers starting from SC1A to CFG2.
 * SC1A specifies the main ADC channel while CFG2 specifies the alternate channel. 
 * Though we don't use SC1B, CFG1 registers when changing the ADC channel, they have 
 * to be there to the DMA tranfer in a single minor loop.
 */
typedef struct adc_ch_mux_block {
    uint32_t SC1A;    /* This register contains the main ADC channel configuration. */
    uint32_t SC1B;
    uint32_t CFG1;                             
    uint32_t CFG2;    /* This register contains the alternate ADC channel configuration. */
} adc_ch_mux_block_t;

struct adc_mcux_data {
    uint32_t ftm_ticks_per_sec;
    edma_handle_t dma_h_result; /* DMA handle for the channel that transfers ADC result. */
    edma_handle_t dma_h_ch;     /* DMA handle for the channel that transfers ADC channels. */
    adc16_config_t adc_config;
    adc_ch_mux_block_t* ch_mux_block;
    uint8_t* ch_cfg;            /* Stores the DMA channel configuration (main and alternate). */
    volatile void* buffer;      /* Buffer that stores ADC results. */
    uint8_t seq_len;            /* Length of the ADC sequence. */
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
#if 0
    /* Remove the following debugging block when the implementation is stable. */
    struct device* dev = (struct device *)arg;
    const struct adc_mcux_config* config = dev->config_info;
    uint16_t adc_val = ADC16_GetChannelConversionValue(config->adc_base, 0);
    LOG_DBG("ADC irq val %" PRIu16 "-------------", adc_val);
#endif

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
#if 0
    /* Remove the following debugging block when the implementation is stable. */
    struct device* dev = (struct device *)param;
    const struct adc_mcux_config* config = dev->config_info;
    struct adc_mcux_data* data = dev->driver_data;
    LOG_DBG("DMA ch %" PRIu8 ", done: %" PRIu8, handle->channel, transfer_done);
#endif
}

/**
 * @brief   Set the values of SC1A, SC1B, CFG1 & CFG2 registers with relevant 
 *          the main and alternate ADC channels.
 * 
 * We write into all SC1A, SC1B, CFG1 & CFG2 registers due to the way the DMA transfer is done.
 * Therefore, we have to set these registers to their appropriate values.
 * This function reads these registers from the ADC perepharal and set relevant channel multiplexer 
 * block.  
 * 
 * @param config 
 * @param data 
 */
static inline void set_channel_mux_config(const struct adc_mcux_config* config, 
                                    struct adc_mcux_data* data)
{
    int i;
    uint32_t reg_masked;
    for (i = 0; i < data->seq_len - 1; i++) {
        reg_masked = config->adc_base->SC1[0] & ~ADC_SC1_ADCH_MASK;
        data->ch_mux_block[i].SC1A = reg_masked | ADC_SC1_ADCH(ADC_MCUX_GET_CH(data->ch_cfg[i + 1]));
        data->ch_mux_block[i].SC1B = config->adc_base->SC1[1];
        data->ch_mux_block[i].CFG1 = config->adc_base->CFG1;
        reg_masked = config->adc_base->CFG2 & ~ADC_CFG2_MUXSEL_MASK;
        data->ch_mux_block[i].CFG2 = reg_masked | ADC_CFG2_MUXSEL(ADC_MCUX_GET_ALT_CH(data->ch_cfg[i + 1]));
    }

    reg_masked = config->adc_base->SC1[0] & ~ADC_SC1_ADCH_MASK;
    data->ch_mux_block[i].SC1A = reg_masked | ADC_SC1_ADCH(ADC_MCUX_GET_CH(data->ch_cfg[0]));
    data->ch_mux_block[i].SC1B = config->adc_base->SC1[1];
    data->ch_mux_block[i].CFG1 = config->adc_base->CFG1;
    reg_masked = config->adc_base->CFG2 & ~ADC_CFG2_MUXSEL_MASK;
    data->ch_mux_block[i].CFG2 = reg_masked | ADC_CFG2_MUXSEL(ADC_MCUX_GET_ALT_CH(data->ch_cfg[0]));
    /* Set the main and alternate channels of the first in the ADC channel sequence. */
    config->adc_base->SC1[0] = data->ch_mux_block[i].SC1A;
    config->adc_base->CFG2 = data->ch_mux_block[i].CFG2; 
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
        LOG_ERR("Invalid sequence length");
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
    LOG_DBG("ADC buffer size: %" PRIu8 "", seq_cfg->buffer_size);

    data->buffer = seq_cfg->buffer;

    /* Stop the ADC trigger timer before changes. */
    FTM_StopTimer(config->ftm_base);
    /* Abort any ongoing transfers. */
    EDMA_AbortTransfer(&data->dma_h_result);
    EDMA_AbortTransfer(&data->dma_h_ch);

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

        /* Do calibration */
        ADC16_DoAutoCalibration(config->adc_base);
        /* Uncomment the following only when debugging */
        //config->adc_base->SC1[0] |= ADC_SC1_AIEN_MASK; 
        ADC16_EnableHardwareTrigger(config->adc_base, true);
        ADC16_EnableDMA(config->adc_base, true);
    }

    set_channel_mux_config(config, data);

    edma_transfer_config_t tran_cfg_result;
    EDMA_PrepareTransfer(&tran_cfg_result, 
                        /* Source address (ADCx_RA) */
                        (uint32_t*)(config->adc_base->R),
                        /* Source width (2 bytes) */
                        sizeof(uint16_t),
                        /* Destination buffer */
                        (uint16_t*)data->buffer,
                        /* Destination width (2 bytes) */
                        sizeof(uint16_t),
                        /* Bytes transferred in each minor loop (2 bytes) */
                        sizeof(uint16_t),
                        /* Total of bytes to transfer (data->seq_len*2 bytes) */
                        data->seq_len * sizeof(uint16_t),
                        /* From ADC to Memory */
                        kEDMA_PeripheralToMemory);
    EDMA_SubmitTransfer(&data->dma_h_result, &tran_cfg_result);
    EDMA_EnableAutoStopRequest(config->dma_base, config->dma_ch_result, false);

    edma_transfer_config_t tran_cfg_ch;
    EDMA_PrepareTransfer(&tran_cfg_ch, 
                        /* Source address */
                        data->ch_mux_block,
                        /* Source width (16 bytes) */
                        sizeof(adc_ch_mux_block_t),
                        /* Destination address */
                        (uint32_t*)(config->adc_base->SC1),
                        /* Destination width (16 bytes) */ 
                        sizeof(adc_ch_mux_block_t),
                        /* Bytes transferred in each minor loop (16 bytes) */
                        sizeof(adc_ch_mux_block_t),
                        /* Total of bytes to transfer (data->seq_len* 16 bytes) */
                        data->seq_len * sizeof(adc_ch_mux_block_t),
                        /* From memory to ADC */
                        kEDMA_MemoryToPeripheral);
    EDMA_SubmitTransfer(&data->dma_h_ch, &tran_cfg_ch);
    EDMA_EnableAutoStopRequest(config->dma_base, config->dma_ch_ch, false);

    /* Adjust the offset to be added to the last source/destination addresses after 
     * the major loop ends.  
     */
    config->dma_base->TCD[config->dma_ch_ch].SLAST = -1 * sizeof(adc_ch_mux_block_t) * data->seq_len;
    config->dma_base->TCD[config->dma_ch_result].DLAST_SGA = -1 * sizeof(uint16_t) * data->seq_len;

    EDMA_SetChannelLink(config->dma_base, config->dma_ch_result, kEDMA_MinorLink, config->dma_ch_ch);
    EDMA_SetChannelLink(config->dma_base, config->dma_ch_result, kEDMA_MajorLink, config->dma_ch_ch);

    /* Enable transfer requests for the DMA channel.  */
    EDMA_EnableChannelRequest(config->dma_base, config->dma_ch_result);
    /* TODO: Find why we do not need to enable config->dma_ch_ch.
     * Could it be due to it is always enabled? 
     */
    /* Disable all DMA requests as we do not need them. */
    EDMA_DisableChannelInterrupts(config->dma_base, 
                                    config->dma_ch_result, 
                                    (kEDMA_ErrorInterruptEnable 
                                        | kEDMA_MajorInterruptEnable 
                                        | kEDMA_HalfInterruptEnable));
    EDMA_DisableChannelInterrupts(config->dma_base, 
                                    config->dma_ch_ch, 
                                    (kEDMA_ErrorInterruptEnable 
                                        | kEDMA_MajorInterruptEnable 
                                        | kEDMA_HalfInterruptEnable));

#if 0
    /* Remove the following debugging block when the implementation is stable. */
    LOG_DBG("TCD[result] SADDR 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_result].SADDR);
    LOG_DBG("TCD[result] DADDR 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_result].DADDR);
    LOG_DBG("TCD[result] SOFF 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_result].SOFF);
    LOG_DBG("TCD[result] DOFF 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_result].DOFF);
    LOG_DBG("TCD[result] NBYTES_MLNO 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_result].NBYTES_MLNO);
    LOG_DBG("TCD[result] DLAST_SGA %" PRId32, config->dma_base->TCD[config->dma_ch_result].DLAST_SGA);
    LOG_DBG("TCD[result] SLAST %" PRId32, config->dma_base->TCD[config->dma_ch_result].SLAST);
    LOG_DBG("TCD[result] CITER 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_result].CITER_ELINKYES);
    LOG_DBG("TCD[result] BITER 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_result].BITER_ELINKYES);
    LOG_DBG("TCD[result] ATTR 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_result].ATTR);
    LOG_DBG("TCD[result] CSR 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_result].CSR);

    LOG_DBG("TCD[ch] SADDR 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_ch].SADDR);
    LOG_DBG("TCD[ch] DADDR 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_ch].DADDR);
    LOG_DBG("TCD[ch] SOFF 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_ch].SOFF);
    LOG_DBG("TCD[ch] DOFF 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_ch].DOFF);
    LOG_DBG("TCD[ch] NBYTES_MLNO 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_ch].NBYTES_MLNO);
    LOG_DBG("TCD[ch] DLAST_SGA %" PRId32, config->dma_base->TCD[config->dma_ch_ch].DLAST_SGA);
    LOG_DBG("TCD[ch] SLAST %" PRId32, config->dma_base->TCD[config->dma_ch_ch].SLAST);
    LOG_DBG("TCD[ch] CITER 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_ch].CITER_ELINKNO);
    LOG_DBG("TCD[ch] BITER 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_ch].BITER_ELINKNO);
    LOG_DBG("TCD[ch] ATTR 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_ch].ATTR);
    LOG_DBG("TCD[ch] CSR 0x%" PRIx32, config->dma_base->TCD[config->dma_ch_ch].CSR);
#endif

#if 1
    LOG_DBG("ADC SC1  0x%" PRIx32, config->adc_base->SC1[0]);
    LOG_DBG("ADC CFG1 0x%" PRIx32, config->adc_base->CFG1);
    LOG_DBG("ADC CFG2 0x%" PRIx32, config->adc_base->CFG2);
    LOG_DBG("ADC SC2  0x%" PRIx32, config->adc_base->SC2);
    LOG_DBG("ADC SC3  0x%" PRIx32, config->adc_base->SC3);
#endif

    uint32_t period = (((uint64_t)seq_cfg->interval_us * (uint64_t)data->ftm_ticks_per_sec) / (uint64_t)1e6);
    LOG_DBG("FTM period: %" PRIu32 "", period);

    if (period < 2) {
        LOG_ERR("FTM period too small: %" PRIu32 "", period);
        return -EINVAL;
    }

    FTM_SetTimerPeriod(config->ftm_base, period);
    FTM_StartTimer(config->ftm_base, config->ftm_clock_source);

    return 0;
}

static inline uint32_t adc_mcux_get_ftm_exttrig(uint8_t channel)
{
    switch(channel) {
        case 0:
            return kFTM_Chnl0Trigger;
        case 1:
            return kFTM_Chnl1Trigger;
        case 2:
            return kFTM_Chnl2Trigger;
        case 3:
            return kFTM_Chnl3Trigger;
        case 4:
            return kFTM_Chnl4Trigger;
        case 5:
            return kFTM_Chnl5Trigger;
#if defined(FSL_FEATURE_FTM_HAS_CHANNEL6_TRIGGER) && (FSL_FEATURE_FTM_HAS_CHANNEL6_TRIGGER)
        case 6:
            return kFTM_Chnl6Trigger;
#endif
#if defined(FSL_FEATURE_FTM_HAS_CHANNEL7_TRIGGER) && (FSL_FEATURE_FTM_HAS_CHANNEL7_TRIGGER)
        case 7:
            return kFTM_Chnl7Trigger;
#endif
        default:
            LOG_ERR("Invalid FTM external trigger %" PRIu8, channel);
            return kFTM_Chnl0Trigger;
    }

    return kFTM_Chnl0Trigger;
}

static int adc_mcux_init(struct device* dev)
{
    const struct adc_mcux_config* config = dev->config_info;
    struct adc_mcux_data* data = dev->driver_data;

    /* ---------------- ADC configuration ---------------- */

    /* Sample time (T_sample) calculation
     * T_sample     = SFCAdder + AverageNum * (BCT + LSTAdder + HSCAdder)
     * 
     * SFCAdder     = 3 ADCK cycles + 5 BUS cycles
     * AverageNum   = 8
     * BCT          = 25 ADCK cycles
     * LSTAdder     = 6 ADCK cycles
     * HSCAdder     = 2 ADCK cycles
     * f_ADCK       = 12 MHz
     * f_BUS        = 60 MHz
     * 
     * T_sample     = (3/12) + (5/60) + (8 * (25 + 6 + 2) / 12)
     *              = 22.33 us
     */
    ADC16_GetDefaultConfig(&data->adc_config);
    data->adc_config.referenceVoltageSource = kADC16_ReferenceVoltageSourceVref;
    data->adc_config.clockSource = kADC16_ClockSourceAsynchronousClock; // Select ADACK = 12 MHz
    data->adc_config.enableAsynchronousClock = true; // Pre-enable to avoid startup delay
    data->adc_config.clockDivider = kADC16_ClockDivider1; // For maximum clock frequency 
    data->adc_config.resolution = kADC16_ResolutionSE16Bit; // For maximum accuracy
    data->adc_config.longSampleMode = kADC16_LongSampleCycle10; // Reasonable compromise for conversion time
    data->adc_config.enableHighSpeed = true; // This setting is needed for maximum sample rate
    data->adc_config.enableLowPower = false; // This setting is needed for maximum sample rate
    data->adc_config.enableContinuousConversion = false;
    ADC16_Init(config->adc_base, &data->adc_config);

    ADC16_SetHardwareAverage(config->adc_base, kADC16_HardwareAverageCount8);
    ADC16_DoAutoCalibration(config->adc_base);
    ADC16_EnableHardwareTrigger(config->adc_base, true);
    ADC16_EnableDMA(config->adc_base, true);

#if 0
    adc16_channel_config_t adc_ch_cfg;
    adc_ch_cfg.channelNumber = 0; /* Set to channel zero by default. */
    adc_ch_cfg.enableInterruptOnConversionCompleted = false;
    adc_ch_cfg.enableDifferentialConversion = false;
    ADC16_SetChannelConfig(config->adc_base, 0, &adc_ch_cfg);
#endif

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
    ftm_config.extTriggers = adc_mcux_get_ftm_exttrig(config->ftm_channel);
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

    EDMA_CreateHandle(&data->dma_h_result, config->dma_base, config->dma_ch_result);
    EDMA_CreateHandle(&data->dma_h_ch, config->dma_base, config->dma_ch_ch);

    EDMA_SetCallback(&data->dma_h_result, adc_mcux_edma_callback, dev);
    EDMA_SetCallback(&data->dma_h_ch, adc_mcux_edma_callback, dev);

    return 0;
}

static const struct adc_mcux_driver_api adc_mcux_driver_api = {
    .read = adc_mcux_read_impl,
    .channel_setup = adc_mcux_channel_setup_impl,
    .set_sequence_len = adc_mcux_set_sequence_len_impl,
    .get_sequence_len = adc_mcux_get_sequence_len_impl,
};


#define _DT_IRQ_BY_IDX_(node_id, idx, cell)  DT_IRQ_BY_IDX(node_id, idx, cell)
#define ADC_MUCX_DMA_IRQ(adc_inst, dma_ch)                                                          \
    _DT_IRQ_BY_IDX_(DT_INST_PHANDLE_BY_NAME(adc_inst, dmas, dma_ch),                                \
                    DT_INST_PHA_BY_NAME(adc_inst, dmas, dma_ch, channel),                           \
                    irq)
#define ADC_MUCX_DMA_IRQ_PRIORITY(adc_inst, dma_ch)                                                 \
    _DT_IRQ_BY_IDX_(DT_INST_PHANDLE_BY_NAME(adc_inst, dmas, dma_ch),                                \
                    DT_INST_PHA_BY_NAME(adc_inst, dmas, dma_ch, channel),                           \
                    priority)

#define ADC_MCUX_SET_FTM_TRIGGER(n, ftm_base)                                                       \
    do {                                                                                            \
        uint32_t trg_sel = SIM->SOPT7 & ~((SIM_SOPT7_ADC ##n## TRGSEL_MASK) |                       \
                                          (SIM_SOPT7_ADC ##n## ALTTRGEN_MASK));                     \
        switch ((uint32_t)(ftm_base)) {                                                             \
            case (uint32_t)FTM0:                                                                    \
                trg_sel |= SIM_SOPT7_ADC ##n## TRGSEL(0x8);                                         \
                break;                                                                              \
            case (uint32_t)FTM1:                                                                    \
                trg_sel |= SIM_SOPT7_ADC ##n## TRGSEL(0x9);                                         \
                break;                                                                              \
            case (uint32_t)FTM2:                                                                    \
                trg_sel |= SIM_SOPT7_ADC ##n## TRGSEL(0xA);                                         \
                break;                                                                              \
            case (uint32_t)FTM3:                                                                    \
                trg_sel |= SIM_SOPT7_ADC ##n## TRGSEL(0xB);                                         \
                break;                                                                              \
        }                                                                                           \
        trg_sel |= SIM_SOPT7_ADC ##n## ALTTRGEN(0x1);                                               \
        SIM->SOPT7 = trg_sel;                                                                       \
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
    static adc_ch_mux_block_t                                                                       \
        adc_mcux_ch_mux_block_##n[DT_INST_PROP(n, max_channels)] __attribute__ ((aligned (16)));    \
    static uint8_t adc_mcux_ch_cfg_##n[DT_INST_PROP(n, max_channels)];                              \
    static struct adc_mcux_data adc_mcux_data_##n = {                                               \
        .ch_cfg = &adc_mcux_ch_cfg_##n[0],                                                          \
        .ch_mux_block = &adc_mcux_ch_mux_block_##n[0],                                              \
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
        return 0;                                                                                   \
    }                                                                                               

DT_INST_FOREACH_STATUS_OKAY(ADC_MCUX_DEVICE)