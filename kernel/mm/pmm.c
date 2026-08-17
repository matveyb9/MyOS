#include <stddef.h>
#include <stdint.h>

#include <limine.h>

#include <pmm.h>

#define PMM_MAX_PHYSICAL_MEMORY UINT64_C(0x100000000)
#define PMM_MAX_FRAMES (PMM_MAX_PHYSICAL_MEMORY / PMM_PAGE_SIZE)
#define PMM_BITMAP_BYTES (PMM_MAX_FRAMES / 8U)
#define PMM_OWNER_COUNT (FRAME_OWNER_UNMANAGED + 1U)

static uint8_t frame_bitmap[PMM_BITMAP_BYTES];
static uint8_t usable_bitmap[PMM_BITMAP_BYTES];
static uint8_t permanent_bitmap[PMM_BITMAP_BYTES];
static uint8_t frame_owners[PMM_MAX_FRAMES];
static uint64_t owner_frame_counts[PMM_OWNER_COUNT];
static uint64_t tracked_frames;
static uint64_t usable_frames;
static uint64_t free_frames;

static uint64_t align_up_page(uint64_t address) {
    if (address > UINT64_MAX - (PMM_PAGE_SIZE - 1U)) {
        return 0U;
    }
    return (address + (PMM_PAGE_SIZE - 1U)) & ~(PMM_PAGE_SIZE - 1U);
}

static uint64_t align_down_page(uint64_t address) {
    return address & ~(PMM_PAGE_SIZE - 1U);
}

static uint8_t frame_bit(uint64_t frame) {
    return (uint8_t)(UINT8_C(1) << (frame % 8U));
}

static int frame_is_usable(uint64_t frame) {
    return (usable_bitmap[frame / 8U] & frame_bit(frame)) != 0U;
}

static int frame_is_free_index(uint64_t frame) {
    return (frame_bitmap[frame / 8U] & frame_bit(frame)) == 0U;
}

static int frame_is_permanent(uint64_t frame) {
    return (permanent_bitmap[frame / 8U] & frame_bit(frame)) != 0U;
}

static void mark_frame_usable(uint64_t frame) {
    const uint64_t byte_index = frame / 8U;
    const uint8_t bit = frame_bit(frame);

    if ((usable_bitmap[byte_index] & bit) == 0U) {
        usable_bitmap[byte_index] |= bit;
        usable_frames++;
    }
}

static void mark_frame_free(uint64_t frame) {
    const uint64_t byte_index = frame / 8U;
    const uint8_t bit = frame_bit(frame);

    if ((frame_bitmap[byte_index] & bit) != 0U) {
        frame_bitmap[byte_index] &= (uint8_t)~bit;
        free_frames++;
    }
}

static void mark_frame_used(uint64_t frame) {
    const uint64_t byte_index = frame / 8U;
    const uint8_t bit = frame_bit(frame);

    if ((frame_bitmap[byte_index] & bit) == 0U) {
        frame_bitmap[byte_index] |= bit;
        free_frames--;
    }
}

static void mark_frame_permanent(uint64_t frame) {
    permanent_bitmap[frame / 8U] |= frame_bit(frame);
}

static void set_frame_owner(uint64_t frame, enum frame_owner owner) {
    const enum frame_owner previous = (enum frame_owner)frame_owners[frame];

    if (previous == owner) {
        return;
    }
    owner_frame_counts[previous]--;
    owner_frame_counts[owner]++;
    frame_owners[frame] = (uint8_t)owner;
}

static int physical_address_to_frame(uint64_t physical_address, uint64_t *frame) {
    if (frame == (uint64_t *)0 || (physical_address & (PMM_PAGE_SIZE - 1U)) != 0U) {
        return 0;
    }
    *frame = physical_address / PMM_PAGE_SIZE;
    return *frame < tracked_frames;
}

static enum frame_owner owner_from_memory_map_type(uint64_t type) {
    if (type == LIMINE_MEMMAP_USABLE) {
        return FRAME_OWNER_FREE;
    }
    if (type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
        return FRAME_OWNER_BOOTLOADER;
    }
    if (type == LIMINE_MEMMAP_EXECUTABLE_AND_MODULES) {
        return FRAME_OWNER_KERNEL;
    }
    return FRAME_OWNER_UNMANAGED;
}

