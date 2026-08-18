#ifndef MYOS_MOUSE_H
#define MYOS_MOUSE_H

#include <stdint.h>

int mouse_init(void);
void mouse_on_irq(uint8_t irq);
uint64_t mouse_packet_count(void);
uint64_t mouse_dropped_packet_count(void);

#endif
