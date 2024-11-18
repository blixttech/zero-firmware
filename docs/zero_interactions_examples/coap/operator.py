from aiocoap import *
import logging
from zc_messages.zc_messages_pb2 import *

logging.basicConfig(level=logging.INFO)

class CoAP_Device:
    
    def __init__(self, addr, content_format=30001):
        '''Returns an instance of the CoAP Basic Parameters'''
        self.address = addr
        self.content_format = content_format

    async def zero_get(self, endpoint:str):
        '''
        Executes a GET request to the provided endpoint parameter,
        to the address acquired from the base instance
        '''
        logging.info(f"Atempting GET Request to coap://{self.address}/{endpoint}")
        protocol = await Context.create_client_context()

        status_request = Message(code=GET, uri =f"coap://{self.address}/{endpoint}")

        try:
            response = await protocol.request(status_request).response
        except Exception as e:
            return(f"Failed to fetch {endpoint} from device!")
        return response.payload

    async def zero_post(self, endpoint:str, protobuf_payload = ''):
        '''
        Executes a POST request to the provided endpoint parameter, sending the provided
        payload parameter, to the address acquired from the base instance.
        '''

        protocol = await Context.create_client_context()

        status_request = Message(code=POST, payload=protobuf_payload ,uri =f"coap://{self.address}/{endpoint}")
        status_request.opt.content_format = self.content_format

        try:
            response = await protocol.request(status_request).response
        except Exception as e:
            return(f"Failed to post {endpoint} to device!")
        return response.payload


    async def zero_get_config(self, content_format=30001):
        req_zc_msgs = {}
        res_zc_msgs = {}

        zc_req = ZCMessage()
        zc_req.req.set_config.config.curve.direction = ZCFlowDirection.ZC_FLOW_DIRECTION_FORWARD
        req_zc_msgs["curve"] = zc_req

        # zc_req = ZCMessage()
        # zc_req.req.get_config.csom.null = 0
        # req_zc_msgs["csom"] = zc_req

        # zc_req = ZCMessage()
        # zc_req.req.get_config.ocp_hw.null = 0
        # req_zc_msgs["ocp_hw"] = zc_req

        # zc_req = ZCMessage()
        # zc_req.req.get_config.ouvp.null = 0
        # req_zc_msgs["ouvp"] = zc_req

        # zc_req = ZCMessage()
        # zc_req.req.get_config.oufp.null = 0
        # req_zc_msgs["oufp"] = zc_req

        # zc_req = ZCMessage()
        # zc_req.req.get_config.calib.type = ZCCalibType.ZC_CALIB_TYPE_VOLTAGE_1
        # req_zc_msgs["calib_v1"] = zc_req

        # zc_req = ZCMessage()
        # zc_req.req.get_config.calib.type = ZCCalibType.ZC_CALIB_TYPE_VOLTAGE_2
        # req_zc_msgs["calib_v2"] = zc_req

        # zc_req = ZCMessage()
        # zc_req.req.get_config.calib.type = ZCCalibType.ZC_CALIB_TYPE_CURRENT_1
        # req_zc_msgs["calib_c1"] = zc_req

        # zc_req = ZCMessage()
        # zc_req.req.get_config.calib.type = ZCCalibType.ZC_CALIB_TYPE_CURRENT_2
        # req_zc_msgs["calib_c2"] = zc_req

        for key, zc_req in req_zc_msgs.items():
            coap_req = Message(code=POST, uri =f"coap://{self.address}/config")
            coap_req.opt.content_format = content_format

            print(zc_req)

            coap_req.payload = zc_req.SerializeToString()

            protocol = await Context.create_client_context()

            coap_res = await protocol.request(coap_req).response

            zc_res = ZCMessage()
            zc_res.ParseFromString(coap_res.payload)

            print(str(zc_res))

            if zc_req.WhichOneof("msg") != "res" and zc_res.res.WhichOneof("res") != "config":
                logging.debug("not a response %s", str(zc_res))
                continue

            res_zc_msgs[key] = zc_res

        return req_zc_msgs
