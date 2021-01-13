/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_kinetis_pit_ctd_counter

#include <drivers/clock_control.h>
#include <drivers/counter_ctd.h>
#include <drivers/counter_ctd_mcux_pit.h>
#include <errno.h>
#include <soc.h>
#include <fsl_pit.h>
#include <fsl_clock.h>

#define LOG_LEVEL CONFIG_COUNTER_CTD_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(counter_ctd_mcux_pit);

#define MAX_CHANNELS    ARRAY_SIZE(PIT->CHANNEL)

struct counter_ctd_mcux_pit_config {
    PIT_Type *base;
    char *clock_name;
    clock_control_subsys_t clock_subsys;
    bool enable_in_debug;
};

struct counter_ctd_alarm_data {
    struct device *dev;
    ctd_alarm_callback_t callback;
    void* user_data;
    uint8_t chan_id;
};

struct counter_ctd_mcux_pit_data {
    struct counter_ctd_alarm_data alarm_data[MAX_CHANNELS];
};

static void pit_irq_handler(void *arg)
{
    struct counter_ctd_alarm_data *data = (struct counter_ctd_alarm_data*)arg;
    const struct counter_ctd_mcux_pit_config *config = data->dev->config_info;

    PIT_ClearStatusFlags(config->base, data->chan_id, kPIT_TimerFlag);
    /* A note from NXP's SDK:
     * Added for, and affects, all PIT handlers. For CPU clock which is much larger than the IP bus clock,
     * CPU can run out of the interrupt handler before the interrupt flag being cleared, resulting in the
     * CPU's entering the handler again and again. Adding DSB can prevent the issue from happening.
     */
    __DSB();

    if (data->callback) {
        data->callback(data->dev, data->chan_id, data->user_data);
    }
}

static int counter_ctd_mcux_pit_start(struct device *dev, uint8_t chan_id)
{
    if (chan_id >= MAX_CHANNELS) {
        LOG_ERR("Invalid channel id");
        return -ENOTSUP;
    }

    const struct counter_ctd_mcux_pit_config *config = dev->config_info;
    PIT_StartTimer(config->base, chan_id);

    return 0;
}

static int counter_ctd_mcux_pit_stop(struct device *dev, uint8_t chan_id)
{
    if (chan_id >= MAX_CHANNELS) {
        LOG_ERR("Invalid channel id");
        return -ENOTSUP;
    }

    const struct counter_ctd_mcux_pit_config *config = dev->config_info;
    PIT_StopTimer(config->base, chan_id);

    return 0;
}

static uint32_t counter_ctd_mcux_pit_get_value(struct device *dev, uint8_t chan_id)
{
    if (chan_id >= MAX_CHANNELS) {
        LOG_ERR("Invalid channel id");
        return 0;
    }

    const struct counter_ctd_mcux_pit_config *config = dev->config_info;
    return PIT_GetCurrentTimerCount(config->base, chan_id);
}

static int counter_ctd_mcux_pit_set_top_value(struct device *dev, uint8_t chan_id, uint32_t ticks)
{
    if (chan_id >= MAX_CHANNELS) {
        LOG_ERR("Invalid channel id");
        return -ENOTSUP;
    }

    const struct counter_ctd_mcux_pit_config *config = dev->config_info;
    PIT_SetTimerPeriod(config->base, chan_id, ticks);

    return 0;
}

static uint32_t counter_ctd_mcux_pit_get_top_value(struct device *dev, uint8_t chan_id)
{
    if (chan_id >= MAX_CHANNELS) {
        LOG_ERR("Invalid channel id");
        return 0;
    }

    const struct counter_ctd_mcux_pit_config *config = dev->config_info;
    return config->base->CHANNEL[chan_id].LDVAL;
}

static int counter_ctd_mcux_pit_set_alarm(struct device *dev, uint8_t chan_id, 
                                            const struct ctd_alarm_cfg *cfg)
{
    if (chan_id >= MAX_CHANNELS) {
        LOG_ERR("Invalid channel id");
        return -ENOTSUP;
    }

    const struct counter_ctd_mcux_pit_config *config = dev->config_info;
    struct counter_ctd_mcux_pit_data *data = dev->driver_data;
    data->alarm_data[chan_id].callback = cfg->callback;
    data->alarm_data[chan_id].user_data = cfg->user_data;

    PIT_EnableInterrupts(config->base, chan_id, kPIT_TimerInterruptEnable);

    return 0;
}

static int counter_ctd_mcux_pit_cancel_alarm(struct device *dev, uint8_t chan_id)
{
    if (chan_id >= MAX_CHANNELS) {
        LOG_ERR("Invalid channel id");
        return -ENOTSUP;
    }

    const struct counter_ctd_mcux_pit_config *config = dev->config_info;
    PIT_DisableInterrupts(config->base, chan_id, kPIT_TimerInterruptEnable);

    return 0;
}

