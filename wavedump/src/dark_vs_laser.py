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
        dark_100 = [[] for _ in range(16)]
        dark_200 = [[] for _ in range(16)]
        dark_300 = [[] for _ in range(16)]
        laser = [[] for _ in range(16)]
        for row in reader:
            if not 'vino_cutoff' in row['Extra']:
                continue
            
            if float(row['Bias']) != 45:
                continue
            
            val = float(row['SPE Charge'])
            if row['Source'] == 'SCOPE':
                continue
                if row['Extra'].startswith('LASER'):
                    SCOPE_laser.append(val) 
                elif row['Extra'].startswith('DARK_195'):
                    SCOPE_dark_195.append(val)
                elif row['Extra'].startswith('DARK'):
                    SCOPE_dark.append(val)
            else:
                idx = int(row['Channel'][2:])
                if row['Extra'].startswith('LASER'):
                    laser[idx].append(val) 
                elif row['Extra'].startswith('DARK_100'):
                    dark_100[idx].append(val)
                elif row['Extra'].startswith('DARK_200'):
                    dark_200[idx].append(val)
                elif row['Extra'].startswith('DARK_300'):
                    dark_300[idx].append(val)
        dark_100 = np.mean(dark_100, axis=-1)
        dark_200 = np.mean(dark_200, axis=-1)
        dark_300 = np.mean(dark_300, axis=-1)
        laser = np.mean(laser, axis=-1)

        la = np.mean(laser)
        d200a = np.mean(dark_200)
        
        print(f'Laser average: {la}')
        print(f'Dark 200 average: {d200a}')
        print(f'Dark 200 is {((d200a - la)/la) * 100}% off from laser')

        plt.figure()
        plt.plot(np.arange(16), laser, label='Laser 100ns')
        plt.plot(np.arange(16), dark_100, label='Dark 100ns')
        plt.plot(np.arange(16), dark_200, label='Dark 200ns')
        plt.plot(np.arange(16), dark_300, label='Dark 300ns')
        plt.suptitle('SPE Charge Data over each CAEN Channel with 45V SiPM Bias')
        plt.xlabel("Channel")
        plt.ylabel("Charge (pC)")
        plt.legend(loc='center left', bbox_to_anchor=(1, 0.5))
        plt.subplots_adjust(right=0.75)
        plt.show()
except FileNotFoundError:
    print(f"Couldn't open file {args.data_file}")
    sys.exit(1)

