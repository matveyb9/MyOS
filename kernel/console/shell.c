#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <arch.h>
#include <framebuffer.h>
#include <heap.h>
#include <initramfs.h>
#include <irq.h>
#include <keyboard.h>
#include <pic.h>
#include <pit.h>
#include <paging.h>
#include <pmm.h>
#include <serial.h>
#include <scheduler.h>
#include <shell.h>
#include <syscall.h>
#include <user.h>

#define SHELL_LINE_CAPACITY 128U
#define SHELL_PAGING_TEST_ADDRESS UINT64_C(0xFFFFA00000000000)
#define AUTO_INIT_WAIT_TICKS UINT64_C(300)

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

static const char *task_state_name(enum task_state state) {
    if (state == TASK_STATE_READY) {
        return "ready";
    }
    if (state == TASK_STATE_RUNNING) {
        return "running";
    }
    if (state == TASK_STATE_SLEEPING) {
        return "sleeping";
    }
    if (state == TASK_STATE_ZOMBIE) {
        return "zombie";
    }
    if (state == TASK_STATE_WAITING) {
        return "waiting";
    }
    if (state == TASK_STATE_INPUT) {
        return "input";
    }
    return "unused";
}

static void print_prompt(void) {
    serial_write("kernel> ");
}

static int start_user_shell(void) {
    if (initramfs_start_init() == 0) {
        return 0;
    }
    serial_write("/init scheduled; kernel console input is now owned by user space.\n");
    for (;;) {
        arch_wait_for_interrupt();
    }
}

static int auto_init_cancelled(void) {
    const uint64_t deadline = pit_ticks() + AUTO_INIT_WAIT_TICKS;

    while (pit_ticks() < deadline) {
        char input;

        if (keyboard_has_char() != 0) {
            input = keyboard_read_char();
        } else if (serial_input_available() != 0) {
            input = serial_read_char();
        } else {
            arch_wait_for_interrupt();
            continue;
        }
        if (input == 'k' || input == 'K') {
            return 1;
        }
    }
    return 0;
}

