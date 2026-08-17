#include <stdint.h>

#include <arch.h>
#include <serial.h>

#define COM1 0x3F8U

static int serial_transmitter_empty(void) {
    return (arch_in8(COM1 + 5U) & 0x20U) != 0U;
}

int serial_input_available(void) {
    return (arch_in8(COM1 + 5U) & 0x01U) != 0U;
}

void serial_init(void) {
    arch_out8(COM1 + 1U, 0x00U);
    arch_out8(COM1 + 3U, 0x80U);
    arch_out8(COM1 + 0U, 0x03U);
    arch_out8(COM1 + 1U, 0x00U);
    arch_out8(COM1 + 3U, 0x03U);
    arch_out8(COM1 + 2U, 0xC7U);
    arch_out8(COM1 + 4U, 0x0BU);
}

void serial_write_char(char character) {
    if (character == '\n') {
        serial_write_char('\r');
    }

    while (serial_transmitter_empty() == 0) {
    }
    arch_out8(COM1, (uint8_t)character);
}

void serial_write(const char *text) {
    while (*text != '\0') {
        serial_write_char(*text);
        text++;
    }
}

char serial_read_char(void) {
    while (serial_input_available() == 0) {
    }
    return (char)arch_in8(COM1);
}

void serial_write_hex64(uint64_t value) {
    static const char digits[] = "0123456789ABCDEF";

    serial_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        const uint8_t nibble = (uint8_t)((value >> (uint8_t)shift) & 0xFU);
        serial_write_char(digits[nibble]);
    }
}
