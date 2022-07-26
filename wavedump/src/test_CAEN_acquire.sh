rm blah.hdf5

make

./wavedump -o blah.hdf5 -n 5000 --barcode 1 --voltage 45

./analyze-waveforms blah.hdf5 -o blah.root --s 0 --IT 100 --active "ch0"
