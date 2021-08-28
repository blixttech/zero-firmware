#!/usr/bin/env python3

import sys
import socket
import math
import logging
import argparse
import urllib.parse
import csv
import cbor2

logger = logging.getLogger(__name__)


class BytesIOSocket(object):
    def __init__(self, socket):
        self._socket = socket

    def read(self, size):
        return self._socket.recv(size)


def get_next_sample_block(decoder):
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


if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='ADC sample collection client for Blixt Zero')
    parser.add_argument('address', metavar='ADDRESS', type=str, help='IP:Port')
    parser.add_argument('output', metavar='OUTPUT', type=str, help='Output CSV file')
    parser.add_argument('-s', '--samples', type=int, default=1000, help='Number of samples')

    args = parser.parse_args()
    config = vars(args)

    url = urllib.parse.urlsplit('//' + config['address'])

    socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    socket.connect((url.hostname, url.port))

    logger.info("Connected to %s:%d" % (url.hostname, url.port))

    cbor_decoder = cbor2.CBORDecoder(BytesIOSocket(socket))
    last_block_idx = 0
    sample_blocks = []
    sample_len = 0
    block_missed = False

    while sample_len < config['samples']:
        sample_block = get_next_sample_block(cbor_decoder)
        socket.send(bytearray([0x00]))
        if (last_block_idx != 0 and (sample_block['index'] - last_block_idx) != 1):
            block_missed = True
            logger.warning("sample block missed")

        last_block_idx = sample_block['index']

        adc_data_len = len(sample_block['adc_data'][0])
        sample_blocks.append(sample_block)
        sample_len += adc_data_len

        sys.stdout.write("\rsamples %s" %  ("%d" % sample_len).ljust(10))

    sys.stdout.write("\rsamples %s\n" %  ("%d" % sample_len).ljust(10))
    socket.close()

    csv_file = open(config['output'], 'w')
    csv_writer = csv.writer(csv_file)

    csv_writer.writerow(['sample_interval', sample_blocks[0]['sample_interval']])
    csv_writer.writerow(['sample_time', sample_blocks[0]['sample_time']])
    csv_writer.writerow(['sample_resolution', sample_blocks[0]['sample_resolution']])
    csv_writer.writerow(['sample_missed', block_missed])
    csv_writer.writerow(['channels', sample_blocks[0]['channels']])
    csv_writer.writerow(['sample_len', sample_len])

    last_block_idx = 0
    sample_idx = 0
    for block in sample_blocks:
        adc_data = block['adc_data']

        idx_diff = sample_block['index'] - last_block_idx
        if last_block_idx != 0 and idx_diff > 1:
            sample_idx += (idx_diff * len(adc_data[0]))

        last_block_idx = sample_block['index']

        for z in zip(*adc_data.values()):
            csv_writer.writerow((sample_idx,) + z)
            sample_idx += 1

    csv_file.close()
