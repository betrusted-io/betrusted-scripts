#!/bin/bash

UPDATE_FPGA=1
UPDATE_KERNEL=1

for arg in "$@"
do
    case $arg in
	-k|--kernel-skip)
	    UPDATE_KERNEL=0
	    shift
	    ;;
	-f|--fpga-skip)
	    UPDATE_FPGA=0
	    shift
	    ;;
	-h|--help)
	    echo "$0 provisions betrusted. --kernel-skip skips the kernel, --fpga-skip skips the FPGA"
	    exit 0
	    ;;
	*)
	    OTHER_ARGUMENTS+=("$1")
	    shift
	    ;;
    esac
done

md5sum ../precursors/encrypted.bin
md5sum ../precursors/betrusted-soc.bin

# sudo ./reset-soc.sh
if [ $UPDATE_FPGA -eq 1 ]
then
    cd jtag-tools && ./jtag_gpio.py -f ../../precursors/encrypted.bin --bitstream --spi-mode -r
    cd ..
    # sudo openocd -c 'set BITSTREAM_FILE ../precursors/encrypted.bin' -f spi-bitstream.cfg
fi

if [ $UPDATE_KERNEL -eq 1 ]
then
    cd jtag-tools && ./jtag_gpio.py -f ../../precursors/betrusted-soc.bin --raw-binary -a 0x500000 -s -r
    cd ..
    # sudo openocd -c 'set FIRMWARE_FILE ../precursors/betrusted-soc.bin' -f spi-fw.cfg
fi
sudo ./reset-soc.sh
