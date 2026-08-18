#ifndef MYOS_SDK_MYOS_H
#define MYOS_SDK_MYOS_H

#include <stdint.h>

#define MYOS_ABI_VERSION UINT64_C(0x00010000)

#define MYOS_SYS_WRITE UINT64_C(1)
#define MYOS_SYS_EXIT UINT64_C(2)
#define MYOS_SYS_READ UINT64_C(3)
#define MYOS_SYS_TICKS UINT64_C(4)
#define MYOS_SYS_GETPID UINT64_C(8)
#define MYOS_SYS_SLEEP UINT64_C(9)

#define MYOS_STDOUT UINT64_C(1)
#define MYOS_SYSCALL_ERROR UINT64_MAX

static inline uint64_t myos_syscall(uint64_t number, uint64_t descriptor, uint64_t buffer,
                                    uint64_t length) {
    uint64_t result;

    __asm__ volatile ("syscall"
                      : "=a"(result)
                      : "a"(number), "D"(descriptor), "S"(buffer), "d"(length)
                      : "rcx", "r11", "memory");
    return result;
}

static inline uint64_t myos_write(uint64_t descriptor, const void *data, uint64_t length) {
    return myos_syscall(MYOS_SYS_WRITE, descriptor, (uint64_t)(uintptr_t)data, length);
}

static inline uint64_t myos_text_length(const char *text) {
    uint64_t length = 0U;

    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static inline void myos_write_text(const char *text) {
    uint64_t remaining = myos_text_length(text);

    while (remaining != 0U) {
        const uint64_t chunk = remaining > UINT64_C(256) ? UINT64_C(256) : remaining;

        (void)myos_write(MYOS_STDOUT, text, chunk);
        text += chunk;
        remaining -= chunk;
    }
}

static inline uint64_t myos_getpid(void) {
    return myos_syscall(MYOS_SYS_GETPID, 0U, 0U, 0U);
}

static inline uint64_t myos_ticks(void) {
    return myos_syscall(MYOS_SYS_TICKS, 0U, 0U, 0U);
}

static inline void myos_exit(uint64_t status) __attribute__((noreturn));

static inline void myos_exit(uint64_t status) {
    (void)myos_syscall(MYOS_SYS_EXIT, status, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}

int myos_main(uint64_t argc, const char *arguments);

#endif
