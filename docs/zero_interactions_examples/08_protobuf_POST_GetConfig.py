# Daniel ZDM | Blixt Tech | 17 Feb 2025
# Blixt Zero SG - Zero Example 01
# Basic Call to Zero Example

# Description:

import asyncio
import logging
import time

import aiocoap
from common.zc_messages.zc_messages_pb2 import *

logging.basicConfig(level=logging.INFO)
zero_ip_addr = "0.0.0.0"

content_format = "30001"
target_endpoint = "config"

complete_address = f"coap://{zero_ip_addr}/{target_endpoint}"

logging.info(f"Preparing POST request to {complete_address}")

async def zero_post_request(request_address, cf, payload):
    '''
    We made an async function here, because the aiocoap library runs async,
    so we need to keep things in async context.
    - First Arg is the complete address of the request
    - Second Arg is the content format
    - Third Arg is the Payload
    '''

    zero_data = ZCMessage(req=payload)

    zero_payload = zero_data.SerializeToString()


    CoAP_Context = await aiocoap.Context.create_client_context()

    post_request = aiocoap.Message(code=aiocoap.POST, payload=zero_payload ,uri =request_address)
    post_request.opt.content_format = cf

    try:
        response = await CoAP_Context.request(post_request).response
    except Exception as e:
        return(f"Failed to post {request_address} to device!")
    return response.payload


get_config_request = ZCRequestGetConfig()
get_config_request.curve.direction = ZCFlowDirection.ZC_FLOW_DIRECTION_FORWARD  # Specify direction if needed

zc_request = ZCRequest()
zc_request.get_config.CopyFrom(get_config_request)


logging.info(f"Waiting for the async request to return...")


result = asyncio.run(zero_post_request(complete_address, content_format, zc_request))

logging.info(f"Decoding the message received...")
zero_data = ZCMessage()

zero_data.ParseFromString(result)


print(zero_data.ListFields())


logging.info(f"Finished, end of program.")
