#include <stdint.h>

#include <syscall.h>

#define STACK_PROBE_BYTES UINT64_C(12288)

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

    while (text[length] != '\0') { length++; }
    (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)text, length);
}

static void write_number(uint64_t value) {
    char digits[21];
    uint64_t count = 0U;

    if (value == 0U) {
        write_text("0");
        return;
    }
    while (value != 0U) {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    }
    while (count != 0U) {
        count--;
        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)&digits[count], 1U);
    }
}

void _start(uint64_t argc, const char *arguments) {
    volatile uint8_t bytes[STACK_PROBE_BYTES];
    uint64_t checksum = 0U;

    (void)argc;
    (void)arguments;
    for (uint64_t index = 0U; index < STACK_PROBE_BYTES; index++) {
        bytes[index] = (uint8_t)(index & UINT64_C(0xFF));
        checksum += bytes[index];
    }
    write_text("stackprobe: ");
    write_number(STACK_PROBE_BYTES);
    write_text(" bytes checksum ");
    write_number(checksum);
    write_text("\n");
    (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
