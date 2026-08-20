#include <stdint.h>

#include <ahci.h>
#include <framebuffer.h>
#include <inventory.h>
#include <keyboard.h>
#include <mouse.h>
#include <pit.h>
#include <scheduler.h>
#include <vfs.h>

#define NEWC_HEADER_SIZE 110U
#define PERSIST_METADATA_LBA AHCI_DATA_LBA_START
#define PERSIST_RECORD_LBA (AHCI_DATA_LBA_START + 2U)
#define PERSIST_RECORD_SECTORS 32U
#define PERSIST_BITMAP_LBA (PERSIST_RECORD_LBA + PERSIST_RECORD_SECTORS)
#define PERSIST_BITMAP_SECTORS 48U
#define PERSIST_DATA_LBA (PERSIST_BITMAP_LBA + PERSIST_BITMAP_SECTORS)
#define PERSIST_JOURNAL_LBA AHCI_DATA_LBA_END
#define PERSIST_STAGING_LBA (PERSIST_JOURNAL_LBA - 512U)
#define PERSIST_DATA_LAST_LBA (PERSIST_STAGING_LBA - 1U)
#define PERSIST_DATA_BLOCKS (PERSIST_DATA_LAST_LBA - PERSIST_DATA_LBA + 1U)
#define PERSIST_RECORD_SIZE 128U
#define PERSIST_GROW_BLOCKS 128U
#define PERSIST_ROOT_INDEX UINT16_C(0)
#define PERSIST_PARENT_ROOT UINT16_C(0xFFFF)
#define PERSIST_PARENT_SYSTEM UINT16_C(0xFFFE)
#define TMP_PARENT_ROOT UINT16_C(0xFFFF)
#define TARGET_ROOT 0U
#define TARGET_SYSTEM 1U
#define TARGET_CORE 2U
#define TARGET_LIVE 3U
#define TARGET_PERSIST 4U
#define TARGET_TEMP_ROOT 5U
#define TARGET_TEMP 6U
#define JOURNAL_RECORD_SIZE 48U
#define JOURNAL_HEADER_SIZE 16U
#define LEGACY_FILE_SECTORS (UINT64_C(32768) / AHCI_SECTOR_SIZE)
#define LEGACY_RECORD_SIZE 56U
#define LEGACY_RECORD_OFFSET 16U

struct path_parts {
    uint64_t count;
    char item[VFS_PATH_DEPTH_MAX][VFS_NAME_MAX];
};

struct vfs_target {
    uint8_t kind;
    uint16_t index;
};

struct persistent_extent {
    uint32_t start_block;
    uint32_t block_count;
};

struct persistent_node {
    uint8_t used;
    uint8_t type;
    uint8_t name_length;
    uint8_t flags;
    uint16_t parent;
    uint16_t reserved;
    uint64_t size;
    struct persistent_extent extent[VFS_PERSIST_EXTENT_MAX];
    char name[VFS_NAME_MAX];
};

struct tmp_node {
    uint8_t used;
    uint8_t type;
    uint8_t name_length;
    uint8_t reserved;
    uint16_t parent;
    uint64_t size;
    char name[VFS_NAME_MAX];
    uint8_t data[VFS_TMPFS_FILE_CAPACITY];
};

struct legacy_record {
    uint8_t used;
    uint64_t size;
    char name[40];
};

static const uint8_t *mounted_archive;
static uint64_t mounted_length;
static uint64_t mounted_files;
static struct persistent_node persistent_nodes[VFS_PERSIST_MAX_FILES];
static struct tmp_node tmp_nodes[VFS_TMPFS_MAX_FILES];
static uint8_t persistent_bitmap[PERSIST_BITMAP_SECTORS * AHCI_SECTOR_SIZE];
static uint8_t persistent_mounted;
static uint8_t persistent_open_buffer[VFS_PERSIST_OPEN_CAPACITY];
static uint8_t sector_buffer[AHCI_SECTOR_SIZE];
static uint8_t migration_buffer[AHCI_SECTOR_SIZE];
static uint8_t live_buffer[512U];

static uint64_t align_up4(uint64_t value) {
    return (value + 3U) & ~UINT64_C(3);
}

static int ascii_fold_equal_char(char left, char right) {
    if (left >= 'A' && left <= 'Z') {
        left = (char)(left - 'A' + 'a');
    }
    if (right >= 'A' && right <= 'Z') {
        right = (char)(right - 'A' + 'a');
    }
    return left == right;
}

static int text_equal_fold(const char *left, const char *right) {
    uint64_t index = 0U;

    if (left == (const char *)0 || right == (const char *)0) {
        return 0;
    }
    while (left[index] != '\0' && right[index] != '\0') {
        if (ascii_fold_equal_char(left[index], right[index]) == 0) {
            return 0;
        }
        index++;
    }
    return left[index] == '\0' && right[index] == '\0';
}

static int bytes_equal_fold(const uint8_t *left, uint64_t left_length, const char *right) {
    uint64_t index = 0U;

    if (left == (const uint8_t *)0 || right == (const char *)0) {
        return 0;
    }
    while (index < left_length && left[index] != '\0' && right[index] != '\0') {
        if (ascii_fold_equal_char((char)left[index], right[index]) == 0) {
            return 0;
        }
        index++;
    }
    return index < left_length && left[index] == '\0' && right[index] == '\0';
}

static uint64_t text_length(const char *text, uint64_t limit) {
    uint64_t length = 0U;

    while (length < limit && text[length] != '\0') {
        length++;
    }
    return length;
}

static void text_copy(char *destination, uint64_t destination_capacity, const char *source) {
    uint64_t index = 0U;

    if (destination == (char *)0 || destination_capacity == 0U) {
        return;
    }
    while (index + 1U < destination_capacity && source != (const char *)0 && source[index] != '\0') {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

static int text_starts_with_fold(const char *text, const char *prefix) {
    uint64_t index = 0U;

    while (prefix[index] != '\0') {
        if (text[index] == '\0' || ascii_fold_equal_char(text[index], prefix[index]) == 0) {
            return 0;
        }
        index++;
    }
    return 1;
}

static int parse_path(const char *path, struct path_parts *parts) {
    uint64_t index = 0U;

    if (path == (const char *)0 || parts == (struct path_parts *)0 || path[0] != '/') {
        return 0;
    }
    parts->count = 0U;
    while (path[index] != '\0') {
        uint64_t length = 0U;

        while (path[index] == '/') {
            index++;
        }
        if (path[index] == '\0') {
            break;
        }
        while (path[index + length] != '\0' && path[index + length] != '/') {
            const char character = path[index + length];

            if (character < ' ' || character > '~' || length + 1U >= VFS_NAME_MAX) {
                return 0;
            }
            length++;
        }
        if (length == 1U && path[index] == '.') {
            index += length;
            continue;
        }
        if (length == 2U && path[index] == '.' && path[index + 1U] == '.') {
            if (parts->count != 0U) {
                parts->count--;
            }
            index += length;
            continue;
        }
        if (parts->count >= VFS_PATH_DEPTH_MAX) {
            return 0;
        }
        for (uint64_t character = 0U; character < length; character++) {
            parts->item[parts->count][character] = path[index + character];
        }
        parts->item[parts->count][length] = '\0';
        parts->count++;
        index += length;
    }
    return text_length(path, VFS_PATH_MAX + 1U) <= VFS_PATH_MAX;
}

static int hex8(const uint8_t *text, uint64_t *value) {
    uint64_t result = 0U;

    for (uint64_t index = 0U; index < 8U; index++) {
        uint8_t digit;

        if (text[index] >= '0' && text[index] <= '9') {
            digit = (uint8_t)(text[index] - '0');
        } else if (text[index] >= 'a' && text[index] <= 'f') {
            digit = (uint8_t)(text[index] - 'a' + 10U);
        } else if (text[index] >= 'A' && text[index] <= 'F') {
            digit = (uint8_t)(text[index] - 'A' + 10U);
        } else {
            return 0;
        }
        result = (result << 4U) | digit;
    }
    *value = result;
    return 1;
}

static int next_cpio_entry(uint64_t *offset, const uint8_t **name, uint64_t *name_size,
                           const uint8_t **data, uint64_t *data_size) {
    const uint8_t *header;
    uint64_t entry_name_size;
    uint64_t entry_data_size;
    uint64_t name_offset;
    uint64_t data_offset;
    uint64_t next_offset;

    if (offset == (uint64_t *)0 || *offset > mounted_length
        || mounted_length - *offset < NEWC_HEADER_SIZE) {
        return 0;
    }
    header = mounted_archive + *offset;
    if (header[0] != '0' || header[1] != '7' || header[2] != '0' || header[3] != '7'
        || header[4] != '0' || header[5] != '1'
        || hex8(header + 54U, &entry_data_size) == 0 || hex8(header + 94U, &entry_name_size) == 0) {
        return 0;
    }
    name_offset = *offset + NEWC_HEADER_SIZE;
    if (entry_name_size == 0U || name_offset > mounted_length
        || entry_name_size > mounted_length - name_offset) {
        return 0;
    }
    data_offset = align_up4(name_offset + entry_name_size);
    if (data_offset > mounted_length || entry_data_size > mounted_length - data_offset) {
        return 0;
    }
    next_offset = align_up4(data_offset + entry_data_size);
    if (next_offset <= *offset || next_offset > mounted_length) {
        return 0;
    }
    *offset = next_offset;
    *name = mounted_archive + name_offset;
    *name_size = entry_name_size;
    *data = mounted_archive + data_offset;
    *data_size = entry_data_size;
    return 1;
}

static int cpio_find(const char *path, const uint8_t **data, uint64_t *size) {
    uint64_t offset = 0U;
    const uint8_t *name;
    const uint8_t *entry_data;
    uint64_t name_size;
    uint64_t data_size;

    if (mounted_archive == (const uint8_t *)0 || path == (const char *)0
        || data == (const uint8_t **)0 || size == (uint64_t *)0) {
        return 0;
    }
    while (next_cpio_entry(&offset, &name, &name_size, &entry_data, &data_size) != 0) {
        if (bytes_equal_fold(name, name_size, "TRAILER!!!") != 0) {
            return 0;
        }
        if (bytes_equal_fold(name, name_size, path) != 0) {
            *data = entry_data;
            *size = data_size;
            return 1;
        }
    }
    return 0;
}

static void write_u16(uint8_t *destination, uint16_t value) {
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8U);
}

static uint16_t read_u16(const uint8_t *source) {
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8U);
}

