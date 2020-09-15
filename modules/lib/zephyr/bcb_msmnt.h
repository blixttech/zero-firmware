#ifndef _BCB_MSMNT_H_
#define _BCB_MSMNT_H_

#include <stdint.h>
#include <autoconf.h>
#include <sys_clock.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    BCB_MSMNT_TEMP_PWR_IN = 0,
    BCB_MSMNT_TEMP_PWR_OUT,
    BCB_MSMNT_TEMP_AMB,
    BCB_MSMNT_TEMP_MCU
} bcb_msmnt_temp_t;


int32_t bcb_msmnt_get_temp(bcb_msmnt_temp_t sensor);


int32_t bcb_msmnt_get_current_l();
int32_t bcb_msmnt_get_current_h();
int32_t bcb_msmnt_get_voltage();
int32_t bcb_msmnt_get_current_l_rms();
int32_t bcb_msmnt_get_current_h_rms();
int32_t bcb_msmnt_get_voltage_rms();

#ifdef __cplusplus
}
#endif

#endif // _BCB_MSMNT_H_