#!/bin/sh

if [ ! -d /sys/class/gpio/gpio21 ]
then
    echo "21" > /sys/class/gpio/export
fi

sleep 0.1
echo "out" > /sys/class/gpio/gpio21/direction
echo "0" > /sys/class/gpio/gpio21/value
