zephyr_library_get_current_dir_lib_name(${ZEPHYR_CURRENT_MODULE_DIR} lib_name)

if (TARGET ${lib_name})
    zephyr_library_amend()
    # We don't want to compile Zephyr's built-in ADC driver(s)
    zephyr_library_source_remove_all()
else()
    zephyr_library()
endif()

if(CONFIG_ADC_DMA_MCUX)
    zephyr_include_directories(.)
    zephyr_library_sources(adc_dma_mcux.c)
endif()
