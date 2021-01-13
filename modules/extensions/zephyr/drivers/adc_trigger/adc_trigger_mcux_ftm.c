#include <stdint.h>
#define DT_DRV_COMPAT nxp_kinetis_ftm_trigger

#include <drivers/adc_trigger.h>
#include <fsl_ftm.h>
#include <fsl_clock.h>
#include <fsl_rcm.h>
#include <drivers/clock_control.h>

#define LOG_LEVEL CONFIG_ADC_FTM_TRIGGER_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(adc_ftm_trigger);

struct adc_ftm_trigger_config {
    FTM_Type *base;
    ftm_clock_source_t clock_source;
    ftm_clock_prescale_t prescale;
    uint8_t channel;

};

struct adc_ftm_trigger_data {
    uint32_t period;
    uint32_t ticks_per_sec;
};

static int adc_ftm_trigger_start(struct device *dev)
{
    const struct adc_ftm_trigger_config *config = dev->config_info;
    struct adc_ftm_trigger_data *data = dev->driver_data;
    FTM_SetTimerPeriod(config->base, data->period);
    FTM_StartTimer(config->base, config->clock_source);

    return 0;
}

static int adc_ftm_trigger_stop(struct device *dev)
{
    const struct adc_ftm_trigger_config *config = dev->config_info;
    FTM_StopTimer(config->base);
    return 0;
}

static int adc_ftm_trigger_set_interval(struct device *dev, uint32_t us)
{
    struct adc_ftm_trigger_data *data = dev->driver_data;

    uint32_t period = (((uint64_t)us * (uint64_t)data->ticks_per_sec) / (uint64_t)1e6);
    if (period < 2) {
        LOG_ERR("FTM period too small: %" PRIu32 "", period);
        return -EINVAL;
    }
    data->period = period;

    return 0;
}

static uint32_t adc_ftm_trigger_get_interval(struct device *dev)
{
    struct adc_ftm_trigger_data *data = dev->driver_data;
    return (1e6 * (uint64_t)data->period)/data->ticks_per_sec;
}

static inline uint32_t adc_ftm_trigger_exttrig(uint8_t channel)
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


static int adc_ftm_trigger_init(struct device *dev)
{
    const struct adc_ftm_trigger_config *config = dev->config_info;
    struct adc_ftm_trigger_data *data = dev->driver_data;

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

    ftm_config_t ftm_config;
    FTM_GetDefaultConfig(&ftm_config);
    ftm_config.prescale = config->prescale;
    ftm_config.extTriggers = adc_ftm_trigger_exttrig(config->channel);
    FTM_Init(config->base, &ftm_config);
    FTM_SetupOutputCompare(config->base, config->channel, kFTM_NoOutputSignal, 0);

    data->period = UINT16_MAX;

    return 0;
}

#define ADC_FTM_TRIGGER(n)                                                                          \
    static struct adc_ftm_trigger_config adc_ftm_trigger_config_##n = {                             \
        .base = (FTM_Type *)DT_INST_REG_ADDR(n),                                                    \
        .clock_source = DT_INST_PROP(n, clock_source),                                              \
    };                                                                                              \
                                                                                                    \
    static struct adc_ftm_trigger_data adc_ftm_trigger_data_##n;                                    \
                                                                                                    \
    static const struct adc_trigger_driver_api adc_trigger_driver_api_##n = {                       \
        .start = adc_ftm_trigger_start,                                                             \
        .stop = adc_ftm_trigger_stop,                                                               \
        .get_interval = adc_ftm_trigger_get_interval,                                               \
        .set_interval = adc_ftm_trigger_set_interval,                                               \
    };                                                                                              \
                                                                                                    \
    DEVICE_AND_API_INIT(adc_ftm_trigger_##n,                                                        \
                        DT_INST_LABEL(n),                                                           \
                        &adc_ftm_trigger_init,                                                      \
                        &adc_ftm_trigger_data_##n,                                                  \
                        &adc_ftm_trigger_config_##n,                                                \
                        POST_KERNEL,                                                                \
                        CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                                         \
                        &adc_trigger_driver_api_##n);

DT_INST_FOREACH_STATUS_OKAY(ADC_FTM_TRIGGER)