#include <stdint.h>

#include <myos.h>

#define MYOS_CP_FILE_MAX (UINT64_C(8) * UINT64_C(1024) * UINT64_C(1024))

static void write_number(uint64_t value) {
    char digits[21];
    uint64_t length = 0U;

    if (value == 0U) {
        myos_write_text("0");
        return;
    }
    while (value != 0U && length < sizeof(digits)) {
        digits[length++] = (char)('0' + value % 10U);
        value /= 10U;
    }
    while (length != 0U) {
        length--;
        (void)myos_write(MYOS_STDOUT, &digits[length], 1U);
    }
}

static int copy_token(const char *arguments, uint64_t *offset, char *destination, uint64_t capacity) {
    uint64_t length = 0U;

    while (arguments[*offset] == ' ') {
        (*offset)++;
    }
    while (arguments[*offset] != '\0' && arguments[*offset] != ' ' && length + 1U < capacity) {
        destination[length++] = arguments[*offset];
        (*offset)++;
    }
    destination[length] = '\0';
    return length != 0U && (arguments[*offset] == '\0' || arguments[*offset] == ' ');
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

static void copy_path(char *destination, const char *source) {
    uint64_t index = 0U;

    while (index + 1U < MYOS_VFS_PATH_MAX && source[index] != '\0') {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

int myos_main(uint64_t argc, const char *arguments) {
    struct myos_vfs_read_request read_request = { 0U, { 0 }, { 0 } };
    struct myos_vfs_path_request target_request = { { 0 } };
    struct myos_vfs_write_request write_request = { 0U, 0U, { 0 }, { 0 } };
    uint64_t argument_offset = 0U;
    uint64_t copied = 0U;
    int failed = 0;

    if (argc != 1U
        || copy_token(arguments, &argument_offset, read_request.path, sizeof(read_request.path)) == 0
        || copy_token(arguments, &argument_offset, target_request.path, sizeof(target_request.path)) == 0
        || arguments[argument_offset] != '\0'
        || read_request.path[0] != '/'
        || target_request.path[0] != '/'
        || text_equal(read_request.path, target_request.path) != 0) {
        myos_write_text("Usage: run cp <absolute-source> <new-absolute-target>\n");
        return 2;
    }
    copy_path(write_request.path, target_request.path);
    if (myos_vfs_create_file(&target_request) == MYOS_SYSCALL_ERROR) {
        myos_write_text("cp: target must not exist and parent directory must exist\n");
        return 1;
    }
    for (;;) {
        uint64_t read_count;

        read_request.offset = copied;
        read_count = myos_vfs_read(&read_request);
        if (read_count == MYOS_SYSCALL_ERROR || read_count > MYOS_VFS_READ_CHUNK
            || read_count > MYOS_CP_FILE_MAX - copied) {
            failed = 1;
            break;
        }
        if (read_count == 0U) {
            break;
        }
        write_request.offset = copied;
        write_request.length = read_count;
        for (uint64_t index = 0U; index < read_count; index++) {
            write_request.data[index] = read_request.data[index];
        }
        if (myos_vfs_write(&write_request) != read_count) {
            failed = 1;
            break;
        }
        copied += read_count;
    }
    if (failed != 0) {
        (void)myos_vfs_remove(&target_request);
        myos_write_text("cp: copy failed; partial target removed\n");
        return 1;
    }
    myos_write_text("Copied ");
    write_number(copied);
    myos_write_text(" byte(s): ");
    myos_write_text(read_request.path);
    myos_write_text(" -> ");
    myos_write_text(target_request.path);
    myos_write_text("\n");
    return 0;
}
