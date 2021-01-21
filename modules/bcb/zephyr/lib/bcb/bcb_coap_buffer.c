#include "bcb_coap_buffer.h"

NET_BUF_POOL_DEFINE(bcb_coap_buffer_pool, CONFIG_BCB_COAP_MAX_BUF_COUNT, 
                    CONFIG_BCB_COAP_MAX_MSG_LEN, CONFIG_BCB_COAP_MAX_USER_DATA_SIZE, NULL);

struct net_buf* bcb_coap_buf_alloc()
{
    return net_buf_alloc(&bcb_coap_buffer_pool, K_NO_WAIT);
}

void bcb_coap_buf_free(struct net_buf* buf)
{
    net_buf_unref(buf);
}