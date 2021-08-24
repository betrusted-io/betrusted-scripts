#!/usr/bin/python3

try:
    import RPi.GPIO as GPIO
except RuntimeError:
    print("Error importing RPi.GPIO! Did you run as root?")

import argparse
import time
import subprocess
import logging
import sys
import binascii

import pexpect
from pexpect.fdpexpect import fdspawn
import ast

from enum import Enum

CONSOLE_SENTINEL = 'CONS_SENTINEL|'

TCK_pin = 4
TMS_pin = 17
TDI_pin = 27  # TDI on FPGA, out for this script
TDO_pin = 22  # TDO on FPGA, in for this script
PRG_pin = 24

class JtagLeg(Enum):
    DR = 0
    IR = 1
    RS = 2 # reset
    DL = 3 # long delay
    ID = 4 # idle in run-test
    IRP = 5  # IR with pause
    IRD = 6  # transition to IR directly
    DRC = 7  # DR for config: MSB-to-LSB order, and use fast protocols
    DRR = 8  # DR for recovery: print out the value returned in non-debug modes
    DRS = 9  # DR for SPI: MSB-to-LSB order, use fast protocols, but also readback data

class JtagState(Enum):
    TEST_LOGIC_RESET = 0
    RUN_TEST_IDLE = 1
    SELECT_SCAN = 2
    CAPTURE = 3
    SHIFT = 4
    EXIT1 = 5
    PAUSE = 6
    EXIT2 = 7
    UPDATE = 8

state = JtagState.RUN_TEST_IDLE
cur_leg = []
jtag_legs = []
tdo_vect = ''
tdo_stash = ''
jtag_results = []
do_pause = False
readout = False
readdata = 0

from math import log
def bytes_needed(n):
    if n == 0:
        return 1
    return int(log(n, 256))+1

def int_to_binstr(n):
    return bin(n)[2:].zfill(bytes_needed(n)*8)

def int_to_binstr_bitwidth(n, bitwidth):
    return bin(n)[2:].zfill(bitwidth)

def phy_sync(tdi, tms):
    global TCK_pin, TMS_pin, TDI_pin, TDO_pin

    tdo = GPIO.input(TDO_pin) # grab the TDO value before the clock changes

    GPIO.output( (TCK_pin, TDI_pin, TMS_pin), (0, tdi, tms) )
    GPIO.output( (TCK_pin, TDI_pin, TMS_pin), (1, tdi, tms) )
    GPIO.output( (TCK_pin, TDI_pin, TMS_pin), (0, tdi, tms) )

    return tdo

def reset_fpga():
    global PRG_pin

    GPIO.output(PRG_pin, 0)
    time.sleep(0.1)
    GPIO.output(PRG_pin, 1)


def decode_ir(ir):
    if ir == 0b100110:
        return 'EXTEST'
    elif ir == 0b111100:
        return 'EXTEST_PULSE'
    elif ir == 0b111101:
        return 'EXTEST_TRAIN'
    elif ir == 0b000001:
        return 'SAMPLE'
    elif ir == 0b000010:
        return 'USER1'
    elif ir == 0b000011:
        return 'USER2'
    elif ir == 0b100010:
        return 'USER3'
    elif ir == 0b100011:
        return 'USER4'
    elif ir == 0b000100:
        return 'CFG_OUT'
    elif ir == 0b000101:
        return 'CFG_IN'
    elif ir == 0b001001:
        return 'IDCODE'
    elif ir == 0b001010:
        return 'HIGHZ_IO'
    elif ir == 0b001011:
        return 'JPROGRAM'
    elif ir == 0b001100:
        return 'JSTART'
    elif ir == 0b001101:
        return 'JSHUTDOWN'
    elif ir == 0b110111:
        return 'XADC_DRP'
    elif ir == 0b010000:
        return 'ISC_ENABLE'
    elif ir == 0b010001:
        return 'ISC_PROGRAM'
    elif ir == 0b010010:
        return 'XSC_PROGRAM_KEY'
    elif ir == 0b010111:
        return 'XSC_DNA'
    elif ir == 0b110010:
        return 'FUSE_DNA'
    elif ir == 0b010100:
        return 'ISC_NOOP'
    elif ir == 0b010110:
        return 'ISC_DISABLE'
    elif ir == 0b111111:
        return 'BYPASS'
    elif ir == 0b110001:
        return 'FUSE_KEY'
    elif ir == 0b110011:
        return 'FUSE_USER'
    elif ir == 0b110100:
        return 'FUSE_CNTL'
    else:
        return ''  # unknown just leave blank for now

