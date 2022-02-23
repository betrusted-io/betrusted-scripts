#!/bin/bash
set -e

mkdir -p ../precursors

wget https://ci.betrusted.io/latest-ci/loader.bin --no-check-certificate -O ../precursors/loader.bin

wget https://ci.betrusted.io/latest-ci/xous.img --no-check-certificate -O ../precursors/xous.img

wget https://ci.betrusted.io/latest-ci/soc_csr.bin --no-check-certificate -O ../precursors/soc_csr.bin

wget https://ci.betrusted.io/latest-ci/ec_fw.bin --no-check-certificate -O ../precursors/ec_fw.bin
wget https://ci.betrusted.io/latest-ci/bt-ec.bin --no-check-certificate -O ../precursors/bt-ec.bin
wget https://ci.betrusted.io/latest-ci/wfm_wf200_C0.sec --no-check-certificate -O ../precursors/wfm_wf200_C0.sec
wget https://ci.betrusted.io/latest-ci/wf200_fw.bin --no-check-certificate -O ../precursors/wf200_fw.bin