static void write_u32(uint8_t *destination, uint32_t value) {
    for (uint64_t byte = 0U; byte < 4U; byte++) {
        destination[byte] = (uint8_t)(value >> (byte * 8U));
    }
}

static uint32_t read_u32(const uint8_t *source) {
    uint32_t value = 0U;
    for (uint64_t byte = 0U; byte < 4U; byte++) {
        value |= (uint32_t)source[byte] << (byte * 8U);
    }
    return value;
}

static void write_u64(uint8_t *destination, uint64_t value) {
    for (uint64_t byte = 0U; byte < 8U; byte++) {
        destination[byte] = (uint8_t)(value >> (byte * 8U));
    }
}

static uint64_t read_u64(const uint8_t *source) {
    uint64_t value = 0U;
    for (uint64_t byte = 0U; byte < 8U; byte++) {
        value |= (uint64_t)source[byte] << (byte * 8U);
    }
    return value;
}

static void clear_node(struct persistent_node *node) {
    for (uint64_t byte = 0U; byte < sizeof(*node); byte++) {
        ((uint8_t *)node)[byte] = 0U;
    }
}

static void clear_tmp_node(struct tmp_node *node) {
    for (uint64_t byte = 0U; byte < sizeof(*node); byte++) {
        ((uint8_t *)node)[byte] = 0U;
    }
}

static void persistent_reset(void) {
    for (uint64_t index = 0U; index < VFS_PERSIST_MAX_FILES; index++) {
        clear_node(&persistent_nodes[index]);
    }
    for (uint64_t index = 0U; index < sizeof(persistent_bitmap); index++) {
        persistent_bitmap[index] = 0U;
    }
    persistent_mounted = 0U;
}

static void tmp_reset(void) {
    for (uint64_t index = 0U; index < VFS_TMPFS_MAX_FILES; index++) {
        clear_tmp_node(&tmp_nodes[index]);
    }
}

static uint64_t record_lba(uint64_t index) {
    return PERSIST_RECORD_LBA + (index * PERSIST_RECORD_SIZE) / AHCI_SECTOR_SIZE;
}

static uint64_t record_offset(uint64_t index) {
    return (index * PERSIST_RECORD_SIZE) % AHCI_SECTOR_SIZE;
}

static void encode_node(uint8_t *record, const struct persistent_node *node) {
    for (uint64_t index = 0U; index < PERSIST_RECORD_SIZE; index++) {
        record[index] = 0U;
    }
    record[0] = node->used;
    record[1] = node->type;
    record[2] = node->name_length;
    record[3] = node->flags;
    write_u16(record + 4U, node->parent);
    write_u16(record + 6U, node->reserved);
    write_u64(record + 8U, node->size);
    for (uint64_t index = 0U; index < VFS_PERSIST_EXTENT_MAX; index++) {
        write_u32(record + 16U + index * 8U, node->extent[index].start_block);
        write_u32(record + 20U + index * 8U, node->extent[index].block_count);
    }
    for (uint64_t index = 0U; index < VFS_NAME_MAX; index++) {
        record[64U + index] = (uint8_t)node->name[index];
    }
}

static void decode_node(struct persistent_node *node, const uint8_t *record) {
    clear_node(node);
    node->used = record[0];
    node->type = record[1];
    node->name_length = record[2];
    node->flags = record[3];
    node->parent = read_u16(record + 4U);
    node->reserved = read_u16(record + 6U);
    node->size = read_u64(record + 8U);
    for (uint64_t index = 0U; index < VFS_PERSIST_EXTENT_MAX; index++) {
        node->extent[index].start_block = read_u32(record + 16U + index * 8U);
        node->extent[index].block_count = read_u32(record + 20U + index * 8U);
    }
    for (uint64_t index = 0U; index < VFS_NAME_MAX; index++) {
        node->name[index] = (char)record[64U + index];
    }
}

static void decode_node_mypfs003(struct persistent_node *node, const uint8_t *record) {
    clear_node(node);
    node->used = record[0];
    node->type = record[1];
    node->name_length = record[2];
    node->flags = record[3];
    node->parent = read_u16(record + 4U);
    node->reserved = read_u16(record + 6U);
    node->size = read_u64(record + 8U);
    node->extent[0].start_block = read_u32(record + 16U);
    node->extent[0].block_count = read_u32(record + 20U);
    for (uint64_t index = 0U; index < VFS_NAME_MAX; index++) {
        node->name[index] = (char)record[24U + index];
    }
}

static int persistent_store_node(uint64_t index) {
    const uint64_t offset = record_offset(index);

    if (index >= VFS_PERSIST_MAX_FILES || ahci_read_sector(record_lba(index), sector_buffer) == 0) {
        return 0;
    }
    encode_node(sector_buffer + offset, &persistent_nodes[index]);
    return ahci_write_data_sector(record_lba(index), sector_buffer);
}

static int persistent_store_bitmap(void) {
    for (uint64_t sector = 0U; sector < PERSIST_BITMAP_SECTORS; sector++) {
        if (ahci_write_data_sector(PERSIST_BITMAP_LBA + sector,
                                   persistent_bitmap + sector * AHCI_SECTOR_SIZE) == 0) {
            return 0;
        }
    }
    return 1;
}

static int bitmap_is_set(uint64_t block) {
    return block < PERSIST_DATA_BLOCKS
           && (persistent_bitmap[block / 8U] & (uint8_t)(UINT8_C(1) << (block % 8U))) != 0U;
}

static void bitmap_set(uint64_t block, int used) {
    const uint8_t mask = (uint8_t)(UINT8_C(1) << (block % 8U));

    if (block >= PERSIST_DATA_BLOCKS) {
        return;
    }
    if (used != 0) {
        persistent_bitmap[block / 8U] |= mask;
    } else {
        persistent_bitmap[block / 8U] &= (uint8_t)~mask;
    }
}

static uint64_t node_block_capacity(const struct persistent_node *node) {
    uint64_t total = 0U;

    if (node == (const struct persistent_node *)0) { return 0U; }
    for (uint64_t index = 0U; index < VFS_PERSIST_EXTENT_MAX; index++) {
        total += node->extent[index].block_count;
    }
    return total;
}

static int blocks_are_free(uint64_t start, uint64_t count) {
    if (start >= PERSIST_DATA_BLOCKS || count > PERSIST_DATA_BLOCKS - start) { return 0; }
    for (uint64_t index = 0U; index < count; index++) {
        if (bitmap_is_set(start + index) != 0) { return 0; }
    }
    return 1;
}

static int allocate_run(uint64_t needed, uint32_t *start_block) {
    if (needed == 0U || start_block == (uint32_t *)0 || needed > PERSIST_DATA_BLOCKS) { return 0; }
    for (uint64_t start = 0U; start + needed <= PERSIST_DATA_BLOCKS; start++) {
        if (blocks_are_free(start, needed) != 0) {
            for (uint64_t block = 0U; block < needed; block++) { bitmap_set(start + block, 1); }
            *start_block = (uint32_t)start;
            return persistent_store_bitmap();
        }
    }
    return 0;
}

static int persistent_expand_file(struct persistent_node *node, uint64_t byte_count) {
    uint64_t required;
    uint64_t current;
    uint64_t missing;
    uint64_t reserve;
    const uint64_t maximum = VFS_PERSIST_FILE_CAPACITY / AHCI_SECTOR_SIZE;
    int last = -1;
    int free_extent = -1;

    if (node == (struct persistent_node *)0 || node->type != VFS_OBJECT_REGULAR
        || byte_count > VFS_PERSIST_FILE_CAPACITY) { return 0; }
    required = (byte_count + AHCI_SECTOR_SIZE - 1U) / AHCI_SECTOR_SIZE;
    current = node_block_capacity(node);
    if (required <= current) { return 1; }
    missing = required - current;
    reserve = missing < PERSIST_GROW_BLOCKS ? PERSIST_GROW_BLOCKS : missing;
    if (reserve > maximum - current) { reserve = maximum - current; }
    for (uint64_t index = 0U; index < VFS_PERSIST_EXTENT_MAX; index++) {
        if (node->extent[index].block_count != 0U) { last = (int)index; }
        else if (free_extent < 0) { free_extent = (int)index; }
    }
    if (last >= 0) {
        const uint64_t next = (uint64_t)node->extent[last].start_block + node->extent[last].block_count;
        if (blocks_are_free(next, reserve) != 0) {
            for (uint64_t block = 0U; block < reserve; block++) { bitmap_set(next + block, 1); }
            node->extent[last].block_count += (uint32_t)reserve;
            return persistent_store_bitmap();
        }
    }
    if (free_extent < 0 || reserve > UINT32_MAX || allocate_run(reserve, &node->extent[free_extent].start_block) == 0) {
        return 0;
    }
    node->extent[free_extent].block_count = (uint32_t)reserve;
    return 1;
}

