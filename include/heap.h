#ifndef MYOS_HEAP_H
#define MYOS_HEAP_H

#include <stddef.h>
#include <stdint.h>

void heap_init(void);
void *kmalloc(size_t size);
int kfree(void *address);
uint64_t heap_used_bytes(void);
uint64_t heap_capacity_bytes(void);
uint64_t heap_mapped_page_count(void);
uint64_t heap_allocation_count(void);
uint64_t heap_free_block_count(void);
uint64_t heap_reuse_count(void);

#endif
