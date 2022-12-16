#ifndef _BCB_H_
#define _BCB_H_

#include <lib/bcb_common.h>
#include <lib/bcb_tc.h>
#include <sys/slist.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bcb_tc_callback {
	sys_snode_t node;
	bcb_tc_callback_handler_t handler;
} bcb_tc_callback_t;

int bcb_init(void);
int bcb_close(void);
int bcb_open(void);
int bcb_toggle(void);
bcb_tc_state_t bcb_get_state(void);
bcb_tc_cause_t bcb_get_cause(void);
int bcb_set_tc(const struct bcb_tc *curve);
const struct bcb_tc * bcb_get_tc(void);
int bcb_add_tc_callback(bcb_tc_callback_t *callback);

#ifdef __cplusplus
}
#endif

#endif // _BCB_H_