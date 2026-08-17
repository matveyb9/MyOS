#include <stdint.h>

#include <syscall.h>

#define GREP_LINE_CAPACITY 128U

static uint64_t system_call(uint64_t number, uint64_t descriptor, uint64_t buffer, uint64_t length) {
    uint64_t result;

    __asm__ volatile ("syscall"
                      : "=a"(result)
                      : "a"(number), "D"(descriptor), "S"(buffer), "d"(length)
                      : "rcx", "r11", "memory");
    return result;
}

static void write_text(const char *text) {
    uint64_t length = 0U;

    while (text[length] != '\0') {
        length++;
    }
    while (length != 0U) {
        const uint64_t chunk = length > UINT64_C(256) ? UINT64_C(256) : length;

        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)text, chunk);
        text += chunk;
        length -= chunk;
    }
}

static int line_contains(const char *line, uint64_t line_length, const char *needle, uint64_t needle_length) {
    if (needle_length > line_length) {
        return 0;
    }
    for (uint64_t start = 0U; start + needle_length <= line_length; start++) {
        uint64_t index = 0U;

        while (index < needle_length && line[start + index] == needle[index]) {
            index++;
        }
        if (index == needle_length) {
            return 1;
        }
    }
    return 0;
}

void _start(uint64_t argc, const char *arguments) {
    struct myos_vfs_read_request request = { 0U, { 0 }, { 0 } };
    char needle[MYOS_SPAWN_ARGUMENTS_MAX];
    char line[GREP_LINE_CAPACITY];
    uint64_t needle_length = 0U;
    uint64_t file_length = 0U;
    uint64_t line_length = 0U;
    int line_overflow = 0;

    while (arguments[needle_length] != '\0' && arguments[needle_length] != ' '
           && needle_length + 1U < sizeof(needle)) {
        needle[needle_length] = arguments[needle_length];
        needle_length++;
    }
    needle[needle_length] = '\0';
    if (argc != 1U || needle_length == 0U || arguments[needle_length] != ' ') {
        write_text("Usage: run grep <text> <file>\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    file_length = needle_length + 1U;
    while (arguments[file_length] != '\0' && arguments[file_length] != ' '
           && file_length - needle_length < MYOS_VFS_NAME_MAX) {
        request.path[file_length - needle_length - 1U] = arguments[file_length];
        file_length++;
    }
    if (arguments[file_length] != '\0' || file_length == needle_length + 1U) {
        write_text("Usage: run grep <text> <file>\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    request.path[file_length - needle_length - 1U] = '\0';
    for (;;) {
        const uint64_t count = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&request,
                                           sizeof(request));

        if (count == UINT64_MAX) {
            write_text("grep: unable to read file\n");
            (void)system_call(MYOS_SYS_EXIT, 1U, 0U, 0U);
        }
        if (count == 0U) {
            break;
        }
        for (uint64_t index = 0U; index < count; index++) {
            const char character = (char)request.data[index];

            if (character == '\n') {
                if (line_overflow == 0 && line_contains(line, line_length, needle, needle_length) != 0) {
                    (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)line, line_length);
                    write_text("\n");
                }
                line_length = 0U;
                line_overflow = 0;
            } else if (line_length + 1U < GREP_LINE_CAPACITY) {
                line[line_length] = character;
                line_length++;
            } else {
                line_overflow = 1;
            }
        }
        request.offset += count;
    }
    if (line_length != 0U && line_overflow == 0
        && line_contains(line, line_length, needle, needle_length) != 0) {
        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)line, line_length);
        write_text("\n");
    }
    (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
