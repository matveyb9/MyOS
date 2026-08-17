#include <stdint.h>

#include <paging.h>
#include <pmm.h>

#define PAGE_ADDRESS_MASK UINT64_C(0x000FFFFFFFFFF000)
#define PAGE_PRESENT UINT64_C(0x001)
#define PAGE_WRITABLE UINT64_C(0x002)
#define PAGE_WRITE_THROUGH UINT64_C(0x008)
#define PAGE_CACHE_DISABLE UINT64_C(0x010)
#define PAGE_MMIO_FLAGS (PAGE_PRESENT | PAGE_WRITABLE | PAGE_WRITE_THROUGH | PAGE_CACHE_DISABLE)
#define PAGE_TABLE_FLAGS (PAGE_PRESENT | PAGE_WRITABLE)
#define PAGE_TABLE_ENTRIES 512U

static uint64_t direct_map_offset;

static uint64_t read_cr3(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
}

static void invalidate_page(uint64_t address) {
    __asm__ volatile ("invlpg (%0)" : : "r"(address) : "memory");
}

static uint64_t *physical_to_virtual(uint64_t physical_address) {
    return (uint64_t *)(uintptr_t)(direct_map_offset + physical_address);
}

static void zero_page(uint64_t physical_address) {
    uint64_t *page = physical_to_virtual(physical_address);
    for (uint64_t index = 0U; index < PAGE_TABLE_ENTRIES; index++) {
        page[index] = 0U;
    }
}

static uint64_t *next_table(uint64_t *table, uint16_t index) {
    uint64_t entry = table[index];

    if ((entry & PAGE_PRESENT) == 0U) {
        const uint64_t frame = pmm_allocate_frame();
        if (frame == PMM_INVALID_ADDRESS) {
            return (uint64_t *)0;
        }
        zero_page(frame);
        table[index] = frame | PAGE_TABLE_FLAGS;
        entry = table[index];
    }
    return physical_to_virtual(entry & PAGE_ADDRESS_MASK);
}

void paging_init(uint64_t hhdm_offset) {
    direct_map_offset = hhdm_offset;
}

int paging_map_mmio_page(uint64_t virtual_address, uint64_t physical_address) {
    uint64_t *pml4;
    uint64_t *pdpt;
    uint64_t *page_directory;
    uint64_t *page_table;
    const uint16_t pml4_index = (uint16_t)((virtual_address >> 39U) & UINT64_C(0x1FF));
    const uint16_t pdpt_index = (uint16_t)((virtual_address >> 30U) & UINT64_C(0x1FF));
    const uint16_t pd_index = (uint16_t)((virtual_address >> 21U) & UINT64_C(0x1FF));
    const uint16_t pt_index = (uint16_t)((virtual_address >> 12U) & UINT64_C(0x1FF));

    if (direct_map_offset == 0U || (virtual_address & (PAGING_PAGE_SIZE - 1U)) != 0U
        || (physical_address & (PAGING_PAGE_SIZE - 1U)) != 0U) {
        return 0;
    }

    pml4 = physical_to_virtual(read_cr3() & PAGE_ADDRESS_MASK);
    pdpt = next_table(pml4, pml4_index);
    if (pdpt == (uint64_t *)0) {
        return 0;
    }
    page_directory = next_table(pdpt, pdpt_index);
    if (page_directory == (uint64_t *)0) {
        return 0;
    }
    page_table = next_table(page_directory, pd_index);
    if (page_table == (uint64_t *)0) {
        return 0;
    }

    page_table[pt_index] = (physical_address & PAGE_ADDRESS_MASK) | PAGE_MMIO_FLAGS;
    invalidate_page(virtual_address);
    return 1;
}
