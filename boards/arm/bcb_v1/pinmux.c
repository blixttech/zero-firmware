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

#ifdef DT_ALIAS_PINMUX_A_LABEL
    struct device *porta = device_get_binding(DT_ALIAS_PINMUX_A_LABEL);
#endif
#ifdef DT_ALIAS_PINMUX_B_LABEL
    struct device *portb = device_get_binding(DT_ALIAS_PINMUX_B_LABEL);
#endif
#ifdef DT_ALIAS_PINMUX_C_LABEL
    struct device *portc = device_get_binding(DT_ALIAS_PINMUX_C_LABEL);
#endif
#ifdef DT_ALIAS_PINMUX_D_LABEL
    struct device *portd = device_get_binding(DT_ALIAS_PINMUX_D_LABEL);
#endif
#ifdef DT_ALIAS_PINMUX_E_LABEL
    struct device *porte = device_get_binding(DT_ALIAS_PINMUX_E_LABEL);
#endif

#ifdef CONFIG_UART_MCUX_4
    /* UART4 RX, TX */
    pinmux_pin_set(porte, 24, PORT_PCR_MUX(kPORT_MuxAlt3));
    pinmux_pin_set(porte, 25, PORT_PCR_MUX(kPORT_MuxAlt3));
#endif

#ifdef CONFIG_BUTTON_FRONT
    /* SW1 / FRONT_BUTTON */
    pinmux_pin_set(portc, 13, PORT_PCR_MUX(kPORT_MuxAsGpio));
#endif
    
#ifdef CONFIG_LED_0
    /* Red LED */
    pinmux_pin_set(portc, 14, PORT_PCR_MUX(kPORT_MuxAsGpio));
#endif 

#ifdef CONFIG_LED_1
    /* Green LED */
    pinmux_pin_set(portc, 15, PORT_PCR_MUX(kPORT_MuxAsGpio));
#endif

#ifdef CONFIG_SPI_2
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

#if CONFIG_ADC_0
    /* I_LOW_GAIN - 70, PTC0, ADC0_SE14 */
    pinmux_pin_set(portc, 0, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
    /* I_HIGH_GAIN - 71, PTC1/LLWU_P6, ADC0_SE15 */
    pinmux_pin_set(portc, 1, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
    /* V_MAINS - 72, PTC2, ADC0_SE4b/CMP1_IN0 */
    pinmux_pin_set(portc, 2, PORT_PCR_MUX(kPORT_PinDisabledOrAnalog));
#endif

#if CONFIG_ADC_1
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

#if CONFIG_ETH_MCUX_0
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
