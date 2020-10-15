#!/bin/sh
wishbone-tool -s gdb --bind-addr 0.0.0.0 -s terminal --csr-csv=../bin/soc-csr.csv --debug-offset=0xefff0000
