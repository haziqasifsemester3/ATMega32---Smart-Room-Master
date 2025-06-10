#ifndef Serial_H
#define Serial_H

#include <stdint.h>

#include "Serial_conf_lib.h"

void serial_init(void);
void serial_send_char(char c);
void serial_send_string(char *s);

#if SERIAL_INTERRUPT == 0 //if user doesn't want to use interrupts:
char serial_receive_char(void);
uint32_t serial_receive_string(char *stringArr, uint8_t size);
#endif

#endif
