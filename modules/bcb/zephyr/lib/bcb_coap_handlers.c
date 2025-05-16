#include <lib/bcb_tc.h>
#include <lib/bcb_zd.h>
#include <lib/bcb_coap_handlers.h>
#include <lib/bcb_coap.h>
#include <lib/bcb_coap_buffer.h>
#include <lib/bcb_msmnt.h>
#include <lib/bcb_sw.h>
#include <lib/bcb.h>
#include <lib/bcb_tc_def.h>
#include <lib/bcb_tc_def_msm.h>
#include <lib/bcb_tc_def_csom_mod.h>
#include <stdbool.h>
#include <zc_messages.pb.h>
#include <pb.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <zephyr.h>
#include <kernel.h>
#include <net/socket.h>
#include <net/net_if.h>
#include <net/coap.h>
#include <net/coap_link_format.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#define COAP_CONTENT_FORMAT_NANOPB 30001
#define MAX_CURVE_POINTS CONFIG_BCB_TRIP_CURVE_DEFAULT_MAX_POINTS

#define LOG_LEVEL CONFIG_BCB_COAP_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_coap_handlers);

struct coap_handler_data {
	struct coap_resource *res_status;
	struct bcb_tc_callback trip_callback;
	uint16_t id;
	uint8_t token[8];
	uint8_t token_len;
	struct coap_packet response;
	pb_istream_t istream;
	pb_ostream_t ostream;
	zc_message_t zc_msg;
	uint8_t zc_buffer[ZC_MESSAGE_SIZE];
};

static struct coap_handler_data handler_data;

int bcb_coap_handlers_wellknowncore_get(struct coap_resource *resource, struct coap_packet *request,
					struct sockaddr *addr, socklen_t addr_len)
{
	int r;
	r = coap_well_known_core_get(resource, request, &handler_data.response,
				     bcb_coap_response_buffer(), CONFIG_BCB_COAP_MAX_MSG_LEN);
	if (r < 0) {
		return r;
	}

	return bcb_coap_send_response(&handler_data.response, addr);
}

int bcb_coap_handlers_version_get(struct coap_resource *resource, struct coap_packet *request,
				  struct sockaddr *addr, socklen_t addr_len)
{
	int r;
	uint16_t format;
	struct net_if *default_if;
	zc_version_t *version;

	handler_data.id = coap_header_get_id(request);
	handler_data.token_len = coap_header_get_token(request, handler_data.token);

	r = coap_packet_init(&handler_data.response, bcb_coap_response_buffer(),
			     CONFIG_BCB_COAP_MAX_MSG_LEN, 1, COAP_TYPE_ACK, handler_data.token_len,
			     handler_data.token, COAP_RESPONSE_CODE_CONTENT, handler_data.id);
	if (r < 0) {
		return r;
	}

	format = htons(COAP_CONTENT_FORMAT_NANOPB);
	r = coap_packet_append_option(&handler_data.response, COAP_OPTION_CONTENT_FORMAT,
				      (uint8_t *)&format, sizeof(format));
	if (r < 0) {
		return r;
	}

	r = coap_packet_append_payload_marker(&handler_data.response);
	if (r < 0) {
		return r;
	}

	memset(&handler_data.zc_msg, 0, sizeof(handler_data.zc_msg));

	handler_data.zc_msg.which_msg = ZC_MESSAGE_RES_TAG;
	handler_data.zc_msg.msg.res.which_res = ZC_RESPONSE_VERSION_TAG;
	version = &handler_data.zc_msg.msg.res.res.version;

	version->api = ZC_API_VERSION_1;
	version->has_uuid = true;
	version->uuid.size = 16;
	/* Copy UIDH, UIDMH, UIDML and UIDL  */
	memcpy(&version->uuid.bytes[0], (void *)&SIM->UIDH, 16);

	default_if = net_if_get_default();
	if (default_if && default_if->if_dev &&
	    pb_membersize(zc_version_t, link_addr) >= default_if->if_dev->link_addr.len) {
		uint8_t i;

		version->has_link_addr = true;
		version->link_addr.size = default_if->if_dev->link_addr.len;

		for (i = 0; i < default_if->if_dev->link_addr.len; i++) {
			version->link_addr.bytes[i] = default_if->if_dev->link_addr.addr[i];
		}
	}

