rm blah.hdf5

make

./wavedump -o blah.hdf5 -n 10000 --barcode 1 --voltage 45 --trigger "software"

./analyze-waveforms blah.hdf5 -o blah.root --s -100 --IT 300 --active "ch0" --plot
