#!/bin/sh
wishbone-tool --uart /dev/ttyS0 -b 115200 -s gdb --bind-addr 0.0.0.0 --debug-offset 0xe00f0000
