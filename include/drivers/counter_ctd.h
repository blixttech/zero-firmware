/**
 * @file
 * @brief Public API header file for count down counter driver.
 */

#ifndef _ZEPHYR_INCLUDE_DRIVERS_COUNTER_CTD_H_
#define _ZEPHYR_INCLUDE_DRIVERS_COUNTER_CTD_H_

/**
 * @brief Count Down Counter Interface.
 * @defgroup counter_ctd_interface Count Down Counter Interface
 * @ingroup io_interfaces
 * @{
 */

#include <errno.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Alarm callback
 * @param dev           Pointer to the device structure for the driver instance.
 * @param chan_id    Channel ID.
 * @param user_data     A pointer to the user data. 
 */
typedef void (*ctd_alarm_callback_t)(struct device* dev, u8_t chan_id, void* user_data);

/**
 * @brief Alarm callback structure.
 * @param callback      Callback called on alarm (cannot be NULL).
 * @param user_data     User data returned in callback.
 */
struct ctd_alarm_cfg {
    ctd_alarm_callback_t callback;
    void* user_data;
};

typedef int (*counter_ctd_api_start)(struct device* dev, u8_t chan_id);
typedef int (*counter_ctd_api_stop)(struct device* dev, u8_t chan_id);
typedef u32_t (*counter_ctd_api_get_value)(struct device* dev, u8_t chan_id);
typedef int (*counter_ctd_api_set_top_value)(struct device* dev, u8_t chan_id, u32_t ticks);
typedef u32_t (*counter_ctd_api_get_top_value)(struct device* dev, u8_t chan_id);
typedef int (*counter_ctd_api_set_alarm)(struct device* dev, u8_t chan_id, const struct ctd_alarm_cfg* cfg);
typedef int (*counter_ctd_api_cancel_alarm)(struct device* dev, u8_t chan_id);
typedef u64_t (*counter_ctd_api_ns_to_ticks)(struct device* dev, u64_t ns);
typedef u64_t (*counter_ctd_api_ticks_to_ns)(struct device* dev, u64_t ticks);
typedef u32_t (*counter_ctd_api_get_frequency)(struct device *dev);

__subsystem struct counter_ctd_driver_api {
    counter_ctd_api_start start;
    counter_ctd_api_stop stop;
    counter_ctd_api_get_value get_value;
    counter_ctd_api_set_top_value set_top_value;
    counter_ctd_api_get_top_value get_top_value;
    counter_ctd_api_set_alarm set_alarm;
    counter_ctd_api_cancel_alarm cancel_alarm;
    counter_ctd_api_ns_to_ticks ns_to_ticks;
    counter_ctd_api_ticks_to_ns ticks_to_ns;
    counter_ctd_api_get_frequency get_frequency;
};

/**
 * @brief Start a counter in the Down Counter Array.
 * 
 * @param[in] dev           A pointer to the device structure for the driver instance. 
 * @param[in] chan_id        Channel ID.
 * @retval 0 If successful.  
 */
__syscall int counter_ctd_start(struct device* dev, u8_t chan_id);
static inline int z_impl_counter_ctd_start(struct device* dev, u8_t chan_id)
{
    struct counter_ctd_driver_api *api;
    api = (struct counter_ctd_driver_api *)dev->driver_api;
    return api->start(dev, chan_id);
}

/**
 * @brief Stop a counter in the Down Counter Array.
 * 
 * @param[in] dev           A pointer to the device structure for the driver instance. 
 * @param[in] chan_id        Channel ID.
 * @retval 0 If successful.  
 */
__syscall int counter_ctd_stop(struct device* dev, u8_t chan_id);
static inline int z_impl_counter_ctd_stop(struct device* dev, u8_t chan_id)
{
    struct counter_ctd_driver_api *api;
    api = (struct counter_ctd_driver_api *)dev->driver_api;
    return api->stop(dev, chan_id);
}

/**
 * @brief Get the current value of a counter in the Down Counter Array.
 * 
 * @param[in] dev           A pointer to the device structure for the driver instance.
 * @param[in] chan_id        Channel ID.
 * @retval Value of the counter in ticks. 
 */
__syscall u32_t counter_ctd_get_value(struct device* dev, u8_t chan_id);
static inline u32_t z_impl_counter_ctd_get_value(struct device* dev, u8_t chan_id)
{
    struct counter_ctd_driver_api *api;
    api = (struct counter_ctd_driver_api *)dev->driver_api;
    return api->get_value(dev, chan_id);
}

/**
 * @brief Set the top value of a counter in the Down Counter Array.
 * 
 * @param[in] dev           A pointer to the device structure for the driver instance.
 * @param[in] chan_id        Channel ID.
 * @param[in] ticks         Top value of the counter in ticks.
 * @retval 0 If successful. 
 */
