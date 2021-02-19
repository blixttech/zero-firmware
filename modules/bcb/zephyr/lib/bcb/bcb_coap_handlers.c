#include "bcb_coap_handlers.h"
#include "bcb_coap.h"
#include "bcb_coap_buffer.h"
#include "bcb_msmnt.h"
#include "bcb_sw.h"
#include "bcb.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <zephyr.h>
#include <kernel.h>
#include <net/socket.h>
#include <net/net_if.h>
#include <net/coap.h>
#include <net/coap_link_format.h>

#define LOG_LEVEL CONFIG_BCB_COAP_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_coap_handlers);

struct coap_handler_data {
	struct coap_resource *res_status;
	struct bcb_trip_callback trip_callback;
};

static struct coap_handler_data handler_data;

int bcb_coap_handlers_wellknowncore_get(struct coap_resource *resource, struct coap_packet *request,
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

int bcb_coap_handlers_version_get(struct coap_resource *resource, struct coap_packet *request,
				  struct sockaddr *addr, socklen_t addr_len)
{
	uint16_t id;
	uint8_t token[8];
	uint8_t token_len;
	int r;

	id = coap_header_get_id(request);
	token_len = coap_header_get_token(request, token);

	struct coap_packet response;
	r = coap_packet_init(&response, bcb_coap_response_buffer(), CONFIG_BCB_COAP_MAX_MSG_LEN, 1,
			     COAP_TYPE_ACK, token_len, (uint8_t *)token, COAP_RESPONSE_CODE_CONTENT,
			     id);
	if (r < 0) {
		return r;
	}

	uint8_t format = 0;
	r = coap_packet_append_option(&response, COAP_OPTION_CONTENT_FORMAT, &format,
				      sizeof(format));
	if (r < 0) {
		return r;
	}

	r = coap_packet_append_payload_marker(&response);
	if (r < 0) {
		return r;
	}

	uint8_t payload[70];
	uint8_t consumed = 0;
	r = snprintk((char *)(payload + consumed), sizeof(payload) - consumed,
		     "%08" PRIx32 "%08" PRIx32 "%08" PRIx32 "%08" PRIx32, SIM->UIDH, SIM->UIDL,
		     SIM->UIDMH, SIM->UIDML);
	if (r < 0) {
		return -EINVAL;
	}
	consumed += r;

	r = snprintk((char *)(payload + consumed), sizeof(payload) - consumed, ",");
	if (r < 0) {
		return -EINVAL;
	}
	consumed += r;

	struct net_if *default_if = net_if_get_default();
	if (default_if && default_if->if_dev) {
		uint8_t i;
		for (i = 0; i < default_if->if_dev->link_addr.len; i++) {
			r = snprintk((char *)(payload + consumed), sizeof(payload) - consumed,
				     "%02" PRIx8, default_if->if_dev->link_addr.addr[i]);
			if (r < 0) {
				return -EINVAL;
			}
			consumed += r;
		}
	}

	/* Power-In, Power-Out, Controller board revisions */
	r = snprintk((char *)(payload + consumed), sizeof(payload) - consumed, ",1,1,1");
	if (r < 0) {
		return -EINVAL;
	}
	consumed += r;

	r = coap_packet_append_payload(&response, payload, consumed);
	if (r < 0) {
		return r;
	}

	return bcb_coap_send_response(&response, addr);
}

static uint8_t create_status_payload(uint8_t *buf, uint8_t buf_len)
{
	int r;
	uint8_t total;

	r = snprintk((char *)buf, buf_len, "%010" PRIu32 ",%1" PRIu8 ",%1" PRIu8 ",%1" PRIu8,
		     k_uptime_get_32(), (uint8_t)bcb_sw_is_on(), (uint8_t)0U, (uint8_t)0U);
	if (r < 0) {
		return 0;
	}
	total = r;

	r = snprintk((char *)buf + total, buf_len - total,
		     ",%07" PRId32 ",%06" PRIu32 ",%07" PRId32 ",%06" PRIu32,
		     bcb_msmnt_get_current(), bcb_msmnt_get_current_rms(), bcb_msmnt_get_voltage(),
		     bcb_msmnt_get_voltage_rms());
	if (r < 0) {
		return total;
	}
	total += r;

	r = snprintk((char *)buf + total, buf_len - total,
		     ",%04" PRId32 ",%04" PRId32 ",%04" PRId32 ",%04" PRId32,
		     bcb_msmnt_get_temp(BCB_TEMP_SENSOR_PWR_IN),
		     bcb_msmnt_get_temp(BCB_TEMP_SENSOR_PWR_OUT),
		     bcb_msmnt_get_temp(BCB_TEMP_SENSOR_AMB),
		     bcb_msmnt_get_temp(BCB_TEMP_SENSOR_MCU));
	if (r < 0) {
		return total;
	}
	total += r;

	return total;
}

int send_notification_status(struct coap_resource *resource, struct sockaddr *addr, uint8_t type,
			     uint16_t id, const uint8_t *token, uint8_t token_len, bool notify,
			     uint32_t obs_seq)
{
	int r;
	struct coap_packet response;
	uint8_t format = 0;
	uint8_t payload[70];

	r = coap_packet_init(&response, bcb_coap_response_buffer(), CONFIG_BCB_COAP_MAX_MSG_LEN, 1,
			     type, token_len, (uint8_t *)token, COAP_RESPONSE_CODE_CONTENT, id);
	if (r < 0) {
		return r;
	}

	if (notify) {
		r = coap_append_option_int(&response, COAP_OPTION_OBSERVE, obs_seq);
		if (r < 0) {
			return r;
		}
	}

	format = 0;
	r = coap_packet_append_option(&response, COAP_OPTION_CONTENT_FORMAT, &format,
				      sizeof(format));
	if (r < 0) {
		return r;
	}

	r = coap_packet_append_payload_marker(&response);
	if (r < 0) {
		return r;
	}

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

void bcb_coap_handlers_status_notify(struct coap_resource *resource, struct coap_observer *observer)
{
	struct bcb_coap_notifier *notifier = bcb_coap_get_notifier(resource, observer);
	bool pending;
	uint8_t type;

	if (!notifier) {
		return;
	}

	if (!notifier->msgs_no_ack) {
		return;
	}

	pending = bcb_coap_has_pending(&observer->addr);

	if (pending) {
		if (notifier->msgs_no_ack > 0) {
			notifier->msgs_no_ack--;
		}
	} else {
		notifier->msgs_no_ack = CONFIG_BCB_COAP_MAX_MSGS_NO_ACK;
	}

	if (notifier->is_async || (notifier->seq % 4 == 0 && !pending)) {
		type = COAP_TYPE_CON;
	} else {
		type = COAP_TYPE_NON_CON;
	}

	notifier->seq++;

	send_notification_status(resource, &observer->addr, type, coap_next_id(), observer->token,
				 observer->tkl, true, notifier->seq);
}

int bcb_coap_handlers_status_get(struct coap_resource *resource, struct coap_packet *request,
				 struct sockaddr *addr, socklen_t addr_len)
{
	struct coap_option options[4];
	uint16_t id;
	uint8_t token[8];
	uint8_t tkl;
	int obs_opt;
	int r;

	id = coap_header_get_id(request);
	tkl = coap_header_get_token(request, token);
	obs_opt = coap_get_option_int(request, COAP_OPTION_OBSERVE);

	if (obs_opt == 0) {
		bool has_notifier;
		int i;
		uint32_t period = CONFIG_BCB_COAP_MIN_OBSERVE_PERIOD;

		r = coap_find_options(request, COAP_OPTION_URI_QUERY, options, 4);
		if (r < 0) {
			return -EINVAL;
		}

		for (i = 0; i < r; i++) {
			if (options[i].len < 3) {
				continue;
			}
			if (options[i].value[0] == 'p' && options[i].value[1] == '=') {
				options[i].value[sizeof(options[i].value) - 1] = '\0';
				period = strtoul((char *)&options[i].value[2], NULL, 0);
			}
		}

		r = bcb_coap_notifier_add(resource, request, addr, period);
		if (r < 0) {
			has_notifier = false;
		} else {
			has_notifier = true;
		}

		return send_notification_status(resource, addr, COAP_TYPE_ACK, id, token, tkl,
						has_notifier, 0);
	}

	if (obs_opt == 1) {
		/* Observer deregister request. Refer RFC7641 section 3.6 */
		bcb_coap_notifier_remove(addr, token, tkl);
	}

	return send_notification_status(resource, addr, COAP_TYPE_ACK, id, token, tkl, false, 0);
}

int bcb_coap_handlers_switch_get(struct coap_resource *resource, struct coap_packet *request,
				 struct sockaddr *addr, socklen_t addr_len)
{
	uint16_t id;
	uint8_t token[8];
	uint8_t tkl;

	id = coap_header_get_id(request);
	tkl = coap_header_get_token(request, token);

	return send_notification_status(resource, addr, COAP_TYPE_ACK, id, token, tkl, false, 0);
}

int bcb_coap_handlers_switch_post(struct coap_resource *resource, struct coap_packet *request,
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

	int i;
	for (i = 0; i < r; i++) {
		if (options[i].len < 3) {
			continue;
		}
		if (options[i].len == 4 && !strncmp(options[i].value, "a=on", 4)) {
			bcb_close();
		} else if (options[i].len == 5 && !strncmp(options[i].value, "a=off", 5)) {
			bcb_open();
		} else {
			continue;
		}
	}

	return send_notification_status(resource, addr, addr_len, id, token, token_len, false, 0);
}

void bcb_trip_curve_callback(const struct bcb_trip_curve *curve, bcb_trip_cause_t type, uint8_t limit)
{
	if (!handler_data.res_status) {
		return;
	}

	bcb_coap_notify_async(handler_data.res_status);
}

int bcb_coap_handlers_init(void)
{
	memset(&handler_data, 0, sizeof(handler_data));

	handler_data.res_status = bcb_coap_get_resource(BCB_COAP_RESOURCE_STATUS_PATH);
	if (!handler_data.res_status) {
		LOG_ERR("cannot get switch resource");
		return -ENOENT;
	}

	handler_data.trip_callback.handler = bcb_trip_curve_callback;
	bcb_add_trip_callback(&handler_data.trip_callback);

	return 0;
}
