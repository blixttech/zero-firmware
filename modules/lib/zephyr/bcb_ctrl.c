#include <bcb_ctrl.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <fsl_ftm.h>

/**
 * @def BCB_OCP_TEST_TGR_DIR_P
 * Overcurrent protection test current direction positive. 
 * 
 * @def BCB_OCP_TEST_TGR_DIR_N
 * Overcurrent protection test current direction negative. 
 */

#define BCB_GPIO_PORT(pin_name)     (bcb_ctrl_data.port_##pin_name)

#define BCB_INIT_GPIO_PIN(dt_label, pin_name, pin_flags)                                                \
    do {                                                                                                \
        BCB_GPIO_PORT(pin_name) = device_get_binding(DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(dt_label),\
                                                                gpios, pin_name)));                     \
        if (BCB_GPIO_PORT(pin_name) != NULL) {                                                          \
            gpio_pin_configure(BCB_GPIO_PORT(pin_name),                                                 \
                                DT_PHA_BY_NAME(DT_NODELABEL(dt_label), gpios, pin_name, pin),           \
                                (pin_flags));                                                           \
        }                                                                                               \
    } while(0)                                                                              

#define BCB_SET_GPIO_PIN(dt_label, pin_name, value)                                 \
    gpio_pin_set_raw(BCB_GPIO_PORT(pin_name),                                       \
                    DT_PHA_BY_NAME(DT_NODELABEL(dt_label), gpios, pin_name, pin),   \
                    (value))                     

#define BCB_GET_GPIO_PIN(dt_label, pin_name)                                        \
    gpio_pin_get_raw(BCB_GPIO_PORT(pin_name),                                       \
                    DT_PHA_BY_NAME(DT_NODELABEL(dt_label), gpios, pin_name, pin)) 

struct bcb_ctrl_data {
    struct device*  port_on_off;
    struct device*  port_oc_ot_reset;
    struct device*  port_on_off_status;
    struct device*  port_oc_test_tr_n;
    struct device*  port_oc_test_tr_p;
    bcb_ocp_callback_t ocp_callback;
    bcb_otp_callback_t otp_callback;
};

struct bcb_ctrl_data bcb_ctrl_data;
static struct gpio_callback on_off_cb_data;

static void ftm3_irq_handler(void* arg)
{

}

static void on_off_status_changed(struct device* dev, struct gpio_callback* cb, u32_t pins)
{
    int on_off_status = BCB_GET_GPIO_PIN(dctrl, on_off_status);
    int on_off = BCB_GET_GPIO_PIN(dctrl, on_off);

    printk("\non_off_status_changed: status %d, on_off %d\n", on_off_status, on_off);

    if (on_off == 1 && on_off_status == 0) {
        /* OCP or OTP has been triggered. */
        /* TODO: Check the MOSFET temperature to see if the OTP is triggered. */
        if (bcb_ctrl_data.ocp_callback) {
            bcb_ctrl_data.ocp_callback(0);
        }
    }
}

