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