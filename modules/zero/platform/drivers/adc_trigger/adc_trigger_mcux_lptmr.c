#define DT_DRV_COMPAT nxp_kinetis_adc_trigger_lptmr

#include <zephyr/drivers/adc_trigger.h>
#include <zephyr/drivers/clock_control.h>

#include <fsl_clock.h>
#include <fsl_lptmr.h>

#define LOG_LEVEL CONFIG_ADC_TRIGGER_LPTMR_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lptmr_trigger);

struct lptmr_trigger_config {
	LPTMR_Type *base;
	lptmr_prescaler_clock_select_t clock_source;
	lptmr_prescaler_glitch_value_t clock_divider;
};

struct lptmr_trigger_data {
	uint32_t period;
	uint32_t ticks_per_sec;
};

static int lptmr_trigger_start(const struct device *dev)
{
	const struct lptmr_trigger_config *config = dev->config;
	struct lptmr_trigger_data *data = dev->data;

	LPTMR_SetTimerPeriod(config->base, data->period);
	LPTMR_StartTimer(config->base);

	return 0;
}

static int lptmr_trigger_stop(const struct device *dev)
{
	const struct lptmr_trigger_config *config = dev->config;

	LPTMR_StopTimer(config->base);
	return 0;
}

static int lptmr_trigger_set_interval(const struct device *dev, uint32_t us)
{
	struct lptmr_trigger_data *data = dev->data;

	uint32_t period = (((uint64_t)us * (uint64_t)data->ticks_per_sec) / (uint64_t)1e6);
	if (period < 2) {
		LOG_ERR("LPTMR period too small: %" PRIu32 "", period);
		return -EINVAL;
	}
	data->period = period;
	LOG_DBG("period: %" PRIu32, data->period);

	return 0;
}

static uint32_t lptmr_trigger_get_interval(const struct device *dev)
{
	struct lptmr_trigger_data *data = dev->data;

	return (1e6 * (uint64_t)data->period) / data->ticks_per_sec;
}

static int adc_trigger_lptmr_init(const struct device *dev)
{
	const struct lptmr_trigger_config *config = dev->config;
	struct lptmr_trigger_data *data = dev->data;
	clock_name_t clock_name;
	lptmr_config_t lptmr_config;

	if (config->clock_source == 0) {
		clock_name = kCLOCK_McgInternalRefClk;
	} else if (config->clock_source == 1) {
		clock_name = kCLOCK_LpoClk;
	} else if (config->clock_source == 2) {
		clock_name = kCLOCK_Er32kClk;
	} else if (config->clock_source == 3) {
		clock_name = kCLOCK_Osc0ErClk;
	} else {
		LOG_ERR("Not supported LPTMR clock");
		return -EINVAL;
	}

	data->ticks_per_sec = CLOCK_GetFreq(clock_name) >> (config->clock_divider + 1);
	LOG_DBG("ticks/sec: %" PRIu32, data->ticks_per_sec);

	data->period = UINT16_MAX;

	/*
	 * lptmrConfig.timerMode = kLPTMR_TimerModeTimeCounter;
	 * lptmrConfig.pinSelect = kLPTMR_PinSelectInput_0;
	 * lptmrConfig.pinPolarity = kLPTMR_PinPolarityActiveHigh;
	 * lptmrConfig.enableFreeRunning = false;
	 * lptmrConfig.bypassPrescaler = true;
	 * lptmrConfig.prescalerClockSource = kLPTMR_PrescalerClock_1;
	 * lptmrConfig.value = kLPTMR_Prescale_Glitch_0;
	 */
	LPTMR_GetDefaultConfig(&lptmr_config);

	lptmr_config.enableFreeRunning = false;
	lptmr_config.bypassPrescaler = false;
	lptmr_config.prescalerClockSource = config->clock_source;
	lptmr_config.value = config->clock_divider;

	LPTMR_Init(config->base, &lptmr_config);
	LPTMR_DisableInterrupts(config->base, kLPTMR_TimerInterruptEnable);
	return 0;
}

static const struct adc_trigger_driver_api adc_trigger_lptmr_api = {
	.start = lptmr_trigger_start,
	.stop = lptmr_trigger_stop,
	.get_interval = lptmr_trigger_get_interval,
	.set_interval = lptmr_trigger_set_interval,
};

#define LPTMR_TRIGGER(n)                                                                           \
	static struct lptmr_trigger_config adc_trigger_lptmr_config_##n = {                        \
		.base = (LPTMR_Type *)DT_INST_REG_ADDR(n),                                         \
		.clock_source = DT_INST_PROP(n, clk_source),                                       \
		.clock_divider = DT_INST_PROP(n, clk_divider),                                     \
	};                                                                                         \
                                                                                                   \
	static struct lptmr_trigger_data adc_trigger_lptmr_data_##n;                               \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &adc_trigger_lptmr_init, NULL, &adc_trigger_lptmr_data_##n,       \
			      &adc_trigger_lptmr_config_##n, POST_KERNEL,                          \
			      CONFIG_ADC_INIT_PRIORITY, &adc_trigger_lptmr_api);

DT_INST_FOREACH_STATUS_OKAY(LPTMR_TRIGGER)