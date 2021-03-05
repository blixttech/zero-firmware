#ifndef _BCB_COAP_HANDLERS_H_
#define _BCB_COAP_HANDLERS_H_

#include <net/coap.h>
#include <net/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BCB_COAP_RESOURCE_VERSION_PATH		((const char *const[]){ "version", NULL })
#define BCB_COAP_RESOURCE_VERSION_ATTRIBUTES	((const char *const[]){ NULL })
#define BCB_COAP_RESOURCE_STATUS_PATH		((const char *const[]){ "status", NULL })
#define BCB_COAP_RESOURCE_STATUS_ATTRIBUTES	((const char *const[]){ "obs", NULL })
#define BCB_COAP_RESOURCE_SWITCH_PATH		((const char *const[]){ "switch", NULL })
#define BCB_COAP_RESOURCE_SWITCH_ATTRIBUTES	((const char *const[]){ NULL })
#define BCB_COAP_RESOURCE_TC_DEFAULT_PATH		((const char *const[]){ "tc_default", NULL })
#define BCB_COAP_RESOURCE_TC_DEFAULT_ATTRIBUTES	((const char *const[]){ NULL })

int bcb_coap_handlers_wellknowncore_get(struct coap_resource *resource, struct coap_packet *request,
					struct sockaddr *addr, socklen_t addr_len);
int bcb_coap_handlers_version_get(struct coap_resource *resource, struct coap_packet *request,
				  struct sockaddr *addr, socklen_t addr_len);
int bcb_coap_handlers_status_get(struct coap_resource *resource, struct coap_packet *request,
				 struct sockaddr *addr, socklen_t addr_len);
void bcb_coap_handlers_status_notify(struct coap_resource *resource,
				     struct coap_observer *observer);
int bcb_coap_handlers_switch_get(struct coap_resource *resource, struct coap_packet *request,
				 struct sockaddr *addr, socklen_t addr_len);
int bcb_coap_handlers_switch_post(struct coap_resource *resource, struct coap_packet *request,
				  struct sockaddr *addr, socklen_t addr_len);
int bcb_coap_handlers_tc_default_get(struct coap_resource *resource, struct coap_packet *request,
				     struct sockaddr *addr, socklen_t addr_len);
int bcb_coap_handlers_tc_default_post(struct coap_resource *resource, struct coap_packet *request,
				      struct sockaddr *addr, socklen_t addr_len);
int bcb_coap_handlers_init(void);

#ifdef __cplusplus
}
#endif

#endif // _BCB_COAP_HANDLERS_H_