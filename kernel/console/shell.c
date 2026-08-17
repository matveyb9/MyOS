#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <arch.h>
#include <pmm.h>
#include <serial.h>
#include <shell.h>

#define SHELL_LINE_CAPACITY 128U

static int text_equal(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return 0;
        }
        left++;
        right++;
    }
    return *left == *right;
}

static int text_starts_with(const char *text, const char *prefix) {
    while (*prefix != '\0') {
        if (*text != *prefix) {
            return 0;
        }
        text++;
        prefix++;
    }
    return 1;
}

static void print_prompt(void) {
    serial_write("myos> ");
}

static void print_help(void) {
    serial_write("Built-in commands:\n");
    serial_write("  help              List available commands.\n");
    serial_write("  version           Show kernel version and architecture.\n");
    serial_write("  meminfo           Show memory supplied by the bootloader.\n");
    serial_write("  firmware          Show the current firmware environment.\n");
    serial_write("  pmm               Show physical page-manager statistics.\n");
    serial_write("  alloc             Allocate and report one 4 KiB physical frame.\n");
    serial_write("  echo <text>       Print text.\n");
    serial_write("  clear             Clear the serial terminal.\n");
    serial_write("  crash             Trigger a divide error to test the IDT.\n");
    serial_write("  halt              Stop the processor safely.\n");
}

static void execute_command(const char *line, const struct shell_context *context) {
    if (line[0] == '\0') {
        return;
    }

    if (text_equal(line, "help")) {
        print_help();
        return;
    }

    if (text_equal(line, "version")) {
        serial_write("MyOS 0.3.0-dev (x86_64, freestanding C11 + NASM)\n");
        return;
    }

    if (text_equal(line, "meminfo")) {
        serial_write("Usable physical memory: ");
        serial_write_hex64(context->usable_memory_bytes);
        serial_write(" bytes across ");
        serial_write_hex64(context->memory_region_count);
        serial_write(" map regions.\n");
        return;
    }

    if (text_equal(line, "firmware")) {
        serial_write("Firmware: ");
        serial_write(context->firmware_name);
        serial_write("\n");
        return;
    }

    if (text_equal(line, "pmm")) {
        serial_write("Tracked frames: ");
        serial_write_hex64(pmm_tracked_frame_count());
        serial_write("; free frames: ");
        serial_write_hex64(pmm_free_frame_count());
        serial_write("; page size: 0x1000 bytes.\n");
        return;
    }

    if (text_equal(line, "alloc")) {
        const uint64_t frame = pmm_allocate_frame();
        if (frame == PMM_INVALID_ADDRESS) {
            serial_write("No free physical frame is available.\n");
        } else {
            serial_write("Allocated physical frame: ");
            serial_write_hex64(frame);
            serial_write("\n");
        }
        return;
    }

    if (text_starts_with(line, "echo ")) {
        serial_write(line + 5);
        serial_write("\n");
        return;
    }

    if (text_equal(line, "clear")) {
        serial_write("\x1b[2J\x1b[H");
        return;
    }

    if (text_equal(line, "crash")) {
        serial_write("Triggering divide error.\n");
        arch_trigger_divide_by_zero();
    }

    if (text_equal(line, "halt")) {
        serial_write("Halting CPU.\n");
        arch_halt();
    }

    serial_write("Unknown command: ");
    serial_write(line);
    serial_write(". Type 'help'.\n");
}

void shell_run(const struct shell_context *context) {
    char line[SHELL_LINE_CAPACITY];
    size_t length = 0U;

    serial_write("\nMyOS serial shell ready. Type 'help'.\n");
    print_prompt();

    for (;;) {
        const char input = serial_read_char();

        if (input == '\r' || input == '\n') {
            serial_write("\n");
            line[length] = '\0';
            execute_command(line, context);
            length = 0U;
            print_prompt();
            continue;
        }

        if (input == '\b' || input == 0x7f) {
            if (length > 0U) {
                length--;
                serial_write("\b \b");
            }
            continue;
        }

        if ((uint8_t)input >= 0x20U && (uint8_t)input <= 0x7eU) {
            if (length + 1U < SHELL_LINE_CAPACITY) {
                line[length] = input;
                length++;
                serial_write_char(input);
            }
        }
    }
}
