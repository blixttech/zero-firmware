#ifndef _BCB_H_
#define _BCB_H_

#include "bcb_common.h"
#include "bcb_trip_curve.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/slist.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bcb_trip_callback {
	sys_snode_t node;
	bcb_trip_curve_callback_t handler;
};

int bcb_init(void);
int bcb_close(void);
int bcb_open(void);
int bcb_toggle(void);
bcb_curve_state_t bcb_get_state(void);
bcb_trip_cause_t bcb_get_cause(void);
int bcb_set_trip_curve(const struct bcb_trip_curve *curve);
const struct bcb_trip_curve * bcb_get_trip_curve(void);
int bcb_add_trip_callback(struct bcb_trip_callback *callback);

#ifdef __cplusplus
}
#endif

#endif // _BCB_H_