#include <stdint.h>

#include <gdt.h>

struct gdt_descriptor {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern void arch_load_gdt(const struct gdt_descriptor *descriptor);

static uint64_t gdt_entries[] __attribute__((aligned(16))) = {
    0x0000000000000000ULL, /* null */
    0x00AF9A000000FFFFULL, /* kernel code: selector 0x08 */
    0x00CF92000000FFFFULL, /* kernel data: selector 0x10 */
    0x00AFFA000000FFFFULL, /* user code: selector 0x18, RPL 3 => 0x1B */
    0x00CFF2000000FFFFULL  /* user data: selector 0x20, RPL 3 => 0x23 */
};

void gdt_init(void) {
    const struct gdt_descriptor descriptor = {
        .limit = (uint16_t)(sizeof(gdt_entries) - 1U),
        .base = (uint64_t)(uintptr_t)&gdt_entries[0]
    };

    arch_load_gdt(&descriptor);
}
