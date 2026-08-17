#include <stddef.h>
#include <stdint.h>

#include <heap.h>
#include <paging.h>
#include <pmm.h>

#define HEAP_ALIGNMENT UINT64_C(16)

static uint64_t heap_next;
static uint64_t heap_mapped_end;
static uint64_t heap_end;
static uint64_t allocation_count;

static uint64_t align_up(uint64_t value, uint64_t alignment) {
    if (value > UINT64_MAX - (alignment - 1U)) {
        return 0U;
    }
    return (value + (alignment - 1U)) & ~(alignment - 1U);
}

static int map_until(uint64_t end_address) {
    while (heap_mapped_end < end_address) {
        const uint64_t frame = pmm_allocate_frame();
        if (frame == PMM_INVALID_ADDRESS
            || paging_map_page(heap_mapped_end, frame, PAGING_FLAG_WRITABLE) == 0) {
            return 0;
        }
        heap_mapped_end += PAGING_PAGE_SIZE;
    }
    return 1;
}

void heap_init(void) {
    heap_next = PAGING_KERNEL_HEAP_START;
    heap_mapped_end = PAGING_KERNEL_HEAP_START;
    heap_end = PAGING_KERNEL_HEAP_START + PAGING_KERNEL_HEAP_SIZE;
    allocation_count = 0U;
}

void *kmalloc(size_t size) {
    uint64_t aligned_size;
    uint64_t allocation_end;
    uint8_t *allocation;

    if (size == 0U) {
        size = 1U;
    }
    aligned_size = align_up((uint64_t)size, HEAP_ALIGNMENT);
    if (aligned_size == 0U || heap_next > heap_end || aligned_size > heap_end - heap_next) {
        return (void *)0;
    }

    allocation_end = heap_next + aligned_size;
    if (map_until(allocation_end) == 0) {
        return (void *)0;
    }

    allocation = (uint8_t *)(uintptr_t)heap_next;
    for (uint64_t index = 0U; index < aligned_size; index++) {
        allocation[index] = 0U;
    }
    heap_next = allocation_end;
    allocation_count++;
    return allocation;
}

uint64_t heap_used_bytes(void) {
    return heap_next - PAGING_KERNEL_HEAP_START;
}

uint64_t heap_capacity_bytes(void) {
    return heap_end - PAGING_KERNEL_HEAP_START;
}

uint64_t heap_mapped_page_count(void) {
    return (heap_mapped_end - PAGING_KERNEL_HEAP_START) / PAGING_PAGE_SIZE;
}

uint64_t heap_allocation_count(void) {
    return allocation_count;
}
