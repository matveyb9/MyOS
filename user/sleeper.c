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

void _start(void) {
    static const char started[] = "[sleeper] blocking for 2 seconds\n";
    static const char finished[] = "[sleeper] woke up and is exiting\n";

    (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)started, sizeof(started) - 1U);
    (void)system_call(MYOS_SYS_SLEEP, UINT64_C(200), 0U, 0U);
    (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)finished, sizeof(finished) - 1U);
    (void)system_call(MYOS_SYS_EXIT, 7U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