	handler_data.ostream =
		pb_ostream_from_buffer(handler_data.zc_buffer, sizeof(handler_data.zc_buffer));
	if (!pb_encode(&handler_data.ostream, ZC_MESSAGE_FIELDS, &handler_data.zc_msg)) {
		LOG_ERR("cannot encode version %s", handler_data.ostream.errmsg);
		return -EINVAL;
	}

	r = coap_packet_append_payload(&handler_data.response, handler_data.zc_buffer,
				       handler_data.ostream.bytes_written);
	if (r < 0) {
		return r;
	}

	return bcb_coap_send_response(&handler_data.response, addr);
}

static inline void encode_status(zc_status_t *status)
{
	status->uptime = k_uptime_get_32();

	if (bcb_sw_is_on()) {
		status->switch_state = ZC_SWITCH_STATE_CLOSED;
	} else {
		status->switch_state = ZC_SWITCH_STATE_OPENED;
	}

	switch (bcb_get_state()) {
	case BCB_TC_STATE_OPENED:
		status->device_state = ZC_DEVICE_STATE_OPENED;
		break;
	case BCB_TC_STATE_CLOSED:
		status->device_state = ZC_DEVICE_STATE_CLOSED;
		break;
	case BCB_TC_STATE_TRANSIENT:
		status->device_state = ZC_DEVICE_STATE_TRANSIENT;
		break;
	default:
		status->device_state = ZC_DEVICE_STATE_UNDEFINED;
		break;
	}

	switch (bcb_get_cause()) {
	case BCB_TC_CAUSE_EXT:
		status->cause = ZC_TRIP_CAUSE_EXT;
		break;
	case BCB_TC_CAUSE_OCP_HW:
		status->cause = ZC_TRIP_CAUSE_OCP_HW;
		break;
	case BCB_TC_CAUSE_OCP_SW:
		status->cause = ZC_TRIP_CAUSE_OCP_CURVE;
		break;
	case BCB_TC_CAUSE_OCP_TEST:
		status->cause = ZC_TRIP_CAUSE_OCP_HW_TEST;
		break;
	case BCB_TC_CAUSE_OTP:
		status->cause = ZC_TRIP_CAUSE_OTP;
		break;
	case BCB_TC_CAUSE_UVP:
		status->cause = ZC_TRIP_CAUSE_UVP;
		break;
	default:
		status->cause = ZC_TRIP_CAUSE_NONE;
		break;
	}

	status->current = bcb_msmnt_get_current_rms();
	status->voltage = bcb_msmnt_get_voltage_rms();
	status->freq = bcb_zd_get_frequency();
	status->direction = ZC_FLOW_DIRECTION_FORWARD;

	status->temp_count = 4;
	status->temp[0].loc = ZC_TEMP_LOC_AMB;
	status->temp[0].value = bcb_msmnt_get_temp(BCB_TEMP_SENSOR_AMB);
	status->temp[1].loc = ZC_TEMP_LOC_MCU_1;
	status->temp[1].value = bcb_msmnt_get_temp(BCB_TEMP_SENSOR_MCU);
	status->temp[2].loc = ZC_TEMP_LOC_BRD_1;
	status->temp[2].value = bcb_msmnt_get_temp(BCB_TEMP_SENSOR_PWR_IN);
	status->temp[3].loc = ZC_TEMP_LOC_BRD_2;
	status->temp[3].value = bcb_msmnt_get_temp(BCB_TEMP_SENSOR_PWR_OUT);
}

