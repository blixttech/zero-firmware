import logging
import asyncio
import aiocoap
import psutil
import ipaddress
import socket
from zc_messages.zc_messages_pb2 import *

NANOPB_CONTENT_FORMAT = 30001


class DiscoveryProtocol:
    def __init__(self, config, callback):
        self.logger = logging.getLogger(self.__class__.__name__)
        self.config = config
        self.callback = callback
        self.transport = None
        self.message_id = 0
        self.addrs = []

    def connection_made(self, transport):
        self.transport = transport
        self._update_broadcast_addresses4()
        self._send_discovery_message()

    def connection_lost(self, transport):
        pass

    def datagram_received(self, data, addr):
        resources = {}
        try:
            msg = aiocoap.Message.decode(rawdata=data, remote=addr)
            if msg.opt.content_format != 40:
                self.logger.warning("only core link-format supported")
                return

            resources = self._get_resources(msg.payload)
            self.logger.debug("resources: %s", str(resources))

        except Exception as ex:
            self.logger.debug("decode error: %s", ex)

        if resources.get("/version", {}).get("content-format", 0) != NANOPB_CONTENT_FORMAT:
            self.logger.debug("device not supported: %s", resources)
            return

        self.callback and self.callback(addr)

    def _get_resources(self, payload):
        resources = {}
        for params in payload.decode(encoding="utf-8").split(","):
            resource = {}
            for param in params.split(";"):
                if param.startswith("<"):
                    resource["path"] = param[1:-1]
                elif param.startswith("title="):
                    resource["title"] = param[6:].strip()
                elif param.startswith("rt="):
                    resource["type"] = param[3:].strip()
                elif param.startswith("if="):
                    resource["interface"] = param[3:].strip()
                elif param.startswith("sz="):
                    resource["max-size"] = int(param[3:].strip())
                elif param.startswith("ct="):
                    resource["content-format"] = int(param[3:].strip())
                elif param.startswith("obs"):
                    resource["observable"] = True
                else:
                    pass

            if resource.get("path", None):
                resources[resource["path"]] = resource

        return resources

    def _send_discovery_message(self):
        if self.transport.is_closing():
            return

        for addr in self.addrs:
            url = f"coap://{addr}/.well-known/core"
            msg = aiocoap.Message(code=aiocoap.GET, mtype=aiocoap.NON, mid=self.message_id, uri=url)
            self.transport.sendto(msg.encode(), (str(addr), 5683))

        self.message_id += 1
        loop = asyncio.get_running_loop()
        loop.call_later(self.config.get("device_scan_interval", 1), self._send_discovery_message)

    def _update_broadcast_addresses4(self):
        if self.transport.is_closing():
            return

        addrs = []
        for _, snicaddrs in psutil.net_if_addrs().items():
            if len(snicaddrs) == 0:
                continue

            for snicaddr in snicaddrs:
                if snicaddr.family is not socket.AddressFamily.AF_INET:
                    continue

                # Filter local host addresses
                if snicaddr.address.startswith("127"):
                    continue

                net = ipaddress.IPv4Network(snicaddr.address + "/" + snicaddr.netmask, False)
                addrs.append(net.broadcast_address)
        self.addrs = addrs

        loop = asyncio.get_running_loop()
        loop.call_later(self.config.get("nic_scan_interval", 10), self._update_broadcast_addresses4)


