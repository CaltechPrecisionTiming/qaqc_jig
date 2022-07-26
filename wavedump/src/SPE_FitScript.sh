#!/bin/sh
# set -x
set -e

#Author : Guillermo Reales & Anthony La Torre, with modifications by Kai Svenson
#Commands required to measure SPE charge
#Script follows here:

# * ipadress
ipad="192.168.0.182"

echo "Starting Time:"
date +"%D %T"

# IF error "Permission denied" run:
# chmod u+r+x <filename>.sh

# IF variable gives error "Command not recognize"
# Remember that the equal sign can have no spaces to the variable name

### DATA SOURCiE
# true takes data from CAEN, false from scope
src=false

### VARIABLES
# * LYSO barcode
Lcode=1
# * Date
date=$(date +%m%d%Y)
# * Time
Time=$(date +%k":"%M":"%S)
# * Bias Voltage
BV="45"
# * Integration Start Time
s="200"
# * Integration Time
IT="200"
# * number of events
ne=10000
# * Active channel - if left as empty string, all channels are analyzed.
active="ch1"
# * Extra
EXTRA="dark"

# Creating filenames
spe_hdf5="spe_${date}_${Time}_${BV}v_${EXTRA}.hdf5"
spe_root="spe_${date}_${Time}_${BV}v_${EXTRA}.root"


python3 clear_temp_data.py

echo "---CHECKLIST---"
if [ "$src" = true ] ; then
	echo "SOURCE: CAEN"
else
	echo "SOURCE: SCOPE"
fi
echo "BIAS: $BV"
echo "EVENTS: $ne"
echo "ACTIVE: $active"
echo "Press any key to continue"
read

# SPE ANALYSIS
if [ "$src" = true ] ; then
	make
	./wavedump -o $spe_hdf5 -n $ne --barcode $Lcode --voltage $BV
else
	./acquire-waveforms -n $ne --ip-address $ipad -o $spe_hdf5
fi
./analyze-waveforms $spe_hdf5 -o $spe_root --pdf --s $s --IT $IT --active $active
./fit-histograms $spe_root --pdf 

python3 save_spe_data.py --BV $BV --n $ne --date $date --time $Time --source "SCOPE" --extra $EXTRA

set -edd data to csv / excel automatically
