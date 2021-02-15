#ifndef _ZEPHYR_INCLUDE_DRIVERS_ADC_DMA_H_
#define _ZEPHYR_INCLUDE_DRIVERS_ADC_DMA_H_

#include <stdint.h>
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum adc_dma_ref {
	ADC_DMA_REF_VDD_1, /**< VDD. */
	ADC_DMA_REF_VDD_1_2, /**< VDD/2. */
	ADC_DMA_REF_VDD_1_3, /**< VDD/3. */
	ADC_DMA_REF_VDD_1_4, /**< VDD/4. */
	ADC_DMA_REF_INTERNAL, /**< Internal. */
	ADC_DMA_REF_EXTERNAL0, /**< External, input 0. */
	ADC_DMA_REF_EXTERNAL1, /**< External, input 1. */
} adc_dma_ref_t;

typedef enum adc_dma_perf_level {
	ADC_DMA_PERF_LEVEL_0, /**< Level 0- lowest performace level, best DC accuracy. */
	ADC_DMA_PERF_LEVEL_1, /**< Level 1. */
	ADC_DMA_PERF_LEVEL_2, /**< Level 2. */
	ADC_DMA_PERF_LEVEL_3, /**< Level 3  */
	ADC_DMA_PERF_LEVEL_4, /**< Level 3  */
	ADC_DMA_PERF_LEVEL_5, /**< Level 5 - highest performance level, lowest DC accuracy. */
} adc_dma_performance_level_t;

typedef void (*adc_dma_sequence_callback_t)(struct device *dev);

typedef struct adc_dma_sequence_config {
	volatile void *buffer; /* Pointer to the buffer where the samples are written */
	size_t buffer_size; /* Size of the buffer */
	uint32_t samples; /* Total number of samples to be taken */
	uint8_t len; /* ADC channels in the sequence. */
	adc_dma_sequence_callback_t callback; /* Set to NULL if not used */
} adc_dma_sequence_config_t;

typedef struct adc_dma_channel_config {
	uint8_t channel;
	uint8_t alt_channel;
} adc_dma_channel_config_t;

typedef int (*adc_dma_api_read)(struct device *dev, const adc_dma_sequence_config_t *seq_cfg);

typedef int (*adc_dma_api_stop)(struct device *dev);

typedef int (*adc_dma_api_channel_setup)(struct device *dev, uint8_t seq_idx,
					 const adc_dma_channel_config_t *ch_cfg);

typedef int (*adc_dma_api_set_reference)(struct device *dev, adc_dma_ref_t reference);

typedef int (*adc_dma_api_set_resolution)(struct device *dev, uint8_t resolution);

typedef int (*adc_dma_api_set_perf_level)(struct device *dev, adc_dma_performance_level_t level);

typedef int (*adc_dma_api_calibrate)(struct device *dev);

typedef int (*adc_dma_api_set_calibration_values)(struct device *dev, void *params, size_t size);

typedef int (*adc_dma_api_get_calibration_values)(struct device *dev, void *params, size_t size);

typedef size_t (*adc_dma_api_get_calibration_values_length)(struct device *dev);

typedef uint32_t (*adc_dma_api_get_sampling_time)(struct device *dev);

__subsystem struct adc_dma_driver_api {
	adc_dma_api_read read;
	adc_dma_api_stop stop;
	adc_dma_api_channel_setup channel_setup;
	adc_dma_api_set_reference set_reference;
	adc_dma_api_set_resolution set_resolution;
	adc_dma_api_set_perf_level set_performance_level;
	adc_dma_api_calibrate calibrate;
	adc_dma_api_set_calibration_values set_calibration_values;
	adc_dma_api_get_calibration_values get_calibration_values;
	adc_dma_api_get_calibration_values_length get_calibration_values_length;
	adc_dma_api_get_sampling_time get_sampling_time;
};

