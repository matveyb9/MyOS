#include <stdint.h>

#define MYOS_SYS_WRITE UINT64_C(1)
#define MYOS_SYS_EXIT UINT64_C(2)

static uint64_t system_call(uint64_t number, uint64_t descriptor, uint64_t buffer, uint64_t length) {
    uint64_t result;
    __asm__ volatile ("syscall"
                      : "=a"(result)
                      : "a"(number), "D"(descriptor), "S"(buffer), "d"(length)
                      : "rcx", "r11", "memory");
    return result;
}

void _start(void) {
    static const char message[] = "[hello] independent user process is running\n";
    (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)message, sizeof(message) - 1U);
    (void)system_call(MYOS_SYS_EXIT, 42U, 0U, 0U);
    for (;;) { __asm__ volatile ("pause"); }
}
