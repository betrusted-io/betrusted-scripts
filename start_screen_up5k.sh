#!/bin/sh
echo "Starting serial terminal emulator (screen)..."
echo "  To exit screen, use Ctrl-A k"
sleep 2
./uart_up5k.sh
# Using -fn turns off screen's default automatic XON/XOFF flow control. Auto
# flow control can unpredictably interrupt the serial debug log, resulting in a
# blank serial terminal screen, an unresponsive keyboard, and no indication of
# what went wrong.
screen -fn /dev/serial0 115200
