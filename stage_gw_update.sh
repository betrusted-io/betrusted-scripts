#!/bin/bash

md5sum ../precursors/soc_csr.bin

sudo ./reset-soc.sh
cd jtag-tools && ./jtag_gpio.py -f ../../precursors/soc_csr.bin --raw-binary -a 0x280000 --spi-mode -r
cd ..
sudo ./reset-soc.sh
