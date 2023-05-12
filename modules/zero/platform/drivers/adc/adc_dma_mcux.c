#define DT_DRV_COMPAT nxp_kinetis_adc16_edma

#include <zephyr/device.h>
#include <zephyr/drivers/adc_dma.h>
#include <zephyr/drivers/adc_trigger.h>
#include <zephyr/drivers/dma.h>
#include <zephyr/drivers/pinctrl.h>

#include <fsl_adc16.h>
#include <fsl_common.h>
#include <fsl_edma.h>

#define LOG_LEVEL CONFIG_ADC_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(adc_mcux_edma);

#define ADC_MCUX_CH_MASK   (0x1FU)
#define ADC_MCUX_CH_SHIFT  (0U)
#define ADC_MCUX_CH(x)	   (((uint8_t)(((uint8_t)(x)) << ADC_MCUX_CH_SHIFT)) & ADC_MCUX_CH_MASK)
#define ADC_MCUX_GET_CH(x) (((uint8_t)(x)&ADC_MCUX_CH_MASK) >> ADC_MCUX_CH_SHIFT)

#define ADC_MCUX_ALT_CH_MASK  (0x20U)
#define ADC_MCUX_ALT_CH_SHIFT (5U)
#define ADC_MCUX_ALT_CH(x)                                                                         \
	(((uint8_t)(((uint8_t)(x)) << ADC_MCUX_ALT_CH_SHIFT)) & ADC_MCUX_ALT_CH_MASK)
#define ADC_MCUX_GET_ALT_CH(x) (((uint8_t)(x)&ADC_MCUX_ALT_CH_MASK) >> ADC_MCUX_ALT_CH_SHIFT)

#define ADC_MCUX_CH_TYPE_MASK  (0x40U)
#define ADC_MCUX_CH_TYPE_SHIFT (6U)
#define ADC_MCUX_CH_TYPE(x)                                                                        \
	(((uint8_t)(((uint8_t)(x)) << ADC_MCUX_CH_TYPE_SHIFT)) & ADC_MCUX_CH_TYPE_MASK)
#define ADC_MCUX_GET_CH_TYPE(x) (((uint8_t)(x)&ADC_MCUX_CH_TYPE_MASK) >> ADC_MCUX_CH_TYPE_SHIFT)

/**
 * This structure maps ADC registers starting from SC1A to CFG2.
 * SC1A and CFG2 registers specify the main and alternate ADC channels.
 * Though we don't use SC1B, CFG1 registers when changing the ADC channel, they have
 * to be there to do the DMA transfer in a single minor loop.
 */
typedef struct adc_mcux_mux_regs {
	uint32_t SC1A;
	uint32_t SC1B;
	uint32_t CFG1;
	uint32_t CFG2;
} adc_mcux_mux_regs_t;

struct adc_mcux_config {
	ADC_Type *base;
	DMA_Type *dma_base;
	uint8_t max_channels;
	uint8_t clock_source;
	uint8_t clock_divider;
	uint8_t long_sample;
	uint8_t high_speed;
	uint8_t dma_source_result;
	uint8_t dma_source_channel;
	const char *trigger_dev;
	const char *dma_dev_result;
	const char *dma_dev_channel;
	const struct pinctrl_dev_config *pin_ctrl_cfg;
};

struct adc_mcux_dma_conf {
	const struct device *dev;
	uint32_t ch_result;  /* DMA channel for transferring ADC result. */
	uint32_t ch_channel; /* DMA channel for changing ADC channel. */
	struct dma_config conf_result;
	struct dma_config conf_channel;
	struct dma_block_config blk_conf_result1;
	struct dma_block_config blk_conf_result2;
	struct dma_block_config blk_conf_channel;
};

struct adc_mcux_data {
	uint8_t started;
	uint8_t seq_len; /* Length of the ADC sequence. */
	uint8_t *adc_channels;
	void *buffer;
	size_t buffer_size;
	enum adc_dma_sampling_mode mode;
	const struct device *trigger_dev;
	adc_mcux_mux_regs_t *channel_mux_regs;
	struct adc_mcux_dma_conf dma_config;
	adc_dma_seq_callback callback;
};

