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

#define LOG_LEVEL CONFIG_INPUT_CAPTURE_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(input_capture_mcux_ftm);

#define MAX_CHANNELS    ARRAY_SIZE(FTM0->CONTROLS)

struct ic_mcux_ftm_config {
	FTM_Type* base;
	char* clock_name;
	clock_control_subsys_t clock_subsys;
	ftm_clock_source_t ftm_clock_source;
	ftm_clock_prescale_t prescale;
	u8_t channel_count;
};

struct ic_mcux_ftm_data {
    bool started;
    u32_t ticks_per_sec;
	u32_t edge[MAX_CHANNELS];
};

#if 0
/* TODO: Implement input capture interrupts */
static void ftm3_irq_handler(void* arg)
{
}
#endif

static u32_t ic_mcux_ftm_get_counter(struct device* dev)
{
    if (dev == NULL) {
        LOG_ERR("Invalid device");
        return 0;
    }

    const struct ic_mcux_ftm_config* config = dev->config_info;
    return FTM_GetCurrentTimerCount(config->base);
}

static int ic_mcux_ftm_set_channel(struct device* dev, u32_t channel, u32_t edge)
{
    if (dev == NULL) {
        LOG_ERR("Invalid device");
        return -ENOENT;
    }

    const struct ic_mcux_ftm_config* config = dev->config_info;
    struct ic_mcux_ftm_data* data = dev->driver_data;

    if (channel >= config->channel_count) {
        LOG_ERR("Invalid channel count");
        return -ENOTSUP;
    }

    if (edge == IC_EDGE_SINGLE_RISING) {
        FTM_SetupInputCapture(config->base, channel, kFTM_RisingEdge, 0);
    } else if (edge == IC_EDGE_SINGLE_FALLING) {
        FTM_SetupInputCapture(config->base, channel, kFTM_FallingEdge, 0);
    } else if (edge == IC_EDGE_SINGLE_RISING_FALLING) {
        FTM_SetupInputCapture(config->base, channel, kFTM_RiseAndFallEdge, 0);
    } else {
        if (channel >= config->channel_count - 1) {
            LOG_ERR("Invalid channel count for dual edge capture");
            return -ENOTSUP;
        }

        ftm_dual_edge_capture_param_t edge_param;
        /* Always run in the continoues mode. */
        edge_param.mode = kFTM_Continuous;

        if (edge == IC_EDGE_DUAL_PULSE_RISING) {
            edge_param.currChanEdgeMode = kFTM_RisingEdge;
            edge_param.nextChanEdgeMode = kFTM_FallingEdge;
        } else if (edge == IC_EDGE_DUAL_PULSE_FALLING) {
            edge_param.currChanEdgeMode = kFTM_FallingEdge;
            edge_param.nextChanEdgeMode = kFTM_RisingEdge;
        } else if (edge == IC_EDGE_DUAL_PERIOD_RISING) {
            edge_param.currChanEdgeMode = kFTM_RisingEdge;
            edge_param.nextChanEdgeMode = kFTM_RisingEdge;
        } else if (edge == IC_EDGE_DUAL_PERIOD_FALLING) {
            edge_param.currChanEdgeMode = kFTM_FallingEdge;
            edge_param.nextChanEdgeMode = kFTM_FallingEdge;
        } else {
            LOG_ERR("Invalid edge selection");
            return -ENOTSUP;
        }

        FTM_SetupDualEdgeCapture(config->base, channel, &edge_param, 0);
    }

    data->edge[channel] = edge;
    
    if (!data->started) {
        FTM_StartTimer(config->base, config->ftm_clock_source);
        data->started = true;
    }

    return 0;
}

static u32_t ic_mcux_ftm_get_value(struct device* dev, u32_t channel)
{
    if (dev == NULL) {
        LOG_ERR("Invalid device");
        return 0;
    }

    const struct ic_mcux_ftm_config* config = dev->config_info;
    if (channel >= config->channel_count) {
        LOG_ERR("Invalid channel count");
        return -ENOTSUP;
    }
    
    return config->base->CONTROLS[channel].CnV;
}

static int ic_mcux_ftm_init(struct device* dev)
{
    const struct ic_mcux_ftm_config* config = dev->config_info;
	struct ic_mcux_ftm_data* data = dev->driver_data;
	struct device* clock_dev;
	ftm_config_t ftm_config;
    u32_t clock_freq;
    int i;

    data->started = false;

    if (config->channel_count > ARRAY_SIZE(data->edge)) {
        LOG_ERR("Invalid channel count");
        return -EINVAL;
    }

	clock_dev = device_get_binding(config->clock_name);
    if (clock_dev == NULL) {
        LOG_ERR("Could not get clock device");
        return -EINVAL;
    }

	if (clock_control_get_rate(clock_dev, config->clock_subsys, &clock_freq)) {
        LOG_ERR("Could not get clock frequency");
        return -EINVAL;
	}

    data->ticks_per_sec = clock_freq >> config->prescale;

    for (i = 0; i < config->channel_count; i++) {
        data->edge[i] = IC_EDGE_NONE;
    }

    /* Only initialise the timer here and start it when the specified channel is set. */
    FTM_GetDefaultConfig(&ftm_config);
    ftm_config.prescale = config->prescale;
    FTM_Init(config->base, &ftm_config);
    config->base->MOD = 0xFFFF; /* free-running mode */

#if 0
    /* TODO: Implement input capture interrupts */
    IRQ_CONNECT(DT_IRQ(DT_NODELABEL(ftm3), irq), 
                DT_IRQ(DT_NODELABEL(ftm3), priority), 
                ftm3_irq_handler, 
                NULL, 
                0);
    irq_enable(DT_IRQ(DT_NODELABEL(ftm3), irq));
#endif

	return 0;
}

static const struct input_capture_driver_api ic_mcux_ftm_driver_api = {
    .get_counter = ic_mcux_ftm_get_counter,
    .set_channel = ic_mcux_ftm_set_channel,
    .get_value = ic_mcux_ftm_get_value,
};

#define TO_FTM_PRESCALE_DIVIDE(val) _DO_CONCAT(kFTM_Prescale_Divide_, val)

#define IC_MCUX_FTM_DEVICE(n)                                                       \
    static const struct ic_mcux_ftm_config ic_mcux_ftm_config_##n = {               \
        .base = (FTM_Type *)DT_INST_REG_ADDR(n),                                    \
        .clock_name = DT_INST_CLOCKS_LABEL(n),                                      \
        .clock_subsys = (clock_control_subsys_t) DT_INST_CLOCKS_CELL(n, name),      \
        .ftm_clock_source = kFTM_SystemClock,                                        \
        .prescale = TO_FTM_PRESCALE_DIVIDE(DT_INST_PROP(n, prescaler)),             \
        .channel_count = FSL_FEATURE_FTM_CHANNEL_COUNTn((FTM_Type *)                \
                                                        DT_INST_REG_ADDR(n)),       \
    };                                                                              \
    static struct ic_mcux_ftm_data ic_mcux_ftm_data_##n;                            \
    DEVICE_AND_API_INIT(ic_mcux_ftm_##n,                                            \
                        DT_INST_LABEL(n),                                           \
                        &ic_mcux_ftm_init,                                          \
                        &ic_mcux_ftm_data_##n,                                      \
                        &ic_mcux_ftm_config_##n,                                    \
                        POST_KERNEL,                                                \
                        CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                         \
                        &ic_mcux_ftm_driver_api);


DT_INST_FOREACH_STATUS_OKAY(IC_MCUX_FTM_DEVICE)