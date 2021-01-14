zephyr_library_get_current_dir_lib_name(${ZEPHYR_CURRENT_MODULE_DIR} lib_name)

if (TARGET ${lib_name})
    zephyr_library_amend()
    zephyr_library_source_remove("dma_mcux_edma.c")
else()
    zephyr_library()
endif()

zephyr_library_include_directories(.)
zephyr_include_directories(.)
zephyr_library_sources_ifdef(CONFIG_DMA_MCUX_EDMA      dma_mcux_partial.c)

