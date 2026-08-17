#include <stdint.h>

#include <arch.h>
#include <idt.h>
#include <irq.h>
#include <serial.h>

#define IDT_ENTRY_COUNT 256U
#define KERNEL_CODE_SELECTOR 0x08U
#define IDT_INTERRUPT_GATE 0x8EU
#define EXCEPTION_COUNT 32U

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_descriptor {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern void arch_load_idt(const struct idt_descriptor *descriptor);

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

static struct idt_entry idt_entries[IDT_ENTRY_COUNT] __attribute__((aligned(16)));

static void (*const exception_stubs[EXCEPTION_COUNT])(void) = {
    isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
    isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
};

static void (*const irq_stubs[IRQ_COUNT])(void) = {
    irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
    irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
};

static const char *const exception_names[EXCEPTION_COUNT] = {
    "Divide error", "Debug", "Non-maskable interrupt", "Breakpoint",
    "Overflow", "Bound range exceeded", "Invalid opcode", "Device not available",
    "Double fault", "Coprocessor segment overrun", "Invalid TSS", "Segment not present",
    "Stack-segment fault", "General protection fault", "Page fault", "Reserved",
    "x87 floating-point", "Alignment check", "Machine check", "SIMD floating-point",
    "Virtualization", "Control protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Hypervisor injection",
    "VMM communication", "Security", "Reserved"
};

static void idt_set_gate(uint8_t vector, void (*handler)(void)) {
    const uint64_t address = (uint64_t)(uintptr_t)handler;
    struct idt_entry *entry = &idt_entries[vector];

    entry->offset_low = (uint16_t)(address & 0xffffU);
    entry->selector = KERNEL_CODE_SELECTOR;
    entry->ist = 0U;
    entry->type_attributes = IDT_INTERRUPT_GATE;
    entry->offset_middle = (uint16_t)((address >> 16U) & 0xffffU);
    entry->offset_high = (uint32_t)(address >> 32U);
    entry->reserved = 0U;
}

void idt_init(void) {
    for (uint8_t vector = 0U; vector < EXCEPTION_COUNT; vector++) {
        idt_set_gate(vector, exception_stubs[vector]);
    }

    const struct idt_descriptor descriptor = {
        .limit = (uint16_t)(sizeof(idt_entries) - 1U),
        .base = (uint64_t)(uintptr_t)&idt_entries[0]
    };
    arch_load_idt(&descriptor);
}

void idt_install_irq_gates(void) {
    for (uint8_t irq = 0U; irq < IRQ_COUNT; irq++) {
        idt_set_gate((uint8_t)(IRQ_VECTOR_BASE + irq), irq_stubs[irq]);
    }
}

void idt_handle_exception(unsigned long vector, unsigned long error_code,
                          unsigned long instruction_pointer) {
    serial_write("\n\n*** KERNEL EXCEPTION ***\n");
    serial_write("Vector: ");
    serial_write_hex64((uint64_t)vector);
    if (vector < EXCEPTION_COUNT) {
        serial_write(" (");
        serial_write(exception_names[vector]);
        serial_write(")");
    }
    serial_write("\nError code: ");
    serial_write_hex64((uint64_t)error_code);
    serial_write("\nInstruction pointer: ");
    serial_write_hex64((uint64_t)instruction_pointer);
    serial_write("\nSystem halted to preserve diagnostic state.\n");
    arch_halt();
}
