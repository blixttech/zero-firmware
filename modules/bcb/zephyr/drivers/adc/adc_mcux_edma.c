#define DT_DRV_COMPAT nxp_kinetis_adc16_edma
#include "adc_mcux_edma.h"
#include <drivers/adc_dma.h>
#include <drivers/adc_trigger.h>
#include <drivers/clock_control.h>
#include <fsl_common.h>
#include <fsl_adc16.h>
#include <fsl_dmamux.h>
#include <fsl_edma.h>
#include <fsl_ftm.h>
#include <fsl_clock.h>
#include <fsl_rcm.h>
#include <stdint.h>

#define LOG_LEVEL CONFIG_ADC_MCUX_EDMA_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(adc_mcux_edma);

#define FTM_MAX_CHANNELS ARRAY_SIZE(FTM0->CONTROLS)

#define ADC_MCUX_CH_MASK (0x1FU)
#define ADC_MCUX_CH_SHIFT (0U)
#define ADC_MCUX_CH(x) (((uint8_t)(((uint8_t)(x)) << ADC_MCUX_CH_SHIFT)) & ADC_MCUX_CH_MASK)
#define ADC_MCUX_GET_CH(x) (((uint8_t)(x)&ADC_MCUX_CH_MASK) >> ADC_MCUX_CH_SHIFT)

#define ADC_MCUX_ALT_CH_MASK (0x20U)
#define ADC_MCUX_ALT_CH_SHIFT (5U)
#define ADC_MCUX_ALT_CH(x)                                                                         \
	(((uint8_t)(((uint8_t)(x)) << ADC_MCUX_ALT_CH_SHIFT)) & ADC_MCUX_ALT_CH_MASK)
#define ADC_MCUX_GET_ALT_CH(x) (((uint8_t)(x)&ADC_MCUX_ALT_CH_MASK) >> ADC_MCUX_ALT_CH_SHIFT)

typedef struct adc_mcux_perf_config {
	adc16_config_t adc_config;
	adc16_hardware_average_mode_t avg_mode;
	uint32_t sampling_time;
} adc_mcux_perf_config_t;

struct adc_mcux_config {
	ADC_Type *adc_base;
	DMA_Type *dma_base;
	DMAMUX_Type *dma_mux_base;
	uint8_t max_channels; /* Maximum number of ADC channels supported by the driver */
	uint8_t dma_ch_result; /* DMA channel used for transfering ADC result */
	uint8_t dma_ch_ch; /* DMA channel used for changing ADC main channel */
	uint8_t dma_src_result; /* DMA request source used for transfering ADC result */
	uint8_t dma_src_ch; /* DMA request source used for changing ADC main channel */
	const char *trigger;
	uint32_t sample_interval;
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
	uint32_t SC1A; /* This register contains the main ADC channel configuration. */
	uint32_t SC1B;
	uint32_t CFG1;
	uint32_t CFG2; /* This register contains the alternate ADC channel configuration. */
} adc_ch_mux_block_t;

struct adc_mcux_data {
	uint8_t started;
	edma_handle_t dma_h_result; /* DMA handle for the channel that transfers ADC result. */
	edma_handle_t dma_h_ch; /* DMA handle for the channel that transfers ADC channels. */
	adc_ch_mux_block_t *ch_mux_block;
	uint8_t *ch_cfg; /* Stores the DMA channel configuration (main and alternate). */
	volatile void *buffer; /* Buffer that stores ADC results. */
	size_t buffer_size;
	uint8_t seq_len; /* Length of the ADC sequence. */
	adc16_reference_voltage_source_t v_ref;
	adc_dma_sequence_callback_t callback;
	adc_dma_performance_level_t perf_level;
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
         *              = 384.883 us
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
	.sampling_time = 384883U, /* 384883 ns */
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
         *              = 133.150 us
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
	.sampling_time = 133150U, /* 133150 ns */
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
	.sampling_time = 33950U, /* 33950 ns */
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
	.sampling_time = 9150U, /* 9150 ns */
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
         *              = 17.017 us
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
	.sampling_time = 17017U, /* 17017 ns */
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
         *              = 4.617 us
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
	.sampling_time = 4617U, /* 4617 ns */
    }
};

