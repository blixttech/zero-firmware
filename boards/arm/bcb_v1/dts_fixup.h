/* SPDX-License-Identifier: Apache-2.0 */

/* Board level DTS fixup file */

#ifdef DT_ALIAS_PINMUX_A_BASE_ADDRESS
#define DT_ALIAS_PINMUX_A_LABEL "porta"
#endif

#ifdef DT_ALIAS_PINMUX_B_BASE_ADDRESS
#define DT_ALIAS_PINMUX_B_LABEL "portb"
#endif

#ifdef DT_ALIAS_PINMUX_C_BASE_ADDRESS
#define DT_ALIAS_PINMUX_C_LABEL "portc"
#endif

#ifdef DT_ALIAS_PINMUX_D_BASE_ADDRESS
#define DT_ALIAS_PINMUX_D_LABEL "portd"
#endif

#ifdef DT_ALIAS_PINMUX_E_BASE_ADDRESS
#define DT_ALIAS_PINMUX_E_LABEL "porte"
#endif

/* End of Board Level DTS fixup file */
