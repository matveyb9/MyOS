#include <stdint.h>

#include <irq.h>
#include <lapic.h>
#include <pic.h>

static irq_handler_t handlers[IRQ_COUNT];
static volatile uint64_t counters[IRQ_COUNT];

void irq_init(void) {
    for (uint8_t irq = 0U; irq < IRQ_COUNT; irq++) {
        handlers[irq] = (irq_handler_t)0;
        counters[irq] = 0U;
    }
}

void irq_register_handler(uint8_t irq, irq_handler_t handler) {
    if (irq < IRQ_COUNT) {
        handlers[irq] = handler;
    }
}

void irq_dispatch(uint8_t irq) {
    if (irq >= IRQ_COUNT) {
        return;
    }

    counters[irq]++;
    if (handlers[irq] != (irq_handler_t)0) {
        handlers[irq](irq);
    }
    pic_send_eoi(irq);
    lapic_send_eoi();
}

uint64_t irq_count(uint8_t irq) {
    return irq < IRQ_COUNT ? counters[irq] : 0U;
}
