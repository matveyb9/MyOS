#include <stdint.h>

#include <arch.h>
#include <framebuffer.h>
#include <keyboard.h>
#include <mouse.h>

#define PS2_DATA_PORT 0x60U
#define PS2_STATUS_PORT 0x64U
#define PS2_STATUS_OUTPUT_FULL 0x01U
#define PS2_STATUS_INPUT_FULL 0x02U
#define PS2_STATUS_AUXILIARY 0x20U
#define PS2_COMMAND_READ_CONFIGURATION 0x20U
#define PS2_COMMAND_ENABLE_AUXILIARY 0xA8U
#define PS2_COMMAND_WRITE_CONFIGURATION 0x60U
#define PS2_COMMAND_WRITE_AUXILIARY 0xD4U
#define PS2_AUXILIARY_ENABLE_STREAMING 0xF4U
#define PS2_ACK 0xFAU
#define PS2_MOUSE_PACKET_SYNC 0x08U
#define PS2_MOUSE_PACKET_LEFT_BUTTON 0x01U
#define PS2_MOUSE_PACKET_X_SIGN 0x10U
#define PS2_MOUSE_PACKET_Y_SIGN 0x20U
#define PS2_MOUSE_PACKET_X_OVERFLOW 0x40U
#define PS2_MOUSE_PACKET_Y_OVERFLOW 0x80U

static uint8_t mouse_packet[3];
static uint8_t mouse_packet_index;
static uint8_t mouse_left_button;
static uint64_t received_packets;
static uint64_t dropped_packets;

static int ps2_wait_for_input_empty(void) {
    for (uint32_t spins = 0U; spins < 100000U; spins++) {
        if ((arch_in8(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) == 0U) {
            return 1;
        }
    }
    return 0;
}

static int ps2_write_command(uint8_t command) {
    if (ps2_wait_for_input_empty() == 0) {
        return 0;
    }
    arch_out8(PS2_STATUS_PORT, command);
    return 1;
}

static int ps2_write_data(uint8_t value) {
    if (ps2_wait_for_input_empty() == 0) {
        return 0;
    }
    arch_out8(PS2_DATA_PORT, value);
    return 1;
}

static int ps2_read_controller_byte(uint8_t *value) {
    for (uint32_t spins = 0U; spins < 100000U; spins++) {
        const uint8_t status = arch_in8(PS2_STATUS_PORT);

        if ((status & PS2_STATUS_OUTPUT_FULL) != 0U) {
            *value = arch_in8(PS2_DATA_PORT);
            return (status & PS2_STATUS_AUXILIARY) == 0U;
        }
    }
    return 0;
}

static int ps2_read_auxiliary_byte(uint8_t *value) {
    for (uint32_t spins = 0U; spins < 100000U; spins++) {
        const uint8_t status = arch_in8(PS2_STATUS_PORT);

        if ((status & PS2_STATUS_OUTPUT_FULL) != 0U) {
            *value = arch_in8(PS2_DATA_PORT);
            return (status & PS2_STATUS_AUXILIARY) != 0U;
        }
    }
    return 0;
}

static int64_t mouse_delta(uint8_t value, uint8_t sign_set) {
    return sign_set != 0U ? (int64_t)value - INT64_C(256) : (int64_t)value;
}

static void mouse_process_packet(void) {
    const uint8_t flags = mouse_packet[0];
    const int left_pressed = (flags & PS2_MOUSE_PACKET_LEFT_BUTTON) != 0U;
    char gui_action;

    if ((flags & (PS2_MOUSE_PACKET_X_OVERFLOW | PS2_MOUSE_PACKET_Y_OVERFLOW)) != 0U) {
        dropped_packets++;
        return;
    }
    gui_action = framebuffer_gui_handle_mouse(mouse_delta(mouse_packet[1], flags & PS2_MOUSE_PACKET_X_SIGN),
                                               mouse_delta(mouse_packet[2], flags & PS2_MOUSE_PACKET_Y_SIGN),
                                               left_pressed, mouse_left_button != 0U);
    if (gui_action != '\0') {
        keyboard_inject_char(gui_action);
    }
    mouse_left_button = left_pressed != 0 ? 1U : 0U;
    received_packets++;
}

int mouse_init(void) {
    uint8_t configuration;
    uint8_t response;

    mouse_packet_index = 0U;
    mouse_left_button = 0U;
    received_packets = 0U;
    dropped_packets = 0U;

    if (ps2_write_command(PS2_COMMAND_ENABLE_AUXILIARY) == 0
        || ps2_write_command(PS2_COMMAND_READ_CONFIGURATION) == 0
        || ps2_read_controller_byte(&configuration) == 0) {
        return 0;
    }
    configuration = (uint8_t)((configuration | 0x02U) & (uint8_t)~0x20U);
    if (ps2_write_command(PS2_COMMAND_WRITE_CONFIGURATION) == 0 || ps2_write_data(configuration) == 0
        || ps2_write_command(PS2_COMMAND_WRITE_AUXILIARY) == 0
        || ps2_write_data(PS2_AUXILIARY_ENABLE_STREAMING) == 0
        || ps2_read_auxiliary_byte(&response) == 0 || response != PS2_ACK) {
        return 0;
    }
    return 1;
}

void mouse_on_irq(uint8_t irq) {
    uint8_t status;
    uint8_t value;

    (void)irq;
    status = arch_in8(PS2_STATUS_PORT);
    if ((status & (PS2_STATUS_OUTPUT_FULL | PS2_STATUS_AUXILIARY))
        != (PS2_STATUS_OUTPUT_FULL | PS2_STATUS_AUXILIARY)) {
        return;
    }
    value = arch_in8(PS2_DATA_PORT);
    if (mouse_packet_index == 0U && (value & PS2_MOUSE_PACKET_SYNC) == 0U) {
        dropped_packets++;
        return;
    }
    mouse_packet[mouse_packet_index++] = value;
    if (mouse_packet_index == 3U) {
        mouse_packet_index = 0U;
        mouse_process_packet();
    }
}

uint64_t mouse_packet_count(void) {
    return received_packets;
}

uint64_t mouse_dropped_packet_count(void) {
    return dropped_packets;
}
