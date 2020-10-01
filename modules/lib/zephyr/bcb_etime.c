#include <bcb_etime.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/counter_ctd.h>
#include <counter_ctd_mcux_pit.h>

#define LOG_LEVEL CONFIG_BCB_LIB_ETIME_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_etime);

struct bcb_etime_data {
    struct device*  dev_cnt_ctd;
};

static struct bcb_etime_data bcb_etime_data;

static int bcb_etime_init()
{
    bcb_etime_data.dev_cnt_ctd = device_get_binding(DT_LABEL(DT_NODELABEL(pit0)));
    if (bcb_etime_data.dev_cnt_ctd == NULL) {
        LOG_ERR("Could not get counter_ctd device");
        return -EINVAL;
    }

    uint32_t clock_freq = counter_ctd_get_frequency(bcb_etime_data.dev_cnt_ctd);
    uint32_t prescale = clock_freq / CONFIG_BCB_LIB_ETIME_SECOND;
    prescale = prescale ? prescale : 2; /* should be at least two */

    LOG_DBG("prescale %" PRIu32, prescale);

    /* We use channel 0 is used as a prescaller */
    counter_ctd_set_top_value(bcb_etime_data.dev_cnt_ctd, 0, prescale - 1);
    counter_ctd_set_top_value(bcb_etime_data.dev_cnt_ctd, 1, UINT32_MAX);
    counter_ctd_set_top_value(bcb_etime_data.dev_cnt_ctd, 2, UINT32_MAX);
    /* timer1 is chained with timer0 and timer2 is chained with timer1. */
    counter_ctd_mcux_pit_chain(bcb_etime_data.dev_cnt_ctd, 1, true);
    counter_ctd_mcux_pit_chain(bcb_etime_data.dev_cnt_ctd, 2, true);

    counter_ctd_start(bcb_etime_data.dev_cnt_ctd, 2);
    counter_ctd_start(bcb_etime_data.dev_cnt_ctd, 1);
    counter_ctd_start(bcb_etime_data.dev_cnt_ctd, 0);

    return 0;
}

uint64_t bcb_etime_get_now()
{
    uint32_t ctd_l = UINT32_MAX - counter_ctd_get_value(bcb_etime_data.dev_cnt_ctd, 1); 
    uint32_t ctd_h = UINT32_MAX - counter_ctd_get_value(bcb_etime_data.dev_cnt_ctd, 2);

    return (uint64_t)ctd_l + ((uint64_t)ctd_h << 32);;
}

SYS_INIT(bcb_etime_init, APPLICATION, CONFIG_BCB_LIB_ETIME_INIT_PRIORITY);