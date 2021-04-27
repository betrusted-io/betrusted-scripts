#!/bin/sh

if [ ! -d /sys/class/gpio/gpio25 ]
then
    echo "25" > /sys/class/gpio/export
fi

sleep 0.1
echo "out" > /sys/class/gpio/gpio25/direction
echo 0 > /sys/class/gpio/gpio25/value
sleep 0.1
echo 1 > /sys/class/gpio/gpio25/value

