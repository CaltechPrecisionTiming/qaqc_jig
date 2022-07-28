import csv
import matplotlib.pyplot as plt
import numpy as np
# from argparse import ArgumentParser
# 
# parser = ArgumentParser()
# parser.add_argument('--sodium', default=False, action='store_true', help='flag to indicate data is from a sodium source')
# parser.add_argument('--laser', default=False, action='store_true', help='flag to indicate data is from a laser source')
# parser.add_argument('--plot', default=False, action='store_true', help='plot the waveforms and charge integral')
# parser.add_argument('--chunks', default=10000, type=int, help='number of waveforms to process at a time')
# parser.add_argument('--pdf', default=False, action='store_true', help='prints pdfs')
# parser.add_argument('--IT', default=300, type=float, help='SPE integration length')
# parser.add_argument('--s', default=50, type=float, help='start time of the SPE integration')
# parser.add_argument('--active', default=None, help='Only take data from a single channel. If not specified, all channels are analyzed.')
# 
# args = parser.parse_args()

try:
    with open('Fit_Data.csv', 'r') as file:
        reader = csv.DictReader(file)
        SCOPE_dark = []
        SCOPE_dark_195 = []
        SCOPE_laser = []
        dark = [[] for _ in range(16)]
        dark_195 = [[] for _ in range(16)]
        laser = [[] for _ in range(16)]
        for row in reader:
            val = float(row['SPE Charge'])
            if row['Bias'] != '45':
                continue
            if row['Source'] == 'SCOPE':
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
                elif row['Extra'].startswith('DARK_195'):
                    dark_195[idx].append(val)
                elif row['Extra'].startswith('DARK'):
                    dark[idx].append(val)
        dark = np.mean(dark, axis=-1)
        dark_195 = np.mean(dark_195, axis=-1)
        laser = np.mean(laser, axis=-1)

        print(f'SCOPE laser SPE: {np.mean(SCOPE_laser)}')
        print(f'SCOPE dark 100 SPE: {np.mean(SCOPE_dark)}')
        print(f'SCOPE dark 195 SPE: {np.mean(SCOPE_dark_195)}')

        plt.figure()
        plt.plot(np.arange(16), laser, label='Laser 100ns')
        plt.plot(np.arange(16), dark, label='Dark 100ns')
        plt.plot(np.arange(16), dark_195, label='Dark 195ns')
        plt.suptitle('SPE Charge Data over each CAEN Channel with 45V SiPM Bias')
        plt.xlabel("Channel")
        plt.ylabel("Charge (pC)")
        plt.legend(loc='center left', bbox_to_anchor=(1, 0.5))
        plt.subplots_adjust(right=0.75)
        plt.show()
        input()
except FileNotFoundError:
    print('No data')
    exit()
                