static void adc_mcux_dma_callback(const struct device *dev, void *user_data, uint32_t channel,
				  int status)
{
	const struct device *adc_dev = user_data;
	const struct adc_mcux_config *config = adc_dev->config;
	struct adc_mcux_data *data = adc_dev->data;
	void *buffer;
	size_t buffer_size;

	if (status != 0) {
		LOG_ERR("dma error");
		return;
	}

	if (data->mode == ADC_DMA_SAMPLING_MODE_SINGLE) {
		if (data->trigger_dev) {
			adc_trigger_stop(data->trigger_dev);
		}
		buffer = data->buffer;
		buffer_size = data->buffer_size;
	} else {
		uint32_t dest_addr = config->dma_base->TCD[channel].DADDR;

		if (dest_addr == data->dma_config.blk_conf_result1.dest_address) {
			buffer = (void *)data->dma_config.blk_conf_result2.dest_address;
			buffer_size = data->dma_config.blk_conf_result2.block_size;

		} else if (dest_addr == data->dma_config.blk_conf_result2.dest_address) {
			buffer = (void *)data->dma_config.blk_conf_result1.dest_address;
			buffer_size = data->dma_config.blk_conf_result1.block_size;
		} else {
			LOG_ERR("invalid buffer address");
			return;
		}
	}

	if (data->callback) {
		data->callback(adc_dev, buffer, buffer_size);
	}
}

/**
 * We write into all SC1A, SC1B, CFG1 & CFG2 registers due to the way the DMA transfer is done.
 * Therefore, we have to set these registers to their appropriate values.
 * This function reads these registers from the ADC peripheral and set the relevant adc channel
 * multiplexer registers.
 * NOTE: Set the HW trigger mode before using this function.
 */
static inline void set_channel_mux_regs(const struct adc_mcux_config *config,
					struct adc_mcux_data *data)
{
	int i;
	uint32_t reg_masked;
	for (i = 0; i < data->seq_len - 1; i++) {
		reg_masked = config->base->SC1[0] & ~ADC_SC1_ADCH_MASK;
		data->channel_mux_regs[i].SC1A =
			reg_masked | ADC_SC1_ADCH(ADC_MCUX_GET_CH(data->adc_channels[i + 1])) |
			ADC_SC1_DIFF(ADC_MCUX_GET_CH_TYPE(data->adc_channels[i + 1]));
		data->channel_mux_regs[i].SC1B = config->base->SC1[1];

		data->channel_mux_regs[i].CFG1 = config->base->CFG1;

		reg_masked = config->base->CFG2 & ~ADC_CFG2_MUXSEL_MASK;
		data->channel_mux_regs[i].CFG2 =
			reg_masked |
			ADC_CFG2_MUXSEL(ADC_MCUX_GET_ALT_CH(data->adc_channels[i + 1]));
	}

	/* Load channel multiplexer registers with the first channel in the sequence. */
	reg_masked = config->base->SC1[0] & ~ADC_SC1_ADCH_MASK;
	data->channel_mux_regs[i].SC1A = reg_masked |
					 ADC_SC1_ADCH(ADC_MCUX_GET_CH(data->adc_channels[0])) |
					 ADC_SC1_DIFF(ADC_MCUX_GET_CH_TYPE(data->adc_channels[0]));
	data->channel_mux_regs[i].SC1B = config->base->SC1[1];

	data->channel_mux_regs[i].CFG1 = config->base->CFG1;

	reg_masked = config->base->CFG2 & ~ADC_CFG2_MUXSEL_MASK;
	data->channel_mux_regs[i].CFG2 =
		reg_masked | ADC_CFG2_MUXSEL(ADC_MCUX_GET_ALT_CH(data->adc_channels[0]));

	config->base->CFG2 = data->channel_mux_regs[i].CFG2;
	/* Following starts ADC conversion unless HW trigger mode is selected. */
	config->base->SC1[0] = data->channel_mux_regs[i].SC1A;
}

static void adc_mcux_irq_handler(void *arg)
{
/* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
 * exception return operation might vector to incorrect interrupt
 */
#if defined __CORTEX_M && (__CORTEX_M == 4U)
	__DSB();
#endif
}

