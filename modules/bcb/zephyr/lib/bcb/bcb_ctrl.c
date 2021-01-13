#include <bcb_ctrl.h>
#include <bcb_etime.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/input_capture.h>
#include <drivers/pwm.h>
#include <drivers/dac.h>

#define LOG_LEVEL CONFIG_BCB_CTRL_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_ctrl);

/* ---------------- GPIO related macros ---------------- */

#define BCB_GPIO_DEV(pin_name)      (bcb_ctrl_data.port_##pin_name)

#define BCB_INIT_GPIO_PIN(dt_nlabel, pin_name, pin_flags)                                               \
    do {                                                                                                \
        BCB_GPIO_DEV(pin_name) = device_get_binding(DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(dt_nlabel),\
                                                                gpios, pin_name)));                     \
        if (BCB_GPIO_DEV(pin_name) == NULL) {                                                           \
            LOG_ERR("Could not get GPIO device");                                                       \
            return -EINVAL;                                                                             \
        }                                                                                               \
        gpio_pin_configure(BCB_GPIO_DEV(pin_name),                                                      \
                            DT_PHA_BY_NAME(DT_NODELABEL(dt_nlabel), gpios, pin_name, pin),              \
                            (pin_flags));                                                               \
    } while(0)                                                                              

#define BCB_SET_GPIO_PIN(dt_nlabel, pin_name, value)                                \
    gpio_pin_set_raw(BCB_GPIO_DEV(pin_name),                                        \
                    DT_PHA_BY_NAME(DT_NODELABEL(dt_nlabel), gpios, pin_name, pin),  \
                    (value))                     

#define BCB_GET_GPIO_PIN(dt_nlabel, pin_name)                                       \
    gpio_pin_get_raw(BCB_GPIO_DEV(pin_name),                                        \
                    DT_PHA_BY_NAME(DT_NODELABEL(dt_nlabel), gpios, pin_name, pin))


/* ---------------- Input capture related macros ---------------- */
#define BCB_IC_DEV(ch_name)         (bcb_ctrl_data.ic_##ch_name)

#define BCB_INIT_IC(dt_nlabel, ch_name)                                                                 \
    do {                                                                                                \
        BCB_IC_DEV(ch_name) = device_get_binding(DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(dt_nlabel),   \
                                                                input_captures, ch_name)));             \
        if (BCB_IC_DEV(ch_name) == NULL) {                                                              \
            LOG_ERR("Could not get IC device");                                                         \
            return -EINVAL;                                                                             \
        }                                                                                               \
    } while(0)

#define BCB_SET_IC_CHANNEL(dt_nlabel, ch_name)                                                          \
    input_capture_set_channel(BCB_IC_DEV(ch_name),                                                      \
                            DT_PHA_BY_NAME(DT_NODELABEL(dt_nlabel), input_captures, ch_name, channel),  \
                            DT_PHA_BY_NAME(DT_NODELABEL(dt_nlabel), input_captures, ch_name, edge))

#define BCB_GET_IC_VALUE(dt_nlabel, ch_name)                                                            \
    input_capture_get_value(BCB_IC_DEV(ch_name),                                                        \
                            DT_PHA_BY_NAME(DT_NODELABEL(dt_nlabel), input_captures, ch_name, channel))

#define BCB_GET_IC_COUNTER(ch_name)     input_capture_get_counter(BCB_IC_DEV(ch_name))

/* ---------------- On/off event timestamping related macros ---------------- */

#if (CONFIG_BCB_LIB_ETIME_SECOND != CONFIG_BCB_LIB_IC_ONOFF_SECOND)
#warning  BCB_LIB_ETIME_SECOND != BCB_LIB_IC_ONOFF_SECOND
#define BCB_IC_ONOFF_ETIME_COMP_HT(ic_ticks, etime_ticks)   (((ic_ticks) * CONFIG_BCB_LIB_ETIME_SECOND) > ((etime_ticks) * CONFIG_BCB_LIB_IC_ONOFF_SECOND))
#define BCB_ETIME_TICKS_TO_IC_ONOFF(etime_ticks)            ((etime_ticks) * CONFIG_BCB_LIB_IC_ONOFF_SECOND / CONFIG_BCB_LIB_ETIME_SECOND)
#else
#define BCB_IC_ONOFF_ETIME_COMP_HT(ic_ticks, etime_ticks)   ((ic_ticks) > (etime_ticks))
#define BCB_ETIME_TICKS_TO_IC_ONOFF(etime_ticks)            (etime_ticks)
#endif

