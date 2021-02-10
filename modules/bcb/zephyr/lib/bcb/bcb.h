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
int bcb_on(void);
int bcb_off(void);
bool bcb_is_on();

int bcb_set_trip_curve(const struct bcb_trip_curve *curve);
int bcb_set_trip_limit(uint8_t limit);
uint8_t bcb_get_trip_limit(void);
int bcb_add_trip_callback(struct bcb_trip_callback *callback);

#ifdef __cplusplus
}
#endif

#endif // _BCB_H_