void pmm_init(const struct limine_memmap_response *memory_map) {
    for (uint64_t index = 0U; index < PMM_BITMAP_BYTES; index++) {
        frame_bitmap[index] = UINT8_MAX;
        usable_bitmap[index] = 0U;
        permanent_bitmap[index] = 0U;
    }
    for (uint64_t index = 0U; index < PMM_MAX_FRAMES; index++) {
        frame_owners[index] = (uint8_t)FRAME_OWNER_UNMANAGED;
    }
    for (uint64_t index = 0U; index < PMM_OWNER_COUNT; index++) {
        owner_frame_counts[index] = 0U;
    }

    tracked_frames = PMM_MAX_FRAMES;
    owner_frame_counts[FRAME_OWNER_UNMANAGED] = tracked_frames;
    usable_frames = 0U;
    free_frames = 0U;
    if (memory_map == NULL) {
        return;
    }

    for (uint64_t index = 0U; index < memory_map->entry_count; index++) {
        const struct limine_memmap_entry *entry = memory_map->entries[index];
        const enum frame_owner owner = owner_from_memory_map_type(entry->type);
        const uint64_t begin = align_up_page(entry->base);
        uint64_t end;
        uint64_t first_frame;
        uint64_t last_frame;

        if (entry->length == 0U || entry->base > UINT64_MAX - entry->length
            || (entry->base != 0U && begin == 0U)) {
            continue;
        }
        end = align_down_page(entry->base + entry->length);
        first_frame = begin / PMM_PAGE_SIZE;
        last_frame = end / PMM_PAGE_SIZE;
        if (first_frame >= tracked_frames) {
            continue;
        }
        if (last_frame > tracked_frames) {
            last_frame = tracked_frames;
        }
        for (uint64_t frame = first_frame; frame < last_frame; frame++) {
            if (entry->type == LIMINE_MEMMAP_USABLE && frame != 0U) {
                mark_frame_usable(frame);
                mark_frame_free(frame);
            }
            set_frame_owner(frame, owner);
        }
    }
}

static uint64_t allocate_frame_for_owner(enum frame_owner owner) {
    for (uint64_t frame = 1U; frame < tracked_frames; frame++) {
        if (frame_is_usable(frame) != 0 && frame_is_free_index(frame) != 0
            && frame_owners[frame] == FRAME_OWNER_FREE) {
            mark_frame_used(frame);
            set_frame_owner(frame, owner);
            return frame * PMM_PAGE_SIZE;
        }
    }
    return PMM_INVALID_ADDRESS;
}

uint64_t pmm_allocate_frame(void) {
    return allocate_frame_for_owner(FRAME_OWNER_KERNEL);
}

uint64_t pmm_allocate_user_frame(void) {
    return allocate_frame_for_owner(FRAME_OWNER_USER);
}

int pmm_reserve_frame(uint64_t physical_address) {
    uint64_t frame;

    if (physical_address_to_frame(physical_address, &frame) == 0 || frame_is_usable(frame) == 0
        || frame_is_free_index(frame) == 0 || frame_owners[frame] != FRAME_OWNER_FREE) {
        return 0;
    }
    mark_frame_used(frame);
    set_frame_owner(frame, FRAME_OWNER_KERNEL);
    return 1;
}

int pmm_reserve_kernel_range(uint64_t physical_start, uint64_t physical_end) {
    uint64_t start;
    uint64_t end;

    if (physical_start >= physical_end || physical_start >= PMM_MAX_PHYSICAL_MEMORY) {
        return 0;
    }
    start = align_down_page(physical_start);
    end = align_up_page(physical_end);
    if (end == 0U || end > PMM_MAX_PHYSICAL_MEMORY) {
        end = PMM_MAX_PHYSICAL_MEMORY;
    }
    if (start >= end) {
        return 0;
    }

    for (uint64_t frame = start / PMM_PAGE_SIZE; frame < end / PMM_PAGE_SIZE; frame++) {
        if (frame_is_usable(frame) != 0 && frame_is_free_index(frame) != 0) {
            mark_frame_used(frame);
        }
        set_frame_owner(frame, FRAME_OWNER_KERNEL);
        mark_frame_permanent(frame);
    }
    return 1;
}

int pmm_free_frame(uint64_t physical_address) {
    uint64_t frame;

    if (physical_address_to_frame(physical_address, &frame) == 0 || frame_is_usable(frame) == 0
        || frame_is_free_index(frame) != 0 || frame_is_permanent(frame) != 0
        || frame_owners[frame] == FRAME_OWNER_BOOTLOADER
        || frame_owners[frame] == FRAME_OWNER_UNMANAGED) {
        return 0;
    }
    mark_frame_free(frame);
    set_frame_owner(frame, FRAME_OWNER_FREE);
    return 1;
}

int pmm_frame_is_free(uint64_t physical_address) {
    uint64_t frame;

    return physical_address_to_frame(physical_address, &frame) != 0 && frame_is_usable(frame) != 0
        && frame_is_free_index(frame) != 0 && frame_owners[frame] == FRAME_OWNER_FREE;
}

enum frame_owner pmm_frame_owner(uint64_t physical_address) {
    const uint64_t frame = physical_address / PMM_PAGE_SIZE;

    if (frame >= tracked_frames || frame_owners[frame] > FRAME_OWNER_UNMANAGED) {
        return FRAME_OWNER_UNMANAGED;
    }
    return (enum frame_owner)frame_owners[frame];
}

uint64_t pmm_frame_owner_count(enum frame_owner owner) {
    if (owner > FRAME_OWNER_UNMANAGED) {
        return 0U;
    }
    return owner_frame_counts[owner];
}

uint64_t pmm_free_frame_count(void) {
    return free_frames;
}

uint64_t pmm_usable_frame_count(void) {
    return usable_frames;
}

uint64_t pmm_used_usable_frame_count(void) {
    return usable_frames - free_frames;
}

uint64_t pmm_tracked_frame_count(void) {
    return tracked_frames;
}
