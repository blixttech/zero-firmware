zephyr_library_get_current_dir_lib_name(${ZEPHYR_CURRENT_MODULE_DIR} lib_name)

if (TARGET ${lib_name})
    zephyr_library_amend()
    # We don't want to compile Zephyr's built-in SICI driver(s)
    zephyr_library_source_remove_all()
else()
    zephyr_library()
endif()

zephyr_library_sources_ifdef(CONFIG_SICI            sici.c)
zephyr_library_sources_ifdef(CONFIG_TLX4971         tlx4971.c)
zephyr_library_sources_ifdef(CONFIG_TLX4971_SHELL   tlx4971_shell.c)