#define BCB_IC_ONOFF_TICKS_GUARD                            100 

/* ---------------- Overcurrent protection test related macros ---------------- */
#define BCB_OCPT_ADJ_PWM_DEV(ch_name)            (bcb_ctrl_data.pwm_##ch_name)

#define BCB_INIT_OCPT_ADJ_PWM(dt_nlabel, ch_name)                                                               \
    do {                                                                                                        \
        BCB_OCPT_ADJ_PWM_DEV(ch_name) = device_get_binding(DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(dt_nlabel), \
                                                                pwms, ch_name)));                               \
        if (BCB_OCPT_ADJ_PWM_DEV(ch_name) == NULL) {                                                            \
            LOG_ERR("Could not get PWM device");                                                                \
            return -EINVAL;                                                                                     \
        }                                                                                                       \
    } while(0)

#define BCB_SET_OCPT_ADJ_PWM(dt_nlabel, ch_name, dcycle)                                                \
    pwm_pin_set_cycles(BCB_OCPT_ADJ_PWM_DEV(ch_name),                                                   \
                    DT_PHA_BY_NAME(DT_NODELABEL(dt_nlabel), pwms, ch_name, channel),                    \
                    DT_PHA_BY_NAME(DT_NODELABEL(dt_nlabel), pwms, ch_name, period),                     \
                    (DT_PHA_BY_NAME(DT_NODELABEL(dt_nlabel), pwms, ch_name, period)*(dcycle)/100),      \
                    0);

/* ---------------- Overcurrent protection limit related macros ---------------- */
#define BCB_OCPL_ADJ_DAC_DEV(ch_name)            (bcb_ctrl_data.dac_##ch_name)

#define BCB_INIT_OCPL_ADJ_DAC(dt_nlabel, ch_name)                                                               \
    do {                                                                                                        \
        BCB_OCPL_ADJ_DAC_DEV(ch_name) = device_get_binding(DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(dt_nlabel), \
                                                                io_channels, ch_name)));                        \
        if (BCB_OCPL_ADJ_DAC_DEV(ch_name) == NULL) {                                                            \
            LOG_ERR("Could not get DAC device");                                                                \
            return -EINVAL;                                                                                     \
        }                                                                                                       \
        struct dac_channel_cfg cfg =                                                                            \
        {                                                                                                       \
            .channel_id = DT_PHA_BY_NAME(DT_NODELABEL(dt_nlabel), io_channels, ch_name, output),                \
            .resolution = 12                                                                                    \
        };                                                                                                      \
        dac_channel_setup(BCB_OCPL_ADJ_DAC_DEV(ch_name), &cfg);                                                 \
    } while(0)

#define BCB_SET_OCPL_ADJ_DAC(dt_nlabel, ch_name, value)                                                 \
    do {                                                                                                \
        dac_write_value(BCB_OCPL_ADJ_DAC_DEV(ch_name),                                                  \
                        DT_PHA_BY_NAME(DT_NODELABEL(dt_nlabel), io_channels, ch_name, output),          \
                        value); \
    } while(0)

struct bcb_ctrl_data {
    struct device *port_on_off;
    struct device *port_ocp_otp_reset;
    struct device *port_on_off_status;
    struct device *port_ocp_test_tr_n;
    struct device *port_ocp_test_tr_p;
    struct device *ic_on_off_status_r;
    struct device *ic_on_off_status_f;
    struct device *ic_ocp_test_tr_n;
    struct device *ic_ocp_test_tr_p;
    struct device *pwm_ocp_test_adj;
    struct device *dac_ocp_limit_adj;
    bcb_ocp_callback_t ocp_callback;
    bcb_otp_callback_t otp_callback;
    bcb_ocpt_callback_t ocpt_callback;
    volatile uint64_t etime_on;
    volatile bool ocpt_active;
    volatile int ocpt_direction;
};

static struct bcb_ctrl_data bcb_ctrl_data;
static struct gpio_callback on_off_cb_data;

