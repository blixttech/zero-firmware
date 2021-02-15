# SPDX-License-Identifier: Apache-2.0

set(lib_sources
    bcb_sim.c
    bcb_user_if.c
    bcb_etime.c
    bcb_config.c
    bcb_zd.c
    bcb_msmnt.c
    bcb_msmnt_calib.c
    bcb_sw.c
    bcb_ocp_otp.c
    bcb.c
)

zephyr_library()
zephyr_library_include_directories(.)
zephyr_include_directories(.)
zephyr_library_sources(${lib_sources})
zephyr_library_sources_ifdef(CONFIG_BCB_TRIP_CURVE_DEFAULT  bcb_trip_curve_default.c)
zephyr_library_sources_ifdef(CONFIG_BCB_SHELL               bcb_shell.c)
zephyr_library_sources_ifdef(CONFIG_BCB_COAP                bcb_coap.c)
zephyr_library_sources_ifdef(CONFIG_BCB_COAP                bcb_coap_buffer.c)
zephyr_library_sources_ifdef(CONFIG_BCB_COAP                bcb_coap_handlers.c)

