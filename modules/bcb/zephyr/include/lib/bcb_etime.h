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

#ifdef __cplusplus
extern "C" {
#endif

int bcb_etime_init(void);
uint64_t bcb_etime_get_now();
uint32_t bcb_etime_get_frequency();

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif // _BCB_ETIME_H_