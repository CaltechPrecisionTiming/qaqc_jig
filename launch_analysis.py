#! /usr/bin/python3

import os
import argparse

parser = argparse.ArgumentParser(description='Launch reco')
parser.add_argument("-s", "--submit", help="submit jobs", action='store_true')
args = parser.parse_args()

path = 'data/MiB5015/sourceMedium/'
sourceType = 'sodium'

#path = 'data/MiB5015/intrinsic/'
#sourceType = 'lyso'

modules = [
    #'module_32110000100020_Vov3.00_Nspe100000_Nsource200000',
    #'module_32110000100073_Vov2.00_Nspe100000_Nsource200000',
    #'module_32110000100073_Vov3.00_Nspe100000_Nsource200000',
    #'module_32110000100073_Vov4.00_Nspe100000_Nsource200000',    
    #'module_32110000200043_Vov3.00_Nspe100000_Nsource200000',
    #'module_32110000200089_Vov3.00_Nspe100000_Nsource200000',
    #'module_32110000200044_Vov3.00_Nspe100000_Nsource200000',
    #'module_32110000300024_Vov3.00_Nspe100000_Nsource200000',
    #'module_32110000200096_Vov4.00_Nspe100000_Nsource200000',
    'module_99990000200745_Vov5.00_Nspe100000_Nsource200000'
]

for module in modules:
    command = './python/analyze-waveforms --sourceType %s --print-pdfs \"/var/www/html/\" -o %s/%s_analysis.root %s/%s_integrals.hdf5' % (sourceType,path,module,path,module)
    print(command)
    if args.submit: os.system(command)
