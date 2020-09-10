/**
 * @file
 * @brief Header file for ADC driver implementation using EDMA.
 */

#ifndef _ZEPHYR_INCLUDE_DRIVERS_ADC_MCUX_EDMA_H_
#define _ZEPHYR_INCLUDE_DRIVERS_ADC_MCUX_EDMA_H_

#include <stdint.h>
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_MCUX_CHNLS_A            (2LLU)
#define ADC_MCUX_CHNL_BIT(c, a)     ((1LLU) << (((uint64_t)(c)*ADC_MCUX_CHNLS_A)+(uint64_t)(a)))

#define ADC_MCUX_GET_CHNL(idx)      ((uint64_t)(idx) / ADC_MCUX_CHNLS_A)
#define ADC_MCUX_GET_CHNL_A(idx)    ((uint64_t)(idx) % ADC_MCUX_CHNLS_A)

/**
 * @brief Driver APIs for DMA based minimalistic ADC driver for NXP K64F
 * @defgroup adc_interface ADC driver APIs
 * @ingroup io_interfaces
 * @{
 */

/** @brief ADC references. */
enum adc_mcux_ref {
    ADC_MCUX_REF_EXTERNAL,  /**< External, VREFH & VREFL. */
    ADC_MCUX_REF_INTERNAL,  /**< Internal 1.2V. */
};

/**
 * @brief Structure defining an ADC sampling sequence configuration.
 */
struct adc_mcux_sequence_config {

    /**
     * Interval between consecutive samplings (in microseconds). Should not be zero.
     */
    uint32_t interval_us;

    /**
     * Pointer to a buffer where the samples are to be written.
     * Though the samples are written in 16-bit words.
     */
    volatile void* buffer;

    /**
     * Specifies the size of the buffer pointed by the "buffer" in bytes.
     */
    size_t buffer_size;

    /** Reference selection. */
    enum adc_mcux_ref reference;
};

/**
 * @brief Structure defining ADC channel configuration.
 * 
 */
struct adc_mcux_channel_config {
    /**
     * ADC channel.
     */
    uint8_t channel;
    /**
     * Alternate ADC channel. Each ADC channel may be multiplexed among several
     * alternate channels.
     */
    uint8_t alt_channel;
};

typedef int (*adc_mcux_api_read)(struct device* dev,
				                const struct adc_mcux_sequence_config* seq_cfg,
				                bool single);

typedef int (*adc_mcux_api_channel_setup)(struct device* dev,
                                            uint8_t seq_idx,
				                            const struct adc_mcux_channel_config* ch_cfg);

typedef int (*adc_mcux_api_set_sequence_len)(struct device* dev, uint8_t seq_len);

typedef uint8_t (*adc_mcux_api_get_sequence_len)(struct device* dev);

/**
 * @brief ADC driver API
 *
 * This is the mandatory API any ADC driver needs to expose.
 */
__subsystem struct adc_mcux_driver_api {
    adc_mcux_api_read               read;
    adc_mcux_api_channel_setup      channel_setup;
    adc_mcux_api_set_sequence_len   set_sequence_len;
    adc_mcux_api_get_sequence_len   get_sequence_len;
};

__syscall int adc_mcux_read(struct device* dev, 
                            const struct adc_mcux_sequence_config* seq_cfg, bool async);

static inline int z_impl_adc_mcux_read(struct device* dev, 
                                        const struct adc_mcux_sequence_config* seq_cfg, bool async)
{
    struct adc_mcux_driver_api *api;
    api = (struct adc_mcux_driver_api *)dev->driver_api;
    return api->read(dev, seq_cfg, async);
}

__syscall int adc_mcux_channel_setup(struct device* dev, uint8_t seq_idx, 
                                        const struct adc_mcux_channel_config* ch_cfg);

static inline int z_impl_adc_mcux_channel_setup(struct device* dev, uint8_t seq_idx, 
                                                const struct adc_mcux_channel_config* ch_cfg)
{
    struct adc_mcux_driver_api *api;
    api = (struct adc_mcux_driver_api *)dev->driver_api;
    return api->channel_setup(dev, seq_idx, ch_cfg);
}

__syscall int adc_mcux_set_sequence_len(struct device* dev, uint8_t seq_len);

static inline int z_impl_adc_mcux_set_sequence_len(struct device* dev, uint8_t seq_len)
{
    struct adc_mcux_driver_api *api;
    api = (struct adc_mcux_driver_api *)dev->driver_api;
    return api->set_sequence_len(dev, seq_len);
}

__syscall uint8_t adc_mcux_get_sequence_len(struct device* dev);

static inline uint8_t z_impl_adc_mcux_get_sequence_len(struct device* dev)
{
    struct adc_mcux_driver_api *api;
    api = (struct adc_mcux_driver_api *)dev->driver_api;
    return api->get_sequence_len(dev);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#include <syscalls/adc_mcux_edma.h>

#endif // _ZEPHYR_INCLUDE_DRIVERS_ADC_MCUX_EDMA_H_