#include <init.h>
#include <autoconf.h>
#include <fsl_dmamux.h>
#include <fsl_edma.h>

static int dma_mcux_partial()
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

SYS_INIT(dma_mcux_partial, PRE_KERNEL_2, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);