#include <stdint.h>

#include <paging.h>
#include <pmm.h>

#define PAGE_ADDRESS_MASK UINT64_C(0x000FFFFFFFFFF000)
#define PAGE_PRESENT UINT64_C(0x001)
#define PAGE_HUGE UINT64_C(0x080)
#define PAGE_MANAGED UINT64_C(0x200)
#define PAGE_GUARD UINT64_C(0x400)
#define PAGE_TABLE_FLAGS (PAGE_PRESENT | PAGING_FLAG_WRITABLE)
#define PAGE_MMIO_FLAGS (PAGE_PRESENT | PAGING_FLAG_WRITABLE | PAGING_FLAG_WRITE_THROUGH | PAGING_FLAG_CACHE_DISABLE)
#define PAGE_TABLE_ENTRIES 512U
#define PAGE_LEAF_FLAGS (PAGING_FLAG_WRITABLE | PAGING_FLAG_USER | PAGING_FLAG_WRITE_THROUGH | PAGING_FLAG_CACHE_DISABLE)

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

static uint64_t page_table_flags(uint64_t leaf_flags) {
    return PAGE_TABLE_FLAGS | (leaf_flags & PAGING_FLAG_USER);
}

static uint64_t *next_table(uint64_t *table, uint16_t index, uint64_t leaf_flags) {
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
        table[index] = frame | page_table_flags(leaf_flags);
        entry = table[index];
    } else if ((leaf_flags & PAGING_FLAG_USER) != 0U) {
        table[index] |= PAGING_FLAG_USER;
        entry = table[index];
    }
    return physical_to_virtual(entry & PAGE_ADDRESS_MASK);
}

static uint64_t *existing_table(uint64_t *table, uint16_t index) {
    const uint64_t entry = table[index];

    if ((entry & PAGE_PRESENT) == 0U || (entry & PAGE_HUGE) != 0U) {
        return (uint64_t *)0;
    }
    return physical_to_virtual(entry & PAGE_ADDRESS_MASK);
}

static int table_is_empty(const uint64_t *table) {
    for (uint64_t index = 0U; index < PAGE_TABLE_ENTRIES; index++) {
        if (table[index] != 0U) {
            return 0;
        }
    }
    return 1;
}

static uint64_t *active_pml4(void) {
    return physical_to_virtual(read_cr3() & PAGE_ADDRESS_MASK);
}

static int page_address_is_valid(uint64_t virtual_address) {
    return direct_map_offset != 0U && is_canonical(virtual_address) != 0
        && (virtual_address & (PAGING_PAGE_SIZE - 1U)) == 0U;
}

static int page_address_is_protected(uint64_t virtual_address) {
    return virtual_address == PAGING_LAPIC_VIRTUAL_ADDRESS;
}

static void page_indices(uint64_t virtual_address, uint16_t *pml4_index, uint16_t *pdpt_index,
                         uint16_t *pd_index, uint16_t *pt_index) {
    *pml4_index = (uint16_t)((virtual_address >> 39U) & UINT64_C(0x1FF));
    *pdpt_index = (uint16_t)((virtual_address >> 30U) & UINT64_C(0x1FF));
    *pd_index = (uint16_t)((virtual_address >> 21U) & UINT64_C(0x1FF));
    *pt_index = (uint16_t)((virtual_address >> 12U) & UINT64_C(0x1FF));
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
    uint16_t pml4_index;
    uint16_t pdpt_index;
    uint16_t pd_index;
    uint16_t pt_index;

    if (page_address_is_valid(virtual_address) == 0
        || (physical_address & (PAGING_PAGE_SIZE - 1U)) != 0U) {
        return 0;
    }

    page_indices(virtual_address, &pml4_index, &pdpt_index, &pd_index, &pt_index);
    pml4 = active_pml4();
    pdpt = next_table(pml4, pml4_index, flags);
    if (pdpt == (uint64_t *)0) {
        return 0;
    }
    page_directory = next_table(pdpt, pdpt_index, flags);
    if (page_directory == (uint64_t *)0) {
        return 0;
    }
    page_table = next_table(page_directory, pd_index, flags);
    if (page_table == (uint64_t *)0) {
        return 0;
    }

    existing = page_table[pt_index];
    page_table[pt_index] = (physical_address & PAGE_ADDRESS_MASK) | PAGE_PRESENT | PAGE_MANAGED
                           | (flags & PAGE_LEAF_FLAGS);
    if ((existing & PAGE_PRESENT) == 0U) {
        mapped_page_count++;
    }
    invalidate_page(virtual_address);
    return 1;
}