#define ADC_MCUX_MAX_PERF_LVLS (sizeof(adc_perf_lvls) / sizeof(adc_mcux_perf_config_t))

static void adc_mcux_edma_dma_irq_handler(void *arg)
{
	edma_handle_t *handle = (edma_handle_t *)arg;
	EDMA_HandleIRQ(handle);
/* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
 * exception return operation might vector to incorrect interrupt 
 */
#if defined __CORTEX_M && (__CORTEX_M == 4U)
	__DSB();
#endif
}

static uint32_t inline edma_get_start_major_count(DMA_Type *base, uint32_t channel)
{
	uint32_t count;

	if ((base->TCD[channel].BITER_ELINKNO & DMA_BITER_ELINKNO_ELINK_MASK) != 0) {
		count = (((uint32_t)base->TCD[channel].BITER_ELINKYES &
			  DMA_BITER_ELINKYES_BITER_MASK) >>
			 DMA_BITER_ELINKYES_BITER_SHIFT);
	} else {
		count = (((uint32_t)base->TCD[channel].BITER_ELINKNO &
			  DMA_BITER_ELINKNO_BITER_MASK) >>
			 DMA_BITER_ELINKNO_BITER_SHIFT);
	}

	return count;
}

static void adc_mcux_edma_callback(edma_handle_t *handle, void *param, bool transfer_done,
				   uint32_t tcds)
{
	struct device *dev = param;
	const struct adc_mcux_config *config = dev->config_info;
	struct adc_mcux_data *data = dev->driver_data;
	if (data->callback) {
		uint32_t remaining =
			EDMA_GetRemainingMajorLoopCount(config->dma_base, config->dma_ch_result);
		uint32_t original =
			edma_get_start_major_count(config->dma_base, config->dma_ch_result);
		volatile uint16_t *buffer = data->buffer;

		/* Execution of this callback may be delayed due to the house keeping work
		 * done by Zephyr and the execution of other interrupt handlers (if any).
		 * Therefore, we cannot rely on "transfer_done" flag since it can be cleared by
		 * EDMA engine when the channel is activated (Refer the DMA_TCDn_CSR field descriptions
		 * field descriptions in the reference manual).
		 * Therefore, we rely on the value of the remaining major loop count.
		 */

		if (remaining == 0 || remaining > (original >> 1)) {
			/* Major loop completion  */
			data->callback(dev, &buffer[(original >> 1)], (original - (original >> 1)));
		} else {
			/* Half of the major loop completion */
			data->callback(dev, buffer, (original >> 1));
		}
	}
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

/**
 * @brief   Set the values of SC1A, SC1B, CFG1 & CFG2 registers with relevant 
 *          main and alternate ADC channels. Then initiate ADC conversion if the hardware
 *          trigger is not selected.
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
		data->ch_mux_block[i].SC1A =
			reg_masked | ADC_SC1_ADCH(ADC_MCUX_GET_CH(data->ch_cfg[i + 1]));
		data->ch_mux_block[i].SC1B = config->adc_base->SC1[1];
		data->ch_mux_block[i].CFG1 = config->adc_base->CFG1;
		reg_masked = config->adc_base->CFG2 & ~ADC_CFG2_MUXSEL_MASK;
		data->ch_mux_block[i].CFG2 =
			reg_masked | ADC_CFG2_MUXSEL(ADC_MCUX_GET_ALT_CH(data->ch_cfg[i + 1]));
	}

	reg_masked = config->adc_base->SC1[0] & ~ADC_SC1_ADCH_MASK;
	data->ch_mux_block[i].SC1A = reg_masked | ADC_SC1_ADCH(ADC_MCUX_GET_CH(data->ch_cfg[0]));
	data->ch_mux_block[i].SC1B = config->adc_base->SC1[1];
	data->ch_mux_block[i].CFG1 = config->adc_base->CFG1;
	reg_masked = config->adc_base->CFG2 & ~ADC_CFG2_MUXSEL_MASK;
	data->ch_mux_block[i].CFG2 =
		reg_masked | ADC_CFG2_MUXSEL(ADC_MCUX_GET_ALT_CH(data->ch_cfg[0]));

	config->adc_base->CFG2 = data->ch_mux_block[i].CFG2;
	/* Following starts ADC conversion unless HW trigger mode is selected. */
	config->adc_base->SC1[0] = data->ch_mux_block[i].SC1A;
}

