/*
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include <drivers/pinmux.h>
#include <fsl_port.h>

#define SET_PINMUX(label, pin_name) \
    frdm_k64f_pinmux_set( \
        DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(label), pinmuxs, pin_name)), \
        DT_PHA_BY_NAME(DT_NODELABEL(label), pinmuxs, pin_name, pin), \
        DT_PHA_BY_NAME(DT_NODELABEL(label), pinmuxs, pin_name, function) \
    );

static void frdm_k64f_pinmux_set(const char* name, uint32_t pin, uint32_t func)
{
    struct device *dev = device_get_binding(name);
    if (dev == NULL) {
        return;
    }
    pinmux_pin_set(dev, pin, func);    
}

static int frdm_k64f_pinmux_init(struct device *dev)
{
    ARG_UNUSED(dev);

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart4), okay) \
    && CONFIG_SERIAL
    SET_PINMUX(uart4_pins, uart_tx);
    SET_PINMUX(uart4_pins, uart_rx);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(spi2_pins), okay) \
    && CONFIG_SPI
    SET_PINMUX(spi2_pins, spi_mosi);
    SET_PINMUX(spi2_pins, spi_miso);
    SET_PINMUX(spi2_pins, spi_sclk);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(eeprom0_pins), okay) \
    && CONFIG_EEPROM
    SET_PINMUX(eeprom0_pins, eeprom_ss);
    SET_PINMUX(eeprom0_pins, eeprom_wp);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(breaker_pins), okay)
    SET_PINMUX(breaker_pins, led_red);
    SET_PINMUX(breaker_pins, led_green);
    SET_PINMUX(breaker_pins, sw_front);

    SET_PINMUX(breaker_pins, on_off);
    SET_PINMUX(breaker_pins, ocp_otp_reset);
    SET_PINMUX(breaker_pins, on_off_status);
    SET_PINMUX(breaker_pins, ocp_test_tr_n);
    SET_PINMUX(breaker_pins, ocp_test_tr_p);
    SET_PINMUX(breaker_pins, ocp_test_adj_pwm);

    SET_PINMUX(breaker_pins, v_mains);
    SET_PINMUX(breaker_pins, i_low_gain);
    SET_PINMUX(breaker_pins, i_high_gain);
    SET_PINMUX(breaker_pins, t_mosfet_out);
    SET_PINMUX(breaker_pins, t_mosfet_in);
    SET_PINMUX(breaker_pins, t_ambient);
    SET_PINMUX(breaker_pins, ref_1v5);
    SET_PINMUX(breaker_pins, hw_rev_in);
    SET_PINMUX(breaker_pins, hw_rev_out);
    SET_PINMUX(breaker_pins, hw_rev_ctrl);
    SET_PINMUX(breaker_pins, on_off_status_m);
    SET_PINMUX(breaker_pins, ocp_test_tr_n_m);
    SET_PINMUX(breaker_pins, ocp_test_tr_p_m);
    SET_PINMUX(breaker_pins, i_low_gain_cmp0_in0);
    SET_PINMUX(breaker_pins, i_high_gain_cmp0_in1);
    SET_PINMUX(breaker_pins, ref_1v5_cmp1_in1);
    SET_PINMUX(breaker_pins, zd_v_mains);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(enet), okay) \
    && CONFIG_NET_L2_ETHERNET \
    && DT_NODE_HAS_STATUS(DT_NODELABEL(enet_pins), okay)
    SET_PINMUX(enet_pins, phy_rxer);
    SET_PINMUX(enet_pins, phy_rxd1);
    SET_PINMUX(enet_pins, phy_rxd0);
    SET_PINMUX(enet_pins, phy_crs_dv);
    SET_PINMUX(enet_pins, phy_txen);
    SET_PINMUX(enet_pins, phy_txd0);
    SET_PINMUX(enet_pins, phy_txd1);
    SET_PINMUX(enet_pins, phy_mdio);
    SET_PINMUX(enet_pins, phy_mdc);
    SET_PINMUX(enet_pins, phy_reset);
    SET_PINMUX(enet_pins, phy_interrupt);
#endif

    return 0;
}

SYS_INIT(frdm_k64f_pinmux_init, PRE_KERNEL_1, CONFIG_PINMUX_INIT_PRIORITY);
