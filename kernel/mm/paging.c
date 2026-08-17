#include <stdint.h>

#include <paging.h>
#include <pmm.h>

#define PAGE_ADDRESS_MASK UINT64_C(0x000FFFFFFFFFF000)
#define PAGE_PRESENT UINT64_C(0x001)
#define PAGE_HUGE UINT64_C(0x080)
#define PAGE_TABLE_FLAGS (PAGE_PRESENT | PAGING_FLAG_WRITABLE)
#define PAGE_MMIO_FLAGS (PAGE_PRESENT | PAGING_FLAG_WRITABLE | PAGING_FLAG_WRITE_THROUGH | PAGING_FLAG_CACHE_DISABLE)
#define PAGE_TABLE_ENTRIES 512U

static uint64_t direct_map_offset;
static uint64_t active_root;
static uint64_t mapped_page_count;

static uint64_t read_cr3(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
}

static void write_cr3(uint64_t value) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(value) : "memory");
}

static void invalidate_page(uint64_t address) {
    __asm__ volatile ("invlpg (%0)" : : "r"(address) : "memory");
}

static int is_canonical(uint64_t address) {
    const uint64_t upper = address >> 48U;
    return upper == 0U || upper == UINT64_C(0xFFFF);
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

    if ((entry & PAGE_PRESENT) != 0U && (entry & PAGE_HUGE) != 0U) {
        return (uint64_t *)0;
    }
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

static uint64_t *active_pml4(void) {
    const uint64_t root = active_root != 0U ? active_root : (read_cr3() & PAGE_ADDRESS_MASK);
    return physical_to_virtual(root);
}

void paging_init(uint64_t hhdm_offset) {
    direct_map_offset = hhdm_offset;
    active_root = 0U;
    mapped_page_count = 0U;
}

int paging_take_control(void) {
    uint64_t *old_root;
    uint64_t *new_root;
    uint64_t new_root_physical;

    if (direct_map_offset == 0U) {
        return 0;
    }
    if (active_root != 0U) {
        return 1;
    }

    new_root_physical = pmm_allocate_frame();
    if (new_root_physical == PMM_INVALID_ADDRESS) {
        return 0;
    }

    old_root = physical_to_virtual(read_cr3() & PAGE_ADDRESS_MASK);
    new_root = physical_to_virtual(new_root_physical);
    for (uint64_t index = 0U; index < PAGE_TABLE_ENTRIES; index++) {
        new_root[index] = old_root[index];
    }

    active_root = new_root_physical;
    write_cr3(active_root);
    return 1;
}

int paging_map_page(uint64_t virtual_address, uint64_t physical_address, uint64_t flags) {
    uint64_t *pml4;
    uint64_t *pdpt;
    uint64_t *page_directory;
    uint64_t *page_table;
    uint64_t existing;
    const uint16_t pml4_index = (uint16_t)((virtual_address >> 39U) & UINT64_C(0x1FF));
    const uint16_t pdpt_index = (uint16_t)((virtual_address >> 30U) & UINT64_C(0x1FF));
    const uint16_t pd_index = (uint16_t)((virtual_address >> 21U) & UINT64_C(0x1FF));
    const uint16_t pt_index = (uint16_t)((virtual_address >> 12U) & UINT64_C(0x1FF));

    if (direct_map_offset == 0U || is_canonical(virtual_address) == 0
        || (virtual_address & (PAGING_PAGE_SIZE - 1U)) != 0U
        || (physical_address & (PAGING_PAGE_SIZE - 1U)) != 0U) {
        return 0;
    }

    pml4 = active_pml4();
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

    existing = page_table[pt_index];
    page_table[pt_index] = (physical_address & PAGE_ADDRESS_MASK) | PAGE_PRESENT
                           | (flags & (PAGING_FLAG_WRITABLE | PAGING_FLAG_WRITE_THROUGH
                                       | PAGING_FLAG_CACHE_DISABLE));
    if ((existing & PAGE_PRESENT) == 0U) {
        mapped_page_count++;
    }
    invalidate_page(virtual_address);
    return 1;
}

int paging_map_mmio_page(uint64_t virtual_address, uint64_t physical_address) {
    return paging_map_page(virtual_address, physical_address, PAGE_MMIO_FLAGS);
}

uint64_t paging_active_root_physical(void) {
    return active_root != 0U ? active_root : (read_cr3() & PAGE_ADDRESS_MASK);
}

uint64_t paging_mapping_count(void) {
    return mapped_page_count;
}

int paging_translate(uint64_t virtual_address, uint64_t *physical_address) {
    uint64_t *pml4;
    uint64_t *pdpt;
    uint64_t *page_directory;
    uint64_t *page_table;
    uint64_t entry;
    const uint16_t pml4_index = (uint16_t)((virtual_address >> 39U) & UINT64_C(0x1FF));
    const uint16_t pdpt_index = (uint16_t)((virtual_address >> 30U) & UINT64_C(0x1FF));
    const uint16_t pd_index = (uint16_t)((virtual_address >> 21U) & UINT64_C(0x1FF));
    const uint16_t pt_index = (uint16_t)((virtual_address >> 12U) & UINT64_C(0x1FF));

    if (physical_address == (uint64_t *)0 || direct_map_offset == 0U
        || is_canonical(virtual_address) == 0) {
        return 0;
    }

    pml4 = active_pml4();
    entry = pml4[pml4_index];
    if ((entry & PAGE_PRESENT) == 0U || (entry & PAGE_HUGE) != 0U) {
        return 0;
    }
    pdpt = physical_to_virtual(entry & PAGE_ADDRESS_MASK);
    entry = pdpt[pdpt_index];
    if ((entry & PAGE_PRESENT) == 0U || (entry & PAGE_HUGE) != 0U) {
        return 0;
    }
    page_directory = physical_to_virtual(entry & PAGE_ADDRESS_MASK);
    entry = page_directory[pd_index];
    if ((entry & PAGE_PRESENT) == 0U || (entry & PAGE_HUGE) != 0U) {
        return 0;
    }
    page_table = physical_to_virtual(entry & PAGE_ADDRESS_MASK);
    entry = page_table[pt_index];
    if ((entry & PAGE_PRESENT) == 0U) {
        return 0;
    }
    *physical_address = (entry & PAGE_ADDRESS_MASK) | (virtual_address & (PAGING_PAGE_SIZE - 1U));
    return 1;
}
