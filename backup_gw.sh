#!/bin/bash
cd jtag-tools && ./jtag_gpio.py -f ../soc_csr.bak --read --read-len 0x280000 --read-addr 0x0 -s -r -n
