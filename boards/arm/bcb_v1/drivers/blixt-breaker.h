#ifndef _BLIXT_BREAKER_DRIVER_H_
#define _BLIXT_BREAKER_DRIVER_H_

#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct breaker_iv_data {
    uint32_t i_ma;
    uint32_t v_mv;
    uint32_t t;
} breaker_iv_data;

typedef int    (*breaker_api_on)       (struct device* dev);
typedef int    (*breaker_api_off)      (struct device* dev);
typedef int    (*breaker_api_is_on)    (struct device* dev);
typedef int    (*breaker_api_reset)    (struct device* dev);

__subsystem struct breaker_driver_api {
    breaker_api_on          on;
    breaker_api_off         off;
    breaker_api_is_on       is_on;
    breaker_api_reset       reset;
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

__syscall int breaker_is_on(struct device* dev);

static inline int z_impl_breaker_is_on(struct device* dev)
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

#ifdef __cplusplus
}
#endif

#include <syscalls/blixt-breaker.h>

#endif // _BLIXT_BREAKER_DRIVER_H_