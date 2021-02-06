#ifndef _BCB_H_
#define _BCB_H_

#include "bcb_common.h"
#include "bcb_trip_curve.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*bcb_trip_callback_t)(bcb_trip_cause_t type, uint8_t limit);

int bcb_init(void);
int bcb_on(void);
int bcb_off(void);
bool bcb_is_on();
int32_t bcb_get_temp(bcb_temp_sensor_t sensor);
int32_t bcb_get_voltage(void);
int32_t bcb_get_current(void);
uint32_t bcb_get_voltage_rms(void);
uint32_t bcb_get_current_rms(void);

int bcb_set_trip_curve(const struct bcb_trip_curve *curve);
int bcb_set_trip_limit(uint8_t limit);
uint8_t bcb_get_trip_limit(void);
//void bcb_add_trip_callback(bcb_trip_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // _BCB_H_