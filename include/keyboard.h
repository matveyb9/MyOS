#ifndef MYOS_KEYBOARD_H
#define MYOS_KEYBOARD_H

#include <stdint.h>

int keyboard_init(void);
void keyboard_on_irq(uint8_t irq);
int keyboard_has_char(void);
char keyboard_read_char(void);
uint64_t keyboard_dropped_char_count(void);

#endif
