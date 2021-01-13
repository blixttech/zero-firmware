/**
 * @file
 * @brief Header file for ADC trigger driver interface.
 */

#ifndef _ZEPHYR_INCLUDE_DRIVERS_ADC_TRIGGER_H_
#define _ZEPHYR_INCLUDE_DRIVERS_ADC_TRIGGER_H_

#include <stdint.h>
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Driver APIs for ADC trigger
 * @defgroup adc_trigger ADC driver APIs
 * @ingroup io_interfaces
 * @{
 */

typedef int         (*adc_trigger_api_start)(struct device *dev);
typedef int         (*adc_trigger_api_stop)(struct device *dev);
typedef uint32_t    (*adc_trigger_api_get_interval)(struct device *dev);
typedef int         (*adc_trigger_api_set_interval)(struct device *dev, uint32_t us);

/**
 * @brief ADC trigger driver API
 *
 * This is the mandatory API any ADC trigger driver needs to expose.
 */
__subsystem struct adc_trigger_driver_api {
    adc_trigger_api_start           start;
    adc_trigger_api_stop            stop;
    adc_trigger_api_get_interval    get_interval;
    adc_trigger_api_set_interval    set_interval;
};

__syscall int adc_trigger_start(struct device *dev);
static inline int z_impl_adc_trigger_start(struct device *dev)
{
    struct adc_trigger_driver_api *api;
    api = (struct adc_trigger_driver_api *)dev->driver_api;
    return api->start(dev);
}

__syscall int adc_trigger_stop(struct device *dev);
static inline int z_impl_adc_trigger_stop(struct device *dev)
{
    struct adc_trigger_driver_api *api;
    api = (struct adc_trigger_driver_api *)dev->driver_api;
    return api->stop(dev);
}

__syscall uint32_t adc_trigger_get_interval(struct device *dev);
static inline uint32_t z_impl_adc_trigger_get_interval(struct device *dev)
{
    struct adc_trigger_driver_api *api;
    api = (struct adc_trigger_driver_api *)dev->driver_api;
    return api->get_interval(dev);
}

__syscall int adc_trigger_set_interval(struct device *dev, uint32_t us);
static inline int z_impl_adc_trigger_set_interval(struct device *dev, uint32_t us)
{
    struct adc_trigger_driver_api *api;
    api = (struct adc_trigger_driver_api *)dev->driver_api;
    return api->set_interval(dev, us);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#include <syscalls/adc_trigger.h>

#endif // _ZEPHYR_INCLUDE_DRIVERS_ADC_TRIGGER_H_