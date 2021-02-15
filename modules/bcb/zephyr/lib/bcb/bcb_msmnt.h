#ifndef _BCB_MSMNT_H_
#define _BCB_MSMNT_H_

#include <stdint.h>
#include "bcb_common.h"

#ifdef __cplusplus
extern "C" {
#endif

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

int bcb_msmnt_init(void);
int bcb_msmnt_config_load(void);
int bcb_msmnt_config_store(void);

int32_t bcb_msmnt_get_temp(bcb_temp_sensor_t sensor);
int32_t bcb_msmnt_get_voltage(void);
int32_t bcb_msmnt_get_current(void);
uint32_t bcb_msmnt_get_voltage_rms(void);
uint32_t bcb_msmnt_get_current_rms(void);
uint16_t bcb_msmnt_get_raw(bcb_msmnt_type_t type);

int bcb_msmnt_set_calib_param_a(bcb_msmnt_type_t type, uint16_t a);
int bcb_msmnt_set_calib_param_b(bcb_msmnt_type_t type, uint16_t b);
int bcb_msmnt_get_calib_param_a(bcb_msmnt_type_t type, uint16_t *a);
int bcb_msmnt_get_calib_param_b(bcb_msmnt_type_t type, uint16_t *b);

int bcb_msmnt_start(void);
int bcb_msmnt_stop(void);
void bcb_msmnt_rms_start(uint8_t interval);
void bcb_msmnt_rms_stop(void);

#ifdef __cplusplus
}
#endif

#endif // _BCB_MSMNT_H_