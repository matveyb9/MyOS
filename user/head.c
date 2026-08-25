#include <stdint.h>

#include <syscall.h>

#define HEAD_DEFAULT_LINES UINT64_C(10)
#define HEAD_LINE_LIMIT_MAX UINT64_C(64)
#define HEAD_OUTPUT_MAX UINT64_C(4096)

static uint64_t system_call(uint64_t number, uint64_t descriptor, uint64_t buffer, uint64_t length) {
    uint64_t result;

    __asm__ volatile ("syscall"
                      : "=a"(result)
                      : "a"(number), "D"(descriptor), "S"(buffer), "d"(length)
                      : "rcx", "r11", "memory");
    return result;
}

static void write_bytes(const uint8_t *data, uint64_t length) {
    while (length != 0U) {
        const uint64_t chunk = length > MYOS_VFS_READ_CHUNK ? MYOS_VFS_READ_CHUNK : length;

        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)data, chunk);
        data += chunk;
        length -= chunk;
    }
}

static void write_text(const char *text) {
    uint64_t length = 0U;

    while (text[length] != '\0') {
        length++;
    }
    write_bytes((const uint8_t *)text, length);
}

static int copy_path(const char *arguments, uint64_t *offset, char *path) {
    uint64_t length = 0U;

    if (arguments == (const char *)0 || offset == (uint64_t *)0 || path == (char *)0
        || arguments[*offset] != '/') {
        return 0;
    }
    while (arguments[*offset] != '\0' && arguments[*offset] != ' ' && length + 1U < MYOS_VFS_PATH_MAX) {
        const char character = arguments[*offset];

        if (character < ' ' || character > '~') {
            return 0;
        }
        path[length++] = character;
        (*offset)++;
    }
    if (arguments[*offset] != '\0' && arguments[*offset] != ' ') {
        return 0;
    }
    if (length == 0U || (length > 1U && path[length - 1U] == '/')) {
        return 0;
    }
    path[length] = '\0';
    return 1;
}

static int parse_line_limit(const char *arguments, uint64_t *offset, uint64_t *limit) {
    uint64_t value = 0U;
    uint64_t digits = 0U;

    if (arguments == (const char *)0 || offset == (uint64_t *)0 || limit == (uint64_t *)0
        || arguments[*offset] != ' ') {
        return 0;
    }
    (*offset)++;
    while (arguments[*offset] >= '0' && arguments[*offset] <= '9') {
        const uint64_t digit = (uint64_t)(arguments[*offset] - '0');

        if (value > (HEAD_LINE_LIMIT_MAX - digit) / UINT64_C(10)) {
            return 0;
        }
        value = value * UINT64_C(10) + digit;
        digits++;
        (*offset)++;
    }
    if (digits == 0U || value == 0U || value > HEAD_LINE_LIMIT_MAX || arguments[*offset] != '\0') {
        return 0;
    }
    *limit = value;
    return 1;
}

static void usage(void) {
    write_text("Usage: run head <absolute-file> [1..64 lines]\n");
}

void _start(uint64_t argc, const char *arguments) {
    struct myos_vfs_read_request request = { 0U, { 0 }, { 0 } };
    uint64_t offset = 0U;
    uint64_t line_limit = HEAD_DEFAULT_LINES;
    uint64_t lines = 0U;
    uint64_t emitted = 0U;
    int output_limited = 0;

    (void)argc;
    if (copy_path(arguments, &offset, request.path) == 0
        || (arguments[offset] != '\0' && parse_line_limit(arguments, &offset, &line_limit) == 0)) {
        usage();
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    while (lines < line_limit && emitted < HEAD_OUTPUT_MAX) {
        const uint64_t count = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));
        uint64_t candidate;
        uint64_t emit = 0U;

        if (count == UINT64_MAX) {
            write_text("head: unable to read file\n");
            (void)system_call(MYOS_SYS_EXIT, 1U, 0U, 0U);
        }
        if (count == 0U) {
            break;
        }
        candidate = count;
        if (candidate > HEAD_OUTPUT_MAX - emitted) {
            candidate = HEAD_OUTPUT_MAX - emitted;
        }
        while (emit < candidate) {
            emit++;
            if (request.data[emit - 1U] == '\n') {
                lines++;
                if (lines == line_limit) {
                    break;
                }
            }
        }
        write_bytes(request.data, emit);
        emitted += emit;
        if (emitted == HEAD_OUTPUT_MAX && lines < line_limit) {
            output_limited = 1;
            break;
        }
        if (lines == line_limit) {
            break;
        }
        request.offset += count;
    }
    if (output_limited != 0) {
        write_text("\nhead: output limit reached\n");
    }
    (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
