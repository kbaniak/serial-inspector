#ifndef _SIO_H
#define _SIO_H

#include <stdint.h>
#include <stdbool.h>

#define SIO_ASCII_SPECIAL 0x1

#ifdef __cplusplus
extern "C" {
#endif

  /* ------------------------ */

  void  _delay(float sec);
  char* wlogTime(char *buf);
  void  logg(int ilev, char * mesg, ...);
  int   setup_serial(int fd, int baud);
  int   setup_siobus(const char *sdev, int baud);
  void  acode_dump(const char *pstr, unsigned int isize);
  void  pddump(const char *pstr, unsigned int isize, uint8_t flags, bool colors);

  /* ------------------------ */

#ifdef __cplusplus
}
#endif

#endif