def debug_spew(cur_leg):
    
    if not((cur_leg[0] == JtagLeg.DRC) or (cur_leg[0] == JtagLeg.DRS)):
        logging.debug("start: %s (%s) / %s", str(cur_leg), str(decode_ir(int(cur_leg[1],2))), str(cur_leg[2]) )
    else:
        logging.debug("start: %s config data of length %s", cur_leg[0], str(len(cur_leg[1])))

# take a trace and attempt to extract IR, DR values
# assume: at the start of each 'trace' we are coming from TEST-LOGIC-RESET
def jtag_step():
    global state
    global cur_leg
    global jtag_legs
    global jtag_results
    global tdo_vect, tdo_stash
    global do_pause
    global TCK_pin, TMS_pin, TDI_pin, TDO_pin
    global readout
    global readdata

    # logging.debug(state)
    if state == JtagState.TEST_LOGIC_RESET:
        phy_sync(0, 0)
        state = JtagState.RUN_TEST_IDLE

    elif state == JtagState.RUN_TEST_IDLE:
        if len(cur_leg):
            # logging.debug(cur_leg[0])
            if cur_leg[0] == JtagLeg.DR or cur_leg[0] == JtagLeg.DRC or cur_leg[0] == JtagLeg.DRR or cur_leg[0] == JtagLeg.DRS:
                phy_sync(0, 1)
                if cur_leg[0] == JtagLeg.DRR or cur_leg[0] == JtagLeg.DRS:
                    readout = True
                else:
                    readout = False
                state = JtagState.SELECT_SCAN
            elif cur_leg[0] == JtagLeg.IR or cur_leg[0] == JtagLeg.IRD:
                phy_sync(0, 1)
                phy_sync(0, 1)
                do_pause = False
                state = JtagState.SELECT_SCAN
            elif cur_leg[0] == JtagLeg.IRP:
                phy_sync(0, 1)
                phy_sync(0, 1)
                do_pause = True
                state = JtagState.SELECT_SCAN
            elif cur_leg[0] == JtagLeg.RS:
                logging.debug("tms reset")
                phy_sync(0, 1)
                phy_sync(0, 1)
                phy_sync(0, 1)
                phy_sync(0, 1)
                phy_sync(0, 1)
                phy_sync(0, 1)
                phy_sync(0, 1)
                phy_sync(0, 1)
                phy_sync(0, 1)
                phy_sync(0, 1)
                phy_sync(0, 1)
                phy_sync(0, 1)
                cur_leg = jtag_legs.pop(0)
                debug_spew(cur_leg)
                state = JtagState.TEST_LOGIC_RESET
            elif cur_leg[0] == JtagLeg.DL:
                time.sleep(0.005) # 5ms delay
                cur_leg = jtag_legs.pop(0)
                debug_spew(cur_leg)
            elif cur_leg[0] == JtagLeg.ID:
                phy_sync(0, 0)
                cur_leg = jtag_legs.pop(0)
                debug_spew(cur_leg)
        else:
            if len(jtag_legs):
                cur_leg = jtag_legs.pop(0)
                debug_spew(cur_leg)
            else:
                phy_sync(0, 0)
            state = JtagState.RUN_TEST_IDLE
                
    elif state == JtagState.SELECT_SCAN:
        phy_sync(0, 0)
        state = JtagState.CAPTURE

    elif state == JtagState.CAPTURE:
        phy_sync(0, 0)
        tdo_vect = ''  # prep the tdo_vect to receive data
        state = JtagState.SHIFT

    elif state == JtagState.SHIFT:
        if cur_leg[0] == JtagLeg.DRC or cur_leg[0] == JtagLeg.DRS:
            if cur_leg[0] == JtagLeg.DRC: # duplicate code because we want speed (eliminating TDO readback is significa
                GPIO.output((TCK_pin, TDI_pin), (0, 1))
                for bit in cur_leg[1][:-1]:
                    if bit == '1':
                        GPIO.output((TCK_pin, TDI_pin), (1, 1))
                        GPIO.output((TCK_pin, TDI_pin), (0, 1))
                    else:
                        GPIO.output((TCK_pin, TDI_pin), (1, 0))
                        GPIO.output((TCK_pin, TDI_pin), (0, 0))
            else:  # jtagleg is DRS -- duplicate code, as TDO readback slows things down significantly
                GPIO.output((TCK_pin, TDI_pin), (0, 1))
                for bit in cur_leg[1][:-1]:
                   if bit == '1':
                       GPIO.output( (TCK_pin, TDI_pin), (1, 1) )
                       GPIO.output( (TCK_pin, TDI_pin), (0, 1) )
                   else:
                       GPIO.output( (TCK_pin, TDI_pin), (1, 0) )
                       GPIO.output( (TCK_pin, TDI_pin), (0, 0) )
                tdo = GPIO.input(TDO_pin)
                if tdo == 1 :
                    tdo_vect = '1' + tdo_vect
                else:
                    tdo_vect = '0' + tdo_vect

            state = JtagState.SHIFT

            if cur_leg[-1:] == '1':
                tdi = 1
            else:
                tdi = 0
            cur_leg = ''
            tdo = phy_sync(tdi, 1)
            if tdo == 1:
                tdo_vect = '1' + tdo_vect
            else:
                tdo_vect = '0' + tdo_vect
            state = JtagState.EXIT1
            logging.debug('leaving config')
                
        else:
            if len(cur_leg[1]) > 1:
                if cur_leg[1][-1] == '1':
                    tdi = 1
                else:
                    tdi = 0
                cur_leg[1] = cur_leg[1][:-1]
                tdo = phy_sync(tdi, 0)
                if tdo == 1:
                    tdo_vect = '1' + tdo_vect
                else:
                    tdo_vect = '0' + tdo_vect
                state = JtagState.SHIFT
            else: # this is the last item
                if cur_leg[1][0] == '1':
                    tdi = 1
                else:
                    tdi = 0
                cur_leg = ''
                tdo = phy_sync(tdi, 1)
                if tdo == 1:
                    tdo_vect = '1' + tdo_vect
                else:
                    tdo_vect = '0' + tdo_vect
                state = JtagState.EXIT1

    elif state == JtagState.EXIT1:
        tdo_stash = tdo_vect
        if do_pause:
            phy_sync(0, 0)
            state = JtagState.PAUSE
            do_pause = False
        else:
            phy_sync(0, 1)        
            state = JtagState.UPDATE

    elif state == JtagState.PAUSE:
        logging.debug("pause")
        # we could put more pauses in here but we haven't seen this needed yet
        phy_sync(0, 1)        
        state = JtagState.EXIT2

    elif state == JtagState.EXIT2:
        phy_sync(0, 1)        
        state = JtagState.UPDATE

    elif state == JtagState.UPDATE:
        jtag_results.append(int(tdo_vect, 2)) # interpret the vector and save it
        logging.debug("result: %s", str(hex(int(tdo_vect, 2))) )
        if readout:
            #print('readout: 0x{:08x}'.format( int(tdo_vect, 2) ) )
            readdata = int(tdo_vect, 2)
            readout = False
        tdo_vect = ''

        # handle case of "shortcut" to DR
        if len(jtag_legs):
            if (jtag_legs[0][0] == JtagLeg.DR) or (jtag_legs[0][0] == JtagLeg.IRP) or (jtag_legs[0][0] == JtagLeg.IRD):
                if jtag_legs[0][0] == JtagLeg.IRP or jtag_legs[0][0] == JtagLeg.IRD:
                    phy_sync(0, 1)  # +1 cycle on top of the DR cycle below
                    logging.debug("IR bypassing wait state")
                if jtag_legs[0][0] == JtagLeg.IRP:
                    do_pause = True
                    
                cur_leg = jtag_legs.pop(0)
                debug_spew(cur_leg)
                phy_sync(0,1)
                state = JtagState.SELECT_SCAN
            else:
                phy_sync(0, 0)        
                state = JtagState.RUN_TEST_IDLE
        else:
            phy_sync(0, 0)        
            state = JtagState.RUN_TEST_IDLE

    else:
        print("Illegal state encountered!")