static int logical_block_to_physical(const struct persistent_node *node, uint64_t logical, uint64_t *physical) {
    uint64_t remaining = logical;

    if (node == (const struct persistent_node *)0 || physical == (uint64_t *)0) { return 0; }
    for (uint64_t index = 0U; index < VFS_PERSIST_EXTENT_MAX; index++) {
        const uint64_t count = node->extent[index].block_count;
        if (remaining < count) {
            *physical = (uint64_t)node->extent[index].start_block + remaining;
            return *physical < PERSIST_DATA_BLOCKS;
        }
        remaining -= count;
    }
    return 0;
}

static void release_extent(const struct persistent_node *node) {
    if (node == (const struct persistent_node *)0 || node->type != VFS_OBJECT_REGULAR) { return; }
    for (uint64_t extent = 0U; extent < VFS_PERSIST_EXTENT_MAX; extent++) {
        for (uint64_t block = 0U; block < node->extent[extent].block_count
             && (uint64_t)node->extent[extent].start_block + block < PERSIST_DATA_BLOCKS; block++) {
            bitmap_set((uint64_t)node->extent[extent].start_block + block, 0);
        }
    }
}

static int persistent_read_bytes(const struct persistent_node *node, uint64_t offset,
                                 uint8_t *data, uint64_t length) {
    uint64_t copied = 0U;

    if (node == (const struct persistent_node *)0 || data == (uint8_t *)0
        || node->type != VFS_OBJECT_REGULAR || offset > node->size || length > node->size - offset) {
        return 0;
    }
    while (copied < length) {
        const uint64_t position = offset + copied;
        const uint64_t sector_offset = position % AHCI_SECTOR_SIZE;
        uint64_t sector;
        const uint64_t chunk = length - copied < AHCI_SECTOR_SIZE - sector_offset
                                   ? length - copied : AHCI_SECTOR_SIZE - sector_offset;

        if (logical_block_to_physical(node, position / AHCI_SECTOR_SIZE, &sector) == 0
            || ahci_read_sector(PERSIST_DATA_LBA + sector, sector_buffer) == 0) {
            return 0;
        }
        for (uint64_t index = 0U; index < chunk; index++) {
            data[copied + index] = sector_buffer[sector_offset + index];
        }
        copied += chunk;
    }
    return 1;
}

static int persistent_write_bytes(struct persistent_node *node, uint64_t offset,
                                  const uint8_t *data, uint64_t length) {
    uint64_t copied = 0U;

    if (node == (struct persistent_node *)0 || (data == (const uint8_t *)0 && length != 0U)
        || node->type != VFS_OBJECT_REGULAR || offset > VFS_PERSIST_FILE_CAPACITY
        || length > VFS_PERSIST_FILE_CAPACITY - offset
        || persistent_expand_file(node, offset + length) == 0) {
        return 0;
    }
    while (copied < length) {
        const uint64_t position = offset + copied;
        const uint64_t sector_offset = position % AHCI_SECTOR_SIZE;
        uint64_t sector;
        const uint64_t chunk = length - copied < AHCI_SECTOR_SIZE - sector_offset
                                   ? length - copied : AHCI_SECTOR_SIZE - sector_offset;

        if (logical_block_to_physical(node, position / AHCI_SECTOR_SIZE, &sector) == 0
            || ahci_read_sector(PERSIST_DATA_LBA + sector, sector_buffer) == 0) {
            return 0;
        }
        for (uint64_t index = 0U; index < chunk; index++) {
            sector_buffer[sector_offset + index] = data[copied + index];
        }
        if (ahci_write_data_sector(PERSIST_DATA_LBA + sector, sector_buffer) == 0) {
            return 0;
        }
        copied += chunk;
    }
    return 1;
}

static int node_name_valid(const char *name) {
    const uint64_t length = text_length(name, VFS_NAME_MAX);

    if (length == 0U || length >= VFS_NAME_MAX || text_equal_fold(name, ".") != 0
        || text_equal_fold(name, "..") != 0) {
        return 0;
    }
    for (uint64_t index = 0U; index < length; index++) {
        if (name[index] == '/' || name[index] < ' ' || name[index] > '~') {
            return 0;
        }
    }
    return 1;
}

static int persistent_find_child(uint16_t parent, const char *name) {
    for (uint64_t index = 0U; index < VFS_PERSIST_MAX_FILES; index++) {
        if (persistent_nodes[index].used != 0U && persistent_nodes[index].parent == parent
            && text_equal_fold(persistent_nodes[index].name, name) != 0) {
            return (int)index;
        }
    }
    return -1;
}

static int tmp_find_child(uint16_t parent, const char *name) {
    for (uint64_t index = 0U; index < VFS_TMPFS_MAX_FILES; index++) {
        if (tmp_nodes[index].used != 0U && tmp_nodes[index].parent == parent
            && text_equal_fold(tmp_nodes[index].name, name) != 0) {
            return (int)index;
        }
    }
    return -1;
}

static int target_child(struct vfs_target current, const char *name, struct vfs_target *next) {
    int index;

    if (next == (struct vfs_target *)0) {
        return 0;
    }
    if (current.kind == TARGET_ROOT) {
        if (text_equal_fold(name, "system") != 0) {
            next->kind = TARGET_SYSTEM; next->index = 0U; return 1;
        }
        if (text_equal_fold(name, "apps") != 0) {
            next->kind = TARGET_PERSIST; next->index = 3U; return persistent_nodes[3U].used != 0U;
        }
        if (text_equal_fold(name, "users") != 0) {
            next->kind = TARGET_PERSIST; next->index = 4U; return persistent_nodes[4U].used != 0U;
        }
        if (text_equal_fold(name, "temp") != 0) {
            next->kind = TARGET_TEMP_ROOT; next->index = 0U; return 1;
        }
        return 0;
    }
    if (current.kind == TARGET_SYSTEM) {
        if (text_equal_fold(name, "core") != 0) {
            next->kind = TARGET_CORE; next->index = 0U; return 1;
        }
        if (text_equal_fold(name, "live") != 0) {
            next->kind = TARGET_LIVE; next->index = 0U; return 1;
        }
        if (text_equal_fold(name, "data") != 0) {
            next->kind = TARGET_PERSIST; next->index = 1U; return persistent_nodes[1U].used != 0U;
        }
        if (text_equal_fold(name, "config") != 0) {
            next->kind = TARGET_PERSIST; next->index = 2U; return persistent_nodes[2U].used != 0U;
        }
        return 0;
    }
    if (current.kind == TARGET_PERSIST) {
        index = persistent_find_child(current.index, name);
        if (index < 0) { return 0; }
        next->kind = TARGET_PERSIST; next->index = (uint16_t)index; return 1;
    }
    if (current.kind == TARGET_TEMP_ROOT) {
        index = tmp_find_child(TMP_PARENT_ROOT, name);
        if (index < 0) { return 0; }
        next->kind = TARGET_TEMP; next->index = (uint16_t)index; return 1;
    }
    if (current.kind == TARGET_TEMP) {
        index = tmp_find_child(current.index, name);
        if (index < 0) { return 0; }
        next->kind = TARGET_TEMP; next->index = (uint16_t)index; return 1;
    }
    return 0;
}

static int resolve_mutable(const struct path_parts *parts, struct vfs_target *target) {
    struct vfs_target current = { TARGET_ROOT, 0U };

    if (parts == (const struct path_parts *)0 || target == (struct vfs_target *)0) {
        return 0;
    }
    for (uint64_t index = 0U; index < parts->count; index++) {
        if (target_child(current, parts->item[index], &current) == 0) {
            return 0;
        }
    }
    *target = current;
    return 1;
}

static int resolve_parent(const struct path_parts *parts, struct vfs_target *parent, const char **name) {
    struct path_parts parent_parts;

    if (parts == (const struct path_parts *)0 || parent == (struct vfs_target *)0 || name == (const char **)0
        || parts->count == 0U || node_name_valid(parts->item[parts->count - 1U]) == 0) {
        return 0;
    }
    parent_parts.count = parts->count - 1U;
    for (uint64_t index = 0U; index < parent_parts.count; index++) {
        text_copy(parent_parts.item[index], VFS_NAME_MAX, parts->item[index]);
    }
    if (resolve_mutable(&parent_parts, parent) == 0) {
        return 0;
    }
    *name = parts->item[parts->count - 1U];
    return 1;
}

static int create_persistent_node(uint16_t parent, const char *name, uint8_t type) {
    int free_index = -1;
    struct persistent_node *node;

    if (persistent_find_child(parent, name) >= 0 || (type != VFS_OBJECT_REGULAR && type != VFS_OBJECT_DIRECTORY)) {
        return 0;
    }
    for (uint64_t index = 0U; index < VFS_PERSIST_MAX_FILES; index++) {
        if (persistent_nodes[index].used == 0U) {
            free_index = (int)index;
            break;
        }
    }
    if (free_index < 0) {
        return 0;
    }
    node = &persistent_nodes[free_index];
    clear_node(node);
    node->used = 1U;
    node->type = type;
    node->parent = parent;
    node->name_length = (uint8_t)text_length(name, VFS_NAME_MAX);
    text_copy(node->name, VFS_NAME_MAX, name);
    if (persistent_store_node((uint64_t)free_index) == 0) {
        clear_node(node);
        return 0;
    }
    return 1;
}

