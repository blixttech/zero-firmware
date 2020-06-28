#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <blixt-breaker.h>

/* This should match blixt,breaker.yaml */
#define DT_DRV_COMPAT blixt_breaker

#define INIT_BREAKER_GPIO_PIN(label, pin_name, data_struct, pin_flags) \
    (data_struct)->port_##pin_name = device_get_binding(DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(label), \
                                                                                    gpios, \
                                                                                    pin_name) \
                                                                ) \
                                                        ); \
    if ((data_struct)->port_##pin_name == NULL) { \
        return -EINVAL; \
    } \
    gpio_pin_configure((data_struct)->port_##pin_name, \
                        DT_PHA_BY_NAME(DT_NODELABEL(label), gpios, pin_name, pin), \
                        (pin_flags));

#define SET_BREAKER_GPIO_PIN(label, pin_name, data_struct, value) \
    gpio_pin_set((data_struct)->port_##pin_name, \
                        DT_PHA_BY_NAME(DT_NODELABEL(label), gpios, pin_name, pin), \
                        (value));                     

struct breaker_config {

};

struct breaker_data {
    struct device*  port_on_off;
    struct device*  port_oc_ot_reset;
    struct device*  port_on_off_status;
    struct device*  port_oc_test_tr_n;
    struct device*  port_oc_test_tr_p;
};


static int driver_init(struct device *dev)
{   
    INIT_BREAKER_GPIO_PIN(dctrl, 
                        on_off, 
                        (struct breaker_data*)dev->driver_data, 
                        (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    SET_BREAKER_GPIO_PIN(dctrl, on_off, (struct breaker_data*)dev->driver_data, 0);

    INIT_BREAKER_GPIO_PIN(dctrl, 
                        oc_ot_reset, 
                        (struct breaker_data*)dev->driver_data, 
                        (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    SET_BREAKER_GPIO_PIN(dctrl, oc_ot_reset, (struct breaker_data*)dev->driver_data, 0);

    INIT_BREAKER_GPIO_PIN(dctrl, 
                        oc_test_tr_n, 
                        (struct breaker_data*)dev->driver_data, 
                        (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    SET_BREAKER_GPIO_PIN(dctrl, oc_test_tr_n, (struct breaker_data*)dev->driver_data, 0);

    INIT_BREAKER_GPIO_PIN(dctrl, 
                        oc_test_tr_p, 
                        (struct breaker_data*)dev->driver_data, 
                        (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    SET_BREAKER_GPIO_PIN(dctrl, oc_test_tr_p, (struct breaker_data*)dev->driver_data, 0);

    INIT_BREAKER_GPIO_PIN(dctrl, 
                        on_off_status, 
                        (struct breaker_data*)dev->driver_data, 
                        GPIO_INPUT);

    return 0;
}

static int driver_on(struct device *dev)
{
    return SET_BREAKER_GPIO_PIN(dctrl, on_off, (struct breaker_data*)dev->driver_data, 1);
}

static int driver_off(struct device *dev)
{
    return SET_BREAKER_GPIO_PIN(dctrl, on_off, (struct breaker_data*)dev->driver_data, 0);
}

static int driver_is_on(struct device *dev)
{
    return 0;
}

static int driver_reset(struct device *dev)
{
    SET_BREAKER_GPIO_PIN(dctrl, oc_ot_reset, (struct breaker_data*)dev->driver_data, 0);
    SET_BREAKER_GPIO_PIN(dctrl, oc_ot_reset, (struct breaker_data*)dev->driver_data, 1);
    SET_BREAKER_GPIO_PIN(dctrl, oc_ot_reset, (struct breaker_data*)dev->driver_data, 0);
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
