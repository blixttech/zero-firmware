#include "bcb_zd.h"
#include "bcb_macros.h"
#include <device.h>
#include <devicetree.h>
#include <drivers/input_capture.h>
#include <drivers/gpio.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_zd);

#define BCB_IC_DEV(ch_name) (zd_data.dev_ic_##ch_name)
#define BCB_GPIO_DEV(pin_name) (zd_data.dev_gpio_##pin_name)

struct zd_data {
	struct device *dev_ic_zd_v_mains;
	struct device *dev_gpio_zd_v_mains;
	uint32_t zd_v_last_timestamp;
	volatile uint32_t zd_v_pulse_ticks;
	sys_slist_t zd_v_callback_list;
};

static struct zd_data zd_data;

static void zd_v_mains_callback(struct device *dev, uint8_t channel, uint8_t edge)
{
	bool is_zd_high = BCB_GPIO_PIN_GET_RAW(dctrl, zd_v_mains) == 1;
	uint32_t now = k_uptime_get_32();
	uint32_t pulse_duration;
	sys_snode_t *node;

	pulse_duration = zd_data.zd_v_last_timestamp > now ?
				 UINT32_MAX - zd_data.zd_v_last_timestamp + now :
				 now - zd_data.zd_v_last_timestamp;
	zd_data.zd_v_last_timestamp = now;

	if (pulse_duration < 5) {
		return;
	}

	if (is_zd_high) {
		zd_data.zd_v_pulse_ticks = input_capture_get_value(dev, channel);
	}

	SYS_SLIST_FOR_EACH_NODE (&zd_data.zd_v_callback_list, node) {
		struct bcb_zd_callback  *callback = (struct bcb_zd_callback  *)node;
		if (callback && callback->handler) {
			callback->handler();
		}
	}
}

int bcb_zd_init(void)
{
	sys_slist_init(&zd_data.zd_v_callback_list);

	BCB_IC_INIT(itimestamp, zd_v_mains);
	BCB_IC_CHANNEL_SET(itimestamp, zd_v_mains);

	input_capture_set_callback(zd_data.dev_ic_zd_v_mains,
				   BCB_IC_CHANNEL(itimestamp, zd_v_mains), zd_v_mains_callback);
	input_capture_enable_interrupts(zd_data.dev_ic_zd_v_mains,
					BCB_IC_CHANNEL(itimestamp, zd_v_mains), true);

	BCB_GPIO_PIN_INIT(dctrl, zd_v_mains);
	BCB_GPIO_PIN_CONFIG(dctrl, zd_v_mains, GPIO_INPUT);

	return 0;
}

uint32_t bcb_zd_get_frequency(void)
{
	if (!zd_data.zd_v_pulse_ticks) {
		return 0;
	}

	return input_capture_get_frequency(zd_data.dev_ic_zd_v_mains) * 500 /
	       zd_data.zd_v_pulse_ticks;
}

int bcb_zd_voltage_add_callback(struct bcb_zd_callback  *callback)
{
	if (!callback || !callback->handler) {
		return -ENOTSUP;;
	}

	sys_slist_append(&zd_data.zd_v_callback_list, &callback->node);

	return 0;
}

void bcb_zd_voltage_remove_callback(struct bcb_zd_callback  *callback)
{
	if (!callback) {
		return;
	}
	sys_slist_find_and_remove(&zd_data.zd_v_callback_list, &callback->node);
}
