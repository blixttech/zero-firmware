#include <bcb_coap.h>
#include <bcb_ctrl.h>
#include <bcb_msmnt.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/_stdint.h>
#include <zephyr.h>
#include <kernel.h>
#include <init.h>
#include <net/socket.h>
#include <net/net_ip.h>
#include <net/udp.h>
#include <net/coap.h>
#include <net/coap_link_format.h>

#define LOG_LEVEL CONFIG_BCB_COAP_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_coap_handlers);


struct resource_obs_data {
    uint32_t seq;
};

int bcb_coap_handlers_wellknowncore_get(struct coap_resource *resource, 
                                        struct coap_packet *request, 
                                        struct sockaddr *addr, socklen_t addr_len)
{
    struct coap_packet response;
    uint8_t *data;

    data = (uint8_t *)k_malloc(CONFIG_BCB_COAP_MAX_MSG_LEN);
    if (!data) {
        return -ENOMEM;
    }

    int r;
    r = coap_well_known_core_get(resource, request, &response, data, CONFIG_BCB_COAP_MAX_MSG_LEN);
    if (r < 0) {
        r = -EPROTO;
        goto cleanup;
    }

    return bcb_coap_send_response(&response, addr, addr_len);

cleanup:
    k_free(data);
    return r;
}

static uint8_t bcb_coap_create_status_payload(uint8_t *buf, uint8_t buf_len)
{
    int r;
    r = snprintk((char*)buf, buf_len, 
                "%010" PRIu32 
                ",%1" PRIu8 ",%1" PRIu8 ",%1" PRIu8 
                ",%07" PRId32 ",%06" PRIu32 
                ",%07" PRId32 ",%06" PRIu32 
                ",%04" PRId32 ",%04" PRId32 ",%04" PRId32 ",%04" PRId32,
                k_uptime_get_32(),
                (uint8_t)bcb_is_on(), (uint8_t)0U, (uint8_t)0U,
                bcb_get_current(), bcb_get_current_rms(),
                bcb_get_voltage(), bcb_get_voltage_rms(),
                bcb_get_temp(BCB_TEMP_SENSOR_PWR_IN),
                bcb_get_temp(BCB_TEMP_SENSOR_PWR_OUT),
                bcb_get_temp(BCB_TEMP_SENSOR_AMB),
                bcb_get_temp(BCB_TEMP_SENSOR_MCU));
    if (r < 0) {
        return 0;
    }

    return (uint8_t)r;
}

int bcb_coap_send_notification_status(struct coap_resource *resource, 
                            struct sockaddr *addr, socklen_t addr_len,
                            uint16_t id, const uint8_t *token, uint8_t token_len, uint8_t notify)
{
    uint8_t type;
    if (notify) {
        type = COAP_TYPE_NON_CON;
    } else {
        type = COAP_TYPE_ACK;
    }

    int r;
    uint8_t *data;
    data = (uint8_t *)k_malloc(CONFIG_BCB_COAP_MAX_MSG_LEN);
    if (!data) {
        return -ENOMEM;
    }

    struct coap_packet response;
	r = coap_packet_init(&response, data, CONFIG_BCB_COAP_MAX_MSG_LEN, 1, 
                        type, token_len, (uint8_t *)token, COAP_RESPONSE_CODE_CONTENT, id);
    if (r < 0) {
        goto cleanup;
    }

    if (notify) {
        uint32_t age = *((uint32_t *)resource->user_data);
        r = coap_append_option_int(&response, COAP_OPTION_OBSERVE, age);
        if (r < 0) {
            goto cleanup;
        }   
    }

    uint8_t format = 0;
	r = coap_packet_append_option(&response, COAP_OPTION_CONTENT_FORMAT, &format, sizeof(format));
    if (r < 0) {
        goto cleanup;
    }

    r = coap_packet_append_payload_marker(&response);
    if (r < 0) {
        goto cleanup;
    }

    uint8_t payload[70];
    r = bcb_coap_create_status_payload(payload, sizeof(payload));
    if (r < 0) {
        r = -EINVAL;
        goto cleanup;
    }

	r = coap_packet_append_payload(&response, payload, r);
    if (r < 0) {
        goto cleanup;
    }

    return bcb_coap_send_response(&response, addr, addr_len);

cleanup:
    k_free(data);
    return r;
}

void bcb_coap_handlers_status_notify(struct coap_resource *resource, 
                                    struct coap_observer *observer)
{
    bcb_coap_send_notification_status(resource, 
                                    &observer->addr, sizeof(observer->addr), 
                                    coap_next_id(), observer->token, observer->tkl, true);
}


int bcb_coap_handlers_status_get(struct coap_resource *resource, 
                                struct coap_packet *request, 
                                struct sockaddr *addr, socklen_t addr_len)
{
    struct coap_option options[4];
    uint16_t id;
    uint8_t token[8];
    uint8_t token_len;
    int r;

    id = coap_header_get_id(request);
    token_len = coap_header_get_token(request, token);

    r = coap_find_options(request, COAP_OPTION_URI_QUERY, options, 4);
	if (r < 0) {
		return -EINVAL;
	}

	if (coap_request_is_observe(request)) {
        int i;
        uint32_t period = 0;
        r = coap_find_options(request, COAP_OPTION_URI_QUERY, options, 4);
        for (i = 0; i < r; i++) {
            if (options[i].len < 3) {
                continue;
            }
            if (options[i].value[0] == 'p' && options[i].value[1] == '=') {
                options[i].value[sizeof(options[i].value) -1] = '\0';
                period = strtoul((char*)&options[i].value[2], NULL, 0);
            }  
        }
        struct coap_observer* observer = bcb_coap_register_observer(resource, request, addr, period);
        if (observer) {
            return bcb_coap_send_notification_status(resource, addr, addr_len, 
                                                    id, token, token_len, true);
        }
	}

    return bcb_coap_send_notification_status(resource, addr, addr_len, 
                                            id, token, token_len, false);
}

int bcb_coap_handlers_switch_put(struct coap_resource *resource, 
                                struct coap_packet *request, 
                                struct sockaddr *addr, socklen_t addr_len)
{
    return -ENOENT;
}