static int adc_mcux_channel_setup_impl(struct device *dev, uint8_t seq_idx,
				       const adc_dma_channel_config_t *ch_cfg)
{
	const struct adc_mcux_config *config = dev->config_info;
	struct adc_mcux_data *data = dev->driver_data;
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
	struct device *trigger_dev;

	if (!data->started) {
		return 0;
	}

	trigger_dev = device_get_binding(config->trigger);
	if (!trigger_dev) {
		LOG_ERR("Trigger device %s not found", config->trigger);
		return -ENODEV;
	}

	adc_trigger_stop(trigger_dev);

	/* Writing all 1s to channel selection bits disables ADC. */
	config->adc_base->SC1[0] |= ADC_SC1_ADCH_MASK;
	ADC16_EnableHardwareTrigger(config->adc_base, false);
	ADC16_EnableDMA(config->adc_base, false);

	EDMA_AbortTransfer(&data->dma_h_result);
	EDMA_AbortTransfer(&data->dma_h_ch);

	EDMA_DisableChannelInterrupts(config->dma_base, config->dma_ch_result,
				      (kEDMA_ErrorInterruptEnable | kEDMA_MajorInterruptEnable |
				       kEDMA_HalfInterruptEnable));
	EDMA_DisableChannelInterrupts(config->dma_base, config->dma_ch_ch,
				      (kEDMA_ErrorInterruptEnable | kEDMA_MajorInterruptEnable |
				       kEDMA_HalfInterruptEnable));

	data->started = 0;
	return 0;
}

