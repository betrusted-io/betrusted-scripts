#!/bin/sh
md5sum ../precursors/usbc_img.bin
sudo ../fomu-flash/fomu-flash -4 && sudo ../fomu-flash/fomu-flash -w ../precursors/usbc_img.bin && sudo ../fomu-flash/fomu-flash -r 
