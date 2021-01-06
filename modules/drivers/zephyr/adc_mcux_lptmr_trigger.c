#define DT_DRV_COMPAT nxp_kinetis_lptmr_trigger

#include <adc_trigger.h>
#include <fsl_lptmr.h>
#include <fsl_clock.h>
#include <fsl_rcm.h>
#include <drivers/clock_control.h>

#define LOG_LEVEL CONFIG_LPTMR_TRIGGER_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(lptmr_trigger);

struct lptmr_trigger_config {
    LPTMR_Type *base;
    lptmr_prescaler_clock_select_t clock_source;
    lptmr_prescaler_glitch_value_t prescale;
    uint32_t period;
};

struct lptmr_trigger_data {
    uint32_t period;
    uint32_t ticks_per_sec;
};

static void lptmr_trigger_irq_handler(void *arg)
{
    struct device *dev = arg;
    const struct lptmr_trigger_config *config = dev->config_info;
    LPTMR_ClearStatusFlags(config->base, kLPTMR_TimerCompareFlag);
/* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
 * exception return operation might vector to incorrect interrupt 
 */
#if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
#endif
}

static int lptmr_trigger_start(struct device *dev)
{
    const struct lptmr_trigger_config *config = dev->config_info;
    struct lptmr_trigger_data *data = dev->driver_data;
    LPTMR_SetTimerPeriod(config->base, data->period);
    LPTMR_StartTimer(config->base);

    return 0;
}

static int lptmr_trigger_stop(struct device *dev)
{
    const struct lptmr_trigger_config *config = dev->config_info;
    LPTMR_StopTimer(config->base);
    return 0;
}

static int lptmr_trigger_set_interval(struct device *dev, uint32_t us)
{
    struct lptmr_trigger_data *data = dev->driver_data;

    uint32_t period = (((uint64_t)us * (uint64_t)data->ticks_per_sec) / (uint64_t)1e6);
    if (period < 2) {
        LOG_ERR("LPTMR period too small: %" PRIu32 "", period);
        return -EINVAL;
    }
    data->period = period;

    return 0;
}

static uint32_t lptmr_trigger_get_interval(struct device *dev)
{
    struct lptmr_trigger_data *data = dev->driver_data;
    return (1e6 * (uint64_t)data->period)/data->ticks_per_sec;
}


static int lptmr_trigger_init(struct device *dev)
{
    const struct lptmr_trigger_config *config = dev->config_info;
    struct lptmr_trigger_data *data = dev->driver_data;

    clock_name_t clock_name;
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

    data->ticks_per_sec = CLOCK_GetFreq(clock_name) >> (config->prescale + 1);
    data->period = config->period;

	lptmr_config_t lptmr_config;
    lptmr_config.timerMode = kLPTMR_TimerModeTimeCounter;
    lptmr_config.pinSelect = kLPTMR_PinSelectInput_0;
    lptmr_config.pinPolarity = kLPTMR_PinPolarityActiveHigh;
    lptmr_config.enableFreeRunning = false;
    lptmr_config.bypassPrescaler = false;
    lptmr_config.prescalerClockSource = config->clock_source;
    lptmr_config.value = config->prescale;

    LPTMR_Init(config->base, &lptmr_config);
    LPTMR_DisableInterrupts(config->base, kLPTMR_TimerInterruptEnable);
    LPTMR_SetTimerPeriod(config->base, data->period);

    return 0;
}

#define LPTMR_TRIGGER(n)                                                            \
    static struct lptmr_trigger_config lptmr_trigger_config_##n = {                 \
        .base = (LPTMR_Type *)DT_INST_REG_ADDR(n),                                  \
        .clock_source = DT_INST_PROP(n, clk_source),                                \
        .prescale = DT_INST_PROP(n, prescaler),                                     \
        .period = DT_INST_PROP(n, period),                                          \
    };                                                                              \
                                                                                    \
    static struct lptmr_trigger_data lptmr_trigger_data_##n;                        \
                                                                                    \
    static const struct adc_trigger_driver_api lptmr_trigger_driver_api_##n = {     \
        .start = lptmr_trigger_start,                                               \
        .stop = lptmr_trigger_stop,                                                 \
        .get_interval = lptmr_trigger_get_interval,                                 \
        .set_interval = lptmr_trigger_set_interval,                                 \
    };                                                                              \
                                                                                    \
    static int lptmr_trigger_init_##n(struct device *dev);                          \
                                                                                    \
    DEVICE_AND_API_INIT(lptmr_trigger_##n,                                          \
                        DT_INST_LABEL(n),                                           \
                        &lptmr_trigger_init_##n,                                    \
                        &lptmr_trigger_data_##n,                                    \
                        &lptmr_trigger_config_##n,                                  \
                        POST_KERNEL,                                                \
                        CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                         \
                        &lptmr_trigger_driver_api_##n);                             \
                                                                                    \
    static int lptmr_trigger_init_##n(struct device *dev)                           \
    {                                                                               \
        lptmr_trigger_init(dev);                                                    \
        IRQ_CONNECT(DT_INST_IRQ_BY_IDX(n, 0, irq),                                  \
                    DT_INST_IRQ_BY_IDX(n, 0, priority),                             \
                    lptmr_trigger_irq_handler,                                      \
                    DEVICE_GET(lptmr_trigger_##n), 0);                              \
        irq_enable(DT_INST_IRQ_BY_IDX(n, 0, irq));                                  \
        return 0;                                                                   \
    }                                                                               

DT_INST_FOREACH_STATUS_OKAY(LPTMR_TRIGGER)