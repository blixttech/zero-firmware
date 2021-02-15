#include "bcb_msmnt_calib.h"
#include "bcb_msmnt.h"
#include <drivers/adc_dma.h>
#include <adc_mcux_edma.h>
#include <kernel.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_msmnt_calib);

#define SORT_AND_SET(field)                                                                        \
	do {                                                                                       \
		for (i = 0; i < samples; i++) {                                                    \
			sort_buf[i] = values[i].field;                                             \
		}                                                                                  \
		qsort(sort_buf, samples, sizeof(uint16_t), uint16_cmp);                            \
		current_values.field = sort_buf[samples / 2];                                      \
	} while (0)

static int uint16_cmp(const void *a, const void *b)
{
	uint16_t arg_a = *((uint16_t *)a);
	uint16_t arg_b = *((uint16_t *)b);

	if (arg_a < arg_b) {
		return -1;
	} else if (arg_a > arg_b) {
		return 1;
	} else {
		return 0;
	}
	return 0;
}

static int calibrate_adc(struct device *dev, uint16_t samples)
{
	adc_mcux_calibration_values_t *values = NULL;
	adc_mcux_calibration_values_t current_values;
	uint16_t *sort_buf = NULL;
	int i;
	int r;

	values = k_malloc(sizeof(adc_mcux_calibration_values_t) * samples);
	if (!values) {
		return -ENOMEM;
	}

	sort_buf = k_malloc(sizeof(uint16_t) * samples);
	if (!sort_buf) {
		r = -ENOMEM;
		goto cleanup;
	}

	for (i = 0; i < samples; i++) {
		r = adc_dma_calibrate(dev);
		if (r) {
			goto cleanup;
		}

		r = adc_dma_get_calibration_values(dev, &values[i],
						   sizeof(adc_mcux_calibration_values_t));
		if (r) {
			goto cleanup;
		}
	}

	SORT_AND_SET(ofs);
	SORT_AND_SET(pg);
	SORT_AND_SET(mg);
	SORT_AND_SET(clpd);
	SORT_AND_SET(clps);
	SORT_AND_SET(clp4);
	SORT_AND_SET(clp3);
	SORT_AND_SET(clp2);
	SORT_AND_SET(clp1);
	SORT_AND_SET(clp0);
	SORT_AND_SET(clmd);
	SORT_AND_SET(clms);
	SORT_AND_SET(clm4);
	SORT_AND_SET(clm3);
	SORT_AND_SET(clm2);
	SORT_AND_SET(clm1);
	SORT_AND_SET(clm0);

	r = adc_dma_set_calibration_values(dev, &current_values,
					   sizeof(adc_mcux_calibration_values_t));

cleanup:
	k_free(values);
	k_free(sort_buf);

	return r;
}

int bcb_msmnt_calib_adc(uint16_t samples)
{
	struct device *adc_dev;
	int r;

	bcb_msmnt_stop();

	adc_dev = device_get_binding(DT_LABEL(DT_NODELABEL(adc0)));
	if (adc_dev == NULL) {
		LOG_ERR("could not get ADC device %s", DT_LABEL(DT_NODELABEL(adc0)));
		return -EINVAL;
	}

	r = calibrate_adc(adc_dev, samples);
	if (r) {
		return r;
	}

	adc_dev = device_get_binding(DT_LABEL(DT_NODELABEL(adc1)));
	if (adc_dev == NULL) {
		LOG_ERR("could not get ADC device %s", DT_LABEL(DT_NODELABEL(adc1)));
		return -EINVAL;
	}

	r = calibrate_adc(adc_dev, samples);
	if (r) {
		return r;
	}

	r = bcb_msmnt_config_store();
	if (r) {
		return r;
	}

	r = bcb_msmnt_start();

	return r;
}


int bcb_msmnt_calib_b(bcb_msmnt_type_t type, uint16_t samples)
{
	uint32_t raw = 0;
	int i;
	int r;

	for (i = 0; i < samples; i++) {
		raw += bcb_msmnt_get_raw(type);
		k_sleep(K_MSEC(1));
	}

	raw /= samples;

	r = bcb_msmnt_set_calib_param_b(type, (uint16_t)raw);
	if (r) {
		LOG_ERR("cannot set b parameter: %d", r);
		return r;
	}

	r = bcb_msmnt_config_store();
	if (r) {
		LOG_ERR("cannot store b parameter: %d", r);
	}

	return r;
}

int bcb_msmnt_calib_a(bcb_msmnt_type_t type, int32_t x, uint16_t samples)
{
	uint32_t raw = 0;
	uint16_t a;
	uint16_t b;
	int i;
	int r;

	for (i = 0; i < samples; i++) {
		raw += bcb_msmnt_get_raw(type);
		k_sleep(K_MSEC(1));
	}

	raw /= samples;

	r = bcb_msmnt_get_calib_param_b(type, &b);
	if (r) {
		LOG_ERR("cannot get b parameter: %d", r);
		return r;
	}

	if (raw < (uint32_t)b) {
		LOG_ERR("x should be positive");
		return -EINVAL;
	}

	a = (uint16_t)(((uint32_t)raw - (uint32_t)b) * 1e3 / (uint32_t)x);
	r = bcb_msmnt_set_calib_param_a(type, a);
	if (r) {
		LOG_ERR("cannot set a parameter: %d", r);
		return r;
	}

	r = bcb_msmnt_config_store();
	if (r) {
		LOG_ERR("cannot store a parameter: %d", r);
	}

	return r;
}