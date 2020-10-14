#!/bin/sh
wishbone-tool -s gdb --bind-addr 0.0.0.0 -s terminal --csr-csv=soc-csr.csv --debug-offset=0xefff0000
