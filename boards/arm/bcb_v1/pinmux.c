/*
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include <drivers/pinmux.h>
#include <fsl_port.h>

#define SET_PINMUX(pin_name) \
    frdm_k64f_pinmux_set( \
        DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(pins), pinmuxs, pin_name)), \
        DT_PHA_BY_NAME(DT_NODELABEL(pins), pinmuxs, pin_name, pin), \
        DT_PHA_BY_NAME(DT_NODELABEL(pins), pinmuxs, pin_name, function) \
    );

static void frdm_k64f_pinmux_set(const char* name, uint32_t pin, uint32_t func)
{
    struct device *dev = device_get_binding(name);
    if (dev == NULL) {
        return;
    }
    pinmux_pin_set(dev, pin, PORT_PCR_MUX(func));    
}

static int frdm_k64f_pinmux_init(struct device *dev)
{
    ARG_UNUSED(dev);

    /*
      PINMUX device drivers are still registered with the name provided via Kconfig.  
      This is probably due to the label property is not set in the dts files. 
      Therefore, we have to depend on Kconfig and use hardcoded ports (and pins) to configure PINMUX.
     */

#if 0
#ifdef CONFIG_PINMUX_MCUX_PORTA
	struct device *porta = device_get_binding(CONFIG_PINMUX_MCUX_PORTA_NAME);
#endif
#ifdef CONFIG_PINMUX_MCUX_PORTB
	struct device *portb = device_get_binding(CONFIG_PINMUX_MCUX_PORTB_NAME);
#endif
#ifdef CONFIG_PINMUX_MCUX_PORTC
	struct device *portc = device_get_binding(CONFIG_PINMUX_MCUX_PORTC_NAME);
#endif
#ifdef CONFIG_PINMUX_MCUX_PORTD
	struct device *portd = device_get_binding(CONFIG_PINMUX_MCUX_PORTD_NAME);
#endif
#ifdef CONFIG_PINMUX_MCUX_PORTE
	struct device *porte = device_get_binding(CONFIG_PINMUX_MCUX_PORTE_NAME);
#endif
#endif

#if CONFIG_SERIAL
    SET_PINMUX(uart_tx);
    SET_PINMUX(uart_rx);
    //pinmux_pin_set(porte, 24, PORT_PCR_MUX(kPORT_MuxAlt3));
    //pinmux_pin_set(porte, 25, PORT_PCR_MUX(kPORT_MuxAlt3));
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(led_red), okay)
    SET_PINMUX(led_red);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(led_green), okay)
    SET_PINMUX(led_green);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(sw_front), okay)
    SET_PINMUX(sw_front);
#endif


#if DT_NODE_HAS_STATUS(DT_NODELABEL(enet), okay) && CONFIG_NET_L2_ETHERNET
{
    struct device *porta = device_get_binding(CONFIG_PINMUX_MCUX_PORTA_NAME);
    struct device *portb = device_get_binding(CONFIG_PINMUX_MCUX_PORTB_NAME);

    /* PHY_RXER - 39, PTA5 (ALT4 - RMII0_RXER/MII0_RXER) */
    pinmux_pin_set(porta,  5, PORT_PCR_MUX(kPORT_MuxAlt4));
    /* PHY_RXD1 - 42, PTA12 (ALT4 - RMII0_RXD1/MII0_RXD1) */
    pinmux_pin_set(porta, 12, PORT_PCR_MUX(kPORT_MuxAlt4));
    /* PHY_RXD0 - 43, PTA13/LLWU_P4 (ALT4 - RMII0_RXD0/MII0_RXD0) */
    pinmux_pin_set(porta, 13, PORT_PCR_MUX(kPORT_MuxAlt4));
    /* PHY_CRS_DV - 44, PTA14 (ALT4 - RMII0_CRS_DV/MII0_RXDV) */
    pinmux_pin_set(porta, 14, PORT_PCR_MUX(kPORT_MuxAlt4));
    /* PHY_TXEN - 45, PTA15 (ALT4 - RMII0_TXEN/MII0_TXEN) */
    pinmux_pin_set(porta, 15, PORT_PCR_MUX(kPORT_MuxAlt4));
    /* PHY_TXD0 - 46, PTA16 (ALT4 - RMII0_TXD0/MII0_TXD0) */
    pinmux_pin_set(porta, 16, PORT_PCR_MUX(kPORT_MuxAlt4));
    /* PHY_TXD1 - 47, PTA17 (ALT4 - RMII0_TXD1/MII0_TXD1) */
    pinmux_pin_set(porta, 17, PORT_PCR_MUX(kPORT_MuxAlt4));
    /* PHY_MDIO - 53, PTB0/LLWU_P5 (ALT4 - RMII0_MDIO/MII0_MDIO) */
    pinmux_pin_set(portb,  0, PORT_PCR_MUX(kPORT_MuxAlt4)
        | PORT_PCR_ODE_MASK | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK);
    /* PHY_MDC - 54, PTB1 (ALT4 - RMII0_MDC/MII0_MDC) */
    pinmux_pin_set(portb,  1, PORT_PCR_MUX(kPORT_MuxAlt4));
    /* PHY_RESET - 55, PTB2 (ALT1 - PTB2) NOTE: Disable for the moment */
    pinmux_pin_set(portb, 2, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
    /* PHY_INTERUPT - PTB3 (ALT1 - PTB3) NOTE: Disable for the moment */
    pinmux_pin_set(portb, 3, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
}
#endif

    return 0;
}

SYS_INIT(frdm_k64f_pinmux_init, PRE_KERNEL_1, CONFIG_PINMUX_INIT_PRIORITY);