int paging_unmap_page(uint64_t virtual_address) {
    uint64_t *pml4;
    uint64_t *pdpt;
    uint64_t *page_directory;
    uint64_t *page_table;
    uint64_t entry;
    uint16_t pml4_index;
    uint16_t pdpt_index;
    uint16_t pd_index;
    uint16_t pt_index;

    if (page_address_is_valid(virtual_address) == 0 || page_address_is_protected(virtual_address) != 0) {
        return 0;
    }

    page_indices(virtual_address, &pml4_index, &pdpt_index, &pd_index, &pt_index);
    pml4 = active_pml4();
    pdpt = existing_table(pml4, pml4_index);
    if (pdpt == (uint64_t *)0) {
        return 0;
    }
    page_directory = existing_table(pdpt, pdpt_index);
    if (page_directory == (uint64_t *)0) {
        return 0;
    }
    page_table = existing_table(page_directory, pd_index);
    if (page_table == (uint64_t *)0) {
        return 0;
    }

    entry = page_table[pt_index];
    if ((entry & PAGE_MANAGED) == 0U) {
        return 0;
    }
    page_table[pt_index] = 0U;
    if ((entry & PAGE_PRESENT) != 0U && mapped_page_count != 0U) {
        mapped_page_count--;
    }
    invalidate_page(virtual_address);

    if (table_is_empty(page_table) != 0) {
        const uint64_t page_table_frame = page_directory[pd_index] & PAGE_ADDRESS_MASK;
        page_directory[pd_index] = 0U;
        (void)pmm_free_frame(page_table_frame);
        if (table_is_empty(page_directory) != 0) {
            const uint64_t page_directory_frame = pdpt[pdpt_index] & PAGE_ADDRESS_MASK;
            pdpt[pdpt_index] = 0U;
            (void)pmm_free_frame(page_directory_frame);
            if (table_is_empty(pdpt) != 0) {
                const uint64_t pdpt_frame = pml4[pml4_index] & PAGE_ADDRESS_MASK;
                pml4[pml4_index] = 0U;
                (void)pmm_free_frame(pdpt_frame);
            }
        }
    }
    return 1;
}

int paging_map_guard(uint64_t virtual_address) {
    uint64_t *pml4;
    uint64_t *pdpt;
    uint64_t *page_directory;
    uint64_t *page_table;
    uint64_t existing;
    uint16_t pml4_index;
    uint16_t pdpt_index;
    uint16_t pd_index;
    uint16_t pt_index;

    if (page_address_is_valid(virtual_address) == 0 || page_address_is_protected(virtual_address) != 0) {
        return 0;
    }

    page_indices(virtual_address, &pml4_index, &pdpt_index, &pd_index, &pt_index);
    pml4 = active_pml4();
    pdpt = next_table(pml4, pml4_index, 0U);
    if (pdpt == (uint64_t *)0) {
        return 0;
    }
    page_directory = next_table(pdpt, pdpt_index, 0U);
    if (page_directory == (uint64_t *)0) {
        return 0;
    }
    page_table = next_table(page_directory, pd_index, 0U);
    if (page_table == (uint64_t *)0) {
        return 0;
    }

    existing = page_table[pt_index];
    if ((existing & PAGE_PRESENT) == 0U && (existing & PAGE_GUARD) != 0U) {
        return 1;
    }
    page_table[pt_index] = PAGE_MANAGED | PAGE_GUARD;
    if ((existing & PAGE_PRESENT) != 0U && (existing & PAGE_MANAGED) != 0U && mapped_page_count != 0U) {
        mapped_page_count--;
    }
    invalidate_page(virtual_address);
    return 1;
}

int paging_is_guard_page(uint64_t virtual_address) {
    uint64_t *pml4;
    uint64_t *pdpt;
    uint64_t *page_directory;
    uint64_t *page_table;
    uint64_t entry;
    uint16_t pml4_index;
    uint16_t pdpt_index;
    uint16_t pd_index;
    uint16_t pt_index;

    if (page_address_is_valid(virtual_address) == 0) {
        return 0;
    }

    page_indices(virtual_address, &pml4_index, &pdpt_index, &pd_index, &pt_index);
    pml4 = active_pml4();
    pdpt = existing_table(pml4, pml4_index);
    if (pdpt == (uint64_t *)0) {
        return 0;
    }
    page_directory = existing_table(pdpt, pdpt_index);
    if (page_directory == (uint64_t *)0) {
        return 0;
    }
    page_table = existing_table(page_directory, pd_index);
    if (page_table == (uint64_t *)0) {
        return 0;
    }
    entry = page_table[pt_index];
    return (entry & PAGE_PRESENT) == 0U && (entry & PAGE_GUARD) != 0U;
}

