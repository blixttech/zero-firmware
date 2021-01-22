#include "bcb_coap_handlers.h"
#include "bcb_coap_buffer.h"
#include <sys/slist.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <zephyr.h>
#include <init.h>
#include <net/socket.h>
#include <net/net_ip.h>
#include <net/udp.h>
#include <net/coap.h>
#include <net/coap_link_format.h>

#define LOG_LEVEL CONFIG_BCB_COAP_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_coap);

struct bcb_coap_periodic_notifier {
    sys_snode_t node;
    struct coap_resource *resource;
    struct coap_observer *observer;
    uint32_t seq;
    uint32_t period;
    uint32_t start;
    uint8_t used;
};

struct bcb_coap_data {
    int socket;
    uint8_t req_buf[CONFIG_BCB_COAP_MAX_MSG_LEN];
    uint8_t res_buf[CONFIG_BCB_COAP_MAX_MSG_LEN];
    struct coap_pending pendings[CONFIG_BCB_COAP_MAX_PENDING];
    struct coap_observer observers[CONFIG_BCB_COAP_MAX_OBSERVERS];
    struct bcb_coap_periodic_notifier notifiers[CONFIG_BCB_COAP_MAX_OBSERVERS];
    sys_slist_t notifier_list;
    struct k_delayed_work retransmit_work;
    struct k_delayed_work observer_work;
    struct k_fifo fifo;
    struct k_work receive_work;
    struct k_thread thread;
    K_THREAD_STACK_MEMBER(stack, CONFIG_BCB_COAP_THREAD_STACK_SIZE);
    struct coap_resource resources[];
};

static struct bcb_coap_data bcb_coap_data = {
    .resources = {
        {   .get = bcb_coap_handlers_wellknowncore_get,
            .path = COAP_WELL_KNOWN_CORE_PATH,
        },
        {   .get = bcb_coap_handlers_status_get,
            .notify = bcb_coap_handlers_status_notify,
            .path = BCB_COAP_RESOURCE_STATUS_PATH,
        },
        {   .get = bcb_coap_handlers_switch_put,
            .path = BCB_COAP_RESOURCE_SWITCH_PATH,
        },
        { },
    },
};

static void retransmit_work(struct k_work *work)
{
    struct coap_pending *pending;
    pending = coap_pending_next_to_expire(bcb_coap_data.pendings, CONFIG_BCB_COAP_MAX_PENDING);
    if (!pending) {
        /* Something went wrong. */
        return;
    }

    struct net_buf *buf = (struct net_buf *)pending->data;
    int r = sendto(bcb_coap_data.socket, buf->data, buf->len, 0, &pending->addr, 
                    sizeof(struct sockaddr));
    if (r < 0) {
        LOG_ERR("Failed to send %d", errno);
    }

    if (!coap_pending_cycle(pending)) {
        /* This is the last retransmission */
        bcb_coap_buf_free(buf);
        coap_pending_clear(pending);
    }

    pending = coap_pending_next_to_expire(bcb_coap_data.pendings, CONFIG_BCB_COAP_MAX_PENDING);
    if (pending) {
        k_delayed_work_submit(&bcb_coap_data.retransmit_work, K_MSEC(pending->timeout));
    }
}

static void periodic_notifier_schedule()
{
    if (sys_slist_is_empty(&bcb_coap_data.notifier_list)) {
        return;
    }

    sys_snode_t *node = sys_slist_peek_head(&bcb_coap_data.notifier_list);
    struct bcb_coap_periodic_notifier* notifier = (struct bcb_coap_periodic_notifier*) node;
    uint32_t t_d;
    uint32_t t_p;
    uint32_t t_now = k_uptime_get_32();

    /* Find how soon the delayed work has to be scheduled */
    t_d = notifier->start + notifier->period - t_now;
    t_p = notifier->period;
    SYS_SLIST_FOR_EACH_NODE(&bcb_coap_data.notifier_list, node) {
        notifier = (struct bcb_coap_periodic_notifier*) node;
        if (notifier->start + notifier->period - t_now < t_d) {
            t_d = notifier->start + notifier->period - t_now;
            t_p = notifier->period;   
        }
    }
    t_d = t_d ? t_d : t_p;
    k_delayed_work_submit(&bcb_coap_data.observer_work, K_MSEC(t_d));
}


static void observer_notify_work(struct k_work *work)
{
    sys_snode_t *node;
    struct bcb_coap_periodic_notifier* notifier;
    SYS_SLIST_FOR_EACH_NODE(&bcb_coap_data.notifier_list, node) {
        notifier = (struct bcb_coap_periodic_notifier*) node;
        uint32_t t_now = k_uptime_get_32();
        if (t_now - notifier->start >= notifier->period) {
            if(notifier->resource->notify) {
                notifier->seq++;
                notifier->start = t_now;
                notifier->resource->notify(notifier->resource, notifier->observer);
            }
        }
    }
    periodic_notifier_schedule();
}

