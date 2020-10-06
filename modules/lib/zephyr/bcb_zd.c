#include <bcb_zd.h>
#include <cmp_mcux.h>
#include <device.h>
#include <devicetree.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_zd);

#define BCB_INIT_ZD(ds, dt_node, name)                                                              \
do {                                                                                                \
    (ds).dev_cmp_##name = device_get_binding(                                                       \
                        DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(dt_node), comparators, name)));    \
    if ((ds).dev_cmp_##name == NULL) {                                                              \
        LOG_ERR("Unknown CMP device");                                                              \
    } else {                                                                                        \
        cmp_mcux_set_channels((ds).dev_cmp_##name,                                                  \
                            DT_PHA_BY_NAME(DT_NODELABEL(dt_node), comparators, name, inp),          \
                            DT_PHA_BY_NAME(DT_NODELABEL(dt_node), comparators, name, inn));         \
        cmp_mcux_enable_interrupts((ds).dev_cmp_##name,                                             \
                                DT_PHA_BY_NAME(DT_NODELABEL(dt_node), comparators, name, edge));    \
    }                                                                                               \
} while(0)


struct bcb_zd_data {
    struct device* dev_cmp_v_zd;
    struct device* dev_cmp_i_zd;
};

static struct bcb_zd_data bcb_zd_data = {
    .dev_cmp_v_zd = NULL,
    .dev_cmp_i_zd = NULL,
};

static void bcb_zd_on_v_zd(struct device* dev, bool status)
{
    LOG_DBG("V_ZD");
}

static void bcb_zd_on_i_zd(struct device* dev, bool status)
{
    LOG_DBG("I_ZD");
}

static int bcb_zd_init()
{
    BCB_INIT_ZD(bcb_zd_data, acmp, v_zd);
    BCB_INIT_ZD(bcb_zd_data, acmp, i_zd);

    cmp_mcux_set_callback(bcb_zd_data.dev_cmp_v_zd, bcb_zd_on_v_zd);
    cmp_mcux_set_callback(bcb_zd_data.dev_cmp_i_zd, bcb_zd_on_i_zd);

    cmp_mcux_set_enable(bcb_zd_data.dev_cmp_v_zd, true);
    cmp_mcux_set_enable(bcb_zd_data.dev_cmp_i_zd, true);

    return 0;
}

SYS_INIT(bcb_zd_init, APPLICATION, CONFIG_BCB_LIB_ZD_INIT_PRIORITY);