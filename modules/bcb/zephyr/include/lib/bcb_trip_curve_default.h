#ifndef _BCB_TRIP_CURVE_DEFAULT_H_
#define _BCB_TRIP_CURVE_DEFAULT_H_

#include "bcb_trip_curve.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int bcb_trip_curve_default_set_recovery(uint16_t attempts);
uint16_t bcb_trip_curve_default_get_recovery(void);
const struct bcb_trip_curve* bcb_trip_curve_get_default(void);

#ifdef __cplusplus
}
#endif

#endif /* _BCB_TRIP_CURVE_DEFAULT_H_ */