#ifndef MYOS_PAGING_H
#define MYOS_PAGING_H

#include <stdint.h>

#define PAGING_PAGE_SIZE UINT64_C(4096)
#define PAGING_LAPIC_VIRTUAL_ADDRESS UINT64_C(0xFFFFFFFFC0000000)

void paging_init(uint64_t hhdm_offset);
int paging_map_mmio_page(uint64_t virtual_address, uint64_t physical_address);

#endif