static uint32_t counter_ctd_mcux_pit_get_frequency(struct device *dev)
{
    const struct counter_ctd_mcux_pit_config *config = dev->config_info;
    struct device *clock_dev;
    uint32_t clock_freq = 0;

    clock_dev = device_get_binding(config->clock_name);
    if (clock_dev == NULL) {
        LOG_ERR("Could not get clock device");
        return 0;
    }

    if (clock_control_get_rate(clock_dev, config->clock_subsys, &clock_freq)) {
        LOG_ERR("Could not get clock frequency");
        return 0;
    }

    return clock_freq;
}

int z_impl_counter_ctd_mcux_pit_chain(struct device *dev, uint8_t chan_id, bool enable)
{
    /* As in the datasheet timer 0 cannot be chained */
    if (chan_id >= MAX_CHANNELS || chan_id == 0) {
        LOG_ERR("Invalid channel id");
        return -ENOTSUP;
    }

    const struct counter_ctd_mcux_pit_config *config = dev->config_info;
    PIT_SetTimerChainMode(config->base, chan_id, enable);

    return 0;
}

#define MCUX_PIT_INIT_IRQ(n, channel)                                           \
    do {                                                                        \
        IRQ_CONNECT(DT_INST_IRQ_BY_IDX(n, channel, irq),                        \
                    DT_INST_IRQ_BY_IDX(n, channel, priority),                   \
                    pit_irq_handler,                                            \
                    &mcux_pit_data_##n.alarm_data[channel],                     \
                    0);                                                         \
        irq_enable(DT_INST_IRQ_BY_IDX(n, channel, irq));                        \
    } while (0)

#define COUNTER_CTD_MCUX_PIT_DEVICE(n)                                                      \
    static struct counter_ctd_mcux_pit_config mcux_pit_config_##n = {                       \
        .base = (PIT_Type *)DT_INST_REG_ADDR(n),                                            \
        .clock_name = DT_INST_CLOCKS_LABEL(n),                                              \
        .clock_subsys = (clock_control_subsys_t)DT_INST_CLOCKS_CELL(n, name),               \
        .enable_in_debug = true,                                                            \
    };                                                                                      \
                                                                                            \
    static struct counter_ctd_mcux_pit_data mcux_pit_data_##n;                              \
                                                                                            \
    static int counter_ctd_mcux_pit_init_##n(struct device *dev)                            \
    {                                                                                       \
        const struct counter_ctd_mcux_pit_config* config = dev->config_info;                \
        pit_config_t pit_cfg;                                                               \
        PIT_GetDefaultConfig(&pit_cfg);                                                     \
        pit_cfg.enableRunInDebug = config->enable_in_debug;                                 \
        PIT_Init(config->base, &pit_cfg);                                                   \
        MCUX_PIT_INIT_IRQ(n, 0);                                                            \
        MCUX_PIT_INIT_IRQ(n, 1);                                                            \
        MCUX_PIT_INIT_IRQ(n, 2);                                                            \
        MCUX_PIT_INIT_IRQ(n, 3);                                                            \
        return 0;                                                                           \
    }                                                                                       \
                                                                                            \
    static const struct counter_ctd_driver_api counter_ctd_mcux_pit_driver_api_##n = {      \
        .start = counter_ctd_mcux_pit_start,                                                \
        .stop = counter_ctd_mcux_pit_stop,                                                  \
        .get_value = counter_ctd_mcux_pit_get_value,                                        \
        .set_top_value = counter_ctd_mcux_pit_set_top_value,                                \
        .get_top_value = counter_ctd_mcux_pit_get_top_value,                                \
        .set_alarm = counter_ctd_mcux_pit_set_alarm,                                        \
        .cancel_alarm = counter_ctd_mcux_pit_cancel_alarm,                                  \
        .get_frequency = counter_ctd_mcux_pit_get_frequency,                                \
    };                                                                                      \
                                                                                            \
    DEVICE_AND_API_INIT(counter_ctd_mcux_pit_##n,                                           \
                        DT_INST_LABEL(n),                                                   \
                        &counter_ctd_mcux_pit_init_##n,                                     \
                        &mcux_pit_data_##n,                                                 \
                        &mcux_pit_config_##n,                                               \
                        POST_KERNEL,                                                        \
                        CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                                 \
                        &counter_ctd_mcux_pit_driver_api_##n);

DT_INST_FOREACH_STATUS_OKAY(COUNTER_CTD_MCUX_PIT_DEVICE)