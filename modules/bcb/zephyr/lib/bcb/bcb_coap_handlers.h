#ifndef _BCB_COAP_HANDLERS_H_
#define _BCB_COAP_HANDLERS_H_

#include <net/coap.h>
#include <net/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BCB_COAP_RESOURCE_STATUS_PATH       ((const char * const[]) { "status", NULL })
#define BCB_COAP_RESOURCE_SWITCH_PATH       ((const char * const[]) { "switch", NULL })

int bcb_coap_handlers_wellknowncore_get(struct coap_resource *resource, 
                                        struct coap_packet *request, 
                                        struct sockaddr *addr, socklen_t addr_len);
int bcb_coap_handlers_status_get(struct coap_resource *resource, 
                                struct coap_packet *request, 
                                struct sockaddr *addr, socklen_t addr_len);
void bcb_coap_handlers_status_notify(struct coap_resource *resource, 
                                struct coap_observer *observer);
int bcb_coap_handlers_switch_put(struct coap_resource *resource, 
                                struct coap_packet *request, 
                                struct sockaddr *addr, socklen_t addr_len);

#ifdef __cplusplus
}
#endif

#endif // _BCB_COAP_HANDLERS_H_