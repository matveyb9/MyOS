#ifndef MYOS_ARCH_H
#define MYOS_ARCH_H

#include <stdint.h>

void arch_halt(void) __attribute__((noreturn));
void arch_out8(uint16_t port, uint8_t value);
uint8_t arch_in8(uint16_t port);
void arch_trigger_divide_by_zero(void) __attribute__((noreturn));

#endif
