#ifndef _BCB_TRIP_CURVE_H_
#define _BCB_TRIP_CURVE_H_

#include "bcb_common.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bcb_trip_curve;

typedef void (*bcb_trip_curve_callback_t)(const struct bcb_trip_curve *curve, bcb_trip_cause_t type,
					  uint8_t limit);
typedef int (*bcb_trip_curve_init_t)(void);
typedef int (*bcb_trip_curve_shutdown_t)(void);
typedef int (*bcb_trip_curve_on_t)(void);
typedef int (*bcb_trip_curve_off_t)(void);
typedef int (*bcb_trip_curve_set_current_limit_t)(uint8_t limit);
typedef uint8_t (*bcb_trip_curve_get_current_limit_t)(void);
typedef void (*bcb_trip_curve_set_callback_t)(bcb_trip_curve_callback_t callback);

struct bcb_trip_curve {
	bcb_trip_curve_init_t init;
	bcb_trip_curve_shutdown_t shutdown;
	bcb_trip_curve_on_t on;
	bcb_trip_curve_off_t off;
	bcb_trip_curve_set_current_limit_t set_limit;
	bcb_trip_curve_get_current_limit_t get_limit;
	bcb_trip_curve_set_callback_t set_callback;
};

#ifdef __cplusplus
}
#endif

#endif /* _BCB_TRIP_CURVE_H_ */