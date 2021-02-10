#include "bcb_leds.h"
#include "bcb_etime.h"
#include "bcb_config.h"
#include "bcb_msmnt.h"
#include "bcb_zd.h"
#include "bcb_sw.h"
#include "bcb_ocp_otp.h"
#include "bcb_coap.h"
#include "bcb_shell.h"
#include "bcb.h"
#include <init.h>

static int bcb_sim_init()
{
	bcb_leds_init();
	bcb_etime_init();
	bcb_config_init();
	bcb_zd_init();
	bcb_msmnt_init();
	bcb_sw_init();
	bcb_ocp_otp_init();
	bcb_init();

#if CONFIG_BCB_COAP
	bcb_coap_init();
#endif

#if CONFIG_BCB_SHELL
	bcb_shell_init();
#endif
	return 0;
}

SYS_INIT(bcb_sim_init, APPLICATION, CONFIG_BCB_LIB_SIM_INIT_PRIORITY);