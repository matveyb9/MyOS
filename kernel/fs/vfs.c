#include <stdint.h>

#include <vfs.h>
#include <ahci.h>

#define NEWC_HEADER_SIZE 110U
#define PERSIST_METADATA_LBA AHCI_DATA_LBA_START
#define PERSIST_FILE_LBA_BASE (AHCI_DATA_LBA_START + 1U)
#define PERSIST_RECORD_SIZE UINT64_C(56)
#define PERSIST_RECORD_OFFSET UINT64_C(16)

struct tmpfs_file {
    uint8_t used;
    char name[VFS_NAME_MAX];
    uint64_t size;
    uint8_t data[VFS_TMPFS_FILE_CAPACITY];
};

struct persistent_file {
    uint8_t used;
    char name[VFS_PERSIST_NAME_CAPACITY];
    uint64_t size;
    uint8_t data[VFS_PERSIST_FILE_CAPACITY];
};

static const uint8_t *mounted_archive;
static uint64_t mounted_length;
static uint64_t mounted_files;
static struct tmpfs_file tmpfs_files[VFS_TMPFS_MAX_FILES];
static struct persistent_file persistent_files[VFS_PERSIST_MAX_FILES];
static uint8_t persistent_mounted;

static uint64_t align_up4(uint64_t value) {
    return (value + 3U) & ~UINT64_C(3);
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

static int name_equal(const uint8_t *name, uint64_t name_size, const char *expected) {
    uint64_t index = 0U;

    if (name_size == 0U) {
        return 0;
    }
    while (expected[index] != '\0' && index + 1U < name_size) {
        if (name[index] != (uint8_t)expected[index]) {
            return 0;
        }
        index++;
    }
    return expected[index] == '\0' && index + 1U == name_size && name[index] == '\0';
}

static int text_equal(const char *left, const char *right) {
    uint64_t index = 0U;

    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) {
            return 0;
        }
        index++;
    }
    return left[index] == right[index];
}

static int tmpfs_path_is_valid(const char *path) {
    uint64_t index;

    if (path == (const char *)0 || path[0] != 't' || path[1] != 'm' || path[2] != 'p' || path[3] != '/') {
        return 0;
    }
    for (index = 4U; index < VFS_NAME_MAX; index++) {
        const char character = path[index];

        if (character == '\0') {
            return index > 4U;
        }
        if (character == '/' || character < ' ' || character > '~') {
            return 0;
        }
    }
    return 0;
}

static int tmpfs_find(const char *path) {
    for (uint64_t index = 0U; index < VFS_TMPFS_MAX_FILES; index++) {
        if (tmpfs_files[index].used != 0U && text_equal(tmpfs_files[index].name, path) != 0) {
            return (int)index;
        }
    }
    return -1;
}

static void tmpfs_clear_file(struct tmpfs_file *file) {
    for (uint64_t index = 0U; index < VFS_NAME_MAX; index++) {
        file->name[index] = '\0';
    }
    for (uint64_t index = 0U; index < VFS_TMPFS_FILE_CAPACITY; index++) {
        file->data[index] = 0U;
    }
    file->used = 0U;
    file->size = 0U;
}

static void tmpfs_reset(void) {
    for (uint64_t index = 0U; index < VFS_TMPFS_MAX_FILES; index++) {
        tmpfs_clear_file(&tmpfs_files[index]);
    }
}

static void persistent_clear_file(struct persistent_file *file) {
    for (uint64_t index = 0U; index < VFS_PERSIST_NAME_CAPACITY; index++) {
        file->name[index] = '\0';
    }
    for (uint64_t index = 0U; index < VFS_PERSIST_FILE_CAPACITY; index++) {
        file->data[index] = 0U;
    }
    file->used = 0U;
    file->size = 0U;
}

static void persistent_reset(void) {
    for (uint64_t index = 0U; index < VFS_PERSIST_MAX_FILES; index++) {
        persistent_clear_file(&persistent_files[index]);
    }
    persistent_mounted = 0U;
}

static int persistent_path_is_valid(const char *path) {
    uint64_t index;

    if (path == (const char *)0 || path[0] != 'd' || path[1] != 'i' || path[2] != 's' || path[3] != 'k' || path[4] != '/') {
        return 0;
    }
    for (index = 5U; index < VFS_PERSIST_NAME_CAPACITY; index++) {
        const char character = path[index];

        if (character == '\0') {
            return index > 5U;
        }
        if (character == '/' || character < ' ' || character > '~') {
            return 0;
        }
    }
    return 0;
}