__syscall int counter_ctd_set_top_value(struct device* dev, u8_t chan_id, u32_t ticks);
static inline int z_impl_counter_ctd_set_top_value(struct device* dev, u8_t chan_id, u32_t ticks)
{
    struct counter_ctd_driver_api *api;
    api = (struct counter_ctd_driver_api *)dev->driver_api;
    return api->set_top_value(dev, chan_id, ticks);
}

/**
 * @brief Get the top value of a counter in the Down Counter Array.
 * 
 * @param[in]   dev            A pointer to the device structure for the driver instance.
 * @param[in]   chan_id        Channel ID.
 * @retval Top value of the counter in ticks.
 */
__syscall u32_t counter_ctd_get_top_value(struct device* dev, u8_t chan_id);
static inline u32_t z_impl_counter_ctd_get_top_value(struct device* dev, u8_t chan_id)
{
    struct counter_ctd_driver_api *api;
    api = (struct counter_ctd_driver_api *)dev->driver_api;
    return api->get_top_value(dev, chan_id);
}

/**
 * @brief Set an alarm to a counter in the Down Counter Array.
 * 
 * @param[in] dev           A pointer to the device structure for the driver instance.
 * @param[in] chan_id        Channel ID.
 * @param[in] cfg           Alarm configuration.
 * @retval 0 If successful. 
 */
__syscall int counter_ctd_set_alarm(struct device* dev, u8_t chan_id, const struct ctd_alarm_cfg* cfg);
static inline int z_impl_counter_ctd_set_alarm(struct device* dev, u8_t chan_id, const struct ctd_alarm_cfg* cfg)
{
    struct counter_ctd_driver_api *api;
    api = (struct counter_ctd_driver_api *)dev->driver_api;
    return api->set_alarm(dev, chan_id, cfg);
}

/**
 * @brief Cancel the alarm of a counter in the Down Counter Array.
 * 
 * @param[in] dev           A pointer to the device structure for the driver instance.
 * @param[in] chan_id        Channel ID.
 * @retval 0 If successful. 
 */
__syscall int counter_ctd_cancel_alarm(struct device* dev, u8_t chan_id);
static inline int z_impl_counter_ctd_cancel_alarm(struct device* dev, u8_t chan_id)
{
    struct counter_ctd_driver_api *api;
    api = (struct counter_ctd_driver_api *)dev->driver_api;
    return api->cancel_alarm(dev, chan_id);
}

/**
 * @brief Function to convert ticks to nanoseconds.
 * 
 * @param[in] dev    A pointer to the device structure for the driver instance.
 * @param[in] ns    Duration in nanoseconds.
 * @return Ticks in nanoseconds.
 */
__syscall u64_t counter_ctd_ns_to_ticks(struct device* dev, u64_t ns);
static inline u64_t z_impl_counter_ctd_ns_to_ticks(struct device* dev, u64_t ns)
{
    struct counter_ctd_driver_api *api;
    api = (struct counter_ctd_driver_api *)dev->driver_api;
    if (api->ns_to_ticks) {
        return api->ns_to_ticks(dev, ns);
    }
    return 0;
}

/**
 * @brief Function to convert nanoseconds to ticks.
 * 
 * @param[in] dev     A pointer to the device structure for the driver instance.
 * @param[in] ticks    Duration in timer ticks. 
 * @return  Nanoseconds in ticks.
 */
__syscall u64_t counter_ctd_ticks_to_ns(struct device* dev, u64_t ticks);
static inline u64_t z_impl_counter_ctd_ticks_to_ns(struct device* dev, u64_t ticks)
{
    struct counter_ctd_driver_api *api;
    api = (struct counter_ctd_driver_api *)dev->driver_api;
    if (api->ticks_to_ns) {
        return api->ticks_to_ns(dev, ticks);
    }
    return 0;
}

/**
 * @brief Function to get the clock frequency the count down counter.
 * 
 * @param[in] dev    A pointer to the device structure for the driver instance. 
 * @return    Number of ticks per second.
 */
__syscall u32_t counter_ctd_get_frequency(struct device *dev);
static inline u32_t z_impl_counter_ctd_get_frequency(struct device *dev)
{
    struct counter_ctd_driver_api *api;
    api = (struct counter_ctd_driver_api *)dev->driver_api;
    if (api->get_frequency) {
        return api->get_frequency(dev);
    }
    return 0;
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#include <syscalls/counter_ctd.h>

#endif // _ZEPHYR_INCLUDE_DRIVERS_COUNTER_CTD_H_