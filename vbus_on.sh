#!/bin/sh

. ./functions.sh

ensure_pigpiod
pigs modes 21 w write 21 1
