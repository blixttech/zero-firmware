#ifndef _BCB_OCP_OTP_H_
#define _BCB_OCP_OTP_H_

#include <stdint.h>
#include <stdbool.h>
#include <sys/slist.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BCB_OCP_DIRECTION_P = 0,
    BCB_OCP_DIRECTION_N,
} bcb_ocp_direction_t;

typedef void (*bcb_ocp_callback_handler_t)(uint64_t duration);
typedef void (*bcb_ocp_test_callback_handler_t)(bcb_ocp_direction_t direction, uint32_t duration);
typedef void (*bcb_otp_callback_handler_t)(int8_t temperature);

struct bcb_ocp_callback {
	sys_snode_t node;
	bcb_ocp_callback_handler_t handler;
};

struct bcb_otp_callback {
	sys_snode_t node;
	bcb_otp_callback_handler_t handler;
};

struct bcb_ocp_test_callback {
	sys_snode_t node;
	bcb_ocp_test_callback_handler_t handler;
};

int bcb_ocp_otp_init(void);
int bcb_ocp_otp_reset(void);
int bcb_ocp_set_limit(uint8_t current);
int bcb_ocp_test_trigger(bcb_ocp_direction_t direction, bool enable);

int bcb_ocp_add_callback(struct bcb_ocp_callback *callback);
int bcb_otp_add_callback(struct bcb_otp_callback *callback);
int bcb_ocp_test_add_callback(struct bcb_ocp_test_callback *callback);
void bcb_ocp_remove_callback(struct bcb_ocp_callback *callback);
void bcb_otp_remove_callback(struct bcb_otp_callback *callback);
void bcb_ocp_test_remove_callback(struct bcb_ocp_test_callback *callback);

#ifdef __cplusplus
}
#endif

#endif /* _BCB_OCP_OTP_H_ */