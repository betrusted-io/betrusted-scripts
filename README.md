# betrusted-scripts

Scripts to provision Precursor using a Raspberry Pi plus the Raspberry Pi debug hat.

## Setup

The core scripts most users will want to use are `provision-fw.sh`, `provision-xous.sh`, `config_up5k.sh`,
and `wfx_firmware.sh`.

These scripts assume the following file structure:

```
/home/pi/
   |
  code---
        |
        ------ betrusted-scripts
        |
        ------ bin
	|
	------ fomu-flash

This repo uses submodules, so be sure to either clone using `--recursive` or call a
`git submodule init && git submodule update` after cloning.

Note that the `jtag-tools` has dependencies, please consult its README file on how to
setup and install.

Furthermore note that you will need [fomu-flash](https://github.com/im-tomu/fomu-flash) installed
and built for the Pi. Please refer to its repo for compilation and installation instructions.

If you want GDB debugging capability, you may also want to install [wishbone-tool](https://wishbone-utils.readthedocs.io/en/latest/wishbone-tool/) and plug Precursor's USB port into the Raspberry Pi's USB port.

## Usage

There are corresponding `buildpush.sh` scripts in the firmware generation directories
for Xous and the validation firmware image (betrusted-soc/fw) that will build firmware images
and copy them to the `code/bin` directory on the Raspberry Pi automatically, once you
specify the IP address of the Pi and a ssh private key file (if used).

Here is a description of the relevant commands, in the order that you would execute them to bring up a board "from factory blank state" (that is, with brand new, blank FLASH memories everywhere):

- `wfx-firmware.sh` will write the firmware blob for the SiLabs WF200 to the EC's SPI memory space. Note that the WF200 is an untrusted entity, and the system trusts the WF200 precisely as much as it would trust any cable modem or core router.
- `config_up5k.sh` will set the QE bit of the EC SPI memory and provision an image located in `bin/bt-ec.bin` onto the EC SPI. This effectively provisions the EC.
- `provision_fw.sh` will burn both an FPGA image `bin/encrypted.bin` and a firmware file `bin/betrusted-soc.bin` to the correct locations in SoC FLASH space. This is used for the low-level validation (if you plan to use Xous, use `provision_xous.sh`, this is unecessary).
- `provision_xous.sh` will burn both an FPGA image `bin/encrypted.bin` and a firmware file `bin/xous.img` to the correct locations in SoC FLASH space. This used for Xous.

The validation image boots directly from SPI, but Xous boots from
internal ROM, which is why the provisioning scripts are different. The
intention is that eventually cryptographic boot/signature checks will
be made available in the ROM that make it safer to load the Xous OS
image from SPI, but these have not yet been written, whereas the
validation firmware's intention is a factory test to be run only in
the factory and then erased; however it's also very useful for developers
who want direct access to the hardware or who want to develop or port their
own OS to Precursor.

Note that an "encrypted" image is used for the FPGA by default; however,
for FPGAs that have not been sealed by the end user, the encrypted image
is encrypted by default to the "dummy key", e.g. an AES key of all 0's. 

## Contribution Guidelines

[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-v2.0%20adopted-ff69b4.svg)](CODE_OF_CONDUCT.md)

Please see [CONTRIBUTING](CONTRIBUTING.md) for details on
how to make a contribution.

Please note that this project is released with a
[Contributor Code of Conduct](CODE_OF_CONDUCT.md).
By participating in this project you agree to abide its terms.

## License

Copyright © 2019

Licensed under the [GPL-3.0](https://opensource.org/licenses/GPL-3.0) [LICENSE](LICENSE)
