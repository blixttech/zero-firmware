/**
 * @file
 * @brief SICI driver public API header file.
 */

/*
 * Copyright (c) 2023 Blixt Tech AB
 *
 */

#ifndef _ZEPHYR_INCLUDE_DRIVERS_SICI_H_
#define _ZEPHYR_INCLUDE_DRIVERS_SICI_H_

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*sici_power_t)(const struct device *dev, bool on);
typedef int (*sici_enable_t)(const struct device *dev, bool enable);
typedef int (*sici_transfer_t)(const struct device *dev, uint16_t in, uint16_t *out);

__subsystem struct sici_driver_api {
	sici_power_t power;
	sici_enable_t enable;
	sici_transfer_t transfer;
};

/**
 * @brief Power on/off the connected device.
 *
 * This function sets the power to the device via an external IO pin.
 *
 * @param[in] dev Pointer to the device structure for the driver instance.
 * @param on Set to true to turn on the device. Otherwise, false.
 *
 * @retval 0      If successful.
 * @retval -errno Negative error code on error.
 */
__syscall int sici_power(const struct device *dev, bool on);

static inline int z_impl_sici_power(const struct device *dev, bool on)
{
	const struct sici_driver_api *api = (const struct sici_driver_api *)dev->api;

	return api->power(dev, on);
}

/**
 * @brief Enable/disable the SICI interface of the connected device.
 *
 * When enabling, followings happen in order.
 * 1. Apply specified multiplexer settings on the SICI communication pins of the MCU.
 * 2. Send the enter-interface command.
 *
 * When disabling, followings happen in order.
 * 1. Leave SICI mode by pulling communication line for 6 ms.
 * 2. Disable SICI communication pins on the MCU (high impedance mode).
 *
 * Note that the connected device accepts the enter-interface command only after a power on
 * (about 100 us after the power on).
 * Therefore, call @ref sici_power function to toggle the power to the device prior to calling
 * this function.
 *
 * @param[in] dev Pointer to the device structure for the driver instance.
 * @param enable Set to true to enable. False to disable.
 *
 * @retval 0      If successful.
 * @retval -errno Negative error code on error.
 */
__syscall int sici_enable(const struct device *dev, bool enable);

__syscall int z_impl_sici_enable(const struct device *dev, bool enable)
{
	const struct sici_driver_api *api = (const struct sici_driver_api *)dev->api;

	return api->enable(dev, enable);
}

/**
 * @brief Transfer data between the MCU and the connected device.
 *
 * Data transfers between the MCU and the connected device happen in 16-bit words.
 * The connected device responds with a 16-bit word for each 16-bit word sent from the MCU.
 *
 * @param[in] dev Pointer to the device structure for the driver instance.
 * @param in 16-bit data to be sent.
 * @param[out] out Received 16-bit data.
 *
 * @retval 0      If successful.
 * @retval -errno Negative error code on error.
 */
__syscall int sici_transfer(const struct device *dev, uint16_t in, uint16_t *out);

__syscall int z_impl_sici_transfer(const struct device *dev, uint16_t in, uint16_t *out)
{
	const struct sici_driver_api *api = (const struct sici_driver_api *)dev->api;

	return api->transfer(dev, in, out);
}

#ifdef __cplusplus
}
#endif

#include <syscalls/sici.h>

#endif // _ZEPHYR_INCLUDE_DRIVERS_SICI_H_