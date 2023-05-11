#ifndef _ZEPHYR_INCLUDE_DT_BINDINGS_KINETIS_LPTMR_H_
#define _ZEPHYR_INCLUDE_DT_BINDINGS_KINETIS_LPTMR_H_

#define KINETIS_LPTMR_PRESCALER_CLOCK_0 0x0U
#define KINETIS_LPTMR_PRESCALER_CLOCK_1 0x1U
#define KINETIS_LPTMR_PRESCALER_CLOCK_2 0x2U
#define KINETIS_LPTMR_PRESCALER_CLOCK_3 0x3U

#define KINETIS_LPTMR_PRESCALE_GLITCH_0	 0x0U /*!< Prescaler divide 2 */
#define KINETIS_LPTMR_PRESCALE_GLITCH_1	 0x1U /*!< Prescaler divide 4, glitch filter 2 */
#define KINETIS_LPTMR_PRESCALE_GLITCH_2	 0x2U /*!< Prescaler divide 8, glitch filter 4 */
#define KINETIS_LPTMR_PRESCALE_GLITCH_3	 0x3U /*!< Prescaler divide 16, glitch filter 8 */
#define KINETIS_LPTMR_PRESCALE_GLITCH_4	 0x4U /*!< Prescaler divide 32, glitch filter 16 */
#define KINETIS_LPTMR_PRESCALE_GLITCH_5	 0x5U /*!< Prescaler divide 64, glitch filter 32 */
#define KINETIS_LPTMR_PRESCALE_GLITCH_6	 0x6U /*!< Prescaler divide 128, glitch filter 64 */
#define KINETIS_LPTMR_PRESCALE_GLITCH_7	 0x7U /*!< Prescaler divide 256, glitch filter 128 */
#define KINETIS_LPTMR_PRESCALE_GLITCH_8	 0x8U /*!< Prescaler divide 512, glitch filter 256 */
#define KINETIS_LPTMR_PRESCALE_GLITCH_9	 0x9U /*!< Prescaler divide 1024, glitch filter 512 */
#define KINETIS_LPTMR_PRESCALE_GLITCH_10 0xAU /*!< Prescaler divide 2048 glitch filter 1024 */
#define KINETIS_LPTMR_PRESCALE_GLITCH_11 0xBU /*!< Prescaler divide 4096, glitch filter 2048 */
#define KINETIS_LPTMR_PRESCALE_GLITCH_12 0xCU /*!< Prescaler divide 8192, glitch filter 4096 */
#define KINETIS_LPTMR_PRESCALE_GLITCH_13 0xDU /*!< Prescaler divide 16384, glitch filter 8192 */
#define KINETIS_LPTMR_PRESCALE_GLITCH_14 0xEU /*!< Prescaler divide 32768, glitch filter 16384 */
#define KINETIS_LPTMR_PRESCALE_GLITCH_15 0xFU /*!< Prescaler divide 65536, glitch filter 32768 */

#endif // _ZEPHYR_INCLUDE_DT_BINDINGS_KINETIS_LPTMR_H_