#!/bin/sh

. ./functions.sh

ensure_pigpiod
pigs modes 18 w write 18 0
