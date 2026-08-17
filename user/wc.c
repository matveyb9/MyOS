#include <stdint.h>

#include <syscall.h>

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

static void write_number(uint64_t value) {
    char digits[21];
    uint64_t count = 0U;

    if (value == 0U) {
        write_text("0");
        return;
    }
    while (value != 0U) {
        digits[count] = (char)('0' + (value % 10U));
        value /= 10U;
        count++;
    }
    while (count != 0U) {
        count--;
        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)&digits[count], 1U);
    }
}

static int is_space(uint8_t character) {
    return character == ' ' || character == '\n' || character == '\r' || character == '\t';
}

void _start(uint64_t argc, const char *arguments) {
    struct myos_vfs_read_request request = { 0U, { 0 }, { 0 } };
    uint64_t name_length = 0U;
    uint64_t lines = 0U;
    uint64_t words = 0U;
    uint64_t bytes = 0U;
    int inside_word = 0;

    while (arguments[name_length] != '\0' && arguments[name_length] != ' '
           && name_length + 1U < MYOS_VFS_NAME_MAX) {
        request.path[name_length] = arguments[name_length];
        name_length++;
    }
    if (argc != 1U || name_length == 0U || arguments[name_length] != '\0') {
        write_text("Usage: run wc <file>\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    request.path[name_length] = '\0';
    for (;;) {
        const uint64_t count = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&request,
                                           sizeof(request));

        if (count == UINT64_MAX) {
            write_text("wc: unable to read file\n");
            (void)system_call(MYOS_SYS_EXIT, 1U, 0U, 0U);
        }
        if (count == 0U) {
            break;
        }
        for (uint64_t index = 0U; index < count; index++) {
            const uint8_t character = request.data[index];

            if (character == '\n') {
                lines++;
            }
            if (is_space(character) != 0) {
                inside_word = 0;
            } else if (inside_word == 0) {
                words++;
                inside_word = 1;
            }
        }
        bytes += count;
        request.offset += count;
    }
    write_number(lines);
    write_text(" lines, ");
    write_number(words);
    write_text(" words, ");
    write_number(bytes);
    write_text(" bytes: ");
    write_text(request.path);
    write_text("\n");
    (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
