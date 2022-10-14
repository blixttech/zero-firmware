#ifndef _BCB_ZD_H_
#define _BCB_ZD_H_

#include <stdint.h>
#include <sys/slist.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*bcb_zd_callback_handler_t)(void);

typedef enum { BCB_ZD_TYPE_VOLTAGE = 0, BCB_ZD_TYPE_CURRENT } bcb_zd_type_t;

struct bcb_zd_callback {
	sys_snode_t node;
	bcb_zd_callback_handler_t handler;
};

int bcb_zd_init(void);
uint32_t bcb_zd_get_frequency(void);
int bcb_zd_voltage_add_callback(struct bcb_zd_callback *callback);
int bcb_zd_add_callback(bcb_zd_type_t type, struct bcb_zd_callback *callback);
void bcb_zd_remove_callback(bcb_zd_type_t type, struct bcb_zd_callback *callback);
void bcb_zd_set_enable(bcb_zd_type_t type, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* _BCB_ZD_H_ */