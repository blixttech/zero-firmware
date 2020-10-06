#define DT_DRV_COMPAT nxp_kinetis_cmp

#include <cmp_mcux.h>
#include <fsl_cmp.h>

//#define LOG_LEVEL CONFIG_CMP_MCUX_LOG_LEVEL
#define LOG_LEVEL   LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(cmp_mcux);

struct cmp_mcux_config {
    CMP_Type* base;
    cmp_hysteresis_mode_t hysteresis;
    uint8_t filter_count;
    uint8_t filter_period;
};

struct cmp_mcux_data {
    cmp_config_t cmp_config;
    cmp_mcux_callback_t callback;
};

static void cmp_mcux_irq_handler(void *arg)
{
    struct device* dev = (struct device *)arg;
    const struct cmp_mcux_config* config = dev->config_info;
    struct cmp_mcux_data* data = dev->driver_data;
    if (data->callback != NULL) {
        data->callback(dev, ((CMP_GetStatusFlags(config->base) & kCMP_OutputAssertEventFlag) != 0));
    }
    CMP_ClearStatusFlags(config->base, (kCMP_OutputRisingEventFlag | kCMP_OutputFallingEventFlag));

/* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
 * exception return operation might vector to incorrect interrupt 
 */
#if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
#endif
}

static int cmp_mcux_set_enable_impl(struct device* dev, bool status)
{
    const struct cmp_mcux_config* config = dev->config_info;
    CMP_Enable(config->base, status);
    return 0;
}

static int cmp_mcux_set_channels_impl(struct device* dev, int chp, int chn)
{
    const struct cmp_mcux_config* config = dev->config_info;
    if (chp > 7 || chn > 7) {
        LOG_ERR("Input channel should be < 7");
        return -EINVAL;
    }
    CMP_SetInputChannels(config->base, chp, chn);
    return 0;
}

static int cmp_mcux_enable_interrupts_impl(struct device* dev, int interrupts)
{
    const struct cmp_mcux_config* config = dev->config_info;
    CMP_EnableInterrupts(config->base, interrupts);
    return 0;
}

static int cmp_mcux_disable_interrupts_impl(struct device* dev, int interrupts)
{
    const struct cmp_mcux_config* config = dev->config_info;
    CMP_DisableInterrupts(config->base, interrupts);
    return 0;
}

static int cmp_mcux_set_callback_impl(struct device* dev, cmp_mcux_callback_t callback)
{
    struct cmp_mcux_data* data = dev->driver_data;
    data->callback = callback;
    return 0;
}

static int cmp_mcux_init(struct device* dev)
{
    const struct cmp_mcux_config* config = dev->config_info;
    struct cmp_mcux_data* data = dev->driver_data;
    /*
     * mCmpConfigStruct.enableCmp = true;
     * mCmpConfigStruct.hysteresisMode = kCMP_HysteresisLevel0;
     * mCmpConfigStruct.enableHighSpeed = false;
     * mCmpConfigStruct.enableInvertOutput = false;
     * mCmpConfigStruct.useUnfilteredOutput = false;
     * mCmpConfigStruct.enablePinOut = false;
     * mCmpConfigStruct.enableTriggerMode = false;
     */
    CMP_GetDefaultConfig(&data->cmp_config);
    data->cmp_config.enableCmp = false;
    data->cmp_config.enableHighSpeed = true;
    data->cmp_config.hysteresisMode = config->hysteresis;

    CMP_Init(config->base, &data->cmp_config);

    if (config->filter_count > 7) {
        LOG_ERR("Filter count should be < 7");
        return -EINVAL;
    }

    cmp_filter_config_t filter_cfg;
    filter_cfg.enableSample = true;
    filter_cfg.filterCount = config->filter_count;
    filter_cfg.filterPeriod = config->filter_period;

    CMP_SetFilterConfig(config->base, &filter_cfg);

    return 0;
}

static const struct cmp_mcux_driver_api cmp_mcux_driver_api = {
    .set_enable = cmp_mcux_set_enable_impl,
    .set_channels = cmp_mcux_set_channels_impl,
    .enable_interrupts = cmp_mcux_enable_interrupts_impl,
    .disable_interrupts = cmp_mcux_disable_interrupts_impl,
    .set_callback = cmp_mcux_set_callback_impl,
};

#define CMP_MCUX_DEVICE(n)                                                                          \
    static struct cmp_mcux_config cmp_mcux_config_##n = {                                           \
        .base = (CMP_Type *)DT_INST_REG_ADDR(n),                                                    \
        .hysteresis = DT_INST_PROP(n, hysteresis),                                                  \
        .filter_count = DT_INST_PROP(n, filter_count),                                              \
        .filter_period = DT_INST_PROP(n, filter_period),                                            \
    };                                                                                              \
                                                                                                    \
    static struct cmp_mcux_data cmp_mcux_data_##n;                                                  \
                                                                                                    \
    static int cmp_mcux_init_##n(struct device* dev);                                               \
    DEVICE_AND_API_INIT(cmp_mcux_##n,                                                               \
                        DT_INST_LABEL(n),                                                           \
                        &cmp_mcux_init_##n,                                                         \
                        &cmp_mcux_data_##n,                                                         \
                        &cmp_mcux_config_##n,                                                       \
                        POST_KERNEL,                                                                \
                        CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                                         \
                        &cmp_mcux_driver_api);                                                      \
                                                                                                    \
    static int cmp_mcux_init_##n(struct device* dev)                                                \
    {                                                                                               \
        cmp_mcux_init(dev);                                                                         \
        IRQ_CONNECT(DT_INST_IRQ(n, irq),                                                            \
                    DT_INST_IRQ(n, priority),                                                       \
                    cmp_mcux_irq_handler, DEVICE_GET(cmp_mcux_##n), 0);                             \
        irq_enable(DT_INST_IRQ(n, irq));                                                            \
        return 0;                                                                                   \
    }

DT_INST_FOREACH_STATUS_OKAY(CMP_MCUX_DEVICE)