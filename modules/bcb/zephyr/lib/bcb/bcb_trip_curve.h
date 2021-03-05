#ifndef _BCB_TRIP_CURVE_H_
#define _BCB_TRIP_CURVE_H_

#include "bcb_common.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	BCB_CURVE_STATE_UNDEFINED = 0,
	BCB_CURVE_STATE_OPENED,
	BCB_CURVE_STATE_CLOSED,
	BCB_CURVE_STATE_TRANSIENT,
} bcb_curve_state_t;

typedef enum {
	BCB_TRIP_CAUSE_NONE = 0,
	BCB_TRIP_CAUSE_OCP_HW,
	BCB_TRIP_CAUSE_OCP_SW,
	BCB_TRIP_CAUSE_OCP_TEST,
	BCB_TRIP_CAUSE_OTP,
	BCB_TRIP_CAUSE_UVP
} bcb_trip_cause_t;

typedef enum {
	BCB_CURVE_LIMIT_TYPE_SW = 0,
	BCB_CURVE_LIMIT_TYPE_HW
} bcb_curve_limit_type_t;

struct bcb_trip_curve;

typedef void (*bcb_trip_curve_callback_t)(const struct bcb_trip_curve *curve, bcb_trip_cause_t type);
typedef int (*bcb_trip_curve_init_t)(void);
typedef int (*bcb_trip_curve_shutdown_t)(void);
typedef int (*bcb_trip_curve_close_t)(void);
typedef int (*bcb_trip_curve_open_t)(void);
typedef bcb_curve_state_t (*bcb_trip_curve_get_state_t)(void);
typedef bcb_trip_cause_t (*bcb_trip_curve_get_cause_t)(void);
typedef int (*bcb_trip_curve_set_current_limit_t)(uint8_t limit, bcb_curve_limit_type_t type);
typedef uint8_t (*bcb_trip_curve_get_current_limit_t)(bcb_curve_limit_type_t type);
typedef int (*bcb_trip_curve_set_callback_t)(bcb_trip_curve_callback_t callback);

struct bcb_trip_curve {
	bcb_trip_curve_init_t init;
	bcb_trip_curve_shutdown_t shutdown;
	bcb_trip_curve_close_t close;
	bcb_trip_curve_open_t open;
	bcb_trip_curve_get_state_t get_state;
	bcb_trip_curve_get_cause_t get_cause;
	bcb_trip_curve_set_current_limit_t set_limit;
	bcb_trip_curve_get_current_limit_t get_limit;
	bcb_trip_curve_set_callback_t set_callback;
};

#ifdef __cplusplus
}
#endif

#endif /* _BCB_TRIP_CURVE_H_ */