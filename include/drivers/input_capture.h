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

/**
 * @typedef input_capture_get_counter_t
 * @brief Callback API upon getting counter
 * See @a input_capture_get_counter() for argument description
 */
typedef uint32_t (*input_capture_get_counter_t)(struct device *dev);

/**
 * @typedef input_capture_set_channel_t
 * @brief Callback API upon setting the channel
 * See @a input_capture_set_channel() for argument description
 */
typedef int (*input_capture_set_channel_t)(struct device *dev, uint32_t channel, uint32_t edge);

/**
 * @typedef input_capture_get_value_t
 * @brief Callback API upon getting the captured value
 * See @a input_capture_get_value() for argument description
 */
typedef uint32_t (*input_capture_get_value_t)(struct device *dev, uint32_t channel);

/**
 * @typedef input_capture_get_counter_t
 * @brief Callback API upon getting the counter frequency
 * See @a input_capture_get_frequency() for argument description
 */
typedef uint32_t (*input_capture_get_frequency_t)(struct device *dev);

/** @brief Input Capture driver API definition. */
__subsystem struct input_capture_driver_api {
    input_capture_get_counter_t get_counter;
    input_capture_set_channel_t set_channel;
    input_capture_get_value_t get_value;
    input_capture_get_frequency_t get_frequency;
};

/**
 * @brief Function to get the current counter value.
 * @param[in]  dev    Pointer to the device structure for the driver instance.
 * @retval the counter value.
 */
__syscall uint32_t input_capture_get_counter(struct device* dev);
static inline uint32_t z_impl_input_capture_get_counter(struct device* dev)
{
    struct input_capture_driver_api *api;

    api = (struct input_capture_driver_api *)dev->driver_api;
    return api->get_counter(dev);
}

/**
 * @brief Function to set the channel configuration.
 * @param[in]   dev     Pointer to the device structure for the driver instance.
 * @param[in]   channel Channel number.
 * @param[in]   edge    The edge to be captured.
 * @retval 0 If successful.
 */
__syscall int input_capture_set_channel(struct device* dev, uint32_t channel, uint32_t edge);
static inline int z_impl_input_capture_set_channel(struct device* dev, uint32_t channel, uint32_t edge)
{
    struct input_capture_driver_api *api;

    api = (struct input_capture_driver_api *)dev->driver_api;
    return api->set_channel(dev, channel, edge);
}

/**
 * @brief Function to get the captured counter value of the channel.
 * @param[in]   dev     Pointer to the device structure for the driver instance.
 * @param[in]   channel Channel number.
 * @retval the captured counter value.
 */
__syscall uint32_t input_capture_get_value(struct device* dev, uint32_t channel);
static inline uint32_t z_impl_input_capture_get_value(struct device* dev, uint32_t channel)
{
    struct input_capture_driver_api *api;

    api = (struct input_capture_driver_api *)dev->driver_api;
    return api->get_value(dev, channel);
}

/**
 * @brief Function to get the frequency of the counter.
 * @param[in]  dev    Pointer to the device structure for the driver instance.
 * @retval the frequency in hertz.
 */
__syscall uint32_t input_capture_get_frequency(struct device* dev);
static inline uint32_t z_impl_input_capture_get_frequency(struct device* dev)
{
    struct input_capture_driver_api *api;

    api = (struct input_capture_driver_api *)dev->driver_api;
    return api->get_frequency(dev);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#include <syscalls/input_capture.h>

#endif // _ZEPHYR_INCLUDE_DRIVERS_INPUT_CAPTURE_H_