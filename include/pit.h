#ifndef MYOS_PIT_H
#define MYOS_PIT_H

#include <stdint.h>

#define PIT_FREQUENCY_HZ 100U

void pit_init(uint32_t frequency_hz);
void pit_on_irq(uint8_t irq);
uint64_t pit_ticks(void);
uint32_t pit_frequency_hz(void);

#endif
