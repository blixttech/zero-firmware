#ifndef _BCB_OCP_OTP_H_
#define _BCB_OCP_OTP_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BCB_OCP_DIRECTION_P = 0,
    BCB_OCP_DIRECTION_N,
} bcb_ocp_direction_t;

typedef void (*bcb_ocp_callback_t)(uint64_t duration);
typedef void (*bcb_ocp_test_callback_t)(bcb_ocp_direction_t direction, uint32_t duration);
typedef void (*bcb_otp_callback_t)(int8_t temperature);

int bcb_ocp_otp_init(void);
int bcb_ocp_otp_reset(void);
void bcb_ocp_set_callback(bcb_ocp_callback_t callback);
void bcb_otp_set_callback(bcb_otp_callback_t callback);
int bcb_ocp_set_limit(uint8_t current);
int bcb_ocp_test_trigger(bcb_ocp_direction_t direction, bool enable);
void bcb_otp_test_set_callback(bcb_ocp_test_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif /* _BCB_OCP_OTP_H_ */