static int create_tmp_node(uint16_t parent, const char *name, uint8_t type) {
    int free_index = -1;
    struct tmp_node *node;

    if (tmp_find_child(parent, name) >= 0 || (type != VFS_OBJECT_REGULAR && type != VFS_OBJECT_DIRECTORY)) {
        return 0;
    }
    for (uint64_t index = 0U; index < VFS_TMPFS_MAX_FILES; index++) {
        if (tmp_nodes[index].used == 0U) {
            free_index = (int)index;
            break;
        }
    }
    if (free_index < 0) { return 0; }
    node = &tmp_nodes[free_index];
    clear_tmp_node(node);
    node->used = 1U;
    node->type = type;
    node->parent = parent;
    node->name_length = (uint8_t)text_length(name, VFS_NAME_MAX);
    text_copy(node->name, VFS_NAME_MAX, name);
    return 1;
}

static int persistent_store_superblocks(uint8_t revision) {
    uint8_t superblock[AHCI_SECTOR_SIZE] = { 0U };

    superblock[0] = 'M'; superblock[1] = 'Y'; superblock[2] = 'P'; superblock[3] = 'F';
    superblock[4] = 'S'; superblock[5] = '0'; superblock[6] = '0'; superblock[7] = revision;
    write_u32(superblock + 8U, (uint32_t)VFS_PERSIST_MAX_FILES);
    write_u32(superblock + 12U, (uint32_t)PERSIST_DATA_BLOCKS);
    return ahci_write_data_sector(PERSIST_METADATA_LBA, superblock) != 0
           && ahci_write_data_sector(PERSIST_METADATA_LBA + 1U, superblock) != 0;
}

static int persistent_format(void) {
    persistent_reset();
    if (persistent_store_superblocks('4') == 0) {
        return 0;
    }
    for (uint64_t sector = 0U; sector < PERSIST_RECORD_SECTORS; sector++) {
        for (uint64_t byte = 0U; byte < AHCI_SECTOR_SIZE; byte++) { sector_buffer[byte] = 0U; }
        if (ahci_write_data_sector(PERSIST_RECORD_LBA + sector, sector_buffer) == 0) { return 0; }
    }
    if (persistent_store_bitmap() == 0) { return 0; }
    persistent_nodes[0].used = 1U;
    persistent_nodes[0].type = VFS_OBJECT_DIRECTORY;
    persistent_nodes[0].parent = PERSIST_PARENT_ROOT;
    if (persistent_store_node(0U) == 0
        || create_persistent_node(PERSIST_PARENT_SYSTEM, "data", VFS_OBJECT_DIRECTORY) == 0
        || create_persistent_node(PERSIST_PARENT_SYSTEM, "config", VFS_OBJECT_DIRECTORY) == 0
        || create_persistent_node(PERSIST_PARENT_ROOT, "apps", VFS_OBJECT_DIRECTORY) == 0
        || create_persistent_node(PERSIST_PARENT_ROOT, "users", VFS_OBJECT_DIRECTORY) == 0
        || create_persistent_node(4U, "myos", VFS_OBJECT_DIRECTORY) == 0
        || create_persistent_node(5U, "files", VFS_OBJECT_DIRECTORY) == 0
        || create_persistent_node(6U, "notes", VFS_OBJECT_DIRECTORY) == 0
        || create_persistent_node(6U, "imported", VFS_OBJECT_DIRECTORY) == 0
        || create_persistent_node(5U, "projects", VFS_OBJECT_DIRECTORY) == 0
        || create_persistent_node(5U, "data", VFS_OBJECT_DIRECTORY) == 0
        || create_persistent_node(5U, "config", VFS_OBJECT_DIRECTORY) == 0) {
        return 0;
    }
    persistent_mounted = 1U;
    return 1;
}

static int journal_present(uint8_t *journal) {
    return journal[0] == 'M' && journal[1] == '3' && journal[2] == 'M' && journal[3] == 'G';
}

static int journal_mypfs004_present(const uint8_t *journal) {
    return journal != (const uint8_t *)0 && journal[0] == 'M' && journal[1] == '4'
           && journal[2] == 'M' && journal[3] == 'G';
}

static int legacy_path_to_new(const char *legacy, char *path, uint64_t capacity) {
    const char *suffix = legacy;
    const char *prefix;

    if (text_starts_with_fold(legacy, "disk/bin/") != 0) {
        suffix = legacy + 9U;
        prefix = "/apps/";
        if (text_length(prefix, capacity) + text_length(suffix, capacity) + 10U >= capacity) { return 0; }
        text_copy(path, capacity, prefix);
        for (uint64_t index = text_length(path, capacity); suffix[index - text_length(prefix, capacity)] != '\0'; index++) {
            path[index] = suffix[index - text_length(prefix, capacity)];
            path[index + 1U] = '\0';
        }
        {
            const uint64_t length = text_length(path, capacity);
            path[length] = '/'; path[length + 1U] = 'm'; path[length + 2U] = 'a'; path[length + 3U] = 'i';
            path[length + 4U] = 'n'; path[length + 5U] = '.'; path[length + 6U] = 'e'; path[length + 7U] = 'l';
            path[length + 8U] = 'f'; path[length + 9U] = '\0';
        }
        return 1;
    }
    if (text_equal_fold(legacy, "disk/note") != 0) {
        text_copy(path, capacity, "/users/myos/files/notes/note");
        return 1;
    }
    if (text_starts_with_fold(legacy, "disk/") != 0) {
        suffix = legacy + 5U;
        text_copy(path, capacity, "/users/myos/files/imported/");
        {
            uint64_t offset = text_length(path, capacity);
            for (uint64_t index = 0U; suffix[index] != '\0' && offset + 1U < capacity; index++) {
                path[offset++] = suffix[index];
            }
            path[offset] = '\0';
        }
        return node_name_valid(suffix);
    }
    return 0;
}

static int migrate_legacy_records(const uint8_t *journal) {
    const uint64_t count = journal[4];

    if (count > 8U || persistent_format() == 0) {
        return 0;
    }
    for (uint64_t index = 0U; index < count; index++) {
        const uint64_t record_offset = JOURNAL_HEADER_SIZE + index * JOURNAL_RECORD_SIZE;
        const uint64_t size = read_u64(journal + record_offset + 1U);
        char legacy[41];
        char new_path[VFS_PATH_MAX];
        struct path_parts parts;
        struct vfs_target parent;
        const char *name;

        for (uint64_t character = 0U; character < 40U; character++) {
            legacy[character] = (char)journal[record_offset + 9U + character];
        }
        legacy[40] = '\0';
        if (journal[record_offset] == 0U) { continue; }
        if (size > 32768U || legacy_path_to_new(legacy, new_path, sizeof(new_path)) == 0
            || parse_path(new_path, &parts) == 0) {
            return 0;
        }
        if (parts.count == 3U && text_equal_fold(parts.item[0], "apps") != 0) {
            const int apps_index = persistent_find_child(PERSIST_PARENT_ROOT, "apps");

            if (apps_index < 0 || (create_persistent_node((uint16_t)apps_index, parts.item[1], VFS_OBJECT_DIRECTORY) == 0
                                   && persistent_find_child((uint16_t)apps_index, parts.item[1]) < 0)) {
                return 0;
            }
        }
        if (resolve_parent(&parts, &parent, &name) == 0) { return 0; }
        if (parent.kind == TARGET_PERSIST && create_persistent_node(parent.index, name, VFS_OBJECT_REGULAR) != 0) {
            int node_index = persistent_find_child(parent.index, name);
            uint64_t copied = 0U;

            if (node_index < 0) { return 0; }
            while (copied < size) {
                const uint64_t chunk = size - copied < AHCI_SECTOR_SIZE ? size - copied : AHCI_SECTOR_SIZE;
                if (ahci_read_sector(PERSIST_STAGING_LBA + index * LEGACY_FILE_SECTORS + copied / AHCI_SECTOR_SIZE,
                                     migration_buffer) == 0
                    || persistent_write_bytes(&persistent_nodes[node_index], copied, migration_buffer, chunk) == 0) {
                    return 0;
                }
                copied += chunk;
            }
            persistent_nodes[node_index].size = size;
            if (persistent_store_node((uint64_t)node_index) == 0) { return 0; }
        } else {
            return 0;
        }
    }
    for (uint64_t byte = 0U; byte < AHCI_SECTOR_SIZE; byte++) { sector_buffer[byte] = 0U; }
    return ahci_write_data_sector(PERSIST_JOURNAL_LBA, sector_buffer);
}

static int stage_legacy_migration(void) {
    uint8_t metadata[AHCI_SECTOR_SIZE];
    uint8_t journal[AHCI_SECTOR_SIZE] = { 0U };
    uint64_t count = 0U;

    if (ahci_read_sector(PERSIST_METADATA_LBA, metadata) == 0) { return 0; }
    journal[0] = 'M'; journal[1] = '3'; journal[2] = 'M'; journal[3] = 'G';
    for (uint64_t index = 0U; index < 8U; index++) {
        const uint64_t offset = LEGACY_RECORD_OFFSET + index * LEGACY_RECORD_SIZE;
        const uint64_t size = read_u64(metadata + offset + 1U);

        if (metadata[offset] == 0U) { continue; }
        if (size > 32768U || count >= 8U) { return 0; }
        journal[JOURNAL_HEADER_SIZE + count * JOURNAL_RECORD_SIZE] = 1U;
        write_u64(journal + JOURNAL_HEADER_SIZE + count * JOURNAL_RECORD_SIZE + 1U, size);
        for (uint64_t character = 0U; character < 40U; character++) {
            journal[JOURNAL_HEADER_SIZE + count * JOURNAL_RECORD_SIZE + 9U + character] = metadata[offset + 9U + character];
        }
        for (uint64_t sector = 0U; sector < LEGACY_FILE_SECTORS; sector++) {
            if (ahci_read_sector(PERSIST_METADATA_LBA + 1U + index * LEGACY_FILE_SECTORS + sector, sector_buffer) == 0
                || ahci_write_data_sector(PERSIST_STAGING_LBA + index * LEGACY_FILE_SECTORS + sector,
                                          sector_buffer) == 0) {
                return 0;
            }
        }
        count++;
    }
    journal[4] = (uint8_t)count;
    if (ahci_write_data_sector(PERSIST_JOURNAL_LBA, journal) == 0) { return 0; }
    return migrate_legacy_records(journal);
}

