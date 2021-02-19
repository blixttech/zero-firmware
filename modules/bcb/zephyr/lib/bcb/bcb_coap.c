#include "bcb_coap.h"
#include "bcb_coap_handlers.h"
#include "bcb_coap_buffer.h"
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <sys/slist.h>
#include <zephyr.h>
#include <init.h>
#include <kernel.h>
#include <net/socket.h>
#include <net/net_ip.h>
#include <net/udp.h>
#include <net/coap.h>
#include <net/coap_link_format.h>

#define LOG_LEVEL CONFIG_BCB_COAP_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_coap);

struct async_notify_item {
	struct coap_resource *resource;
};

struct bcb_coap_data {
	int socket;
	uint8_t req_buf[CONFIG_BCB_COAP_MAX_MSG_LEN];
	uint8_t res_buf[CONFIG_BCB_COAP_MAX_MSG_LEN];
	struct coap_pending pendings[CONFIG_BCB_COAP_MAX_PENDING];
	struct coap_observer observers[CONFIG_BCB_COAP_MAX_OBSERVERS];
	struct bcb_coap_notifier notifiers[CONFIG_BCB_COAP_MAX_OBSERVERS];
	struct k_delayed_work retransmit_work;
	struct k_delayed_work observer_work;
	struct k_work async_notify_work;
	struct k_work receive_work;
	struct k_fifo fifo;
	struct k_thread thread;
	K_THREAD_STACK_MEMBER(stack, CONFIG_BCB_COAP_THREAD_STACK_SIZE);
	struct coap_resource resources[];
};

K_MSGQ_DEFINE(async_notify_msgq, sizeof(struct async_notify_item), 10, 4);

static struct bcb_coap_data bcb_coap_data = {
    .resources = {
        {   .get = bcb_coap_handlers_wellknowncore_get,
            .path = COAP_WELL_KNOWN_CORE_PATH,
        },
        {   .get = bcb_coap_handlers_version_get,
            .path = BCB_COAP_RESOURCE_VERSION_PATH,
            .user_data = &((struct coap_core_metadata){
                            .attributes = BCB_COAP_RESOURCE_VERSION_ATTRIBUTES,
                        }),
        },
        {   .get = bcb_coap_handlers_status_get,
            .notify = bcb_coap_handlers_status_notify,
            .user_data = &((struct coap_core_metadata){
                            .attributes = BCB_COAP_RESOURCE_STATUS_ATTRIBUTES,
                        }),
            .path = BCB_COAP_RESOURCE_STATUS_PATH,
        },
        {   .get = bcb_coap_handlers_switch_get,
            .post = bcb_coap_handlers_switch_post,
            .user_data = &((struct coap_core_metadata){
                            .attributes = BCB_COAP_RESOURCE_SWITCH_ATTRIBUTES,
                        }),
            .path = BCB_COAP_RESOURCE_SWITCH_PATH,
        },
        { },
    },
};

static bool is_sockaddr_equal(const struct sockaddr *a, const struct sockaddr *b)
{
	/* FIXME: Should we consider ipv6-mapped ipv4 addresses as equal to  ipv4 addresses? */
	if (a->sa_family != b->sa_family) {
		return false;
	}

	if (a->sa_family == AF_INET) {
		const struct sockaddr_in *a4 = net_sin(a);
		const struct sockaddr_in *b4 = net_sin(b);
		if (a4->sin_port != b4->sin_port) {
			return false;
		}
		return net_ipv4_addr_cmp(&a4->sin_addr, &b4->sin_addr);
	}

	if (b->sa_family == AF_INET6) {
		const struct sockaddr_in6 *a6 = net_sin6(a);
		const struct sockaddr_in6 *b6 = net_sin6(b);
		if (a6->sin6_port != b6->sin6_port) {
			return false;
		}
		return net_ipv6_addr_cmp(&a6->sin6_addr, &b6->sin6_addr);
	}
	/* Invalid address family */
	return false;
}

static void notifier_remove_by_addr(const struct sockaddr *addr)
{
	int i;
	for (i = 0; i < CONFIG_BCB_COAP_MAX_OBSERVERS; i++) {
		struct bcb_coap_notifier *notifier = &bcb_coap_data.notifiers[i];
		if (!notifier->is_used) {
			continue;
		}

		if (is_sockaddr_equal(addr, &notifier->observer->addr)) {
			coap_remove_observer(notifier->resource, notifier->observer);
			memset(notifier->observer, 0, sizeof(struct coap_observer));
			notifier->is_used = false;
		}
	}
}

