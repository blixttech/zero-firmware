# Daniel ZDM | Blixt Tech | 27 Jan 2025
# Blixt Zero SG - Manual Discovery Tool
# Made from Zero CLI tool, originally created by Kasun.
# Note: This is not well optimized and is meant to be used just
# as example, inspiration and base for you application.

import logging
import asyncio
import aiocoap
import psutil
import ipaddress
import socket
from common.zc_messages.zc_messages_pb2 import *

NANOPB_CONTENT_FORMAT = 30001

class DiscoveryProtocol:
    '''
    Protocol Implementation with base protocol functions, you can read more here:
    https://docs.python.org/3/library/asyncio-protocol.html#base-protocol
    '''
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

                if snicaddr.address.startswith("127"):
                    continue

                net = ipaddress.IPv4Network(snicaddr.address + "/" + snicaddr.netmask, False)
                addrs.append(net.broadcast_address)
        self.addrs = addrs

        loop = asyncio.get_running_loop()
        loop.call_later(self.config.get("nic_scan_interval", 10), self._update_broadcast_addresses4)


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
        self._callback and self._callback(device)

    def _zero_found(self, addr):
        if addr not in self.devices:
            task = asyncio.create_task(
                Device.create(self._config, addr, self._coap_ctx, self._callback)
            )
            task.add_done_callback(self._device_created)

    async def _start_discovery(self):
        loop = asyncio.get_running_loop()
        # If you want to learn more how it works, check out here:
        # https://docs.python.org/3/library/asyncio-eventloop.html#opening-network-connections
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


class Device:
    @classmethod
    async def create(cls, config, addr, coap_ctx, callback):
        self = Device()
        self._logger = logging.getLogger(self.__class__.__name__)
        self._config = config
        self.addr = addr
        self._coap_ctx = coap_ctx
        self._callback = callback
        self.device_info = await self._fetch_info()

        if self.device_info:
            print(f"Device found: {self.device_info['ip']}, UUID: {self.device_info.get('uuid', 'Unknown')}")
        
        return self

    async def _fetch_info(self):
        # Fetch Device Information from Zero's version endpoint
        version_endpoint = f"coap://{self.addr[0]}/version"
        payload = await self.zero_get_request(version_endpoint)
        zero_data = ZCMessage()
        zero_data.ParseFromString(payload)
        uuid = zero_data.res.version.uuid.hex()

        return {"ip": self.addr[0], "uuid": uuid}
    
    async def zero_get_request(self, request_address):
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
            logging.debug(f"Atempting GET Request {request_address}")
            response = await CoAP_context.request(zero_request).response
            CoAP_context.shutdown()
        except:
            logging.warning(f"Failed to request data to {request_address}!")
        return response.payload

    def stop_observing(self):
        pass


async def main():

    # This function we're setting to be called on later by the Device Manager,
    # it will be given as argument to it, sio it can be called back later.
    def device_found(device):
        print(f"Found device at IP: {device.device_info['ip']} with UUID: {device.device_info.get('uuid', 'Unknown')}")

    config = {"device_scan_interval": 1, "nic_scan_interval": 10}

    # Create Device Manager and start discovery process
    device_manager = DeviceManager(config, device_found)
    await device_manager.init()

    # Run discovery for a while, while it is "sleeping",
    # the previous async tast will be running.
    await asyncio.sleep(3) 

    # Its important to shutdown the things we've started,
    # so it gets garbage collected. 
    await device_manager.shutdown()

    # Let's print what we found...
    # It would be nice to create a better repport here.
    print("\nAll devices found: ")
    for device, _ in device_manager.devices.items():
        print(f"On IP: {device[0]}")


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    asyncio.run(main())
