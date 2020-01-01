#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "rpi.h"

#define J_TCK  4
#define J_TDI  27
#define J_TDO  22
#define J_TMS  17
#define J_RESET 24
#define J_SEL  18

#define DEBUG_JTAG 0
#define DEBUG_TDO  1

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
  
  sfd = open("/dev/ttyS0", O_RDWR | O_NOCTTY);

  if( sfd == -1 ) {
    fprintf( stderr, "Fatal error opening serial port\n");
    return 1;
  }

  /*---------- Setting the Attributes of the serial port using termios structure --------- */

  struct termios SerialPortSettings;/* Create the structure                          */

  tcgetattr(sfd, &SerialPortSettings);/* Get the current attributes of the Serial port */

  /* Setting the Baud rate */
  cfsetispeed(&SerialPortSettings,B115200); /* Set Read  Speed as 115200                       */
  cfsetospeed(&SerialPortSettings,B115200); /* Set Write Speed as 115200                       */

  /* 8N1 Mode */
  SerialPortSettings.c_cflag &= ~PARENB;   /* Disables the Parity Enable bit(PARENB),So No Parity   */
  SerialPortSettings.c_cflag &= ~CSTOPB;   /* CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit */
  SerialPortSettings.c_cflag &= ~CSIZE; /* Clears the mask for setting the data size             */
  SerialPortSettings.c_cflag |=  CS8;      /* Set the data bits = 8                                 */

  SerialPortSettings.c_cflag &= ~CRTSCTS;       /* No Hardware flow Control                         */
  SerialPortSettings.c_cflag |= CREAD | CLOCAL; /* Enable receiver,Ignore Modem Control lines       */


  SerialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY);          /* Disable XON/XOFF flow control both i/p and o/p */
  SerialPortSettings.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);  /* Non Cannonical mode                            */

  SerialPortSettings.c_oflag &= ~OPOST;/*No Output Processing*/

  /* Setting Time outs */
  SerialPortSettings.c_cc[VMIN] = 1; /* Read at least 1 characters */
  SerialPortSettings.c_cc[VTIME] = 0; /* Wait indefinetly   */


  if((tcsetattr(sfd,TCSANOW,&SerialPortSettings)) != 0) /* Set the attributes to the termios structure*/
    fprintf(stderr, "\n  ERROR ! in Setting attributes");
  /*  else
      printf("\n  BaudRate = 115200 \n  StopBits = 1 \n  Parity   = none");*/


  jtagInit(jtag);
  
  unsigned char c, bits, ret;
  ssize_t count;
  
  tcflush(sfd, TCIFLUSH);   /* Discards old data in the rx buffer            */
  while(1) {
    count = read(sfd, &c, 1);
      
    if( count != 1 ) {
      fprintf( stderr, "unexpected count return, continuing\n" );
    }
    if( c == 0 ) {
      // ignore null reads
      continue;
    }
    //putchar(c);
    //fflush(stdout);

    // characters starting at offset '@' are direct set of pin state (including clock pin)
    // a read data is done immediately upon conclusion of setting pin state: we count on the FPGA being fast
    
    // characters starting at offset '`' are clocked set of pin state (clock pin spec is ignored)
    // this is: set data, rising edge, falling edge, read data
    
    // every charater sent returns a character which is the line state at the conclusion of the command
    bits = 0xf & c;
    if ((c & 0xf0) == 0x40) {  // 0x40 = '@'
      if (DEBUG_JTAG) {
	putchar('*');
	fflush(stdout);
      }

      bits & MASK_TMS ? gpioWrite(jtag->pins.tms, 1) : gpioWrite(jtag->pins.tms, 0);
      bits & MASK_TDI ? gpioWrite(jtag->pins.tdi, 1) : gpioWrite(jtag->pins.tdi, 0);

      gpioRead(jtag->pins.tck);
      jtagPause(jtag);
      
      bits & MASK_TCK ? gpioWrite(jtag->pins.tck, 1) : gpioWrite(jtag->pins.tck, 0);

      gpioRead(jtag->pins.tck);
      jtagPause(jtag);

      ret = gpioRead(jtag->pins.tdo) ? '1' : '0';
    } else if(( c & 0xf0) == 0x60) { // 0x60 = '`'
      bits & MASK_TMS ? gpioWrite(jtag->pins.tms, 1) : gpioWrite(jtag->pins.tms, 0);
      bits & MASK_TDI ? gpioWrite(jtag->pins.tdi, 1) : gpioWrite(jtag->pins.tdi, 0);

      if (DEBUG_JTAG) {
	if( bits & MASK_TMS ) { putchar('M'); } else { putchar('m'); }
	if( bits & MASK_TDI ) { putchar('D'); } else { putchar('d'); }
	fflush(stdout);
      }

      // sample the TDO going *before* pulsing clock: this is because when we are in
      // the "shift" state, TDO is immediately valid.
      ret = gpioRead(jtag->pins.tdo) ? '1' : '0';
      
      // 3.00ns/2.00ns set/hold on TDI/TMS to jtag rising edge
      gpioRead(jtag->pins.tck);
      jtagPause(jtag);

      gpioWrite(jtag->pins.tck, 1);
      // 7.5ns min high pulse width NOM
      gpioRead(jtag->pins.tck);
      jtagPause(jtag);
      
      gpioWrite(jtag->pins.tck, 0);
      // 7.00ns TCK falling edge to tdo valid
      gpioRead(jtag->pins.tck);
      jtagPause(jtag);

      if (DEBUG_JTAG) {
	putchar(ret);
	fflush(stdout);
      }
    } else {
      if (DEBUG_JTAG) {
	printf("%02x ", c);
	fflush(stdout);
      }
    }


    if (DEBUG_TDO) {
      putchar(ret);
      fflush(stdout);
    }
    
    // return the state of the TDO pin
    if( write(sfd, &ret, 1) != 1 ) {
      fprintf(stderr, "return write failed\n");
    }
    syncfs(sfd);
  }
}
