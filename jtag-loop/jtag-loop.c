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
    printf("Fatal error opening serial port\n");
    return 1;
  }

  jtagInit(jtag);
  
  unsigned char c;
  ssize_t count;
  while(1) {
    count = read(sfd, &c, 1);
      
    if( count != 1 ) {
      printf( "unexpected count return, continuing\n" );
    }
    
  }
}
