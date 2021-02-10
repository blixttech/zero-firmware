/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_kinetis_ftm_input_capture

#include <drivers/clock_control.h>
#include <drivers/input_capture.h>
#include <errno.h>
#include <soc.h>
#include <fsl_ftm.h>
#include <fsl_clock.h>

#define LOG_LEVEL CONFIG_IC_MCUX_FTM_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(input_capture_mcux_ftm);

#define MAX_CHANNELS ARRAY_SIZE(FTM0->CONTROLS)

struct ic_mcux_ftm_config {
	FTM_Type *base;
	ftm_clock_source_t clock_source;
	ftm_clock_prescale_t prescale;
	uint8_t channel_count;
	uint8_t filter;
};

struct ic_mcux_ftm_data {
	bool started;
	uint32_t ticks_per_sec;
	volatile bool enabled_interrupts[MAX_CHANNELS];
	volatile uint8_t edges[MAX_CHANNELS];
	volatile input_capture_callback_t callbacks[MAX_CHANNELS];
};

static inline bool is_dual_edge(uint8_t edge)
{
	bool is_dual;
	switch (edge) {
	case IC_EDGE_SINGLE_RISING:
	case IC_EDGE_SINGLE_FALLING:
	case IC_EDGE_SINGLE_RISING_FALLING:
		is_dual = false;
		break;
	case IC_EDGE_DUAL_PULSE_RISING:
	case IC_EDGE_DUAL_PULSE_FALLING:
	case IC_EDGE_DUAL_PERIOD_RISING:
	case IC_EDGE_DUAL_PERIOD_FALLING:
		is_dual = true;
		break;
	default:
		is_dual = false;
	}

	return is_dual;
}

static inline bool is_single_edge(uint8_t edge)
{
	return !is_dual_edge(edge);
}

static inline bool is_dual_edge_base(uint8_t channel)
{
	return channel % 2 == 0;
}

static inline uint8_t get_dual_edge_base(uint8_t channel)
{
	if (channel % 2 == 0) {
		return channel;
	}

	return channel - 1;
}

static void ftm_irq_handler(void *arg)
{
	struct device *dev = (struct device *)arg;
	const struct ic_mcux_ftm_config *config = dev->config_info;
	struct ic_mcux_ftm_data *data = dev->driver_data;
	uint32_t status = FTM_GetStatusFlags(config->base);
	int i;

	for (i = 0; i < config->channel_count;) {
		bool is_first_set = status & (kFTM_Chnl0Flag << i);

		if (!is_first_set) {
			i++;
			continue;
		}

		if (data->callbacks[i]) {
			data->callbacks[i](dev, i, data->edges[i]);
		}

		if (is_single_edge(data->edges[i])) {
			FTM_ClearStatusFlags(config->base, (kFTM_Chnl0Flag << i));
			i++;
			continue;
		}

		if (is_dual_edge_base(i)) {
			bool is_next_set = status & (kFTM_Chnl0Flag << (i + 1));

			if (!is_next_set) {
				if (data->enabled_interrupts[i]) {
					FTM_DisableInterrupts(config->base,
							      kFTM_Chnl0InterruptEnable << i);
				}
			} else {
				uint32_t mask = (kFTM_Chnl0Flag << i) | (kFTM_Chnl0Flag << (i + 1));
				FTM_ClearStatusFlags(config->base, mask);
				if (data->enabled_interrupts[i]) {
					FTM_EnableInterrupts(config->base,
							     kFTM_Chnl0InterruptEnable << i);
				}
			}
			i += 2;
			continue;
		}

		i++;
	}

/* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
 * exception return operation might vector to incorrect interrupt 
 */
#if defined __CORTEX_M && (__CORTEX_M == 4U)
	__DSB();
#endif
}

static uint32_t ic_mcux_ftm_get_counter(struct device *dev)
{
	if (dev == NULL) {
		LOG_ERR("Invalid device");
		return 0;
	}

	const struct ic_mcux_ftm_config *config = dev->config_info;
	return FTM_GetCurrentTimerCount(config->base);
}