static void on_off_status_changed(struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    /* Timer used for capturing of/off status (FTM) is 16-bit.
     * Therefore, we cannot measure time durations longer than (2^16)*ftm_tick_time.
     * Due to this limitation, we use the elapsed time timer (PIT) which is 64-bit to measure 
     * longer durations than (2^16)*ftm_tick_time.
     * To be most accurate, we have to read the elapsed time before doing anything else.
     */
    uint64_t etime_now = bcb_etime_get_now();
    int on_off_status = BCB_GET_GPIO_PIN(dctrl, on_off_status);
    int on_off = BCB_GET_GPIO_PIN(dctrl, on_off);

    if (on_off_status) {
        /* ON event */
        bcb_ctrl_data.etime_on = etime_now;
    } else {
        /* OFF event */

        uint32_t ic_on = BCB_GET_IC_VALUE(itimestamp, on_off_status_r);
        uint32_t ic_off = BCB_GET_IC_VALUE(itimestamp, on_off_status_f);
        uint64_t onoff_duration =  (etime_now < bcb_ctrl_data.etime_on) ? 
                                    ((UINT64_MAX - bcb_ctrl_data.etime_on) + etime_now) : 
                                    (etime_now - bcb_ctrl_data.etime_on);

        /* Calculate time duration between ON and OFF events. */
        if (BCB_IC_ONOFF_ETIME_COMP_HT((uint64_t)(UINT16_MAX-BCB_IC_ONOFF_TICKS_GUARD), onoff_duration)) {
            /* We can use the input capture timer to measure the duration between ON and OFF events
             * more accurately.
             */
            onoff_duration = (ic_off < ic_on) ? ((uint32_t)UINT16_MAX - ic_on + ic_off) : (ic_off - ic_on);
        } else {
            /* Duration between ON event and OFF event is higher than what can be measured with the
             * input capture timer.
             */
            onoff_duration = BCB_ETIME_TICKS_TO_IC_ONOFF(onoff_duration);
        }

        if (on_off) {
            /* OFF event happened while the ON signal is still active. */
            if (bcb_ctrl_data.ocpt_active) {
                /* OCP test is on going. */
                /* Disable the OCP test in both directions to allow capacitor recharging. */
                bcb_ocpt_trigger(BCB_OCP_TEST_TGR_DIR_P, false);
                bcb_ocpt_trigger(BCB_OCP_TEST_TGR_DIR_N, false);

                if (bcb_ctrl_data.ocpt_callback) {
                    /* Read the timestamps when the OCP test has been started. */
                    uint32_t ocpt_start;
                    if (bcb_ctrl_data.ocpt_direction == BCB_OCP_TEST_TGR_DIR_P) {
                        ocpt_start = BCB_GET_IC_VALUE(itimestamp, ocp_test_tr_p);
                    } else {
                        ocpt_start = BCB_GET_IC_VALUE(itimestamp, ocp_test_tr_n);
                    }
                    uint64_t ocpt_duration = (ic_off < ocpt_start) 
                                                ? ((uint32_t)UINT16_MAX - ocpt_start + ic_off) 
                                                : (ic_off - ocpt_start);
                    bcb_ctrl_data.ocpt_callback(ocpt_duration, bcb_ctrl_data.ocpt_direction);
                }
            } else {
                /* Something else happened.
                 * For example, an inrush current situation or MOSFET over temperature situation. 
                 */
                /* TODO: Check the MOSFET temperature to see if the OTP is triggered. */
                if (bcb_ctrl_data.ocp_callback) {
                    bcb_ctrl_data.ocp_callback(onoff_duration);
                }
            }
        }
    }
}

