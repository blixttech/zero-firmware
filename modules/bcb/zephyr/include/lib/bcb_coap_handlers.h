#ifndef _BCB_COAP_HANDLERS_H_
#define _BCB_COAP_HANDLERS_H_

#include <net/coap.h>
#include <net/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
#define BCB_COAP_RESOURCE_VERSION_PATH			((const char *const[]){ "version", NULL })
#define BCB_COAP_RESOURCE_VERSION_ATTRIBUTES		((const char *const[]){ "ct=30001", NULL })
#define BCB_COAP_RESOURCE_STATUS_PATH			((const char *const[]){ "status", NULL })
#define BCB_COAP_RESOURCE_STATUS_ATTRIBUTES		((const char *const[]){ "obs", "ct=30001", NULL })
#define BCB_COAP_RESOURCE_CONFIG_PATH			((const char *const[]){ "config", NULL })
#define BCB_COAP_RESOURCE_CONFIG_ATTRIBUTES		((const char *const[]){ "ct=30001", NULL })
#define BCB_COAP_RESOURCE_DEVICE_PATH			((const char *const[]){ "device", NULL })
#define BCB_COAP_RESOURCE_DEVICE_ATTRIBUTES		((const char *const[]){ "ct=30001", NULL })
#if 0
#define BCB_COAP_RESOURCE_SWITCH_PATH			((const char *const[]){ "switch", NULL })
#define BCB_COAP_RESOURCE_SWITCH_ATTRIBUTES		((const char *const[]){ NULL })
#define BCB_COAP_RESOURCE_TC_DEF_PATH			((const char *const[]){ "tc_def", NULL })
#define BCB_COAP_RESOURCE_TC_DEF_ATTRIBUTES		((const char *const[]){ NULL })
#define BCB_COAP_RESOURCE_TC_PATH			((const char *const[]){ "tc", NULL })
#define BCB_COAP_RESOURCE_TC_ATTRIBUTES			((const char *const[]){ NULL })
#endif
// clang-format on

int bcb_coap_handlers_wellknowncore_get(struct coap_resource *resource, struct coap_packet *request,
					struct sockaddr *addr, socklen_t addr_len);
int bcb_coap_handlers_version_get(struct coap_resource *resource, struct coap_packet *request,
				  struct sockaddr *addr, socklen_t addr_len);
int bcb_coap_handlers_status_get(struct coap_resource *resource, struct coap_packet *request,
				 struct sockaddr *addr, socklen_t addr_len);
int bcb_coap_handlers_status_post(struct coap_resource *resource, struct coap_packet *request,
				 struct sockaddr *addr, socklen_t addr_len);
void bcb_coap_handlers_status_notify(struct coap_resource *resource,
				     struct coap_observer *observer);
int bcb_coap_handlers_config_post(struct coap_resource *resource, struct coap_packet *request,
				  struct sockaddr *addr, socklen_t addr_len);
int bcb_coap_handlers_device_post(struct coap_resource *resource, struct coap_packet *request,
				  struct sockaddr *addr, socklen_t addr_len);
#if 0
int bcb_coap_handlers_switch_get(struct coap_resource *resource, struct coap_packet *request,
				 struct sockaddr *addr, socklen_t addr_len);
int bcb_coap_handlers_switch_post(struct coap_resource *resource, struct coap_packet *request,
				  struct sockaddr *addr, socklen_t addr_len);
int bcb_coap_handlers_tc_def_get(struct coap_resource *resource, struct coap_packet *request,
				 struct sockaddr *addr, socklen_t addr_len);
int bcb_coap_handlers_tc_def_post(struct coap_resource *resource, struct coap_packet *request,
				  struct sockaddr *addr, socklen_t addr_len);
int bcb_coap_handlers_tc_get(struct coap_resource *resource, struct coap_packet *request,
			     struct sockaddr *addr, socklen_t addr_len);
int bcb_coap_handlers_tc_post(struct coap_resource *resource, struct coap_packet *request,
			      struct sockaddr *addr, socklen_t addr_len);
#endif
int bcb_coap_handlers_init(void);

#ifdef __cplusplus
}
#endif

#endif // _BCB_COAP_HANDLERS_H_