static int ic_mcux_ftm_set_channel(struct device *dev, uint8_t channel, uint8_t edge)
{
	const struct ic_mcux_ftm_config *config = dev->config_info;
	struct ic_mcux_ftm_data *data = dev->driver_data;
	bool dual_edge = false;
	ftm_input_capture_edge_t current_edge;
	ftm_input_capture_edge_t next_edge;

	if (dev == NULL) {
		LOG_ERR("Invalid device");
		return -ENOENT;
	}

	if (channel >= config->channel_count) {
		LOG_ERR("Invalid channel count");
		return -ENOTSUP;
	}

	switch (edge) {
	case IC_EDGE_SINGLE_RISING:
		dual_edge = false;
		current_edge = kFTM_RisingEdge;
		break;
	case IC_EDGE_SINGLE_FALLING:
		dual_edge = false;
		current_edge = kFTM_FallingEdge;
		break;
	case IC_EDGE_SINGLE_RISING_FALLING:
		dual_edge = false;
		current_edge = kFTM_RiseAndFallEdge;
		break;
	case IC_EDGE_DUAL_PULSE_RISING:
		dual_edge = true;
		current_edge = kFTM_RisingEdge;
		next_edge = kFTM_FallingEdge;
		break;
	case IC_EDGE_DUAL_PULSE_FALLING:
		dual_edge = true;
		current_edge = kFTM_FallingEdge;
		next_edge = kFTM_RisingEdge;
		break;
	case IC_EDGE_DUAL_PERIOD_RISING:
		dual_edge = true;
		current_edge = kFTM_RisingEdge;
		next_edge = kFTM_RisingEdge;
		break;
	case IC_EDGE_DUAL_PERIOD_FALLING:
		dual_edge = true;
		current_edge = kFTM_FallingEdge;
		next_edge = kFTM_FallingEdge;
		break;
	default:
		LOG_ERR("Invalid edge selection");
		return -ENOTSUP;
	}

	if (!dual_edge) {
		data->edges[channel] = edge;
		FTM_SetupInputCapture(config->base, channel, current_edge, config->filter);
	} else {
		uint8_t base = get_dual_edge_base(channel);
		ftm_dual_edge_capture_param_t edge_param;
		/* For dual edge capture, both n and n + 1 channels are used */
		data->edges[base] = edge;
		data->edges[base + 1] = edge;

		edge_param.mode = kFTM_Continuous;
		edge_param.currChanEdgeMode = current_edge;
		edge_param.nextChanEdgeMode = next_edge;
		FTM_SetupDualEdgeCapture(config->base, (base >> 1), &edge_param, config->filter);
	}

	if (!data->started) {
		FTM_StartTimer(config->base, config->clock_source);
		data->started = true;
	}

	return 0;
}

static uint32_t ic_mcux_ftm_get_value(struct device *dev, uint8_t channel)
{
	const struct ic_mcux_ftm_config *config = dev->config_info;
	struct ic_mcux_ftm_data *data = dev->driver_data;

	if (dev == NULL) {
		LOG_ERR("Invalid device");
		return 0;
	}

	if (channel >= config->channel_count) {
		LOG_ERR("Invalid channel count");
		return -ENOTSUP;
	}

	if (!is_dual_edge(data->edges[channel])) {
		return config->base->CONTROLS[channel].CnV;
	} else {
		uint8_t base = get_dual_edge_base(channel);
		uint32_t cn = (uint32_t)config->base->CONTROLS[base].CnV;
		uint32_t cn1 = config->base->CONTROLS[base + 1].CnV;
		return cn > cn1 ? UINT16_MAX - cn + cn1 : cn1 - cn;
	}

	return 0;
}

static uint32_t ic_mcux_ftm_get_frequency(struct device *dev)
{
	struct ic_mcux_ftm_data *data = dev->driver_data;

	if (dev == NULL) {
		LOG_ERR("Invalid device");
		return 0;
	}

	return data->ticks_per_sec;
}

static uint32_t ic_mcux_ftm_get_counter_maximum(struct device *dev)
{
	return UINT16_MAX;
}

static int ic_mcux_ftm_set_callback(struct device *dev, uint8_t channel,
				    input_capture_callback_t callback)
{
	const struct ic_mcux_ftm_config *config = dev->config_info;
	struct ic_mcux_ftm_data *data = dev->driver_data;

	if (channel >= config->channel_count) {
		LOG_ERR("Invalid channel count");
		return -ENOTSUP;
	}

	if (!is_dual_edge(data->edges[channel])) {
		data->callbacks[channel] = callback;
	} else {
		uint8_t base = get_dual_edge_base(channel);
		data->callbacks[base] = callback;
		data->callbacks[base + 1] = callback;
	}

	return 0;
}

