/**
 * @file
 * @brief Public API for counter array and timer array drivers.
 */

#ifndef _ZEPHYR_INCLUDE_DRIVERS_COUNTER_ARRAY_H_
#define _ZEPHYR_INCLUDE_DRIVERS_COUNTER_ARRAY_H_

/**
 * @defgroup counter_array_interface Counter Array Interface
 * @ingroup io_interfaces
 * @{
 */

#include <stdint.h>
#include <stddef.h>
#include <device.h>
#include <errno.h>
#include <drivers/counter.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*counter_array_api_start)(const struct device *dev, uint8_t id);
typedef int (*counter_array_api_stop)(const struct device *dev, uint8_t id);
typedef int (*counter_array_api_get_value)(const struct device *dev, uint8_t id, uint32_t *ticks);
typedef int (*counter_array_api_set_alarm)(const struct device *dev, uint8_t id, const struct counter_alarm_cfg *alarm_cfg);
typedef int (*counter_array_api_cancel_alarm)(const struct device *dev, uint8_t id);
typedef int (*counter_array_api_set_top_value)(const struct device *dev, uint8_t id, const struct counter_top_cfg *cfg);
typedef uint32_t (*counter_array_api_get_pending_int)(const struct device *dev, uint8_t id);
typedef uint32_t (*counter_array_api_get_top_value)(const struct device *dev, uint8_t id);
typedef uint32_t (*counter_array_api_get_max_relative_alarm)(const struct device *dev, uint8_t id);
typedef uint32_t (*counter_array_api_get_guard_period)(const struct device *dev, uint8_t id, uint32_t flags);
typedef int (*counter_array_api_set_guard_period)(const struct device *dev, uint8_t id, uint32_t ticks, uint32_t flags);
typedef int (*counter_array_api_link)(const struct device *dev, uint8_t id, uint8_t enable);


__subsystem struct counter_array_driver_api {
    counter_array_api_start start;
    counter_array_api_stop stop;
    counter_array_api_get_value get_value;
    counter_array_api_set_alarm set_alarm;
    counter_array_api_cancel_alarm cancel_alarm;
    counter_array_api_set_top_value set_top_value;
    counter_array_api_get_pending_int get_pending_int;
    counter_array_api_get_top_value get_top_value;
    counter_array_api_get_max_relative_alarm get_max_relative_alarm;
    counter_array_api_get_guard_period get_guard_period;
    counter_array_api_set_guard_period set_guard_period;
    counter_array_api_link link;
};

__syscall int counter_array_start(const struct device *dev, uint8_t id);
static inline int z_impl_counter_array_start(const struct device *dev, uint8_t id)
{
    const struct counter_array_driver_api *api = (struct counter_array_driver_api *)dev->driver_api;
    return api->start(dev, id);
}

__syscall int counter_array_stop(const struct device *dev, uint8_t id);
static inline int z_impl_counter_array_stop(const struct device *dev, uint8_t id)
{
    const struct counter_array_driver_api *api = (struct counter_array_driver_api *)dev->driver_api;
    return api->stop(dev, id);
}

__syscall int counter_array_get_value(const struct device *dev, uint8_t id, uint32_t *ticks);
static inline int z_impl_counter_array_get_value(const struct device *dev, uint8_t id, uint32_t *ticks)
{
    const struct counter_array_driver_api *api = (struct counter_array_driver_api *)dev->driver_api;
    return api->get_value(dev, id, ticks);
}

__syscall int counter_array_set_alarm(const struct device *dev, uint8_t id, const struct counter_alarm_cfg *alarm_cfg);
static inline int z_impl_counter_array_set_alarm(const struct device *dev, uint8_t id, const struct counter_alarm_cfg *alarm_cfg)
{
	const struct counter_array_driver_api *api = (struct counter_array_driver_api *)dev->driver_api;
	return api->set_alarm(dev, id, alarm_cfg);
}

__syscall int counter_array_cancel_alarm(const struct device *dev, uint8_t id);
static inline int z_impl_counter_array_cancel_alarm(const struct device *dev, uint8_t id)
{
    const struct counter_array_driver_api *api = (struct counter_array_driver_api *)dev->driver_api;
    return api->cancel_alarm(dev, id);
}

__syscall int counter_array_set_top_value(const struct device *dev, uint8_t id, const struct counter_top_cfg *cfg);
static inline int z_impl_counter_array_set_top_value(const struct device *dev, uint8_t id, const struct counter_top_cfg *cfg)
{
    const struct counter_array_driver_api *api = (struct counter_array_driver_api *)dev->driver_api;
    return api->set_top_value(dev, id, cfg);
}

__syscall int counter_array_get_pending_int(const struct device *dev, uint8_t id);
static inline int z_impl_counter_array_get_pending_int(const struct device *dev, uint8_t id)
{
    const struct counter_array_driver_api *api = (struct counter_array_driver_api *)dev->driver_api;
    return api->get_pending_int(dev, id);
}

__syscall uint32_t counter_array_get_top_value(const struct device *dev, uint8_t id);
static inline uint32_t z_impl_counter_array_get_top_value(const struct device *dev, uint8_t id)
{
    const struct counter_array_driver_api *api = (struct counter_array_driver_api *)dev->driver_api;
    return api->get_top_value(dev, id);
}

__syscall uint32_t counter_array_get_max_relative_alarm(const struct device *dev, uint8_t id);
static inline uint32_t z_impl_counter_array_get_max_relative_alarm(const struct device *dev, uint8_t id)
{
    const struct counter_array_driver_api *api = (struct counter_array_driver_api *)dev->driver_api;
    return api->get_max_relative_alarm(dev, id);
}

__syscall uint32_t counter_array_get_guard_period(const struct device *dev, uint8_t id, uint32_t flags);
static inline uint32_t z_impl_counter_array_get_guard_period(const struct device *dev, uint8_t id, uint32_t flags)
{
    const struct counter_array_driver_api *api = (struct counter_array_driver_api *)dev->driver_api;
    return api->get_guard_period(dev, id, flags);
}

__syscall int counter_array_set_guard_period(const struct device *dev, uint8_t id, uint32_t ticks, uint32_t flags);
static inline int z_impl_counter_array_set_guard_period(const struct device *dev, uint8_t id, uint32_t ticks, uint32_t flags)
{
    const struct counter_array_driver_api *api = (struct counter_array_driver_api *)dev->driver_api;
    return api->set_guard_period(dev, id, ticks, flags);
}

__syscall int counter_array_link(const struct device *dev, uint8_t id, uint8_t enable);
static inline int z_impl_counter_array_link(const struct device *dev, uint8_t id, uint8_t enable)
{
    const struct counter_array_driver_api *api = (struct counter_array_driver_api *)dev->driver_api;
    return api->link(dev, id, enable);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#include <syscalls/counter_array.h>

#endif // _ZEPHYR_INCLUDE_DRIVERS_COUNTER_ARRAY_H_