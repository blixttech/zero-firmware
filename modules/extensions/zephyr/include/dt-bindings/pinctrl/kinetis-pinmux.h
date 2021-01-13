#ifndef _ZEPHYR_INCLUDE_DT_BINDINGS_KINETIS_PINMUX_H_
#define _ZEPHYR_INCLUDE_DT_BINDINGS_KINETIS_PINMUX_H_

#define PORT_PCR_MUX_DISABLED_OR_ANALOG   0U  /*!< Corresponding pin is disabled, but is used as an analog pin. */
#define PORT_PCR_MUX_GPIO                 1U  /*!< Corresponding pin is configured as GPIO. */
#define PORT_PCR_MUX_ALT2                 2U  /*!< Chip-specific */
#define PORT_PCR_MUX_ALT3                 3U  /*!< Chip-specific */
#define PORT_PCR_MUX_ALT4                 4U  /*!< Chip-specific */
#define PORT_PCR_MUX_ALT5                 5U  /*!< Chip-specific */
#define PORT_PCR_MUX_ALT6                 6U  /*!< Chip-specific */
#define PORT_PCR_MUX_ALT7                 7U  /*!< Chip-specific */


/* ----------------------------------------------------------------------------
   -- PORT Register Masks
   ---------------------------------------------------------------------------- */

/*!
 * @addtogroup PORT_Register_Masks PORT Register Masks
 * @{
 */

/*! @name PCR - Pin Control Register n */
/*! @{ */
#define PORT_PCR_PS_MASK                         (0x1U)
#define PORT_PCR_PS_SHIFT                        (0U)
/*! PS - Pull Select
 *  0b0..Internal pulldown resistor is enabled on the corresponding pin, if the corresponding PE field is set.
 *  0b1..Internal pullup resistor is enabled on the corresponding pin, if the corresponding PE field is set.
 */
//#define PORT_PCR_PS(x)                           (((uint32_t)(((uint32_t)(x)) << PORT_PCR_PS_SHIFT)) & PORT_PCR_PS_MASK)
#define PORT_PCR_PS(x)                           (((x) << PORT_PCR_PS_SHIFT) & PORT_PCR_PS_MASK)
#define PORT_PCR_PE_MASK                         (0x2U)
#define PORT_PCR_PE_SHIFT                        (1U)
/*! PE - Pull Enable
 *  0b0..Internal pullup or pulldown resistor is not enabled on the corresponding pin.
 *  0b1..Internal pullup or pulldown resistor is enabled on the corresponding pin, if the pin is configured as a digital input.
 */
//#define PORT_PCR_PE(x)                           (((uint32_t)(((uint32_t)(x)) << PORT_PCR_PE_SHIFT)) & PORT_PCR_PE_MASK)
#define PORT_PCR_PE(x)                           (((x) << PORT_PCR_PE_SHIFT) & PORT_PCR_PE_MASK)
#define PORT_PCR_SRE_MASK                        (0x4U)
#define PORT_PCR_SRE_SHIFT                       (2U)
/*! SRE - Slew Rate Enable
 *  0b0..Fast slew rate is configured on the corresponding pin, if the pin is configured as a digital output.
 *  0b1..Slow slew rate is configured on the corresponding pin, if the pin is configured as a digital output.
 */
//#define PORT_PCR_SRE(x)                          (((uint32_t)(((uint32_t)(x)) << PORT_PCR_SRE_SHIFT)) & PORT_PCR_SRE_MASK)
#define PORT_PCR_SRE(x)                           (((x) << PORT_PCR_SRE_SHIFT) & PORT_PCR_SRE_MASK)
#define PORT_PCR_PFE_MASK                        (0x10U)
#define PORT_PCR_PFE_SHIFT                       (4U)
/*! PFE - Passive Filter Enable
 *  0b0..Passive input filter is disabled on the corresponding pin.
 *  0b1..Passive input filter is enabled on the corresponding pin, if the pin is configured as a digital input. Refer to the device data sheet for filter characteristics.
 */
//#define PORT_PCR_PFE(x)                          (((uint32_t)(((uint32_t)(x)) << PORT_PCR_PFE_SHIFT)) & PORT_PCR_PFE_MASK)
#define PORT_PCR_PFE(x)                           (((x) << PORT_PCR_PFE_SHIFT) & PORT_PCR_PFE_MASK)
#define PORT_PCR_ODE_MASK                        (0x20U)
#define PORT_PCR_ODE_SHIFT                       (5U)
/*! ODE - Open Drain Enable
 *  0b0..Open drain output is disabled on the corresponding pin.
 *  0b1..Open drain output is enabled on the corresponding pin, if the pin is configured as a digital output.
 */
