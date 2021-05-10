#ifndef _BCB_TC_H_
#define _BCB_TC_H_

#include "bcb_common.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bcb_tc bcb_tc_t;

typedef enum {
	BCB_TC_STATE_UNDEFINED = 0,
	BCB_TC_STATE_OPENED, /**< Opened */
	BCB_TC_STATE_CLOSED, /**< Closed */
	BCB_TC_STATE_TRANSIENT, /**< Transient */
} bcb_tc_state_t;

typedef enum {
	BCB_TC_CAUSE_NONE = 0,
	BCB_TC_CAUSE_EXT, /**< External event (e.g. open/closed by user) */
	BCB_TC_CAUSE_OCP_INST, /**<  Instantaneous over current protection */
	BCB_TC_CAUSE_OCP_LONG, /**< Long time over current protection */
	BCB_TC_CAUSE_OCP_SHORT, /**< Short time over current protection */
	BCB_TC_CAUSE_OCP_TEST, /**< Test for instantaneous over current protection */
	BCB_TC_CAUSE_OTP, /**< Over temperature protection */
	BCB_TC_CAUSE_UVP, /**< Under voltage protection */
	BCB_TC_CAUSE_INRUSH_REC /**< Closed by inrush recovery */
} bcb_tc_cause_t;

typedef struct bcb_tc_limit_config {
	uint8_t inst_limit; /**< Instantaneous current limit (A) */
	uint8_t long_limit; /**< Long time current limit (A) */
	uint8_t short_limit; /**< Short time current limit (A) */
	uint8_t pad; /**< Padding */
	uint32_t long_delay; /**< Long time current limit delay (ms) */
	uint32_t short_delay; /**< Short time current limit delay (ms) */
} bcb_tc_limit_config_t;

typedef struct bcb_tc_inrush_config {
	uint8_t rec_enable; /**< Recovery enable/disable */
	uint8_t max_attemps; /**< Maximum number of consecutive recovery attemps */
	uint16_t wait; /**< Time duration to wait after each inrush event (ms) */
} bcb_tc_inrush_config_t;

typedef void (*bcb_tc_callback_t)(const bcb_tc_t *curve, bcb_tc_cause_t cause);
typedef int (*bcb_tc_api_init_t)(void);
typedef int (*bcb_tc_api_shutdown_t)(void);
typedef int (*bcb_tc_api_close_t)(void);
typedef int (*bcb_tc_api_open_t)(void);
typedef bcb_tc_state_t (*bcb_tc_api_get_state_t)(void);
typedef bcb_tc_cause_t (*bcb_tc_api_get_cause_t)(void);
typedef int (*bcb_tc_api_set_limit_config_t)(const bcb_tc_limit_config_t *limit);
typedef int (*bcb_tc_api_get_limit_config_t)(bcb_tc_limit_config_t *limit);
typedef int (*bcb_tc_api_set_inrush_config_t)(const bcb_tc_inrush_config_t *config);
typedef int (*bcb_tc_api_get_inrush_config_t)(bcb_tc_inrush_config_t *config);
typedef int (*bcb_tc_api_set_callback_t)(bcb_tc_callback_t callback);

/**
 * This structure represents the trip curve.
 * Any function in this structure should not be called from threads or interrupts.
 * In other words, they should be called only from the main thread. 
 */
struct bcb_tc {
	bcb_tc_api_init_t init;
	bcb_tc_api_shutdown_t shutdown;
	bcb_tc_api_close_t close;
	bcb_tc_api_open_t open;
	bcb_tc_api_get_state_t get_state;
	bcb_tc_api_get_cause_t get_cause;
	bcb_tc_api_set_limit_config_t set_limit_config;
	bcb_tc_api_get_limit_config_t get_limit_config;
	bcb_tc_api_set_inrush_config_t set_inrush_config;
	bcb_tc_api_get_inrush_config_t get_inrush_config;
	bcb_tc_api_set_callback_t set_callback;
};

#ifdef __cplusplus
}
#endif

#endif /* _BCB_TC_H_ */