#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "rpi.h"

#define J_TCK  4
#define J_TDI  27
#define J_TDO  22
#define J_TMS  17
#define J_RESET 24
#define J_SEL  18

enum jtag_pin {
  JT_TCK,
  JT_TDI,
  JT_TDO,
  JT_TMS,
  JT_RESET,
  JT_SEL,
};

struct ff_jtag {
  struct {
    int tck;
    int tdi;
    int tdo;
    int tms;
    int reset;
    int sel;
  } pins;
};

void jtagSetPin(struct ff_jtag *jtag, enum jtag_pin pin, int val) {
	switch (pin) {
	case JT_TCK: jtag->pins.tck = val; break;
        case JT_TDI: jtag->pins.tdi = val; break;
        case JT_TDO: jtag->pins.tdo = val; break;
        case JT_TMS: jtag->pins.tms = val; break;
        case JT_RESET: jtag->pins.reset = val; break;
	case JT_SEL: jtag->pins.sel = val; break;
	default: fprintf(stderr, "unrecognized pin: %d\n", pin); break;
	}
}
  
struct ff_jtag *jtagAlloc(void) {
  struct ff_jtag *jtag = (struct ff_jtag *)malloc(sizeof(struct ff_jtag));
  memset(jtag, 0, sizeof(*jtag));
  return jtag;
}

void jtagInit(struct ff_jtag *jtag) {
  gpioSetMode(jtag->pins.tck, PI_OUTPUT);
  gpioSetMode(jtag->pins.tdi, PI_OUTPUT);  // data input to fpga from pi
  gpioSetMode(jtag->pins.tdo, PI_INPUT);   // data output from fpga into pi
  gpioSetMode(jtag->pins.tms, PI_OUTPUT);
  // gpioSetMode(jtag->pins.reset, PI_OUTPUT);
  gpioSetMode(jtag->pins.sel, PI_OUTPUT);
  
  gpioWrite(jtag->pins.tck, 0);
  gpioWrite(jtag->pins.tdi, 0);
  gpioWrite(jtag->pins.tms, 0);
  // gpioWrite(jtag->pins.reset, 1);
  gpioWrite(jtag->pins.sel, 1);  // select the Spartan7 UART
}

// pin mappings:  tck  tms  tdi
#define  MASK_TCK  0x4
#define  MASK_TMS  0x2
#define  MASK_TDI  0x1

void jtagPause(struct ff_jtag *jtag) {
	(void)jtag;
//	usleep(1);
	return;
}

int main(int argc, char **argv) {
  (void) argc;
  (void) argv;
  struct ff_jtag *jtag;
  int sfd;

  if (gpioInitialise() < 0) {
    fprintf(stderr, "Unable to initialize GPIO\n");
    return 1;
  }
  jtag = jtagAlloc();

  jtagSetPin(jtag, JT_TCK, J_TCK);
  jtagSetPin(jtag, JT_TDI, J_TDI);
  jtagSetPin(jtag, JT_TDO, J_TDO);
  jtagSetPin(jtag, JT_TMS, J_TMS);
  jtagSetPin(jtag, JT_RESET, J_RESET);
  jtagSetPin(jtag, JT_SEL, J_SEL);
  
  sfd = open("/dev/ttyS0", O_RDWR);

  if( sfd == -1 ) {
    fprintf( stderr, "Fatal error opening serial port\n");
    return 1;
  }

  jtagInit(jtag);
  
  unsigned char c, bits, ret;
  ssize_t count;
  while(1) {
    count = read(sfd, &c, 1);
      
    if( count != 1 ) {
      fprintf( stderr, "unexpected count return, continuing\n" );
    }

    // characters starting at offset '@' are direct set of pin state (including clock pin)
    // a read data is done immediately upon conclusion of setting pin state: we count on the FPGA being fast
    
    // characters starting at offset '`' are clocked set of pin state (clock pin spec is ignored)
    // this is: set data, rising edge, falling edge, read data
    
    // every charater sent returns a character which is the line state at the conclusion of the command
    bits = 0xf & c;
    if ((c & 0xf0) == 0x40) {  // 0x40 = '@'
      bits & MASK_TMS ? gpioWrite(jtag->pins.tms, 1) : gpioWrite(jtag->pins.tms, 0);
      bits & MASK_TDI ? gpioWrite(jtag->pins.tdi, 1) : gpioWrite(jtag->pins.tdi, 0);
      bits & MASK_TCK ? gpioWrite(jtag->pins.tck, 1) : gpioWrite(jtag->pins.tck, 0);

      gpioRead(jtag->pins.tck);
      jtagPause(jtag);

      ret = gpioRead(jtag->pins.tdo) ? '1' : '0';
    } else if(( c & 0xf0) == 0x60) { // 0x60 = '`'
      bits & MASK_TMS ? gpioWrite(jtag->pins.tms, 1) : gpioWrite(jtag->pins.tms, 0);
      bits & MASK_TDI ? gpioWrite(jtag->pins.tdi, 1) : gpioWrite(jtag->pins.tdi, 0);

      gpioWrite(jtag->pins.tck, 1);
      gpioRead(jtag->pins.tck);
      jtagPause(jtag);
      
      gpioWrite(jtag->pins.tck, 0);
      gpioRead(jtag->pins.tck);
      jtagPause(jtag);
      
      ret = gpioRead(jtag->pins.tdo) ? '1' : '0';
    }

    // return the state of the TDO pin
    if( write(sfd, &ret, 1) != 1 ) {
      fprintf(stderr, "return write failed\n");
    }
  }
}
