#!/bin/sh

. ./functions.sh

ensure_pigpiod
pigs modes 24 w write 24 0
sleep 0.1
pigs write 24 1