static int ic_mcux_ftm_enable_interrupts(struct device *dev, uint8_t channel, bool enable)
{
	const struct ic_mcux_ftm_config *config = dev->config_info;
	struct ic_mcux_ftm_data *data = dev->driver_data;
	uint32_t channel_mask;
	uint32_t interrupt_mask;

	if (channel >= config->channel_count) {
		LOG_ERR("Invalid channel count");
		return -ENOTSUP;
	}

	interrupt_mask = kFTM_Chnl0InterruptEnable << channel;
	channel_mask = kFTM_Chnl0Flag << channel;
	data->enabled_interrupts[channel] = enable;

	if (is_dual_edge(data->edges[channel])) {
		uint8_t base = get_dual_edge_base(channel);
		interrupt_mask |= kFTM_Chnl0InterruptEnable << (base + 1);
		channel_mask |= kFTM_Chnl0Flag << (base + 1);
		data->enabled_interrupts[base + 1] = enable;
	}

	FTM_ClearStatusFlags(config->base, channel_mask);

	if (enable) {
		FTM_EnableInterrupts(config->base, interrupt_mask);
	} else {
		FTM_DisableInterrupts(config->base, interrupt_mask);
	}

	return 0;
}

static int ic_mcux_ftm_init(struct device *dev)
{
	const struct ic_mcux_ftm_config *config = dev->config_info;
	struct ic_mcux_ftm_data *data = dev->driver_data;
	ftm_config_t ftm_config;
	int i;

	data->started = false;

	if (config->channel_count > ARRAY_SIZE(data->edges)) {
		LOG_ERR("Invalid channel count");
		return -EINVAL;
	}

	clock_name_t clock_name;
	if (config->clock_source == kFTM_SystemClock) {
		clock_name = kCLOCK_CoreSysClk;
	} else if (config->clock_source == kFTM_FixedClock) {
		clock_name = kCLOCK_McgFixedFreqClk;
	} else {
		LOG_ERR("Not supported FTM clock");
		return -EINVAL;
	}

	data->ticks_per_sec = CLOCK_GetFreq(clock_name) >> config->prescale;
	LOG_DBG("ticks_per_sec %" PRIu32, data->ticks_per_sec);

	for (i = 0; i < config->channel_count; i++) {
		data->edges[i] = IC_EDGE_NONE;
	}

	/* Only initialise the timer here and start it when the specified channel is set. */
	FTM_GetDefaultConfig(&ftm_config);
	ftm_config.prescale = config->prescale;
	FTM_Init(config->base, &ftm_config);
	config->base->MOD = 0xFFFF; /* free-running mode */

	return 0;
}

static const struct input_capture_driver_api ic_mcux_ftm_driver_api = {
	.get_counter = ic_mcux_ftm_get_counter,
	.set_channel = ic_mcux_ftm_set_channel,
	.get_value = ic_mcux_ftm_get_value,
	.get_frequency = ic_mcux_ftm_get_frequency,
	.get_counter_maximum = ic_mcux_ftm_get_counter_maximum,
	.set_callback = ic_mcux_ftm_set_callback,
	.enable_interrupts = ic_mcux_ftm_enable_interrupts,
};

#define TO_FTM_PRESCALE_DIVIDE(val) _DO_CONCAT(kFTM_Prescale_Divide_, val)

#define IC_MCUX_FTM_DEVICE(n)                                                                      \
	static const struct ic_mcux_ftm_config ic_mcux_ftm_config_##n = {                          \
		.base = (FTM_Type *)DT_INST_REG_ADDR(n),                                           \
		.clock_source = DT_INST_PROP(n, clock_source),                                     \
		.prescale = TO_FTM_PRESCALE_DIVIDE(DT_INST_PROP(n, prescaler)),                    \
		.channel_count = FSL_FEATURE_FTM_CHANNEL_COUNTn((FTM_Type *)DT_INST_REG_ADDR(n)),  \
		.filter = DT_INST_PROP(n, glitch_filter),                                          \
	};                                                                                         \
                                                                                                   \
	static struct ic_mcux_ftm_data ic_mcux_ftm_data_##n;                                       \
                                                                                                   \
	int ic_mcux_ftm_init_##n(struct device *dev);                                              \
                                                                                                   \
	DEVICE_AND_API_INIT(ic_mcux_ftm_##n, DT_INST_LABEL(n), &ic_mcux_ftm_init_##n,              \
			    &ic_mcux_ftm_data_##n, &ic_mcux_ftm_config_##n, POST_KERNEL,           \
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &ic_mcux_ftm_driver_api);          \
                                                                                                   \
	int ic_mcux_ftm_init_##n(struct device *dev)                                               \
	{                                                                                          \
		ic_mcux_ftm_init(dev);                                                             \
		IRQ_CONNECT(DT_INST_IRQ(n, irq), DT_INST_IRQ(n, priority), ftm_irq_handler,        \
			    DEVICE_GET(ic_mcux_ftm_##n), 0);                                       \
		irq_enable(DT_INST_IRQ(n, irq));                                                   \
		return 0;                                                                          \
	}

DT_INST_FOREACH_STATUS_OKAY(IC_MCUX_FTM_DEVICE)