def jtag_next():
    global state
    global jtag_results

    if state == JtagState.TEST_LOGIC_RESET or state == JtagState.RUN_TEST_IDLE:
        if len(jtag_legs):
            # run until out of idle
            while state == JtagState.TEST_LOGIC_RESET or state == JtagState.RUN_TEST_IDLE:
                jtag_step()

            # run to idle
            while state != JtagState.TEST_LOGIC_RESET and state != JtagState.RUN_TEST_IDLE:
                jtag_step()
        else:
            # this should do nothing
            jtag_step()
    else:
        # we're in a leg, run to idle
        while state != JtagState.TEST_LOGIC_RESET and state != JtagState.RUN_TEST_IDLE:
            jtag_step()

"""
Reverse the order of bits in a word that is bitwidth bits wide
"""
def bitflip(data_block, bitwidth=32):
    if bitwidth == 0:
        return data_block

    bytewidth = bitwidth // 8
    bitswapped = bytearray()

    i = 0
    while i < len(data_block):
        data = int.from_bytes(data_block[i:i+bytewidth], byteorder='big', signed=False)
        b = '{:0{width}b}'.format(data, width=bitwidth)
        bitswapped.extend(int(b[::-1], 2).to_bytes(bytewidth, byteorder='big'))
        i = i + bytewidth

    return bytes(bitswapped)


