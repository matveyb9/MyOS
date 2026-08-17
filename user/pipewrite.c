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

void _start(uint64_t argc, const char *arguments) {
    const uint64_t length = text_length(arguments);
    const uint64_t result = argc == 1U && length != 0U
                                ? system_call(MYOS_SYS_WRITE, 2U, (uint64_t)(uintptr_t)arguments, length)
                                : UINT64_MAX;

    (void)system_call(MYOS_SYS_EXIT, result == length ? 0U : 1U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
