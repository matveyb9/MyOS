#ifndef MYOS_ACPI_H
#define MYOS_ACPI_H

#include <stdint.h>

int acpi_power_init(const void *rsdp, uint64_t hhdm_offset);
int acpi_power_is_ready(void);
void acpi_poweroff(void) __attribute__((noreturn));

#endif
