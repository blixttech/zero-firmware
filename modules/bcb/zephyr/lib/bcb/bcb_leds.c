#include "bcb_leds.h"
#include "bcb_macros.h"
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>

#define LOG_LEVEL   CONFIG_BCB_LEDS_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_leds);

#define BCB_GPIO_DEV(pin_name)	(leds_data.dev_gpio_##pin_name)

struct bcb_leds_data {
    struct device *dev_gpio_led_red;
    struct device *dev_gpio_led_green;
};

static struct bcb_leds_data leds_data;

void bcb_leds_on(int leds)
{
    if (leds & BCB_LEDS_RED) {
        BCB_GPIO_PIN_SET_RAW(user_iface, led_red, 0);
    }

    if (leds & BCB_LEDS_GREEN) {
        BCB_GPIO_PIN_SET_RAW(user_iface, led_green, 0);
    }
}

void bcb_leds_off(int leds)
{
    if (leds & BCB_LEDS_RED) {
        BCB_GPIO_PIN_SET_RAW(user_iface, led_red, 1);
    }

    if (leds & BCB_LEDS_GREEN) {
        BCB_GPIO_PIN_SET_RAW(user_iface, led_green, 1);
    }
}

void bcb_leds_toggle(int leds)
{
    if (leds & BCB_LEDS_RED) {
        BCB_GPIO_PIN_TOGGLE(user_iface, led_red);
    }

    if (leds & BCB_LEDS_GREEN) {
        BCB_GPIO_PIN_TOGGLE(user_iface, led_green);
    }
}

int bcb_leds_init(void)
{
    BCB_GPIO_PIN_INIT(user_iface, led_red);
    BCB_GPIO_PIN_CONFIG(user_iface, led_red, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    BCB_GPIO_PIN_SET_RAW(user_iface, led_red, 1);

    BCB_GPIO_PIN_INIT(user_iface, led_green);
    BCB_GPIO_PIN_CONFIG(user_iface, led_green, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    BCB_GPIO_PIN_SET_RAW(user_iface, led_green, 1);

    return 0;
}