static int persistent_find(const char *path) {
    for (uint64_t index = 0U; index < VFS_PERSIST_MAX_FILES; index++) {
        if (persistent_files[index].used != 0U && text_equal(persistent_files[index].name, path) != 0) {
            return (int)index;
        }
    }
    return -1;
}

static uint64_t persistent_record_offset(uint64_t index) {
    return PERSIST_RECORD_OFFSET + index * PERSIST_RECORD_SIZE;
}

static int persistent_store_metadata(void) {
    uint8_t sector[AHCI_SECTOR_SIZE] = { 0U };

    sector[0] = 'M'; sector[1] = 'Y'; sector[2] = 'P'; sector[3] = 'F';
    sector[4] = 'S'; sector[5] = '0'; sector[6] = '0'; sector[7] = '1';
    for (uint64_t index = 0U; index < VFS_PERSIST_MAX_FILES; index++) {
        const uint64_t offset = persistent_record_offset(index);
        const struct persistent_file *file = &persistent_files[index];

        sector[offset] = file->used;
        for (uint64_t byte = 0U; byte < 8U; byte++) {
            sector[offset + 1U + byte] = (uint8_t)(file->size >> (byte * 8U));
        }
        for (uint64_t character = 0U; character < VFS_PERSIST_NAME_CAPACITY; character++) {
            sector[offset + 9U + character] = (uint8_t)file->name[character];
        }
    }
    return ahci_write_data_sector(PERSIST_METADATA_LBA, sector);
}

int vfs_mount_persistent(void) {
    uint8_t sector[AHCI_SECTOR_SIZE];
    int valid = 1;

    persistent_reset();
    if (ahci_read_sector(PERSIST_METADATA_LBA, sector) == 0) {
        return 0;
    }
    if (sector[0] != 'M' || sector[1] != 'Y' || sector[2] != 'P' || sector[3] != 'F'
        || sector[4] != 'S' || sector[5] != '0' || sector[6] != '0' || sector[7] != '1') {
        valid = 0;
    }
    if (valid != 0) {
        for (uint64_t index = 0U; index < VFS_PERSIST_MAX_FILES; index++) {
            const uint64_t offset = persistent_record_offset(index);
            struct persistent_file *file = &persistent_files[index];

            if (sector[offset] > 1U) { valid = 0; break; }
            file->used = sector[offset];
            for (uint64_t byte = 0U; byte < 8U; byte++) {
                file->size |= (uint64_t)sector[offset + 1U + byte] << (byte * 8U);
            }
            for (uint64_t character = 0U; character < VFS_PERSIST_NAME_CAPACITY; character++) {
                file->name[character] = (char)sector[offset + 9U + character];
            }
            if (file->used != 0U && (file->size > VFS_PERSIST_FILE_CAPACITY || persistent_path_is_valid(file->name) == 0)) {
                valid = 0;
                break;
            }
            if (file->used != 0U && ahci_read_sector(PERSIST_FILE_LBA_BASE + index, file->data) == 0) {
                valid = 0;
                break;
            }
        }
    }
    if (valid == 0) {
        persistent_reset();
        if (persistent_store_metadata() == 0) {
            return 0;
        }
    }
    persistent_mounted = 1U;
    return 1;
}

int vfs_persistent_create(const char *path) {
    int slot = -1;

    if (persistent_mounted == 0U || persistent_path_is_valid(path) == 0 || persistent_find(path) >= 0) {
        return 0;
    }
    for (uint64_t index = 0U; index < VFS_PERSIST_MAX_FILES; index++) {
        if (persistent_files[index].used == 0U) { slot = (int)index; break; }
    }
    if (slot < 0) { return 0; }
    persistent_clear_file(&persistent_files[slot]);
    for (uint64_t index = 0U; index < VFS_PERSIST_NAME_CAPACITY; index++) {
        persistent_files[slot].name[index] = path[index];
        if (path[index] == '\0') { break; }
    }
    persistent_files[slot].used = 1U;
    return persistent_store_metadata();
}