static int bcb_ctrl_init()
{
    BCB_INIT_GPIO_PIN(dctrl, on_off, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    BCB_SET_GPIO_PIN(dctrl, on_off, 0);

    BCB_INIT_GPIO_PIN(dctrl, ocp_otp_reset, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    BCB_SET_GPIO_PIN(dctrl, ocp_otp_reset, 0);

    BCB_INIT_GPIO_PIN(dctrl, ocp_test_tr_n, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    BCB_SET_GPIO_PIN(dctrl, ocp_test_tr_n, 0);

    BCB_INIT_GPIO_PIN(dctrl, ocp_test_tr_p, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    BCB_SET_GPIO_PIN(dctrl, ocp_test_tr_p, 0);

    BCB_INIT_GPIO_PIN(dctrl, on_off_status, GPIO_INPUT);

    gpio_pin_interrupt_configure(BCB_GPIO_DEV(on_off_status), 
                                DT_PHA_BY_NAME(DT_NODELABEL(dctrl), gpios, on_off_status, pin), 
                                GPIO_INT_EDGE_BOTH);
    gpio_init_callback(&on_off_cb_data, 
                        on_off_status_changed, 
                        BIT(DT_PHA_BY_NAME(DT_NODELABEL(dctrl), gpios, on_off_status, pin)));
    gpio_add_callback(BCB_GPIO_DEV(on_off_status), &on_off_cb_data);

    /* Configure on/off status timestamping */
    BCB_INIT_IC(itimestamp, on_off_status_r);
    BCB_SET_IC_CHANNEL(itimestamp, on_off_status_r);

    BCB_INIT_IC(itimestamp, on_off_status_f);
    BCB_SET_IC_CHANNEL(itimestamp, on_off_status_f);

    /* Configure overcurrent test timestamping */
    BCB_INIT_IC(itimestamp, ocp_test_tr_n);
    BCB_SET_IC_CHANNEL(itimestamp, ocp_test_tr_n);

    BCB_INIT_IC(itimestamp, ocp_test_tr_p);
    BCB_SET_IC_CHANNEL(itimestamp, ocp_test_tr_p);

    /* Configure overcurrent protection test system */
    BCB_INIT_OCPT_ADJ_PWM(actrl, ocp_test_adj);
    BCB_SET_OCPT_ADJ_PWM(actrl, ocp_test_adj, 50);

    BCB_INIT_OCPL_ADJ_DAC(actrl, ocp_limit_adj);
    BCB_SET_OCPL_ADJ_DAC(actrl, ocp_limit_adj, 4095);
    //BCB_SET_OCPL_ADJ_DAC(actrl, ocp_limit_adj, 0);

    return 0;
}

int bcb_on()
{
    if (bcb_is_on()) {
        return 0;
    }
    /* We need to reset before tuning on since overcurrent/overtemperature may have been
     * triggered already.
     */
    bcb_reset();
    BCB_SET_GPIO_PIN(dctrl, on_off, 1);
    return 0;
}

int bcb_off()
{
    if (bcb_ctrl_data.ocpt_active) {
        /* Disable already active OCP test since there is no point of running it after the
         * breaker is turned off.
         */
        bcb_ocpt_trigger(BCB_OCP_TEST_TGR_DIR_P, false);
        bcb_ocpt_trigger(BCB_OCP_TEST_TGR_DIR_N, false);
    }
    
    BCB_SET_GPIO_PIN(dctrl, on_off, 0);
    return 0;
}

int bcb_is_on()
{
    return BCB_GET_GPIO_PIN(dctrl, on_off_status) == 1;
}

int bcb_reset()
{
    /* Generate a positive edge for the D flip-flop. */
    BCB_SET_GPIO_PIN(dctrl, ocp_otp_reset, 0);
    BCB_SET_GPIO_PIN(dctrl, ocp_otp_reset, 1);
    BCB_SET_GPIO_PIN(dctrl, ocp_otp_reset, 0);
    return 0;
}

int bcb_set_ocp_limit(uint32_t i_ma)
{
    /* TODO: Adjust the DAC output to change the OCP limit */
    return 0;
}


void bcb_set_ocp_callback(bcb_ocp_callback_t callback)
{
    bcb_ctrl_data.ocp_callback = callback;
}


void bcb_set_otp_callback(bcb_otp_callback_t callback)
{
    bcb_ctrl_data.otp_callback = callback;
}

void bcb_set_ocpt_callback(bcb_ocpt_callback_t callback)
{
    bcb_ctrl_data.ocpt_callback = callback;
}

int bcb_set_ocpt_current(uint32_t i_ma)
{
    /* TODO: Calculate the PWM duty cycle based on the current specified. */
    return 0;
}

int bcb_ocpt_trigger(int direction, bool enable)
{
    if (enable) {
        if (!bcb_is_on()) {
            /* Breaker should be On before doing an OCP test. */
            return -1;
        }

        if (direction == BCB_OCP_TEST_TGR_DIR_P) {
            BCB_SET_GPIO_PIN(dctrl, ocp_test_tr_p, 1);
        } else if (direction == BCB_OCP_TEST_TGR_DIR_N) {
            BCB_SET_GPIO_PIN(dctrl, ocp_test_tr_n, 1);
        } else {
            return -1;
        }
        bcb_ctrl_data.ocpt_active = enable;
        bcb_ctrl_data.ocpt_direction = direction;

    } else {
        /* OCP test can be done only one direction at a time. So disable both anyway. */
        BCB_SET_GPIO_PIN(dctrl, ocp_test_tr_p, 0);
        BCB_SET_GPIO_PIN(dctrl, ocp_test_tr_n, 0);
        bcb_ctrl_data.ocpt_active = false;
    }

    return 0;
}

SYS_INIT(bcb_ctrl_init, APPLICATION, CONFIG_BCB_LIB_CTRL_INIT_PRIORITY);