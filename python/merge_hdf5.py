#! /usr/bin/python3

import h5py
import sys
import numpy as np



def merge_hdf5_files(filepaths):
    
    outputpath = filepaths[-1]

    # Open the output HDF5 file
    with h5py.File(outputpath, 'a') as fileo:
        
        # Open the first HDF5 file
        with h5py.File(filepaths[0], 'r') as file1:
            
            for name,item in file1.items():
                print(name, item)

                output_group = None
                if isinstance(item, h5py.Group):
                    output_group = fileo.require_group(name)
                
                for name2,item2 in item.items():
                    print(name2,item2)

                    if isinstance(item2, h5py.Dataset):
                        if name2 in ['spe_charge', 'lyso_charge', 'lyso_rise_time', 'lyso_fall_time']:
                            output_group.create_dataset(name2, maxshape=(None,), data=item2[...])
                        else:
                            output_group.create_dataset(name2, data=item2[...])
                    
                    # Open the other HDF5 files and merge the datasets
                    for filepath in filepaths[1:-1]:
                        with h5py.File(filepath, 'r') as filei:
                            output_dataset = output_group[name2]
                            dataseti = filei[name][name2]
                            if name2 in ['spe_charge', 'lyso_charge', 'lyso_rise_time', 'lyso_fall_time']:
                                output_dataset.resize((output_dataset.shape[0] + dataseti.shape[0],) + dataseti.shape[1:])
                                output_dataset[-dataseti.shape[0]:, ...] = dataseti



# Example usage:
nfiles = len(sys.argv) - 2
if nfiles < 2:
    print('Specify at least two files to merge and a destination file.')
    print('Usage is: ./merge_hdf5.py file1.hdf5 file2.hdf5 ... fileN.hdf5 output.hdf5. Exiting...')
    sys.exit(1)

merge_hdf5_files(sys.argv[1:])
