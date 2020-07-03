#ifndef _BLIXT_BREAKER_H_
#define _BLIXT_BREAKER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	BREAKER_OCP_TRIGGER_DIR_P	0
#define	BREAKER_OCP_TRIGGER_DIR_N	1

typedef struct breaker_iv_data {
    uint32_t i_ma;
    uint32_t v_mv;
    uint32_t t;
} breaker_iv_data_t;

typedef void (*breaker_ocp_callback_t)(int32_t duration);

int breaker_on();

int breaker_off();

bool breaker_is_on();

int breaker_reset();

int breaker_set_ocp_callback(breaker_ocp_callback_t callback);

int breaker_ocp_trigger(int direction);

int breaker_get_iv(breaker_iv_data_t* iv_data);

uint32_t breaker_get_timestamp();

#ifdef __cplusplus
}
#endif

#endif // _BLIXT_BREAKER_H_