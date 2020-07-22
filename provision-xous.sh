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

md5sum ../bin/encrypted.bin
md5sum ../bin/xous.img

sudo ./reset-soc.sh
if [ $UPDATE_FPGA -eq 1 ]
then
    sudo openocd -c 'set BITSTREAM_FILE ../bin/encrypted.bin' -f spi-bitstream.cfg
fi

if [ $UPDATE_KERNEL -eq 1 ]
then
    sudo openocd -c 'set FIRMWARE_FILE ../bin/xous.img' -f spi-fw.cfg
fi
sudo ./reset-soc.sh
