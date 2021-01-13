/**
 * @file
 * @brief Header file for Comparator driver implementation.
 */

#ifndef _ZEPHYR_INCLUDE_DRIVERS_CMP_MCUX_H_
#define _ZEPHYR_INCLUDE_DRIVERS_CMP_MCUX_H_

#include <stdint.h>
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Driver APIs for Comparator driver for NXP K64F
 * @defgroup cmp_interface Comparator driver APIs
 * @ingroup io_interfaces
 * @{
 */

typedef void (*cmp_mcux_callback_t)(struct device* dev, bool status);

typedef int (*cmp_mcux_api_set_enable)(struct device* dev, bool enable);
typedef int (*cmp_mcux_api_set_channels)(struct device* dev, int chp, int chn);
typedef int (*cmp_mcux_api_enable_interrupts)(struct device* dev, int interrupts);
typedef int (*cmp_mcux_api_disable_interrupts)(struct device* dev, int interrupts);
typedef int (*cmp_mcux_api_set_callback)(struct device* dev, cmp_mcux_callback_t callback);

/**
 * @brief Comparator driver API
 *
 * This is the mandatory API any Comparator driver needs to expose.
 */
__subsystem struct cmp_mcux_driver_api {
    cmp_mcux_api_set_enable                 set_enable;
    cmp_mcux_api_set_channels               set_channels;
    cmp_mcux_api_enable_interrupts          enable_interrupts;
    cmp_mcux_api_disable_interrupts         disable_interrupts;
    cmp_mcux_api_set_callback               set_callback;
};

__syscall int cmp_mcux_set_enable(struct device* dev, bool status);
static inline int z_impl_cmp_mcux_set_enable(struct device* dev, bool status)
{
    struct cmp_mcux_driver_api *api;
    api = (struct cmp_mcux_driver_api *)dev->driver_api;
    return api->set_enable(dev, status);
}

__syscall int cmp_mcux_set_channels(struct device* dev, int chp, int chn);
static inline int z_impl_cmp_mcux_set_channels(struct device* dev, int chp, int chn)
{
    struct cmp_mcux_driver_api *api;
    api = (struct cmp_mcux_driver_api *)dev->driver_api;
    return api->set_channels(dev, chp, chn);
}

__syscall int cmp_mcux_enable_interrupts(struct device* dev, int interrupts);
static inline int z_impl_cmp_mcux_enable_interrupts(struct device* dev, int interrupts)
{
    struct cmp_mcux_driver_api *api;
    api = (struct cmp_mcux_driver_api *)dev->driver_api;
    return api->enable_interrupts(dev, interrupts);
}

__syscall int cmp_mcux_disable_interrupts(struct device* dev, int interrupts);
static inline int z_impl_cmp_mcux_disable_interrupts(struct device* dev, int interrupts)
{
    struct cmp_mcux_driver_api *api;
    api = (struct cmp_mcux_driver_api *)dev->driver_api;
    return api->disable_interrupts(dev, interrupts);
}

__syscall int cmp_mcux_set_callback(struct device* dev, cmp_mcux_callback_t callback);
static inline int z_impl_cmp_mcux_set_callback(struct device* dev, cmp_mcux_callback_t callback)
{
    struct cmp_mcux_driver_api *api;
    api = (struct cmp_mcux_driver_api *)dev->driver_api;
    return api->set_callback(dev, callback);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#include <syscalls/cmp_mcux.h>

#endif // _ZEPHYR_INCLUDE_DRIVERS_CMP_MCUX_H_