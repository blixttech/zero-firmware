#ifndef _BCB_CTRL_H_
#define _BCB_CTRL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	BCB_OCP_TEST_TGR_DIR_P	0
#define	BCB_OCP_TEST_TGR_DIR_N	1

typedef void (*bcb_ocp_callback_t)(int32_t duration);
typedef void (*bcb_otp_callback_t)();

int bcb_on();
int bcb_off();
int bcb_is_on();
int bcb_reset();
int bcb_set_ocp_limit(uint32_t i_ma);
void bcb_set_ocp_callback(bcb_ocp_callback_t callback);
void bcb_set_otp_callback(bcb_otp_callback_t callback);
int bcb_ocp_set_test_current(uint32_t i_ma);
int bcb_ocp_test_trigger(int direction);

#ifdef __cplusplus
}
#endif


#endif // _BCB_CTRL_H_