int bcb_coap_notifier_remove(const struct sockaddr *addr, const uint8_t *token, uint8_t tkl)
{
	int i;
	for (i = 0; i < CONFIG_BCB_COAP_MAX_OBSERVERS; i++) {
		struct bcb_coap_notifier *notifier = &bcb_coap_data.notifiers[i];
		if (!notifier->is_used) {
			continue;
		}

		if (is_sockaddr_equal(addr, &notifier->observer->addr) &&
		    memcmp(token, notifier->observer->token, tkl) == 0) {
			coap_remove_observer(notifier->resource, notifier->observer);
			memset(notifier->observer, 0, sizeof(struct coap_observer));
			notifier->is_used = false;
			return 0;
		}
	}

	return -ENOENT;
}

bool bcb_coap_has_pending(const struct sockaddr *addr)
{
	int i;
	for (i = 0; i < CONFIG_BCB_COAP_MAX_PENDING; i++) {
		if (bcb_coap_data.pendings[i].timeout != 0 &&
		    is_sockaddr_equal(addr, &bcb_coap_data.pendings[i].addr)) {
			return true;
		}
	}

	return false;
}

void pending_remove_by_addr(const struct sockaddr *addr)
{
	int i;
	for (i = 0; i < CONFIG_BCB_COAP_MAX_PENDING; i++) {
		if (bcb_coap_data.pendings[i].timeout != 0 &&
		    is_sockaddr_equal(addr, &bcb_coap_data.pendings[i].addr)) {
			coap_pending_clear(&bcb_coap_data.pendings[i]);
		}
	}
}

static void retransmit_work(struct k_work *work)
{
	struct coap_pending *pending;
	struct net_buf *buf;
	int r;

	pending = coap_pending_next_to_expire(bcb_coap_data.pendings, CONFIG_BCB_COAP_MAX_PENDING);
	if (!pending) {
		return;
	}

	buf = (struct net_buf *)pending->data;
	r = sendto(bcb_coap_data.socket, buf->data, buf->len, 0, &pending->addr,
		   sizeof(struct sockaddr));
	if (r < 0) {
		LOG_ERR("failed to send %d", errno);
	}

	if (!coap_pending_cycle(pending)) {
		/* This is the last retransmission */
		notifier_remove_by_addr(&pending->addr);
		bcb_coap_buf_free(buf);
		coap_pending_clear(pending);
		LOG_INF("removed stale observer");
	}

	pending = coap_pending_next_to_expire(bcb_coap_data.pendings, CONFIG_BCB_COAP_MAX_PENDING);
	if (pending) {
		k_delayed_work_submit(&bcb_coap_data.retransmit_work, K_MSEC(pending->timeout));
	}
}

static void periodic_notifier_schedule()
{
	int i;
	uint32_t t_now;
	uint32_t t_d = UINT32_MAX;

	t_now = k_uptime_get_32();

	for (i = 0; i < CONFIG_BCB_COAP_MAX_OBSERVERS; i++) {
		struct bcb_coap_notifier *notifier = &bcb_coap_data.notifiers[i];
		if (!notifier->is_used) {
			continue;
		}

		if (!notifier->period) {
			continue;
		}

		if (notifier->start + notifier->period - t_now < t_d) {
			t_d = notifier->start + notifier->period - t_now;
		}
	}

	if (t_d == UINT32_MAX) {
		return;
	}

	k_delayed_work_submit(&bcb_coap_data.observer_work, K_MSEC(t_d));
}

static void observer_notify_work(struct k_work *work)
{
	int i;
	for (i = 0; i < CONFIG_BCB_COAP_MAX_OBSERVERS; i++) {
		struct bcb_coap_notifier *notifier = &bcb_coap_data.notifiers[i];
		uint32_t t_now;

		if (!notifier->is_used) {
			continue;
		}

		if (!notifier->period) {
			continue;
		}

		t_now = k_uptime_get_32();
		if (t_now - notifier->start < notifier->period) {
			continue;
		}

		notifier->seq++;
		notifier->start = t_now;

		if (!notifier->resource->notify) {
			continue;
		}

		notifier->resource->notify(notifier->resource, notifier->observer);
	}

	periodic_notifier_schedule();
}

static void async_notify_work(struct k_work *work)
{
	struct async_notify_item item;

	while (!k_msgq_get(&async_notify_msgq, &item, K_NO_WAIT)) {
		int i;
		for (i = 0; i < CONFIG_BCB_COAP_MAX_OBSERVERS; i++) {
			struct bcb_coap_notifier *notifier = &bcb_coap_data.notifiers[i];
			if (!notifier->is_used) {
				continue;
			}

			if (notifier->resource != item.resource) {
				continue;
			}

			if (!notifier->resource->notify) {
				continue;
			}

			notifier->is_async = true;
			notifier->resource->notify(notifier->resource, notifier->observer);
			notifier->is_async = false;
		}
	}
}

