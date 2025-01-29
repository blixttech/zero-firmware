# Daniel ZDM | Blixt Tech | 24 Jan 2025
# Blixt Zero SG - Zero Example 01
# Basic Call to Zero Example

# Description:
# This example shows you how to issue a command to the Switching Gear
# using an Protobuf ENCODED endpoint and shows how to encode it.

# Note that the POST request is a bit different, we have to first assemble
# the payload, then we encode it, finally add to the request package, then send it.

# Async library is needed since the CoAP libary that we're using works async.
import asyncio
import logging
import time

# Extra non-default libraries
import aiocoap

# Import the previously compiled protobuf libraries
from common.zc_messages.zc_messages_pb2 import *

# We'll be logging whatever is happening on screen, so it makes things easier to follow
logging.basicConfig(level=logging.INFO)

# Here we're keeping things really simple, asking the user to insert the
# Blixt Zero device IP, so its easier to follow along.
default_ip = "192.168.1.81"
zero_ip_addr = input(f"Insert the target Zero IP (or press enter for default {default_ip}): ")

if not zero_ip_addr:
    zero_ip_addr = default_ip
logging.info(f"Selected Target IP: {zero_ip_addr}")

# Sleep, just for the sake of going slower
time.sleep(1)

# Now let's go, we'll need the content format of the package
# 30001 is the default value for Zero.
content_format = "30001" # content format is not used on GET requests.
target_endpoint = "device"

# To form the complete address, we'll format it in just one string here...
complete_address = f"coap://{zero_ip_addr}/{target_endpoint}"

logging.info(f"Preparing the POST request package to {complete_address}")

async def zero_post_request(request_address, cf, payload):
    '''
    We made an async function here, because the aiocoap library runs async,
    so we need to keep things in async context.
    - First Arg is the complete address of the request
    - Second Arg is the content format
    - Third Arg is the Payload
    '''

    # 1 - Here a new instance of a Protobuf Message is created
    zero_data = ZCMessage()

    # 2 - Create the Protobuf Package
    zero_data.req.cmd.cmd = payload

    # 3 - Let's serialize that to send to the device
    zero_payload = zero_data.SerializeToString()

    # 4 - Make the POST request

    CoAP_Context = await aiocoap.Context.create_client_context()

    post_request = aiocoap.Message(code=aiocoap.POST, payload=zero_payload ,uri =request_address)
    post_request.opt.content_format = cf

    try:
        response = await CoAP_Context.request(post_request).response
    except Exception as e:
        return(f"Failed to post {request_address} to device!")
    return response.payload


# Here we create the payload before sending it. The possible payloads are already
# determined by the ProtocolBuffer library.
request_payload = ZCDeviceCmd.ZC_DEVICE_CMD_TOGGLE

input("Press enter now, and you should see your zero toggling state...")

# With this we trigger the async function and wait for it to finish.
logging.info(f"Waiting for the async request to return...")

# So now we will be sending the payload we created and getting the response
result = asyncio.run(zero_post_request(complete_address, content_format, request_payload))

# Let's print it and you can see how it looks encoded
print("Encoded Package (not decoded yet):")
print(result)
print()

# Now let's decode it using the protobuf library
# Lets create an instance of an empty message of this Protobuf schema
logging.info(f"Decoding the message received...")
zero_data = ZCMessage()

# Now we populate it, decoding the payload received
zero_data.ParseFromString(result)

# Finally, let's print what we got
input("Press enter, and the message received from the Zero device will be printed...")
print(zero_data.ListFields())
# The available methods can be found on the Protobuf documentation from google, mentioned on
# the coap_api_spec.md in this repository, inside the docs directory. 

logging.info(f"Finished, end of program.")