static int finish_mypfs003_migration(void) {
    uint8_t journal_clear[AHCI_SECTOR_SIZE] = { 0U };

    persistent_reset();
    for (uint64_t index = 0U; index < VFS_PERSIST_MAX_FILES; index++) {
        const uint64_t offset = record_offset(index);
        if (ahci_read_sector(PERSIST_STAGING_LBA + (index * PERSIST_RECORD_SIZE) / AHCI_SECTOR_SIZE, sector_buffer) == 0) {
            return 0;
        }
        decode_node_mypfs003(&persistent_nodes[index], sector_buffer + offset);
        if (persistent_nodes[index].used > 1U
            || (persistent_nodes[index].used != 0U && persistent_nodes[index].type != VFS_OBJECT_REGULAR
                && persistent_nodes[index].type != VFS_OBJECT_DIRECTORY)
            || (persistent_nodes[index].used != 0U && persistent_nodes[index].name_length >= VFS_NAME_MAX)) {
            return 0;
        }
    }
    for (uint64_t sector = 0U; sector < PERSIST_BITMAP_SECTORS; sector++) {
        if (ahci_read_sector(PERSIST_BITMAP_LBA + sector, persistent_bitmap + sector * AHCI_SECTOR_SIZE) == 0) {
            return 0;
        }
    }
    for (uint64_t index = 0U; index < VFS_PERSIST_MAX_FILES; index++) {
        if (persistent_store_node(index) == 0) { return 0; }
    }
    if (persistent_store_superblocks('4') == 0
        || ahci_write_data_sector(PERSIST_JOURNAL_LBA, journal_clear) == 0) {
        return 0;
    }
    persistent_mounted = 1U;
    return 1;
}

static int stage_mypfs003_migration(void) {
    uint8_t journal[AHCI_SECTOR_SIZE] = { 0U };

    for (uint64_t sector = 0U; sector < PERSIST_RECORD_SECTORS; sector++) {
        if (ahci_read_sector(PERSIST_RECORD_LBA + sector, sector_buffer) == 0
            || ahci_write_data_sector(PERSIST_STAGING_LBA + sector, sector_buffer) == 0) {
            return 0;
        }
    }
    journal[0] = 'M'; journal[1] = '4'; journal[2] = 'M'; journal[3] = 'G';
    if (ahci_write_data_sector(PERSIST_JOURNAL_LBA, journal) == 0) { return 0; }
    return finish_mypfs003_migration();
}

int vfs_mount_persistent(void) {
    uint8_t superblock[AHCI_SECTOR_SIZE];
    uint8_t journal[AHCI_SECTOR_SIZE];

    persistent_reset();
    tmp_reset();
    if (ahci_read_sector(PERSIST_JOURNAL_LBA, journal) == 0 || ahci_read_sector(PERSIST_METADATA_LBA, superblock) == 0) {
        return 0;
    }
    if (journal_mypfs004_present(journal) != 0) {
        return finish_mypfs003_migration();
    }
    if (journal_present(journal) != 0) {
        return migrate_legacy_records(journal);
    }
    if (superblock[0] == 'M' && superblock[1] == 'Y' && superblock[2] == 'P' && superblock[3] == 'F'
        && superblock[4] == 'S' && superblock[5] == '0' && superblock[6] == '0' && superblock[7] == '4') {
        for (uint64_t index = 0U; index < VFS_PERSIST_MAX_FILES; index++) {
            const uint64_t offset = record_offset(index);
            if (ahci_read_sector(record_lba(index), sector_buffer) == 0) { return 0; }
            decode_node(&persistent_nodes[index], sector_buffer + offset);
            if (persistent_nodes[index].used > 1U
                || (persistent_nodes[index].used != 0U && persistent_nodes[index].type != VFS_OBJECT_REGULAR
                    && persistent_nodes[index].type != VFS_OBJECT_DIRECTORY)
                || (persistent_nodes[index].used != 0U && persistent_nodes[index].name_length >= VFS_NAME_MAX)
                || (persistent_nodes[index].used != 0U && persistent_nodes[index].type == VFS_OBJECT_REGULAR
                    && persistent_nodes[index].size > VFS_PERSIST_FILE_CAPACITY)) {
                return 0;
            }
        }
        for (uint64_t sector = 0U; sector < PERSIST_BITMAP_SECTORS; sector++) {
            if (ahci_read_sector(PERSIST_BITMAP_LBA + sector, persistent_bitmap + sector * AHCI_SECTOR_SIZE) == 0) {
                return 0;
            }
        }
        persistent_mounted = 1U;
        return 1;
    }
    if (superblock[0] == 'M' && superblock[1] == 'Y' && superblock[2] == 'P' && superblock[3] == 'F'
        && superblock[4] == 'S' && superblock[5] == '0' && superblock[6] == '0' && superblock[7] == '3') {
        return stage_mypfs003_migration();
    }
    if (superblock[0] == 'M' && superblock[1] == 'Y' && superblock[2] == 'P' && superblock[3] == 'F'
        && superblock[4] == 'S' && superblock[5] == '0' && superblock[6] == '0'
        && (superblock[7] == '1' || superblock[7] == '2')) {
        return stage_legacy_migration();
    }
    return persistent_format();
}

int vfs_mount_newc(const void *archive, uint64_t length) {
    uint64_t offset = 0U;
    uint64_t count = 0U;
    const uint8_t *name;
    const uint8_t *data;
    uint64_t name_size;
    uint64_t data_size;

    tmp_reset();
    mounted_archive = (const uint8_t *)0;
    mounted_length = 0U;
    mounted_files = 0U;
    if (archive == (const void *)0 || length < NEWC_HEADER_SIZE) { return 0; }
    mounted_archive = (const uint8_t *)archive;
    mounted_length = length;
    while (next_cpio_entry(&offset, &name, &name_size, &data, &data_size) != 0) {
        if (bytes_equal_fold(name, name_size, "TRAILER!!!") != 0) {
            mounted_files = count;
            return 1;
        }
        count++;
    }
    mounted_archive = (const uint8_t *)0;
    mounted_length = 0U;
    return 0;
}

static int core_path(const struct path_parts *parts) {
    return parts->count >= 2U && text_equal_fold(parts->item[0], "system") != 0
           && text_equal_fold(parts->item[1], "core") != 0;
}

static int live_path(const struct path_parts *parts) {
    return parts->count >= 2U && text_equal_fold(parts->item[0], "system") != 0
           && text_equal_fold(parts->item[1], "live") != 0;
}

static int parts_to_path(const struct path_parts *parts, char *path, uint64_t capacity) {
    uint64_t offset = 0U;

    if (parts == (const struct path_parts *)0 || path == (char *)0 || capacity < 2U || parts->count == 0U) { return 0; }
    for (uint64_t part = 0U; part < parts->count; part++) {
        const uint64_t length = text_length(parts->item[part], VFS_NAME_MAX);
        if (offset + length + (part + 1U < parts->count ? 1U : 0U) >= capacity) { return 0; }
        for (uint64_t character = 0U; character < length; character++) { path[offset++] = parts->item[part][character]; }
        if (part + 1U < parts->count) { path[offset++] = '/'; }
    }
    path[offset] = '\0';
    return 1;
}

static void live_append_text(uint64_t *length, const char *text) {
    while (*length + 1U < sizeof(live_buffer) && text != (const char *)0 && *text != '\0') {
        live_buffer[*length] = (uint8_t)*text;
        (*length)++;
        text++;
    }
}

static void live_append_uint(uint64_t *length, uint64_t value) {
    char digits[21];
    uint64_t count = 0U;

    if (value == 0U) { live_append_text(length, "0"); return; }
    while (value != 0U && count < sizeof(digits)) {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    }
    while (count != 0U) {
        count--;
        if (*length + 1U < sizeof(live_buffer)) { live_buffer[(*length)++] = (uint8_t)digits[count]; }
    }
}

static void live_append_field_text(uint64_t *length, const char *key, const char *value) {
    live_append_text(length, key);
    live_append_text(length, "=");
    live_append_text(length, value);
    live_append_text(length, "\n");
}

static void live_append_field_uint(uint64_t *length, const char *key, uint64_t value) {
    live_append_text(length, key);
    live_append_text(length, "=");
    live_append_uint(length, value);
    live_append_text(length, "\n");
}

static void live_append_field_ready(uint64_t *length, const char *key, uint64_t ready) {
    live_append_field_text(length, key, ready != 0U ? "ready" : "unavailable");
}

