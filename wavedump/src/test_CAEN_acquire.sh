rm blah.hdf5

make

time ./wavedump -o blah.hdf5 -n 10000 --barcode 1 --voltage 42 --trigger "external"

./analyze-waveforms blah.hdf5 -o blah.root --s -125 --IT 100 --active "ch0" --plot