static int create_pending_request(struct coap_packet *packet, const struct sockaddr *addr)
{
    struct coap_pending *pending;
    int r;

    pending = coap_pending_next_unused(bcb_coap_data.pendings, CONFIG_BCB_COAP_MAX_PENDING);
    if (!pending) {
        return -ENOMEM;
    }

    r = coap_pending_init(pending, packet, addr);
    if (r < 0) {
        return -EINVAL;
    }

    struct net_buf *buf = bcb_coap_buf_alloc();
    if (!buf) {
        return -ENOMEM;
    }
    net_buf_add_mem(buf, packet->data, packet->offset);
    /* We need the net_buf pointer later to free up. */
    pending->data = (uint8_t *)buf;

    pending = coap_pending_next_to_expire(bcb_coap_data.pendings, CONFIG_BCB_COAP_MAX_PENDING);
    if (!pending) {
        /* Something went wrong. We just added one */
        return 0;
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
            LOG_WRN("Unable to create pending");
        }
    }

    r = sendto(bcb_coap_data.socket, packet->data, packet->offset, 0, addr, sizeof(struct sockaddr));
    if (r < 0) {
        LOG_ERR("Failed to send %d", errno);
    }

    return r;
}

uint32_t bcb_coap_get_observer_sequence(struct coap_resource *resource, 
                                        struct coap_observer *observer)
{
    sys_snode_t *node;
    struct bcb_coap_periodic_notifier* notifier;
    SYS_SLIST_FOR_EACH_NODE(&bcb_coap_data.notifier_list, node) {
        notifier = (struct bcb_coap_periodic_notifier*) node;
        if (notifier->resource == resource && notifier->observer == observer) {
            return notifier->seq;
        }
    }

    return 0;
}

uint8_t* bcb_coap_response_buffer()
{
    return bcb_coap_data.res_buf;
}

static struct bcb_coap_periodic_notifier* periodic_notifier_new()
{
    int i;
    for (i = 0; i < CONFIG_BCB_COAP_MAX_OBSERVERS; i++) {
        if (!bcb_coap_data.notifiers[i].used) {
            return &bcb_coap_data.notifiers[i];
        }
    }
    
    return NULL;
}

static struct bcb_coap_periodic_notifier* periodic_notifier_find(struct coap_resource *resource, 
                                                        struct coap_observer* observer)
{
    int i;
    for (i = 0; i < CONFIG_BCB_COAP_MAX_OBSERVERS; i++) {
        if (bcb_coap_data.notifiers[i].resource == resource && 
            bcb_coap_data.notifiers[i].observer == observer) {
            return &bcb_coap_data.notifiers[i];
        }
    }

    return NULL;
}

static struct bcb_coap_periodic_notifier* periodic_notifier_add(struct coap_resource *resource, 
                                                                struct coap_observer* observer,
                                                                uint32_t period)
{
    if (!period) {
        return NULL;
    }

    struct bcb_coap_periodic_notifier* notifier = periodic_notifier_new();
    if (notifier) {
        sys_slist_append(&bcb_coap_data.notifier_list, &notifier->node);
        notifier->used = true;
        notifier->observer = observer;
        notifier->resource = resource;
        notifier->period = period;
        notifier->seq = 0;
        notifier->start = k_uptime_get_32();
    }
    return notifier;
}

static void periodic_notifier_remove(struct coap_resource *resource, struct coap_observer* observer)
{
    struct bcb_coap_periodic_notifier* notifier = periodic_notifier_find(resource, observer);
    if (notifier) {
        sys_slist_find_and_remove(&bcb_coap_data.notifier_list, &notifier->node);
        notifier->used = false;
    }
}

