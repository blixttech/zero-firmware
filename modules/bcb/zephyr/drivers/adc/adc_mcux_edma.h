#ifndef _ZEPHYR_INCLUDE_DRIVERS_ADC_MCUX_EDMA_H_
#define _ZEPHYR_INCLUDE_DRIVERS_ADC_MCUX_EDMA_H_

#include <stdint.h>

typedef struct adc_mcux_calibration_values {
	uint16_t ofs;
	uint16_t pg;
	uint16_t mg;
	uint16_t clpd;
	uint16_t clps;
	uint16_t clp4;
	uint16_t clp3;
	uint16_t clp2;
	uint16_t clp1;
	uint16_t clp0;
	uint16_t clmd;
	uint16_t clms;
	uint16_t clm4;
	uint16_t clm3;
	uint16_t clm2;
	uint16_t clm1;
	uint16_t clm0;
} adc_mcux_calibration_values_t;

#endif /* _ZEPHYR_INCLUDE_DRIVERS_ADC_MCUX_EDMA_H_  */