#!/bin/bash

echo "WARNING: this script is for un-bricking devices, and will overwrite any secret keys stored in the gateware"

UPDATE_FPGA=1
UPDATE_KERNEL=1
UPDATE_LOADER=1

for arg in "$@"
do
    case $arg in
	-k|--kernel-skip)
	    UPDATE_KERNEL=0
	    shift
	    ;;
	-l|--loader-skip)
	    UPDATE_LOADER=0
	    shift
	    ;;
	-f|--fpga-skip)
	    UPDATE_FPGA=0
	    shift
	    ;;
	-h|--help)
	    echo "$0 provisions betrusted. --kernel-skip skips the kernel, --fpga-skip skips the FPGA. This script will overwrite any secret keys stored in the gateware."
	    exit 0
	    ;;
	*)
	    OTHER_ARGUMENTS+=("$1")
	    shift
	    ;;
    esac
done

md5sum ../precursors/soc_csr.bin
md5sum ../precursors/loader.bin
md5sum ../precursors/xous.img

sudo ./reset-soc.sh
if [ $UPDATE_FPGA -eq 1 ]
then
    cd jtag-tools && ./jtag_gpio.py -f ../../precursors/soc_csr.bin --raw-binary --spi-mode -r
    cd ..
fi

if [ $UPDATE_LOADER -eq 1 ]
then
    cd jtag-tools && ./jtag_gpio.py -f ../../precursors/loader.bin --raw-binary -a 0x500000 -s -r
    cd ..
fi

if [ $UPDATE_KERNEL -eq 1 ]
then
    cd jtag-tools && ./jtag_gpio.py -f ../../precursors/xous.img --raw-binary -a 0x980000 -s -r -n
    cd ..
fi
sudo ./reset-soc.sh
