#include <stdint.h>

#include <syscall.h>

#define SORT_LINE_MAX UINT64_C(64)
#define SORT_LINE_CAPACITY UINT64_C(128)

static char sort_lines[SORT_LINE_MAX][SORT_LINE_CAPACITY];
static char *sort_order[SORT_LINE_MAX];
static uint64_t sort_line_count;
static int sort_truncated;

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
        const uint64_t chunk = length > MYOS_VFS_READ_CHUNK ? MYOS_VFS_READ_CHUNK : length;

        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)text, chunk);
        text += chunk;
        length -= chunk;
    }
}

static int copy_path(const char *arguments, char *path) {
    uint64_t index = 0U;

    if (arguments == (const char *)0 || path == (char *)0 || arguments[0] != '/') {
        return 0;
    }
    while (arguments[index] != '\0' && index + 1U < MYOS_VFS_PATH_MAX) {
        const char character = arguments[index];

        if (character < ' ' || character > '~') {
            return 0;
        }
        path[index] = character;
        index++;
    }
    if (arguments[index] != '\0' || index == 0U || (index > 1U && path[index - 1U] == '/')) {
        return 0;
    }
    path[index] = '\0';
    return 1;
}

static void append_line_character(char value, uint64_t *length) {
    if (sort_line_count >= SORT_LINE_MAX) {
        sort_truncated = 1;
        return;
    }
    if (*length + 1U < SORT_LINE_CAPACITY) {
        sort_lines[sort_line_count][(*length)++] = value;
        return;
    }
    sort_truncated = 1;
}

static void finish_line(uint64_t *length) {
    if (sort_line_count >= SORT_LINE_MAX) {
        sort_truncated = 1;
        return;
    }
    sort_lines[sort_line_count][*length] = '\0';
    sort_order[sort_line_count] = sort_lines[sort_line_count];
    sort_line_count++;
    *length = 0U;
}

static int text_compare(const char *left, const char *right) {
    uint64_t index = 0U;

    while (left[index] != '\0' && right[index] != '\0') {
        const uint8_t left_value = (uint8_t)left[index];
        const uint8_t right_value = (uint8_t)right[index];

        if (left_value < right_value) {
            return -1;
        }
        if (left_value > right_value) {
            return 1;
        }
        index++;
    }
    if (left[index] == right[index]) {
        return 0;
    }
    return left[index] == '\0' ? -1 : 1;
}

static void sort_lines_ascending(void) {
    for (uint64_t index = 1U; index < sort_line_count; index++) {
        char *value = sort_order[index];
        uint64_t position = index;

        while (position != 0U && text_compare(value, sort_order[position - 1U]) < 0) {
            sort_order[position] = sort_order[position - 1U];
            position--;
        }
        sort_order[position] = value;
    }
}

void _start(uint64_t argc, const char *arguments) {
    struct myos_vfs_read_request request = { 0U, { 0 }, { 0 } };
    uint64_t line_length = 0U;
    int have_data = 0;

    if (argc != 1U || copy_path(arguments, request.path) == 0) {
        write_text("Usage: run sort <absolute-file>\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    for (;;) {
        const uint64_t count = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));

        if (count == UINT64_MAX || request.offset > UINT64_MAX - count) {
            write_text("sort: unable to read file\n");
            (void)system_call(MYOS_SYS_EXIT, 1U, 0U, 0U);
        }
        if (count == 0U) {
            break;
        }
        for (uint64_t index = 0U; index < count; index++) {
            const char character = (char)request.data[index];

            have_data = 1;
            if (character == '\n') {
                finish_line(&line_length);
            } else if (character != '\r') {
                append_line_character(character, &line_length);
            }
        }
        request.offset += count;
    }
    if (line_length != 0U || (have_data != 0 && sort_line_count == 0U)) {
        finish_line(&line_length);
    }
    sort_lines_ascending();
    for (uint64_t index = 0U; index < sort_line_count; index++) {
        write_text(sort_order[index]);
        write_text("\n");
    }
    if (sort_truncated != 0) {
        write_text("sort: line or entry limit reached\n");
    }
    (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
