#include <stdint.h>

#include <syscall.h>

#define MYOS_INSTALL_FILE_MAX (UINT64_C(8) * UINT64_C(1024) * UINT64_C(1024))

static uint64_t system_call(uint64_t number, uint64_t a, uint64_t b, uint64_t c) {
    uint64_t result;
    __asm__ volatile ("syscall" : "=a"(result) : "a"(number), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory");
    return result;
}

static void write_text(const char *text) {
    uint64_t length = 0U;

    while (text[length] != '\0') { length++; }
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

    if (value == 0U) { write_text("0"); return; }
    while (value != 0U && length < sizeof(text)) {
        text[length++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (length != 0U) {
        length--;
        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)&text[length], 1U);
    }
}

static int copy_token(const char *arguments, uint64_t *offset, char *destination, uint64_t capacity) {
    uint64_t length = 0U;

    while (arguments[*offset] == ' ') { (*offset)++; }
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
        if (left[index] != right[index]) { return 0; }
        index++;
    }
    return left[index] == right[index];
}

static int application_target_path(const char *path, char *package_path) {
    static const char suffix[] = "/main.elf";
    uint64_t length = 0U;
    uint64_t suffix_length = sizeof(suffix) - 1U;

    while (path[length] != '\0' && length + 1U < MYOS_VFS_PATH_MAX) { length++; }
    if (path[0] != '/' || path[1] != 'a' || path[2] != 'p' || path[3] != 'p' || path[4] != 's'
        || path[5] != '/' || length <= 6U + suffix_length || length < suffix_length) {
        return 0;
    }
    for (uint64_t index = 0U; index < suffix_length; index++) {
        if (path[length - suffix_length + index] != suffix[index]) { return 0; }
    }
    for (uint64_t index = 6U; index < length - suffix_length; index++) {
        if (path[index] == '/') { return 0; }
    }
    for (uint64_t index = 0U; index < length - suffix_length; index++) { package_path[index] = path[index]; }
    package_path[length - suffix_length] = '\0';
    return 1;
}

void _start(uint64_t argc, const char *arguments) {
    struct myos_vfs_read_request read_request = { 0U, { 0 }, { 0 } };
    struct myos_vfs_path_request target_request = { { 0 } };
    struct myos_vfs_path_request package_request = { { 0 } };
    struct myos_vfs_write_request write_request = { 0U, 0U, { 0 }, { 0 } };
    uint64_t argument_offset = 0U;
    uint64_t copied = 0U;
    int failed = 0;

    if (argc != 1U || copy_token(arguments, &argument_offset, read_request.path, sizeof(read_request.path)) == 0
        || copy_token(arguments, &argument_offset, target_request.path, sizeof(target_request.path)) == 0
        || arguments[argument_offset] != '\0'
        || read_request.path[0] != '/'
        || application_target_path(target_request.path, package_request.path) == 0) {
        write_text("Usage: install <absolute-source> </apps/name/main.elf>\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    if (text_equal(read_request.path, target_request.path) != 0) {
        write_text("install: source and target must differ\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    for (uint64_t index = 0U; index < MYOS_VFS_PATH_MAX; index++) { write_request.path[index] = target_request.path[index]; }
    (void)system_call(MYOS_SYS_VFS_CREATE_DIRECTORY, 0U, (uint64_t)(uintptr_t)&package_request, sizeof(package_request));
    (void)system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&target_request, sizeof(target_request));
    if (system_call(MYOS_SYS_VFS_CREATE_FILE, 0U, (uint64_t)(uintptr_t)&target_request, sizeof(target_request)) == UINT64_MAX) {
        write_text("install: cannot create target\n");
        (void)system_call(MYOS_SYS_EXIT, 1U, 0U, 0U);
    }
    for (;;) {
        uint64_t read_count;

        read_request.offset = copied;
        read_count = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&read_request, sizeof(read_request));
        if (read_count == UINT64_MAX || read_count > MYOS_VFS_READ_CHUNK || read_count > MYOS_INSTALL_FILE_MAX - copied) {
            failed = 1;
            break;
        }
        if (read_count == 0U) { break; }
        write_request.offset = copied;
        write_request.length = read_count;
        for (uint64_t index = 0U; index < read_count; index++) { write_request.data[index] = read_request.data[index]; }
        if (system_call(MYOS_SYS_VFS_WRITE, 0U, (uint64_t)(uintptr_t)&write_request, sizeof(write_request)) == UINT64_MAX) {
            failed = 1;
            break;
        }
        copied += read_count;
    }
    if (failed != 0 || copied == 0U) {
        (void)system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&target_request, sizeof(target_request));
        write_text("install: copy failed\n");
        (void)system_call(MYOS_SYS_EXIT, 1U, 0U, 0U);
    }
    write_text("Installed "); write_text(read_request.path); write_text(" as "); write_text(target_request.path);
    write_text(" ("); write_number(copied); write_text(" bytes).\n");
    (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    for (;;) { __asm__ volatile ("pause"); }
}
