/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_kinetis_ftm_input_capture

#include <drivers/clock_control.h>
#include <drivers/input_capture.h>
#include <errno.h>
#include <soc.h>
#include <fsl_ftm.h>
#include <fsl_clock.h>

#define LOG_LEVEL CONFIG_INPUT_CAPTURE_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(input_capture_mcux_ftm);