/**
 * @file
 * @brief ADC DMA public API header file.
 */

/*
 * Copyright (c) 2023 Blixt Tech AB
 *
 */

#ifndef _ZEPHYR_INCLUDE_DRIVERS_ADC_DMA_H_
#define _ZEPHYR_INCLUDE_DRIVERS_ADC_DMA_H_

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

enum adc_dma_reference {
	ADC_DMA_REF_VDD_1, /**< VDD. */
	ADC_DMA_REF_VDD_1_2, /**< VDD/2. */
	ADC_DMA_REF_VDD_1_3, /**< VDD/3. */
	ADC_DMA_REF_VDD_1_4, /**< VDD/4. */
	ADC_DMA_REF_INTERNAL, /**< Internal. */
	ADC_DMA_REF_EXTERNAL0, /**< External, input 0. */
	ADC_DMA_REF_EXTERNAL1, /**< External, input 1. */
};

/**
 * @brief Fuction to be called when the samples were written to the buffer.
 * @param dev A pointer to the ADC device
 * @param buffer A pointer to start of the buffer where the samples were written.
 * @param samples Number of samples written to the buffer.
 */
typedef void (*adc_dma_seq_callback)(const struct device *dev, void *buffer, uint32_t samples);

struct adc_dma_sampling_config {
	/** Pointer to the buffer where the samples to be written. */
	void *buffer;
	/** Size of the buffer. */
	size_t buffer_size;
	/** Total number of samples to be taken. */
	uint32_t samples;
	/** Number of channels from the sequence to be used. */
	uint8_t channels;
	/** ADC reference to be used. */
	enum adc_dma_reference reference;
	/** ADC resolution. */
	uint8_t resolution;
	/** ADC sampling completed callback. Set to NULL if not used. */
	adc_dma_seq_callback callback;
};

struct adc_dma_channel_config {
	uint8_t channel; /* Main ADC channel */
	uint8_t alt_channel; /* Alternate ADC channel */
};

typedef int (*adc_dma_api_start)(const struct device *dev,
				 const struct adc_dma_sampling_config *sampling_config);

typedef void (*adc_dma_api_stop)(const struct device *dev);

typedef int (*adc_dma_api_add_channel)(const struct device *dev,
				       const struct adc_dma_channel_config *channel_config,
				       uint8_t sequence_index);

typedef uint32_t (*adc_dma_api_get_sampling_time)(const struct device *dev);

typedef const struct device *(*adc_dma_api_get_trigger_device)(const struct device *dev);

/**
 * @brief ADC DMA driver API
 *
 * This is the mandatory API any ADC DMA driver needs to expose.
 */
__subsystem struct adc_dma_driver_api {
	adc_dma_api_start start;
	adc_dma_api_stop stop;
	adc_dma_api_add_channel add_channel;
	adc_dma_api_get_sampling_time get_sampling_time;
	adc_dma_api_get_trigger_device get_trigger_device;
};

/**
 * @brief Start ADC sampling
 * @param[in]	Pointer to the device structure for the driver instance.
 * @param[in]	Pointer to the sampling configuration structure.
 * @retval 0	On success. Otherwise, a failure.
 */
__syscall int adc_dma_start(const struct device *dev,
			    const struct adc_dma_sampling_config *sampling_config);
static inline int z_impl_adc_dma_start(const struct device *dev,
				       const struct adc_dma_sampling_config *sampling_config)
{
	const struct adc_dma_driver_api *api = (const struct adc_dma_driver_api *)dev->api;

	return api->start(dev, sampling_config);
}

/**
 * @brief Stop ADC sampling
 * @param[in]	Pointer to the device structure for the driver instance.
 */
__syscall void adc_dma_stop(const struct device *dev);
static inline void z_impl_adc_dma_stop(const struct device *dev)
{
	const struct adc_dma_driver_api *api = (const struct adc_dma_driver_api *)dev->api;

	api->stop(dev);
}

/**
 * @brief Add channel to the sampling sequence.
 * @param[in]	Pointer to the device structure for the driver instance.
 * @param[in]	Pointer to the channel configuration structure.
 * @param[in]	Channel index of the sampling sequence. The index starts from zero.
 */
__syscall int adc_dma_add_channel(const struct device *dev,
				  const struct adc_dma_channel_config *channel_config,
				  uint8_t sequence_index);
static inline int z_impl_adc_dma_add_channel(const struct device *dev,
					     const struct adc_dma_channel_config *channel_config,
					     uint8_t sequence_index)
{
	const struct adc_dma_driver_api *api = (const struct adc_dma_driver_api *)dev->api;

	return api->add_channel(dev, channel_config, sequence_index);
}

/**
 * @brief Get ADC sampling time of a channel.
 * @param[in]	Pointer to the device structure for the driver instance.
 * return	ADC sampling time in microseconds.
 */
__syscall uint32_t adc_dma_get_sampling_time(const struct device *dev);
static inline uint32_t z_impl_adc_dma_get_sampling_time(const struct device *dev)
{
	const struct adc_dma_driver_api *api = (const struct adc_dma_driver_api *)dev->api;

	return api->get_sampling_time(dev);
}

/**
 * @brief Get trigger device.
 * @param[in]	Pointer to the device structure for the driver instance.
 * return	Pointer to the device structure of the trigger device.
 */
__syscall const struct device *adc_dma_get_trigger_device(const struct device *dev);
static inline const struct device *z_impl_adc_dma_get_trigger_device(const struct device *dev)
{
	const struct adc_dma_driver_api *api = (const struct adc_dma_driver_api *)dev->api;

	return api->get_trigger_device(dev);
}

#ifdef __cplusplus
}
#endif

#include <syscalls/adc_dma.h>

#endif // _ZEPHYR_INCLUDE_DRIVERS_ADC_DMA_H_