int vfs_persistent_write(const char *path, uint64_t offset, const uint8_t *data, uint64_t length) {
    const int persistent_index = persistent_find(path);
    struct persistent_file *file;

    if (persistent_mounted == 0U || persistent_index < 0 || offset > VFS_PERSIST_FILE_CAPACITY
        || length > VFS_PERSIST_FILE_CAPACITY - offset || (length != 0U && data == (const uint8_t *)0)) {
        return 0;
    }
    file = &persistent_files[persistent_index];
    for (uint64_t index = file->size; index < offset; index++) { file->data[index] = 0U; }
    for (uint64_t index = 0U; index < length; index++) { file->data[offset + index] = data[index]; }
    if (offset + length > file->size) { file->size = offset + length; }
    if (ahci_write_data_sector(PERSIST_FILE_LBA_BASE + (uint64_t)persistent_index, file->data) == 0) { return 0; }
    return persistent_store_metadata();
}

int vfs_persistent_remove(const char *path) {
    const int persistent_index = persistent_find(path);
    struct persistent_file *file;
    uint8_t blank[AHCI_SECTOR_SIZE] = { 0U };

    if (persistent_mounted == 0U || persistent_index < 0) { return 0; }
    file = &persistent_files[persistent_index];
    if (ahci_write_data_sector(PERSIST_FILE_LBA_BASE + (uint64_t)persistent_index, blank) == 0) { return 0; }
    persistent_clear_file(file);
    return persistent_store_metadata();
}

