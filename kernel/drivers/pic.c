#include <stdint.h>

#include <arch.h>
#include <pic.h>

#define PIC1_COMMAND 0x20U
#define PIC1_DATA 0x21U
#define PIC2_COMMAND 0xA0U
#define PIC2_DATA 0xA1U

#define PIC_ICW1_ICW4 0x01U
#define PIC_ICW1_INIT 0x10U
#define PIC_ICW4_8086 0x01U
#define PIC_EOI 0x20U

static void pic_io_wait(void) {
    arch_out8(0x80U, 0U);
}

void pic_init(void) {
    arch_out8(PIC1_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    pic_io_wait();
    arch_out8(PIC2_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    pic_io_wait();

    arch_out8(PIC1_DATA, PIC_IRQ_BASE);
    pic_io_wait();
    arch_out8(PIC2_DATA, PIC_IRQ_BASE + 8U);
    pic_io_wait();

    arch_out8(PIC1_DATA, UINT8_C(1) << 2U);
    pic_io_wait();
    arch_out8(PIC2_DATA, 2U);
    pic_io_wait();

    arch_out8(PIC1_DATA, PIC_ICW4_8086);
    pic_io_wait();
    arch_out8(PIC2_DATA, PIC_ICW4_8086);
    pic_io_wait();

    /* Do not let an unimplemented device enter the kernel unexpectedly. */
    arch_out8(PIC1_DATA, UINT8_MAX);
    arch_out8(PIC2_DATA, UINT8_MAX);
}

void pic_set_mask(uint8_t irq) {
    uint16_t port;
    uint8_t line;

    if (irq >= PIC_IRQ_COUNT) {
        return;
    }
    if (irq < 8U) {
        port = PIC1_DATA;
        line = irq;
    } else {
        port = PIC2_DATA;
        line = (uint8_t)(irq - 8U);
    }
    arch_out8(port, (uint8_t)(arch_in8(port) | (uint8_t)(UINT8_C(1) << line)));
}

void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    uint8_t line;

    if (irq >= PIC_IRQ_COUNT) {
        return;
    }
    if (irq < 8U) {
        port = PIC1_DATA;
        line = irq;
    } else {
        port = PIC2_DATA;
        line = (uint8_t)(irq - 8U);
    }
    arch_out8(port, (uint8_t)(arch_in8(port) & (uint8_t)~(UINT8_C(1) << line)));
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8U) {
        arch_out8(PIC2_COMMAND, PIC_EOI);
    }
    arch_out8(PIC1_COMMAND, PIC_EOI);
}

uint16_t pic_current_mask(void) {
    const uint16_t master = arch_in8(PIC1_DATA);
    const uint16_t slave = arch_in8(PIC2_DATA);
    return (uint16_t)(master | (uint16_t)(slave << 8U));
}
