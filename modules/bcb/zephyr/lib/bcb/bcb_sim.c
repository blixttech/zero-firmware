#include <lib/bcb_user_if.h>
#include <lib/bcb_etime.h>
#include <lib/bcb_config.h>
#include <lib/bcb_msmnt.h>
#include <lib/bcb_zd.h>
#include <lib/bcb_sw.h>
#include <lib/bcb_coap.h>
#include <lib/bcb_coap_handlers.h>
#include <lib/bcb_shell.h>
#include <lib/bcb.h>
#include <init.h>

static int bcb_sim_init()
{
	bcb_user_if_init();
	bcb_etime_init();
	bcb_config_init();
	bcb_zd_init();
	bcb_msmnt_init();
	bcb_sw_init();
	bcb_init();

#if CONFIG_BCB_COAP
	bcb_coap_init();
	bcb_coap_handlers_init();
#endif

#if CONFIG_BCB_SHELL
	bcb_shell_init();
#endif
	return 0;
}

SYS_INIT(bcb_sim_init, APPLICATION, CONFIG_BCB_LIB_SIM_INIT_PRIORITY);