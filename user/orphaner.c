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
    static const char message[] = "[orphaner] spawning sleeper then exiting\n";
    const struct myos_spawn_request request = { "/system/core/apps/sleeper.elf", "", UINT64_MAX, UINT64_MAX };
    const uint64_t child = system_call(MYOS_SYS_SPAWN, 0U, (uint64_t)(uintptr_t)&request,
                                       sizeof(request));

    (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)message, sizeof(message) - 1U);
    (void)system_call(MYOS_SYS_EXIT, child == UINT64_MAX ? 1U : 13U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