static int send_notification_status(struct sockaddr *addr, uint8_t type, uint16_t id,
				    uint8_t *token, uint8_t token_len, bool notify,
				    uint32_t obs_seq)
{
	int r;
	uint16_t format;

	r = coap_packet_init(&handler_data.response, bcb_coap_response_buffer(),
			     CONFIG_BCB_COAP_MAX_MSG_LEN, 1, type, token_len, token,
			     COAP_RESPONSE_CODE_CONTENT, id);
	if (r < 0) {
		return r;
	}

	if (notify) {
		r = coap_append_option_int(&handler_data.response, COAP_OPTION_OBSERVE, obs_seq);
		if (r < 0) {
			return r;
		}
	}

	format = htons(COAP_CONTENT_FORMAT_NANOPB);
	r = coap_packet_append_option(&handler_data.response, COAP_OPTION_CONTENT_FORMAT,
				      (uint8_t *)&format, sizeof(format));
	if (r < 0) {
		return r;
	}

	r = coap_packet_append_payload_marker(&handler_data.response);
	if (r < 0) {
		return r;
	}

	memset(&handler_data.zc_msg, 0, sizeof(handler_data.zc_msg));

	handler_data.zc_msg.which_msg = ZC_MESSAGE_RES_TAG;
	handler_data.zc_msg.msg.res.which_res = ZC_RESPONSE_STATUS_TAG;

	encode_status(&handler_data.zc_msg.msg.res.res.status);

	handler_data.ostream =
		pb_ostream_from_buffer(handler_data.zc_buffer, sizeof(handler_data.zc_buffer));
	if (!pb_encode(&handler_data.ostream, ZC_MESSAGE_FIELDS, &handler_data.zc_msg)) {
		LOG_ERR("cannot encode version %s", handler_data.ostream.errmsg);
		return -EINVAL;
	}

	r = coap_packet_append_payload(&handler_data.response, handler_data.zc_buffer,
				       handler_data.ostream.bytes_written);
	if (r < 0) {
		return r;
	}

	return bcb_coap_send_response(&handler_data.response, addr);
}

static int send_error_status(struct sockaddr *addr, uint8_t type, int error)
{
	int r;
	uint16_t format;

	r = coap_packet_init(&handler_data.response, bcb_coap_response_buffer(),
			     CONFIG_BCB_COAP_MAX_MSG_LEN, 1, type, handler_data.token_len,
			     handler_data.token, COAP_RESPONSE_CODE_CONTENT, handler_data.id);
	if (r < 0) {
		return r;
	}

	format = htons(COAP_CONTENT_FORMAT_NANOPB);
	r = coap_packet_append_option(&handler_data.response, COAP_OPTION_CONTENT_FORMAT,
				      (uint8_t *)&format, sizeof(format));
	if (r < 0) {
		return r;
	}

	r = coap_packet_append_payload_marker(&handler_data.response);
	if (r < 0) {
		return r;
	}

	memset(&handler_data.zc_msg, 0, sizeof(handler_data.zc_msg));

	handler_data.zc_msg.which_msg = ZC_MESSAGE_RES_TAG;
	handler_data.zc_msg.msg.res.which_res = ZC_RESPONSE_ERROR_TAG;
	handler_data.zc_msg.msg.res.res.error.code = abs(error);

	handler_data.ostream =
		pb_ostream_from_buffer(handler_data.zc_buffer, sizeof(handler_data.zc_buffer));
	if (!pb_encode(&handler_data.ostream, ZC_MESSAGE_FIELDS, &handler_data.zc_msg)) {
		LOG_ERR("cannot encode version %s", handler_data.ostream.errmsg);
		return -EINVAL;
	}

	r = coap_packet_append_payload(&handler_data.response, handler_data.zc_buffer,
				       handler_data.ostream.bytes_written);
	if (r < 0) {
		return r;
	}

	return bcb_coap_send_response(&handler_data.response, addr);
}

int bcb_coap_handlers_status_get(struct coap_resource *resource, struct coap_packet *request,
				 struct sockaddr *addr, socklen_t addr_len)
{
	handler_data.id = coap_header_get_id(request);
	handler_data.token_len = coap_header_get_token(request, handler_data.token);

	return send_notification_status(addr, COAP_TYPE_ACK, handler_data.id, handler_data.token,
					handler_data.token_len, false, 0);
}

int bcb_coap_handlers_status_post(struct coap_resource *resource, struct coap_packet *request,
				  struct sockaddr *addr, socklen_t addr_len)
{
	int obs_opt;
	int r;
	bool has_notifier = false;

	handler_data.id = coap_header_get_id(request);
	handler_data.token_len = coap_header_get_token(request, handler_data.token);
	obs_opt = coap_get_option_int(request, COAP_OPTION_OBSERVE);

