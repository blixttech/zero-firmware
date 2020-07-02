#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <blixt-breaker.h>

/* This should match blixt,breaker.yaml */
#define DT_DRV_COMPAT blixt_breaker

#define INIT_BREAKER_GPIO_PIN(label, pin_name, data_struct, pin_flags)                      \
    (data_struct)->port_##pin_name =                                                        \
                        device_get_binding(DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(label), \
                                                                        gpios,              \
                                                                        pin_name)           \
                                                                )                           \
                                                        );                                  \
    if ((data_struct)->port_##pin_name == NULL) {                                           \
        return -EINVAL;                                                                     \
    }                                                                                       \
    gpio_pin_configure((data_struct)->port_##pin_name,                                      \
                        DT_PHA_BY_NAME(DT_NODELABEL(label), gpios, pin_name, pin),          \
                        (pin_flags));

#define SET_BREAKER_GPIO_PIN(label, pin_name, data_struct, value)                           \
    gpio_pin_set((data_struct)->port_##pin_name,                                            \
                        DT_PHA_BY_NAME(DT_NODELABEL(label), gpios, pin_name, pin),          \
                        (value))                     

#define GET_BREAKER_GPIO_PIN(label, pin_name, data_struct)                                  \
    gpio_pin_get_raw((data_struct)->port_##pin_name,                                        \
                    DT_PHA_BY_NAME(DT_NODELABEL(label), gpios, pin_name, pin))                                       

struct breaker_data {
    struct device*  port_on_off;
    struct device*  port_oc_ot_reset;
    struct device*  port_on_off_status;
    struct device*  port_oc_test_tr_n;
    struct device*  port_oc_test_tr_p;
    breaker_ocp_callback_handler_t ocp_callback;
};

static int driver_on(struct device* dev);
static int driver_off(struct device* dev);
static int driver_reset(struct device* dev);

static struct breaker_data device_data;
static struct gpio_callback on_off_cb_data;

static void driver_on_off_status_changed(struct device* dev, struct gpio_callback* cb, u32_t pins)
{
    int on_off_status = GET_BREAKER_GPIO_PIN(dctrl, on_off_status, &device_data);
    int on_off = GET_BREAKER_GPIO_PIN(dctrl, on_off, &device_data);

    if (on_off == 1 && on_off_status == 0) {
        /* OCP triggered. */
        printk("status changed ON_OFF_STATUS %u, ON_OFF %u\n", on_off_status, on_off);
        if (device_data.ocp_callback) {
            device_data.ocp_callback(NULL, 0);
        }
    }
}

static int driver_init(struct device *dev)
{   
    int ret = 0;
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

	ret = gpio_pin_interrupt_configure(((struct breaker_data*)dev->driver_data)->port_on_off_status, 
                                        DT_PHA_BY_NAME(DT_NODELABEL(dctrl), gpios, on_off_status, pin), 
                                        GPIO_INT_EDGE_BOTH);
    if (ret != 0) {
        return ret;
    }
    gpio_init_callback(&on_off_cb_data, 
                        driver_on_off_status_changed, 
                        BIT(DT_PHA_BY_NAME(DT_NODELABEL(dctrl), gpios, on_off_status, pin)));

    gpio_add_callback(((struct breaker_data*)dev->driver_data)->port_on_off_status, 
                        &on_off_cb_data);

    return 0;
}

static int driver_on(struct device *dev)
{
    /*  We need to reset before tuning on since overcurrent/overtemperature may have been
        triggered already.
     */
    int ret = driver_reset(dev);
    if (ret) {
        return ret;
    }
    //gpio_add_callback(((struct breaker_data*)dev->driver_data)->port_on_off_status, 
    //                    &on_off_cb_data);
    return SET_BREAKER_GPIO_PIN(dctrl, on_off, (struct breaker_data*)dev->driver_data, 1);
}

static int driver_off(struct device *dev)
{
    /*  Overcurrent protection is triggered when turning off due to the 
        huge gate discharge current. As we do reset the overcurrent protection when
        turning on, we do not need to do a reset in here.  
     */

    /* Need to remove the callback due to the above reason. */
    //gpio_remove_callback(((struct breaker_data*)dev->driver_data)->port_on_off_status, 
    //                    &on_off_cb_data);

    return SET_BREAKER_GPIO_PIN(dctrl, on_off, (struct breaker_data*)dev->driver_data, 0);
}

static bool driver_is_on(struct device *dev)
{
    return GET_BREAKER_GPIO_PIN(dctrl, on_off_status, (struct breaker_data*)dev->driver_data) == 1;
}

static int driver_reset(struct device *dev)
{
    SET_BREAKER_GPIO_PIN(dctrl, oc_ot_reset, (struct breaker_data*)dev->driver_data, 0);
    SET_BREAKER_GPIO_PIN(dctrl, oc_ot_reset, (struct breaker_data*)dev->driver_data, 1);
    SET_BREAKER_GPIO_PIN(dctrl, oc_ot_reset, (struct breaker_data*)dev->driver_data, 0);
    return 0;
}

static int driver_set_ocp_callback(struct device *dev, breaker_ocp_callback_handler_t callback)
{
    ((struct breaker_data*)dev->driver_data)->ocp_callback = callback;
    return 0;
}

static int driver_ocp_trigger(struct device *dev, int direction)
{
    if (direction == BREAKER_OCP_TRIGGER_DIR_P) {
        SET_BREAKER_GPIO_PIN(dctrl, oc_test_tr_p, (struct breaker_data*)dev->driver_data, 0);
        SET_BREAKER_GPIO_PIN(dctrl, oc_test_tr_p, (struct breaker_data*)dev->driver_data, 1);
        SET_BREAKER_GPIO_PIN(dctrl, oc_test_tr_p, (struct breaker_data*)dev->driver_data, 0);
    } else {
        SET_BREAKER_GPIO_PIN(dctrl, oc_test_tr_n, (struct breaker_data*)dev->driver_data, 0);
        SET_BREAKER_GPIO_PIN(dctrl, oc_test_tr_n, (struct breaker_data*)dev->driver_data, 1);
        SET_BREAKER_GPIO_PIN(dctrl, oc_test_tr_n, (struct breaker_data*)dev->driver_data, 0);      
    }
    return 0;
}

static const struct breaker_driver_api driver_api = {
    .on = driver_on,
    .off = driver_off,
    .is_on = driver_is_on,
    .reset = driver_reset,
    .set_ocp_callback = driver_set_ocp_callback,
    .ocp_trigger = driver_ocp_trigger
};

#define BREAKER_DRIVER_INIT_PRIORITY    50

DEVICE_AND_API_INIT(breaker_device,                     \
                    DT_LABEL(DT_NODELABEL(breaker)),    \
                    driver_init,                        \
                    &device_data,                       \
                    NULL,                               \
                    POST_KERNEL,                        \
                    BREAKER_DRIVER_INIT_PRIORITY,       \
                    &driver_api);