static int create_pending_request(struct coap_packet *packet, const struct sockaddr *addr)
{
	struct coap_pending *pending;
	struct net_buf *buf;
	int r;

	pending = coap_pending_next_unused(bcb_coap_data.pendings, CONFIG_BCB_COAP_MAX_PENDING);
	if (!pending) {
		return -ENOMEM;
	}

	r = coap_pending_init(pending, packet, addr);
	if (r < 0) {
		return -EINVAL;
	}

	buf = bcb_coap_buf_alloc();
	if (!buf) {
		return -ENOMEM;
	}
	net_buf_add_mem(buf, packet->data, packet->offset);
	/* We need the net_buf pointer later to free up. */
	pending->data = (uint8_t *)buf;

	coap_pending_cycle(pending);

	pending = coap_pending_next_to_expire(bcb_coap_data.pendings, CONFIG_BCB_COAP_MAX_PENDING);
	if (!pending) {
		return -EINVAL;
	}

	k_delayed_work_submit(&bcb_coap_data.retransmit_work, K_MSEC(pending->timeout));

	return 0;
}

int bcb_coap_send_response(struct coap_packet *packet, const struct sockaddr *addr)
{
	int r;
	uint8_t type = coap_header_get_type(packet);
	if (type == COAP_TYPE_CON) {
		r = create_pending_request(packet, addr);
		if (r < 0) {
			LOG_WRN("unable to create pending");
		}
	}

	r = sendto(bcb_coap_data.socket, packet->data, packet->offset, 0, addr,
		   sizeof(struct sockaddr));
	if (r < 0) {
		LOG_ERR("failed to send %d", errno);
	}

	return r;
}

uint8_t *bcb_coap_response_buffer()
{
	return bcb_coap_data.res_buf;
}

int bcb_coap_notifier_add(struct coap_resource *resource, struct coap_packet *request,
			  struct sockaddr *addr, uint32_t period)
{
	int i;
	struct coap_observer *observer;

	if (!resource->notify) {
		LOG_WRN("notification not implemented");
		return -EINVAL;
	}

	if (period != 0 && period < CONFIG_BCB_COAP_MIN_OBSERVE_PERIOD) {
		period = CONFIG_BCB_COAP_MIN_OBSERVE_PERIOD;
	}

	observer =
		coap_observer_next_unused(bcb_coap_data.observers, CONFIG_BCB_COAP_MAX_OBSERVERS);

	if (!observer) {
		LOG_WRN("cannot find unused observer");
		return -ENOMEM;
	}

	coap_observer_init(observer, request, addr);
	coap_register_observer(resource, observer);

	for (i = 0; i < CONFIG_BCB_COAP_MAX_OBSERVERS; i++) {
		struct bcb_coap_notifier *notifier = &bcb_coap_data.notifiers[i];
		if (!notifier->is_used) {
			notifier = &bcb_coap_data.notifiers[i];
			notifier->is_used = true;
			notifier->resource = resource;
			notifier->observer = observer;
			notifier->seq = 0;
			notifier->period = period;
			notifier->msgs_no_ack = CONFIG_BCB_COAP_MAX_MSGS_NO_ACK;
			notifier->start = k_uptime_get_32();
			break;
		}
	}

	periodic_notifier_schedule();

	return 0;
}

struct bcb_coap_notifier *bcb_coap_get_notifier(struct coap_resource *resource,
						struct coap_observer *observer)
{
	int i;
	for (i = 0; i < CONFIG_BCB_COAP_MAX_OBSERVERS; i++) {
		struct bcb_coap_notifier *notifier = &bcb_coap_data.notifiers[i];
		if (notifier->is_used && notifier->resource == resource &&
		    notifier->observer == observer) {
			return notifier;
		}
	}

	return NULL;
}

static bool is_uri_path_equal(const char *const *path1, const char *const *path2)
{
	int i;
	bool is_equal = true;

	for (i = 0; path1[i] && path2[i]; i++) {
		size_t len1 = strlen(path1[i]);
		size_t len2 = strlen(path2[i]);

		if (len1 != len2) {
			is_equal = false;
			break;
		}

		if (memcmp(path1[i], path2[i], len1) != 0) {
			is_equal = false;
			break;
		}
	}

	return is_equal;
}

struct coap_resource *bcb_coap_get_resource(const char *const *path)
{
	struct coap_resource *resource;

	for (resource = bcb_coap_data.resources; resource && resource->path; resource++) {
		if (is_uri_path_equal(path, resource->path)) {
			return resource;
		}
	}

	return NULL;
}

