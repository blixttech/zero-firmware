# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_include_directories(drivers ${ZEPHYR_BASE}/drivers)
zephyr_library_sources(drivers/blixt-breaker.c pinmux.c)

