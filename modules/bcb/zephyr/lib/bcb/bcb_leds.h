#ifndef _BCB_LEDS_H_
#define _BCB_LEDS_H_

#ifdef __cplusplus
extern "C" {
#endif

enum bcb_leds {
    BCB_LEDS_RED = 0x1,
    BCB_LEDS_GREEN = 0x2,
    BCB_LEDS_ALL = 0x3,
};

int bcb_leds_init(void);
void bcb_leds_on(int leds);
void bcb_leds_off(int leds);
void bcb_leds_toggle(int leds);

#ifdef __cplusplus
}
#endif

#endif /* _BCB_LEDS_H_ */