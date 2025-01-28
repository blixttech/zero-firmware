import logging
import argparse
import asyncio
from device_manager import *

logger = logging.getLogger(__name__)

def device_updated(device):
    #print(device)
    pass

async def create_device_manager(config):
    dev_man = DeviceManager(config, device_updated)
    await dev_man.init()
    return dev_man


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Zero Controller CLI")
    parser.add_argument("-d", "--debug", action="store_true", default=False, help="Debug output")
    parser.add_argument(
        "-n", "--nic-scan-interval", default=10, help="NIC scan interval in seconds"
    )
    parser.add_argument(
        "-s", "--device-scan-interval", default=1, help="Device scan interval in seconds"
    )

    args = parser.parse_args()
    config = vars(args)

    if config["debug"] is True:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.INFO)

    loop = asyncio.new_event_loop()

    try:
        dev_man = loop.run_until_complete(create_device_manager(config))
        loop.run_forever()
    except KeyboardInterrupt as e:
        print("Stopping...")

    loop.run_until_complete(dev_man.shutdown())
    loop.run_until_complete(asyncio.sleep(1))

    for task in asyncio.all_tasks(loop):
        task.cancel()
        loop.run_until_complete(task)
        task.exception()

    loop.stop()