int bcb_coap_notify_async(struct coap_resource *resource)
{
	struct async_notify_item item;
	int r;

	item.resource = resource;
	r = k_msgq_put(&async_notify_msgq, &item, K_NO_WAIT);

	if (r < 0) {
		return r;
	}

	k_work_submit(&bcb_coap_data.async_notify_work);

	return 0;
}

static int process_coap_packet(uint8_t *data, uint8_t data_len, struct sockaddr *client_addr,
			       socklen_t client_addr_len)
{
	struct coap_packet packet;
	struct coap_option options[16] = { 0 };
	uint8_t opt_num = 16U;
	uint8_t type;
	int r;

	r = coap_packet_parse(&packet, data, data_len, options, opt_num);
	if (r < 0) {
		LOG_ERR("invalid data received %d", r);
		return -EINVAL;
	}

	type = coap_header_get_type(&packet);

	if (type == COAP_TYPE_ACK) {
		struct coap_pending *pending;
		pending = coap_pending_received(&packet, bcb_coap_data.pendings,
						CONFIG_BCB_COAP_MAX_PENDING);
		if (!pending) {
			LOG_WRN("recevied ACK, but no pending");
			return -EINVAL;
		}
		/* pending->data has been replaced with the pointer to net_buf  */
		bcb_coap_buf_free((struct net_buf *)pending->data);
		coap_pending_clear(pending);
		return 0;

	} else if (type == COAP_TYPE_RESET) {
		notifier_remove_by_addr(client_addr);
		pending_remove_by_addr(client_addr);
		return 0;

	} else {
		r = coap_handle_request(&packet, bcb_coap_data.resources, options, opt_num,
					client_addr, client_addr_len);
		if (r < 0) {
			LOG_ERR("request handdling error %d", r);
		}
		return r;
	}

	return 0;
}

static void bcb_coap_receive_work(struct k_work *work)
{
	struct net_buf *buf;
	while ((buf = net_buf_get(&bcb_coap_data.fifo, K_NO_WAIT)) != NULL) {
		struct sockaddr *addr = (struct sockaddr *)net_buf_user_data(buf);
		process_coap_packet(buf->data, buf->len, addr, sizeof(struct sockaddr));
		bcb_coap_buf_free(buf);
	}
}

static void bcb_coap_receive_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("Started");

	while (1) {
		struct sockaddr addr;
		socklen_t addr_len = sizeof(addr);
		struct net_buf *buf;
		struct sockaddr *buf_ud;
		int received;

		received = recvfrom(bcb_coap_data.socket, bcb_coap_data.req_buf,
				    CONFIG_BCB_COAP_MAX_MSG_LEN, 0, &addr, &addr_len);
		if (received < 0) {
			LOG_ERR("reception error %d", errno);
			continue;
		}

		buf = bcb_coap_buf_alloc();
		if (!buf) {
			LOG_ERR("cannot allocate buffer");
			continue;
		}
		net_buf_add_mem(buf, bcb_coap_data.req_buf, received);
		buf_ud = net_buf_user_data(buf);
		net_ipaddr_copy(buf_ud, &addr);

		net_buf_put(&bcb_coap_data.fifo, buf);
		k_work_submit(&bcb_coap_data.receive_work);
	}
}

int bcb_coap_start_server()
{
	int r;
	struct sockaddr_in addr;

	bcb_coap_data.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (bcb_coap_data.socket < 0) {
		LOG_ERR("failed to create UDP socket %d", errno);
		return -errno;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(CONFIG_BCB_COAP_SERVER_PORT);

	r = bind(bcb_coap_data.socket, (struct sockaddr *)&addr, sizeof(addr));
	if (r < 0) {
		LOG_ERR("failed to bind UDP socket %d", errno);
		return -errno;
	}

	k_thread_create(&bcb_coap_data.thread, bcb_coap_data.stack,
			K_THREAD_STACK_SIZEOF(bcb_coap_data.stack), bcb_coap_receive_thread, NULL,
			NULL, NULL, CONFIG_BCB_COAP_THREAD_PRIORITY, 0, K_FOREVER);
	k_thread_name_set(&bcb_coap_data.thread, "coap");
	k_thread_start(&bcb_coap_data.thread);

	return 0;
}

int bcb_coap_init(void)
{
	memset(&bcb_coap_data.notifiers, 0, sizeof(bcb_coap_data.notifiers));
	k_delayed_work_init(&bcb_coap_data.retransmit_work, retransmit_work);
	k_delayed_work_init(&bcb_coap_data.observer_work, observer_notify_work);
	k_work_init(&bcb_coap_data.async_notify_work, async_notify_work);
	k_fifo_init(&bcb_coap_data.fifo);
	k_work_init(&bcb_coap_data.receive_work, bcb_coap_receive_work);

	return 0;
}
