# betrusted-scripts

Scripts to provision Precursor using a Raspberry Pi plus the Raspberry Pi debug hat.

## Setup

The core scripts most users will want to use are `update_xous.sh`, `config_up5k.sh`,
and `wfx_image.sh`. These are all in the root level of this repository.

These scripts assume the following file structure:

```
/home/pi/
   |
  code---
        |
        ------ betrusted-scripts    <-- this repo
        |
        ------ precursors           <-- where firmware artifacts (built on another computer) are staged
	|
	------ fomu-flash           <-- https://github.com/betrusted-io/fomu-flash
```

This repo uses submodules, so be sure to either clone using `--recursive` or call a
`git submodule init && git submodule update` after cloning.

Note that the `jtag-tools` has dependencies, please consult its README file on how to
setup and install.

Furthermore note that you will need [fomu-flash](https://github.com/betrusted-io/fomu-flash) installed
and built for the Pi. Please refer to its repo for compilation and installation instructions.

If you want GDB debugging capability, you may also want to install [wishbone-tool](https://wishbone-utils.readthedocs.io/en/latest/wishbone-tool/) and plug Precursor's USB port into the Raspberry Pi's USB port.

## Limitations
There is a hard cap on the size of a bitstream that can be handled by the script, due
to an FFI array that is pre-allocated. It is 20Mbits, or about 2.9Mbytes. The script will
also segfault if you try to readback-verify very large files (>3.5MiB or so); this may be due
to stack size limitations imposed by the shell environment. This is because readback verification
is done in a "single shot" into a large buffer (for performance reasons). Programming a binary
does not have a parallel limitation because files are broken into erase-sector sized blocks and
iteratively programmed. In pracitce, I have never seen a verify failure, so for large files
I just drop the verification step.

## Usage

"Firmware artifacts" are binary files that correspond to various
sections of the device firmware. If you are simply trying to flash
your device from a pre-build, you would receive an archive of the
binaries and extract them into the `precursors` directory.

Copies of the bleeding-edge binaries can be found [on the CI
server](https://ci.betrusted.io/latest-ci/). They aren't stable, but
if you also don't want to set up a build environment, it's the fastest
way to get started. As of writing, there is not yet a "stable" release
process for Precursor.

If you are trying to build everything from scratch, there are corresponding
scripts in the firmware generation directories for:

- Xous ([xous-core](https://github.com/betrusted-io/xous-core/blob/main/buildpush.sh))
- validation firmware image ([betrusted-soc/fw](https://github.com/betrusted-io/betrusted-soc/blob/main/fw/buildpush.sh))
- the EC image (betrusted-ec - run `cargo xtask push [ip address] [ssh ID file]` in the root of the repo)

Note that you should build *either* Xous *or* the validation firmware image. At the time of writing, it's strongly
encouraged that you build for Xous, as Xous is actively maintained and the validation firmware (which was used mainly
for hardware board bringup) is soon to be depracated.

These scripts will build firmware images and copy them to the `code/precursors` directory on the target
Raspberry Pi automatically, once you specify the IP address of the Pi and a ssh private key file (if used).

Here is a description of the relevant commands, in the order that you would execute them to bring up a board "from factory blank state" (that is, with brand new, blank FLASH memories everywhere):

- `update_xous.sh` (_if you have initialized root keys_) stages updates for your Precursor. The kernel and loader will be immediately effective, but you need to select 'Install gateware update' from the main menu on your device for the gateware to take hold. You will also want to sign the Xous update as well.
- `provision_xous.sh` will do a factory reset of your Precursor. This is also the script to use if you have **not** initialized root keys on the device (that is, it's already in a factory-new state). It burns an FPGA image `precursors/encrypted.bin`, a firmware file `precursors/xous.img`, and a loader `precursors/loader.bin` to the correct locations in SoC FLASH space.
- `wfx_image.sh` will write the firmware blob for the SiLabs WF200 to the EC's SPI memory space. Note that the WF200 is an untrusted entity, and the system trusts the WF200 precisely as much as it would trust any cable modem or core router. The firmware image comen from within the `wfx-firmware` submodule within this repo.
- `config_up5k.sh` will set the QE bit of the EC SPI memory and provision an image located in `precursors/bt-ec.bin` onto the EC SPI. This effectively provisions the EC.

Note that an "encrypted" image is used for the FPGA by default; however,
for FPGAs that have not been sealed by the end user, the encrypted image
is encrypted by default to the "dummy key", e.g. an AES key of all 0's.

## Other scripts

Some other scripts included here:

- `vbus.sh` -- takes an argument `0` or `1` to turn off or on the power to Precursor. Do not turn on power to Precursor if you have it already plugged into a charger. This script is mostly useful for low-level debug of stand-alone boards.
- `uart_fpga.sh` -- mux the SoC UART to the Rpi `/dev/ttyS0`
- `uart_up5k.sh` -- mux the EC UART to the Rpi `/dev/ttyS0`
- `reset_soc.sh` -- pulls the PROG_N line on the SoC, forcing it to reload. Also resets the SPI ROM out of OPI mode.
- `reset_ec.sh` -- pulls the CRESET_B line on the EC, forcing it to reload.
- `start_gdb*.sh` -- will start `wishbone-tools` wish some defaults that enable variations of GDB connectivity, either via USB, TTY, or otherwise. Assumes `crossover` UART unless `-noterm` is used. Requires a .csv file that describes the FPGA in question, which should be provisioned automatically if you use the `buildpush.sh` script in the correpsonding FPGA's repo.

# BBRAM key provisioning

The BBRAM key is an ephemeral (battery-backed) key for the FPGA. You should use this if you have high value,
ephemeral secrets you wish to protect. Don't use it for archival (because when the battery dies you lose
your data). It's also useful for development.

The 7-Series FPGA is unable to provision its own BBRAM key, unfortunately. It needs help of an external tool
to receive the secret material and turn it into JTAG commands. `bbram_helper.py` is a script that does this.
While it creates no files or intentionally permanent record of the key, if you are worried about safety:

- Run this on a use-once Raspberry Pi that has been airgapped from the network
- Turn off the device immediately after provisioning
- Wait at least few minutes (so the DRAM cells fully discharge) before powering on again
- Ideally, destroy the microSD card used to boot the image, just in case portions of RAM were committed to disk (e.g. as swap).

## Prerequisites

You will need to install the following packages:

- `pexpect` (installed via `pip3 install` or `pip install` based on your distro)

## Running

1. Connect the Precursor device to a Raspberry Pi via the debug HAT (see https://github.com/betrusted-io/betrusted-wiki/wiki/Updating-Your-Device#failsafe-method)
1. Boot the Precursor device
1. Run `./bbram_helper.py` and follow the on-screen instructions

# Contribution Guidelines

[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-v2.0%20adopted-ff69b4.svg)](CODE_OF_CONDUCT.md)

Please see [CONTRIBUTING](CONTRIBUTING.md) for details on
how to make a contribution.

Please note that this project is released with a
[Contributor Code of Conduct](CODE_OF_CONDUCT.md).
By participating in this project you agree to abide its terms.

## License

Copyright © 2021

Licensed under the [GPL-3.0](https://opensource.org/licenses/GPL-3.0) [LICENSE](LICENSE)
