import argparse
import csv
import numpy
from numpy.lib.function_base import average

def load_csv_file(file_name, header_size=6):
    csv_file = open(config['input'])
    csv_reader = csv.reader(csv_file)

    sample_data = {}
    for row in [next(csv_reader) for i in range(0, header_size)]:
        sample_data[row[0]] = row[1]

    sample_data['sample_interval'] = int(sample_data['sample_interval'])
    sample_data['sample_time'] = int(sample_data['sample_time'])
    sample_data['channels'] = int(sample_data['channels'])
    sample_data['sample_resolution'] = int(sample_data['sample_resolution'])
    sample_data['sample_missed'] = bool(sample_data['sample_missed'])
    sample_data['sample_len'] = int(sample_data['sample_len'])

    adc_data = {i: [] for i in range(0, sample_data['channels']) }
    for row in csv_reader:
        for c in range(0, sample_data['channels']):
            adc_data[c].append(int(row[1 + c]))

    csv_file.close()

    sample_data['adc_data'] = adc_data

    return sample_data


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='ADC sample analysis tool for Blixt Zero')
    parser.add_argument('input', metavar='INPUT', type=str, help='Output CSV file')

    args = parser.parse_args()
    config = vars(args)

    sample_data = load_csv_file(config['input'])

    for c in range(0, sample_data['channels']):
        adc_data = sample_data['adc_data'][c]
        avg = numpy.mean(adc_data)
        std = numpy.std(adc_data)
        ptp = numpy.ptp(adc_data)
        print(c, avg, std, ptp)

    

