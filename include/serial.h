#ifndef MYOS_SERIAL_H
#define MYOS_SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_write_char(char character);
void serial_write(const char *text);
void serial_write_hex64(uint64_t value);

int serial_input_available(void);
char serial_read_char(void);

#endif
