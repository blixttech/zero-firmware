#ifndef _BCB_SW_H_
#define _BCB_SW_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int bcb_sw_init(void);
int bcb_sw_on(void);
int bcb_sw_off(void);
bool bcb_sw_is_on();
bool bcb_sw_is_on_cmd_active();

#ifdef __cplusplus
}
#endif

#endif /* _BCB_SW_H_ */