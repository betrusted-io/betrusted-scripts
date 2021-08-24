echo "This script is used for doing development on the FPGA bootloader. You must configure the FPGA to try to jump to a bootloader image at 0xD0_0000 for this to work. Normally, the bootloader is burned into the FPGA."
cd jtag-tools && ./jtag_gpio.py -f ../../precursors/boot.bin --raw-binary -a 0xd00000 -s -r && cd .. && ./reset-soc.sh