static int next_entry(uint64_t *offset, const uint8_t **name, uint64_t *name_size,
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

int vfs_mount_newc(const void *archive, uint64_t length) {
    uint64_t offset = 0U;
    uint64_t count = 0U;
    const uint8_t *name;
    const uint8_t *data;
    uint64_t name_size;
    uint64_t data_size;

    tmpfs_reset();
    mounted_archive = (const uint8_t *)0;
    mounted_length = 0U;
    mounted_files = 0U;
    if (archive == (const void *)0 || length < NEWC_HEADER_SIZE) {
        return 0;
    }
    mounted_archive = (const uint8_t *)archive;
    mounted_length = length;
    while (next_entry(&offset, &name, &name_size, &data, &data_size) != 0) {
        if (name_equal(name, name_size, "TRAILER!!!") != 0) {
            mounted_files = count;
            return 1;
        }
        count++;
    }
    mounted_archive = (const uint8_t *)0;
    mounted_length = 0U;
    mounted_files = 0U;
    return 0;
}

int vfs_open(const char *path, struct vfs_file *file) {
    uint64_t offset = 0U;
    const uint8_t *name;
    const uint8_t *data;
    uint64_t name_size;
    uint64_t data_size;
    int tmpfs_index;
    int persistent_index;

    if (path == (const char *)0 || file == (struct vfs_file *)0) {
        return 0;
    }
    tmpfs_index = tmpfs_find(path);
    if (tmpfs_index >= 0) {
        file->data = tmpfs_files[tmpfs_index].data;
        file->size = tmpfs_files[tmpfs_index].size;
        return 1;
    }
    persistent_index = persistent_find(path);
    if (persistent_index >= 0) {
        file->data = persistent_files[persistent_index].data;
        file->size = persistent_files[persistent_index].size;
        return 1;
    }
    if (mounted_archive == (const uint8_t *)0) {
        return 0;
    }
    while (next_entry(&offset, &name, &name_size, &data, &data_size) != 0) {
        if (name_equal(name, name_size, "TRAILER!!!") != 0) {
            return 0;
        }
        if (name_equal(name, name_size, path) != 0) {
            file->data = data;
            file->size = data_size;
            return 1;
        }
    }
    return 0;
}

int vfs_get_entry(uint64_t index, char *name, uint64_t name_capacity, uint64_t *size) {
    uint64_t offset = 0U;
    uint64_t current = 0U;
    const uint8_t *entry_name;
    const uint8_t *data;
    uint64_t entry_name_size;
    uint64_t data_size;

    if (mounted_archive == (const uint8_t *)0 || name == (char *)0 || name_capacity == 0U
        || size == (uint64_t *)0) {
        return 0;
    }
    if (index < mounted_files) {
        while (next_entry(&offset, &entry_name, &entry_name_size, &data, &data_size) != 0) {
            if (name_equal(entry_name, entry_name_size, "TRAILER!!!") != 0) {
                return 0;
            }
            if (current == index) {
                if (entry_name_size > name_capacity) {
                    return 0;
                }
                for (uint64_t character = 0U; character < entry_name_size; character++) {
                    name[character] = (char)entry_name[character];
                }
                *size = data_size;
                return 1;
            }
            current++;
        }
        return 0;
    }
    index -= mounted_files;
    for (uint64_t tmpfs_index = 0U; tmpfs_index < VFS_TMPFS_MAX_FILES; tmpfs_index++) {
        const struct tmpfs_file *file = &tmpfs_files[tmpfs_index];

        if (file->used == 0U) {
            continue;
        }
        if (index == 0U) {
            uint64_t character = 0U;

            while (character < VFS_NAME_MAX && file->name[character] != '\0') {
                if (character + 1U >= name_capacity) {
                    return 0;
                }
                name[character] = file->name[character];
                character++;
            }
            if (character >= VFS_NAME_MAX || character >= name_capacity) {
                return 0;
            }
            name[character] = '\0';
            *size = file->size;
            return 1;
        }
        index--;
    }
    for (uint64_t persistent_index = 0U; persistent_index < VFS_PERSIST_MAX_FILES; persistent_index++) {
        const struct persistent_file *file = &persistent_files[persistent_index];

        if (file->used == 0U) { continue; }
        if (index == 0U) {
            uint64_t character = 0U;

            while (character < VFS_PERSIST_NAME_CAPACITY && file->name[character] != '\0') {
                if (character + 1U >= name_capacity) { return 0; }
                name[character] = file->name[character];
                character++;
            }
            if (character >= VFS_PERSIST_NAME_CAPACITY || character >= name_capacity) { return 0; }
            name[character] = '\0';
            *size = file->size;
            return 1;
        }
        index--;
    }
    return 0;
}

int vfs_tmpfs_create(const char *path) {
    int slot = -1;

    if (tmpfs_path_is_valid(path) == 0 || tmpfs_find(path) >= 0) {
        return 0;
    }
    for (uint64_t index = 0U; index < VFS_TMPFS_MAX_FILES; index++) {
        if (tmpfs_files[index].used == 0U) {
            slot = (int)index;
            break;
        }
    }
    if (slot < 0) {
        return 0;
    }
    tmpfs_clear_file(&tmpfs_files[slot]);
    for (uint64_t index = 0U; index < VFS_NAME_MAX; index++) {
        tmpfs_files[slot].name[index] = path[index];
        if (path[index] == '\0') {
            break;
        }
    }
    tmpfs_files[slot].used = 1U;
    return 1;
}

int vfs_tmpfs_write(const char *path, uint64_t offset, const uint8_t *data, uint64_t length) {
    const int tmpfs_index = tmpfs_find(path);
    struct tmpfs_file *file;

    if (tmpfs_index < 0 || offset > VFS_TMPFS_FILE_CAPACITY
        || length > VFS_TMPFS_FILE_CAPACITY - offset
        || (length != 0U && data == (const uint8_t *)0)) {
        return 0;
    }
    file = &tmpfs_files[tmpfs_index];
    for (uint64_t index = file->size; index < offset; index++) {
        file->data[index] = 0U;
    }
    for (uint64_t index = 0U; index < length; index++) {
        file->data[offset + index] = data[index];
    }
    if (offset + length > file->size) {
        file->size = offset + length;
    }
    return 1;
}

int vfs_tmpfs_remove(const char *path) {
    const int tmpfs_index = tmpfs_find(path);

    if (tmpfs_index < 0) {
        return 0;
    }
    tmpfs_clear_file(&tmpfs_files[tmpfs_index]);
    return 1;
}

uint64_t vfs_file_count(void) {
    uint64_t count = mounted_files;

    for (uint64_t index = 0U; index < VFS_TMPFS_MAX_FILES; index++) {
        if (tmpfs_files[index].used != 0U) {
            count++;
        }
    }
    for (uint64_t index = 0U; index < VFS_PERSIST_MAX_FILES; index++) {
        if (persistent_files[index].used != 0U) {
            count++;
        }
    }
    return count;
}

uint64_t vfs_total_size(void) {
    uint64_t total = mounted_length;

    for (uint64_t index = 0U; index < VFS_TMPFS_MAX_FILES; index++) {
        if (tmpfs_files[index].used != 0U) {
            total += tmpfs_files[index].size;
        }
    }
    for (uint64_t index = 0U; index < VFS_PERSIST_MAX_FILES; index++) {
        if (persistent_files[index].used != 0U) {
            total += persistent_files[index].size;
        }
    }
    return total;
}
