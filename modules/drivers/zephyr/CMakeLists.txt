# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_include_directories(.)
zephyr_library_sources_ifdef(CONFIG_INPUT_CAPTURE_MCUX_FTM      input_capture_mcux_ftm.c)
zephyr_library_sources_ifdef(CONFIG_COUNTER_CTD_MCUX_PIT        counter_ctd_mcux_pit.c)
zephyr_library_sources_ifdef(CONFIG_ADC_MCUX_EDMA               adc_mcux_edma.c)
zephyr_library_sources_ifdef(CONFIG_ADC_MCUX_EDMA               adc_mcux_ftm_trigger.c)
zephyr_library_sources_ifdef(CONFIG_ADC_MCUX_EDMA               adc_mcux_lptmr_trigger.c)
zephyr_library_sources_ifdef(CONFIG_DMA_MCUX_EDMA               dma_mcux_partial.c)
zephyr_library_sources_ifdef(CONFIG_CMP_MCUX                    cmp_mcux.c)
zephyr_library_sources_ifdef(CONFIG_EEPROM_M95080               eeprom_m95080.c)
zephyr_include_directories(.)