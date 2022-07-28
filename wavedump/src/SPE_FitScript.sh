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
src=true

### VARIABLES
# * LYSO barcode
Lcode=1
# * Date
date=$(date +%m%d%Y)
# * Time
Time=$(date +%k":"%M":"%S)
# * Bias Voltage
BV="42"
# * number of events
ne=100000
# * Active channel - if left as empty string, all channels are analyzed.
active="ch0"
# * Extra
EXTRA="SPE_Charge"

# Creating filenames


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
	spe_hdf5="CAEN_${active}_${date}_${Time}_${BV}v_${EXTRA}.hdf5"
	spe_root="CAEN_${active}_${date}_${Time}_${BV}v_LASER_${EXTRA}.root"
	
	make
	./wavedump -o $spe_hdf5 -n $ne --barcode $Lcode --voltage $BV --trigger "external"
	
	# * Integration Start Time.
	s="-100"
	# * Integration Duration
	IT="100"
	./analyze-waveforms $spe_hdf5 -o $spe_root --pdf --s $s --IT $IT --active $active
	./fit-histograms $spe_root --pdf 
	python3 save_spe_data.py --BV $BV --n $ne --date $date --time $Time --source "CAEN" --extra "LASER_$EXTRA"
	
	spe_root="CAEN_${active}_${date}_${Time}_${BV}v_DARK_${EXTRA}.root"
	# * Integration Start Time.
	s="0"
	# * Integration Duration
	IT="200"
	./analyze-waveforms $spe_hdf5 -o $spe_root --pdf --s $s --IT $IT --active $active
	./fit-histograms $spe_root --pdf 
	python3 save_spe_data.py --BV $BV --n $ne --date $date --time $Time --source "CAEN" --extra "DARK_${IT}_${EXTRA}"
else
	spe_hdf5="SCOPE_${active}_${date}_${Time}_${BV}v_${EXTRA}.hdf5"
	spe_root="SCOPE_${active}_${date}_${Time}_${BV}v_LASER_${EXTRA}.root"
	
	./acquire-waveforms -n $ne --ip-address $ipad -o $spe_hdf5
	
	# * Integration Start Time.
	s="40"
	# * Integration Duration
	IT="100"
	./analyze-waveforms $spe_hdf5 -o $spe_root --pdf --s $s --IT $IT --active $active
	./fit-histograms $spe_root --pdf 
	python3 save_spe_data.py --BV $BV --n $ne --date $date --time $Time --source "SCOPE" --extra "LASER_$EXTRA"
	
	spe_root="SCOPE_${active}_${date}_${Time}_${BV}v_DARK_${EXTRA}.root"
	# * Integration Start Time.
	s="200"
	# * Integration Duration
	IT="200"
	./analyze-waveforms $spe_hdf5 -o $spe_root --pdf --s $s --IT $IT --active $active
	./fit-histograms $spe_root --pdf 
	python3 save_spe_data.py --BV $BV --n $ne --date $date --time $Time --source "SCOPE" --extra "DARK_${IT}_${EXTRA}" 
fi

set -edd data to csv / excel automatically
