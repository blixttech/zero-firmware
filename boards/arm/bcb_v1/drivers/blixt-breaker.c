#include <device.h>
#include <devicetree.h>
#include <blixt-breaker.h>

/* This should match blixt,breaker.yaml */
#define DT_DRV_COMPAT blixt_breaker

struct breaker_config {

};

struct breaker_data {

};


static int driver_init(struct device *dev)
{
    return 0;
}

static int driver_on(struct device *dev)
{
    return 0;
}

static int driver_off(struct device *dev)
{
    return 0;
}

static int driver_is_on(struct device *dev)
{
    return 0;
}

static int driver_reset(struct device *dev)
{
    return 0;
}

static const struct breaker_driver_api driver_api = {
    .on = driver_on,
    .off = driver_off,
    .is_on = driver_is_on,
    .reset = driver_reset
};

static const struct breaker_config driver_config = {

};

static struct breaker_data device_data = {

};

#define BREAKER_DRIVER_INIT_PRIORITY    50

DEVICE_AND_API_INIT(breaker_device,                     \
                    DT_LABEL(DT_NODELABEL(breaker)),    \
                    driver_init,                        \
                    &device_data,                       \
                    &driver_config,                     \
                    POST_KERNEL,                        \
                    BREAKER_DRIVER_INIT_PRIORITY,       \
                    &driver_api);
