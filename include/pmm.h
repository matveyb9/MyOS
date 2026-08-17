#ifndef MYOS_PMM_H
#define MYOS_PMM_H

#include <stdint.h>

struct limine_memmap_response;

#define PMM_PAGE_SIZE UINT64_C(4096)
#define PMM_INVALID_ADDRESS UINT64_MAX

void pmm_init(const struct limine_memmap_response *memory_map);
uint64_t pmm_allocate_frame(void);
uint64_t pmm_free_frame_count(void);
uint64_t pmm_tracked_frame_count(void);

#endif