static int live_open(const struct path_parts *parts, struct vfs_file *file) {
    const struct system_inventory_boot_state *boot = system_inventory_boot_state();
    uint64_t length = 0U;

    if (parts->count == 4U && text_equal_fold(parts->item[2], "boot") != 0
        && text_equal_fold(parts->item[3], "info") != 0) {
        live_append_field_text(&length, "myos_version", "0.13.1-gui-preview.1");
        live_append_field_text(&length, "architecture", "x86_64");
        live_append_field_text(&length, "bootloader", boot->bootloader);
        live_append_field_text(&length, "bootloader_version", boot->bootloader_version);
        live_append_field_text(&length, "firmware", boot->firmware);
        live_append_field_uint(&length, "usable_memory_bytes", boot->usable_memory_bytes);
        live_append_field_uint(&length, "memory_regions", boot->memory_region_count);
        live_append_field_ready(&length, "initramfs", boot->initramfs_ready);
        live_append_field_uint(&length, "initramfs_bytes", boot->initramfs_bytes);
        live_append_field_uint(&length, "initramfs_files", boot->initramfs_files);
        live_append_field_ready(&length, "framebuffer", boot->framebuffer_ready);
        live_append_field_ready(&length, "persistent_storage", boot->persistent_ready);
    } else if (parts->count == 4U && text_equal_fold(parts->item[2], "drivers") != 0
               && text_equal_fold(parts->item[3], "framebuffer") != 0) {
        live_append_field_uint(&length, "compiled_in", 1U);
        live_append_field_ready(&length, "status", framebuffer_console_available() != 0 ? 1U : 0U);
        live_append_field_uint(&length, "columns", framebuffer_console_columns());
        live_append_field_uint(&length, "rows", framebuffer_console_rows());
        live_append_field_uint(&length, "scroll_count", framebuffer_console_scroll_count());
        live_append_field_uint(&length, "gui_session", framebuffer_gui_active() != 0 ? 1U : 0U);
    } else if (parts->count == 4U && text_equal_fold(parts->item[2], "drivers") != 0
               && text_equal_fold(parts->item[3], "keyboard") != 0) {
        live_append_field_uint(&length, "compiled_in", 1U);
        live_append_field_ready(&length, "status", boot->keyboard_ready);
        live_append_field_uint(&length, "dropped_characters", keyboard_dropped_char_count());
    } else if (parts->count == 4U && text_equal_fold(parts->item[2], "drivers") != 0
               && text_equal_fold(parts->item[3], "mouse") != 0) {
        live_append_field_uint(&length, "compiled_in", 1U);
        live_append_field_ready(&length, "status", boot->mouse_ready);
        live_append_field_uint(&length, "packets", mouse_packet_count());
        live_append_field_uint(&length, "dropped_packets", mouse_dropped_packet_count());
    } else if (parts->count == 4U && text_equal_fold(parts->item[2], "drivers") != 0
               && text_equal_fold(parts->item[3], "ahci") != 0) {
        live_append_field_uint(&length, "compiled_in", 1U);
        live_append_field_ready(&length, "controller", boot->ahci_controller_ready);
        live_append_field_ready(&length, "probe", boot->ahci_probe_ready);
        live_append_field_ready(&length, "persistent_storage", boot->persistent_ready);
        live_append_field_uint(&length, "data_lba_start", AHCI_DATA_LBA_START);
        live_append_field_uint(&length, "data_lba_end", AHCI_DATA_LBA_END);
    } else if (parts->count == 4U && text_equal_fold(parts->item[2], "drivers") != 0
               && text_equal_fold(parts->item[3], "acpi") != 0) {
        live_append_field_uint(&length, "compiled_in", 1U);
        live_append_field_ready(&length, "s5_poweroff", boot->acpi_ready);
    } else if (parts->count == 4U && text_equal_fold(parts->item[2], "drivers") != 0
               && text_equal_fold(parts->item[3], "pit") != 0) {
        live_append_field_uint(&length, "compiled_in", 1U);
        live_append_field_uint(&length, "frequency_hz", pit_frequency_hz());
    } else if (parts->count == 4U && text_equal_fold(parts->item[2], "drivers") != 0
               && text_equal_fold(parts->item[3], "rtc") != 0) {
        live_append_field_uint(&length, "compiled_in", 1U);
        live_append_field_text(&length, "status", "available");
    } else if (parts->count == 4U && text_equal_fold(parts->item[2], "drivers") != 0
               && text_equal_fold(parts->item[3], "pci") != 0) {
        live_append_field_uint(&length, "compiled_in", 1U);
        live_append_field_text(&length, "status", "available");
    } else if (parts->count == 4U && text_equal_fold(parts->item[2], "devices") != 0
               && text_equal_fold(parts->item[3], "storage") == 0) {
        live_append_field_text(&length, "driver", "ahci");
        live_append_field_ready(&length, "controller", boot->ahci_controller_ready);
        live_append_field_ready(&length, "persistent_storage", boot->persistent_ready);
    } else if (parts->count == 4U && text_equal_fold(parts->item[2], "devices") != 0
               && text_equal_fold(parts->item[3], "display") == 0) {
        live_append_field_text(&length, "driver", "framebuffer");
        live_append_field_ready(&length, "status", framebuffer_console_available() != 0 ? 1U : 0U);
        live_append_field_uint(&length, "columns", framebuffer_console_columns());
        live_append_field_uint(&length, "rows", framebuffer_console_rows());
    } else if (parts->count == 4U && text_equal_fold(parts->item[2], "devices") != 0
               && text_equal_fold(parts->item[3], "input") == 0) {
        live_append_field_ready(&length, "keyboard", boot->keyboard_ready);
        live_append_field_ready(&length, "mouse", boot->mouse_ready);
        live_append_field_uint(&length, "keyboard_dropped_characters", keyboard_dropped_char_count());
        live_append_field_uint(&length, "mouse_packets", mouse_packet_count());
    } else if (parts->count == 4U && text_equal_fold(parts->item[2], "devices") != 0
               && text_equal_fold(parts->item[3], "clock") == 0) {
        live_append_field_uint(&length, "pit_frequency_hz", pit_frequency_hz());
        live_append_field_text(&length, "rtc", "available");
    } else if (parts->count == 5U && text_equal_fold(parts->item[2], "processes") != 0
               && text_equal_fold(parts->item[4], "info") != 0) {
        uint64_t pid = 0U;
        struct myos_task_info info;
        for (uint64_t index = 0U; parts->item[3][index] != '\0'; index++) {
            if (parts->item[3][index] < '0' || parts->item[3][index] > '9') { return 0; }
            pid = pid * 10U + (uint64_t)(parts->item[3][index] - '0');
        }
        if (scheduler_task_info(pid, &info) != 0 || info.state == MYOS_TASK_STATE_UNUSED) { return 0; }
        live_append_field_uint(&length, "pid", info.id);
        live_append_field_uint(&length, "state", info.state);
        live_append_field_uint(&length, "kind", info.kind);
        live_append_field_text(&length, "name", info.name);
        live_append_field_uint(&length, "run_count", info.run_count);
        live_append_field_uint(&length, "exit_status", info.exit_status);
    } else {
        return 0;
    }
    file->data = live_buffer;
    file->size = length;
    file->type = VFS_OBJECT_VIRTUAL;
    return 1;
}

int vfs_open(const char *path, struct vfs_file *file) {
    struct path_parts parts;
    struct vfs_target target;
    const uint8_t *data;
    uint64_t size;

    if (file == (struct vfs_file *)0 || parse_path(path, &parts) == 0) {
        if (path != (const char *)0 && path[0] != '/') {
            if (cpio_find(path, &data, &size) != 0) {
                file->data = data; file->size = size; file->type = VFS_OBJECT_REGULAR; return 1;
            }
        }
        return 0;
    }
    if (core_path(&parts) != 0) {
        char cpio_path[VFS_PATH_MAX];
        if (parts_to_path(&parts, cpio_path, sizeof(cpio_path)) == 0 || cpio_find(cpio_path, &data, &size) == 0) {
            return 0;
        }
        file->data = data; file->size = size; file->type = VFS_OBJECT_REGULAR; return 1;
    }
    if (live_path(&parts) != 0) { return live_open(&parts, file); }
    if (resolve_mutable(&parts, &target) == 0) { return 0; }
    if (target.kind == TARGET_PERSIST && persistent_nodes[target.index].type == VFS_OBJECT_REGULAR) {
        if (persistent_nodes[target.index].size > sizeof(persistent_open_buffer)
            || persistent_read_bytes(&persistent_nodes[target.index], 0U, persistent_open_buffer,
                                     persistent_nodes[target.index].size) == 0) { return 0; }
        file->data = persistent_open_buffer;
        file->size = persistent_nodes[target.index].size;
        file->type = VFS_OBJECT_REGULAR;
        return 1;
    }
    if (target.kind == TARGET_TEMP && tmp_nodes[target.index].type == VFS_OBJECT_REGULAR) {
        file->data = tmp_nodes[target.index].data;
        file->size = tmp_nodes[target.index].size;
        file->type = VFS_OBJECT_REGULAR;
        return 1;
    }
    return 0;
}