static int adc_mcux_read_impl(struct device *dev, const adc_dma_sequence_config_t *seq_cfg)
{
	const struct adc_mcux_config *config = dev->config_info;
	struct adc_mcux_data *data = dev->driver_data;
	struct device *trigger_dev;
	edma_transfer_config_t tran_cfg_result;
	edma_transfer_config_t tran_cfg_ch;

	if (!seq_cfg->len) {
		LOG_ERR("No channels to sample");
		return -EINVAL;
	}

	if (seq_cfg->len > config->max_channels) {
		LOG_ERR("Invalid sequence length %" PRIu8, seq_cfg->len);
		return -EINVAL;
	}

	if (seq_cfg->samples > DMA_BITER_ELINKYES_BITER_MASK) {
		LOG_ERR("Total number of samples should be <= %" PRId32,
			DMA_BITER_ELINKYES_BITER_MASK);
		return -EINVAL;
	}

	if (seq_cfg->samples % seq_cfg->len) {
		LOG_ERR("Invalid number of samples: %" PRIu32, seq_cfg->samples);
		return -EINVAL;
	}

	if (seq_cfg->buffer_size < sizeof(uint16_t) * seq_cfg->samples) {
		LOG_ERR("Not enough memory to store samples");
		return -EINVAL;
	}

	data->seq_len = seq_cfg->len;
	data->buffer = seq_cfg->buffer;
	data->buffer_size = sizeof(uint16_t) * seq_cfg->samples;
	data->callback = seq_cfg->callback;

	adc_mcux_stop_impl(dev);

	/* Enable HW trigger mode for periodic trigger events. */
	ADC16_EnableHardwareTrigger(config->adc_base, true);
	/* Set channel configurations (this does not start conversions since HW trigger mode is set) */
	set_channel_mux_config(config, data);

	EDMA_PrepareTransfer(&tran_cfg_result,
			     /* Source address (ADCx_RA) */
			     (uint32_t *)(config->adc_base->R),
			     /* Source width (2 bytes) */
			     sizeof(uint16_t),
			     /* Destination buffer */
			     (uint16_t *)data->buffer,
			     /* Destination width (2 bytes) */
			     sizeof(uint16_t),
			     /* Bytes transferred in each minor loop (2 bytes) */
			     sizeof(uint16_t),
			     /* Total of bytes to transfer (2*seq_cfg->samples bytes) */
			     data->buffer_size,
			     /* From ADC to Memory */
			     kEDMA_PeripheralToMemory);
	EDMA_SubmitTransfer(&data->dma_h_result, &tran_cfg_result);
	EDMA_EnableAutoStopRequest(config->dma_base, config->dma_ch_result, false);

	EDMA_PrepareTransfer(&tran_cfg_ch,
			     /* Source address */
			     data->ch_mux_block,
			     /* Source width (16 bytes) */
			     sizeof(adc_ch_mux_block_t),
			     /* Destination address */
			     (uint32_t *)(config->adc_base->SC1),
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
	config->dma_base->TCD[config->dma_ch_ch].SLAST =
		-1 * sizeof(adc_ch_mux_block_t) * data->seq_len;
	config->dma_base->TCD[config->dma_ch_result].DLAST_SGA = -1 * data->buffer_size;

	EDMA_SetChannelLink(config->dma_base, config->dma_ch_result, kEDMA_MinorLink,
			    config->dma_ch_ch);
	EDMA_SetChannelLink(config->dma_base, config->dma_ch_result, kEDMA_MajorLink,
			    config->dma_ch_ch);

	EDMA_EnableChannelRequest(config->dma_base, config->dma_ch_result);
	/* TODO: Find why we do not need to enable config->dma_ch_ch.
	 * Could it be due to it is always enabled? 
	 */

	/* Disable major/minor DMA interrupts again since calling EDMA_SubmitTransfer() causes to
	 * enable DMA major interrupts again.
	 */
	EDMA_DisableChannelInterrupts(config->dma_base, config->dma_ch_result,
				      (kEDMA_MajorInterruptEnable | kEDMA_HalfInterruptEnable));
	EDMA_DisableChannelInterrupts(config->dma_base, config->dma_ch_ch,
				      (kEDMA_MajorInterruptEnable | kEDMA_HalfInterruptEnable));

	EDMA_EnableChannelInterrupts(config->dma_base, config->dma_ch_result,
					kEDMA_ErrorInterruptEnable);
	EDMA_EnableChannelInterrupts(config->dma_base, config->dma_ch_ch,
					kEDMA_ErrorInterruptEnable);

	if (data->callback) {
		EDMA_EnableChannelInterrupts(config->dma_base, config->dma_ch_result,
					     kEDMA_MajorInterruptEnable);
		if (edma_get_start_major_count(config->dma_base, config->dma_ch_result) > 50) {
			EDMA_EnableChannelInterrupts(config->dma_base, config->dma_ch_result,
						     kEDMA_HalfInterruptEnable);
		}
	}

	ADC16_EnableDMA(config->adc_base, true);

	trigger_dev = device_get_binding(config->trigger);
	if (!trigger_dev) {
		LOG_ERR("Trigger device %s not found", config->trigger);
		return -ENODEV;
	}

	adc_trigger_set_interval(trigger_dev, config->sample_interval);
	adc_trigger_start(trigger_dev);

	data->started = 1;

	return 0;
}

static int adc_mcux_set_reference_impl(struct device *dev, adc_dma_ref_t ref)
{
	struct adc_mcux_data *data = dev->driver_data;

	if (ref == ADC_DMA_REF_INTERNAL) {
		data->v_ref = kADC16_ReferenceVoltageSourceValt;
	} else if (ref == ADC_DMA_REF_EXTERNAL0) {
		data->v_ref = kADC16_ReferenceVoltageSourceVref;
	} else {
		LOG_ERR("Invalid reference level: %d", ref);
		return -EINVAL;
	}

	return 0;
}

static int adc_mcux_set_perf_level_impl(struct device *dev, adc_dma_performance_level_t level)
{
	const struct adc_mcux_config *config = dev->config_info;
	struct adc_mcux_data *data = dev->driver_data;

	if (level > ADC_MCUX_MAX_PERF_LVLS - 1) {
		LOG_ERR("Invalid performance level. Should be < %" PRIu32 "",
			ADC_MCUX_MAX_PERF_LVLS);
		return -EINVAL;
	}

	data->perf_level = level;
	adc_perf_lvls[level].adc_config.referenceVoltageSource = data->v_ref;
	ADC16_Init(config->adc_base, &(adc_perf_lvls[level].adc_config));
	ADC16_SetHardwareAverage(config->adc_base, adc_perf_lvls[level].avg_mode);

	return 0;
}

static int adc_mcux_calibrate_impl(struct device *dev)
{
	const struct adc_mcux_config *config = dev->config_info;
	status_t status;

	adc_mcux_stop_impl(dev);

	LOG_DBG("Calibrating ADC(single)..");
	status = ADC16_DoAutoCalibration(config->adc_base);

	if (status != kStatus_Success) {
		LOG_ERR("Calibration failed. status %" PRIu32 "", status);
		return -EIO;
	}

	return 0;
}

static int adc_mcux_set_cal_params_impl(struct device *dev, void *params, size_t size)
{
	const struct adc_mcux_config *config = dev->config_info;
	adc_mcux_calibration_values_t values;

	if (size != sizeof(adc_mcux_calibration_values_t)) {
		LOG_ERR("Invalid calibration value size: %d", size);
		return -EINVAL;
	}

	memcpy(&values, params, size);

	config->adc_base->OFS = ADC_OFS_OFS(values.ofs);
	config->adc_base->PG = ADC_PG_PG(values.pg);
	config->adc_base->MG = ADC_MG_MG(values.mg);
	config->adc_base->CLPD = ADC_CLPD_CLPD(values.clpd);
	config->adc_base->CLPS = ADC_CLPS_CLPS(values.clps);
	config->adc_base->CLP4 = ADC_CLP4_CLP4(values.clp4);
	config->adc_base->CLP3 = ADC_CLP3_CLP3(values.clp3);
	config->adc_base->CLP2 = ADC_CLP2_CLP2(values.clp2);
	config->adc_base->CLP1 = ADC_CLP1_CLP1(values.clp1);
	config->adc_base->CLP0 = ADC_CLP0_CLP0(values.clp0);
	config->adc_base->CLMD = ADC_CLMD_CLMD(values.clmd);
	config->adc_base->CLMS = ADC_CLMS_CLMS(values.clms);
	config->adc_base->CLM4 = ADC_CLM4_CLM4(values.clm4);
	config->adc_base->CLM3 = ADC_CLM3_CLM3(values.clm3);
	config->adc_base->CLM2 = ADC_CLM2_CLM2(values.clm2);
	config->adc_base->CLM1 = ADC_CLM1_CLM1(values.clm1);
	config->adc_base->CLM0 = ADC_CLM0_CLM0(values.clm0);

	return 0;
}

static int adc_mcux_get_cal_params_impl(struct device *dev, void *params, size_t size)
{
	const struct adc_mcux_config *config = dev->config_info;
	adc_mcux_calibration_values_t values;

	if (size > sizeof(adc_mcux_calibration_values_t)) {
		LOG_ERR("Invalid calibration value size: %d", size);
		return -EINVAL;
	}

	values.ofs = (uint16_t)config->adc_base->OFS;
	values.pg = (uint16_t)config->adc_base->PG;
	values.mg = (uint16_t)config->adc_base->MG;
	values.clpd = (uint16_t)config->adc_base->CLPD;
	values.clps = (uint16_t)config->adc_base->CLPS;
	values.clp4 = (uint16_t)config->adc_base->CLP4;
	values.clp3 = (uint16_t)config->adc_base->CLP3;
	values.clp2 = (uint16_t)config->adc_base->CLP2;
	values.clp1 = (uint16_t)config->adc_base->CLP1;
	values.clp0 = (uint16_t)config->adc_base->CLP0;
	values.clmd = (uint16_t)config->adc_base->CLMD;
	values.clms = (uint16_t)config->adc_base->CLMS;
	values.clm4 = (uint16_t)config->adc_base->CLM4;
	values.clm3 = (uint16_t)config->adc_base->CLM3;
	values.clm2 = (uint16_t)config->adc_base->CLM2;
	values.clm1 = (uint16_t)config->adc_base->CLM1;
	values.clm0 = (uint16_t)config->adc_base->CLM0;

	memcpy(params, &values, size);

	return 0;
}

static size_t adc_mcux_get_calibration_values_length(struct device *dev)
{
	return sizeof(adc_mcux_calibration_values_t);
}

static uint32_t adc_mcux_get_sampling_time(struct device *dev)
{
	struct adc_mcux_data *data = dev->driver_data;
	adc_dma_performance_level_t level = data->perf_level > ADC_MCUX_MAX_PERF_LVLS - 1 ?
						    ADC_MCUX_MAX_PERF_LVLS - 1 :
						    data->perf_level;

	return adc_perf_lvls[level].sampling_time;
}

static const char* adc_mcux_get_trig_dev(struct device *dev)
{
	const struct adc_mcux_config *config = dev->config_info;

	return config->trigger;
}

static int adc_mcux_init(struct device *dev)
{
	const struct adc_mcux_config *config = dev->config_info;
	struct adc_mcux_data *data = dev->driver_data;

	DMAMUX_SetSource(config->dma_mux_base, config->dma_ch_result, config->dma_src_result);
	DMAMUX_EnableChannel(config->dma_mux_base, config->dma_ch_result);
	DMAMUX_SetSource(config->dma_mux_base, config->dma_ch_ch, config->dma_src_ch);
	DMAMUX_EnableChannel(config->dma_mux_base, config->dma_ch_ch);

	EDMA_CreateHandle(&data->dma_h_result, config->dma_base, config->dma_ch_result);
	EDMA_CreateHandle(&data->dma_h_ch, config->dma_base, config->dma_ch_ch);

	EDMA_SetCallback(&data->dma_h_result, adc_mcux_edma_callback, dev);
	EDMA_SetCallback(&data->dma_h_ch, adc_mcux_edma_callback, dev);

	EDMA_DisableChannelInterrupts(config->dma_base, config->dma_ch_result,
				      (kEDMA_ErrorInterruptEnable | kEDMA_MajorInterruptEnable |
				       kEDMA_HalfInterruptEnable));
	EDMA_DisableChannelInterrupts(config->dma_base, config->dma_ch_ch,
				      (kEDMA_ErrorInterruptEnable | kEDMA_MajorInterruptEnable |
				       kEDMA_HalfInterruptEnable));

	adc_mcux_set_reference_impl(dev, ADC_DMA_REF_INTERNAL);
	adc_mcux_set_perf_level_impl(dev, ADC_DMA_PERF_LEVEL_0);
	adc_mcux_calibrate_impl(dev);

	return 0;
}

static const struct adc_dma_driver_api adc_mcux_driver_api = {
	.read = adc_mcux_read_impl,
	.stop = adc_mcux_stop_impl,
	.channel_setup = adc_mcux_channel_setup_impl,
	.set_performance_level = adc_mcux_set_perf_level_impl,
	.set_reference = adc_mcux_set_reference_impl,
	.calibrate = adc_mcux_calibrate_impl,
	.set_calibration_values = adc_mcux_set_cal_params_impl,
	.get_calibration_values = adc_mcux_get_cal_params_impl,
	.get_calibration_values_length = adc_mcux_get_calibration_values_length,
	.get_sampling_time = adc_mcux_get_sampling_time,
	.get_trig_dev = adc_mcux_get_trig_dev,
};

#define _DT_IRQ_BY_IDX_(node_id, idx, cell) DT_IRQ_BY_IDX(node_id, idx, cell)
#define ADC_MUCX_DMA_IRQ(adc_inst, dma_ch)                                                         \
	_DT_IRQ_BY_IDX_(DT_INST_PHANDLE_BY_NAME(adc_inst, dmas, dma_ch),                           \
			DT_INST_PHA_BY_NAME(adc_inst, dmas, dma_ch, channel), irq)
#define ADC_MUCX_DMA_IRQ_PRIORITY(adc_inst, dma_ch)                                                \
	_DT_IRQ_BY_IDX_(DT_INST_PHANDLE_BY_NAME(adc_inst, dmas, dma_ch),                           \
			DT_INST_PHA_BY_NAME(adc_inst, dmas, dma_ch, channel), priority)

#define ADC_MCUX_SET_TRIGGER(n, trigger)                                                           \
	do {                                                                                       \
		uint32_t trg_sel = SIM->SOPT7 & ~((SIM_SOPT7_ADC##n##TRGSEL_MASK) |                \
						  (SIM_SOPT7_ADC##n##ALTTRGEN_MASK));              \
		trg_sel |= SIM_SOPT7_ADC##n##TRGSEL((trigger));                                    \
		trg_sel |= SIM_SOPT7_ADC##n##ALTTRGEN(0x1);                                        \
		SIM->SOPT7 = trg_sel;                                                              \
	} while (0)

#define TO_FTM_PRESCALE_DIVIDE(val) _DO_CONCAT(kFTM_Prescale_Divide_, val)

#define ADC_MCUX_DEVICE(n)                                                                         \
	static struct adc_mcux_config adc_mcux_config_##n = {                                      \
		.adc_base = (ADC_Type *)DT_INST_REG_ADDR(n),                                       \
		.dma_base = DMA0,                                                                  \
		.dma_mux_base = DMAMUX0,                                                           \
		.max_channels = DT_INST_PROP(n, max_channels),                                     \
		.dma_ch_result = DT_INST_PHA_BY_NAME(n, dmas, result, channel),                    \
		.dma_ch_ch = DT_INST_PHA_BY_NAME(n, dmas, ch, channel),                            \
		.dma_src_result = DT_INST_PHA_BY_NAME(n, dmas, result, source),                    \
		.dma_src_ch = DT_INST_PHA_BY_NAME(n, dmas, ch, source),                            \
		.trigger = DT_LABEL(DT_INST_PHANDLE_BY_IDX(n, triggers, 0)),                       \
		.sample_interval = DT_INST_PROP(n, sample_interval),                               \
	};                                                                                         \
                                                                                                   \
	static adc_ch_mux_block_t adc_mcux_ch_mux_block_##n[DT_INST_PROP(n, max_channels)]         \
		__attribute__((aligned(16)));                                                      \
	static uint8_t adc_mcux_ch_cfg_##n[DT_INST_PROP(n, max_channels)];                         \
	static struct adc_mcux_data adc_mcux_data_##n = {                                          \
		.ch_cfg = &adc_mcux_ch_cfg_##n[0],                                                 \
		.ch_mux_block = &adc_mcux_ch_mux_block_##n[0],                                     \
		.v_ref = kADC16_ReferenceVoltageSourceVref,                                        \
		.callback = NULL,                                                                  \
		.started = 0,                                                                      \
		.perf_level = ADC_DMA_PERF_LEVEL_0,                                                \
	};                                                                                         \
                                                                                                   \
	static int adc_mcux_init_##n(struct device *dev);                                          \
                                                                                                   \
	DEVICE_AND_API_INIT(adc_mcux_##n, DT_INST_LABEL(n), &adc_mcux_init_##n,                    \
			    &adc_mcux_data_##n, &adc_mcux_config_##n, POST_KERNEL,                 \
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &adc_mcux_driver_api);             \
                                                                                                   \
	static int adc_mcux_init_##n(struct device *dev)                                           \
	{                                                                                          \
		adc_mcux_init(dev);                                                                \
		IRQ_CONNECT(ADC_MUCX_DMA_IRQ(n, result), ADC_MUCX_DMA_IRQ_PRIORITY(n, result),     \
			    adc_mcux_edma_dma_irq_handler, &(adc_mcux_data_##n.dma_h_result), 0);  \
		irq_enable(ADC_MUCX_DMA_IRQ(n, result));                                           \
		IRQ_CONNECT(ADC_MUCX_DMA_IRQ(n, ch), ADC_MUCX_DMA_IRQ_PRIORITY(n, ch),             \
			    adc_mcux_edma_dma_irq_handler, &(adc_mcux_data_##n.dma_h_ch), 0);      \
		irq_enable(ADC_MUCX_DMA_IRQ(n, ch));                                               \
                                                                                                   \
		IRQ_CONNECT(DT_INST_IRQ(n, irq), DT_INST_IRQ(n, priority),                         \
			    adc_mcux_edma_adc_irq_handler, DEVICE_GET(adc_mcux_##n), 0);           \
		irq_enable(DT_INST_IRQ(n, irq));                                                   \
                                                                                                   \
		ADC_MCUX_SET_TRIGGER(n, DT_INST_PHA_BY_IDX(n, triggers, 0, sim_config));           \
		return 0;                                                                          \
	}

DT_INST_FOREACH_STATUS_OKAY(ADC_MCUX_DEVICE)