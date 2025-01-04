#!/bin/sh

. ./functions.sh

if [ "$1" != "0" ] && [ "$1" != "1" ]; then
    echo "Needs an argument of 0 or 1"
    exit 0
fi

ensure_pigpiod
pigs modes 21 w write 21 $1
