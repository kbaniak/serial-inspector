/*
 ============================================================================
 serial-inspector
 (C) 2018 Krystian Baniak, <krystian.baniak@exios.pl>
 license: MIT
 purpose: test application for serial port inspection for embedded devices
 ============================================================================
*/

#define _XOPEN_SOURCE

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>

#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <stdbool.h>

#include "sio.h"
#include "colors.h"

#define RSCOM_PORT   "/dev/ttyUSB0"

#define MAX_CMD                      16
#define VERS                    "1.0.4"
#define MAXSYSLOG_MESG_LEN          256

 char *optarg;

/* GLOBALS */
static int fd = -1;        /* serial tty descriptors */
bool useSyslog = false;    /* use system log */
int severity = LOG_INFO;   /* default severity */
bool hexDump = false;      /* enable hex dump for received data */
bool hexFlags = 0;
bool modeQuiet = false;
int crmode = 0;
int cbaud = B9600;
const char* crconf[] = { "\r", "\n", "\r\n" };

/* main code entry */
int
main(int argc, char *argv[])
{
  int rr, ii, maxfd, opt;
  fd_set rfds;
  struct timeval tv;
  char buff[256];
  char sdevice[256];
  char sdcommand[256];
  bool hasColors = false;
  bool isTerminal = isatty(STDOUT_FILENO);
  bool isSingleCmd = false;


  if (isTerminal) {
    /* assume we have colors */
    hasColors = true;
  }

  /* opts */
  memset(sdevice, 0, sizeof(sdevice));
  memset(sdcommand, 0, sizeof(sdcommand));
  snprintf(sdevice, 255, "%s", RSCOM_PORT);

  while ((opt = getopt(argc, argv, "vdp:b:c:l:xhqs")) != -1) {
    switch (opt) {
      case 'x':
        hexDump = true;
        break;
      case 'd':
        severity = LOG_DEBUG;
        logg(LOG_INFO, "+  running with full syslog verbosity (!)");
        break;
      case 'p':
        logg(LOG_INFO, "+ using serial device: %s", optarg);
        snprintf(sdevice, 255, "%s", optarg);
        break;
      case 'q':
        modeQuiet = true;
        break;
      case 's':
        useSyslog = true;
        break;
      case 'c':
        {
          if (optarg) {
            snprintf(sdcommand, 255, "%s", optarg);
            isSingleCmd = true;
          }
        }
        break;
      case 'b':
        {
          long int ubd = 0;
          errno = 0;
          ubd = strtol(optarg, NULL, 10);
          if ((errno == ERANGE && (ubd == LONG_MAX || ubd == LONG_MIN)) || (errno != 0 && ubd == 0)) {
            logg(LOG_INFO, "- malformed baud rate given!");
            exit(EXIT_FAILURE);
          }
          switch (ubd) {
            case 9600:
              cbaud = B9600;
              break;
            case 19200:
              cbaud = B19200;
              break;
            case 38400:
              cbaud = B38400;
              break;
            case 57600:
              cbaud = B57600;
              break;
            default:
              logg(LOG_INFO, "- unsupported baud rate: B%d", (int) ubd);
          }
          logg(LOG_INFO, "+ using baud rate: B%d", (int) ubd);
        }
        break;
      case 'l':
        {
          if (optarg) {
            if (strcmp(optarg, "cr") == 0) {
              crmode = 0;
            }
            if (strcmp(optarg, "lf") == 0) {
              crmode = 1;
            }
            if (strcmp(optarg, "crlf") == 0) {
              crmode = 2;
            }
          }
        }
        break;
      case 'v':
        logg(LOG_INFO, "* serial port debugger %s\n(C) Krystian Baniak 2018", VERS);
        exit(EXIT_SUCCESS);
      case 'h':
      default: /* '?' */
        show_usage(argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  logg(LOG_INFO, "(*) Staring serial port protocol debugger %s. isTerm(%d)", VERS, isTerminal);
  fd = setup_siobus(sdevice, cbaud);

  if (fd == -1) {
    logg(LOG_ERR, "! can not open serial port handle: %s", sdevice);
    exit(EXIT_FAILURE);
  }

  /* if we are used in a batch mode we signal that we are ready */
  fprintf(stdout, "READY\n");
  fflush(stdout);

  if (isSingleCmd) {
    dprintf(fd,"%s%s", sdcommand, crconf[crmode]);
  }

  /* main loop */
  for (;;) {
    /* stdin is fd: 0 */
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);  // serial
    FD_SET(0,  &rfds);  // stdin

    tv.tv_sec  = 1;
    tv.tv_usec = 0;

    /* close stale sockets */
    time_t t_now;
    time(&t_now);

    /* calculate maximum fd value to use in select */
    maxfd = fd + 1;

    /* zeroize buffer */
    memset(&buff,0,sizeof(buff));
    if ((rr = select(maxfd, &rfds, NULL, NULL, &tv)) == -1) {
      /* error */
      if (errno == EINTR) {
        continue;
      } else {
        logg(LOG_ERR, "! select() failed: %d %s", rr, strerror(errno));
        goto __finally;
      }
    } else if (rr) {

      /* data is available */
      if (FD_ISSET(0, &rfds)){
        /* command from console */
        memset(&buff, 0, sizeof(buff));
        ii = read(0, &buff, 255);
        if (strncmp(buff,"quit", 4)==0){
          logg(LOG_INFO, "quiting on a user request\n");
          goto __finally;
        } else if (strncmp(buff, ".acode on\n",255)==0) {
          hexFlags |= SIO_ASCII_SPECIAL;
        } else if (strncmp(buff, ".acode off\n",255)==0) {
          hexFlags &= ~SIO_ASCII_SPECIAL;
        } else if (strncmp(buff, ".gs\n", 255)==0) {
          dprintf(fd,"%s%s", "at!gstatus?", crconf[crmode]);
        } else {
          buff[ii-1] = 0x00;
          dprintf(fd,"%s%s", buff, crconf[crmode]);
        }
      }

      /* data waiting on the serial device buffer */
      if (FD_ISSET(fd, &rfds)){
        memset(&buff, 0, sizeof(buff));
        ii = read(fd, &buff, 255);
        if (hexDump)
          pddump(buff, ii, hexFlags, hasColors);
        if (hasColors) {
          printf("%s%s%s", YELLOW_TEXT, buff, COLOR_RESET);
        } else {
          printf("%s", buff);
        }
        if (isSingleCmd && strncmp(buff + (ii-4),"OK", 2)==0) {
          goto __finally;
        }
      }
    }

  }

__finally:
  close(fd);
  exit(EXIT_SUCCESS);
}

/* help instructions */
void
show_usage(const char *progname)
{
  fprintf(stdout, "%s(C) 2018 Krystian Baniak, Exios Consulting%s\nUsage: %s [opts] -p /dev/ttyPort\n", BOLD, COLOR_RESET, progname);
  fprintf(stdout, "[options]:\n");
  fprintf(stdout, "  -d -- enable a debug mode\n");
  fprintf(stdout, "  -q -- enable a quiet mode\n");
  fprintf(stdout, "  -s -- enable logging to system logs\n");
  fprintf(stdout, "  -x -- enable a hex dump of received payloads\n");
  fprintf(stdout, "  -v -- print version and exit\n");
  fprintf(stdout, "[options with parameters]\n");
  fprintf(stdout, "  -b 9600|19200|38400|57600 -- baud rate\n");
  fprintf(stdout, "  -l cr|lf|crlf             -- configure default line endiing\n");
  fprintf(stdout, "  -c [command]              -- execute AT command or a macro\n");
}

/* delay seconds using float input */
void
_delay(float sec)
{
  unsigned int usec = (unsigned int) (1e6 * sec);
  usleep(usec);
}

char*
wlogTime(char *buf)
{
  struct tm lc;
  struct timeval __tm;

  gettimeofday(&__tm, NULL);
  localtime_r( &__tm.tv_sec, &lc );
  sprintf( buf, "%04d-%02d-%02d %02d:%02d:%02d.%06lu", lc.tm_year + 1900, lc.tm_mon + 1,
      lc.tm_mday, lc.tm_hour, lc.tm_min, lc.tm_sec, __tm.tv_usec );

  return buf;
}

/* logger function */
void
logg(int ilev, char * mesg, ...)
{
  char    buff[MAXSYSLOG_MESG_LEN];
  char    stmp[32];
  va_list al;

  /* prepare message */
  va_start(al, mesg);
  vsnprintf(buff, MAXSYSLOG_MESG_LEN - 1, mesg, al);
  va_end(al);

  /* write to console */
  if (!modeQuiet)
    fprintf(stderr, "%s: [%d] %s \n", wlogTime(stmp), ilev, buff);
  /* write to syslog
   * LOG_LOCAL0 == 16
   */
  if (useSyslog && (ilev <= severity))
    syslog(LOG_MAKEPRI(16, ilev), "%s", buff);
}


/* setup serial port device */
int
setup_serial(int fd, int baud)
{
  struct termios config;

  if (!isatty(fd)) {
    logg(LOG_ERR, "! not a tty device");
    return -1;
  }

  if (tcgetattr(fd, &config) < 0) {
    logg(LOG_ERR, "! can not get current config");
    return -1;
  }

  /* baudrate $baud bps, 8N1,
   * ignore modem status lines,
   * hang up on last close and disable other flags
   * */
  config.c_cflag = (baud | CS8 | CREAD | CLOCAL | HUPCL);
  config.c_oflag = 0;
  config.c_iflag = 0;
  config.c_lflag = ICANON;

  if (tcsetattr(fd, TCSAFLUSH, &config) < 0) {
    logg(LOG_ERR, "! UART config failed");
    return -1;
  }

  return 0;
}

/* configure serial device */
int
setup_siobus(const char *sdev, int baud)
{
  int fd;
  /* open serial port descriptor */
  fd = open(sdev, O_RDWR | O_NOCTTY | O_NDELAY);
  if (fd == -1) {
    logg(LOG_ERR, "! can not open %s", sdev);
    return -1;
  }
  if (setup_serial(fd, baud) != 0) {
    logg(LOG_ERR, "! can not configure %s", sdev);
    return -1;
  }
  return fd;
}

void
acode_dump(const char *pstr, unsigned int isize)
{
  const char* _arr[33] = {
    "NUL", "SOH", "STX", "ETX", "EQT", "ENQ", "ACK", "BEL", " BS", "TAB", " LF", " VT", " FF",
    " CR", " SO", " SI", "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB", "CAN", " EM",
    "SUB", "ESC", " FS", " GS", " RS", " US"
  };
  unsigned char  ind;

  if (pstr && isize > 0){
    for (int ii=0; ii<isize; ii++) {
      ind = (unsigned char) pstr[ii];
      if ( ind < 32 )
        printf("%s ", _arr[ ind ]);
      else
        printf("___ ");
    }
  }
}

void
pddump(const char *pstr, unsigned int isize, uint8_t flags, bool colors)
{
  int bare = 16;

  if (pstr && isize > 0){
     printf("--[ payload: (%6d Bytes) ]-------------------------------------\n", isize);
     printf("       0    2    4    6    8    10   12   14      ASCII:\n"
            "------------------------------------------------------------------\n");
     if (colors)
       printf("%s", BLUE_TEXT);
     for (int ii=0; ii< (isize / bare)+1; ii++) {
       printf("%.4d: ", ii*bare);
       /* hex dump */
       for (int jj=0; jj<bare; jj++) {
         if (jj%2==0)
           printf(" ");
         if ((ii*bare)+jj < isize)
           printf("%.2hhx", pstr[ii*bare+jj]);
         else
           printf("  ");
       }
       printf("    ");
       /* ascii dump */
       for (int jj=0; jj<bare; jj++) {
         if ((ii*bare)+jj < isize)
           printf("%c", (pstr[ii*bare+jj] >= 32 && pstr[ii*bare+jj]<127) ? pstr[ii*bare+jj] : '.');
         else
           printf(" ");
       }
       if (flags & SIO_ASCII_SPECIAL) {
         printf("    ");
         acode_dump(pstr + ii*bare, (ii*bare+16 < isize) ? 16 : isize - ii*bare);
       }
       printf("\n");
     }
     if (colors)
       printf("%s", COLOR_RESET);
     printf("------------------------------------------------------------------\n");
     printf("\n");
   }
}


