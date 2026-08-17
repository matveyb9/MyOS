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

static uint64_t text_length(const char *text) {
    uint64_t length = 0U;

    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static void write_text(const char *text) {
    uint64_t length = text_length(text);

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

void _start(uint64_t argc, const char *arguments) {
    write_text("[argshow] argc=");
    write_number(argc);
    write_text(" arguments=");
    write_text(arguments);
    write_text("\n");
    (void)system_call(MYOS_SYS_EXIT, 11U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
