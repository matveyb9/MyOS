#ifndef MYOS_LAPIC_H
#define MYOS_LAPIC_H

#include <stdint.h>

int lapic_init(void);
void lapic_send_eoi(void);
int lapic_is_active(void);

#endif
