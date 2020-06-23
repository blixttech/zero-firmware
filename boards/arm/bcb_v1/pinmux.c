/*
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include <drivers/pinmux.h>
#include <fsl_port.h>


static int frdm_k64f_pinmux_init(struct device *dev)
{
    ARG_UNUSED(dev);

    /*
      PINMUX device drivers are still registered with the name provided via Kconfig.  
      This is probably due to the label property is not set in the dts files. 
      Therefore, we have to depend on Kconfig and use hardcoded ports (and pins) to configure PINMUX.
     */

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

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart4), okay) && CONFIG_SERIAL
    /* UART4 RX, TX */
    pinmux_pin_set(porte, 24, PORT_PCR_MUX(kPORT_MuxAlt3));
    pinmux_pin_set(porte, 25, PORT_PCR_MUX(kPORT_MuxAlt3));
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(button_front), okay)
    /* SW1 / FRONT_SW - 85, PTC13 */
    pinmux_pin_set(portc, 13, PORT_PCR_MUX(kPORT_MuxAsGpio));
#endif
    
#if DT_NODE_HAS_STATUS(DT_NODELABEL(led_red), okay)
    /* Red LED - 86, PTC14 */
    pinmux_pin_set(portc, 14, PORT_PCR_MUX(kPORT_MuxAsGpio));
#endif 

#if DT_NODE_HAS_STATUS(DT_NODELABEL(led_green), okay)
    /* Green LED - 87, PTC15 */
    pinmux_pin_set(portc, 15, PORT_PCR_MUX(kPORT_MuxAsGpio));
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(on_off), okay)
    /* ON_OFF - 97, PTD4/LLWU_P14 (ALT1) */
    pinmux_pin_set(portd, 4, PORT_PCR_MUX(kPORT_MuxAsGpio));
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(oc_ot_reset), okay)
    /* OC_OT_RESET - 98, PTD5 (ALT1)*/
    pinmux_pin_set(portd, 5, PORT_PCR_MUX(kPORT_MuxAsGpio));
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(on_off_status), okay)
    /* ON_OFF_STATUS - 93, PTD0/LLWU_P12 (ALT4 - FTM3_CH0)/ 94, PTD1, ADC0_SE5b (ALT1 - PTD1) */
    pinmux_pin_set(portd, 0, PORT_PCR_MUX(kPORT_MuxAlt4));
    pinmux_pin_set(portd, 1, PORT_PCR_MUX(kPORT_MuxAsGpio));
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(spi2), okay) && CONFIG_SPI
    /* EEPROM_WP - 65, PTB19 */
    pinmux_pin_set(portb, 19, PORT_PCR_MUX(kPORT_MuxAsGpio));
    /* EEPROM_SS - 66, PTB20 (ALT2 - SPI2_PCS0) */
    pinmux_pin_set(portb, 20, PORT_PCR_MUX(kPORT_MuxAlt2));
    /* SPI_SCLK - 67, PTB21 (ALT2 - SPI2_SCK) */
    pinmux_pin_set(portd, 21, PORT_PCR_MUX(kPORT_MuxAlt2));
    /* SPI_MOSI - 68, PTB22 (ALT2 - SPI2_SOUT) */
    pinmux_pin_set(portd, 22, PORT_PCR_MUX(kPORT_MuxAlt2));
    /* SPI_MISO - 69, PTB23 (ALT2 - SPI2_SIN)*/
    pinmux_pin_set(portd, 23, PORT_PCR_MUX(kPORT_MuxAlt2));
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(adc0), okay) && CONFIG_ADC
    /* I_LOW_GAIN - 70, PTC0, ADC0_SE14 */
    pinmux_pin_set(portc, 0, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
    /* I_HIGH_GAIN - 71, PTC1/LLWU_P6, ADC0_SE15 */
    pinmux_pin_set(portc, 1, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
    /* V_MAINS - 72, PTC2, ADC0_SE4b/CMP1_IN0 */
    pinmux_pin_set(portc, 2, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(adc1), okay) && CONFIG_ADC
    /* T_AMBIENT - 58, PTB10, ADC1_SE14 */
    pinmux_pin_set(portb, 10, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
    /* HW_REV_CTRL - 59, PTB11, ADC1_SE15 */
    pinmux_pin_set(portb, 11, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
    /* REF_1V5 - 80, PTC8, ADC1_SE4b/CMP0_IN2 */
    pinmux_pin_set(portc, 8, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
    /* T_MOSFET_OUT - 81, PTC9, ADC1_SE5b/CMP0_IN3 */
    pinmux_pin_set(portc, 9, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
    /* HW_REV_IN - 82, PTC10, ADC1_SE6b */
    pinmux_pin_set(portc, 10, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
    /* HW_REV_OUT - 83, PTC11/LLWU_P11, ADC1_SE7b */
    pinmux_pin_set(portc, 11, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
    /* OC_TEST_ADJ - 1, PTE0,ADC1_SE4a  */
    pinmux_pin_set(porte, 0, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
    /* T_MOSFET_IN - 2, PTE1/LLWU_P0, ADC1_SE5a */
    pinmux_pin_set(porte, 1, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
#endif

    /* Comparators */
    /* REF_1V5 - 73, PTC3/LLWU_P7, CMP1_IN1 */
    pinmux_pin_set(portc, 3, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
    /* REF_1V5 - 80, PTC8, ADC1_SE4b/CMP0_IN2 */
    pinmux_pin_set(portc, 8, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
    /* I_LOW_GAIN - 79, PTC6/LLWU_P10, CMP0_IN0 */
    pinmux_pin_set(portc, 6, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
    /* I_HIGH_GAIN - 78, PTC7, CMP0_IN1 */
    pinmux_pin_set(portc, 7, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));

#ifdef CONFIG_PWM_2
    /* OC_TEST_ADJ_PWM - 64, PTB18 (ALT3 - FTM2_CH0) */
    pinmux_pin_set(portb, 18, PORT_PCR_MUX(kPORT_MuxAlt3));
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(enet), okay) && CONFIG_NET_L2_ETHERNET
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
#endif

    return 0;
}

SYS_INIT(frdm_k64f_pinmux_init, PRE_KERNEL_1, CONFIG_PINMUX_INIT_PRIORITY);