__syscall int adc_dma_read(struct device *dev, const adc_dma_sequence_config_t *seq_cfg);
static inline int z_impl_adc_dma_read(struct device *dev, const adc_dma_sequence_config_t *seq_cfg)
{
	struct adc_dma_driver_api *api;
	api = (struct adc_dma_driver_api *)dev->driver_api;
	return api->read(dev, seq_cfg);
}

__syscall int adc_dma_stop(struct device *dev);
static inline int z_impl_adc_dma_stop(struct device *dev)
{
	struct adc_dma_driver_api *api;
	api = (struct adc_dma_driver_api *)dev->driver_api;
	return api->stop(dev);
}

__syscall int adc_dma_channel_setup(struct device *dev, uint8_t seq_idx,
				    const adc_dma_channel_config_t *ch_cfg);
static inline int z_impl_adc_dma_channel_setup(struct device *dev, uint8_t seq_idx,
					       const adc_dma_channel_config_t *ch_cfg)
{
	struct adc_dma_driver_api *api;
	api = (struct adc_dma_driver_api *)dev->driver_api;
	return api->channel_setup(dev, seq_idx, ch_cfg);
}

__syscall int adc_dma_set_reference(struct device *dev, adc_dma_ref_t ref);
static inline int z_impl_adc_dma_set_reference(struct device *dev, adc_dma_ref_t reference)
{
	struct adc_dma_driver_api *api;
	api = (struct adc_dma_driver_api *)dev->driver_api;
	return api->set_reference(dev, reference);
}

__syscall int adc_dma_set_resolution(struct device *dev, uint8_t resolution);
static inline int z_impl_adc_dma_set_resolution(struct device *dev, uint8_t resolution)
{
	struct adc_dma_driver_api *api;
	api = (struct adc_dma_driver_api *)dev->driver_api;
	return api->set_resolution(dev, resolution);
}

__syscall int adc_dma_set_performance_level(struct device *dev, adc_dma_performance_level_t level);
static inline int z_impl_adc_dma_set_performance_level(struct device *dev,
						       adc_dma_performance_level_t level)
{
	struct adc_dma_driver_api *api;
	api = (struct adc_dma_driver_api *)dev->driver_api;
	return api->set_performance_level(dev, level);
}

__syscall int adc_dma_calibrate(struct device *dev);
static inline int z_impl_adc_dma_calibrate(struct device *dev)
{
	struct adc_dma_driver_api *api;
	api = (struct adc_dma_driver_api *)dev->driver_api;
	return api->calibrate(dev);
}

__syscall int adc_dma_set_calibration_values(struct device *dev, void *params, size_t size);
static inline int z_impl_adc_dma_set_calibration_values(struct device *dev, void *params,
							size_t size)
{
	struct adc_dma_driver_api *api;
	api = (struct adc_dma_driver_api *)dev->driver_api;
	return api->set_calibration_values(dev, params, size);
}

__syscall int adc_dma_get_calibration_values(struct device *dev, void *params, size_t size);
static inline int z_impl_adc_dma_get_calibration_values(struct device *dev, void *params,
							size_t size)
{
	struct adc_dma_driver_api *api;
	api = (struct adc_dma_driver_api *)dev->driver_api;
	return api->get_calibration_values(dev, params, size);
}

__syscall size_t adc_dma_get_calibration_values_length(struct device *dev);
static inline size_t z_impl_adc_dma_get_calibration_values_length(struct device *dev)
{
	struct adc_dma_driver_api *api;
	api = (struct adc_dma_driver_api *)dev->driver_api;
	return api->get_calibration_values_length(dev);
}

__syscall size_t adc_dma_get_sampling_time(struct device *dev);
static inline size_t z_impl_adc_dma_sampling_time(struct device *dev)
{
	struct adc_dma_driver_api *api;
	api = (struct adc_dma_driver_api *)dev->driver_api;
	return api->get_sampling_time(dev);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#include <syscalls/adc_dma.h>

#endif // _ZEPHYR_INCLUDE_DRIVERS_ADC_DMA_H_