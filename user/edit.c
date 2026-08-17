#include <stdint.h>

#include <syscall.h>

#define EDIT_LINE_CAPACITY 128U

static uint64_t system_call(uint64_t number, uint64_t a, uint64_t b, uint64_t c) {
    uint64_t result;
    __asm__ volatile ("syscall" : "=a"(result) : "a"(number), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory");
    return result;
}

static void write_bytes(const char *text, uint64_t length) {
    while (length != 0U) {
        const uint64_t chunk = length > 128U ? 128U : length;
        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)text, chunk);
        text += chunk;
        length -= chunk;
    }
}

static void write_text(const char *text) {
    uint64_t length = 0U;
    while (text[length] != '\0') { length++; }
    write_bytes(text, length);
}

static uint64_t read_line(char *line) {
    uint64_t length = 0U;
    while (length + 1U < EDIT_LINE_CAPACITY) {
        char character;
        const uint64_t result = system_call(MYOS_SYS_READ, 0U, (uint64_t)(uintptr_t)&character, 1U);
        if (result == UINT64_MAX || result == 0U) { continue; }
        if (character == '\r' || character == '\n') { write_text("\n"); break; }
        if ((character == '\b' || character == 0x7FU) && length != 0U) { length--; write_text("\b \b"); continue; }
        if (character >= ' ' && character <= '~') { line[length++] = character; write_bytes(&character, 1U); }
    }
    line[length] = '\0';
    return length;
}

void _start(uint64_t argc, const char *arguments) {
    struct myos_tmpfs_path_request path = { { 0 } };
    struct myos_tmpfs_write_request write_request = { 0U, 0U, { 0 }, { 0 } };
    struct myos_vfs_read_request read_request = { 0U, { 0 }, { 0 } };
    char line[EDIT_LINE_CAPACITY];
    uint64_t length = 0U;
    uint64_t create_number;
    uint64_t remove_number;
    uint64_t write_number;

    while (arguments[length] != '\0' && arguments[length] != ' ' && length + 1U < MYOS_VFS_NAME_MAX) {
        path.path[length] = arguments[length];
        read_request.path[length] = arguments[length];
        write_request.path[length] = arguments[length];
        length++;
    }
    if (argc != 1U || length == 0U || arguments[length] != '\0') {
        write_text("Usage: run edit <tmp/file | disk/file>\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    path.path[length] = '\0'; read_request.path[length] = '\0'; write_request.path[length] = '\0';
    create_number = path.path[0] == 'd' && path.path[1] == 'i' && path.path[2] == 's' && path.path[3] == 'k' && path.path[4] == '/'
        ? MYOS_SYS_PERSIST_CREATE : MYOS_SYS_TMPFS_CREATE;
    remove_number = create_number == MYOS_SYS_PERSIST_CREATE ? MYOS_SYS_PERSIST_REMOVE : MYOS_SYS_TMPFS_REMOVE;
    write_number = create_number == MYOS_SYS_PERSIST_CREATE ? MYOS_SYS_PERSIST_WRITE : MYOS_SYS_TMPFS_WRITE;
    (void)system_call(create_number, 0U, (uint64_t)(uintptr_t)&path, sizeof(path));
    write_text("Current: ");
    const uint64_t existing = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&read_request, sizeof(read_request));
    if (existing != UINT64_MAX && existing != 0U) { write_bytes((const char *)read_request.data, existing); }
    write_text("\nNew line: ");
    write_request.length = read_line(line);
    for (uint64_t index = 0U; index < write_request.length; index++) { write_request.data[index] = (uint8_t)line[index]; }
    (void)system_call(remove_number, 0U, (uint64_t)(uintptr_t)&path, sizeof(path));
    if (system_call(create_number, 0U, (uint64_t)(uintptr_t)&path, sizeof(path)) == UINT64_MAX
        || system_call(write_number, 0U, (uint64_t)(uintptr_t)&write_request, sizeof(write_request)) == UINT64_MAX) {
        write_text("edit: write failed\n");
        (void)system_call(MYOS_SYS_EXIT, 1U, 0U, 0U);
    }
    (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    for (;;) { __asm__ volatile ("pause"); }
}
