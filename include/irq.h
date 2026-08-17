#ifndef MYOS_IRQ_H
#define MYOS_IRQ_H

#include <stdint.h>

#define IRQ_VECTOR_BASE 0x20U
#define IRQ_COUNT 16U

typedef void (*irq_handler_t)(uint8_t irq);

void irq_init(void);
void irq_register_handler(uint8_t irq, irq_handler_t handler);
uint64_t *irq_dispatch(uint8_t irq, uint64_t *interrupted_context);
uint64_t irq_count(uint8_t irq);

#endif