static void print_help(void) {
    serial_write("Built-in commands:\n");
    serial_write("  help              List available commands.\n");
    serial_write("  version           Show kernel version and architecture.\n");
    serial_write("  meminfo           Show memory supplied by the bootloader.\n");
    serial_write("  firmware          Show the current firmware environment.\n");
    serial_write("  pmm               Show physical page-manager statistics.\n");
    serial_write("  ownmap            Show physical-frame ownership totals.\n");
    serial_write("  alloc             Allocate and report one 4 KiB physical frame.\n");
    serial_write("  pmmtest           Verify PMM allocate, reserve and free semantics.\n");
    serial_write("  ticks             Show elapsed PIT timer ticks.\n");
    serial_write("  irqs              Show enabled IRQ counters and PIC mask.\n");
    serial_write("  flags             Show RFLAGS and the interrupt-enable flag.\n");
    serial_write("  keyboard          Show PS/2 keyboard buffer diagnostics.\n");
    serial_write("  paging            Show active PML4 and MyOS mapping counters.\n");
    serial_write("  pagingtest        Verify map, translate, unmap and guard-page policy.\n");
    serial_write("  aspacetest        Verify isolated user PML4, CR3 switch and cleanup.\n");
    serial_write("  tasks             Show round-robin kernel-thread scheduler state.\n");
    serial_write("  syscalls          Show fast-syscall counters.\n");
    serial_write("  userdemo          Enter the first ring-3 sys_write demo.\n");
    serial_write("  initramfs         Show initramfs module diagnostics.\n");
    serial_write("  init              Load and enter /init ELF from initramfs.\n");
    serial_write("  heap              Show kernel heap statistics.\n");
    serial_write("  heaptest          Verify multi-page heap allocation, free and reuse.\n");
    serial_write("  pagefault         Trigger the controlled heap guard-page fault.\n");
    serial_write("  fbinfo            Show framebuffer console geometry and scroll counter.\n");
    serial_write("  fbdemo            Print enough lines to exercise framebuffer scrolling.\n");
    serial_write("  echo <text>       Print text.\n");
    serial_write("  clear             Clear serial and framebuffer terminals.\n");
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
        serial_write("MyOS 0.12.2-dev (x86_64, freestanding C11 + NASM)\n");
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

    if (text_equal(line, "ownmap")) {
        serial_write("Frame owners — free: ");
        serial_write_hex64(pmm_frame_owner_count(FRAME_OWNER_FREE));
        serial_write("; kernel: ");
        serial_write_hex64(pmm_frame_owner_count(FRAME_OWNER_KERNEL));
        serial_write("; bootloader: ");
        serial_write_hex64(pmm_frame_owner_count(FRAME_OWNER_BOOTLOADER));
        serial_write("; user: ");
        serial_write_hex64(pmm_frame_owner_count(FRAME_OWNER_USER));
        serial_write("; unmanaged: ");
        serial_write_hex64(pmm_frame_owner_count(FRAME_OWNER_UNMANAGED));
        serial_write("\n");
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

    if (text_equal(line, "pmmtest")) {
        const uint64_t before = pmm_free_frame_count();
        const uint64_t frame = pmm_allocate_frame();
        int passed = 0;

        if (frame != PMM_INVALID_ADDRESS && pmm_frame_is_free(frame) == 0
            && pmm_free_frame(frame) != 0 && pmm_reserve_frame(frame) != 0
            && pmm_free_frame(frame) != 0 && pmm_free_frame_count() == before) {
            passed = 1;
        }
        serial_write(passed != 0 ? "PMM reserve/free test passed.\n" : "PMM reserve/free test failed.\n");
        return;
    }

    if (text_equal(line, "ticks")) {
        serial_write("PIT ticks: ");
        serial_write_hex64(pit_ticks());
        serial_write(" at ");
        serial_write_hex64(pit_frequency_hz());
        serial_write(" Hz.\n");
        return;
    }

    if (text_equal(line, "irqs")) {
        serial_write("IRQ0 timer count: ");
        serial_write_hex64(irq_count(0U));
        serial_write("; IRQ1 keyboard count: ");
        serial_write_hex64(irq_count(1U));
        serial_write("; PIC mask: ");
        serial_write_hex64(pic_current_mask());
        serial_write("\n");
        return;
    }

    if (text_equal(line, "flags")) {
        const uint64_t flags = arch_read_rflags();
        serial_write("RFLAGS: ");
        serial_write_hex64(flags);
        serial_write("; IF: ");
        serial_write((flags & (UINT64_C(1) << 9U)) != 0U ? "enabled\n" : "disabled\n");
        return;
    }

    if (text_equal(line, "keyboard")) {
        serial_write("PS/2 keyboard IRQ count: ");
        serial_write_hex64(irq_count(1U));
        serial_write("; dropped characters: ");
        serial_write_hex64(keyboard_dropped_char_count());
        serial_write("\n");
        return;
    }

    if (text_equal(line, "paging")) {
        uint64_t lapic_physical = 0U;
        serial_write("Active PML4 physical: ");
        serial_write_hex64(paging_active_root_physical());
        serial_write("; MyOS mappings: ");
        serial_write_hex64(paging_mapping_count());
        serial_write("; LAPIC physical: ");
        if (paging_translate(PAGING_LAPIC_VIRTUAL_ADDRESS, &lapic_physical) != 0) {
            serial_write_hex64(lapic_physical);
        } else {
            serial_write("unmapped");
        }
        serial_write("\n");
        return;
    }

    if (text_equal(line, "pagingtest")) {
        const uint64_t before = paging_mapping_count();
        const uint64_t frame = pmm_allocate_frame();
        uint64_t translated = 0U;
        int passed = 0;

        if (frame != PMM_INVALID_ADDRESS
            && paging_map_page(SHELL_PAGING_TEST_ADDRESS, frame, PAGING_FLAG_WRITABLE) != 0
            && paging_translate(SHELL_PAGING_TEST_ADDRESS, &translated) != 0
            && translated == frame
            && paging_unmap_page(SHELL_PAGING_TEST_ADDRESS) != 0
            && paging_translate(SHELL_PAGING_TEST_ADDRESS, &translated) == 0
            && pmm_free_frame(frame) != 0
            && paging_mapping_count() == before
            && paging_is_guard_page(PAGING_KERNEL_HEAP_GUARD_ADDRESS) != 0) {
            passed = 1;
        }
        serial_write(passed != 0 ? "Paging map/unmap/guard test passed.\n"
                                 : "Paging map/unmap/guard test failed.\n");
        return;
    }

    if (text_equal(line, "aspacetest")) {
        serial_write(paging_space_self_test() != 0
                         ? "Isolated user address-space test passed.\n"
                         : "Isolated user address-space test failed.\n");
        return;
    }

    if (text_equal(line, "tasks")) {
        serial_write("Scheduler current task: ");
        serial_write_hex64(scheduler_current_task_id());
        serial_write("; runnable: ");
        serial_write_hex64(scheduler_runnable_task_count());
        serial_write("; switches: ");
        serial_write_hex64(scheduler_switch_count());
        serial_write("\n");
        for (uint64_t task_id = 0U; task_id < SCHEDULER_MAX_TASKS; task_id++) {
            serial_write("  task ");
            serial_write_hex64(task_id);
            serial_write(" ");
            serial_write(scheduler_task_name(task_id));
            serial_write(": ");
            serial_write(task_state_name(scheduler_task_state(task_id)));
            serial_write("; runs: ");
            serial_write_hex64(scheduler_task_run_count(task_id));
            serial_write("\n");
        }
        return;
    }

    if (text_equal(line, "syscalls")) {
        serial_write("Syscalls: ");
        serial_write_hex64(syscall_count());
        serial_write("; sys_write: ");
        serial_write_hex64(syscall_write_count());
        serial_write("\n");
        return;
    }

    if (text_equal(line, "userdemo")) {
        if (user_demo_prepare() == 0) {
            serial_write("Unable to prepare ring-3 demo pages.\n");
            return;
        }
        serial_write("Entering ring-3 demo at ");
        serial_write_hex64(user_demo_code_address());
        serial_write(" with user stack top ");
        serial_write_hex64(user_demo_stack_top());
        serial_write(".\n");
        user_demo_enter();
    }

    if (text_equal(line, "initramfs")) {
        serial_write("Initramfs bytes: ");
        serial_write_hex64(initramfs_size());
        serial_write("; files: ");
        serial_write_hex64(initramfs_file_count());
        serial_write("; /init: ");
        serial_write(initramfs_has_init() != 0 ? "available\n" : "missing\n");
        return;
    }

    if (text_equal(line, "init")) {
        if (start_user_shell() == 0) {
            serial_write("Unable to load /init from initramfs.\n");
        }
        return;
    }

    if (text_equal(line, "heap")) {
        serial_write("Kernel heap used: ");
        serial_write_hex64(heap_used_bytes());
        serial_write(" / ");
        serial_write_hex64(heap_capacity_bytes());
        serial_write(" bytes; mapped pages: ");
        serial_write_hex64(heap_mapped_page_count());
        serial_write("; active allocations: ");
        serial_write_hex64(heap_allocation_count());
        serial_write("; free blocks: ");
        serial_write_hex64(heap_free_block_count());
        serial_write("; reuses: ");
        serial_write_hex64(heap_reuse_count());
        serial_write("\n");
        return;
    }

    if (text_equal(line, "heaptest")) {
        uint8_t *first = kmalloc(64U);
        uint8_t *second = kmalloc((size_t)PAGING_PAGE_SIZE + 64U);
        uint8_t *reused;
        int passed = 0;

        if (first != (uint8_t *)0 && second != (uint8_t *)0) {
            first[0] = 0xA5U;
            first[63] = 0x5AU;
            second[0] = 0x3CU;
            second[PAGING_PAGE_SIZE] = 0xC3U;
            if (first[0] == 0xA5U && first[63] == 0x5AU && second[0] == 0x3CU
                && second[PAGING_PAGE_SIZE] == 0xC3U && kfree(first) != 0
                && kfree(second) != 0) {
                reused = kmalloc(64U);
                if (reused == first && kfree(reused) != 0) {
                    passed = 1;
                }
            }
        }
        serial_write(passed != 0 ? "Heap allocate/free/reuse test passed.\n"
                                 : "Heap allocate/free/reuse test failed.\n");
        return;
    }

    if (text_equal(line, "pagefault")) {
        volatile uint8_t *const fault_address =
            (volatile uint8_t *)(uintptr_t)PAGING_KERNEL_HEAP_GUARD_ADDRESS;
        serial_write("Triggering controlled heap guard-page fault.\n");
        (void)*fault_address;
        return;
    }

    if (text_equal(line, "fbinfo")) {
        if (framebuffer_console_available() == 0) {
            serial_write("Framebuffer console unavailable.\n");
        } else {
            serial_write("Framebuffer console: ");
            serial_write_hex64(framebuffer_console_columns());
            serial_write(" x ");
            serial_write_hex64(framebuffer_console_rows());
            serial_write(" cells; scrolls: ");
            serial_write_hex64(framebuffer_console_scroll_count());
            serial_write("\n");
        }
        return;
    }

    if (text_equal(line, "fbdemo")) {
        if (framebuffer_console_available() == 0) {
            serial_write("Framebuffer console unavailable.\n");
            return;
        }
        serial_write("Framebuffer scrolling demonstration begins.\n");
        for (uint64_t row = 0U; row < framebuffer_console_rows() + 4U; row++) {
            serial_write("FB DEMO ROW ");
            serial_write_hex64(row);
            serial_write(" — THE QUICK BROWN FOX JUMPS OVER 0123456789.\n");
        }
        return;
    }

    if (text_starts_with(line, "echo ")) {
        serial_write(line + 5);
        serial_write("\n");
        return;
    }

    if (text_equal(line, "clear")) {
        framebuffer_console_clear();
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

    if (initramfs_has_init() != 0) {
        serial_write("\n[boot] User shell starts in 3 seconds; press K for kernel shell.\n");
        if (auto_init_cancelled() == 0) {
            serial_write("[boot] Starting user shell.\n");
            if (start_user_shell() == 0) {
                serial_write("[boot] User shell failed to start; entering kernel shell.\n");
            }
        } else {
            serial_write("[boot] Automatic startup cancelled; entering kernel shell.\n");
        }
    } else {
        serial_write("[boot] User shell is unavailable; entering kernel shell.\n");
    }

    serial_write("MyOS kernel shell ready. Type 'help' for diagnostics or 'init' for user shell.\n");
    print_prompt();

    for (;;) {
        char input;

        if (keyboard_has_char() != 0) {
            input = keyboard_read_char();
        } else if (serial_input_available() != 0) {
            input = serial_read_char();
        } else {
            arch_wait_for_interrupt();
            continue;
        }

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
