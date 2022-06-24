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


def setup_logger(config):
    date_format = '%d-%m-%Y %H:%M:%S'
    log_format = '%(asctime)s %(levelname)-8s %(message)s'
    formatter = NewLineFormatter(log_format, datefmt=date_format)

    handler = logging.StreamHandler()
    handler.setFormatter(formatter)
    logger.addHandler(handler)
    logger.setLevel(config['log_level'])


class DataProtocol:

    class SampleBlock:
        def __init__(self):
            self.index = 0
            self.sample_interval = 0
            self.sample_time = 0
            self.channels = 0
            self.resolution = 0
            self.a_b_data = {}
            self.adc_data = {}

    class StatsItem:
        def __init__(self):
            self.blocks = 0
            self.samples = 0
            self.missed = False

    def __init__(self, config, csv_writer, loop):
        self.config = config
        self.loop = loop
        self.transport = None
        self.stats = {}
        self.prev_lines = 0
        self.csv_writer = csv_writer

    def connection_made(self, transport):
        self.transport = transport
        sock = transport.get_extra_info("socket")
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self._send_data_request()
        self._print_stats()

    def datagram_received(self, data, addr):
        sample_block = self._get_sample_block(data)
        ipaddr = addr[0]
        if ipaddr not in self.stats:
            item = DataProtocol.StatsItem()
            self.stats[ipaddr] = item
        else:
            item = self.stats[ipaddr]

        item.blocks += 1
        item.samples += len(sample_block.adc_data[0])

        self._write_sample_block(sample_block, ipaddr)

    def connection_lost(self, exc):
        pass

    def _send_data_request(self):
        logger.debug("Sending request")
        self.transport.sendto(bytearray([0x01]),
                              (self.config.get('remote_addr'),
                               self.config.get('remote_port')))
        self.loop.call_later(1, self._send_data_request)

    def _print_stats(self):
        for _ in range(self.prev_lines):
            sys.stdout.write('\033[A')
        sys.stdout.write('\r')
        for ipaddr, item in self.stats.items():
            sys.stdout.write('%s => %d\n' % (ipaddr, item.samples))

        self.prev_lines = len(self.stats)
        self.loop.call_later(0.1, self._print_stats)

    def _get_sample_block(self, data):
        sample_block = DataProtocol.SampleBlock()
        decoder = cbor2.CBORDecoder(io.BytesIO(data))
        cbor_data = decoder.decode()

        sample_block.index = cbor_data[0]
        sample_block.sample_interval = cbor_data[1]
        sample_block.sample_time = cbor_data[2]
        sample_block.channels = cbor_data[3]
        sample_block.resolution = cbor_data[4]
        bytes_per_sample = math.ceil(sample_block.resolution / 8)
        a_b_data_bytes = cbor_data[5]

        if len(a_b_data_bytes) % 4:
            logger.error("Invalid a and b values %d", len(a_b_data_bytes))
            return None

        for i in range(0, sample_block.channels):
            a_val = int(a_b_data_bytes[i*4]) + int(a_b_data_bytes[(i*4)+1] << 8)
            b_val = int(a_b_data_bytes[(i*4)+2]) + int(a_b_data_bytes[(i*4)+3] << 8)
            sample_block.a_b_data[i] = (a_val, b_val)

        sample_bytes = cbor_data[6]

        adc_data = {i: [] for i in range(0, sample_block.channels)}
        if len(sample_bytes) % bytes_per_sample:
            logger.error("Invalid samples: samples %d, bytes/sample %d",
                         len(sample_bytes), bytes_per_sample)
            return None

        for i in range(0, len(sample_bytes), bytes_per_sample * sample_block.channels):
            for c in range(0, sample_block.channels):
                value = 0
                for b in range(0, bytes_per_sample):
                    value += (sample_bytes[i + (c * bytes_per_sample) + b] << (8 * b))
                adc_data[c].append(value)

        sample_block.adc_data = adc_data

        return sample_block

    def _write_sample_block(self, sample_block, ipaddr):
        row_base = []
        row_base.append(ipaddr)
        row_base.append(sample_block.index)
        row_base.append(sample_block.sample_interval)
        row_base.append(sample_block.sample_time)
        row_base.append(sample_block.resolution)
        row_base.append(sample_block.channels)

        for i in range(0, sample_block.channels):
            row_base.append(sample_block.a_b_data[i][0])
            row_base.append(sample_block.a_b_data[i][1])

        for z in zip(*sample_block.adc_data.values()):
            row = []
            row.extend(row_base)
            for val in z:
                row.append(val)
            self.csv_writer.writerow(row)


async def main_coro(config, csv_writer, loop):
    _, _ = await loop.create_datagram_endpoint(
        lambda: DataProtocol(config, csv_writer, loop),
        local_addr=('0.0.0.0', 56789))

if __name__ == "__main__":

    def validate_log_level(arg):
        arg = str(arg).lower()
        level = {"info": logging.INFO, "warn": logging.WARN, "debug": logging.DEBUG,
                 "i": logging.INFO, "w": logging.WARN, "d": logging.DEBUG}
        if arg not in level:
            raise argparse.ArgumentTypeError("log level should be (i)nfo, (w)arn or (d)ebug")
        return level[arg]

    parser = argparse.ArgumentParser(description='ADC sample collection client for Blixt Zero')
    parser.add_argument('address', metavar='ADDRESS', type=str,
                        help='IP (or broadcast) address in IP:Port format')
    parser.add_argument('output', metavar='OUTPUT', type=str, help='Output CSV file')
    parser.add_argument('-l', '--log-level', type=validate_log_level, default="info",
                        help='Set log level: info, warn, debug')

    args = parser.parse_args()
    config = vars(args)

    url = urllib.parse.urlsplit('//' + config['address'])
    config['remote_addr'] = url.hostname
    config['remote_port'] = url.port

    csv_file = open(config['output'], 'w', encoding='utf-8')
    csv_writer = csv.writer(csv_file)

    loop = asyncio.new_event_loop()

    try:
        loop.run_until_complete(main_coro(config, csv_writer, loop))
        loop.run_forever()
    except KeyboardInterrupt as e:
        print("Stopping...")
    finally:
        loop.stop()

    csv_file.close()
