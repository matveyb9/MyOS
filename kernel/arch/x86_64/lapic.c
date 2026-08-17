#include <stdint.h>

#include <lapic.h>
#include <paging.h>

#define IA32_APIC_BASE_MSR 0x1BU
#define IA32_APIC_BASE_ENABLE (UINT64_C(1) << 11U)
#define IA32_APIC_BASE_ADDRESS_MASK UINT64_C(0xFFFFF000)

#define LAPIC_REG_EOI 0x0B0U
#define LAPIC_REG_TPR 0x080U
#define LAPIC_REG_SVR 0x0F0U
#define LAPIC_REG_LVT_LINT0 0x350U

#define LAPIC_SVR_SOFTWARE_ENABLE 0x100U
#define LAPIC_SPURIOUS_VECTOR 0xFFU
#define LAPIC_LVT_DELIVERY_EXTINT 0x700U
#define LAPIC_LVT_MASK 0x10000U

static volatile uint32_t *lapic_base;

static int cpu_has_local_apic(void) {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;

    __asm__ volatile ("cpuid"
                      : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                      : "a"(1U), "c"(0U));
    (void)eax;
    (void)ebx;
    (void)ecx;
    return (edx & (UINT32_C(1) << 9U)) != 0U;
}

static uint64_t read_msr(uint32_t msr) {
    uint32_t low;
    uint32_t high;

    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32U) | low;
}

static uint32_t lapic_read(uint32_t offset) {
    return lapic_base[offset / sizeof(uint32_t)];
}

static void lapic_write(uint32_t offset, uint32_t value) {
    lapic_base[offset / sizeof(uint32_t)] = value;
    (void)lapic_read(LAPIC_REG_EOI);
}

int lapic_init(void) {
    uint64_t apic_base_msr;
    uint64_t physical_base;
    uint32_t lint0;
    uint32_t svr;

    lapic_base = (volatile uint32_t *)0;
    if (cpu_has_local_apic() == 0) {
        return 0;
    }
    apic_base_msr = read_msr(IA32_APIC_BASE_MSR);
    if ((apic_base_msr & IA32_APIC_BASE_ENABLE) == 0U) {
        return 0;
    }
    physical_base = apic_base_msr & IA32_APIC_BASE_ADDRESS_MASK;
    if (paging_map_mmio_page(PAGING_LAPIC_VIRTUAL_ADDRESS, physical_base) == 0) {
        return 0;
    }

    lapic_base = (volatile uint32_t *)(uintptr_t)PAGING_LAPIC_VIRTUAL_ADDRESS;
    lapic_write(LAPIC_REG_TPR, 0U);

    svr = lapic_read(LAPIC_REG_SVR);
    lapic_write(LAPIC_REG_SVR, (svr & UINT32_C(0xFFFFFF00))
                | LAPIC_SVR_SOFTWARE_ENABLE | LAPIC_SPURIOUS_VECTOR);

    lint0 = lapic_read(LAPIC_REG_LVT_LINT0);
    lint0 &= ~(LAPIC_LVT_DELIVERY_EXTINT | LAPIC_LVT_MASK);
    lint0 |= LAPIC_LVT_DELIVERY_EXTINT;
    lapic_write(LAPIC_REG_LVT_LINT0, lint0);
    return 1;
}

void lapic_send_eoi(void) {
    if (lapic_base != (volatile uint32_t *)0) {
        lapic_write(LAPIC_REG_EOI, 0U);
    }
}

int lapic_is_active(void) {
    return lapic_base != (volatile uint32_t *)0;
}
