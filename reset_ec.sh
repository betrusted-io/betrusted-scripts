#!/bin/sh

. ./functions.sh

ensure_pigpiod
pigs modes 25 w write 25 0
sleep 0.1
pigs write 25 1
