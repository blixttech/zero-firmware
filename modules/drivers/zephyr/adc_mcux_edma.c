#define DT_DRV_COMPAT nxp_kinetis_adc16_edma


#include <adc_mcux_edma.h>
#include <fsl_common.h>
#include <fsl_adc16.h>
#include <fsl_dmamux.h>
#include <fsl_edma.h>
#include <fsl_ftm.h>
#include <fsl_clock.h>
#include <fsl_rcm.h>
#include <drivers/clock_control.h>

#if CONFIG_ADC_MCUX_EDMA_CAL_R
#include <stdlib.h> // For qsort()
#endif // CONFIG_ADC_MCUX_EDMA_CAL_R

#define LOG_LEVEL CONFIG_ADC_MCUX_EDMA_LOG_LEVEL
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

typedef struct adc_mcux_perf_config {
    adc16_config_t adc_config;
    adc16_hardware_average_mode_t avg_mode;
} adc_mcux_perf_config_t;

#if CONFIG_ADC_MCUX_EDMA_CAL_R
typedef struct adc_mcux_cal_params_store {
    uint16_t ofs[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
    uint16_t pg[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
    uint16_t mg[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
    uint16_t clpd[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
    uint16_t clps[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
    uint16_t clp4[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
    uint16_t clp3[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
    uint16_t clp2[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
    uint16_t clp1[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
    uint16_t clp0[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
    uint16_t clmd[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
    uint16_t clms[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
    uint16_t clm4[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
    uint16_t clm3[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
    uint16_t clm2[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
    uint16_t clm1[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
    uint16_t clm0[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES];
} adc_mcux_cal_params_store_t;
#endif // CONFIG_ADC_MCUX_EDMA_CAL_R

struct adc_mcux_config {
    ADC_Type *adc_base;
    FTM_Type *ftm_base;
    DMA_Type *dma_base;
    DMAMUX_Type *dma_mux_base;
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
    adc_ch_mux_block_t *ch_mux_block;
    uint8_t *ch_cfg;            /* Stores the DMA channel configuration (main and alternate). */
    volatile void *buffer;      /* Buffer that stores ADC results. */
    uint8_t seq_len;            /* Length of the ADC sequence. */
    adc16_reference_voltage_source_t v_ref;
#if CONFIG_ADC_MCUX_EDMA_CAL_R
    adc_mcux_cal_params_store_t cal_params_store;
#endif // CONFIG_ADC_MCUX_EDMA_CAL_R
};

/* 
 * ADCK should be > 2 MHz fpr 16-bit single ended conversions.
 *
 * NOTE: It is not clear what frequencies for f_ADCK are supported for different
 *       configurations of CFG2[ADHSC] and CFG1[ADLPC].
 * 
 * Sample time (T_sample) calculation
 * T_sample     = SFCAdder + AverageNum * (BCT + LSTAdder + HSCAdder)
 */ 

static adc_mcux_perf_config_t adc_perf_lvls[] = {
    {
        /* 
         * SFCAdder     = 3 ADCK cycles + 5 BUS cycles
         * AverageNum   = 32
         * BCT          = 25 ADCK cycles (16-bit single-ended)
         * LSTAdder     = 20 ADCK cycles (long sample mode)
         * HSCAdder     = 0 ADCK cycles (normal speed conversion)
         * f_ADCK       = 3.75 MHz
         * f_BUS        = 60 MHz
         * 
         * T_sample     = ((3/3.75)+(5/60)) + (32 * (25 + 20 + 0)/3.75)
         *              = 384.88 us
         */
        .adc_config =   {
            .referenceVoltageSource = kADC16_ReferenceVoltageSourceVref,
            .clockSource = kADC16_ClockSourceAlt1, // Select (Bus clock)/2 = 30 MHz
            .enableAsynchronousClock = false, // Not using asynchronous clock
            .clockDivider = kADC16_ClockDivider8, // ADCK = 3.75 MHz (30/8 MHz)
            .resolution = kADC16_ResolutionSE16Bit, // 25 ADCK cycles
            .longSampleMode = kADC16_LongSampleCycle24, // 20 ADCK cycles
            .enableHighSpeed = false, // 0 ADCK cycles
            .enableLowPower = false,
            .enableContinuousConversion = false,
        },
        .avg_mode = kADC16_HardwareAverageCount32,
    },
    {
        /* 
         * SFCAdder     = 3 ADCK cycles + 5 BUS cycles
         * AverageNum   = 16
         * BCT          = 25 ADCK cycles (16-bit single-ended)
         * LSTAdder     = 6 ADCK cycles (long sample mode)
         * HSCAdder     = 0 ADCK cycles (normal speed conversion)
         * f_ADCK       = 3.75 MHz
         * f_BUS        = 60 MHz
         * 
         * T_sample     = ((3/3.75)+(5/60)) + (16 * (25 + 6 + 0)/3.75)
         *              = 133.15 us
         */
        .adc_config =   {
            .referenceVoltageSource = kADC16_ReferenceVoltageSourceVref,
            .clockSource = kADC16_ClockSourceAlt1, // Select (Bus clock)/2 = 30 MHz
            .enableAsynchronousClock = false, // Not using asynchronous clock
            .clockDivider = kADC16_ClockDivider8, // ADCK = 3.75 MHz (30/8 MHz)
            .resolution = kADC16_ResolutionSE16Bit, // 25 ADCK cycles
            .longSampleMode = kADC16_LongSampleCycle10, // 6 ADCK cycles
            .enableHighSpeed = false, // 0 ADCK cycles
            .enableLowPower = false, 
            .enableContinuousConversion = false,
        },
        .avg_mode = kADC16_HardwareAverageCount16,
    },
    {
        /* 
         * SFCAdder     = 3 ADCK cycles + 5 BUS cycles
         * AverageNum   = 4
         * BCT          = 25 ADCK cycles (16-bit single-ended)
         * LSTAdder     = 6 ADCK cycles (long sample mode)
         * HSCAdder     = 0 ADCK cycles (normal speed conversion)
         * f_ADCK       = 3.75 MHz
         * f_BUS        = 60 MHz
         * 
         * T_sample     = ((3/3.75)+(5/60)) + (4 * (25 + 6 + 0)/3.75)
         *              = 33.95 us
         */
        .adc_config =   {
            .referenceVoltageSource = kADC16_ReferenceVoltageSourceVref,
            .clockSource = kADC16_ClockSourceAlt1, // Select (Bus clock)/2 = 30 MHz
            .enableAsynchronousClock = true, // Not using asynchronous clock
            .clockDivider = kADC16_ClockDivider8, // ADCK = 3.75 MHz (30/8 MHz)
            .resolution = kADC16_ResolutionSE16Bit, // 25 ADCK cycles
            .longSampleMode = kADC16_LongSampleCycle10, // 6 ADCK cycles
            .enableHighSpeed = false, // 0 ADCK cycles
            .enableLowPower = false,
            .enableContinuousConversion = false,
        },
        .avg_mode = kADC16_HardwareAverageCount4,
    },
    {
        /* 
         * SFCAdder     = 3 ADCK cycles + 5 BUS cycles
         * AverageNum   = 1
         * BCT          = 25 ADCK cycles (16-bit single-ended)
         * LSTAdder     = 6 ADCK cycles (long sample mode)
         * HSCAdder     = 0 ADCK cycles (normal speed conversion)
         * f_ADCK       = 3.75 MHz
         * f_BUS        = 60 MHz
         * 
         * T_sample     = ((3/3.75)+(5/60)) + (1 * (25 + 6 + 0)/3.75)
         *              = 9.15 us
         */
        .adc_config =   {
            .referenceVoltageSource = kADC16_ReferenceVoltageSourceVref,
            .clockSource = kADC16_ClockSourceAlt1, // Select (Bus clock)/2 = 30 MHz
            .enableAsynchronousClock = true, // Not using asynchronous clock
            .clockDivider = kADC16_ClockDivider8, // ADCK = 3.75 MHz (30/8 MHz)
            .resolution = kADC16_ResolutionSE16Bit, // 25 ADCK cycles
            .longSampleMode = kADC16_LongSampleCycle10, // 6 ADCK cycles
            .enableHighSpeed = false, // 0 ADCK cycles
            .enableLowPower = false,
            .enableContinuousConversion = false,
        },
        .avg_mode = kADC16_HardwareAverageDisabled,
    },
    {
        /* 
         * SFCAdder     = 3 ADCK cycles + 5 BUS cycles
         * AverageNum   = 4
         * BCT          = 25 ADCK cycles (16-bit single-ended)
         * LSTAdder     = 6 ADCK cycles (long sample mode)
         * HSCAdder     = 0 ADCK cycles (normal speed conversion)
         * f_ADCK       = 7.5 MHz
         * f_BUS        = 60 MHz
         * 
         * T_sample     = ((3/7.5)+(5/60)) + (4 * (25 + 6 + 0)/7.5)
         *              = 17.02 us
         */
        .adc_config =   {
            .referenceVoltageSource = kADC16_ReferenceVoltageSourceVref,
            .clockSource = kADC16_ClockSourceAlt1, // Select (Bus clock)/2 = 30 MHz
            .enableAsynchronousClock = true, // Not using asynchronous clock
            .clockDivider = kADC16_ClockDivider4, // ADCK = 7.5 MHz (30/4 MHz)
            .resolution = kADC16_ResolutionSE16Bit, // 25 ADCK cycles
            .longSampleMode = kADC16_LongSampleCycle10, // 6 ADCK cycles
            .enableHighSpeed = false, // 0 ADCK cycles
            .enableLowPower = false,
            .enableContinuousConversion = false,
        },
        .avg_mode = kADC16_HardwareAverageCount4,
    },
    {
        /* 
         * SFCAdder     = 3 ADCK cycles + 5 BUS cycles
         * AverageNum   = 1
         * BCT          = 25 ADCK cycles (16-bit single-ended)
         * LSTAdder     = 6 ADCK cycles (long sample mode)
         * HSCAdder     = 2 ADCK cycles (high speed conversion)
         * f_ADCK       = 7.5 MHz
         * f_BUS        = 60 MHz
         * 
         * T_sample     = ((3/7.5)+(5/60)) + (1 * (25 + 6 + 0)/7.5)
         *              = 4.62 us
         */
        .adc_config =   {
            .referenceVoltageSource = kADC16_ReferenceVoltageSourceVref,
            .clockSource = kADC16_ClockSourceAlt1, // Select (Bus clock)/2 = 30 MHz
            .enableAsynchronousClock = true, // Not using asynchronous clock
            .clockDivider = kADC16_ClockDivider4, // ADCK = 7.5 MHz (30/4 MHz)
            .resolution = kADC16_ResolutionSE16Bit, // 25 ADCK cycles
            .longSampleMode = kADC16_LongSampleCycle10, // 6 ADCK cycles
            .enableHighSpeed = false, // 0 ADCK cycles
            .enableLowPower = false,
            .enableContinuousConversion = false,
        },
        .avg_mode = kADC16_HardwareAverageDisabled,
    }
};

#define ADC_MCUX_MAX_PERF_LVLS  (sizeof(adc_perf_lvls)/sizeof(adc_mcux_perf_config_t))

static void adc_mcux_edma_dma_irq_handler(void *arg)
{
    edma_handle_t *handle = (edma_handle_t*)arg;
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

/* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
 * exception return operation might vector to incorrect interrupt 
 */
#if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
#endif
}

static void adc_mcux_edma_callback(edma_handle_t *handle, void *param, 
                                    bool transfer_done, uint32_t tcds)
{
    /* Nothing to do here for the moment. */
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
static inline void set_channel_mux_config(const struct adc_mcux_config *config, 
                                    struct adc_mcux_data *data)
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

static int adc_mcux_channel_setup_impl(struct device *dev, uint8_t seq_idx, 
                                        const adc_mcux_channel_config_t *ch_cfg)
{
    const struct adc_mcux_config *config = dev->config_info;
    struct adc_mcux_data* data = dev->driver_data;
    if (seq_idx > (config->max_channels - 1)) {
        LOG_ERR("Invalid sequence index");
        return -EINVAL;
    }
    data->ch_cfg[seq_idx] = ADC_MCUX_CH(ch_cfg->channel) | ADC_MCUX_ALT_CH(ch_cfg->alt_channel);
    return 0;
}

static int adc_mcux_stop_impl(struct device *dev)
{
    const struct adc_mcux_config *config = dev->config_info;
    struct adc_mcux_data *data = dev->driver_data;

    /* Stop the ADC trigger timer before changes. */
    FTM_StopTimer(config->ftm_base);
    /* Abort any ongoing transfers. */
    EDMA_AbortTransfer(&data->dma_h_result);
    EDMA_AbortTransfer(&data->dma_h_ch);

    return 0;
}

static int adc_mcux_read_impl(struct device* dev, const adc_mcux_sequence_config_t* seq_cfg)
{
    const struct adc_mcux_config* config = dev->config_info;
    struct adc_mcux_data* data = dev->driver_data;

    if (seq_cfg->seq_len == 0) {
        LOG_ERR("No channels to sample");
        return -EINVAL;
    }

    if (seq_cfg->seq_len > config->max_channels) {
        LOG_ERR("Invalid sequence length %" PRIu8, seq_cfg->seq_len);
        return -EINVAL;
    }
    data->seq_len = seq_cfg->seq_len;
    LOG_DBG("seq_len: %" PRIu8 "", data->seq_len);

    if (seq_cfg->buffer_size < data->seq_len * sizeof(uint16_t)) {
        LOG_ERR("Not enough memory to store samples");
        return -EINVAL;        
    }
    data->buffer = seq_cfg->buffer;

    /* Stop any already started ADC conversions. */
    adc_mcux_stop_impl(dev);

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

static int adc_mcux_set_reference_impl(struct device *dev, adc_mcux_ref_t ref)
{
    struct adc_mcux_data *data = dev->driver_data;

    if (ref == ADC_MCUX_REF_INTERNAL) {
        data->v_ref = kADC16_ReferenceVoltageSourceValt;
    } else {
        data->v_ref = kADC16_ReferenceVoltageSourceVref;
    }

    return 0;
}

static int adc_mcux_set_perf_level_impl(struct device *dev, uint8_t level)
{
    const struct adc_mcux_config *config = dev->config_info;
    struct adc_mcux_data *data = dev->driver_data;

    if (level > ADC_MCUX_MAX_PERF_LVLS - 1) {
        LOG_ERR("Invalid performance level. Should be < %" PRIu32 "", ADC_MCUX_MAX_PERF_LVLS);
        return -EINVAL;
    }

    adc_perf_lvls[level].adc_config.referenceVoltageSource = data->v_ref;
    ADC16_Init(config->adc_base, &(adc_perf_lvls[level].adc_config));
    ADC16_SetHardwareAverage(config->adc_base, adc_perf_lvls[level].avg_mode);
    ADC16_EnableHardwareTrigger(config->adc_base, true);
    ADC16_EnableDMA(config->adc_base, true);

    return 0;
}

#if CONFIG_ADC_MCUX_EDMA_CAL_R
static int adc_mcux_param_cmp(const void *a, const void *b)
{
    uint16_t arg_a = *((uint16_t*)a);
    uint16_t arg_b = *((uint16_t*)b);

    if (arg_a < arg_b) {
        return -1;
    } else if (arg_a > arg_b) {
        return 1;
    } else {
        return 0;
    }
    return 0;
}

static status_t adc_mcux_calibrate_r_impl(struct device *dev)
{
    const struct adc_mcux_config *config = dev->config_info;
    struct adc_mcux_data *data = dev->driver_data;

    status_t status;
    int i;
    for (i = 0; i < CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES; i++) {
        status = ADC16_DoAutoCalibration(config->adc_base);
        if (status != kStatus_Success) {
            return status;
        }
        data->cal_params_store.ofs[i] = (uint16_t)config->adc_base->OFS;
        data->cal_params_store.pg[i] = (uint16_t)config->adc_base->PG;
        data->cal_params_store.mg[i] = (uint16_t)config->adc_base->MG;
        data->cal_params_store.clpd[i] = (uint16_t)config->adc_base->CLPD;
        data->cal_params_store.clps[i] = (uint16_t)config->adc_base->CLPS;
        data->cal_params_store.clp4[i] = (uint16_t)config->adc_base->CLP4;
        data->cal_params_store.clp3[i] = (uint16_t)config->adc_base->CLP3;
        data->cal_params_store.clp2[i] = (uint16_t)config->adc_base->CLP2;
        data->cal_params_store.clp1[i] = (uint16_t)config->adc_base->CLP1;
        data->cal_params_store.clp0[i] = (uint16_t)config->adc_base->CLP0;
        data->cal_params_store.clmd[i] = (uint16_t)config->adc_base->CLMD;
        data->cal_params_store.clms[i] = (uint16_t)config->adc_base->CLMS;
        data->cal_params_store.clm4[i] = (uint16_t)config->adc_base->CLM4;
        data->cal_params_store.clm3[i] = (uint16_t)config->adc_base->CLM3;
        data->cal_params_store.clm2[i] = (uint16_t)config->adc_base->CLM2;
        data->cal_params_store.clm1[i] = (uint16_t)config->adc_base->CLM1;
        data->cal_params_store.clm0[i] = (uint16_t)config->adc_base->CLM0;
    }

    qsort(data->cal_params_store.ofs, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp); 
    qsort(data->cal_params_store.pg, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp);
    qsort(data->cal_params_store.mg, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp);
    qsort(data->cal_params_store.clpd, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp);
    qsort(data->cal_params_store.clps, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp);
    qsort(data->cal_params_store.clp4, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp);
    qsort(data->cal_params_store.clp3, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp);
    qsort(data->cal_params_store.clp2, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp);
    qsort(data->cal_params_store.clp1, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp);
    qsort(data->cal_params_store.clp0, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp);
    qsort(data->cal_params_store.clmd, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp);
    qsort(data->cal_params_store.clms, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp);
    qsort(data->cal_params_store.clm4, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp);
    qsort(data->cal_params_store.clm3, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp);
    qsort(data->cal_params_store.clm2, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp);
    qsort(data->cal_params_store.clm1, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp);
    qsort(data->cal_params_store.clm0, CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES, sizeof(uint16_t), adc_mcux_param_cmp);

    config->adc_base->OFS = ADC_OFS_OFS(data->cal_params_store.ofs[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);
    config->adc_base->PG = ADC_PG_PG(data->cal_params_store.pg[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);
    config->adc_base->MG = ADC_MG_MG(data->cal_params_store.mg[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);
    config->adc_base->CLPD = ADC_CLPD_CLPD(data->cal_params_store.clpd[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);
    config->adc_base->CLPS = ADC_CLPS_CLPS(data->cal_params_store.clps[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);
    config->adc_base->CLP4 = ADC_CLP4_CLP4(data->cal_params_store.clp4[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);
    config->adc_base->CLP3 = ADC_CLP3_CLP3(data->cal_params_store.clp3[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);
    config->adc_base->CLP2 = ADC_CLP2_CLP2(data->cal_params_store.clp2[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);
    config->adc_base->CLP1 = ADC_CLP1_CLP1(data->cal_params_store.clp1[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);
    config->adc_base->CLP0 = ADC_CLP0_CLP0(data->cal_params_store.clp0[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);
    config->adc_base->CLMD = ADC_CLMD_CLMD(data->cal_params_store.clmd[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);
    config->adc_base->CLMS = ADC_CLMS_CLMS(data->cal_params_store.clms[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);
    config->adc_base->CLM4 = ADC_CLM4_CLM4(data->cal_params_store.clm4[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);
    config->adc_base->CLM3 = ADC_CLM3_CLM3(data->cal_params_store.clm3[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);
    config->adc_base->CLM2 = ADC_CLM2_CLM2(data->cal_params_store.clm2[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);
    config->adc_base->CLM1 = ADC_CLM1_CLM1(data->cal_params_store.clm1[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);
    config->adc_base->CLM0 = ADC_CLM0_CLM0(data->cal_params_store.clm0[CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES/2]);

    return kStatus_Success;
}
#endif // CONFIG_ADC_MCUX_EDMA_CAL_R

static int adc_mcux_calibrate_impl(struct device *dev)
{
    const struct adc_mcux_config *config = dev->config_info;
    
    /* Need to disable interrupts, HW trigger and DMA before the calibration */
    config->adc_base->SC1[0] &= ~ADC_SC1_AIEN_MASK;
    ADC16_EnableHardwareTrigger(config->adc_base, false);
    ADC16_EnableDMA(config->adc_base, false);

    status_t status = kStatus_Success;
#if CONFIG_ADC_MCUX_EDMA_CAL_R
    LOG_INF("Calibrating ADC(repeated %" PRId32 ")..", CONFIG_ADC_MCUX_EDMA_CAL_R_SAMPLES);
    status = adc_mcux_calibrate_r_impl(dev);
#else
    LOG_INF("Calibrating ADC(single)..");
    status = ADC16_DoAutoCalibration(config->adc_base);
#endif // CONFIG_ADC_MCUX_EDMA_CAL_R

    ADC16_EnableHardwareTrigger(config->adc_base, true);
    ADC16_EnableDMA(config->adc_base, true);

    if (status != kStatus_Success) {
        LOG_ERR("Calibration failed. status %" PRIu32 "", status);
        return -EIO;
    }

    return 0;
}

static int adc_mcux_set_cal_params_impl(struct device *dev, adc_mcux_cal_params_t *params)
{
    const struct adc_mcux_config *config = dev->config_info;
    config->adc_base->OFS = ADC_OFS_OFS(params->ofs);
    config->adc_base->PG = ADC_PG_PG(params->pg);
    config->adc_base->MG = ADC_MG_MG(params->mg);
    config->adc_base->CLPD = ADC_CLPD_CLPD(params->clpd);
    config->adc_base->CLPS = ADC_CLPS_CLPS(params->clps);
    config->adc_base->CLP4 = ADC_CLP4_CLP4(params->clp4);
    config->adc_base->CLP3 = ADC_CLP3_CLP3(params->clp3);
    config->adc_base->CLP2 = ADC_CLP2_CLP2(params->clp2);
    config->adc_base->CLP1 = ADC_CLP1_CLP1(params->clp1);
    config->adc_base->CLP0 = ADC_CLP0_CLP0(params->clp0);
    config->adc_base->CLMD = ADC_CLMD_CLMD(params->clmd);
    config->adc_base->CLMS = ADC_CLMS_CLMS(params->clms);
    config->adc_base->CLM4 = ADC_CLM4_CLM4(params->clm4);
    config->adc_base->CLM3 = ADC_CLM3_CLM3(params->clm3);
    config->adc_base->CLM2 = ADC_CLM2_CLM2(params->clm2);
    config->adc_base->CLM1 = ADC_CLM1_CLM1(params->clm1);
    config->adc_base->CLM0 = ADC_CLM0_CLM0(params->clm0);
    return 0;
}

static int adc_mcux_get_cal_params_impl(struct device *dev, adc_mcux_cal_params_t *params)
{
    const struct adc_mcux_config *config = dev->config_info;
    params->ofs = (uint16_t)config->adc_base->OFS;
    params->pg = (uint16_t)config->adc_base->PG;
    params->mg = (uint16_t)config->adc_base->MG;
    params->clpd = (uint16_t)config->adc_base->CLPD;
    params->clps = (uint16_t)config->adc_base->CLPS;
    params->clp4 = (uint16_t)config->adc_base->CLP4;
    params->clp3 = (uint16_t)config->adc_base->CLP3;
    params->clp2 = (uint16_t)config->adc_base->CLP2;
    params->clp1 = (uint16_t)config->adc_base->CLP1;
    params->clp0 = (uint16_t)config->adc_base->CLP0;
    params->clmd = (uint16_t)config->adc_base->CLMD;
    params->clms = (uint16_t)config->adc_base->CLMS;
    params->clm4 = (uint16_t)config->adc_base->CLM4;
    params->clm3 = (uint16_t)config->adc_base->CLM3;
    params->clm2 = (uint16_t)config->adc_base->CLM2;
    params->clm1 = (uint16_t)config->adc_base->CLM1;
    params->clm0 = (uint16_t)config->adc_base->CLM0;
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

static int adc_mcux_init(struct device *dev)
{
    const struct adc_mcux_config *config = dev->config_info;
    struct adc_mcux_data *data = dev->driver_data;


    /* ---------------- ADC configuration ---------------- */

    adc_mcux_set_reference_impl(dev, ADC_MCUX_REF_EXTERNAL);
    adc_mcux_set_perf_level_impl(dev, ADC_MCUX_PERF_LVL_0);
    adc_mcux_calibrate_impl(dev);

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
    .stop = adc_mcux_stop_impl,
    .channel_setup = adc_mcux_channel_setup_impl,
    .set_perf_level = adc_mcux_set_perf_level_impl,
    .set_reference = adc_mcux_set_reference_impl,
    .calibrate = adc_mcux_calibrate_impl,
    .set_cal_params = adc_mcux_set_cal_params_impl,
    .get_cal_params = adc_mcux_get_cal_params_impl,
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
        .v_ref = kADC16_ReferenceVoltageSourceVref,                                                 \
    };                                                                                              \
                                                                                                    \
    static int adc_mcux_init_##n(struct device *dev);                                               \
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
    static int adc_mcux_init_##n(struct device *dev)                                                \
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