#include "bcb_user_if.h"
#include "bcb_macros.h"
#include <device.h>
#include <devicetree.h>
#include <drivers/input_capture.h>
#include <drivers/gpio.h>

#define LOG_LEVEL CONFIG_BCB_LEDS_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_user_if);

#define BCB_GPIO_DEV(pin_name) (user_if_data.dev_gpio_##pin_name)

struct bcb_user_if_data {
	struct device *dev_gpio_switch_front;
	struct device *dev_gpio_led_red;
	struct device *dev_gpio_led_green;
	struct gpio_callback button_callback;
	sys_slist_t button_callback_list;
	volatile uint32_t last_button_event_time;
};

static struct bcb_user_if_data user_if_data;

static void button_event(struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	bool is_pressed;
	uint32_t now = k_uptime_get_32();
	uint32_t time_diff = user_if_data.last_button_event_time < now ?
				     UINT32_MAX - user_if_data.last_button_event_time + now :
				     now - user_if_data.last_button_event_time;
	if (time_diff < 100) {
		return;
	}

	is_pressed = BCB_GPIO_PIN_GET_RAW(user_iface, switch_front) == 0;

	sys_snode_t *node;
	SYS_SLIST_FOR_EACH_NODE (&user_if_data.button_callback_list, node) {
		struct bcb_user_button_callback *callback = (struct bcb_user_button_callback *)node;
		if (callback && callback->handler) {
			callback->handler(is_pressed);
		}
	}
}

int bcb_user_if_init()
{
	memset(&user_if_data, 0, sizeof(user_if_data));

	BCB_GPIO_PIN_INIT(user_iface, led_red);
	BCB_GPIO_PIN_CONFIG(user_iface, led_red, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
	BCB_GPIO_PIN_SET_RAW(user_iface, led_red, 1);

	BCB_GPIO_PIN_INIT(user_iface, led_green);
	BCB_GPIO_PIN_CONFIG(user_iface, led_green, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
	BCB_GPIO_PIN_SET_RAW(user_iface, led_green, 1);

	BCB_GPIO_PIN_INIT(user_iface, switch_front);
	BCB_GPIO_PIN_CONFIG(user_iface, switch_front, GPIO_INPUT);

	gpio_pin_interrupt_configure(BCB_GPIO_DEV(switch_front),
				     BCB_GPIO_PIN(user_iface, switch_front), GPIO_INT_EDGE_BOTH);
	gpio_init_callback(&user_if_data.button_callback, button_event,
			   BIT(BCB_GPIO_PIN(user_iface, switch_front)));
	gpio_add_callback(BCB_GPIO_DEV(switch_front), &user_if_data.button_callback);

	return 0;
}

int bcb_user_button_add_callback(struct bcb_user_button_callback *callback)
{
	if (!callback || !callback->handler) {
		return -ENOTSUP;
	}

	sys_slist_append(&user_if_data.button_callback_list, &callback->node);

	return 0;
}

void bcb_user_button_remove_callback(struct bcb_user_button_callback *callback)
{
	if (!callback) {
		return;
	}

	sys_slist_find_and_remove(&user_if_data.button_callback_list, &callback->node);
}

void bcb_user_leds_on(bcb_user_leds_t leds)
{
	if (leds & BCB_USER_LEDS_RED) {
		BCB_GPIO_PIN_SET_RAW(user_iface, led_red, 0);
	}

	if (leds & BCB_USER_LEDS_GREEN) {
		BCB_GPIO_PIN_SET_RAW(user_iface, led_green, 0);
	}
}

void bcb_user_leds_off(bcb_user_leds_t leds)
{
	if (leds & BCB_USER_LEDS_RED) {
		BCB_GPIO_PIN_SET_RAW(user_iface, led_red, 1);
	}

	if (leds & BCB_USER_LEDS_GREEN) {
		BCB_GPIO_PIN_SET_RAW(user_iface, led_green, 1);
	}
}

void bcb_user_leds_toggle(bcb_user_leds_t leds)
{
	if (leds & BCB_USER_LEDS_RED) {
		BCB_GPIO_PIN_TOGGLE(user_iface, led_red);
	}

	if (leds & BCB_USER_LEDS_GREEN) {
		BCB_GPIO_PIN_TOGGLE(user_iface, led_green);
	}
}