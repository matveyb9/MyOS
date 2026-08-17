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

void _start(uint64_t argc, const char *arguments) {
    char buffer[128];

    (void)argc;
    (void)arguments;
    for (;;) {
        const uint64_t count = system_call(MYOS_SYS_READ, 1U, (uint64_t)(uintptr_t)buffer, sizeof(buffer));

        if (count == 0U) {
            (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
        }
        if (count == UINT64_MAX) {
            (void)system_call(MYOS_SYS_SLEEP, 1U, 0U, 0U);
            continue;
        }
        if (system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)buffer, count) != count) {
            (void)system_call(MYOS_SYS_EXIT, 1U, 0U, 0U);
        }
    }
}
