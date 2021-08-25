#!/bin/sh
set -x
./uart_up5k.sh
./reset_ec.sh \
    && sleep 0.1 \
    && wishbone-tool --uart /dev/serial0 -b 115200 -s terminal --csr-csv=../precursors/csr.csv
