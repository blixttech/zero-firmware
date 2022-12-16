#ifndef _BCB_TC_DEF_MSM_H_
#define _BCB_TC_DEF_MSM_H_

#include <lib/bcb_tc.h>
#include <kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The enumeration of events for the main state machine.
 */
typedef enum {
	BCB_TC_DEF_EV_NONE = 0,
	BCB_TC_DEF_EV_CMD_OPEN,
	BCB_TC_DEF_EV_CMD_CLOSE,
	BCB_TC_DEF_EV_ZD_V,
	BCB_TC_DEF_EV_ZD_I,
	BCB_TC_DEF_EV_OCD,
	BCB_TC_DEF_EV_SW_OPENED,
	BCB_TC_DEF_EV_SW_CLOSED,
	BCB_TC_DEF_EV_SUPPLY_TIMER,
	BCB_TC_DEF_EV_REC_TIMER
} bcb_tc_def_event_t;

/**
 * The enumeration of states for the main state machine.
 */
typedef enum {
	BCB_TC_DEF_MSM_STATE_UNDEFINED = 0,
	BCB_TC_DEF_MSM_STATE_OPENED,
	BCB_TC_DEF_MSM_STATE_SUPPLY_WAIT,
	BCB_TC_DEF_MSM_STATE_CLOSE_WAIT,
	BCB_TC_DEF_MSM_STATE_CLOSED,
	BCB_TC_DEF_MSM_STATE_OPEN_WAIT
} bcb_tc_def_msm_state_t;

/**
 * The enumeration of causes for the main state machine to change its status.
 */
typedef enum {
	BCB_TC_DEF_MSM_CAUSE_NONE = 0,
	BCB_TC_DEF_MSM_CAUSE_EXT,
	BCB_TC_DEF_MSM_CAUSE_OCP,
	BCB_TC_DEF_MSM_CAUSE_OCD,
	BCB_TC_DEF_MSM_CAUSE_OTHER,
} bcb_tc_def_msm_cause_t;

/**
 * The enumeration of closed-state operation mode for the main state machine.
 */
typedef enum {
	BCB_TC_DEF_MSM_CSOM_NONE = 0, /**< No operation mode. */
	BCB_TC_DEF_MSM_CSOM_MOD, /**< Modulation control operation mode. */
	BCB_TC_DEF_MSM_CSOM_END
} bcb_tc_def_msm_csom_t;

/**
 * A structure representing the configuration of the main state machine.
 */
typedef struct __attribute__((packed)) bcb_msm_config {
	bcb_tc_def_msm_csom_t csom; /**< Closed-state operation mode. */
	bool rec_enabled; /**< Set to true if recovery is enabled. */
	uint16_t rec_attempts; /**< Number of recovery attempts. */
} bcb_tc_def_msm_config_t;

/**
 * Initialises the main state machine.
 * @param[in] notify_work A pointer to the Workqueue item to be notified when the state changes.
 */
int bcb_tc_def_msm_init(struct k_work *notify_work);

/**
 * Feeds an event to the main state machine.
 * @param[in] event The event.
 * @param[in] arg Argument for the event. 
 */
int bcb_tc_def_msm_event(bcb_tc_def_event_t event, void *arg);

/**
 * Set the configuration.
 * @param[in] config A pointer to the configuration structure. 
 */
int bcb_tc_def_msm_config_set(bcb_tc_def_msm_config_t *config);

/**
 * Get the configuration.
 * @param[out] config A pointer to the configuration structure. 
 */
int bcb_tc_def_msm_config_get(bcb_tc_def_msm_config_t *config);

/**
 * Get the current state of the main state machine.
 * @return The current state. 
 */
bcb_tc_def_msm_state_t bcb_tc_def_msm_get_state(void);

/**
 * Get the cause for the main state machine to change its state.
 * @return The cause. 
 */
bcb_tc_def_msm_cause_t bcb_tc_def_msm_get_cause(void);

#ifdef __cplusplus
}
#endif

#endif /* _BCB_TC_DEF_MSM_H_ */