#ifndef _BCB_TC_DEF_CSOM_MOD_H_
#define _BCB_TC_DEF_CSOM_MOD_H_

#include <lib/bcb_tc_def_msm.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bcb_tc_def_csom_mod_config {
	uint8_t zdc_closed;
	uint8_t zdc_period;
} bcb_tc_def_csom_mod_config_t;

/**
 * Initialises the modulation control state machine.
 */
int bcb_tc_def_csom_mod_init(void);

/**
 * Set the configuration of the modulation control state machine.
 * @param[in] config A pointer to the configuration structure.
 */
int bcb_tc_def_csom_mod_config_set(bcb_tc_def_csom_mod_config_t *config);

/**
 * Get the configuration of the modulation control state machine.
 * @param[out] config A pointer to the configuration structure.
 */
int bcb_tc_def_csom_mod_config_get(bcb_tc_def_csom_mod_config_t *config);

/**
 * Feeds an event to the modulation control state machine.
 * @param[in] event The event.
 * @param[in] arg Argument for the event. 
 */
int bcb_tc_def_csom_mod_event(bcb_tc_def_event_t event, void *arg);

/**
 * Clean up the modulation control state machine.
 */
void bcb_tc_def_csom_mod_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* _BCB_TC_DEF_CSOM_MOD_H_ */