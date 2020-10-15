#include <bcb_leds.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>

#define LOG_LEVEL   CONFIG_BCB_LEDS_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_leds);

#define BCB_GPIO_DEV(pin_name)      (bcb_leds_data.dev_##pin_name)

#define BCB_INIT_GPIO_PIN(dt_nlabel, pin_name, pin_flags)                                               \
    do {                                                                                                \
        BCB_GPIO_DEV(pin_name) = device_get_binding(DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(dt_nlabel),\
                                                                gpios, pin_name)));                     \
        if (BCB_GPIO_DEV(pin_name) == NULL) {                                                           \
            LOG_ERR("Could not get GPIO device");                                                       \
            return -EINVAL;                                                                             \
        }                                                                                               \
        gpio_pin_configure(BCB_GPIO_DEV(pin_name),                                                      \
                            DT_PHA_BY_NAME(DT_NODELABEL(dt_nlabel), gpios, pin_name, pin),              \
                            (pin_flags));                                                               \
    } while(0)                                                                              

#define BCB_SET_GPIO_PIN(dt_nlabel, pin_name, value)                                \
    gpio_pin_set_raw(BCB_GPIO_DEV(pin_name),                                        \
                    DT_PHA_BY_NAME(DT_NODELABEL(dt_nlabel), gpios, pin_name, pin),  \
                    (value))                     

#define BCB_GET_GPIO_PIN(dt_nlabel, pin_name)                                       \
    gpio_pin_get_raw(BCB_GPIO_DEV(pin_name),                                        \
                    DT_PHA_BY_NAME(DT_NODELABEL(dt_nlabel), gpios, pin_name, pin))

struct bcb_leds_data {
    struct device* dev_led_red;
    struct device* dev_led_green;
};

static struct bcb_leds_data bcb_leds_data = {
    .dev_led_red = NULL,
    .dev_led_green = NULL,
};

void bcb_leds_on(int leds)
{
    if (leds & BCB_LEDS_RED) {
        BCB_SET_GPIO_PIN(user_iface, led_red, 0);
    }

    if (leds & BCB_LEDS_GREEN) {
        BCB_SET_GPIO_PIN(user_iface, led_green, 0);
    }
}

void bcb_leds_off(int leds)
{
    if (leds & BCB_LEDS_RED) {
        BCB_SET_GPIO_PIN(user_iface, led_red, 1);
    }

    if (leds & BCB_LEDS_GREEN) {
        BCB_SET_GPIO_PIN(user_iface, led_green, 1);
    }
}

void bcb_leds_toggle(int leds)
{

}

static int bcb_leds_init()
{

    BCB_INIT_GPIO_PIN(user_iface, led_red, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    BCB_SET_GPIO_PIN(user_iface, led_red, 1);
    BCB_INIT_GPIO_PIN(user_iface, led_green, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    BCB_SET_GPIO_PIN(user_iface, led_green, 1);

    return 0;
}

SYS_INIT(bcb_leds_init, APPLICATION, CONFIG_BCB_LIB_LEDS_INIT_PRIORITY);