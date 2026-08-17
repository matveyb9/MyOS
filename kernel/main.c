#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <limine.h>

#include <arch.h>
#include <framebuffer.h>
#include <gdt.h>
#include <heap.h>
#include <idt.h>
#include <irq.h>
#include <keyboard.h>
#include <lapic.h>
#include <paging.h>
#include <pic.h>
#include <pit.h>
#include <pmm.h>
#include <serial.h>
#include <shell.h>

/*
 * Limine performs the firmware-specific setup. Everything after kmain() is
 * owned by MyOS and will progressively move into independent subsystems.
 */
__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memory_map_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request executable_address_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_bootloader_info_request bootloader_info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_firmware_type_request firmware_type_request = {
    .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_stack_size_request stack_size_request = {
    .id = LIMINE_STACK_SIZE_REQUEST_ID,
    .revision = 0,
    .stack_size = 64U * 1024U
};

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

extern uint8_t __kernel_start[];
extern uint8_t __kernel_end[];
extern uint8_t __kernel_stack_guard[];

static int reserve_kernel_image(void) {
    struct limine_executable_address_response *response = executable_address_request.response;
    const uint64_t kernel_virtual_start = (uint64_t)(uintptr_t)__kernel_start;
    const uint64_t kernel_virtual_end = (uint64_t)(uintptr_t)__kernel_end;
    uint64_t physical_start;
    uint64_t image_size;

    if (response == NULL || kernel_virtual_start < response->virtual_base
        || kernel_virtual_end <= kernel_virtual_start) {
        return 0;
    }
    image_size = kernel_virtual_end - kernel_virtual_start;
    if (response->physical_base > UINT64_MAX - (kernel_virtual_start - response->virtual_base)) {
        return 0;
    }
    physical_start = response->physical_base + (kernel_virtual_start - response->virtual_base);
    if (physical_start > UINT64_MAX - image_size) {
        return 0;
    }
    return pmm_reserve_kernel_range(physical_start, physical_start + image_size);
}

static void initialise_framebuffer(void) {
    struct limine_framebuffer_response *response = framebuffer_request.response;
    if (response == NULL || response->framebuffer_count == 0U) {
        serial_write("[warn] Framebuffer unavailable; serial console remains active.\n");
        return;
    }

    struct limine_framebuffer *framebuffer = response->framebuffers[0];
    if (framebuffer_console_init(framebuffer) == 0) {
        serial_write("[warn] Framebuffer console requires a supported 32-bit RGB mode.\n");
        return;
    }

    serial_write("MYOS FRAMEBUFFER CONSOLE\n");
    serial_write("[ok] Framebuffer console: ");
    serial_write_hex64(framebuffer_console_columns());
    serial_write(" x ");
    serial_write_hex64(framebuffer_console_rows());
    serial_write(" text cells\n");
}

static const char *firmware_name(void) {
    struct limine_firmware_type_response *firmware = firmware_type_request.response;
    if (firmware == NULL) {
        return "unknown";
    }
    if (firmware->firmware_type == LIMINE_FIRMWARE_TYPE_X86BIOS) {
        return "BIOS";
    }
    if (firmware->firmware_type == LIMINE_FIRMWARE_TYPE_EFI64) {
        return "UEFI x86_64";
    }
    return "other";
}

static uint64_t usable_memory_bytes(void) {
    struct limine_memmap_response *memory_map = memory_map_request.response;
    uint64_t usable_bytes = 0U;

    if (memory_map == NULL) {
        return 0U;
    }
    for (uint64_t index = 0; index < memory_map->entry_count; index++) {
        const struct limine_memmap_entry *entry = memory_map->entries[index];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            usable_bytes += entry->length;
        }
    }
    return usable_bytes;
}

static uint64_t memory_region_count(void) {
    struct limine_memmap_response *memory_map = memory_map_request.response;
    return memory_map == NULL ? 0U : memory_map->entry_count;
}

static void report_boot_environment(void) {
    struct limine_bootloader_info_response *bootloader = bootloader_info_request.response;
    if (bootloader != NULL) {
        serial_write("[ok] Bootloader: ");
        serial_write(bootloader->name);
        serial_write(" ");
        serial_write(bootloader->version);
        serial_write("\n");
    }

    serial_write("[ok] Firmware: ");
    serial_write(firmware_name());
    serial_write("\n");

    serial_write("[ok] Usable physical memory: ");
    serial_write_hex64(usable_memory_bytes());
    serial_write(" bytes\n");
}

void kmain(void) {
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        arch_halt();
    }

    serial_init();
    gdt_init();
    idt_init();
    idt_install_irq_gates();
    irq_init();
    pmm_init(memory_map_request.response);
    const int kernel_image_reserved = reserve_kernel_image();
    paging_init(hhdm_request.response == NULL ? 0U : hhdm_request.response->offset);
    const int paging_ready = paging_take_control();
    const int stack_guard_ready = paging_map_guard((uint64_t)(uintptr_t)__kernel_stack_guard);
    const int heap_guard_ready = paging_map_guard(PAGING_KERNEL_HEAP_GUARD_ADDRESS);
    heap_init();
    (void)lapic_init();
    pic_init();
    irq_register_handler(0U, pit_on_irq);
    irq_register_handler(1U, keyboard_on_irq);
    pit_init(PIT_FREQUENCY_HZ);
    const int keyboard_ready = keyboard_init();
    pic_clear_mask(0U);
    if (keyboard_ready != 0) {
        pic_clear_mask(1U);
    }
    arch_enable_interrupts();

    serial_write("\nMyOS 0.8.0-dev — x86_64 kernel\n");
    serial_write("--------------------------------\n");

    report_boot_environment();
    initialise_framebuffer();

    serial_write("[ok] GDT and exception IDT installed.\n");
    serial_write("[ok] PMM free frames: ");
    serial_write_hex64(pmm_free_frame_count());
    serial_write("; kernel image reservation: ");
    serial_write(kernel_image_reserved != 0 ? "active\n" : "unavailable\n");
    serial_write("[ok] Managed paging: ");
    serial_write(paging_ready != 0 ? "active PML4 at " : "unavailable; using bootloader tables at ");
    serial_write_hex64(paging_active_root_physical());
    serial_write("\n");
    serial_write("[ok] Kernel heap: ");
    serial_write_hex64(heap_capacity_bytes());
    serial_write(" bytes reserved at ");
    serial_write_hex64(PAGING_KERNEL_HEAP_START);
    serial_write("; guard pages: ");
    serial_write(stack_guard_ready != 0 && heap_guard_ready != 0 ? "active\n" : "degraded\n");
    serial_write("[ok] Local APIC virtual wire: ");
    serial_write(lapic_is_active() != 0 ? "enabled\n" : "unavailable (legacy PIC only)\n");
    serial_write("[ok] PIC remapped; PIT frequency: ");
    serial_write_hex64(pit_frequency_hz());
    serial_write(" Hz; mask: ");
    serial_write_hex64(pic_current_mask());
    serial_write("\n");
    serial_write("[ok] PS/2 keyboard IRQ: ");
    serial_write(keyboard_ready != 0 ? "enabled\n" : "unavailable; serial input remains active\n");
    serial_write("[ok] Bootstrap complete. IRQ0 timer is enabled.\n");
    serial_write("[ok] Framebuffer text console is active; COM1 remains mirrored.\n");
    serial_write("[next] scheduler and kernel-thread context switching.\n");

    const struct shell_context shell_context = {
        .usable_memory_bytes = usable_memory_bytes(),
        .memory_region_count = memory_region_count(),
        .firmware_name = firmware_name()
    };
    shell_run(&shell_context);
}
