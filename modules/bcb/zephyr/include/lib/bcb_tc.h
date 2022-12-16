#ifndef _BCB_TC_H_
#define _BCB_TC_H_

#include <lib/bcb_common.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	BCB_TC_STATE_UNDEFINED = 0,
	BCB_TC_STATE_OPENED,
	BCB_TC_STATE_CLOSED,
	BCB_TC_STATE_TRANSIENT,
} bcb_tc_state_t;

typedef enum {
	BCB_TC_CAUSE_NONE = 0,
	BCB_TC_CAUSE_EXT,
	BCB_TC_CAUSE_OCP_HW,
	BCB_TC_CAUSE_OCP_SW,
	BCB_TC_CAUSE_OCP_TEST,
	BCB_TC_CAUSE_OTP,
	BCB_TC_CAUSE_UVP
} bcb_tc_cause_t;

typedef struct __attribute__((packed)) bcb_tc_pt {
	uint16_t i; /**< Current (A) */
	uint16_t d; /**< Duration (this is a scaled value) */
} bcb_tc_pt_t;

typedef struct bcb_tc bcb_tc_t;

typedef void (*bcb_tc_callback_handler_t)(const bcb_tc_t *curve, bcb_tc_cause_t type);
typedef int (*bcb_tc_init_t)(void);
typedef int (*bcb_tc_shutdown_t)(void);
typedef int (*bcb_tc_close_t)(void);
typedef int (*bcb_tc_open_t)(void);
typedef bcb_tc_state_t (*bcb_tc_state_get_t)(void);
typedef bcb_tc_cause_t (*bcb_tc_cause_get_t)(void);
typedef int (*bcb_tc_limit_hw_set_t)(uint8_t limit);
typedef uint8_t (*bcb_tc_limit_hw_get_t)(void);
typedef int (*bcb_tc_callback_set_t)(bcb_tc_callback_handler_t callback);
typedef int (*bcb_tc_points_set_t)(const bcb_tc_pt_t *data, uint16_t points);
typedef int (*bcb_tc_points_get_t)(bcb_tc_pt_t *data, uint16_t points);
typedef uint16_t (*bcb_tc_point_count_get_t)(void);

struct bcb_tc {
	bcb_tc_init_t init;
	bcb_tc_shutdown_t shutdown;
	bcb_tc_close_t close;
	bcb_tc_open_t open;
	bcb_tc_state_get_t get_state;
	bcb_tc_cause_get_t get_cause;
	bcb_tc_limit_hw_set_t set_limit_hw;
	bcb_tc_limit_hw_get_t get_limit_hw;
	bcb_tc_callback_set_t set_callback;
	bcb_tc_points_set_t set_points;
	bcb_tc_points_get_t get_points;
	bcb_tc_point_count_get_t get_point_count;
};

#ifdef __cplusplus
}
#endif

#endif /* _BCB_TC_H_ */