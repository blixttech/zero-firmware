#ifndef _BCB_MSMNT_CALIB_H_
#define _BCB_MSMNT_CALIB_H_

#include "bcb_msmnt.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int bcb_msmnt_calib_adc(uint16_t samples);
int bcb_msmnt_calib_a(bcb_msmnt_type_t type, int32_t x, uint16_t samples);
int bcb_msmnt_calib_b(bcb_msmnt_type_t type, uint16_t samples);

#ifdef __cplusplus
}
#endif

#endif /* _BCB_MSMNT_CALIB_H_ */