//#define PORT_PCR_ODE(x)                          (((uint32_t)(((uint32_t)(x)) << PORT_PCR_ODE_SHIFT)) & PORT_PCR_ODE_MASK)
#define PORT_PCR_ODE(x)                           (((x) << PORT_PCR_ODE_SHIFT) & PORT_PCR_ODE_MASK)
#define PORT_PCR_DSE_MASK                        (0x40U)
#define PORT_PCR_DSE_SHIFT                       (6U)
/*! DSE - Drive Strength Enable
 *  0b0..Low drive strength is configured on the corresponding pin, if pin is configured as a digital output.
 *  0b1..High drive strength is configured on the corresponding pin, if pin is configured as a digital output.
 */
//#define PORT_PCR_DSE(x)                          (((uint32_t)(((uint32_t)(x)) << PORT_PCR_DSE_SHIFT)) & PORT_PCR_DSE_MASK)
#define PORT_PCR_DSE(x)                           (((x) << PORT_PCR_DSE_SHIFT) & PORT_PCR_DSE_MASK)
#define PORT_PCR_MUX_MASK                        (0x700U)
#define PORT_PCR_MUX_SHIFT                       (8U)
/*! MUX - Pin Mux Control
 *  0b000..Pin disabled (analog).
 *  0b001..Alternative 1 (GPIO).
 *  0b010..Alternative 2 (chip-specific).
 *  0b011..Alternative 3 (chip-specific).
 *  0b100..Alternative 4 (chip-specific).
 *  0b101..Alternative 5 (chip-specific).
 *  0b110..Alternative 6 (chip-specific).
 *  0b111..Alternative 7 (chip-specific).
 */
//#define PORT_PCR_MUX(x)                          (((uint32_t)(((uint32_t)(x)) << PORT_PCR_MUX_SHIFT)) & PORT_PCR_MUX_MASK)
#define PORT_PCR_MUX(x)                           (((x) << PORT_PCR_MUX_SHIFT) & PORT_PCR_MUX_MASK)
#define PORT_PCR_LK_MASK                         (0x8000U)
#define PORT_PCR_LK_SHIFT                        (15U)
/*! LK - Lock Register
 *  0b0..Pin Control Register fields [15:0] are not locked.
 *  0b1..Pin Control Register fields [15:0] are locked and cannot be updated until the next system reset.
 */
//#define PORT_PCR_LK(x)                           (((uint32_t)(((uint32_t)(x)) << PORT_PCR_LK_SHIFT)) & PORT_PCR_LK_MASK)
#define PORT_PCR_LK(x)                           (((x) << PORT_PCR_LK_SHIFT) & PORT_PCR_LK_MASK)
#define PORT_PCR_IRQC_MASK                       (0xF0000U)
#define PORT_PCR_IRQC_SHIFT                      (16U)
/*! IRQC - Interrupt Configuration
 *  0b0000..Interrupt/DMA request disabled.
 *  0b0001..DMA request on rising edge.
 *  0b0010..DMA request on falling edge.
 *  0b0011..DMA request on either edge.
 *  0b1000..Interrupt when logic 0.
 *  0b1001..Interrupt on rising-edge.
 *  0b1010..Interrupt on falling-edge.
 *  0b1011..Interrupt on either edge.
 *  0b1100..Interrupt when logic 1.
 */
//#define PORT_PCR_IRQC(x)                         (((uint32_t)(((uint32_t)(x)) << PORT_PCR_IRQC_SHIFT)) & PORT_PCR_IRQC_MASK)
#define PORT_PCR_IRQC(x)                           (((x) << PORT_PCR_IRQC_SHIFT) & PORT_PCR_IRQC_MASK)
#define PORT_PCR_ISF_MASK                        (0x1000000U)
#define PORT_PCR_ISF_SHIFT                       (24U)
/*! ISF - Interrupt Status Flag
 *  0b0..Configured interrupt is not detected.
 *  0b1..Configured interrupt is detected. If the pin is configured to generate a DMA request, then the corresponding flag will be cleared automatically at the completion of the requested DMA transfer. Otherwise, the flag remains set until a logic 1 is written to the flag. If the pin is configured for a level sensitive interrupt and the pin remains asserted, then the flag is set again immediately after it is cleared.
 */
//#define PORT_PCR_ISF(x)                          (((uint32_t)(((uint32_t)(x)) << PORT_PCR_ISF_SHIFT)) & PORT_PCR_ISF_MASK)
#define PORT_PCR_ISF(x)                           (((x) << PORT_PCR_ISF_SHIFT) & PORT_PCR_ISF_MASK)
/*! @} */

/* The count of PORT_PCR */
#define PORT_PCR_COUNT                           (32U)


#endif // _ZEPHYR_INCLUDE_DT_BINDINGS_KINETIS_PINMUX_H_