#ifndef _BCB_MSMNT_H_
#define _BCB_MSMNT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BCB_TEMP_PWR_IN = 0,
    BCB_TEMP_PWR_OUT,
    BCB_TEMP_AMB,
    BCB_TEMP_MCU
} bcb_temp_t;

int32_t bcb_get_temp(bcb_temp_t sensor);
int bcb_msmnt_setup_default();

#ifdef __cplusplus
}
#endif

#endif // _BCB_MSMNT_H_