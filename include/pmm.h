#ifndef MYOS_PMM_H
#define MYOS_PMM_H

#include <stdint.h>

struct limine_memmap_response;

#define PMM_PAGE_SIZE UINT64_C(4096)
#define PMM_INVALID_ADDRESS UINT64_MAX

enum frame_owner {
    FRAME_OWNER_FREE = 0,
    FRAME_OWNER_KERNEL,
    FRAME_OWNER_BOOTLOADER,
    FRAME_OWNER_USER,
    FRAME_OWNER_UNMANAGED
};

void pmm_init(const struct limine_memmap_response *memory_map);
uint64_t pmm_allocate_frame(void);
uint64_t pmm_allocate_user_frame(void);
int pmm_reserve_frame(uint64_t physical_address);
int pmm_reserve_kernel_range(uint64_t physical_start, uint64_t physical_end);
int pmm_free_frame(uint64_t physical_address);
int pmm_frame_is_free(uint64_t physical_address);
enum frame_owner pmm_frame_owner(uint64_t physical_address);
uint64_t pmm_frame_owner_count(enum frame_owner owner);
uint64_t pmm_free_frame_count(void);
uint64_t pmm_usable_frame_count(void);
uint64_t pmm_used_usable_frame_count(void);
uint64_t pmm_tracked_frame_count(void);

#endif