	if (obs_opt == 0) {
		struct coap_option cntnt_fmt_opt;
		uint16_t format;
		uint16_t payload_len;
		const uint8_t *payload_data;

		memset(&cntnt_fmt_opt, 0, sizeof(cntnt_fmt_opt));

		r = coap_find_options(request, COAP_OPTION_CONTENT_FORMAT, &cntnt_fmt_opt, 1);
		if (r < 0 || r == 0 || cntnt_fmt_opt.len != sizeof(uint16_t)) {
			return -EINVAL;
		}

		format = (uint16_t)cntnt_fmt_opt.value[0] + ((uint16_t)cntnt_fmt_opt.value[1] << 8);
		if (format != htons(COAP_CONTENT_FORMAT_NANOPB)) {
			return -EINVAL;
		}

		payload_data = coap_packet_get_payload(request, &payload_len);
		if (!payload_data || !payload_len) {
			return -EINVAL;
		}

		handler_data.istream = pb_istream_from_buffer(payload_data, payload_len);

		memset(&handler_data.zc_msg, 0, sizeof(handler_data.zc_msg));

		if (!pb_decode(&handler_data.istream, ZC_MESSAGE_FIELDS, &handler_data.zc_msg)) {
			return -EINVAL;
		}

		if (handler_data.zc_msg.which_msg != ZC_MESSAGE_REQ_TAG ||
		    handler_data.zc_msg.msg.req.which_req != ZC_REQUEST_SET_CONFIG_TAG ||
		    handler_data.zc_msg.msg.req.req.set_config.config.which_config !=
			    ZC_CONFIG_NOTIF_TAG) {
			return -EINVAL;
		}

		r = bcb_coap_notifier_add(
			resource, request, addr,
			handler_data.zc_msg.msg.req.req.set_config.config.config.notif.interval);
		if (r == 0) {
			has_notifier = true;
		}

	} else if (obs_opt == 1) {
		/* Observer deregister request. Refer RFC7641 section 3.6 */
		bcb_coap_notifier_remove(addr, handler_data.token, handler_data.token_len);
	} else {
		/* Invalid observe option. */
	}

	return send_notification_status(addr, COAP_TYPE_ACK, handler_data.id, handler_data.token,
					handler_data.token_len, has_notifier, 0);
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

	send_notification_status(&observer->addr, type, coap_next_id(), observer->token,
				 observer->tkl, true, notifier->seq);
}

static inline void encode_config_curve(zc_curve_config_t *config)
{
	bcb_tc_pt_t points[MAX_CURVE_POINTS];
	uint8_t point_count;
	int i;

	point_count = sizeof(points) / sizeof(bcb_tc_pt_t);
	bcb_get_tc()->get_points(points, &point_count);

	if (pb_arraysize(zc_curve_config_t, points) < point_count) {
		point_count = pb_arraysize(zc_curve_config_t, points);
	}

	config->direction = ZC_FLOW_DIRECTION_FORWARD;
	config->points_count = point_count;
	for (i = 0; i < point_count; i++) {
		config->points[i].limit = points[i].i;
		config->points[i].duration = points[i].d;
	}
}

static inline void encode_config_csom_mod(zc_csom_mod_config_t *config)
{
	bcb_tc_def_csom_mod_config_t mod_config;

	bcb_tc_def_csom_mod_config_get(&mod_config);

	config->closed = mod_config.zdc_closed;
	config->period = mod_config.zdc_period;
}

static inline void encode_config_csom(zc_csom_config_t *config)
{
	bcb_tc_def_msm_config_t msm_config;

	bcb_tc_def_msm_config_get(&msm_config);

	switch (msm_config.csom) {
	case BCB_TC_DEF_MSM_CSOM_MOD: {
		config->enabled = true;
		encode_config_csom_mod(&config->config.mod);
	} break;
	default:
		config->enabled = false;
		break;
	}
}

static inline void encode_config_ocp(zc_ocp_hw_config_t *config)
{
	bcb_tc_def_msm_config_t msm_config;

	bcb_tc_def_msm_config_get(&msm_config);

	config->limit = bcb_get_tc()->get_limit_hw();
	config->rec_en = msm_config.rec_enabled;
	config->rec_attempts = msm_config.rec_attempts;
	config->rec_delay = msm_config.rec_delay;
	config->rec_reset_timeout = msm_config.rec_reset_timeout;
}

