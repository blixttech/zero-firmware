#ifndef _BCB_CTRL_H_
#define _BCB_CTRL_H_

#include <stdint.h>
#include <autoconf.h>
#include <sys_clock.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @def BCB_OCP_TEST_TGR_DIR_P
 * Overcurrent protection test current direction positive.  
 */
#define	BCB_OCP_TEST_TGR_DIR_P	0
/**
 * @def BCB_OCP_TEST_TGR_DIR_N
 * Overcurrent protection test current direction negative. 
 */

#define	BCB_OCP_TEST_TGR_DIR_N	1

/**
 * @brief Convert ON/OFF duration time ticks to microseconds
 * @param ticks Ticks    
 */
#define BCB_ONOFF_TICKS_TO_US(ticks)     ((ticks) * USEC_PER_SEC / CONFIG_BCB_LIB_IC_ONOFF_SECOND)
/**
 * @brief Convert microseconds to ON/OFF duration time ticks
 * @param us    Time in microseconds
 */
#define BCB_ONOFF_US_TO_TICKS(us)        ((us) * CONFIG_BCB_LIB_IC_ONOFF_SECOND / USEC_PER_SEC)

/**
 * @brief Callback function to be used when overcurrent protection is activated.
 * @param onoff_ticks   Time duration between ON and OFF events in time ticks.
 */
typedef void (*bcb_ocp_callback_t)(uint64_t onoff_ticks);
/**
 * @brief Callback function to be used when over temperation protection is activated.
 */
typedef void (*bcb_otp_callback_t)();

/**
 * @brief Turn on the breaker.
 * 
 * If the breaker is already on, this function simply returns 0.
 * Inside this function, a reset for the overcurrent and overtemperature protection 
 * is done before the actual turn on happens. @sa bcb_reset()
 * 
 * @return 0 if successful.  
 */
int bcb_on();

/**
 * @brief Turn off the breaker.
 * 
 * @return 0 if successful. 
 */
int bcb_off();

/**
 * @brief Check if the breaker is turned on.
 * 
 * @return 1 if the breaker is turned on.
 */
int bcb_is_on();

/**
 * @brief Reset the overcurrent and the overtemperature protection.
 * 
 * If the overcurrent or the overtemperature protection is already triggered,
 * this function can be used to reset the trigger status.
 * Note that this fuction does not turn off the breaker. 
 * The breaker will be turned on again if this function is called just after the   
 * overcurrent or the overtemperature protection triggered.
 * If the breaker needs to be turned off, call @sa bcb_off() explicitly. 
 * 
 * @return Only for future use. Can be ignored. 
 */
int bcb_reset();

/**
 * @brief Set the overcurrent protection limit.
 * 
 * @param i_ma Current limit in milliamperes.
 * @return 0 if successfull. 
 */
int bcb_set_ocp_limit(uint32_t i_ma);

/**
 * @brief Set the callback function for overcurrent protection trigger.
 * 
 * @param callback A valid handler function pointer.
 */
void bcb_set_ocp_callback(bcb_ocp_callback_t callback);

/**
 * @brief Set the callback function for overtemperature protection trigger.
 * 
 * @param callback A valid handler function pointer.
 */
void bcb_set_otp_callback(bcb_otp_callback_t callback);

/**
 * @brief Set the test current used for overcurrent protection test.
 * 
 * @param i_ma The current in milliamperes.
 * @return 0 if successfull. 
 */
int bcb_ocp_set_test_current(uint32_t i_ma);

/**
 * @brief Trigger the overcurrent protection test.
 * 
 * @param direction The direction of the current flow
 * @return 0 if the function id called with correct parameters.
 */
int bcb_ocp_test_trigger(int direction);

#ifdef __cplusplus
}
#endif


#endif // _BCB_CTRL_H_