# python sux
def auto_int(x):
    return int(x, 0)

def slow_send(console, s):
    for c in s:
        console.send(c)
        time.sleep(0.1)

def expand_binary(digits, value):
    return '%0*d' % (digits, int(bin(value)[2:]))

def main():
    global TCK_pin, TMS_pin, TDI_pin, TDO_pin, PRG_pin
    global jtag_legs, jtag_results
    global CONSOLE_SENTINEL

    GPIO.setwarnings(False)
    
    parser = argparse.ArgumentParser(description="Receive and burn BBRAM keys into a Precursor")
    parser.add_argument(
        "-d", "--debug", help="turn on debugging spew", default=False, action="store_true"
    )
    parser.add_argument(
        '--tdi', type=int, help="Specify TDI GPIO. Defaults to 27", default=27
    )
    parser.add_argument(
        '--tdo', type=int, help="Specify TDO GPIO. Defaults to 22", default=22
    )
    parser.add_argument(
        '--tms', type=int, help="Specify TMS GPIO. Defaults to 17", default=17
    )
    parser.add_argument(
        '--tck', type=int, help="Specify TCK GPIO. Defaults to 4", default=4
    )
    parser.add_argument(
        '--prg', type=int, help="Specify PRG (prog) GPIO. Defaults to 24", default=24
    )
    args = parser.parse_args()
    if args.debug:
       print("Debug logging is on! This will print secret material to the screen. Hit enter to continue if this is what you actually intended, or ^C to abort...")
       input()
       logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)

    if TCK_pin != args.tck:
        TCK_pin = args.tck
    if TDI_pin != args.tdi:
        TDI_pin = args.tdi
    if TDO_pin != args.tdo:
        TDO_pin = args.tdo
    if TMS_pin != args.tms:
        TMS_pin = args.tms
    if PRG_pin != args.prg:
        PRG_pin = args.prg
        # prog not in FFI, so no need for compat if it changes

    # build all the jtag commands up to the point where we want to insert the keys
    jtag_legs.append([JtagLeg.RS, '0', 'reset'])
    jtag_legs.append([JtagLeg.DL, '0', ' '])
    jtag_legs.append([JtagLeg.IR, '001011', 'jprogram'])
    jtag_legs.append([JtagLeg.IR, '010100', 'isc_noop'])
    jtag_legs.append([JtagLeg.IR, '010100', 'isc_noop'])
    jtag_legs.append([JtagLeg.IRP,'010000', 'isc_enable'])
    jtag_legs.append([JtagLeg.DR, '10101', ' '])
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.DR, '10101', ' '])
    jtag_legs.append([JtagLeg.IRP,'010010', 'program_key'])
    jtag_legs.append([JtagLeg.ID, '0', '0'])
    jtag_legs.append([JtagLeg.DR, expand_binary(32, 0xffffffff), ' '])
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.ID, '0', '0'])    
    jtag_legs.append([JtagLeg.IR, '010001', 'isc_program'])
    jtag_legs.append([JtagLeg.DR, expand_binary(32, 0x557b), ' '])

    print("It's recommended to run this script with the network disconnected, to eliminate")
    print("the possibility of key exfiltration via network. This script will initiate the")
    print("BBRAM transformation process; you will have to enter your update password")
    print("*ON THE PRECURSOR* as part of this process. This script will never request")
    print("any passwords, and you should never type that password into anything but the Precursor.\n")
    print("This script expects the following configuration:")
    print(" -A Raspberry Pi (3 or 4) with the Precusror debug HAT installed")
    print(" -A Precursor attached to the debug HAT via the debug flex cable")
    print(" -'No login over serial', 'hardware serial port enabled' in raspi-config->interfacing options->serial")
    print(" -This should provide a serial console at /dev/ttyS0 (not /dev/ttyAMA0)")
    print(" -No other process accessing the serial console (in particular do 'ps -aux | grep -i screen' to confirm no zombie console sessions are open")
    print("Press enter when you're ready to proceed.")
    input()
    print("Press enter on the Precursor screen to start the comms test...")
    ps = subprocess.check_output(['ps', 'aux']).decode('utf-8')
    found_screen = False
    for line in ps.split('\n'):
        if 'screen' in line.lower():
            if '/dev/ttyS0' in line:
                print(line)
                found_screen = True
    if found_screen:
        print("Screen processes found occupying /dev/ttyS0, aborting.")
        exit(0)

    # ensure we can talk to /dev/ttyS0 without having to be sudo. pip doesn't install the dependencies in sudo env
    perms = subprocess.check_output(['sudo', 'usermod', '-a', '-G', 'dialout', 'pi'])

    # open a serial terminal
    import serial
    ser = serial.Serial()
    ser.baudrate = 115200
    ser.port="/dev/ttyS0"
    ser.stopbits=serial.STOPBITS_ONE
    ser.xonxoff=0
    try:
        ser.open()
    except:
        print("couldn't open serial port")
        exit(1)
    console = fdspawn(ser)

    logging.debug("waiting for sentinel")
    try:
        slow_send(console, 'keys bbram\r') # send the command to initiate the provisioning
        console.expect_exact(CONSOLE_SENTINEL, 30)
    except Exception as e:
        print('problem talking to device: {}', str(e))

    logging.debug("sending hello")
    slow_send(console, 'HELPER_OK\r')

    print("You may need to enter the update password on the Precursor device now; do not type the password here!")
    
    console.expect_exact(CONSOLE_SENTINEL, 60)
    log = console.before.decode('utf-8')
    bbram_copies = []
    for line in log.split('\n'):
        logging.debug(line)
        if 'BBKEY|:' in line:
            liststr = line[line.find('['):line.find(']')+1]
            bbram_copies.append(ast.literal_eval(liststr))

    sanity_check = True
    if len(bbram_copies) != 3:
        sanity_check = False

    if bbram_copies[0] != bbram_copies[1] or bbram_copies[0] != bbram_copies[2]:
        sanity_check = False

    if sanity_check == False:
        print("BBRAM key failed integrity check, can't continue!")

    logging.debug('key (ssssh!): %s', binascii.hexlify(bytes(bbram_copies[0])))
    
    # wait until the routine reports it is finished
    console.expect_exact(CONSOLE_SENTINEL, 4*60)
    log = console.before.decode('utf-8')
    for line in log.split('\n'):
        logging.debug(line)

    console.close()

    # finish out the JTAG command sequence
    key_words = [bbram_copies[0][i:i+4] for i in range(0, len(bbram_copies[0]), 4)] # split key into 32-bit words
    for word in key_words:
        jtag_legs.append([JtagLeg.IR, '010001', 'isc_program'])
        prog_word = int.from_bytes(bytes(word), byteorder='big')
        jtag_legs.append([JtagLeg.DR, expand_binary(32, prog_word), ' '])

    for i in range(9):
        jtag_legs.append([JtagLeg.IR, '010101', 'bbkey_rbk'])
        jtag_legs.append([JtagLeg.DR, expand_binary(37, 0x1fffffffff), ' '])
        
    jtag_legs.append([JtagLeg.IR, '010110', 'isc_disable'])

    for i in range(12):
        jtag_legs.append([JtagLeg.ID, '0', '0'])    
                   
    jtag_legs.append([JtagLeg.RS, '0', 'reset'])
    for i in range(5):
        jtag_legs.append([JtagLeg.ID, '0', '0'])
    jtag_legs.append([JtagLeg.IR, '111111', 'bypass'])
    jtag_legs.append([JtagLeg.DL, '0', ' '])
    jtag_legs.append([JtagLeg.IR, '111111', 'bypass'])

    for legs in jtag_legs:
        logging.debug(legs)

    # burn the BBRAM keys by running the jtag_legs "script"
    GPIO.setmode(GPIO.BCM)

    GPIO.setup((TCK_pin, TMS_pin, TDI_pin), GPIO.OUT)
    GPIO.setup(TDO_pin, GPIO.IN)
    GPIO.setup(PRG_pin, GPIO.OUT)

    # logging.debug(jtag_legs)

    reset_fpga()
    while len(jtag_legs):
        # time.sleep(0.002) # give 2 ms between each command
        jtag_next()
        
#        while len(jtag_results):
#            result = jtag_result.pop()
            # printout happens in situ

    time.sleep(0.5)
    reset_fpga()
    GPIO.cleanup()

    print("Key burning process has concluded.")
    print("It's strongly recommended to turn off your Raspberry Pi immediately")
    print("and leave it off for several minutes so the key data in DRAM is erased.")
    print("If you're paranoid, destroy the SD card. No copies of the key were")
    print("intentionally written to the disk, but with Python/Linux you can never know.")

    exit(0)

from typing import Any, Iterable, Mapping, Optional, Set, Union
def int_to_bytes(x: int) -> bytes:
    if x != 0:
        return x.to_bytes((x.bit_length() + 7) // 8, 'big')
    else:
        return bytes(1)  # a length 1 bytes with value of 0

if __name__ == "__main__":
    main()
