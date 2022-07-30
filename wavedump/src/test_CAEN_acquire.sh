rm blah.hdf5

make

./wavedump -o par5_test.hdf5 -n 10000 --barcode 1 --voltage 45 --trigger "external"

./analyze-waveforms par5_test.hdf5 -o par5_test.root --s -125 --IT 100 --active "ch0" --plot
