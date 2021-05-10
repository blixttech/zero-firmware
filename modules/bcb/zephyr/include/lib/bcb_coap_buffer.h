#ifndef _BCB_COAP_BUFFER_H_
#define _BCB_COAP_BUFFER_H_

#include <net/buf.h>

struct net_buf *bcb_coap_buf_alloc();
void bcb_coap_buf_free(struct net_buf *buf);

#endif // _BCB_COAP_BUFFER_H_