static int adc_mcux_start(const struct device *device,
			  const struct adc_dma_sampling_config *sampling_config)
{
	const struct adc_mcux_config *config;
	struct adc_mcux_data *data;
	adc16_resolution_t resolution;
	uint8_t result_size;
	size_t required_buffer_size;
	uint32_t result_block_count;
	uint32_t tmp32;
	int r;

	if (!device || !sampling_config) {
		return -EINVAL;
	}

	config = device->config;
	data = device->data;

	if (data->started) {
		LOG_WRN("already started");
		return -EINVAL;
	}

	data->trigger_dev = device_get_binding(config->trigger_dev);
	if (!data->trigger_dev) {
		LOG_ERR("Trigger device %s not found", config->trigger_dev);
		return -ENODEV;
	}

	switch (sampling_config->resolution) {
	case 8:
		resolution = kADC16_Resolution8or9Bit;
		result_size = 1U;
		break;
	case 9:
		resolution = kADC16_Resolution8or9Bit;
		result_size = 2U;
		break;
	case 10:
	case 11:
		resolution = kADC16_Resolution10or11Bit;
		result_size = 2U;
		break;
	case 12:
	case 13:
		resolution = kADC16_Resolution12or13Bit;
		result_size = 2U;
		break;
#if defined(FSL_FEATURE_ADC16_MAX_RESOLUTION) && (FSL_FEATURE_ADC16_MAX_RESOLUTION >= 16U)
	case 16:
		resolution = kADC16_Resolution16Bit;
		result_size = 2U;
		break;
#endif
	default:
		LOG_ERR("invalid resolution");
		return -EINVAL;
	}

	required_buffer_size = result_size * sampling_config->sequence_length *
			       sampling_config->samples_per_channel;

	if (!sampling_config->sequence_length) {
		LOG_ERR("no channels to sample");
		return -EINVAL;
	}

	if (sampling_config->sequence_length > config->max_channels) {
		LOG_ERR("invalid number of channels: %" PRIu8, sampling_config->sequence_length);
		return -EINVAL;
	}

	if (sampling_config->buffer_size < required_buffer_size) {
		LOG_ERR("not enough memory for samples");
		return -ENOMEM;
	}

	data->buffer = sampling_config->buffer;
	data->buffer_size = required_buffer_size;
	data->seq_len = sampling_config->sequence_length;
	data->mode = sampling_config->mode;
	data->callback = sampling_config->callback;

	memset(&data->dma_config.conf_result, 0, sizeof(struct dma_config));
	memset(&data->dma_config.conf_channel, 0, sizeof(struct dma_config));
	memset(&data->dma_config.blk_conf_result1, 0, sizeof(struct dma_block_config));
	memset(&data->dma_config.blk_conf_result2, 0, sizeof(struct dma_block_config));
	memset(&data->dma_config.blk_conf_channel, 0, sizeof(struct dma_block_config));

	/*-------- DMA transfer configuration for ADC result --------*/
	data->dma_config.blk_conf_result1.source_address = (uint32_t)&config->base->R[0];
	result_block_count = 1;

	if (sampling_config->mode == ADC_DMA_SAMPLING_MODE_CONTINUOUS) {
		data->dma_config.blk_conf_result1.dest_scatter_en = true;
		if (sampling_config->samples_per_channel > 1) {
			/* We need more than one samples per channel to enable double buffering. */
			uint32_t result1_block_size = result_size *
						      sampling_config->sequence_length *
						      (sampling_config->samples_per_channel >> 1);

			data->dma_config.blk_conf_result1.next_block =
				&(data->dma_config.blk_conf_result2);
			data->dma_config.blk_conf_result1.dest_address = (uint32_t)data->buffer;
			/* Total number of bytes to be transferred for this block. */
			data->dma_config.blk_conf_result1.block_size = result1_block_size;

			data->dma_config.blk_conf_result2.source_address =
				(uint32_t)&config->base->R[0];
			data->dma_config.blk_conf_result2.dest_scatter_en = true;
			data->dma_config.blk_conf_result2.dest_address =
				(uint32_t)data->buffer + result1_block_size;
			/* Total number of bytes to be transferred for this block. */
			data->dma_config.blk_conf_result2.block_size =
				data->buffer_size - result1_block_size;
			result_block_count++;
			data->dma_config.conf_result.cyclic = true;
		} else {
			data->dma_config.blk_conf_result1.dest_address = (uint32_t)data->buffer;
			data->dma_config.blk_conf_result1.block_size = data->buffer_size;
		}

	} else {
		data->dma_config.blk_conf_result1.dest_address = (uint32_t)data->buffer;
		/* Total number of bytes to be transferred for this block. */
		data->dma_config.blk_conf_result1.block_size = data->buffer_size;
	}

	data->dma_config.conf_result.dma_slot = config->dma_source_result;
	data->dma_config.conf_result.channel_direction = PERIPHERAL_TO_MEMORY;
	data->dma_config.conf_result.block_count = result_block_count;
	data->dma_config.conf_result.head_block = &data->dma_config.blk_conf_result1;
	data->dma_config.conf_result.dma_callback = &adc_mcux_dma_callback;
	data->dma_config.conf_result.user_data = (void *)device;
	/* Number of bytes to be transferred at a time.
	 * Note that DMA might transfer bytes several times on each request.
	 * Read the documentation of the device for more details.
	 * In this case, number of bytes transferred at a time and on each request are same.
	 */
	data->dma_config.conf_result.source_data_size = result_size;
	data->dma_config.conf_result.dest_data_size = result_size;
	/* How many bytes to be transferred on each request. */
	data->dma_config.conf_result.dest_burst_length = result_size;
	data->dma_config.conf_result.source_burst_length = result_size;
	/* Enable minor loop link.
	 * After a result transfer is completed, initiate transfer for ADC channel change.
	 */
	data->dma_config.conf_result.source_chaining_en = true;
	data->dma_config.conf_result.linked_channel = data->dma_config.ch_channel;

	/*-------- DMA transfer configuration for ADC channel change --------*/

	data->dma_config.blk_conf_channel.source_address = (uint32_t)data->channel_mux_regs;
	data->dma_config.blk_conf_channel.dest_address = (uint32_t)config->base->SC1;
	/* Total number of bytes to be transferred for this block. */
	data->dma_config.blk_conf_channel.block_size = sizeof(adc_mcux_mux_regs_t) * data->seq_len;
	/* Reload the source address at the end of the block transfer. */
	data->dma_config.blk_conf_channel.source_reload_en = true;

	data->dma_config.conf_channel.channel_direction = MEMORY_TO_PERIPHERAL;
	data->dma_config.conf_channel.block_count = 1U;
	data->dma_config.conf_channel.head_block = &data->dma_config.blk_conf_channel;
	/* In this case, number of bytes transferred at a time is less than the total number of
	 * bytes on each request.
	 */
	data->dma_config.conf_channel.source_data_size = 4U;
	data->dma_config.conf_channel.dest_data_size = 4U;
	/* How many bytes to be transferred on each request. */
	data->dma_config.conf_channel.dest_burst_length = sizeof(adc_mcux_mux_regs_t);
	data->dma_config.conf_channel.source_burst_length = sizeof(adc_mcux_mux_regs_t);

	r = dma_config(data->dma_config.dev, data->dma_config.ch_result,
		       &data->dma_config.conf_result);
	if (r) {
		LOG_ERR("adc result dma configuration failed");
		return -EIO;
	}

	dma_start(data->dma_config.dev, data->dma_config.ch_result);

	r = dma_config(data->dma_config.dev, data->dma_config.ch_channel,
		       &data->dma_config.conf_channel);
	if (r) {
		LOG_ERR("adc channel dma configuration failed");
		return -EIO;
	}

	dma_start(data->dma_config.dev, data->dma_config.ch_channel);

	tmp32 = config->base->CFG1 & ~(ADC_CFG1_MODE_MASK);
	tmp32 |= ADC_CFG1_MODE(resolution);
	config->base->CFG1 = tmp32;

	set_channel_mux_regs(config, data);

	r = adc_trigger_set_interval(data->trigger_dev, sampling_config->sample_interval);
	if (r) {
		return r;
	}

	r = adc_trigger_start(data->trigger_dev);
	if (r) {
		return r;
	}

	LOG_DBG("started");
	data->started = true;

	return 0;
}

