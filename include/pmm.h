#ifndef MYOS_PMM_H
#define MYOS_PMM_H

#include <stdint.h>

struct limine_memmap_response;

#define PMM_PAGE_SIZE UINT64_C(4096)
#define PMM_INVALID_ADDRESS UINT64_MAX

void pmm_init(const struct limine_memmap_response *memory_map);
uint64_t pmm_allocate_frame(void);
int pmm_reserve_frame(uint64_t physical_address);
int pmm_free_frame(uint64_t physical_address);
int pmm_frame_is_free(uint64_t physical_address);
uint64_t pmm_free_frame_count(void);
uint64_t pmm_usable_frame_count(void);
uint64_t pmm_used_usable_frame_count(void);
uint64_t pmm_tracked_frame_count(void);

#endif
