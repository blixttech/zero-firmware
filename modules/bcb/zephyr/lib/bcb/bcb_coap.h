#ifndef _BCB_COAP_H_
#define _BCB_COAP_H_

#include <net/coap.h>
#include <sys/_stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int bcb_coap_start_server();
int bcb_coap_process();

uint16_t bcb_coap_next_message_id();
struct coap_observer* bcb_coap_register_observer(struct coap_resource *resource, 
                                                struct coap_packet *request, 
                                                struct sockaddr *addr,
                                                uint32_t period);
int bcb_coap_deregister_observer(struct coap_resource *resource, 
                                struct sockaddr *addr, uint8_t *token, uint8_t tkl);
int bcb_coap_send_response(struct coap_packet *packet, const struct sockaddr *addr, socklen_t addr_len);

#ifdef __cplusplus
}
#endif

#endif // _BCB_COAP_H_