int vfs_read(const char *path, uint64_t offset, uint8_t *data, uint64_t length, uint64_t *read_length) {
    struct path_parts parts;
    struct vfs_target target;
    struct vfs_file file;
    uint64_t available;

    if (data == (uint8_t *)0 || read_length == (uint64_t *)0 || parse_path(path, &parts) == 0) { return 0; }
    if (core_path(&parts) != 0 || live_path(&parts) != 0) {
        if (vfs_open(path, &file) == 0 || offset >= file.size) { *read_length = 0U; return 1; }
        available = file.size - offset < length ? file.size - offset : length;
        for (uint64_t index = 0U; index < available; index++) { data[index] = file.data[offset + index]; }
        *read_length = available;
        return 1;
    }
    if (resolve_mutable(&parts, &target) == 0) { return 0; }
    if (target.kind == TARGET_PERSIST && persistent_nodes[target.index].type == VFS_OBJECT_REGULAR) {
        if (offset >= persistent_nodes[target.index].size) { *read_length = 0U; return 1; }
        available = persistent_nodes[target.index].size - offset < length ? persistent_nodes[target.index].size - offset : length;
        if (persistent_read_bytes(&persistent_nodes[target.index], offset, data, available) == 0) { return 0; }
        *read_length = available;
        return 1;
    }
    if (target.kind == TARGET_TEMP && tmp_nodes[target.index].type == VFS_OBJECT_REGULAR) {
        if (offset >= tmp_nodes[target.index].size) { *read_length = 0U; return 1; }
        available = tmp_nodes[target.index].size - offset < length ? tmp_nodes[target.index].size - offset : length;
        for (uint64_t index = 0U; index < available; index++) { data[index] = tmp_nodes[target.index].data[offset + index]; }
        *read_length = available;
        return 1;
    }
    return 0;
}

static int set_directory_entry(struct vfs_directory_entry *entry, const char *name, uint64_t type, uint64_t size) {
    if (entry == (struct vfs_directory_entry *)0) { return 0; }
    text_copy(entry->name, sizeof(entry->name), name);
    entry->type = type;
    entry->size = size;
    return 1;
}

static int list_static(uint64_t index, const char *first, uint64_t first_type, const char *second,
                       uint64_t second_type, const char *third, uint64_t third_type,
                       const char *fourth, uint64_t fourth_type, struct vfs_directory_entry *entry) {
    if (index == 0U) { return set_directory_entry(entry, first, first_type, 0U); }
    if (index == 1U && second != (const char *)0) { return set_directory_entry(entry, second, second_type, 0U); }
    if (index == 2U && third != (const char *)0) { return set_directory_entry(entry, third, third_type, 0U); }
    if (index == 3U && fourth != (const char *)0) { return set_directory_entry(entry, fourth, fourth_type, 0U); }
    return 0;
}

static int list_driver_entry(uint64_t index, struct vfs_directory_entry *entry) {
    if (index == 0U) { return set_directory_entry(entry, "framebuffer", VFS_OBJECT_VIRTUAL, 0U); }
    if (index == 1U) { return set_directory_entry(entry, "keyboard", VFS_OBJECT_VIRTUAL, 0U); }
    if (index == 2U) { return set_directory_entry(entry, "mouse", VFS_OBJECT_VIRTUAL, 0U); }
    if (index == 3U) { return set_directory_entry(entry, "ahci", VFS_OBJECT_VIRTUAL, 0U); }
    if (index == 4U) { return set_directory_entry(entry, "acpi", VFS_OBJECT_VIRTUAL, 0U); }
    if (index == 5U) { return set_directory_entry(entry, "pit", VFS_OBJECT_VIRTUAL, 0U); }
    if (index == 6U) { return set_directory_entry(entry, "rtc", VFS_OBJECT_VIRTUAL, 0U); }
    if (index == 7U) { return set_directory_entry(entry, "pci", VFS_OBJECT_VIRTUAL, 0U); }
    return 0;
}

static int cpio_list_child(const char *parent, uint64_t index, struct vfs_directory_entry *entry) {
    uint64_t offset = 0U;
    uint64_t found = 0U;
    const uint8_t *name;
    const uint8_t *data;
    uint64_t name_size;
    uint64_t data_size;
    const uint64_t parent_length = text_length(parent, VFS_PATH_MAX);

    while (next_cpio_entry(&offset, &name, &name_size, &data, &data_size) != 0) {
        char child[VFS_NAME_MAX];
        uint64_t cursor;
        uint64_t child_length = 0U;
        int directory = 0;
        int duplicate = 0;

        if (bytes_equal_fold(name, name_size, "TRAILER!!!") != 0 || name_size <= parent_length + 1U) { continue; }
        if (name[0] != '/' || text_starts_with_fold((const char *)name, parent) == 0 || name[parent_length] != '/') { continue; }
        cursor = parent_length + 1U;
        while (cursor < name_size && name[cursor] != '\0' && name[cursor] != '/' && child_length + 1U < sizeof(child)) {
            child[child_length++] = (char)name[cursor++];
        }
        child[child_length] = '\0';
        if (child_length == 0U) { continue; }
        if (cursor < name_size && name[cursor] == '/') { directory = 1; }
        {
            uint64_t prior_offset = 0U;
            const uint8_t *prior_name;
            const uint8_t *prior_data;
            uint64_t prior_name_size;
            uint64_t prior_data_size;
            while (next_cpio_entry(&prior_offset, &prior_name, &prior_name_size, &prior_data, &prior_data_size) != 0) {
                char prior_child[VFS_NAME_MAX];
                uint64_t prior_cursor;
                uint64_t prior_length = 0U;
                if (prior_offset >= offset || prior_name_size <= parent_length + 1U || prior_name[0] != '/'
                    || text_starts_with_fold((const char *)prior_name, parent) == 0 || prior_name[parent_length] != '/') { continue; }
                prior_cursor = parent_length + 1U;
                while (prior_cursor < prior_name_size && prior_name[prior_cursor] != '\0' && prior_name[prior_cursor] != '/'
                       && prior_length + 1U < sizeof(prior_child)) {
                    prior_child[prior_length++] = (char)prior_name[prior_cursor++];
                }
                prior_child[prior_length] = '\0';
                if (text_equal_fold(prior_child, child) != 0) { duplicate = 1; break; }
            }
        }
        if (duplicate != 0) { continue; }
        if (found == index) { return set_directory_entry(entry, child,
                                                           directory != 0 ? VFS_OBJECT_DIRECTORY : VFS_OBJECT_REGULAR,
                                                           directory != 0 ? 0U : data_size); }
        found++;
    }
    return 0;
}

int vfs_list(const char *path, uint64_t index, struct vfs_directory_entry *entry) {
    struct path_parts parts;
    struct vfs_target target;
    uint64_t seen = 0U;

    if (entry == (struct vfs_directory_entry *)0 || parse_path(path, &parts) == 0) { return 0; }
    if (core_path(&parts) != 0) {
        char core_path_name[VFS_PATH_MAX];
        return parts_to_path(&parts, core_path_name, sizeof(core_path_name)) != 0
               && cpio_list_child(core_path_name, index, entry) != 0;
    }
    if (live_path(&parts) != 0) {
        if (parts.count == 2U) {
            return list_static(index, "boot", VFS_OBJECT_DIRECTORY, "drivers", VFS_OBJECT_DIRECTORY,
                               "devices", VFS_OBJECT_DIRECTORY, "processes", VFS_OBJECT_DIRECTORY, entry);
        }
        if (parts.count == 3U && text_equal_fold(parts.item[2], "boot") != 0) {
            return list_static(index, "info", VFS_OBJECT_VIRTUAL, (const char *)0, 0U,
                               (const char *)0, 0U, (const char *)0, 0U, entry);
        }
        if (parts.count == 3U && text_equal_fold(parts.item[2], "drivers") != 0) {
            return list_driver_entry(index, entry);
        }
        if (parts.count == 3U && text_equal_fold(parts.item[2], "devices") != 0) {
            return list_static(index, "storage", VFS_OBJECT_VIRTUAL, "display", VFS_OBJECT_VIRTUAL,
                               "input", VFS_OBJECT_VIRTUAL, "clock", VFS_OBJECT_VIRTUAL, entry);
        }
        if (parts.count == 3U && text_equal_fold(parts.item[2], "processes") != 0) {
            for (uint64_t task = 0U; task < SCHEDULER_MAX_TASKS; task++) {
                struct myos_task_info info;
                char name[VFS_NAME_MAX];
                if (scheduler_task_info(task, &info) != 0 || info.state == MYOS_TASK_STATE_UNUSED) { continue; }
                {
                    uint64_t value = info.id;
                    char digits[21];
                    uint64_t count = 0U;
                    uint64_t name_length = 0U;

                    if (value == 0U) { digits[count++] = '0'; }
                    while (value != 0U && count < sizeof(digits)) {
                        digits[count++] = (char)('0' + value % 10U);
                        value /= 10U;
                    }
                    while (count != 0U && name_length + 1U < sizeof(name)) {
                        name[name_length++] = digits[--count];
                    }
                    name[name_length] = '\0';
                }
                if (seen == index) { return set_directory_entry(entry, name, VFS_OBJECT_DIRECTORY, 0U); }
                seen++;
            }
        }
        return 0;
    }
    if (resolve_mutable(&parts, &target) == 0) { return 0; }
    if (target.kind == TARGET_ROOT) {
        return list_static(index, "system", VFS_OBJECT_DIRECTORY, "apps", VFS_OBJECT_DIRECTORY,
                           "users", VFS_OBJECT_DIRECTORY, "temp", VFS_OBJECT_DIRECTORY, entry);
    }
    if (target.kind == TARGET_SYSTEM) {
        return list_static(index, "core", VFS_OBJECT_DIRECTORY, "data", VFS_OBJECT_DIRECTORY,
                           "config", VFS_OBJECT_DIRECTORY, "live", VFS_OBJECT_DIRECTORY, entry);
    }
    if (target.kind == TARGET_PERSIST && persistent_nodes[target.index].type == VFS_OBJECT_DIRECTORY) {
        for (uint64_t node = 0U; node < VFS_PERSIST_MAX_FILES; node++) {
            if (persistent_nodes[node].used == 0U || persistent_nodes[node].parent != target.index) { continue; }
            if (seen == index) { return set_directory_entry(entry, persistent_nodes[node].name,
                                                             persistent_nodes[node].type, persistent_nodes[node].size); }
            seen++;
        }
        return 0;
    }
    if (target.kind == TARGET_TEMP_ROOT || (target.kind == TARGET_TEMP && tmp_nodes[target.index].type == VFS_OBJECT_DIRECTORY)) {
        const uint16_t parent = target.kind == TARGET_TEMP_ROOT ? TMP_PARENT_ROOT : target.index;
        for (uint64_t node = 0U; node < VFS_TMPFS_MAX_FILES; node++) {
            if (tmp_nodes[node].used == 0U || tmp_nodes[node].parent != parent) { continue; }
            if (seen == index) { return set_directory_entry(entry, tmp_nodes[node].name,
                                                             tmp_nodes[node].type, tmp_nodes[node].size); }
            seen++;
        }
    }
    return 0;
}

