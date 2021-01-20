#include "services.h"
#include <stdint.h>
#include <mgmt/smp_udp.h>
#include <os_mgmt/os_mgmt.h>
#include <img_mgmt/img_mgmt.h>
#include <img_mgmt/image.h>

#define LOG_LEVEL CONFIG_ZERO_SERVICES_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(zero_services);

int services_init()
{
    int r;
    //struct image_version version;
    //r = img_mgmt_my_version(&version);
	os_mgmt_register_group();
	img_mgmt_register_group();
    r = smp_udp_open();
    if (r < 0) {
        LOG_ERR("Cannot not open smp udp %d", r);
        return r;
    }
    return 0;
}