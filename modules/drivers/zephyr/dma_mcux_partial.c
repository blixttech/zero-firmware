#define DT_DRV_COMPAT nxp_kinetis_edma_partial

#include <stdint.h>
#include <device.h>
#include <fsl_dmamux.h>
#include <fsl_edma.h>

#define LOG_LEVEL CONFIG_INPUT_CAPTURE_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(dma_mcux_partial);

#if 0
struct dma_mcux_partial_config {
    DMA_Type* dma_base;
    DMAMUX_Type* dma_mux_base;
};

static int dma_mcux_partial_init(struct device* dev)
{
    const struct dma_mcux_partial_config* config = dev->config_info;

    DMAMUX_Init(config->dma_mux_base);

    edma_config_t user_config;
    /*
    * Configure EDMA one shot transfer
    * userConfig.enableRoundRobinArbitration = false;
    * userConfig.enableHaltOnError = true;
    * userConfig.enableContinuousLinkMode = false;
    * userConfig.enableDebugMode = false;
    */
    EDMA_GetDefaultConfig(&user_config);
    user_config.enableHaltOnError = false;
    EDMA_Init(config->dma_base, &user_config);

    return 0;
}

#define DMA_MCUX_PARTIAL_DEVICE(n)                                              \
    static struct dma_mcux_partial_config dma_mcux_config_##n = {               \
        .dma_base = (DMA_Type *)DT_INST_REG_ADDR_BY_NAME(n, DMA),               \
        .dma_mux_base = (DMAMUX_Type *)DT_INST_REG_ADDR_BY_NAME(n, MUX),        \
    };                                                                          \
    DEVICE_AND_API_INIT(dma_mcux_##n,                                           \
                        DT_INST_LABEL(n),                                       \
                        &dma_mcux_partial_init,                                 \
                        NULL,                                                   \
                        &dma_mcux_config_##n,                                   \
                        POST_KERNEL,                                            \
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                    \
                        NULL);

DT_INST_FOREACH_STATUS_OKAY(DMA_MCUX_PARTIAL_DEVICE)
#endif 

static int dma_mcux_partial_init()
{
    DMAMUX_Init(DMAMUX0);

    edma_config_t user_config;
    /*
    * Configure EDMA one shot transfer
    * userConfig.enableRoundRobinArbitration = false;
    * userConfig.enableHaltOnError = true;
    * userConfig.enableContinuousLinkMode = false;
    * userConfig.enableDebugMode = false;
    */
    EDMA_GetDefaultConfig(&user_config);
    user_config.enableHaltOnError = false;
    EDMA_Init(DMA0, &user_config);

    return 0;
}

SYS_INIT(dma_mcux_partial_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);