class Device:
    @classmethod
    async def create(cls, config, addr, coap_ctx, callback):
        self = Device()
        self._logger = logging.getLogger(self.__class__.__name__)
        self._config = config
        self.addr = addr
        self._coap_ctx = coap_ctx
        self._callback = callback
        self._status_req = None
        self._coap_urls = {
            "version": f"coap://{self.addr[0]}:{self.addr[1]}/version",
            "status": f"coap://{self.addr[0]}:{self.addr[1]}/status",
            "config": f"coap://{self.addr[0]}:{self.addr[1]}/config",
            "device": f"coap://{self.addr[0]}:{self.addr[1]}/device",
        }
        self.device_info = await self._fetch_info()
        await self._fetch_config()

        return self

    def start_observing(self):
        zc_msg = ZCMessage()
        zc_msg.req.set_config.config.notif.interval = self._config.get(
            "notification_interval", 1000
        )

        req_msg = aiocoap.Message(code=aiocoap.POST, uri=self._coap_urls["status"])
        req_msg.opt.observe = 0
        req_msg.opt.content_format = NANOPB_CONTENT_FORMAT
        req_msg.payload = zc_msg.SerializeToString()

        self._status_req = self._coap_ctx.request(req_msg)
        self._status_req.observation.register_callback(self._status_observed)

    def stop_observing(self):
        if not self._status_req:
            return

        if not self._status_req.observation.cancelled:
            self._status_req.observation.cancel()

        req_msg = aiocoap.Message(
            code=aiocoap.GET, mtype=aiocoap.RST, uri=self._coap_urls["status"]
        )
        self._coap_ctx.request(req_msg, handle_blockwise=False)

    def _status_observed(self, coap_msg):
        zc_msg = ZCMessage()
        zc_msg.ParseFromString(coap_msg.payload)
        print(zc_msg)
        print("THIS WAS THE STATUS!")
        print(type(zc_msg))
        self._callback and self._callback(self)

    async def _fetch_info(self):
        req_msg = aiocoap.Message(code=aiocoap.GET, uri=self._coap_urls["version"])
        res_msg = await self._coap_ctx.request(req_msg).response

        if res_msg.opt.content_format != NANOPB_CONTENT_FORMAT:
            self._logger.debug("unsupported content format: %d", res_msg.opt.content_format)
            return None

        zc_msg = ZCMessage()
        zc_msg.ParseFromString(res_msg.payload)

        if zc_msg.WhichOneof("msg") != "res" and zc_msg.res.WhichOneof("res") != "version":
            self._logger.debug("not a response %s", str(zc_msg))
            return None

        device_info = {}
        device_info["ip"] = self.addr[0]
        device_info["uuid"] = zc_msg.res.version.uuid.hex()

        if zc_msg.res.version.HasField("sw_ver"):
            device_info["sw_ver"] = zc_msg.res.version.sw_ver

        if zc_msg.res.version.HasField("hw_ver"):
            device_info["hw_ver"] = zc_msg.res.version.hw_ver

        if zc_msg.res.version.HasField("link_addr"):
            device_info["link_addr"] = zc_msg.res.version.link_addr.hex()

        if zc_msg.res.version.HasField("sec_en"):
            device_info["sec_en"] = zc_msg.res.version.sec_en

        return device_info

    async def _fetch_config(self):
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
            coap_req = aiocoap.Message(code=aiocoap.POST, uri=self._coap_urls["config"])
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

        return None

    def _device_cmd(self, cmd):
        zc_msg = ZCMessage()
        zc_msg.req.cmd.cmd = cmd

        req_msg = aiocoap.Message(code=aiocoap.POST, uri=self._coap_urls["device"])
        req_msg.opt.content_format = NANOPB_CONTENT_FORMAT
        req_msg.payload = zc_msg.SerializeToString()

        self._coap_ctx.request(req_msg)

    def open(self):
        self._device_cmd(ZCDeviceCmd.ZC_DEVICE_CMD_OPEN)

    def close(self):
        self._device_cmd(ZCDeviceCmd.ZC_DEVICE_CMD_CLOSE)

    def toggle(self):
        self._device_cmd(ZCDeviceCmd.ZC_DEVICE_CMD_TOGGLE)


class DeviceManager:
    def __init__(self, config, callback):
        self._logger = logging.getLogger(self.__class__.__name__)
        self._config = config
        self._callback = callback
        self._discovery_transport = None
        self._coap_ctx = None
        self.devices = {}

    def _device_created(self, result):
        if result.cancelled():
            return

        device = result.result()
        if not device:
            return

        self.devices[device.addr] = device
        device.start_observing()
        self._callback and self._callback(device)

    def _zero_found(self, addr):
        if addr not in self.devices:
            task = asyncio.create_task(
                Device.create(self._config, addr, self._coap_ctx, self._callback)
            )
            task.add_done_callback(self._device_created)

    async def _start_discovery(self):
        loop = asyncio.get_running_loop()
        self._discovery_transport, _ = await loop.create_datagram_endpoint(
            lambda: DiscoveryProtocol(self._config, self._zero_found),
            local_addr=("0.0.0.0", ""),
            allow_broadcast=True,
        )

    async def init(self):
        self._coap_ctx = await aiocoap.Context.create_client_context()
        await self._start_discovery()

    async def shutdown(self):
        self._discovery_transport.close()
        for _, device in self.devices.items():
            device.stop_observing()
