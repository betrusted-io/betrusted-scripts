#!/bin/sh

if [ "$1" != "0" ] && [ "$1" != "1" ]; then
    echo "Needs an argument of 0 or 1"
    exit 0
fi

if [ ! -d /sys/class/gpio/gpio21 ]
then
    echo "21" > /sys/class/gpio/export
fi

sleep 0.1
echo "out" > /sys/class/gpio/gpio21/direction
echo $1 > /sys/class/gpio/gpio21/value
