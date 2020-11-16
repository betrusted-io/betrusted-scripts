#!/bin/sh
md5sum ../precursors/bt-ec.bin
sudo ../fomu-flash/fomu-flash -4 && sudo ../fomu-flash/fomu-flash -w ../precursors/bt-ec.bin && sudo ../fomu-flash/fomu-flash -r 