static void adc_mcux_stop(const struct device *device)
{
	const struct adc_mcux_config *config;
	struct adc_mcux_data *data;

	if (!device) {
		return;
	}

	config = device->config;
	data = device->data;

	if (!data->started) {
		return;
	}

	if (data->trigger_dev) {
		adc_trigger_stop(data->trigger_dev);
	} else {
		LOG_WRN("trigger device %s not found", config->trigger_dev);
	}

	dma_stop(data->dma_config.dev, data->dma_config.ch_result);
	dma_stop(data->dma_config.dev, data->dma_config.ch_channel);

	data->started = false;

	LOG_DBG("stopped");
}

static int adc_mcux_add_channel(const struct device *device,
				const struct adc_dma_channel_config *channel_config,
				uint8_t sequence_index)
{
	const struct adc_mcux_config *config;
	struct adc_mcux_data *data;

	if (!device || !channel_config) {
		return -EINVAL;
	}

	config = device->config;
	data = device->data;

	if (sequence_index > config->max_channels - 1) {
		LOG_ERR("invalid sequence index");
		return -EINVAL;
	}

	data->adc_channels[sequence_index] = ADC_MCUX_CH(channel_config->channel) |
					     ADC_MCUX_ALT_CH(channel_config->alt_channel) |
					     ADC_MCUX_CH_TYPE(channel_config->type);

	return 0;
}

