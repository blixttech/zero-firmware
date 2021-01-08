#ifndef _BCB_MSMNT_H_
#define _BCB_MSMNT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BCB_TEMP_SENSOR_PWR_IN = 0,
    BCB_TEMP_SENSOR_PWR_OUT,
    BCB_TEMP_SENSOR_AMB,
    BCB_TEMP_SENSOR_MCU
} bcb_temp_sensor_t;

typedef enum {
    BCB_MSMNT_TYPE_I_LOW_GAIN = 0,
    BCB_MSMNT_TYPE_I_HIGH_GAIN,
    BCB_MSMNT_TYPE_V_MAINS,
    BCB_MSMNT_TYPE_T_PWR_IN,
    BCB_MSMNT_TYPE_T_PWR_OUT,
    BCB_MSMNT_TYPE_T_AMB,
    BCB_MSMNT_TYPE_T_MCU,
    BCB_MSMNT_TYPE_REV_IN,
    BCB_MSMNT_TYPE_REV_OUT,
    BCB_MSMNT_TYPE_REV_CTRL,
    BCB_MSMNT_TYPE_OCP_TEST_ADJ,
    BCB_MSMNT_TYPE_V_REF_1V5
} bcb_msmnt_type_t;

int32_t bcb_get_temp(bcb_temp_sensor_t sensor);
int32_t bcb_get_voltage();
int32_t bcb_get_current();
int32_t bcb_get_voltage_rms();
int32_t bcb_get_current_rms();

int bcb_msmnt_setup_default();
uint16_t bcb_msmnt_get_raw(bcb_msmnt_type_t type);
int bcb_msmnt_set_cal_params(bcb_msmnt_type_t type, uint16_t a, uint16_t b);
int bcb_msmnt_get_cal_params(bcb_msmnt_type_t type, uint16_t *a, uint16_t *b);
void bcb_msmnt_rms_start(uint8_t interval);
void bcb_msmnt_rms_stop();

#ifdef __cplusplus
}
#endif

#endif // _BCB_MSMNT_H_