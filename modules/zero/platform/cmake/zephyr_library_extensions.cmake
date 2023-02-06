# Sometimes, we need to enable certain kernel configurations to use a subset of existing 
# source files.
# For example, we need to enable some device drivers to use vendor-provided HAL source files.
# Following two functions provide ways to remove already included source files from the library
# build targets.
# These functions can be used together with already provided zephyr_library_amend() function.

function(zephyr_library_source_remove source)
    get_target_property(target_sources ${ZEPHYR_CURRENT_LIBRARY} SOURCES)
    list(LENGTH target_sources target_sources_len)
    if (target_sources)
        list(REMOVE_ITEM target_sources ${source})
        set_property(TARGET ${ZEPHYR_CURRENT_LIBRARY} PROPERTY SOURCES ${target_sources})
    endif()
endfunction()

function(zephyr_library_source_remove_all)
    set_property(TARGET ${ZEPHYR_CURRENT_LIBRARY} PROPERTY SOURCES)
endfunction()