static const struct device *adc_mcux_get_trigger_device(const struct device *device)
{
	const struct adc_mcux_config *config;

	if (!device) {
		return NULL;
	}

	config = device->config;

	return device_get_binding(config->trigger_dev);
}

static uint32_t adc_mcux_get_sampling_time(const struct device *device)
{
	if (!device) {
		return 0;
	}

	return 0;
}

static int adc_mcux_init(const struct device *device)
{
	const struct adc_mcux_config *config = device->config;
	struct adc_mcux_data *data = device->data;
	enum dma_channel_filter dma_filter;
	adc16_config_t adc_config;
	int ch_result;
	int ch_channel;
	int r;

	ADC16_GetDefaultConfig(&adc_config);

	adc_config.referenceVoltageSource = kADC16_ReferenceVoltageSourceVref;
	adc_config.clockSource = config->clock_source;
	adc_config.enableAsynchronousClock = false;
	adc_config.clockDivider = config->clock_divider;
	adc_config.resolution = kADC16_Resolution16Bit;
	adc_config.longSampleMode = config->long_sample;
	adc_config.enableHighSpeed = config->high_speed;
	adc_config.enableLowPower = false;
	adc_config.enableContinuousConversion = false;

	ADC16_Init(config->base, &adc_config);

#if defined(FSL_FEATURE_ADC16_HAS_CALIBRATION) && FSL_FEATURE_ADC16_HAS_CALIBRATION
	ADC16_SetHardwareAverage(config->base, kADC16_HardwareAverageCount32);
	ADC16_DoAutoCalibration(config->base);
#endif

	ADC16_EnableHardwareTrigger(config->base, true);
	ADC16_EnableDMA(config->base, true);

	r = pinctrl_apply_state(config->pin_ctrl_cfg, PINCTRL_STATE_DEFAULT);
	if (r) {
		return r;
	}

	if (config->dma_dev_result != config->dma_dev_channel) {
		LOG_ERR("different dma controllers");
		return -EINVAL;
	}

	data->dma_config.dev = device_get_binding(config->dma_dev_result);
	if (!data->dma_config.dev || !device_is_ready(data->dma_config.dev)) {
		LOG_ERR("dma bind failed");
		return -ENODEV;
	}

	dma_filter = DMA_CHANNEL_NORMAL;
	ch_result = dma_request_channel(data->dma_config.dev, (void *)&dma_filter);
	ch_channel = dma_request_channel(data->dma_config.dev, (void *)&dma_filter);

	if (ch_result < 0 || ch_channel < 0) {
		LOG_ERR("cannot allocate dma channels");
		return -ENOMEM;
	}

	data->dma_config.ch_result = ch_result;
	data->dma_config.ch_channel = ch_channel;

	return 0;
}

