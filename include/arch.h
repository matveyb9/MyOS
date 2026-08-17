#ifndef MYOS_ARCH_H
#define MYOS_ARCH_H

#include <stdint.h>

void arch_halt(void) __attribute__((noreturn));
void arch_reboot(void) __attribute__((noreturn));
void arch_out8(uint16_t port, uint8_t value);
void arch_out16(uint16_t port, uint16_t value);
void arch_out32(uint16_t port, uint32_t value);
uint8_t arch_in8(uint16_t port);
uint32_t arch_in32(uint16_t port);
void arch_trigger_divide_by_zero(void) __attribute__((noreturn));
void arch_enable_interrupts(void);
void arch_disable_interrupts(void);
void arch_wait_for_interrupt(void);
uint64_t arch_read_rflags(void);
uint64_t arch_read_cr2(void);
uint64_t arch_read_msr(uint32_t msr);
void arch_write_msr(uint32_t msr, uint64_t value);
void arch_enter_user_mode(uint64_t entry, uint64_t stack) __attribute__((noreturn));
void arch_resume_context(uint64_t *context) __attribute__((noreturn));

#endif
