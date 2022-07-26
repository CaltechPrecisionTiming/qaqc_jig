import csv
import matplotlib.pyplot as plt
import numpy as np

try:
    with open('Fit_Data.csv', 'r') as file:
        reader = csv.DictReader(file)
        dark = [[] for _ in range(16)]
        dark_195 = [[] for _ in range(16)]
        laser = [[] for _ in range(16)]
        for row in reader:
            if row['Source'] == 'SCOPE':
                continue
            idx = int(row['Channel'][2:])
            val = float(row['SPE Charge'])
            if row['Extra'].startswith('LASER'):
                laser[idx].append(val) 
            elif row['Extra'].startswith('DARK_195'):
                dark_195[idx].append(val)
            elif row['Extra'].startswith('DARK'):
                dark[idx].append(val)
        dark = np.mean(dark, axis=-1)
        dark_195 = np.mean(dark_195, axis=-1)
        laser = np.mean(laser, axis=-1)
        plt.figure()
        plt.plot(np.arange(16), laser, label='laser SPE charge')
        plt.plot(np.arange(16), dark, label='dark_100 SPE charge')
        plt.plot(np.arange(16), dark_195, label='dark_195 SPE charge')
        plt.legend()
        plt.show()
        input()
except FileNotFoundError:
    print('No data')
    exit()
                

