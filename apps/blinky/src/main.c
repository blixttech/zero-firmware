#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

#define LED0_NODE 	DT_ALIAS(led0)
#define LED1_NODE 	DT_ALIAS(led1)

#define LED0		DT_GPIO_LABEL(LED0_NODE, gpios)
#define LED1		DT_GPIO_LABEL(LED1_NODE, gpios)

#define LED0_PIN	DT_GPIO_PIN(LED0_NODE, gpios)
#define LED1_PIN	DT_GPIO_PIN(LED1_NODE, gpios)

#define LED0_FLAGS	DT_GPIO_FLAGS(LED0_NODE, gpios)
#define LED1_FLAGS	DT_GPIO_FLAGS(LED0_NODE, gpios)

void main(void)
{
	struct device *dev_led0;
	struct device *dev_led1;
	bool led_toggle = true;
	int ret;

	dev_led0 = device_get_binding(LED0);
	if (dev_led0 == NULL) {
		return;
	}

	dev_led1 = device_get_binding(LED0);
	if (dev_led1 == NULL) {
		return;
	}

	ret = gpio_pin_configure(dev_led0, LED0_PIN, GPIO_OUTPUT_ACTIVE | LED0_FLAGS);
	if (ret < 0) {
		return;
	}

	ret = gpio_pin_configure(dev_led1, LED1_PIN, GPIO_OUTPUT_ACTIVE | LED1_FLAGS);
	if (ret < 0) {
		return;
	}

	while (1) {
		gpio_pin_set(dev_led0, LED0_PIN, (int)led_toggle);
		led_toggle = !led_toggle;
		gpio_pin_set(dev_led1, LED1_PIN, (int)led_toggle);
		k_msleep(SLEEP_TIME_MS);
	}
}
