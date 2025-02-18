
import logging
import time

import aiocoap
from common.zc_messages.zc_messages_pb2 import *

logging.basicConfig(level=logging.INFO)

async def zero_get_request(request_address):
    '''
    We made an async function here, because the aiocoap library runs async,
    so we need to keep things in async context.
    - First Arg is the complete address of the request
    - Second Arg is the content format
    '''
    CoAP_context = await aiocoap.Context.create_client_context()
    zero_request = aiocoap.Message(code=aiocoap.GET, uri=request_address)
    try:
        logging.info(f"Atempting GET Request {request_address}")
        time.sleep(1)
        response = await CoAP_context.request(zero_request).response
    except:
        logging.warning(f"Failed to request data to {request_address}!")
    return response.payload

async def zero_post_request(request_address, cf, payload):
    '''
    We made an async function here, because the aiocoap library runs async,
    so we need to keep things in async context.
    - First Arg is the complete address of the request
    - Second Arg is the content format
    - Third Arg is the Payload
    '''
    zero_data = ZCMessage()
    zero_data.req.CopyFrom(payload)
    zero_payload = zero_data.SerializeToString()
    CoAP_Context = await aiocoap.Context.create_client_context()

    post_request = aiocoap.Message(code=aiocoap.POST, payload=zero_payload ,uri =request_address)
    post_request.opt.content_format = cf

    try:
        response = await CoAP_Context.request(post_request).response
    except Exception as e:
        return(f"Failed to post {request_address} to device!")
    return response.payload

