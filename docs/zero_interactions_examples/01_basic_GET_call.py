# Daniel ZDM | Blixt Tech | 24 Jan 2025
# Blixt Zero SG - Zero Example 01
# Basic Call to Zero Example

# Description:
# This example shows you how to make a basic interaction with the Switching Gear
# to an UNENCODED endpoint.

# Async library is needed since the CoAP libary that we're using works async.
import asyncio
import logging
import time

# Extra non-default libraries
import aiocoap

# We'll be logging whatever is happening on screen, so it makes things easier to follow
logging.basicConfig(level=logging.INFO)

# Here we're keeping things really simple, just add your zero ip (that can be found running example number 6)
zero_ip_addr = "0.0.0.0"

# Now let's go, we'll need the content format of the package
# 30001 is the default value for Zero.
# content_format = "30001" # content format is not necessary on GET requests.
target_endpoint = ".well-known/core"

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
        await CoAP_context.shutdown
    except:
        logging.warning(f"Failed to request data to {request_address}!")
    return response.payload

# With this we trigger the async function and wait for it to finish.
logging.info(f"Waiting for the async request to return...")
result = asyncio.run(zero_get_request(complete_address))

# Now, there you have it, let's print the results!
# Note also we're decoding it from a byte string, although not totally necessary in this case.
print(result.decode())

# Note that this is a very basic example of how to request information to the zero device,
# the endpoint ".well-known/core" replies unenconded text.
# The other possible requests, informed by the ".well-known/core" endpoint, are encoded because
# of performance purposes, Protobuf will be faster to send back and forth.

logging.info(f"Finished, end of program.")
