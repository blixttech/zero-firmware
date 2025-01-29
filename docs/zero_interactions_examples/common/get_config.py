from common.zc_messages.zc_messages_pb2 import *
from aiocoap import *

NANOPB_CONTENT_FORMAT = 30001

async def fetch_config(self):
    req_zc_msgs = {}
    res_zc_msgs = {}

    zc_req = ZCMessage()
    zc_req.req.get_config.curve.direction = ZCFlowDirection.ZC_FLOW_DIRECTION_BACKWARD
    req_zc_msgs["curve"] = zc_req

    zc_req = ZCMessage()
    zc_req.req.get_config.csom.null = 0
    req_zc_msgs["csom"] = zc_req

    zc_req = ZCMessage()
    zc_req.req.get_config.ocp_hw.null = 0
    req_zc_msgs["ocp_hw"] = zc_req

    zc_req = ZCMessage()
    zc_req.req.get_config.ouvp.null = 0
    req_zc_msgs["ouvp"] = zc_req

    zc_req = ZCMessage()
    zc_req.req.get_config.oufp.null = 0
    req_zc_msgs["oufp"] = zc_req

    zc_req = ZCMessage()
    zc_req.req.get_config.calib.type = ZCCalibType.ZC_CALIB_TYPE_VOLTAGE_1
    req_zc_msgs["calib_v1"] = zc_req

    zc_req = ZCMessage()
    zc_req.req.get_config.calib.type = ZCCalibType.ZC_CALIB_TYPE_VOLTAGE_2
    req_zc_msgs["calib_v2"] = zc_req

    zc_req = ZCMessage()
    zc_req.req.get_config.calib.type = ZCCalibType.ZC_CALIB_TYPE_CURRENT_1
    req_zc_msgs["calib_c1"] = zc_req

    zc_req = ZCMessage()
    zc_req.req.get_config.calib.type = ZCCalibType.ZC_CALIB_TYPE_CURRENT_2
    req_zc_msgs["calib_c2"] = zc_req

    for key, zc_req in req_zc_msgs.items():
        coap_req = Message(code=POST, uri=self._coap_urls["config"])
        coap_req.opt.content_format = NANOPB_CONTENT_FORMAT
        coap_req.payload = zc_req.SerializeToString()

        coap_res = await self._coap_ctx.request(coap_req).response

        zc_res = ZCMessage()
        zc_res.ParseFromString(coap_res.payload)

        print(str(zc_res))

        if zc_req.WhichOneof("msg") != "res" and zc_res.res.WhichOneof("res") != "config":
            self._logger.debug("not a response %s", str(zc_res))
            continue

        res_zc_msgs[key] = zc_res

    return req_zc_msgs