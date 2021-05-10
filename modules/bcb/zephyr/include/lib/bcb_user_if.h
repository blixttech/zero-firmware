#ifndef _BCB_USER_IF_H_
#define _BCB_USER_IF_H_

#include <sys/slist.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bcb_user_leds {
	BCB_USER_LEDS_RED = 0x1,
	BCB_USER_LEDS_GREEN = 0x2,
	BCB_USER_LEDS_ALL = 0x3,
} bcb_user_leds_t;

typedef enum {
	BCB_USER_IF_BTN_ACTION_PUSHED = 0,
	BCB_USER_IF_BTN_ACTION_RELEASED
} bcb_user_btn_action_t;

typedef void (*bcb_user_btn_callback_handler_t)(bool is_pressed);

struct bcb_user_button_callback {
	sys_snode_t node;
	bcb_user_btn_callback_handler_t handler;
};

int bcb_user_if_init();
int bcb_user_button_add_callback(struct bcb_user_button_callback *callback);
void bcb_user_button_remove_callback(struct bcb_user_button_callback *callback);
void bcb_user_leds_on(bcb_user_leds_t leds);
void bcb_user_leds_off(bcb_user_leds_t leds);
void bcb_user_leds_toggle(bcb_user_leds_t leds);

#ifdef __cplusplus
}
#endif

#endif /* _BCB_USER_IF_H_ */