int paging_map_mmio_page(uint64_t virtual_address, uint64_t physical_address) {
    return paging_map_page(virtual_address, physical_address, PAGE_MMIO_FLAGS);
}

void *paging_physical_to_hhdm(uint64_t physical_address) {
    if (direct_map_offset == 0U || physical_address > UINT64_MAX - direct_map_offset) {
        return (void *)0;
    }
    return (void *)(uintptr_t)(direct_map_offset + physical_address);
}

uint64_t paging_active_root_physical(void) {
    return read_cr3() & PAGE_ADDRESS_MASK;
}

uint64_t paging_mapping_count(void) {
    return mapped_page_count;
}

int paging_user_range_is_mapped(uint64_t address, uint64_t length, int writable) {
    uint64_t end_address;
    uint64_t page_address;

    if (length == 0U || address < PAGING_USER_SPACE_START || address > PAGING_USER_SPACE_END
        || length - 1U > PAGING_USER_SPACE_END - address) {
        return 0;
    }
    end_address = address + length - 1U;
    page_address = address & ~(PAGING_PAGE_SIZE - 1U);
    for (;;) {
        uint64_t *pml4;
        uint64_t *pdpt;
        uint64_t *page_directory;
        uint64_t *page_table;
        uint64_t entry;
        uint16_t pml4_index;
        uint16_t pdpt_index;
        uint16_t pd_index;
        uint16_t pt_index;

        page_indices(page_address, &pml4_index, &pdpt_index, &pd_index, &pt_index);
        pml4 = active_pml4();
        entry = pml4[pml4_index];
        if ((entry & (PAGE_PRESENT | PAGING_FLAG_USER)) != (PAGE_PRESENT | PAGING_FLAG_USER)
            || (entry & PAGE_HUGE) != 0U) {
            return 0;
        }
        pdpt = physical_to_virtual(entry & PAGE_ADDRESS_MASK);
        entry = pdpt[pdpt_index];
        if ((entry & (PAGE_PRESENT | PAGING_FLAG_USER)) != (PAGE_PRESENT | PAGING_FLAG_USER)
            || (entry & PAGE_HUGE) != 0U) {
            return 0;
        }
        page_directory = physical_to_virtual(entry & PAGE_ADDRESS_MASK);
        entry = page_directory[pd_index];
        if ((entry & (PAGE_PRESENT | PAGING_FLAG_USER)) != (PAGE_PRESENT | PAGING_FLAG_USER)
            || (entry & PAGE_HUGE) != 0U) {
            return 0;
        }
        page_table = physical_to_virtual(entry & PAGE_ADDRESS_MASK);
        entry = page_table[pt_index];
        if ((entry & (PAGE_PRESENT | PAGING_FLAG_USER)) != (PAGE_PRESENT | PAGING_FLAG_USER)
            || (writable != 0 && (entry & PAGING_FLAG_WRITABLE) == 0U)) {
            return 0;
        }
        if (page_address >= (end_address & ~(PAGING_PAGE_SIZE - 1U))) {
            return 1;
        }
        page_address += PAGING_PAGE_SIZE;
    }
}

