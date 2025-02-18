# Daniel ZDM | Blixt Tech | 14 Nov 2024
# Blixt Zero SG - Manual Request Tool

import sys
import asyncio

from common.coap.operator import CoAP_Device
from common.zc_messages.zc_messages_pb2 import *

# Makes a new instance of the Switching Gear Device
# change this address for the address of the device you wish to interact with!
device_addr = "0.0.0.0"
zeroSG = CoAP_Device(device_addr)


def call_get(zero:CoAP_Device, endpoint):
    '''
    This method only calls endpoints that returns unencoded byte string messages.
    '''
    return asyncio.run(zero.zero_get(endpoint))


def call_get_encoded(zero:CoAP_Device, endpoint):
    '''
    This method call endpoints that returns Protobuf encoded messages. 
    '''
    payload = asyncio.run(zero.zero_get(endpoint))

    # 1 - Here a new instance of a Protobuf Message is created
    zero_data = ZCMessage()

    # 2 - Then its parsed from a byte of strings, giving the payload as argument to fill in the data
    zero_data.ParseFromString(payload)

    return zero_data

def call_get_configs(zero:CoAP_Device, _):

    payload = asyncio.run(zero.zero_get_config())

    return payload

def call_toggle_zero(zero:CoAP_Device, _):
    '''
    This method call the 'device' endpoint and toggles the Swithing Gear. 
    '''

    # 1 - Here a new instance of a Protobuf Message is created
    zero_data = ZCMessage()

    # 2 - Create the Protobuf Package
    zero_data.req.cmd.cmd = ZCDeviceCmd.ZC_DEVICE_CMD_TOGGLE

    zero_payload = zero_data.SerializeToString()

    status_msg = asyncio.run(zero.zero_post(endpoint='device', protobuf_payload=zero_payload))

    zero_response = ZCMessage()
    zero_response.ParseFromString(status_msg)

    return(zero_response.ListFields())

def clear_scr():
    ''' Clears the terminal Screen '''
    sys.stdout.write("\033[2J\033[H")
    sys.stdout.flush()


# Here you havea list of each endpoint and if it must be unencoded or not.
call_options = [
    [call_get,".well-known/core"], 
    [call_get_encoded,"version"],
    [call_get_encoded,"status"],
    [call_toggle_zero,"device"],
    [call_get_configs,"config"],
    ]


def default_welcome():
    print("\n-- Welcome to the Blixt Zero CoAP Manual Request Tool. --\n")
    print("The intent of this tool is to assist you to learn how to \n\
    make calls to the device using CoAP\n")

    print(f"Target device address is: {zeroSG}\n")

    input("press enter to proceed...")

    clear_scr()

    print('''
    Choose a option using numbers to call a GET endpoint:
    0 - GET  Well Known
    1 - GET  Version
    2 - GET  Status
    3 - POST Toggle
    4 - GET  Config
    ''')
    user_opt = int(input("Option: "))
    
    clear_scr()

    result = call_options[user_opt][0](zeroSG, call_options[user_opt][1])
    print("Response: \n")

    if user_opt == 3:
        print(result)
        return

    if user_opt == 4:
        print(result)
        return

    if user_opt > 0:
        # 3 - The fields can be listed after decoded

        # This example shows how to extract information of a certain field.
        if user_opt == 1:
            res = [f.name for f in result.DESCRIPTOR.fields]
            print(result.res.version.uuid.hex())
            return
        print(result.ListFields())
        return
    print(result)

# Check if this is the main script and run
if __name__ == "__main__":
    clear_scr()
    default_welcome()
