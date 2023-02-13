#define DT_DRV_COMPAT nxp_kinetis_adc_trigger_ftm

#include <zephyr/drivers/adc_trigger.h>
#include <zephyr/drivers/clock_control.h>

#include <fsl_clock.h>
#include <fsl_ftm.h>

#define LOG_LEVEL CONFIG_ADC_TRIGGER_FTM_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(adc_trigger_ftm);

struct adc_trigger_ftm_config {
	FTM_Type *base;
	ftm_clock_source_t clock_source;
	ftm_clock_prescale_t clock_divider;
	uint8_t trigger_source;
};

struct adc_trigger_ftm_data {
	uint32_t period;
	uint32_t ticks_per_sec;
};

static int adc_trigger_ftm_start(const struct device *dev)
{
	const struct adc_trigger_ftm_config *config = dev->config;
	struct adc_trigger_ftm_data *data = dev->data;

	FTM_SetTimerPeriod(config->base, data->period);
	FTM_StartTimer(config->base, config->clock_source);

	return 0;
}

static int adc_trigger_ftm_stop(const struct device *dev)
{
	const struct adc_trigger_ftm_config *config = dev->config;

	FTM_StopTimer(config->base);
	return 0;
}

static int adc_trigger_ftm_set_interval(const struct device *dev, uint32_t us)
{
	struct adc_trigger_ftm_data *data = dev->data;

	uint32_t period = (((uint64_t)us * (uint64_t)data->ticks_per_sec) / (uint64_t)1e6) - 1U;
	if (period < 2) {
		LOG_ERR("FTM period too small: %" PRIu32 "", period);
		return -EINVAL;
	}
	data->period = period;
	LOG_DBG("period: %" PRIu32, data->period);

	return 0;
}

static uint32_t adc_trigger_ftm_get_interval(const struct device *dev)
{
	struct adc_trigger_ftm_data *data = dev->data;

	return ((uint64_t)1e9 * (uint64_t)(data->period + 1U)) / (uint64_t)data->ticks_per_sec;
}

static int adc_trigger_ftm_init(const struct device *dev)
{
	const struct adc_trigger_ftm_config *config = dev->config;
	struct adc_trigger_ftm_data *data = dev->data;
	ftm_config_t ftm_config;
	clock_name_t clock_name;

	if (config->clock_source == kFTM_SystemClock) {
		clock_name = kCLOCK_CoreSysClk;
	} else if (config->clock_source == kFTM_FixedClock) {
		clock_name = kCLOCK_McgFixedFreqClk;
	} else {
		LOG_ERR("Not supported FTM clock");
		return -EINVAL;
	}

	data->ticks_per_sec = CLOCK_GetFreq(clock_name) >> config->clock_divider;
	LOG_DBG("ticks/sec: %" PRIu32, data->ticks_per_sec);

	FTM_GetDefaultConfig(&ftm_config);
	ftm_config.prescale = config->clock_divider;
	ftm_config.extTriggers = config->trigger_source;
	FTM_Init(config->base, &ftm_config);

	data->period = UINT16_MAX;

	return 0;
}

static const struct adc_trigger_driver_api adc_trigger_ftm_api = {
	.start = adc_trigger_ftm_start,
	.stop = adc_trigger_ftm_stop,
	.get_interval = adc_trigger_ftm_get_interval,
	.set_interval = adc_trigger_ftm_set_interval,
};

#define ADC_FTM_TRIGGER(n)                                                                         \
	static struct adc_trigger_ftm_config adc_ftm_trigger_config_##n = {                        \
		.base = (FTM_Type *)DT_INST_REG_ADDR(n),                                           \
		.clock_source = DT_INST_PROP(n, clock_source),                                     \
		.clock_divider = DT_INST_PROP_OR(n, clk_divider, 0),                               \
		.trigger_source = DT_INST_PROP_OR(n, trigger_source, 0),                           \
	};                                                                                         \
                                                                                                   \
	static struct adc_trigger_ftm_data adc_trigger_ftm_data_##n;                               \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &adc_trigger_ftm_init, NULL, &adc_trigger_ftm_data_##n,           \
			      &adc_ftm_trigger_config_##n, POST_KERNEL, CONFIG_ADC_INIT_PRIORITY,  \
			      &adc_trigger_ftm_api);

DT_INST_FOREACH_STATUS_OKAY(ADC_FTM_TRIGGER)
