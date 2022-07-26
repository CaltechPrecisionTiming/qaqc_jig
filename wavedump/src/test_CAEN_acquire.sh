rm blah.hdf5

make

./wavedump -o blah.hdf5 -n 10000 --barcode 1 --voltage 42

./analyze-waveforms blah.hdf5 -o blah.root --s 0 --IT 250 --active "ch0" --plot
