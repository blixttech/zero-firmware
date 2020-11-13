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

typedef struct bcb_msmnt_cal_params {
    uint16_t i_lg_zero_raw;
    uint16_t i_hg_zero_raw;
    uint16_t v_zero_raw;
} bcb_msmnt_cal_params_t ;

typedef void (*bcb_msmnt_cal_callback_t)();
#if CONFIG_BCB_LIB_ADC_DUMP_ENABLE
typedef void (*bcb_msmnt_adc_dump_callback_t)();
#endif // CONFIG_BCB_LIB_ADC_DUMP_ENABLE

int32_t bcb_msmnt_get_temp(bcb_msmnt_temp_t sensor);
//int32_t bcb_msmnt_get_current_l();
//int32_t bcb_msmnt_get_current_h();
//int32_t bcb_msmnt_get_voltage();
//int32_t bcb_msmnt_get_current_l_rms();
//int32_t bcb_msmnt_get_current_h_rms();
//int32_t bcb_msmnt_get_voltage_rms();

int32_t bcb_msmnt_get_current();
int32_t bcb_msmnt_get_voltage();
int32_t bcb_msmnt_get_current_rms();
int32_t bcb_msmnt_get_voltage_rms();

uint16_t bcb_msmnt_get_temp_raw(bcb_msmnt_temp_t sensor);
uint16_t bcb_msmnt_get_i_lg_raw();
uint16_t bcb_msmnt_get_i_hg_raw();
uint16_t bcb_msmnt_get_v_raw();
uint16_t bcb_msmnt_get_v_ref_raw();
uint32_t bcb_msmnt_get_i_lg_sq_acc_raw();
uint32_t bcb_msmnt_get_i_hg_sq_acc_raw();
uint32_t bcb_msmnt_get_v_sq_acc_raw();

int bcb_msmnt_cal_start(bcb_msmnt_cal_callback_t callback);
int bcb_msmnt_cal_stop();

#if CONFIG_BCB_LIB_ADC_DUMP_ENABLE
int bcb_msmnt_adc_dump_sstart(bcb_msmnt_adc_dump_callback_t callback);
int bcb_msmnt_adc_dump_dstart(bcb_msmnt_adc_dump_callback_t callback);
int bcb_msmnt_adc_dump_stop();
#endif // CONFIG_BCB_LIB_ADC_DUMP_ENABLE

#ifdef __cplusplus
}
#endif

#endif // _BCB_MSMNT_H_