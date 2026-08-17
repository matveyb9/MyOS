#ifndef MYOS_PAGING_H
#define MYOS_PAGING_H

#include <stdint.h>

#define PAGING_PAGE_SIZE UINT64_C(4096)
#define PAGING_LAPIC_VIRTUAL_ADDRESS UINT64_C(0xFFFFFFFFC0000000)
#define PAGING_KERNEL_HEAP_START UINT64_C(0xFFFF900000000000)
#define PAGING_KERNEL_HEAP_SIZE UINT64_C(0x0000000040000000)
#define PAGING_KERNEL_HEAP_END (PAGING_KERNEL_HEAP_START + PAGING_KERNEL_HEAP_SIZE)
#define PAGING_KERNEL_HEAP_GUARD_ADDRESS PAGING_KERNEL_HEAP_END
#define PAGING_USER_SPACE_START UINT64_C(0x0000000000001000)
#define PAGING_USER_SPACE_END UINT64_C(0x00007FFFFFFFFFFF)

#define PAGING_FLAG_WRITABLE UINT64_C(0x002)
#define PAGING_FLAG_USER UINT64_C(0x004)
#define PAGING_FLAG_WRITE_THROUGH UINT64_C(0x008)
#define PAGING_FLAG_CACHE_DISABLE UINT64_C(0x010)

struct paging_space {
    uint64_t root_physical;
    uint64_t mapping_count;
};

void paging_init(uint64_t hhdm_offset);
int paging_take_control(void);
int paging_map_page(uint64_t virtual_address, uint64_t physical_address, uint64_t flags);
int paging_unmap_page(uint64_t virtual_address);
int paging_map_guard(uint64_t virtual_address);
int paging_is_guard_page(uint64_t virtual_address);
int paging_map_mmio_page(uint64_t virtual_address, uint64_t physical_address);
int paging_space_create_user(struct paging_space *space);
int paging_space_destroy_user(struct paging_space *space);
int paging_space_map_page(struct paging_space *space, uint64_t virtual_address, uint64_t physical_address,
                          uint64_t flags);
int paging_space_unmap_page(struct paging_space *space, uint64_t virtual_address);
int paging_space_map_guard(struct paging_space *space, uint64_t virtual_address);
int paging_space_activate(const struct paging_space *space);
int paging_activate_kernel_space(void);
int paging_space_self_test(void);
uint64_t paging_kernel_root_physical(void);
uint64_t paging_active_root_physical(void);
uint64_t paging_mapping_count(void);
int paging_translate(uint64_t virtual_address, uint64_t *physical_address);

#endif
