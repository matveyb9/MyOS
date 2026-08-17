#include <stdint.h>

#include <vfs.h>

#define NEWC_HEADER_SIZE 110U

static const uint8_t *mounted_archive;
static uint64_t mounted_length;
static uint64_t mounted_files;

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

    if (mounted_archive == (const uint8_t *)0 || path == (const char *)0 || file == (struct vfs_file *)0) {
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

uint64_t vfs_file_count(void) {
    return mounted_files;
}

uint64_t vfs_total_size(void) {
    return mounted_length;
}
