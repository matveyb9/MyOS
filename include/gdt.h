#ifndef MYOS_GDT_H
#define MYOS_GDT_H

#include <stdint.h>

#define GDT_KERNEL_CODE_SELECTOR UINT16_C(0x08)
#define GDT_KERNEL_DATA_SELECTOR UINT16_C(0x10)
#define GDT_USER_COMPAT_CODE_SELECTOR UINT16_C(0x18)
#define GDT_USER_DATA_SELECTOR UINT16_C(0x20)
#define GDT_USER_CODE_SELECTOR UINT16_C(0x28)
#define GDT_TSS_SELECTOR UINT16_C(0x30)

void gdt_init(void);
void gdt_set_kernel_stack(uint64_t stack_top);

#endif
