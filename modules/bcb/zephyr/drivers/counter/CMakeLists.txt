zephyr_library_get_current_dir_lib_name(${ZEPHYR_CURRENT_MODULE_DIR} lib_name)

if (TARGET ${lib_name})
    zephyr_library_amend()
    # We don't want to compile Zephyr's built-in ADC driver(s)
    zephyr_library_source_remove_all()
else()
    zephyr_library()
endif()

zephyr_library_include_directories(.)
zephyr_include_directories(.)

if(NOT BCB_MODULE_DUMMY)
    zephyr_library_sources_ifdef(CONFIG_COUNTER_MCUX_PIT    counter_ctd_mcux_pit.c)
else()
    zephyr_library_sources(counter_dummy.c)
endif()
