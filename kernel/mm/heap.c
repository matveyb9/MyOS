#include <stddef.h>
#include <stdint.h>

#include <heap.h>
#include <paging.h>
#include <pmm.h>

#define HEAP_ALIGNMENT UINT64_C(16)
#define HEAP_BLOCK_MAGIC UINT64_C(0x4D594F5348454150)
#define HEAP_BLOCK_ALLOCATED UINT64_C(0)
#define HEAP_BLOCK_FREE UINT64_C(1)

typedef struct heap_block {
    uint64_t total_size;
    struct heap_block *next;
    uint64_t magic;
    uint64_t state;
} heap_block_t;

static uint64_t heap_next;
static uint64_t heap_mapped_end;
static uint64_t heap_end;
static heap_block_t *free_list;
static uint64_t active_allocation_count;
static uint64_t active_payload_bytes;
static uint64_t free_block_count;
static uint64_t reuse_count;

static uint64_t align_up(uint64_t value, uint64_t alignment) {
    if (value > UINT64_MAX - (alignment - 1U)) {
        return 0U;
    }
    return (value + (alignment - 1U)) & ~(alignment - 1U);
}

static uint64_t block_payload_size(const heap_block_t *block) {
    return block->total_size - sizeof(heap_block_t);
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

static void zero_payload(heap_block_t *block) {
    uint8_t *payload = (uint8_t *)(block + 1);
    for (uint64_t index = 0U; index < block_payload_size(block); index++) {
        payload[index] = 0U;
    }
}

static heap_block_t *take_free_block(uint64_t total_size) {
    heap_block_t *previous = (heap_block_t *)0;
    heap_block_t *current = free_list;

    while (current != (heap_block_t *)0) {
        if (current->total_size >= total_size) {
            const uint64_t remaining = current->total_size - total_size;
            if (remaining >= sizeof(heap_block_t) + HEAP_ALIGNMENT) {
                heap_block_t *split = (heap_block_t *)((uint8_t *)current + total_size);
                split->total_size = remaining;
                split->next = current->next;
                split->magic = HEAP_BLOCK_MAGIC;
                split->state = HEAP_BLOCK_FREE;
                current->total_size = total_size;
                if (previous == (heap_block_t *)0) {
                    free_list = split;
                } else {
                    previous->next = split;
                }
            } else {
                if (previous == (heap_block_t *)0) {
                    free_list = current->next;
                } else {
                    previous->next = current->next;
                }
                free_block_count--;
            }
            current->next = (heap_block_t *)0;
            current->state = HEAP_BLOCK_ALLOCATED;
            reuse_count++;
            return current;
        }
        previous = current;
        current = current->next;
    }
    return (heap_block_t *)0;
}

static void insert_free_block(heap_block_t *block) {
    heap_block_t *previous = (heap_block_t *)0;
    heap_block_t *current = free_list;

    while (current != (heap_block_t *)0
           && (uint64_t)(uintptr_t)current < (uint64_t)(uintptr_t)block) {
        previous = current;
        current = current->next;
    }

    block->state = HEAP_BLOCK_FREE;
    block->next = current;
    if (previous == (heap_block_t *)0) {
        free_list = block;
    } else {
        previous->next = block;
    }
    free_block_count++;

    if (previous != (heap_block_t *)0
        && (uint8_t *)previous + previous->total_size == (uint8_t *)block) {
        previous->total_size += block->total_size;
        previous->next = block->next;
        block = previous;
        free_block_count--;
    }
    if (block->next != (heap_block_t *)0
        && (uint8_t *)block + block->total_size == (uint8_t *)block->next) {
        heap_block_t *next = block->next;
        block->total_size += next->total_size;
        block->next = next->next;
        free_block_count--;
    }
}

void heap_init(void) {
    heap_next = PAGING_KERNEL_HEAP_START;
    heap_mapped_end = PAGING_KERNEL_HEAP_START;
    heap_end = PAGING_KERNEL_HEAP_START + PAGING_KERNEL_HEAP_SIZE;
    free_list = (heap_block_t *)0;
    active_allocation_count = 0U;
    active_payload_bytes = 0U;
    free_block_count = 0U;
    reuse_count = 0U;
}

void *kmalloc(size_t size) {
    uint64_t total_size;
    heap_block_t *block;

    if (size == 0U) {
        size = 1U;
    }
    if ((uint64_t)size > UINT64_MAX - sizeof(heap_block_t)) {
        return (void *)0;
    }
    total_size = align_up((uint64_t)size + sizeof(heap_block_t), HEAP_ALIGNMENT);
    if (total_size == 0U) {
        return (void *)0;
    }

    block = take_free_block(total_size);
    if (block == (heap_block_t *)0) {
        if (heap_next > heap_end || total_size > heap_end - heap_next
            || map_until(heap_next + total_size) == 0) {
            return (void *)0;
        }
        block = (heap_block_t *)(uintptr_t)heap_next;
        block->total_size = total_size;
        block->next = (heap_block_t *)0;
        block->magic = HEAP_BLOCK_MAGIC;
        block->state = HEAP_BLOCK_ALLOCATED;
        heap_next += total_size;
    }

    zero_payload(block);
    active_allocation_count++;
    active_payload_bytes += block_payload_size(block);
    return (void *)(block + 1);
}

int kfree(void *address) {
    heap_block_t *block;

    if (address == (void *)0 || (uint64_t)(uintptr_t)address < PAGING_KERNEL_HEAP_START + sizeof(heap_block_t)
        || (uint64_t)(uintptr_t)address >= heap_next) {
        return 0;
    }
    block = ((heap_block_t *)address) - 1;
    if (block->magic != HEAP_BLOCK_MAGIC || block->state != HEAP_BLOCK_ALLOCATED
        || block->total_size < sizeof(heap_block_t) + HEAP_ALIGNMENT
        || (uint64_t)(uintptr_t)block + block->total_size > heap_next) {
        return 0;
    }

    active_allocation_count--;
    active_payload_bytes -= block_payload_size(block);
    insert_free_block(block);
    return 1;
}

uint64_t heap_used_bytes(void) {
    return active_payload_bytes;
}

uint64_t heap_capacity_bytes(void) {
    return heap_end - PAGING_KERNEL_HEAP_START;
}

uint64_t heap_mapped_page_count(void) {
    return (heap_mapped_end - PAGING_KERNEL_HEAP_START) / PAGING_PAGE_SIZE;
}

uint64_t heap_allocation_count(void) {
    return active_allocation_count;
}

uint64_t heap_free_block_count(void) {
    return free_block_count;
}

uint64_t heap_reuse_count(void) {
    return reuse_count;
}
