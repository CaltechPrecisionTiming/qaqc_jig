"""
Compares SPE charge measurements taken with the laser to those taken with dark counts with different integration times.
Note: to select which series of data to view, the file must be modified manually.
When writing to the data file (via `save_charge_data.py`), make sure to include keywords in the 'Extra' column so that you can filter out the trials you want to compare.

Author: Kai Svenson
Date: Aug. 4, 2022
"""

import csv
import matplotlib.pyplot as plt
import numpy as np
import sys
from argparse import ArgumentParser
 
parser = ArgumentParser()
parser.add_argument('-d', '--data-file', default='Fit_Data.csv', help='csv file to read SPE data from')
args = parser.parse_args()

try:
    with open(args.data_file, 'r') as file:
        reader = csv.DictReader(file)
        data = [[[] for _ in range(16)] for _ in range(4)]
        data_err = [[[] for _ in range(16)] for _ in range(4)]
        for row in reader:
            if not 'vino_cutoff' in row['Extra']:
                continue
            if float(row['Bias']) != 45:
                continue
            if row['Source'] == 'SCOPE':
                continue
            val = float(row['SPE Charge'])
            val_err = float(row['SPE Charge Err Abs'])            
            idx = int(row['Channel'][2:])
            if row['Extra'].startswith('LASER'):
                data[0][idx].append(val) 
                data_err[0][idx].append(val_err) 
            elif row['Extra'].startswith('DARK_100'):
                data[1][idx].append(val) 
                data_err[1][idx].append(val_err) 
            elif row['Extra'].startswith('DARK_200'):
                data[2][idx].append(val) 
                data_err[2][idx].append(val_err) 
            elif row['Extra'].startswith('DARK_300'):
                data[3][idx].append(val) 
                data_err[3][idx].append(val_err) 

        data = np.mean(data, axis=-1)
        data_err = np.mean(data_err, axis=-1)
        la = np.mean(data[0])
        d200a = np.mean(data[2])
        
        print(f'Laser average: {la}')
        print(f'Dark 200 average: {d200a}')
        print(f'Dark 200 is {((d200a - la)/la) * 100}% off from laser')

        plt.figure()
        plt.errorbar(np.arange(16), data[0], yerr=data_err[0], capsize=2, label='Laser 100ns')
        plt.errorbar(np.arange(16), data[1], yerr=data_err[1], capsize=2, label='Dark 100ns')
        plt.errorbar(np.arange(16), data[2], yerr=data_err[2], capsize=2, label='Dark 200ns')
        plt.errorbar(np.arange(16), data[3], yerr=data_err[3], capsize=2, label='Dark 300ns')
        plt.suptitle('SPE Charge Data over each CAEN Channel with 45V SiPM Bias')
        plt.xlabel("Channel")
        plt.ylabel("Charge (pC)")
        plt.legend(loc='center left', bbox_to_anchor=(1, 0.5))
        plt.subplots_adjust(right=0.75)
        plt.show()
except FileNotFoundError:
    print(f"Couldn't open file {args.data_file}")
    sys.exit(1)