static inline void encode_config_ini_state(zc_ini_state_config_t *config)
{
	switch (bcb_get_ini_state()) {
	case BCB_INI_STATE_OPENED:
		config->state = ZC_DEVICE_INI_STATE_OPENED;
		break;
	case BCB_INI_STATE_CLOSED:
		config->state = ZC_DEVICE_INI_STATE_CLOSED;
		break;
	default:
		config->state = ZC_DEVICE_INI_STATE_PREVIOUS;
		break;
	}
}

static inline int send_config(struct sockaddr *addr, pb_size_t which_config)
{
	int r;
	uint16_t format;
	zc_config_t *config;
	int error = 0;

	r = coap_packet_init(&handler_data.response, bcb_coap_response_buffer(),
			     CONFIG_BCB_COAP_MAX_MSG_LEN, 1, COAP_TYPE_ACK, handler_data.token_len,
			     handler_data.token, COAP_RESPONSE_CODE_CONTENT, handler_data.id);
	if (r < 0) {
		return r;
	}

	format = htons(COAP_CONTENT_FORMAT_NANOPB);
	r = coap_packet_append_option(&handler_data.response, COAP_OPTION_CONTENT_FORMAT,
				      (uint8_t *)&format, sizeof(format));
	if (r < 0) {
		return r;
	}

	r = coap_packet_append_payload_marker(&handler_data.response);
	if (r < 0) {
		return r;
	}

	memset(&handler_data.zc_msg, 0, sizeof(handler_data.zc_msg));

	handler_data.zc_msg.which_msg = ZC_MESSAGE_RES_TAG;
	handler_data.zc_msg.msg.res.which_res = ZC_RESPONSE_CONFIG_TAG;
	config = &handler_data.zc_msg.msg.res.res.config;

	switch (which_config) {
	case ZC_REQUEST_GET_CONFIG_CURVE_TAG:
		config->which_config = ZC_CONFIG_CURVE_TAG;
		encode_config_curve(&config->config.curve);
		break;
	case ZC_REQUEST_GET_CONFIG_CSOM_TAG:
		config->which_config = ZC_CONFIG_CSOM_TAG;
		encode_config_csom(&config->config.csom);
		break;
	case ZC_REQUEST_GET_CONFIG_OCP_HW_TAG:
		config->which_config = ZC_CONFIG_OCP_HW_TAG;
		encode_config_ocp(&config->config.ocp_hw);
		break;
	case ZC_REQUEST_GET_CONFIG_OUVP_TAG:
		error = -ENOTSUP;
		goto send_error;
		break;
	case ZC_REQUEST_GET_CONFIG_OUFP_TAG:
		error = -ENOTSUP;
		goto send_error;
		break;
	case ZC_REQUEST_GET_CONFIG_NOTIF_TAG:
		/* This has to be done with the observe request. */
		error = -ENOTSUP;
		goto send_error;
		break;
	case ZC_REQUEST_GET_CONFIG_CALIB_TAG:
		error = -ENOTSUP;
		goto send_error;
		break;
	case ZC_REQUEST_GET_CONFIG_INI_TAG:
		config->which_config = ZC_CONFIG_INI_TAG;
		encode_config_ini_state(&config->config.ini);
		break;
	default:
		error = -EINVAL;
		goto send_error;
		break;
	}

	handler_data.ostream =
		pb_ostream_from_buffer(handler_data.zc_buffer, sizeof(handler_data.zc_buffer));
	if (!pb_encode(&handler_data.ostream, ZC_MESSAGE_FIELDS, &handler_data.zc_msg)) {
		LOG_ERR("cannot encode version %s", handler_data.ostream.errmsg);
		return -EINVAL;
	}

	r = coap_packet_append_payload(&handler_data.response, handler_data.zc_buffer,
				       handler_data.ostream.bytes_written);
	if (r < 0) {
		return r;
	}

	return bcb_coap_send_response(&handler_data.response, addr);

send_error:
	return send_error_status(addr, COAP_TYPE_ACK, error);
}

static inline int apply_config_curve(zc_curve_config_t *config)
{
	bcb_tc_pt_t points[MAX_CURVE_POINTS];
	uint8_t point_count;
	int i;

	point_count = sizeof(points) / sizeof(bcb_tc_pt_t);
	if (config->points_count < point_count) {
		point_count = config->points_count;
	}

	for (i = 0; i < point_count; i++) {
		points[i].i = config->points[i].limit;
		points[i].d = config->points[i].duration;
	}

	return bcb_get_tc()->set_points(&points[0], point_count);
}

