#ifndef _BCB_COAP_H_
#define _BCB_COAP_H_

#include <stdint.h>
#include <net/coap.h>
#include <net/buf.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bcb_coap_notifier {
	struct coap_resource *resource;
	struct coap_observer *observer;
	uint32_t seq;
	uint32_t period;
	uint32_t start;
	uint8_t msgs_no_ack;
	bool is_used;
};

int bcb_coap_init(void);
int bcb_coap_start_server();
int bcb_coap_notifier_add(struct coap_resource *resource, struct coap_packet *request,
			  struct sockaddr *addr, uint32_t period);
int bcb_coap_notifier_remove(const struct sockaddr *addr, const uint8_t *token, uint8_t tkl);
int bcb_coap_send_response(struct coap_packet *packet, const struct sockaddr *addr);
struct bcb_coap_notifier *bcb_coap_get_notifier(struct coap_resource *resource,
						struct coap_observer *observer);
bool bcb_coap_has_pending(const struct sockaddr *addr);
uint8_t *bcb_coap_response_buffer();

#ifdef __cplusplus
}
#endif

#endif // _BCB_COAP_H_