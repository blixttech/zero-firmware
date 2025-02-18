# Daniel ZDM | Blixt Tech | 17 Feb 2025
# Blixt Zero SG - Zero Example 01
# Basic Call to Zero Example

# Description:
# This script shows you how to configure a tripping curve using python

import asyncio
import logging
import time

import aiocoap
from common.zc_messages.zc_messages_pb2 import *
from common.request_zero import zero_post_request

logging.basicConfig(level=logging.INFO)
zero_ip_addr = "0.0.0.0"

target_endpoint = "config"
content_format = "30001"

complete_address = f"coap://{zero_ip_addr}/{target_endpoint}"

logging.info(f"Preparing POST request to {complete_address}")

# Set each point of the curve
point1 = ZCCurvePoint(limit=2000, duration=3600000)
point2 = ZCCurvePoint(limit=3000, duration=1800000)
point3 = ZCCurvePoint(limit=4000, duration=600000)
point4 = ZCCurvePoint(limit=10000, duration=100)

# Create a Curve Config isntance
curve_config = ZCCurveConfig()

# Wrap the curve points into the curve config
curve_config.points.extend([point1, point2, point3, point4])
# Add for which direction your configuration is valid for
curve_config.direction = ZCFlowDirection.ZC_FLOW_DIRECTION_FORWARD

print(curve_config.ListFields())

csom_config = ZCCsomConfig()
csom_config.enabled = False

zc_config = ZCConfig()
# zc_config.csom.CopyFrom(csom_config)
zc_config.curve.CopyFrom(curve_config)

set_config_req = ZCRequestSetConfig()
set_config_req.config.CopyFrom(zc_config)

zc_request = ZCRequest()
zc_request.set_config.CopyFrom(set_config_req)

logging.info(f"Waiting for the async request to return...")

result = asyncio.run(zero_post_request(complete_address, content_format, zc_request))

logging.info(f"Decoding the message received...")
zero_data = ZCMessage()

zero_data.ParseFromString(result)

print(zero_data.ListFields())

logging.info(f"Finished, end of program.")