static inline int apply_config_csom_mod(zc_csom_mod_config_t *config)
{
	bcb_tc_def_csom_mod_config_t mod_config;

	mod_config.zdc_closed = config->closed;
	mod_config.zdc_period = config->period;

	return bcb_tc_def_csom_mod_config_set(&mod_config);
}

static inline int apply_config_csom(zc_csom_config_t *config)
{
	int r = 0;
	bcb_tc_def_msm_config_t msm_config;

	bcb_tc_def_msm_config_get(&msm_config);

	if (config->enabled) {
		switch (config->which_config) {
		case ZC_CSOM_CONFIG_MOD_TAG:
			msm_config.csom = BCB_TC_DEF_MSM_CSOM_MOD;
			r = apply_config_csom_mod(&config->config.mod);
			break;
		default:
			r = -ENOTSUP;
			break;
		}
	} else {
		msm_config.csom = BCB_TC_DEF_MSM_CSOM_NONE;
	}

	return r < 0 ? r : bcb_tc_def_msm_config_set(&msm_config);
}

static inline int apply_config_ocp(zc_ocp_hw_config_t *config)
{
	bcb_tc_def_msm_config_t msm_config;
	int r;

	r = bcb_get_tc()->set_limit_hw(config->limit);
	if (r < 0) {
		return r;
	}

	bcb_tc_def_msm_config_get(&msm_config);

	msm_config.rec_enabled = config->rec_en;
	msm_config.rec_attempts = config->rec_attempts;
	msm_config.rec_delay = config->rec_delay;
	msm_config.rec_reset_timeout = config->rec_reset_timeout;

	return bcb_tc_def_msm_config_set(&msm_config);
}

static inline int apply_config_ini_state(zc_ini_state_config_t *config)
{
	bcb_ini_state_t state;

	switch (config->state) {
	case ZC_DEVICE_INI_STATE_OPENED:
		state = BCB_INI_STATE_OPENED;
		break;
	case ZC_DEVICE_INI_STATE_CLOSED:
		state = BCB_INI_STATE_CLOSED;
		break;
	case ZC_DEVICE_INI_STATE_PREVIOUS:
		state = BCB_INI_STATE_PREVIOUS;
		break;
	default:
		state = BCB_INI_STATE_OPENED;
		break;
	}

	return bcb_set_ini_state(state);
}

static inline int apply_config(struct sockaddr *addr, zc_config_t *config)
{
	int error = 0;

	switch (config->which_config) {
	case ZC_CONFIG_CURVE_TAG:
		error = apply_config_curve(&config->config.curve);
		break;
	case ZC_CONFIG_CSOM_TAG:
		error = apply_config_csom(&config->config.csom);
		break;
	case ZC_CONFIG_OCP_HW_TAG:
		error = apply_config_ocp(&config->config.ocp_hw);
		break;
	case ZC_CONFIG_OUVP_TAG:
		error = -ENOTSUP;
		goto send_error;
		break;
	case ZC_CONFIG_OUFP_TAG:
		error = -ENOTSUP;
		goto send_error;
		break;
	case ZC_CONFIG_NOTIF_TAG:
		error = -ENOTSUP;
		goto send_error;
		break;
	case ZC_CONFIG_CALIB_TAG:
		error = -ENOTSUP;
		goto send_error;
		break;
	case ZC_CONFIG_INI_TAG:
		error = apply_config_ini_state(&config->config.ini);
		break;
	default:
		error = -ENOTSUP;
		goto send_error;
		break;
	}

send_error:
	return send_error_status(addr, COAP_TYPE_ACK, error);
}

int bcb_coap_handlers_config_post(struct coap_resource *resource, struct coap_packet *request,
				  struct sockaddr *addr, socklen_t addr_len)
{
	struct coap_option cntnt_fmt_opt;
	uint16_t format;
	const uint8_t *payload_data;
	uint16_t payload_len;
	int error = 0;
	int r;

	handler_data.id = coap_header_get_id(request);
	handler_data.token_len = coap_header_get_token(request, handler_data.token);

