#include <stdint.h>

#include <syscall.h>

static uint64_t system_call(uint64_t number, uint64_t argument1, uint64_t argument2, uint64_t argument3) {
    register uint64_t rax __asm__("rax") = number;
    register uint64_t rdi __asm__("rdi") = argument1;
    register uint64_t rsi __asm__("rsi") = argument2;
    register uint64_t rdx __asm__("rdx") = argument3;

    __asm__ volatile ("syscall"
                      : "+a"(rax)
                      : "D"(rdi), "S"(rsi), "d"(rdx)
                      : "rcx", "r11", "memory");
    return rax;
}

void _start(uint64_t argc, const char *arguments) {
    uint64_t status = 0U;

    (void)argc;
    if (arguments[0] != '\0') {
        status = 2U;
    } else if (system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_BEGIN, 0U, 0U) == UINT64_MAX) {
        status = 1U;
    } else {
        for (;;) {
            char character;
            const uint64_t read_result = system_call(MYOS_SYS_READ, 0U, (uint64_t)(uintptr_t)&character, 1U);

            if (read_result == UINT64_MAX || read_result == 0U) {
                continue;
            }
            if (character == '\x1b' || character == 'q' || character == 'Q') {
                (void)system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_END, 0U, 0U);
                break;
            }
            (void)system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_INPUT, (uint64_t)(uint8_t)character, 0U);
        }
    }
    (void)system_call(MYOS_SYS_EXIT, status, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
