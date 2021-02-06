/**
 * @file
 * @brief Public API header file for input capture driver
 */

#ifndef _ZEPHYR_INCLUDE_DRIVERS_INPUT_CAPTURE_H_
#define _ZEPHYR_INCLUDE_DRIVERS_INPUT_CAPTURE_H_

/**
 * @brief Input Capture Interface
 * @defgroup input_capture_interface Input Capture Interface
 * @ingroup io_interfaces
 * @{
 */

#include <errno.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <device.h>
#include <dt-bindings/timer/input-capture.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*input_capture_callback_t)(struct device *dev, uint8_t channel, uint8_t edge);
typedef uint32_t (*input_capture_get_counter_t)(struct device *dev);
typedef int (*input_capture_set_channel_t)(struct device *dev, uint8_t channel, uint8_t edge);
typedef uint32_t (*input_capture_get_value_t)(struct device *dev, uint8_t channel);
typedef uint32_t (*input_capture_get_frequency_t)(struct device *dev);
typedef uint32_t (*input_capture_get_counter_maximum_t)(struct device *dev);
typedef int (*input_capture_set_callback_t)(struct device *dev, uint8_t channel,
					    input_capture_callback_t callback);
typedef int (*input_capture_enable_interrupts_t)(struct device *dev, uint8_t channel, bool enable);

__subsystem struct input_capture_driver_api {
	input_capture_get_counter_t get_counter;
	input_capture_set_channel_t set_channel;
	input_capture_get_value_t get_value;
	input_capture_get_frequency_t get_frequency;
	input_capture_get_counter_maximum_t get_counter_maximum;
	input_capture_set_callback_t set_callback;
	input_capture_enable_interrupts_t enable_interrupts;
};

__syscall uint32_t input_capture_get_counter(struct device *dev);
static inline uint32_t z_impl_input_capture_get_counter(struct device *dev)
{
	struct input_capture_driver_api *api;

	api = (struct input_capture_driver_api *)dev->driver_api;
	return api->get_counter(dev);
}

__syscall int input_capture_set_channel(struct device *dev, uint8_t channel, uint8_t edge);
static inline int z_impl_input_capture_set_channel(struct device *dev, uint8_t channel,
						   uint8_t edge)
{
	struct input_capture_driver_api *api;

	api = (struct input_capture_driver_api *)dev->driver_api;
	return api->set_channel(dev, channel, edge);
}

__syscall uint32_t input_capture_get_value(struct device *dev, uint8_t channel);
static inline uint32_t z_impl_input_capture_get_value(struct device *dev, uint8_t channel)
{
	struct input_capture_driver_api *api;

	api = (struct input_capture_driver_api *)dev->driver_api;
	return api->get_value(dev, channel);
}

__syscall uint32_t input_capture_get_frequency(struct device *dev);
static inline uint32_t z_impl_input_capture_get_frequency(struct device *dev)
{
	struct input_capture_driver_api *api;

	api = (struct input_capture_driver_api *)dev->driver_api;
	return api->get_frequency(dev);
}

__syscall uint32_t input_capture_get_counter_maximum(struct device *dev);
static inline uint32_t z_impl_input_capture_get_counter_maximum(struct device *dev)
{
	struct input_capture_driver_api *api;

	api = (struct input_capture_driver_api *)dev->driver_api;
	return api->get_counter_maximum(dev);
}

__syscall int input_capture_set_callback(struct device *dev, uint8_t channel,
					 input_capture_callback_t callback);
static inline int z_impl_input_capture_set_callback(struct device *dev, uint8_t channel,
						    input_capture_callback_t callback)
{
	struct input_capture_driver_api *api;

	api = (struct input_capture_driver_api *)dev->driver_api;
	return api->set_callback(dev, channel, callback);
}

__syscall int input_capture_enable_interrupts(struct device *dev, uint8_t channel, bool enable);
static inline int z_impl_input_capture_enable_interrupts(struct device *dev, uint8_t channel,
							 bool enable)
{
	struct input_capture_driver_api *api;

	api = (struct input_capture_driver_api *)dev->driver_api;
	return api->enable_interrupts(dev, channel, enable);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#include <syscalls/input_capture.h>

#endif // _ZEPHYR_INCLUDE_DRIVERS_INPUT_CAPTURE_H_