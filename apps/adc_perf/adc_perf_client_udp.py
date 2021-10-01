#!/usr/bin/env python3

import sys
import socket
import math
import logging
import argparse
import urllib.parse
import csv
import io
import asyncio
import cbor2
import re

logger = logging.getLogger(__name__)


class NewLineFormatter(logging.Formatter):
    def __init__(self, fmt, datefmt=None):
        logging.Formatter.__init__(self, fmt, datefmt)

    def escape_ansi(self, line):
        ansi_escape = re.compile(r'(?:\x1B[@-_]|[\x80-\x9F])[0-?]*[ -/]*[@-~]')
        return ansi_escape.sub('', line)

    def format(self, record):
        msg = logging.Formatter.format(self, record)
        if record.message != "":
            parts = msg.split(record.message)
            msg = msg.replace('\n', '\n' + parts[0])
        msg = self.escape_ansi(msg)
        return msg


def get_sample_block(decoder):
    cbor_data = decoder.decode()

    sample_index = cbor_data[0]
    sample_interval = cbor_data[1]
    sample_time = cbor_data[2]
    channels = cbor_data[3]
    sample_resolution = cbor_data[4]
    bytes_per_sample = math.ceil(sample_resolution / 8)
    sample_bytes = cbor_data[5]

    adc_data = {i: [] for i in range(0, channels)}
    if (len(sample_bytes) % bytes_per_sample):
        logger.error("Invalid samples: samples %d, bytes/sample %d" %
                     (len(sample_bytes), bytes_per_sample))
        return None

    for i in range(0, len(sample_bytes), bytes_per_sample * channels):
        for c in range(0, channels):
            value = 0
            for b in range(0, bytes_per_sample):
                value += (sample_bytes[i + (c * bytes_per_sample) + b] << (8 * b))
            adc_data[c].append(value)

    sample_data = {}
    sample_data['index'] = sample_index
    sample_data['sample_interval'] = sample_interval
    sample_data['sample_time'] = sample_time
    sample_data['channels'] = channels
    sample_data['sample_resolution'] = sample_resolution
    sample_data['adc_data'] = adc_data

    return sample_data


class DataProtocol:
    def __init__(self, config, on_completed, loop):
        self.config = config
        self.on_completed = on_completed
        self.loop = loop
        self.sample_blocks = []
        self.sample_missed = False
        self.sample_len = 0
        self.transport = None

    def connection_made(self, transport):
        self.transport = transport
        sock = transport.get_extra_info("socket")
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._send_data_request()

    def _send_data_request(self):
        logger.debug("Sending request")
        self.transport.sendto(bytearray([0x01]))
        self.loop.call_later(1, self._send_data_request)

    def datagram_received(self, data, addr):
        cbor_decoder = cbor2.CBORDecoder(io.BytesIO(data))
        sample_block = get_sample_block(cbor_decoder)

        missed = (len(self.sample_blocks) > 0
                  and (sample_block['index'] - self.sample_blocks[-1]['index']) != 1)
        if missed:
            logger.warning("sample block(s) missed")
            self.sample_missed = True

        self.sample_blocks.append(sample_block)
        self.sample_len += len(sample_block['adc_data'][0])

        sys.stdout.write("\rsamples %s" % ("%d" % self.sample_len).ljust(10))

        if self.sample_len > self.config['samples']:
            sys.stdout.write("\r")
            logger.info("Total samples %d", self.sample_len)
            self._completed()

    def _completed(self):
        info = {'sample_missed': self.sample_missed,
                'sample_len': self.sample_len,
                'sample_blocks': self.sample_blocks}
        self.on_completed.set_result(info)

    def connection_lost(self, exc):
        pass


async def _receive_data_coro(config):
    loop = asyncio.get_event_loop()
    on_completed = loop.create_future()

    transport, _ = await loop.create_datagram_endpoint(
        lambda: DataProtocol(config, on_completed, loop),
        remote_addr=(config['remote_addr'], config['remote_port']),
        local_addr=('0.0.0.0', 56789))

    result = await on_completed
    transport.close()

    return result


def receive_data(config):
    return asyncio.run(_receive_data_coro(config))


def setup_logger(config):
    date_format = '%d-%m-%Y %H:%M:%S'
    log_format = '%(asctime)s %(levelname)-8s %(message)s'
    formatter = NewLineFormatter(log_format, datefmt=date_format)

    handler = logging.StreamHandler()
    handler.setFormatter(formatter)
    logger.addHandler(handler)
    logger.setLevel(config['log_level'])


if __name__ == "__main__":

    def validate_log_level(arg):
        arg = str(arg).lower()
        level = {"info": logging.INFO, "warn": logging.WARN, "debug": logging.DEBUG,
                 "i": logging.INFO, "w": logging.WARN, "d": logging.DEBUG}
        if arg not in level:
            raise argparse.ArgumentTypeError("log level should be (i)nfo, (w)arn or (d)ebug")
        return level[arg]

    parser = argparse.ArgumentParser(description='ADC sample collection client for Blixt Zero')
    parser.add_argument('address', metavar='ADDRESS', type=str, help='IP:Port')
    parser.add_argument('output', metavar='OUTPUT', type=str, help='Output CSV file')
    parser.add_argument('-s', '--samples', type=int, default=1000, help='Number of samples')
    parser.add_argument('-t', '--timeout', type=int, default=5, help='Timeout for receiving in seconds')
    parser.add_argument('-l', '--log-level', type=validate_log_level, default="info",
                        help='Set log level: info, warn, debug')

    args = parser.parse_args()
    config = vars(args)

    url = urllib.parse.urlsplit('//' + config['address'])
    config['remote_addr'] = url.hostname
    config['remote_port'] = url.port

    setup_logger(config)
    logger.info("Connecting to %s:%d" % (url.hostname, url.port))

    sample_data = receive_data(config)
    sample_blocks = sample_data['sample_blocks']

    if len(sample_blocks) == 0:
        logger.warn("No sampled received from %s" % str(config['address']))
        sys.exit(0)

    csv_file = open(config['output'], 'w')
    csv_writer = csv.writer(csv_file)

    csv_writer.writerow(['sample_interval', sample_blocks[0]['sample_interval']])
    csv_writer.writerow(['sample_time', sample_blocks[0]['sample_time']])
    csv_writer.writerow(['sample_resolution', sample_blocks[0]['sample_resolution']])
    csv_writer.writerow(['sample_missed', sample_data['sample_missed']])
    csv_writer.writerow(['channels', sample_blocks[0]['channels']])
    csv_writer.writerow(['sample_len', sample_data['sample_len']])

    last_block_idx = 0
    sample_idx = 0
    for block in sample_blocks:
        adc_data = block['adc_data']

        idx_diff = block['index'] - last_block_idx
        if last_block_idx != 0 and idx_diff > 1:
            sample_idx += (idx_diff * len(adc_data[0]))

        last_block_idx = block['index']

        for z in zip(*adc_data.values()):
            csv_writer.writerow((sample_idx,) + z)
            sample_idx += 1

    csv_file.close()
