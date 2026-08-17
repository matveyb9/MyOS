#include <stdint.h>

#include <irq.h>
#include <lapic.h>
#include <pic.h>
#include <scheduler.h>

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

uint64_t *irq_dispatch(uint8_t irq, uint64_t *interrupted_context) {
    if (irq >= IRQ_COUNT) {
        return interrupted_context;
    }

    counters[irq]++;
    if (handlers[irq] != (irq_handler_t)0) {
        handlers[irq](irq);
    }
    pic_send_eoi(irq);
    lapic_send_eoi();
    if (irq == 0U) {
        return scheduler_on_timer(interrupted_context);
    }
    return interrupted_context;
}

uint64_t irq_count(uint8_t irq) {
    return irq < IRQ_COUNT ? counters[irq] : 0U;
}
