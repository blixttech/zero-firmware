/**
 * @file
 * @brief Header file for PIT specific APIs of the count down counter driver.
 */

#ifndef _ZEPHYR_INCLUDE_DRIVERS_COUNTER_CTD_MCUX_PIT_H_
#define _ZEPHYR_INCLUDE_DRIVERS_COUNTER_CTD_MCUX_PIT_H_

/**
 * @ingroup io_interfaces
 * @{
 */

#include <stdint.h>
#include <stddef.h>
#include <device.h>
#include <errno.h>
#include <drivers/counter_ctd.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enable chaining a counter in the Down Counter Array.
 * 
 * @param[in] dev           A pointer to the device structure for the driver instance. 
 * @param[in] chan_id       Channel ID.
 * @param[in] enable        True if the counter needs to be chained.
 * @retval 0 If successful.  
 */
__syscall int counter_ctd_mcux_pit_chain(struct device *dev, uint8_t chan_id, bool enable);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#include <syscalls/counter_ctd_mcux_pit.h>

#endif // __ZEPHYR_INCLUDE_DRIVERS_COUNTER_CTD_MCUX_PIT__