static const struct adc_dma_driver_api adc_mcux_driver_api = {
	.start = &adc_mcux_start,
	.stop = &adc_mcux_stop,
	.add_channel = &adc_mcux_add_channel,
	.get_sampling_time = &adc_mcux_get_sampling_time,
	.get_trigger_device = &adc_mcux_get_trigger_device,
};

#define ADC_MCUX_SET_TRIGGER(n, trigger)                                                           \
	do {                                                                                       \
		uint32_t trg_sel = SIM->SOPT7 & ~((SIM_SOPT7_ADC##n##TRGSEL_MASK) |                \
						  (SIM_SOPT7_ADC##n##ALTTRGEN_MASK));              \
		trg_sel |= SIM_SOPT7_ADC##n##TRGSEL((trigger));                                    \
		trg_sel |= SIM_SOPT7_ADC##n##ALTTRGEN(0x1);                                        \
		SIM->SOPT7 = trg_sel;                                                              \
	} while (0)

#define ADC_MCUX_DEVICE(n)                                                                         \
	PINCTRL_DT_INST_DEFINE(n);                                                                 \
                                                                                                   \
	static struct adc_mcux_config adc_mcux_config_##n = {                                      \
		.base = (ADC_Type *)DT_INST_REG_ADDR(n),                                           \
		.dma_base = (DMA_Type *)DT_REG_ADDR(DT_INST_PHANDLE_BY_NAME(n, dmas, result)),     \
		.clock_source = DT_INST_PROP_OR(n, clk_source, 0),                                 \
		.clock_divider = DT_INST_PROP_OR(n, clk_divider, 0),                               \
		.long_sample = DT_INST_PROP_OR(n, long_sample, 0),                                 \
		.high_speed = DT_INST_PROP_OR(n, high_speed, 0),                                   \
		.max_channels = DT_INST_PROP(n, max_channels),                                     \
		.dma_source_result = DT_INST_DMAS_CELL_BY_NAME(n, result, source),                 \
		.dma_source_channel = DT_INST_DMAS_CELL_BY_NAME(n, channel, source),               \
		.pin_ctrl_cfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),                                 \
		.trigger_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(n, triggers, 0)),             \
		.dma_dev_result = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_NAME(n, dmas, result)),        \
		.dma_dev_channel = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_NAME(n, dmas, channel)),      \
	};                                                                                         \
                                                                                                   \
	static adc_mcux_mux_regs_t adc_channel_mux_regs_##n[DT_INST_PROP(n, max_channels)]         \
		__attribute__((aligned(16)));                                                      \
                                                                                                   \
	static uint8_t adc_channels_##n[DT_INST_PROP(n, max_channels)];                            \
                                                                                                   \
	static struct adc_mcux_data adc_mcux_data_##n = {                                          \
		.channel_mux_regs = &adc_channel_mux_regs_##n[0],                                  \
		.adc_channels = &adc_channels_##n[0],                                              \
	};                                                                                         \
                                                                                                   \
	static int adc_mcux_init_##n(const struct device *device)                                  \
	{                                                                                          \
		int r;                                                                             \
                                                                                                   \
		r = adc_mcux_init(device);                                                         \
                                                                                                   \
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority), adc_mcux_irq_handler,       \
			    DEVICE_DT_INST_GET(n), 0);                                             \
		irq_enable(DT_INST_IRQN(n));                                                       \
                                                                                                   \
		ADC_MCUX_SET_TRIGGER(n, DT_INST_PHA_BY_IDX(n, triggers, 0, sim_config));           \
		return r;                                                                          \
	}                                                                                          \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &adc_mcux_init_##n, NULL, &adc_mcux_data_##n,                     \
			      &adc_mcux_config_##n, POST_KERNEL, CONFIG_ADC_INIT_PRIORITY,         \
			      &adc_mcux_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ADC_MCUX_DEVICE)