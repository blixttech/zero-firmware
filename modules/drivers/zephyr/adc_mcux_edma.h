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

/**
 * @brief Driver APIs for DMA based minimalistic ADC driver for NXP K64F
 * @defgroup adc_interface ADC driver APIs
 * @ingroup io_interfaces
 * @{
 */

/** @brief ADC references. */
typedef enum adc_mcux_ref {
    ADC_MCUX_REF_EXTERNAL,  /**< External, VREFH & VREFL. */
    ADC_MCUX_REF_INTERNAL,  /**< Internal 1.2V. */
} adc_mcux_ref_t;

/** @brief ADC performance level  */
enum adc_mcux_perf_lvl {
    ADC_MCUX_PERF_LVL_0,    /**< Level 0- lowest performace level, best DC accuracy. */
    ADC_MCUX_PERF_LVL_1,    /**< Level 1. */
    ADC_MCUX_PERF_LVL_2,    /**< Level 2. */
    ADC_MCUX_PERF_LVL_3,    /**< Level 3 - highest performance level, lowest DC accuracy. */
};

/**
 * @brief Structure defining an ADC calibration parameters.
 */
typedef struct adc_mcux_cal_params {
    uint16_t ofs;   /**< ADC Offset Correction Register */
    uint16_t pg;    /**< ADC Plus-Side Gain Register */
    uint16_t mg;    /**< ADC Minus-Side Gain Register */
    uint16_t clpd;  /**< ADC Plus-Side General Calibration Value Register */
    uint16_t clps;  /**< ADC Plus-Side General Calibration Value Register */
    uint16_t clp4;  /**< ADC Plus-Side General Calibration Value Register */
    uint16_t clp3;  /**< ADC Plus-Side General Calibration Value Register */
    uint16_t clp2;  /**< ADC Plus-Side General Calibration Value Register */
    uint16_t clp1;  /**< ADC Plus-Side General Calibration Value Register */
    uint16_t clp0;  /**< ADC Plus-Side General Calibration Value Register */
    uint16_t clmd;  /**< ADC Minus-Side General Calibration Value Register */
    uint16_t clms;  /**< ADC Minus-Side General Calibration Value Register */
    uint16_t clm4;  /**< ADC Minus-Side General Calibration Value Register */
    uint16_t clm3;  /**< ADC Minus-Side General Calibration Value Register */
    uint16_t clm2;  /**< ADC Minus-Side General Calibration Value Register */
    uint16_t clm1;  /**< ADC Minus-Side General Calibration Value Register */
    uint16_t clm0;  /**< ADC Minus-Side General Calibration Value Register */
} adc_mcux_cal_params_t;

/**
 * @brief Structure defining an ADC sampling sequence configuration.
 */
typedef struct adc_mcux_sequence_config {
    /**
     * Interval between consecutive samplings (in microseconds). Should not be zero.
     */
    uint32_t interval_us;
    /**
     * Pointer to a buffer where the samples are to be written.
     * Samples are written in 16-bit words.
     */
    volatile void* buffer;
    /**
     * Specifies the size of the buffer pointed by the "buffer" in bytes.
     */
    size_t buffer_size;
    /**
     * ADC sequence length
     */
    uint8_t seq_len;
} adc_mcux_sequence_config_t;

/**
 * @brief Structure defining ADC channel configuration.
 * 
 */
typedef struct adc_mcux_channel_config {
    /**
     * ADC channel.
     */
    uint8_t channel;
    /**
     * Alternate ADC channel. Each ADC channel may be multiplexed among several
     * alternate channels.
     */
    uint8_t alt_channel;
} adc_mcux_channel_config_t;

typedef int (*adc_mcux_api_read)(struct device* dev, const adc_mcux_sequence_config_t* seq_cfg);

typedef int (*adc_mcux_api_stop)(struct device* dev);

typedef int (*adc_mcux_api_channel_setup)(struct device* dev,
                                            uint8_t seq_idx,
				                            const adc_mcux_channel_config_t* ch_cfg);

typedef int (*adc_mcux_api_set_reference)(struct device* dev, adc_mcux_ref_t ref);

