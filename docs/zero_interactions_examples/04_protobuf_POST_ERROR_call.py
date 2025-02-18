# Daniel ZDM | Blixt Tech | 24 Jan 2025
# Blixt Zero SG - Zero Example 01
# Basic Call to Zero Example

import asyncio
import logging
import time

import aiocoap
from common.zc_messages.zc_messages_pb2 import *
from common.request_zero import zero_post_request

logging.basicConfig(level=logging.INFO)

zero_ip_addr = "0.0.0.0"

content_format = "30001"
target_endpoint = "device"

# To form the complete address, we'll format it in just one string here...
complete_address = f"coap://{zero_ip_addr}/{target_endpoint}"

logging.info(f"Preparing the POST request package to {complete_address}")


# Here we create the payload before sending it. The possible payloads are already
# determined by the ProtocolBuffer library.
device_cmd = ZCDeviceCmd.ZC_DEVICE_CMD_TOGGLE
request_cmd = ZCRequestDeviceCmd(cmd=device_cmd)
request = ZCRequest(cmd=request_cmd)
content_format = "201" # This is wrong on purpose!

print(f"\n---[ Example of Misconfigured Content Format ]---\n")

print("The first error message is due to: Wrong Content Format Code")

print(f"The content format was purposefully set to {content_format} when it should be 30001.\n")
input("Press enter to see the result...\n")

# With this we trigger the async function and wait for it to finish.
logging.info(f"Waiting for the async request to return...")

# So now we will be sending the payload we created and getting the response
result = asyncio.run(zero_post_request(complete_address, content_format, request))

logging.info(f"Decoding the message received...\n")
zero_data = ZCMessage()

# Now we populate it, decoding the payload received
zero_data.ParseFromString(result)

# Finally, let's print what we got
print("Message: \n")
print(zero_data.ListFields())
print()
# The available methods can be found on the Protobuf documentation from google, mentioned on
# the coap_api_spec.md in this repository, inside the docs directory. 

print("\n You may find multiple different error codes for other issues and mistakes.")

print(f"\n---[ End of Misconfigured Content Format Example ]---\n")
logging.info(f"End of Example\n")
