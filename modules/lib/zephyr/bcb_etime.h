/**
 * @file
 * @brief Header file for the elapsed time timer functions.
 */

#ifndef _BCB_ETIME_H_
#define _BCB_ETIME_H_

/**
 * @brief Elapsed Time Timer Interface
 * @addtogroup bcb_lib_interface Blixt Circuit Breaker Library Interface
 * @ingroup io_interfaces
 * @{
 */

#include <stdint.h>
#include <autoconf.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BCB_ETIME_TICKS_TO_US(ticks)    ((ticks) * USEC_PER_SEC / CONFIG_BCB_LIB_ETIME_SECOND)
#define BCB_ETIME_US_TO_TICKS(us)       ((us) * CONFIG_BCB_LIB_ETIME_SECOND / USEC_PER_SEC)

uint64_t bcb_etime_get_now();

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif // _BCB_ETIME_H_