static int vfs_create(const char *path, uint8_t type) {
    struct path_parts parts;
    struct vfs_target parent;
    const char *name;

    if (persistent_mounted == 0U || parse_path(path, &parts) == 0 || resolve_parent(&parts, &parent, &name) == 0) {
        return 0;
    }
    if (parent.kind == TARGET_PERSIST && persistent_nodes[parent.index].type == VFS_OBJECT_DIRECTORY) {
        return create_persistent_node(parent.index, name, type);
    }
    if (parent.kind == TARGET_TEMP_ROOT) {
        return create_tmp_node(TMP_PARENT_ROOT, name, type);
    }
    if (parent.kind == TARGET_TEMP && tmp_nodes[parent.index].type == VFS_OBJECT_DIRECTORY) {
        return create_tmp_node(parent.index, name, type);
    }
    return 0;
}

int vfs_create_file(const char *path) { return vfs_create(path, VFS_OBJECT_REGULAR); }
int vfs_create_directory(const char *path) { return vfs_create(path, VFS_OBJECT_DIRECTORY); }

int vfs_write_file(const char *path, uint64_t offset, const uint8_t *data, uint64_t length) {
    struct path_parts parts;
    struct vfs_target target;

    if (parse_path(path, &parts) == 0 || resolve_mutable(&parts, &target) == 0 || (length != 0U && data == (const uint8_t *)0)) {
        return 0;
    }
    if (target.kind == TARGET_PERSIST && persistent_nodes[target.index].type == VFS_OBJECT_REGULAR) {
        struct persistent_node *node = &persistent_nodes[target.index];
        if (offset > VFS_PERSIST_FILE_CAPACITY || length > VFS_PERSIST_FILE_CAPACITY - offset
            || persistent_write_bytes(node, offset, data, length) == 0) { return 0; }
        if (offset + length > node->size) { node->size = offset + length; }
        return persistent_store_node(target.index);
    }
    if (target.kind == TARGET_TEMP && tmp_nodes[target.index].type == VFS_OBJECT_REGULAR) {
        struct tmp_node *node = &tmp_nodes[target.index];
        if (offset > VFS_TMPFS_FILE_CAPACITY || length > VFS_TMPFS_FILE_CAPACITY - offset) { return 0; }
        for (uint64_t index = node->size; index < offset; index++) { node->data[index] = 0U; }
        for (uint64_t index = 0U; index < length; index++) { node->data[offset + index] = data[index]; }
        if (offset + length > node->size) { node->size = offset + length; }
        return 1;
    }
    return 0;
}

static int persistent_has_child(uint16_t parent) {
    for (uint64_t index = 0U; index < VFS_PERSIST_MAX_FILES; index++) {
        if (persistent_nodes[index].used != 0U && persistent_nodes[index].parent == parent) { return 1; }
    }
    return 0;
}

static int tmp_has_child(uint16_t parent) {
    for (uint64_t index = 0U; index < VFS_TMPFS_MAX_FILES; index++) {
        if (tmp_nodes[index].used != 0U && tmp_nodes[index].parent == parent) { return 1; }
    }
    return 0;
}

int vfs_remove_object(const char *path) {
    struct path_parts parts;
    struct vfs_target target;

    if (parse_path(path, &parts) == 0 || resolve_mutable(&parts, &target) == 0) { return 0; }
    if (target.kind == TARGET_PERSIST && target.index >= 12U) {
        if (persistent_nodes[target.index].type == VFS_OBJECT_DIRECTORY && persistent_has_child(target.index) != 0) { return 0; }
        release_extent(&persistent_nodes[target.index]);
        clear_node(&persistent_nodes[target.index]);
        return persistent_store_bitmap() != 0 && persistent_store_node(target.index) != 0;
    }
    if (target.kind == TARGET_TEMP) {
        if (tmp_nodes[target.index].type == VFS_OBJECT_DIRECTORY && tmp_has_child(target.index) != 0) { return 0; }
        clear_tmp_node(&tmp_nodes[target.index]);
        return 1;
    }
    return 0;
}

int vfs_get_entry(uint64_t index, char *name, uint64_t name_capacity, uint64_t *size) {
    struct vfs_directory_entry entry;

    if (name == (char *)0 || name_capacity == 0U || size == (uint64_t *)0 || vfs_list("/", index, &entry) == 0) {
        return 0;
    }
    if (text_length(entry.name, name_capacity) + 1U > name_capacity) { return 0; }
    text_copy(name, name_capacity, entry.name);
    *size = entry.size;
    return 1;
}

int vfs_tmpfs_create(const char *path) {
    char mapped[VFS_PATH_MAX];
    if (text_starts_with_fold(path, "tmp/") == 0) { return 0; }
    text_copy(mapped, sizeof(mapped), "/temp/");
    {
        uint64_t offset = text_length(mapped, sizeof(mapped));
        for (uint64_t index = 4U; path[index] != '\0' && offset + 1U < sizeof(mapped); index++) { mapped[offset++] = path[index]; }
        mapped[offset] = '\0';
    }
    return vfs_create_file(mapped);
}

int vfs_tmpfs_write(const char *path, uint64_t offset, const uint8_t *data, uint64_t length) {
    char mapped[VFS_PATH_MAX];
    if (text_starts_with_fold(path, "tmp/") == 0) { return 0; }
    text_copy(mapped, sizeof(mapped), "/temp/");
    { uint64_t destination = text_length(mapped, sizeof(mapped));
      for (uint64_t source = 4U; path[source] != '\0' && destination + 1U < sizeof(mapped); source++) { mapped[destination++] = path[source]; }
      mapped[destination] = '\0'; }
    return vfs_write_file(mapped, offset, data, length);
}

int vfs_tmpfs_remove(const char *path) {
    char mapped[VFS_PATH_MAX];
    if (text_starts_with_fold(path, "tmp/") == 0) { return 0; }
    text_copy(mapped, sizeof(mapped), "/temp/");
    { uint64_t destination = text_length(mapped, sizeof(mapped));
      for (uint64_t source = 4U; path[source] != '\0' && destination + 1U < sizeof(mapped); source++) { mapped[destination++] = path[source]; }
      mapped[destination] = '\0'; }
    return vfs_remove_object(mapped);
}

static int legacy_persistent_map(const char *path, char *mapped, uint64_t capacity) {
    if (path == (const char *)0 || legacy_path_to_new(path, mapped, capacity) == 0) { return 0; }
    return 1;
}

int vfs_persistent_create(const char *path) {
    char mapped[VFS_PATH_MAX];
    return legacy_persistent_map(path, mapped, sizeof(mapped)) != 0 && vfs_create_file(mapped) != 0;
}

int vfs_persistent_write(const char *path, uint64_t offset, const uint8_t *data, uint64_t length) {
    char mapped[VFS_PATH_MAX];
    return legacy_persistent_map(path, mapped, sizeof(mapped)) != 0 && vfs_write_file(mapped, offset, data, length) != 0;
}

int vfs_persistent_remove(const char *path) {
    char mapped[VFS_PATH_MAX];
    return legacy_persistent_map(path, mapped, sizeof(mapped)) != 0 && vfs_remove_object(mapped) != 0;
}

uint64_t vfs_file_count(void) {
    uint64_t count = mounted_files;
    for (uint64_t index = 0U; index < VFS_PERSIST_MAX_FILES; index++) {
        if (persistent_nodes[index].used != 0U && persistent_nodes[index].type == VFS_OBJECT_REGULAR) { count++; }
    }
    for (uint64_t index = 0U; index < VFS_TMPFS_MAX_FILES; index++) {
        if (tmp_nodes[index].used != 0U && tmp_nodes[index].type == VFS_OBJECT_REGULAR) { count++; }
    }
    return count;
}

uint64_t vfs_total_size(void) {
    uint64_t total = 0U;
    uint64_t offset = 0U;
    const uint8_t *name;
    const uint8_t *data;
    uint64_t name_size;
    uint64_t data_size;

    while (mounted_archive != (const uint8_t *)0 && next_cpio_entry(&offset, &name, &name_size, &data, &data_size) != 0) {
        if (bytes_equal_fold(name, name_size, "TRAILER!!!") != 0) { break; }
        total += data_size;
    }
    for (uint64_t index = 0U; index < VFS_PERSIST_MAX_FILES; index++) {
        if (persistent_nodes[index].used != 0U && persistent_nodes[index].type == VFS_OBJECT_REGULAR) { total += persistent_nodes[index].size; }
    }
    for (uint64_t index = 0U; index < VFS_TMPFS_MAX_FILES; index++) {
        if (tmp_nodes[index].used != 0U && tmp_nodes[index].type == VFS_OBJECT_REGULAR) { total += tmp_nodes[index].size; }
    }
    return total;
}