typedef int (*adc_mcux_api_set_perf_level)(struct device* dev, uint8_t level);

typedef int (*adc_mcux_api_calibrate)(struct device* dev);

typedef int (*adc_mcux_api_set_cal_params)(struct device* dev, adc_mcux_cal_params_t* params);

typedef int (*adc_mcux_api_get_cal_params)(struct device* dev, adc_mcux_cal_params_t* params);

/**
 * @brief ADC driver API
 *
 * This is the mandatory API any ADC driver needs to expose.
 */
__subsystem struct adc_mcux_driver_api {
    adc_mcux_api_read               read;
    adc_mcux_api_stop               stop;
    adc_mcux_api_channel_setup      channel_setup;
    adc_mcux_api_set_reference      set_reference;
    adc_mcux_api_set_perf_level     set_perf_level;
    adc_mcux_api_calibrate          calibrate;
    adc_mcux_api_set_cal_params     set_cal_params;
    adc_mcux_api_get_cal_params     get_cal_params;
};

__syscall int adc_mcux_read(struct device* dev, const adc_mcux_sequence_config_t* seq_cfg);

static inline int z_impl_adc_mcux_read(struct device* dev, const adc_mcux_sequence_config_t* seq_cfg)
{
    struct adc_mcux_driver_api *api;
    api = (struct adc_mcux_driver_api *)dev->driver_api;
    return api->read(dev, seq_cfg);
}

__syscall int adc_mcux_stop(struct device* dev);

static inline int z_impl_adc_mcux_stop(struct device* dev)
{
    struct adc_mcux_driver_api *api;
    api = (struct adc_mcux_driver_api *)dev->driver_api;
    return api->stop(dev);
}

__syscall int adc_mcux_channel_setup(struct device* dev, uint8_t seq_idx, 
                                        const adc_mcux_channel_config_t* ch_cfg);

static inline int z_impl_adc_mcux_channel_setup(struct device* dev, uint8_t seq_idx, 
                                                const adc_mcux_channel_config_t* ch_cfg)
{
    struct adc_mcux_driver_api *api;
    api = (struct adc_mcux_driver_api *)dev->driver_api;
    return api->channel_setup(dev, seq_idx, ch_cfg);
}

__syscall int adc_mcux_set_reference(struct device* dev, adc_mcux_ref_t ref);

static inline int z_impl_adc_mcux_set_reference(struct device* dev, adc_mcux_ref_t ref)
{
    struct adc_mcux_driver_api *api;
    api = (struct adc_mcux_driver_api *)dev->driver_api;
    return api->set_reference(dev, ref);
}

__syscall int adc_mcux_set_perf_level(struct device* dev, uint8_t level);

static inline int z_impl_adc_mcux_set_perf_level(struct device* dev, uint8_t level)
{
    struct adc_mcux_driver_api *api;
    api = (struct adc_mcux_driver_api *)dev->driver_api;
    return api->set_perf_level(dev, level);
}

__syscall int adc_mcux_calibrate(struct device* dev);

static inline int z_impl_adc_mcux_calibrate(struct device* dev)
{
    struct adc_mcux_driver_api *api;
    api = (struct adc_mcux_driver_api *)dev->driver_api;
    return api->calibrate(dev);
}

__syscall int adc_mcux_set_cal_params(struct device* dev, adc_mcux_cal_params_t* params);

static inline int z_impl_adc_mcux_set_cal_params(struct device* dev, adc_mcux_cal_params_t* params)
{
    struct adc_mcux_driver_api *api;
    api = (struct adc_mcux_driver_api *)dev->driver_api;
    return api->set_cal_params(dev, params);
}

__syscall int adc_mcux_get_cal_params(struct device* dev, adc_mcux_cal_params_t* params);

static inline int z_impl_adc_mcux_get_cal_params(struct device* dev, adc_mcux_cal_params_t* params)
{
    struct adc_mcux_driver_api *api;
    api = (struct adc_mcux_driver_api *)dev->driver_api;
    return api->get_cal_params(dev, params);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#include <syscalls/adc_mcux_edma.h>

#endif // _ZEPHYR_INCLUDE_DRIVERS_ADC_MCUX_EDMA_H_