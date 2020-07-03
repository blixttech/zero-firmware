#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <blixt-breaker.h>

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
    breaker_ocp_callback_t ocp_callback;
};

int driver_on(struct device* dev);
int driver_off(struct device* dev);
int driver_reset(struct device* dev);

static struct breaker_data breaker_data;
static struct gpio_callback on_off_cb_data;

static void on_off_status_changed(struct device* dev, struct gpio_callback* cb, u32_t pins)
{
    int on_off_status = GET_BREAKER_GPIO_PIN(dctrl, on_off_status, &breaker_data);
    int on_off = GET_BREAKER_GPIO_PIN(dctrl, on_off, &breaker_data);

    if (on_off == 1 && on_off_status == 0) {
        /* OCP triggered. */
        printk("status changed ON_OFF_STATUS %u, ON_OFF %u\n", on_off_status, on_off);
        if (breaker_data.ocp_callback) {
            breaker_data.ocp_callback(0);
        }
    }
}

static int breaker_init()
{   
    int ret = 0;
    INIT_BREAKER_GPIO_PIN(dctrl, on_off, &breaker_data, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    SET_BREAKER_GPIO_PIN(dctrl, on_off, &breaker_data, 0);

    INIT_BREAKER_GPIO_PIN(dctrl, oc_ot_reset, &breaker_data, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    SET_BREAKER_GPIO_PIN(dctrl, oc_ot_reset, &breaker_data, 0);

    INIT_BREAKER_GPIO_PIN(dctrl, oc_test_tr_n, &breaker_data, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    SET_BREAKER_GPIO_PIN(dctrl, oc_test_tr_n, &breaker_data, 0);

    INIT_BREAKER_GPIO_PIN(dctrl, oc_test_tr_p, &breaker_data, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    SET_BREAKER_GPIO_PIN(dctrl, oc_test_tr_p, &breaker_data, 0);

    INIT_BREAKER_GPIO_PIN(dctrl, on_off_status, &breaker_data, GPIO_INPUT);

	ret = gpio_pin_interrupt_configure(breaker_data.port_on_off_status, 
                                        DT_PHA_BY_NAME(DT_NODELABEL(dctrl), gpios, on_off_status, pin), 
                                        GPIO_INT_EDGE_BOTH);
    if (ret != 0) {
        return ret;
    }
    gpio_init_callback(&on_off_cb_data, 
                        on_off_status_changed, 
                        BIT(DT_PHA_BY_NAME(DT_NODELABEL(dctrl), gpios, on_off_status, pin)));

    //gpio_add_callback(breaker_data.port_on_off_status, &on_off_cb_data);

    return 0;
}

int breaker_on()
{
    /*  We need to reset before tuning on since overcurrent/overtemperature may have been
        triggered already.
     */
    int ret = breaker_reset();
    if (ret) {
        return ret;
    }
    gpio_add_callback(breaker_data.port_on_off_status, &on_off_cb_data);

    return SET_BREAKER_GPIO_PIN(dctrl, on_off, &breaker_data, 1);
}

int breaker_off(struct device *dev)
{
    /*  Overcurrent protection is triggered when turning off due to the 
        huge gate discharge current. As we do reset the overcurrent protection when
        turning on, we do not need to do a reset in here.  
     */

    /* Need to remove the callback due to the above reason. */
    gpio_remove_callback(breaker_data.port_on_off_status, &on_off_cb_data);

    return SET_BREAKER_GPIO_PIN(dctrl, on_off, &breaker_data, 0);
}

bool breaker_is_on()
{
    return GET_BREAKER_GPIO_PIN(dctrl, on_off_status, &breaker_data) == 1;
}

int breaker_reset()
{
    SET_BREAKER_GPIO_PIN(dctrl, oc_ot_reset, &breaker_data, 0);
    SET_BREAKER_GPIO_PIN(dctrl, oc_ot_reset, &breaker_data, 1);
    SET_BREAKER_GPIO_PIN(dctrl, oc_ot_reset, &breaker_data, 0);
    return 0;
}

int breaker_set_ocp_callback(breaker_ocp_callback_t callback)
{
    breaker_data.ocp_callback = callback;
    return 0;
}

int breaker_ocp_trigger(int direction)
{
    if (direction == BREAKER_OCP_TRIGGER_DIR_P) {
        SET_BREAKER_GPIO_PIN(dctrl, oc_test_tr_p, &breaker_data, 0);
        SET_BREAKER_GPIO_PIN(dctrl, oc_test_tr_p, &breaker_data, 1);
        SET_BREAKER_GPIO_PIN(dctrl, oc_test_tr_p, &breaker_data, 0);
    } else {
        SET_BREAKER_GPIO_PIN(dctrl, oc_test_tr_n, &breaker_data, 0);
        SET_BREAKER_GPIO_PIN(dctrl, oc_test_tr_n, &breaker_data, 1);
        SET_BREAKER_GPIO_PIN(dctrl, oc_test_tr_n, &breaker_data, 0);      
    }
    return 0;
}

#define BREAKER_API_INIT_PRIORITY    50

SYS_INIT(breaker_init, APPLICATION, BREAKER_API_INIT_PRIORITY);
