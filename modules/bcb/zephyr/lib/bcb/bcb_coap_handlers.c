#include "bcb_coap_handlers.h"
#include "bcb_coap.h"
#include "bcb_coap_buffer.h"
#include "bcb_ctrl.h"
#include "bcb_msmnt.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <zephyr.h>
#include <kernel.h>
#include <net/socket.h>
#include <net/coap.h>
#include <net/coap_link_format.h>

#define LOG_LEVEL CONFIG_BCB_COAP_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_coap_handlers);

int bcb_coap_handlers_wellknowncore_get(struct coap_resource *resource, 
                                        struct coap_packet *request, 
                                        struct sockaddr *addr, socklen_t addr_len)
{
    struct coap_packet response;
    int r;
    r = coap_well_known_core_get(resource, request, &response, bcb_coap_response_buffer(), 
                                CONFIG_BCB_COAP_MAX_MSG_LEN);
    if (r < 0) {
        return r;
    }

    return bcb_coap_send_response(&response, addr);
}

static uint8_t create_status_payload(uint8_t *buf, uint8_t buf_len)
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

int send_notification_status(struct coap_resource *resource, 
                            struct sockaddr *addr, socklen_t addr_len,
                            uint16_t id, const uint8_t *token, uint8_t token_len, uint8_t notify, 
                            uint32_t age)
{
    uint8_t type;
    if (notify) {
        type = COAP_TYPE_NON_CON;
    } else {
        type = COAP_TYPE_ACK;
    }

    int r;
    struct coap_packet response;
	r = coap_packet_init(&response, bcb_coap_response_buffer(), CONFIG_BCB_COAP_MAX_MSG_LEN, 1, 
                        type, token_len, (uint8_t *)token, COAP_RESPONSE_CODE_CONTENT, id);
    if (r < 0) {
        return r;
    }

    if (notify) {
        r = coap_append_option_int(&response, COAP_OPTION_OBSERVE, age);
        if (r < 0) {
            return r;
        }   
    }

    uint8_t format = 0;
	r = coap_packet_append_option(&response, COAP_OPTION_CONTENT_FORMAT, &format, sizeof(format));
    if (r < 0) {
        return r;
    }

    r = coap_packet_append_payload_marker(&response);
    if (r < 0) {
        return r;
    }

    uint8_t payload[70];
    r = create_status_payload(payload, sizeof(payload));
    if (r < 0) {
        return r;
    }

	r = coap_packet_append_payload(&response, payload, r);
    if (r < 0) {
        return r;
    }

    return bcb_coap_send_response(&response, addr);
}

void bcb_coap_handlers_status_notify(struct coap_resource *resource, 
                                    struct coap_observer *observer)
{
    uint32_t age = bcb_coap_get_observer_sequence(resource, observer);
    send_notification_status(resource, 
                                    &observer->addr, sizeof(observer->addr), 
                                    coap_next_id(), observer->token, observer->tkl, true, age);
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
        r = bcb_coap_register_observer(resource, request, addr, period);
        if (r < 0) {
            return r;
        }

        return send_notification_status(resource, addr, addr_len, 
                                                id, token, token_len, true, 0);
        
	}

    return send_notification_status(resource, addr, addr_len, 
                                            id, token, token_len, false, 0);
}

int bcb_coap_handlers_switch_put(struct coap_resource *resource, 
                                struct coap_packet *request, 
                                struct sockaddr *addr, socklen_t addr_len)
{
    return -ENOENT;
}