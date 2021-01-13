zephyr_library_get_current_dir_lib_name(${ZEPHYR_CURRENT_MODULE_DIR} lib_name)

if (TARGET ${lib_name})
    zephyr_library_amend()
else()
    zephyr_library()
endif()

zephyr_library_include_directories(.)
zephyr_include_directories(.)

if(NOT BCB_MODULE_DUMMY)
    zephyr_library_sources_ifdef(CONFIG_EEPROM_M95080      eeprom_m95080.c)
else()
    zephyr_library_sources(eeprom_dummy.c)
endif()
