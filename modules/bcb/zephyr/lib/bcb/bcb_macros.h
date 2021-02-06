#ifndef _BCB_GPIO_MACROS_H_
#define _BCB_GPIO_MACROS_H_

//#define BCB_GPIO_DEV(pin_name) (dev_gpio_##pin_name)
#define BCB_GPIO_DEV_LABEL(node_label, pin_name)                                                   \
	DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(node_label), gpios, pin_name))

#define BCB_GPIO_PIN(node_label, pin_name)                                                         \
	DT_PHA_BY_NAME(DT_NODELABEL(node_label), gpios, pin_name, pin)

#define BCB_GPIO_PIN_FLAGS(node_label, pin_name)                                                   \
	DT_PHA_BY_NAME(DT_NODELABEL(node_label), gpios, pin_name, flags)

#define BCB_GPIO_PIN_INIT(node_label, pin_name)                                                    \
	do {                                                                                       \
		BCB_GPIO_DEV(pin_name) =                                                           \
			device_get_binding(BCB_GPIO_DEV_LABEL(node_label, pin_name));              \
		if (BCB_GPIO_DEV(pin_name) == NULL) {                                              \
			LOG_ERR("Could not get GPIO device");                                      \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0)

#define BCB_GPIO_PIN_CONFIG(node_label, pin_name, pin_flags)                                       \
	gpio_pin_configure(BCB_GPIO_DEV(pin_name), BCB_GPIO_PIN(node_label, pin_name), (pin_flags))

#define BCB_GPIO_PIN_SET_RAW(node_label, pin_name, value)                                          \
	gpio_pin_set_raw(BCB_GPIO_DEV(pin_name), BCB_GPIO_PIN(node_label, pin_name), (value))

#define BCB_GPIO_PIN_GET_RAW(node_label, pin_name)                                                 \
	gpio_pin_get_raw(BCB_GPIO_DEV(pin_name), BCB_GPIO_PIN(node_label, pin_name))

#define BCB_GPIO_PIN_TOGGLE(node_label, pin_name)                                                  \
	gpio_pin_toggle(BCB_GPIO_DEV(pin_name), BCB_GPIO_PIN(node_label, pin_name))

/*------------------------------------------------------------------------------------------------*/

//#define BCB_IC_DEV(ch_name) (dev_ic_##ch_name)
#define BCB_IC_DEV_LABEL(node_label, ch_name)                                                      \
	DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(node_label), input_captures, ch_name))

#define BCB_IC_CHANNEL(node_label, ch_name)                                                        \
	DT_PHA_BY_NAME(DT_NODELABEL(node_label), input_captures, ch_name, channel)

#define BCB_IC_EDGE(node_label, ch_name)                                                           \
	DT_PHA_BY_NAME(DT_NODELABEL(node_label), input_captures, ch_name, edge)

#define BCB_IC_INIT(node_label, ch_name)                                                           \
	do {                                                                                       \
		BCB_IC_DEV(ch_name) = device_get_binding(BCB_IC_DEV_LABEL(node_label, ch_name));   \
		if (BCB_IC_DEV(ch_name) == NULL) {                                                 \
			LOG_ERR("Could not get IC device");                                        \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0)

#define BCB_IC_CHANNEL_SET(node_label, ch_name)                                                    \
	input_capture_set_channel(BCB_IC_DEV(ch_name), BCB_IC_CHANNEL(node_label, ch_name),        \
				  BCB_IC_EDGE(node_label, ch_name))

#define BCB_IC_VALUE(node_label, ch_name)                                                          \
	input_capture_get_value(BCB_IC_DEV(ch_name), BCB_IC_CHANNEL(node_label, ch_name))

#define BCB_IC_COUNTER(ch_name) input_capture_get_counter(BCB_IC_DEV(ch_name))

#define BCB_IC_COUNTER_MAX(ch_name) input_capture_get_counter_maximum(BCB_IC_DEV(ch_name))

#define BCB_IC_FREQUENCY(ch_name) input_capture_get_frequency(BCB_IC_DEV(ch_name))

/*------------------------------------------------------------------------------------------------*/

//#define BCB_PWM_DEV(ch_name) (dev_pwm_##ch_name)
#define BCB_PWM_DEV_LABEL(node_label, ch_name)                                                     \
	DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(node_label), pwms, ch_name))

#define BCB_PWM_CHANNEL(node_label, ch_name)                                                       \
	DT_PHA_BY_NAME(DT_NODELABEL(node_label), pwms, ch_name, channel)

#define BCB_PWM_PERIOD(node_label, ch_name)                                                        \
	DT_PHA_BY_NAME(DT_NODELABEL(node_label), pwms, ch_name, period)

#define BCB_PWM_INIT(node_label, ch_name)                                                          \
	do {                                                                                       \
		BCB_PWM_DEV(ch_name) = device_get_binding(BCB_PWM_DEV_LABEL(node_label, ch_name)); \
		if (BCB_PWM_DEV(ch_name) == NULL) {                                                \
			LOG_ERR("Could not get PWM device");                                       \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0)

#define BCB_PWM_DUTY_CYCLE_SET(node_label, ch_name, dcycle)                                        \
	pwm_pin_set_cycles(BCB_PWM_DEV(ch_name), BCB_PWM_CHANNEL(node_label, ch_name),             \
			   BCB_PWM_PERIOD(node_label, ch_name),                                    \
			   (BCB_PWM_PERIOD(node_label, ch_name) * (dcycle) / 100), 0);

/*------------------------------------------------------------------------------------------------*/

//#define BCB_DAC_DEV(ch_name)            (dac_##ch_name)
#define BCB_DAC_DEV_LABEL(node_label, ch_name)                                                     \
	DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(node_label), io_channels, ch_name))

#define BCB_DAC_CHANNEL(node_label, ch_name)                                                       \
	DT_PHA_BY_NAME(DT_NODELABEL(node_label), io_channels, ch_name, output)

#define BCB_DAC_INIT(node_label, ch_name)                                                          \
	do {                                                                                       \
		BCB_DAC_DEV(ch_name) = device_get_binding(BCB_DAC_DEV_LABEL(node_label, ch_name)); \
		if (BCB_DAC_DEV(ch_name) == NULL) {                                                \
			LOG_ERR("Could not get DAC device");                                       \
			return -EINVAL;                                                            \
		}                                                                                  \
		struct dac_channel_cfg cfg = { .channel_id = BCB_DAC_CHANNEL(node_label, ch_name), \
					       .resolution = 12 };                                 \
		dac_channel_setup(BCB_DAC_DEV(ch_name), &cfg);                                     \
	} while (0)

#define BCB_DAC_SET(node_label, ch_name, value)                                                    \
	dac_write_value(BCB_DAC_DEV(ch_name), BCB_DAC_CHANNEL(node_label, ch_name), value)

#endif /* _BCB_GPIO_MACROS_H_ */