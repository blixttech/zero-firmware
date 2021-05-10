#ifndef _BCB_COMMON_H_
#define _BCB_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BCB_TEMP_SENSOR_PWR_IN = 0,
    BCB_TEMP_SENSOR_PWR_OUT,
    BCB_TEMP_SENSOR_AMB,
    BCB_TEMP_SENSOR_MCU
} bcb_temp_sensor_t;

#ifdef __cplusplus
}
#endif

#endif // _BCB_COMMON_H_