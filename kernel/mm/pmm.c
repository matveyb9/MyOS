#include <stddef.h>
#include <stdint.h>

#include <limine.h>

#include <pmm.h>

#define PMM_MAX_PHYSICAL_MEMORY UINT64_C(0x100000000)
#define PMM_MAX_FRAMES (PMM_MAX_PHYSICAL_MEMORY / PMM_PAGE_SIZE)
#define PMM_BITMAP_BYTES (PMM_MAX_FRAMES / 8U)

static uint8_t frame_bitmap[PMM_BITMAP_BYTES];
static uint64_t tracked_frames;
static uint64_t free_frames;

static uint64_t align_up_page(uint64_t address) {
    return (address + (PMM_PAGE_SIZE - 1U)) & ~(PMM_PAGE_SIZE - 1U);
}

static uint64_t align_down_page(uint64_t address) {
    return address & ~(PMM_PAGE_SIZE - 1U);
}

static int frame_is_free(uint64_t frame) {
    return (frame_bitmap[frame / 8U] & (uint8_t)(UINT8_C(1) << (frame % 8U))) == 0U;
}

static void mark_frame_free(uint64_t frame) {
    const uint64_t byte_index = frame / 8U;
    const uint8_t bit = (uint8_t)(UINT8_C(1) << (frame % 8U));

    if ((frame_bitmap[byte_index] & bit) != 0U) {
        frame_bitmap[byte_index] &= (uint8_t)~bit;
        free_frames++;
    }
}

void pmm_init(const struct limine_memmap_response *memory_map) {
    for (uint64_t index = 0U; index < PMM_BITMAP_BYTES; index++) {
        frame_bitmap[index] = UINT8_MAX;
    }

    tracked_frames = PMM_MAX_FRAMES;
    free_frames = 0U;
    if (memory_map == NULL) {
        return;
    }

    for (uint64_t index = 0U; index < memory_map->entry_count; index++) {
        const struct limine_memmap_entry *entry = memory_map->entries[index];
        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        const uint64_t begin = align_up_page(entry->base);
        const uint64_t end = align_down_page(entry->base + entry->length);
        const uint64_t first_frame = begin / PMM_PAGE_SIZE;
        uint64_t last_frame = end / PMM_PAGE_SIZE;

        if (first_frame >= tracked_frames) {
            continue;
        }
        if (last_frame > tracked_frames) {
            last_frame = tracked_frames;
        }
        for (uint64_t frame = first_frame; frame < last_frame; frame++) {
            if (frame != 0U) {
                mark_frame_free(frame);
            }
        }
    }
}

uint64_t pmm_allocate_frame(void) {
    for (uint64_t frame = 1U; frame < tracked_frames; frame++) {
        if (frame_is_free(frame) != 0) {
            const uint64_t byte_index = frame / 8U;
            const uint8_t bit = (uint8_t)(UINT8_C(1) << (frame % 8U));
            frame_bitmap[byte_index] |= bit;
            free_frames--;
            return frame * PMM_PAGE_SIZE;
        }
    }
    return PMM_INVALID_ADDRESS;
}

uint64_t pmm_free_frame_count(void) {
    return free_frames;
}

uint64_t pmm_tracked_frame_count(void) {
    return tracked_frames;
}