	r = coap_find_options(request, COAP_OPTION_CONTENT_FORMAT, &cntnt_fmt_opt, 1);
	if (r < 0 || r == 0 || cntnt_fmt_opt.len != sizeof(uint16_t)) {
		error = -EINVAL;
		goto send_error;
	}

	format = (uint16_t)cntnt_fmt_opt.value[0] + ((uint16_t)cntnt_fmt_opt.value[1] << 8);
	if (format != htons(COAP_CONTENT_FORMAT_NANOPB)) {
		error = -EINVAL;
		goto send_error;
	}

	payload_data = coap_packet_get_payload(request, &payload_len);
	if (!payload_data || !payload_len) {
		error = -EINVAL;
		goto send_error;
	}

	handler_data.istream = pb_istream_from_buffer(payload_data, payload_len);

	memset(&handler_data.zc_msg, 0, sizeof(handler_data.zc_msg));

	if (!pb_decode(&handler_data.istream, ZC_MESSAGE_FIELDS, &handler_data.zc_msg)) {
		error = -EINVAL;
		goto send_error;
	}

	if (handler_data.zc_msg.which_msg != ZC_MESSAGE_REQ_TAG) {
		error = -EINVAL;
		goto send_error;
	}

	if (handler_data.zc_msg.msg.req.which_req == ZC_REQUEST_GET_CONFIG_TAG) {
		return send_config(addr, handler_data.zc_msg.msg.req.req.get_config.which_config);
	} else if (handler_data.zc_msg.msg.req.which_req == ZC_REQUEST_SET_CONFIG_TAG) {
		return apply_config(addr, &handler_data.zc_msg.msg.req.req.set_config.config);
	} else {
		error = -EINVAL;
		goto send_error;
	}

send_error:
	return send_error_status(addr, COAP_TYPE_ACK, error);
}

int bcb_coap_handlers_device_post(struct coap_resource *resource, struct coap_packet *request,
				  struct sockaddr *addr, socklen_t addr_len)
{
	struct coap_option cntnt_fmt_opt;
	uint16_t format;
	const uint8_t *payload_data;
	uint16_t payload_len;
	int error = 0;
	int r;

	handler_data.id = coap_header_get_id(request);
	handler_data.token_len = coap_header_get_token(request, handler_data.token);

	r = coap_find_options(request, COAP_OPTION_CONTENT_FORMAT, &cntnt_fmt_opt, 1);
	if (r < 0 || r == 0 || cntnt_fmt_opt.len != sizeof(uint16_t)) {
		error = -EINVAL;
		goto send_error;
	}

	format = (uint16_t)cntnt_fmt_opt.value[0] + ((uint16_t)cntnt_fmt_opt.value[1] << 8);
	if (format != htons(COAP_CONTENT_FORMAT_NANOPB)) {
		error = -EINVAL;
		goto send_error;
	}

	payload_data = coap_packet_get_payload(request, &payload_len);
	if (!payload_data || !payload_len) {
		error = EINVAL;
		goto send_error;
	}

	handler_data.istream = pb_istream_from_buffer(payload_data, payload_len);

	memset(&handler_data.zc_msg, 0, sizeof(handler_data.zc_msg));

	if (!pb_decode(&handler_data.istream, ZC_MESSAGE_FIELDS, &handler_data.zc_msg)) {
		error = -EINVAL;
		goto send_error;
	}

	if (handler_data.zc_msg.which_msg != ZC_MESSAGE_REQ_TAG ||
	    handler_data.zc_msg.msg.req.which_req != ZC_REQUEST_CMD_TAG) {
		error = -EINVAL;
		goto send_error;
	}

	switch (handler_data.zc_msg.msg.req.req.cmd.cmd) {
	case ZC_DEVICE_CMD_OPEN:
		r = bcb_open();
		break;
	case ZC_DEVICE_CMD_CLOSE:
		r = bcb_close();
		break;
	case ZC_DEVICE_CMD_TOGGLE:
		r = bcb_toggle();
		break;
	default:
		break;
	}

	error = r;

send_error:
	return send_error_status(addr, COAP_TYPE_ACK, error);
}

void bcb_trip_curve_callback(const struct bcb_tc *curve, bcb_tc_cause_t type)
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
	bcb_add_tc_callback(&handler_data.trip_callback);

	return 0;
}
