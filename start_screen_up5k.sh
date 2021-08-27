#!/bin/sh
echo "Starting serial terminal emulator (screen)..."
echo "  To exit screen, use Ctrl-A k"
sleep 2
set -x
./uart_up5k.sh
sleep 0.1
screen /dev/serial0 115200
