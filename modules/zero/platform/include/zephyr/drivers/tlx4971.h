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

#include <stdint.h>
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

enum tlx4971_range {
	TLX4971_RANGE_120 = 0,
	TLX4971_RANGE_100,
	TLX4971_RANGE_75,
	TLX4971_RANGE_50,
	TLX4971_RANGE_37_5,
	TLX4971_RANGE_25
};

enum tlx4971_opmode {
	TLX4971_OPMODE_SD_BID = 0,
	TLX4971_OPMODE_FD,
	TLX4971_OPMODE_SD_UNI,
	TLX4971_OPMODE_SE
};

enum tlx4971_deglitch {
	TLX4971_DEGLITCH_0 = 0,
	TLX4971_DEGLITCH_500,
	TLX4971_DEGLITCH_1000,
	TLX4971_DEGLITCH_1500,
	TLX4971_DEGLITCH_2000,
	TLX4971_DEGLITCH_2500,
	TLX4971_DEGLITCH_3000,
	TLX4971_DEGLITCH_3500,
	TLX4971_DEGLITCH_4000,
	TLX4971_DEGLITCH_4500,
	TLX4971_DEGLITCH_5000,
	TLX4971_DEGLITCH_5500,
	TLX4971_DEGLITCH_6000,
	TLX4971_DEGLITCH_6500,
	TLX4971_DEGLITCH_7000,
	TLX4971_DEGLITCH_7500
};

enum tlx4971_vref_ext {
	TLX4971_VREF_EXT_1V65 = 0,
	TLX4971_VREF_EXT_1V2,
	TLX4971_VREF_EXT_1V5,
	TLX4971_VREF_EXT_1V8
};


struct tlx4971_config {
	enum tlx4971_range range;	     /**< Measurement range. */
	enum tlx4971_opmode opmode;	     /**< Output mode. */
	bool ocd1_en;			     /**< Enable OCD1 output. */
	bool ocd2_en;			     /**< Enable OCD2 output. */
	uint16_t ocd1_level;		     /**< OCD1 trigger level in A. */
	uint16_t ocd2_level;		     /**< OCD2 trigger level in A. */
	enum tlx4971_deglitch ocd1_deglitch; /**< Deglitch time for OCD1. */
	enum tlx4971_deglitch ocd2_deglitch; /**< Deglitch time for OCD2. */
	enum tlx4971_vref_ext vref_ext;      /**< External reference voltage. */
	bool is_temp;			     /**< Write to temporary registers. */
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