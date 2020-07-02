#ifndef _BLIXT_BREAKER_DRIVER_H_
#define _BLIXT_BREAKER_DRIVER_H_

#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	BREAKER_OCP_TRIGGER_DIR_P	0
#define	BREAKER_OCP_TRIGGER_DIR_N	1

typedef struct breaker_iv_data {
    uint32_t i_ma;
    uint32_t v_mv;
    uint32_t t;
} breaker_iv_data;

typedef void (*breaker_ocp_callback_handler_t)(struct device *port, int32_t duration);

typedef int    (*breaker_api_on)       (struct device* dev);
typedef int    (*breaker_api_off)      (struct device* dev);
typedef bool   (*breaker_api_is_on)    (struct device* dev);
typedef int    (*breaker_api_reset)    (struct device* dev);
typedef int    (*breaker_api_set_ocp_callback)		(struct device* dev, breaker_ocp_callback_handler_t callback);
typedef int    (*breaker_api_ocp_trigger)    		(struct device* dev, int direction);

__subsystem struct breaker_driver_api {
    breaker_api_on          		on;
    breaker_api_off         		off;
    breaker_api_is_on       		is_on;
    breaker_api_reset       		reset;
	breaker_api_set_ocp_callback	set_ocp_callback;
	breaker_api_ocp_trigger			ocp_trigger;
};

__syscall int breaker_on(struct device* dev);

static inline int z_impl_breaker_on(struct device* dev)
{
	const struct breaker_driver_api *api = (const struct breaker_driver_api *)dev->driver_api;
	return api->on(dev);
}

__syscall int breaker_off(struct device* dev);

static inline int z_impl_breaker_off(struct device* dev)
{
	const struct breaker_driver_api *api = (const struct breaker_driver_api *)dev->driver_api;
	return api->off(dev);
}

__syscall bool breaker_is_on(struct device* dev);

static inline bool z_impl_breaker_is_on(struct device* dev)
{
	const struct breaker_driver_api *api = (const struct breaker_driver_api *)dev->driver_api;
	return api->is_on(dev);
}

__syscall int breaker_reset(struct device* dev);

static inline int z_impl_breaker_reset(struct device* dev)
{
	const struct breaker_driver_api *api = (const struct breaker_driver_api *)dev->driver_api;
	return api->reset(dev);
}

__syscall int breaker_set_ocp_callback(struct device* dev, breaker_ocp_callback_handler_t callback);

static inline int z_impl_breaker_set_ocp_callback(struct device* dev, breaker_ocp_callback_handler_t callback)
{
	const struct breaker_driver_api *api = (const struct breaker_driver_api *)dev->driver_api;
	return api->set_ocp_callback(dev, callback);
}

__syscall int breaker_ocp_trigger(struct device* dev, int direction);

static inline int z_impl_breaker_ocp_trigger(struct device* dev, int direction)
{
	const struct breaker_driver_api *api = (const struct breaker_driver_api *)dev->driver_api;
	return api->ocp_trigger(dev, direction);
}

#ifdef __cplusplus
}
#endif

#include <syscalls/blixt-breaker.h>

#endif // _BLIXT_BREAKER_DRIVER_H_