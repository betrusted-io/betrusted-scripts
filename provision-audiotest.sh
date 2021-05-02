#!/bin/bash

UPDATE_SHORT_8=0
UPDATE_SHORT_CD=0
UPDATE_LONG_8=0

for arg in "$@"
do
    case $arg in
	--short8)
	    UPDATE_SHORT_8=1
	    shift
	    ;;
	--shortcd)
	    UPDATE_SHORT_CD=1
	    shift
	    ;;
	--long8)
	    UPDATE_LONG_8=1
	    shift
	    ;;
	-h|--help)
	    echo "$0 provisions audio samples for testing. select which region to update with --short8, --shortcd, --long8"
	    exit 0
	    ;;
	*)
	    OTHER_ARGUMENTS+=("$1")
	    shift
	    ;;
    esac
done

if [ $UPDATE_SHORT_8 -eq 0 ] && [ $UPDATE_SHORT_CD -eq 0 ] && [ $UPDATE_LONG_8 -eq 0 ]
then
    echo "$0 requires one or more arguments of --short8, --shortcd, --long8"
    exit 0
fi

md5sum ../precursors/short_8khz.wav
md5sum ../precursors/short_cd.wav
md5sum ../precursors/long_8khz.wav

sudo ./reset-soc.sh
if [ $UPDATE_SHORT_8 -eq 1 ]
then
    cd jtag-tools && ./jtag_gpio.py -f ../../precursors/short_8khz.wav --raw-binary -a 0x6000000 -s -r
    cd ..
fi

if [ $UPDATE_SHORT_CD -eq 1 ]
then
    # can't verify, sample is too big
    cd jtag-tools && ./jtag_gpio.py -f ../../precursors/short_cd.wav --raw-binary -a 0x6080000 -s -r -n
    cd ..
fi

if [ $UPDATE_LONG_8 -eq 1 ]
then
    # can't verify, sample is too big
    cd jtag-tools && ./jtag_gpio.py -f ../../precursors/long_8khz.wav --raw-binary -a 0x6340000 -s -r -n
    cd ..
fi
sudo ./reset-soc.sh
