# Daniel ZDM | Blixt Tech | 24 Jan 2025
# Blixt Zero SG - Zero Example 01
# Basic Call to Zero Example

# Description:
# This example shows you how to make a query to the Switching Gear
# using an Protobuf ENCODED endpoint and shows how to decode it.

# Note that we make the request the same way we do with the unencoded one,
# but we have now to decode the response.
# The request doesn't need to be encoded because we're not sending any data
# that will be processed by the device.

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

# Here we're keeping things really simple, just add your zero ip (that can be found running example number 6)
zero_ip_addr = "0.0.0.0"

# Now let's go, we'll need the content format of the package
# 30001 is the default value for Zero.
# content_format = "30001" # content format is not necessary on GET requests.
target_endpoint = "status"

# To form the complete address, we'll format it in just one string here...
complete_address = f"coap://{zero_ip_addr}/{target_endpoint}"

logging.info(f"Preparing the GET request package to {complete_address}")

async def zero_get_request(request_address):
    '''
    We made an async function here, because the aiocoap library runs async,
    so we need to keep things in async context.
    - First Arg is the complete address of the request
    - Second Arg is the content format
    '''

    # We need a message context here
    CoAP_context = await aiocoap.Context.create_client_context()

    # Then assemble the actual request using the library
    zero_request = aiocoap.Message(code=aiocoap.GET, uri=request_address)

    # Now let's send the actual request...
    try:
        logging.info(f"Atempting GET Request {request_address}")
        time.sleep(1)
        response = await CoAP_context.request(zero_request).response
    except:
        logging.warning(f"Failed to request data to {request_address}!")
    return response.payload

# With this we trigger the async function and wait for it to finish.
logging.info(f"Waiting for the async request to return...")

# So we get the payload value for now
payload = asyncio.run(zero_get_request(complete_address))

# Let's print it and you can see how it looks encoded
print("Encoded Package (not decoded yet):")
print(payload)
print()

# Now let's decode it using the protobuf library
# Lets create an instance of an empty message of this Protobuf schema
logging.info(f"Decoding the message received...")
zero_data = ZCMessage()

# Now we populate it, decoding the payload received
zero_data.ParseFromString(payload)

# Finally, let's print what we got
input("Press enter, and the message received from the Zero device will be printed...")
print(zero_data.ListFields())
# The available methods can be found on the Protobuf documentation from google, mentioned on
# the coap_api_spec.md in this repository, inside the docs directory. 

logging.info(f"Finished, end of program.")
