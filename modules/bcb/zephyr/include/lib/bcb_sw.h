#ifndef _BCB_SW_H_
#define _BCB_SW_H_

#include <stdbool.h>
#include <sys/slist.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BCB_SW_CAUSE_NONE = 0,
    BCB_SW_CAUSE_EXT,
    BCB_SW_CAUSE_OCP,
    BCB_SW_CAUSE_OTP,
    BCB_SW_CAUSE_OCP_TEST,
    BCB_SW_CAUSE_UVP,
} bcb_sw_cause_t;

typedef enum {
    BCB_OCP_DIRECTION_POSITIVE = 0,
    BCB_OCP_DIRECTION_NEGATIVE,
} bcb_ocp_direction_t;

typedef void (*bcb_sw_callback_handler_t)(bool is_closed, bcb_sw_cause_t cause);

typedef struct bcb_sw_callback {
	sys_snode_t node;
	bcb_sw_callback_handler_t handler;
} bcb_sw_callback_t;

int bcb_sw_init(void);
int bcb_sw_on(void);
int bcb_sw_off(void);
bool bcb_sw_is_on();
uint32_t bcb_sw_get_on_off_duration(void);
bcb_sw_cause_t bcb_sw_get_cause(void);
int bcb_sw_add_callback(bcb_sw_callback_t *callback);
void bcb_sw_remove_callback(bcb_sw_callback_t *callback);

int bcb_ocp_set_limit(uint8_t current);
int bcb_ocp_test_trigger(bcb_ocp_direction_t direction, bool enable);
bcb_ocp_direction_t bcb_ocp_test_get_direction(void);
uint32_t bcb_ocp_test_get_duration(void);

#ifdef __cplusplus
}
#endif

#endif /* _BCB_SW_H_ */