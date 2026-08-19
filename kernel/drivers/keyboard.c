#include <stdint.h>

#include <arch.h>
#include <keyboard.h>
#include <scheduler.h>
#include <syscall.h>

#define PS2_DATA_PORT 0x60U
#define PS2_STATUS_PORT 0x64U
#define PS2_STATUS_OUTPUT_FULL 0x01U
#define PS2_STATUS_INPUT_FULL 0x02U
#define PS2_KEYBOARD_ENABLE_SCANNING 0xF4U
#define PS2_ACK 0xFAU
#define PS2_RESEND 0xFEU

static volatile uint8_t input_head;
static volatile uint8_t input_tail;
static volatile char input_buffer[256];
static volatile uint64_t dropped_characters;
static uint8_t shift_held;
static uint8_t control_held;
static uint8_t alt_held;
static uint8_t extended_prefix;

static const char unshifted_set1[128] = {
    [0x01] = 0x1b, [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0a] = '9',
    [0x0b] = '0', [0x0c] = '-', [0x0d] = '=', [0x0e] = '\b', [0x0f] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1a] = '[', [0x1b] = ']', [0x1c] = '\n', [0x1e] = 'a', [0x1f] = 's',
    [0x20] = 'd', [0x21] = 'f', [0x22] = 'g', [0x23] = 'h', [0x24] = 'j',
    [0x25] = 'k', [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2b] = '\\', [0x2c] = 'z', [0x2d] = 'x', [0x2e] = 'c', [0x2f] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.',
    [0x35] = '/', [0x39] = ' '
};

static const char shifted_set1[128] = {
    [0x01] = 0x1b, [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0a] = '(',
    [0x0b] = ')', [0x0c] = '_', [0x0d] = '+', [0x0e] = '\b', [0x0f] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
    [0x1a] = '{', [0x1b] = '}', [0x1c] = '\n', [0x1e] = 'A', [0x1f] = 'S',
    [0x20] = 'D', [0x21] = 'F', [0x22] = 'G', [0x23] = 'H', [0x24] = 'J',
    [0x25] = 'K', [0x26] = 'L', [0x27] = ':', [0x28] = '"', [0x29] = '~',
    [0x2b] = '|', [0x2c] = 'Z', [0x2d] = 'X', [0x2e] = 'C', [0x2f] = 'V',
    [0x30] = 'B', [0x31] = 'N', [0x32] = 'M', [0x33] = '<', [0x34] = '>',
    [0x35] = '?', [0x39] = ' '
};

static int ps2_wait_for_input_empty(void) {
    for (uint32_t spins = 0U; spins < 100000U; spins++) {
        if ((arch_in8(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) == 0U) {
            return 1;
        }
    }
    return 0;
}

static int ps2_read_byte(uint8_t *value) {
    for (uint32_t spins = 0U; spins < 100000U; spins++) {
        if ((arch_in8(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) != 0U) {
            *value = arch_in8(PS2_DATA_PORT);
            return 1;
        }
    }
    return 0;
}

static void keyboard_push_char(char character) {
    const uint8_t next_head = (uint8_t)(input_head + 1U);

    if (next_head == input_tail) {
        dropped_characters++;
        return;
    }
    input_buffer[input_head] = character;
    input_head = next_head;
}

static void keyboard_process_scancode(uint8_t scan_code) {
    const uint8_t key_released = scan_code & 0x80U;
    const uint8_t key_code = scan_code & 0x7fU;
    char character;

    if (scan_code == 0xe0U) {
        extended_prefix = 1U;
        return;
    }

    if (key_code == 0x2aU || key_code == 0x36U) {
        shift_held = key_released == 0U ? 1U : 0U;
        extended_prefix = 0U;
        return;
    }
    if (key_code == 0x1dU) {
        control_held = key_released == 0U ? 1U : 0U;
        extended_prefix = 0U;
        return;
    }
    if (key_code == 0x38U) {
        alt_held = key_released == 0U ? 1U : 0U;
        extended_prefix = 0U;
        return;
    }
    if (key_released != 0U) {
        extended_prefix = 0U;
        return;
    }
    if (extended_prefix != 0U) {
        extended_prefix = 0U;
        if (key_code == 0x4bU) {
            character = (char)MYOS_INPUT_KEY_LEFT;
        } else if (key_code == 0x4dU) {
            character = (char)MYOS_INPUT_KEY_RIGHT;
        } else if (key_code == 0x48U) {
            character = (char)MYOS_INPUT_KEY_UP;
        } else if (key_code == 0x50U) {
            character = (char)MYOS_INPUT_KEY_DOWN;
        } else if (key_code == 0x53U) {
            character = (char)MYOS_INPUT_KEY_DELETE;
        } else if (key_code == 0x47U) {
            character = (char)MYOS_INPUT_KEY_HOME;
        } else if (key_code == 0x4fU) {
            character = (char)MYOS_INPUT_KEY_END;
        } else {
            return;
        }
    } else {
        if (alt_held != 0U && key_code == 0x0fU) {
            character = (char)MYOS_INPUT_KEY_ALT_TAB;
        } else if (alt_held != 0U && key_code == 0x3eU) {
            character = (char)MYOS_INPUT_KEY_ALT_F4;
        } else if (control_held != 0U && key_code == 0x30U) {
            character = (char)MYOS_INPUT_KEY_CTRL_B;
        } else {
            character = shift_held != 0U ? shifted_set1[key_code] : unshifted_set1[key_code];
            if (control_held != 0U && character >= 'a' && character <= 'z') {
                character = (char)(character - 'a' + 1);
            }
        }
    }
    if (character != '\0') {
        keyboard_push_char(character);
        scheduler_wake_console_input();
    }
}

int keyboard_init(void) {
    uint8_t response;

    input_head = 0U;
    input_tail = 0U;
    dropped_characters = 0U;
    shift_held = 0U;
    control_held = 0U;
    alt_held = 0U;
    extended_prefix = 0U;

    while ((arch_in8(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) != 0U) {
        (void)arch_in8(PS2_DATA_PORT);
    }

    for (uint8_t attempt = 0U; attempt < 3U; attempt++) {
        if (ps2_wait_for_input_empty() == 0) {
            return 0;
        }
        arch_out8(PS2_DATA_PORT, PS2_KEYBOARD_ENABLE_SCANNING);
        if (ps2_read_byte(&response) == 0) {
            return 0;
        }
        if (response == PS2_ACK) {
            return 1;
        }
        if (response != PS2_RESEND) {
            return 0;
        }
    }
    return 0;
}

void keyboard_on_irq(uint8_t irq) {
    (void)irq;
    if ((arch_in8(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) != 0U) {
        keyboard_process_scancode(arch_in8(PS2_DATA_PORT));
    }
}

void keyboard_inject_char(char character) {
    if (character == '\0') {
        return;
    }
    keyboard_push_char(character);
    scheduler_wake_console_input();
}

int keyboard_has_char(void) {
    return input_head != input_tail;
}

char keyboard_read_char(void) {
    char character;

    if (input_head == input_tail) {
        return '\0';
    }
    character = input_buffer[input_tail];
    input_tail = (uint8_t)(input_tail + 1U);
    return character;
}

uint64_t keyboard_dropped_char_count(void) {
    return dropped_characters;
}
