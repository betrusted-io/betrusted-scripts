#!/bin/sh
md5sum ../precursors/swap.img
sudo ../fomu-flash/fomu-flash -4 && sudo ../fomu-flash/fomu-flash -w ../precursors/swap.img && sudo ../fomu-flash/fomu-flash -r
cp ../precursors/swap.img .
