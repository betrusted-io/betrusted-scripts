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


// Opens the specified serial port, sets it up for binary communication,
// configures its read timeouts, and sets its baud rate.
// Returns a non-negative file descriptor on success, or -1 on failure.
int open_serial_port(const char * device, uint32_t baud_rate)
{
  int fd = open(device, O_RDWR | O_NOCTTY);
  if (fd == -1)
    {
      perror(device);
      return -1;
    }

  // Flush away any bytes previously read or written.
  int result = tcflush(fd, TCIOFLUSH);
  if (result)
    {
      perror("tcflush failed");  // just a warning, not a fatal error
    }

  // Get the current configuration of the serial port.
  struct termios options;
  result = tcgetattr(fd, &options);
  if (result)
    {
      perror("tcgetattr failed");
      close(fd);
      return -1;
    }

  // Turn off any options that might interfere with our ability to send and
  // receive raw binary bytes.
  options.c_iflag &= ~(INLCR | IGNCR | ICRNL | IXON | IXOFF);
  options.c_oflag &= ~(ONLCR | OCRNL);
  options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

  // Set up timeouts: Calls to read() will return as soon as there is
  // at least one byte available or when 100 ms has passed.
  options.c_cc[VTIME] = 1;
  options.c_cc[VMIN] = 0;

  // This code only supports certain standard baud rates. Supporting
  // non-standard baud rates should be possible but takes more work.
  switch (baud_rate)
    {
    case 4800:   cfsetospeed(&options, B4800);   break;
    case 9600:   cfsetospeed(&options, B9600);   break;
    case 19200:  cfsetospeed(&options, B19200);  break;
    case 38400:  cfsetospeed(&options, B38400);  break;
    case 115200: cfsetospeed(&options, B115200); break;
    default:
      fprintf(stderr, "warning: baud rate %u is not supported, using 9600.\n",
	      baud_rate);
      cfsetospeed(&options, B9600);
      break;
    }
  cfsetispeed(&options, cfgetospeed(&options));

  result = tcsetattr(fd, TCSANOW, &options);
  if (result)
    {
      perror("tcsetattr failed");
      close(fd);
      return -1;
    }

  return fd;
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

  sfd = open_serial_port("/dev/ttyS0", 115200);

  jtagInit(jtag);
  
  unsigned char c, bits, ret;
  ssize_t count;
  
  tcflush(sfd, TCIFLUSH);   /* Discards old data in the rx buffer            */
  while(1) {
    count = read(sfd, &c, 1);
      
    if( count != 1 ) {
      continue;
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
