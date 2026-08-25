#include <stdint.h>

#include <syscall.h>

#define TAIL_DEFAULT_LINES UINT64_C(10)
#define TAIL_LINE_LIMIT_MAX UINT64_C(64)
#define TAIL_OUTPUT_MAX UINT64_C(4096)

static uint8_t tail_buffer[TAIL_OUTPUT_MAX];
static uint64_t tail_start;
static uint64_t tail_length;
static int tail_truncated;

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

        if (value > (TAIL_LINE_LIMIT_MAX - digit) / UINT64_C(10)) {
            return 0;
        }
        value = value * UINT64_C(10) + digit;
        digits++;
        (*offset)++;
    }
    if (digits == 0U || value == 0U || value > TAIL_LINE_LIMIT_MAX || arguments[*offset] != '\0') {
        return 0;
    }
    *limit = value;
    return 1;
}

static void append_byte(uint8_t value) {
    if (tail_length < TAIL_OUTPUT_MAX) {
        tail_buffer[(tail_start + tail_length) % TAIL_OUTPUT_MAX] = value;
        tail_length++;
        return;
    }
    tail_buffer[tail_start] = value;
    tail_start = (tail_start + 1U) % TAIL_OUTPUT_MAX;
    tail_truncated = 1;
}

static uint8_t retained_byte(uint64_t index) {
    return tail_buffer[(tail_start + index) % TAIL_OUTPUT_MAX];
}

static uint64_t output_offset_for_lines(uint64_t line_limit) {
    uint64_t newlines = 0U;
    uint64_t skip = 0U;
    uint64_t seen = 0U;
    const int ends_with_newline = tail_length != 0U && retained_byte(tail_length - 1U) == '\n';

    for (uint64_t index = 0U; index < tail_length; index++) {
        if (retained_byte(index) == '\n') {
            newlines++;
        }
    }
    if (ends_with_newline != 0) {
        if (newlines > line_limit) {
            skip = newlines - line_limit;
        }
    } else if (newlines >= line_limit) {
        skip = newlines - line_limit + 1U;
    }
    if (skip == 0U) {
        return 0U;
    }
    for (uint64_t index = 0U; index < tail_length; index++) {
        if (retained_byte(index) == '\n') {
            seen++;
            if (seen == skip) {
                return index + 1U;
            }
        }
    }
    return 0U;
}

static void write_retained(uint64_t offset) {
    uint64_t remaining;
    uint64_t first;
    uint64_t start;

    if (offset >= tail_length) {
        return;
    }
    remaining = tail_length - offset;
    start = (tail_start + offset) % TAIL_OUTPUT_MAX;
    first = TAIL_OUTPUT_MAX - start;
    if (first > remaining) {
        first = remaining;
    }
    write_bytes(tail_buffer + start, first);
    remaining -= first;
    if (remaining != 0U) {
        write_bytes(tail_buffer, remaining);
    }
}

static void usage(void) {
    write_text("Usage: run tail <absolute-file> [1..64 lines]\n");
}

void _start(uint64_t argc, const char *arguments) {
    struct myos_vfs_read_request request = { 0U, { 0 }, { 0 } };
    uint64_t offset = 0U;
    uint64_t line_limit = TAIL_DEFAULT_LINES;

    (void)argc;
    if (copy_path(arguments, &offset, request.path) == 0
        || (arguments[offset] != '\0' && parse_line_limit(arguments, &offset, &line_limit) == 0)) {
        usage();
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    for (;;) {
        const uint64_t count = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));

        if (count == UINT64_MAX || request.offset > UINT64_MAX - count) {
            write_text("tail: unable to read file\n");
            (void)system_call(MYOS_SYS_EXIT, 1U, 0U, 0U);
        }
        if (count == 0U) {
            break;
        }
        for (uint64_t index = 0U; index < count; index++) {
            append_byte(request.data[index]);
        }
        request.offset += count;
    }
    if (tail_truncated != 0) {
        write_text("tail: retained last 4096 bytes\n");
    }
    write_retained(output_offset_for_lines(line_limit));
    (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