static bool bcb_coap_sockaddr_equal(const struct sockaddr *a, const struct sockaddr *b)
{
    /* FIXME: Should we consider ipv6-mapped ipv4 addresses as equal to
     * ipv4 addresses?
     */
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

int bcb_coap_register_observer(struct coap_resource *resource, 
                                struct coap_packet *request, 
                                struct sockaddr *addr,
                                uint32_t period)
{
    period = period < CONFIG_BCB_COAP_MIN_OBSERVE_PERIOD ? 
                        CONFIG_BCB_COAP_MIN_OBSERVE_PERIOD : period;
    if (!resource->notify) {
        LOG_WRN("Notification not implemented");
        return -EINVAL;
    }

    struct coap_observer* observer;
    observer = coap_observer_next_unused(bcb_coap_data.observers, CONFIG_BCB_COAP_MAX_OBSERVERS);
    if (!observer) {
        LOG_WRN("Cannot find unused observer");
        return -ENOMEM;
    }

    coap_observer_init(observer, request, addr);
    coap_register_observer(resource, observer);

    struct bcb_coap_periodic_notifier* notifier = 
                                        periodic_notifier_add(resource, observer, period);
    if (notifier) {
        periodic_notifier_schedule();
    }

    return 0;   
}

int bcb_coap_deregister_observer(struct sockaddr *addr, uint8_t *token, uint8_t tkl) 
{
    sys_snode_t *node;
    SYS_SLIST_FOR_EACH_NODE(&bcb_coap_data.notifier_list, node) {
        struct bcb_coap_periodic_notifier* notifier = (struct bcb_coap_periodic_notifier*)node;
        if (bcb_coap_sockaddr_equal(addr, &notifier->observer->addr) && 
            !memcmp(token, notifier->observer->token, tkl)) {
            periodic_notifier_remove(notifier->resource, notifier->observer);
            coap_remove_observer(notifier->resource, notifier->observer);
            memset(notifier->observer, 0, sizeof(struct coap_observer));
            periodic_notifier_schedule();
            return 0;
        }
    }

    return -ENOENT;
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
        LOG_ERR("Invalid data received %d", r);
        return -EINVAL;
    }

    type = coap_header_get_type(&packet);

    if (type == COAP_TYPE_ACK) {
        struct coap_pending *pending;
        pending = coap_pending_received(&packet, bcb_coap_data.pendings, CONFIG_BCB_COAP_MAX_PENDING);
        if(!pending) {
            LOG_WRN("Recevied ACK, but no pending");
            return -EINVAL;
        }
        /* pending->data has been replaced with the pointer to net_buf  */
        bcb_coap_buf_free((struct net_buf *)pending->data);
        coap_pending_clear(pending);
        return 0;

    } else if (type == COAP_TYPE_RESET) {
        uint8_t token[8];
        uint8_t token_len;
        token_len = coap_header_get_token(&packet, token);
        r = bcb_coap_deregister_observer(client_addr, token, token_len);
        if (r < 0) {
            LOG_WRN("Cannot deregister observer");
        }
        return r;

    } else {
        r = coap_handle_request(&packet, bcb_coap_data.resources, options, opt_num, 
                                client_addr, client_addr_len);
        if (r < 0) {
            LOG_ERR("Request handdling error %d", r);
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
        int received;
        received = recvfrom(bcb_coap_data.socket, bcb_coap_data.req_buf,
                            CONFIG_BCB_COAP_MAX_MSG_LEN, 0, &addr, &addr_len);
        if (received < 0) {
            LOG_ERR("Reception error %d", errno);
            continue;
        }

        struct net_buf *buf;
        struct sockaddr *buf_ud;

        buf = bcb_coap_buf_alloc();
        if (!buf) {
            LOG_ERR("Cannot allocate buffer");
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
    bcb_coap_data.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (bcb_coap_data.socket < 0) {
        LOG_ERR("Failed to create UDP socket %d", errno);
        return -errno;
    }

    int r;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CONFIG_BCB_COAP_SERVER_PORT);

    r = bind(bcb_coap_data.socket, (struct sockaddr *)&addr, sizeof(addr));
    if (r < 0) {
        LOG_ERR("Failed to bind UDP socket %d", errno);
        return -errno;
    }

    k_thread_create(&bcb_coap_data.thread, bcb_coap_data.stack, 
                    K_THREAD_STACK_SIZEOF(bcb_coap_data.stack), 
                    bcb_coap_receive_thread, NULL, NULL, NULL, 
                    CONFIG_BCB_COAP_THREAD_PRIORITY, 0, K_FOREVER);
    k_thread_name_set(&bcb_coap_data.thread, "coap");
    k_thread_start(&bcb_coap_data.thread);

	return 0;
}

static int bcb_coap_init()
{
    sys_slist_init(&bcb_coap_data.notifier_list);
    k_delayed_work_init(&bcb_coap_data.retransmit_work, retransmit_work);
    k_delayed_work_init(&bcb_coap_data.observer_work, observer_notify_work);
    k_fifo_init(&bcb_coap_data.fifo);
    k_work_init(&bcb_coap_data.receive_work, bcb_coap_receive_work);

    return 0;
}

SYS_INIT(bcb_coap_init, APPLICATION, CONFIG_BCB_LIB_COAP_INIT_PRIORITY);