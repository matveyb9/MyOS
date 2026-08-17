#include <stdint.h>

#include <gdt.h>

struct gdt_descriptor {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_bitmap_offset;
} __attribute__((packed));

extern void arch_load_gdt(const struct gdt_descriptor *descriptor);
extern void arch_load_tss(uint16_t selector);
extern uint8_t __kernel_stack_top[];

static uint64_t gdt_entries[8] __attribute__((aligned(16)));
static struct tss64 kernel_tss;

static void set_tss_descriptor(void) {
    const uint64_t base = (uint64_t)(uintptr_t)&kernel_tss;
    const uint64_t limit = sizeof(kernel_tss) - 1U;

    gdt_entries[6] = (limit & UINT64_C(0xFFFF))
                     | ((base & UINT64_C(0xFFFFFF)) << 16U)
                     | (UINT64_C(0x89) << 40U)
                     | ((limit & UINT64_C(0xF0000)) << 32U)
                     | ((base & UINT64_C(0xFF000000)) << 32U);
    gdt_entries[7] = base >> 32U;
}

void gdt_init(void) {
    const struct gdt_descriptor descriptor = {
        .limit = (uint16_t)(sizeof(gdt_entries) - 1U),
        .base = (uint64_t)(uintptr_t)&gdt_entries[0]
    };

    gdt_entries[0] = 0x0000000000000000ULL;
    gdt_entries[1] = 0x00AF9A000000FFFFULL;
    gdt_entries[2] = 0x00CF92000000FFFFULL;
    gdt_entries[3] = 0x00CFFA000000FFFFULL;
    gdt_entries[4] = 0x00CFF2000000FFFFULL;
    gdt_entries[5] = 0x00AFFA000000FFFFULL;
    kernel_tss.rsp0 = (uint64_t)(uintptr_t)__kernel_stack_top;
    kernel_tss.io_bitmap_offset = (uint16_t)sizeof(kernel_tss);
    set_tss_descriptor();
    arch_load_gdt(&descriptor);
    arch_load_tss(GDT_TSS_SELECTOR);
}

void gdt_set_kernel_stack(uint64_t stack_top) {
    kernel_tss.rsp0 = stack_top;
}