int paging_translate(uint64_t virtual_address, uint64_t *physical_address) {
    uint64_t *pml4;
    uint64_t *pdpt;
    uint64_t *page_directory;
    uint64_t *page_table;
    uint64_t entry;
    uint16_t pml4_index;
    uint16_t pdpt_index;
    uint16_t pd_index;
    uint16_t pt_index;

    if (physical_address == (uint64_t *)0 || direct_map_offset == 0U || is_canonical(virtual_address) == 0) {
        return 0;
    }

    page_indices(virtual_address, &pml4_index, &pdpt_index, &pd_index, &pt_index);
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

static uint64_t *space_pml4(const struct paging_space *space) {
    return physical_to_virtual(space->root_physical);
}

static int space_is_user_valid(const struct paging_space *space, uint64_t virtual_address) {
    return space != (const struct paging_space *)0 && space->root_physical != 0U
        && virtual_address >= PAGING_USER_SPACE_START && virtual_address <= PAGING_USER_SPACE_END
        && page_address_is_valid(virtual_address) != 0;
}

static int space_map_page(struct paging_space *space, uint64_t virtual_address, uint64_t physical_address,
                          uint64_t flags) {
    uint64_t *pml4;
    uint64_t *pdpt;
    uint64_t *page_directory;
    uint64_t *page_table;
    uint64_t existing;
    uint16_t pml4_index;
    uint16_t pdpt_index;
    uint16_t pd_index;
    uint16_t pt_index;

    if (space_is_user_valid(space, virtual_address) == 0
        || (physical_address & (PAGING_PAGE_SIZE - 1U)) != 0U
        || (flags & PAGING_FLAG_USER) == 0U) {
        return 0;
    }
    page_indices(virtual_address, &pml4_index, &pdpt_index, &pd_index, &pt_index);
    pml4 = space_pml4(space);
    pdpt = next_table(pml4, pml4_index, flags);
    if (pdpt == (uint64_t *)0) {
        return 0;
    }
    page_directory = next_table(pdpt, pdpt_index, flags);
    if (page_directory == (uint64_t *)0) {
        return 0;
    }
    page_table = next_table(page_directory, pd_index, flags);
    if (page_table == (uint64_t *)0) {
        return 0;
    }
    existing = page_table[pt_index];
    page_table[pt_index] = (physical_address & PAGE_ADDRESS_MASK) | PAGE_PRESENT | PAGE_MANAGED
                           | (flags & PAGE_LEAF_FLAGS);
    if ((existing & PAGE_PRESENT) == 0U) {
        space->mapping_count++;
    }
    if ((read_cr3() & PAGE_ADDRESS_MASK) == space->root_physical) {
        invalidate_page(virtual_address);
    }
    return 1;
}

static int space_unmap_page(struct paging_space *space, uint64_t virtual_address) {
    uint64_t *pml4;
    uint64_t *pdpt;
    uint64_t *page_directory;
    uint64_t *page_table;
    uint64_t entry;
    uint16_t pml4_index;
    uint16_t pdpt_index;
    uint16_t pd_index;
    uint16_t pt_index;

    if (space_is_user_valid(space, virtual_address) == 0) {
        return 0;
    }
    page_indices(virtual_address, &pml4_index, &pdpt_index, &pd_index, &pt_index);
    pml4 = space_pml4(space);
    pdpt = existing_table(pml4, pml4_index);
    if (pdpt == (uint64_t *)0) {
        return 0;
    }
    page_directory = existing_table(pdpt, pdpt_index);
    if (page_directory == (uint64_t *)0) {
        return 0;
    }
    page_table = existing_table(page_directory, pd_index);
    if (page_table == (uint64_t *)0) {
        return 0;
    }
    entry = page_table[pt_index];
    if ((entry & PAGE_MANAGED) == 0U) {
        return 0;
    }
    page_table[pt_index] = 0U;
    if ((entry & PAGE_PRESENT) != 0U && space->mapping_count != 0U) {
        space->mapping_count--;
    }
    if ((read_cr3() & PAGE_ADDRESS_MASK) == space->root_physical) {
        invalidate_page(virtual_address);
    }
    return 1;
}

static void destroy_user_table_tree(uint64_t physical_address, uint64_t level) {
    uint64_t *table = physical_to_virtual(physical_address);

    for (uint64_t index = 0U; index < PAGE_TABLE_ENTRIES; index++) {
        const uint64_t entry = table[index];

        if ((entry & PAGE_PRESENT) == 0U) {
            continue;
        }
        if (level == 1U) {
            if ((entry & PAGE_MANAGED) != 0U) {
                (void)pmm_free_frame(entry & PAGE_ADDRESS_MASK);
            }
        } else if ((entry & PAGE_HUGE) == 0U) {
            destroy_user_table_tree(entry & PAGE_ADDRESS_MASK, level - 1U);
        }
    }
    (void)pmm_free_frame(physical_address);
}

int paging_space_create_user(struct paging_space *space) {
    uint64_t *kernel_root;
    uint64_t *user_root;
    uint64_t root_frame;

    if (space == (struct paging_space *)0 || active_root == 0U) {
        return 0;
    }
    root_frame = pmm_allocate_frame();
    if (root_frame == PMM_INVALID_ADDRESS) {
        return 0;
    }
    kernel_root = physical_to_virtual(active_root);
    user_root = physical_to_virtual(root_frame);
    zero_page(root_frame);
    for (uint64_t index = PAGE_TABLE_ENTRIES / 2U; index < PAGE_TABLE_ENTRIES; index++) {
        user_root[index] = kernel_root[index];
    }
    space->root_physical = root_frame;
    space->mapping_count = 0U;
    return 1;
}

int paging_space_destroy_user(struct paging_space *space) {
    uint64_t *root;

    if (space == (struct paging_space *)0 || space->root_physical == 0U
        || (read_cr3() & PAGE_ADDRESS_MASK) == space->root_physical) {
        return 0;
    }
    root = space_pml4(space);
    for (uint64_t index = 0U; index < PAGE_TABLE_ENTRIES / 2U; index++) {
        const uint64_t entry = root[index];

        if ((entry & PAGE_PRESENT) != 0U && (entry & PAGE_HUGE) == 0U) {
            destroy_user_table_tree(entry & PAGE_ADDRESS_MASK, 3U);
        }
    }
    (void)pmm_free_frame(space->root_physical);
    space->root_physical = 0U;
    space->mapping_count = 0U;
    return 1;
}

int paging_space_map_page(struct paging_space *space, uint64_t virtual_address, uint64_t physical_address,
                          uint64_t flags) {
    return space_map_page(space, virtual_address, physical_address, flags);
}

int paging_space_unmap_page(struct paging_space *space, uint64_t virtual_address) {
    return space_unmap_page(space, virtual_address);
}

int paging_space_activate(const struct paging_space *space) {
    if (space == (const struct paging_space *)0 || space->root_physical == 0U) {
        return 0;
    }
    write_cr3(space->root_physical);
    return 1;
}

uint64_t paging_kernel_root_physical(void) {
    return active_root;
}

int paging_space_map_guard(struct paging_space *space, uint64_t virtual_address) {
    uint64_t *pml4;
    uint64_t *pdpt;
    uint64_t *page_directory;
    uint64_t *page_table;
    uint16_t pml4_index;
    uint16_t pdpt_index;
    uint16_t pd_index;
    uint16_t pt_index;

    if (space_is_user_valid(space, virtual_address) == 0) {
        return 0;
    }
    page_indices(virtual_address, &pml4_index, &pdpt_index, &pd_index, &pt_index);
    pml4 = space_pml4(space);
    pdpt = next_table(pml4, pml4_index, PAGING_FLAG_USER);
    if (pdpt == (uint64_t *)0) {
        return 0;
    }
    page_directory = next_table(pdpt, pdpt_index, PAGING_FLAG_USER);
    if (page_directory == (uint64_t *)0) {
        return 0;
    }
    page_table = next_table(page_directory, pd_index, PAGING_FLAG_USER);
    if (page_table == (uint64_t *)0) {
        return 0;
    }
    if ((page_table[pt_index] & PAGE_PRESENT) != 0U) {
        return 0;
    }
    page_table[pt_index] = PAGE_MANAGED | PAGE_GUARD;
    if ((read_cr3() & PAGE_ADDRESS_MASK) == space->root_physical) {
        invalidate_page(virtual_address);
    }
    return 1;
}

int paging_activate_kernel_space(void) {
    if (active_root == 0U) {
        return 0;
    }
    write_cr3(active_root);
    return 1;
}

int paging_space_self_test(void) {
    const uint64_t test_address = UINT64_C(0x0000000000500000);
    const uint64_t guard_address = test_address - PAGING_PAGE_SIZE;
    struct paging_space space = { 0U, 0U };
    uint64_t frame = PMM_INVALID_ADDRESS;
    volatile uint64_t *test_word = (volatile uint64_t *)(uintptr_t)test_address;
    int passed = 0;

    if (paging_space_create_user(&space) == 0
        || paging_space_map_guard(&space, guard_address) == 0) {
        goto cleanup;
    }
    frame = pmm_allocate_user_frame();
    if (frame == PMM_INVALID_ADDRESS
        || paging_space_map_page(&space, test_address, frame, PAGING_FLAG_USER | PAGING_FLAG_WRITABLE) == 0
        || paging_space_activate(&space) == 0) {
        goto cleanup;
    }
    *test_word = UINT64_C(0x4D594F532D415350);
    passed = *test_word == UINT64_C(0x4D594F532D415350);

cleanup:
    (void)paging_activate_kernel_space();
    if (frame != PMM_INVALID_ADDRESS) {
        (void)paging_space_unmap_page(&space, test_address);
        (void)pmm_free_frame(frame);
    }
    if (space.root_physical != 0U) {
        (void)paging_space_destroy_user(&space);
    }
    return passed;
}
