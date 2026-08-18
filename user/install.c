#include <stdint.h>

#include <syscall.h>

#define MYOS_PERSIST_FILE_MAX UINT64_C(32768)

static uint64_t system_call(uint64_t number, uint64_t a, uint64_t b, uint64_t c) {
    uint64_t result;
    __asm__ volatile ("syscall" : "=a"(result) : "a"(number), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory");
    return result;
}

static void write_text(const char *text) {
    uint64_t length = 0U;

    while (text[length] != '\0') {
        length++;
    }
    while (length != 0U) {
        const uint64_t chunk = length > 128U ? 128U : length;

        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)text, chunk);
        text += chunk;
        length -= chunk;
    }
}

static void write_number(uint64_t value) {
    char text[21];
    uint64_t length = 0U;

    if (value == 0U) {
        write_text("0");
        return;
    }
    while (value != 0U && length < sizeof(text)) {
        text[length++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (length != 0U) {
        length--;
        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)&text[length], 1U);
    }
}

static int copy_token(const char *arguments, uint64_t *offset, char *destination) {
    uint64_t length = 0U;

    while (arguments[*offset] == ' ') {
        (*offset)++;
    }
    while (arguments[*offset] != '\0' && arguments[*offset] != ' ' && length + 1U < MYOS_VFS_NAME_MAX) {
        destination[length++] = arguments[*offset];
        (*offset)++;
    }
    destination[length] = '\0';
    return length != 0U;
}

static int disk_program_path(const char *path) {
    return path[0] == 'd' && path[1] == 'i' && path[2] == 's' && path[3] == 'k'
           && path[4] == '/' && path[5] == 'b' && path[6] == 'i' && path[7] == 'n'
           && path[8] == '/' && path[9] != '\0';
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

void _start(uint64_t argc, const char *arguments) {
    struct myos_vfs_read_request read_request = { 0U, { 0 }, { 0 } };
    struct myos_tmpfs_path_request path_request = { { 0 } };
    struct myos_tmpfs_write_request write_request = { 0U, 0U, { 0 }, { 0 } };
    uint64_t argument_offset = 0U;
    uint64_t copied = 0U;
    int failed = 0;

    if (argc != 1U || copy_token(arguments, &argument_offset, read_request.path) == 0
        || copy_token(arguments, &argument_offset, path_request.path) == 0
        || arguments[argument_offset] != '\0' || disk_program_path(path_request.path) == 0) {
        write_text("Usage: install <source> <disk/bin/name>\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    if (text_equal(read_request.path, path_request.path) != 0) {
        write_text("install: source and target must differ\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    for (uint64_t index = 0U; index < MYOS_VFS_NAME_MAX; index++) {
        write_request.path[index] = path_request.path[index];
        if (path_request.path[index] == '\0') {
            break;
        }
    }
    (void)system_call(MYOS_SYS_PERSIST_REMOVE, 0U, (uint64_t)(uintptr_t)&path_request, sizeof(path_request));
    if (system_call(MYOS_SYS_PERSIST_CREATE, 0U, (uint64_t)(uintptr_t)&path_request, sizeof(path_request)) == UINT64_MAX) {
        write_text("install: cannot create target\n");
        (void)system_call(MYOS_SYS_EXIT, 1U, 0U, 0U);
    }
    for (;;) {
        uint64_t read_count;

        read_request.offset = copied;
        read_count = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&read_request, sizeof(read_request));
        if (read_count == UINT64_MAX || read_count > MYOS_VFS_READ_CHUNK
            || read_count > MYOS_PERSIST_FILE_MAX - copied) {
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
        if (system_call(MYOS_SYS_PERSIST_WRITE, 0U, (uint64_t)(uintptr_t)&write_request, sizeof(write_request)) == UINT64_MAX) {
            failed = 1;
            break;
        }
        copied += read_count;
    }
    if (failed != 0 || copied == 0U) {
        (void)system_call(MYOS_SYS_PERSIST_REMOVE, 0U, (uint64_t)(uintptr_t)&path_request, sizeof(path_request));
        write_text("install: copy failed\n");
        (void)system_call(MYOS_SYS_EXIT, 1U, 0U, 0U);
    }
    write_text("Installed ");
    write_text(read_request.path);
    write_text(" as ");
    write_text(path_request.path);
    write_text(" (");
    write_number(copied);
    write_text(" bytes).\n");
    (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
