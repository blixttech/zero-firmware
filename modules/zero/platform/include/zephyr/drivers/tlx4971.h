/**
 * @file
 * @brief TLx4971 driver public API header file.
 */

/*
 * Copyright (c) 2023 Blixt Tech AB
 *
 */

#ifndef _ZEPHYR_INCLUDE_DRIVERS_TLX4971_H_
#define _ZEPHYR_INCLUDE_DRIVERS_TLX4971_H_

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

enum tlx4971_range {
	TLX4971_RANGE_120 = 0,
	TLX4971_RANGE_100,
	TLX4971_RANGE_75,
	TLX4971_RANGE_50,
	TLX4971_RANGE_25
};

enum tlx4971_opmode {
	TLX4971_OPMODE_SD_BID = 0,
	TLX4971_OPMODE_FD,
	TLX4971_OPMODE_SD_UNI,
	TLX4971_OPMODE_SE
};

struct tlx4971_config {
	enum tlx4971_range range;
	enum tlx4971_opmode opmode;
};

typedef int (*tlx4971_get_config_t)(const struct device *dev, struct tlx4971_config *config);
typedef int (*tlx4971_set_config_t)(const struct device *dev, const struct tlx4971_config *config);

__subsystem struct tlx4971_driver_api {
	tlx4971_get_config_t get_config;
	tlx4971_set_config_t set_config;
};

__syscall int tlx4971_get_config(const struct device *dev, struct tlx4971_config *config);
static inline int z_impl_tlx4971_get_config(const struct device *dev, struct tlx4971_config *config)
{
	const struct tlx4971_driver_api *api = (const struct tlx4971_driver_api *)dev->api;

	return api->get_config(dev, config);
}

__syscall int tlx4971_set_config(const struct device *dev, const struct tlx4971_config *config);
static inline int z_impl_tlx4971_set_config(const struct device *dev,
					    const struct tlx4971_config *config)
{
	const struct tlx4971_driver_api *api = (const struct tlx4971_driver_api *)dev->api;

	return api->set_config(dev, config);
}

#ifdef __cplusplus
}
#endif

#include <syscalls/tlx4971.h>

#endif // _ZEPHYR_INCLUDE_DRIVERS_TLX4971_H_