static int bcb_ctrl_init()
{
    BCB_INIT_GPIO_PIN(dctrl, on_off, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    BCB_SET_GPIO_PIN(dctrl, on_off, 0);

    BCB_INIT_GPIO_PIN(dctrl, oc_ot_reset, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    BCB_SET_GPIO_PIN(dctrl, oc_ot_reset, 0);

    BCB_INIT_GPIO_PIN(dctrl, oc_test_tr_n, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    BCB_SET_GPIO_PIN(dctrl, oc_test_tr_n, 0);

    BCB_INIT_GPIO_PIN(dctrl, oc_test_tr_p, (GPIO_ACTIVE_HIGH | GPIO_OUTPUT_ACTIVE));
    BCB_SET_GPIO_PIN(dctrl, oc_test_tr_p, 0);

    BCB_INIT_GPIO_PIN(dctrl, on_off_status, GPIO_INPUT);

    gpio_pin_interrupt_configure(BCB_GPIO_PORT(on_off_status), 
                                DT_PHA_BY_NAME(DT_NODELABEL(dctrl), gpios, on_off_status, pin), 
                                GPIO_INT_EDGE_BOTH);
    gpio_init_callback(&on_off_cb_data, 
                        on_off_status_changed, 
                        BIT(DT_PHA_BY_NAME(DT_NODELABEL(dctrl), gpios, on_off_status, pin)));
    gpio_add_callback(BCB_GPIO_PORT(on_off_status), &on_off_cb_data);

    /* Configure on/off status timestamping */
    ftm_config_t ftm_info;
    FTM_GetDefaultConfig(&ftm_info);
    //FTM_Init((FTM_Type*)(DT_REG_ADDR(DT_NODELABEL(ftm3))), &ftm_info);

    IRQ_CONNECT(DT_IRQ(DT_NODELABEL(ftm3), irq), 
                DT_IRQ(DT_NODELABEL(ftm3), priority), 
                ftm3_irq_handler, 
                NULL, 
                0);
    irq_enable(DT_IRQ(DT_NODELABEL(ftm3), irq));

    return 0;
}

/**
 * @brief Turn on the breaker.
 * 
 * If the breaker is already on, this function simply returns 0.
 * Inside this function, a reset for the overcurrent and overtemperature protection 
 * is done before the actual turn on happens. @sa bcb_reset()
 * 
 * @return 0 if successful.  
 */
int bcb_on()
{
    if (bcb_is_on()) {
        return 0;
    }
    /*  We need to reset before tuning on since overcurrent/overtemperature may have been
        triggered already.
     */
    bcb_reset();
    /* TODO: Configure FTM3-CH0 in dual capture mode and start FTM3 1 MHz.  */
    BCB_SET_GPIO_PIN(dctrl, on_off, 1);
    return 0;
}

/**
 * @brief Turn off the breaker.
 * 
 * @return 0 if successful. 
 */
int bcb_off()
{
    BCB_SET_GPIO_PIN(dctrl, on_off, 0);
    return 0;
}

/**
 * @brief Check if the breaker is turned on.
 * 
 * @return 1 if the breaker is turned on.
 */
int bcb_is_on()
{
    return BCB_GET_GPIO_PIN(dctrl, on_off_status) == 1;
}

/**
 * @brief Reset the overcurrent and the overtemperature protection.
 * 
 * If the overcurrent or the overtemperature protection is already triggered,
 * this function can be used to reset the trigger status.
 * Note that this fuction does not turn off the breaker. 
 * The breaker will be turned on again if this function is called just after the   
 * overcurrent or the overtemperature protection triggered.
 * If the breaker needs to be turned off, call @sa bcb_off() explicitly. 
 * 
 * @return Only for future use. Can be ignored. 
 */
int bcb_reset()
{
    /* Generate a positive edge for the D flip-flop. */
    BCB_SET_GPIO_PIN(dctrl, oc_ot_reset, 0);
    BCB_SET_GPIO_PIN(dctrl, oc_ot_reset, 1);
    BCB_SET_GPIO_PIN(dctrl, oc_ot_reset, 0);
    return 0;
}

/**
 * @brief Set the overcurrent protection limit.
 * 
 * @param i_ma Current limit in milliamperes.
 * @return 0 if successfull. 
 */
int bcb_set_ocp_limit(uint32_t i_ma)
{
    return 0;
}

/**
 * @brief Set the callback function for overcurrent protection trigger.
 * 
 * @param callback A valid handler function pointer.
 */
void bcb_set_ocp_callback(bcb_ocp_callback_t callback)
{
    bcb_ctrl_data.ocp_callback = callback;
}

/**
 * @brief Set the callback function for overtemperature protection trigger.
 * 
 * @param callback A valid handler function pointer.
 */
void bcb_set_otp_callback(bcb_otp_callback_t callback)
{
    bcb_ctrl_data.otp_callback = callback;
}

/**
 * @brief Set the test current used for overcurrent protection test.
 * 
 * @param i_ma The current in milliamperes.
 * @return 0 if successfull. 
 */
int bcb_ocp_set_test_current(uint32_t i_ma)
{
    return 0;
}

/**
 * @brief Trigger the overcurrent protection test.
 * 
 * @param direction The direction of the current flow
 * @return 0 if the function id called with correct parameters.
 */
int bcb_ocp_test_trigger(int direction)
{
    if (direction == BCB_OCP_TEST_TGR_DIR_P) {
        BCB_SET_GPIO_PIN(dctrl, oc_test_tr_p, 0);
        BCB_SET_GPIO_PIN(dctrl, oc_test_tr_p, 1);
        BCB_SET_GPIO_PIN(dctrl, oc_test_tr_p, 0);
    } else if (direction == BCB_OCP_TEST_TGR_DIR_N) {
        BCB_SET_GPIO_PIN(dctrl, oc_test_tr_n, 0);
        BCB_SET_GPIO_PIN(dctrl, oc_test_tr_n, 1);
        BCB_SET_GPIO_PIN(dctrl, oc_test_tr_n, 0);
    } else {
        return -1;
    }

    return 0;
}

#define BCB_CTRL_INIT_PRIORITY    50
SYS_INIT(bcb_ctrl_init, APPLICATION, BCB_CTRL_INIT_PRIORITY);