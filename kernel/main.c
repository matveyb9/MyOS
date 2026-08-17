#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <limine.h>

#include <arch.h>
#include <gdt.h>
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

static uint32_t framebuffer_colour(const struct limine_framebuffer *framebuffer,
                                   uint8_t red, uint8_t green, uint8_t blue) {
    const uint64_t red_mask = (UINT64_C(1) << framebuffer->red_mask_size) - UINT64_C(1);
    const uint64_t green_mask = (UINT64_C(1) << framebuffer->green_mask_size) - UINT64_C(1);
    const uint64_t blue_mask = (UINT64_C(1) << framebuffer->blue_mask_size) - UINT64_C(1);

    return (uint32_t)((((uint64_t)red & red_mask) << framebuffer->red_mask_shift)
        | (((uint64_t)green & green_mask) << framebuffer->green_mask_shift)
        | (((uint64_t)blue & blue_mask) << framebuffer->blue_mask_shift));
}

static void framebuffer_fill(const struct limine_framebuffer *framebuffer, uint32_t colour) {
    const uint64_t pixels_per_row = framebuffer->pitch / sizeof(uint32_t);
    volatile uint32_t *pixels = (volatile uint32_t *)framebuffer->address;

    for (uint64_t y = 0; y < framebuffer->height; y++) {
        for (uint64_t x = 0; x < framebuffer->width; x++) {
            pixels[(y * pixels_per_row) + x] = colour;
        }
    }
}

static void framebuffer_rect(const struct limine_framebuffer *framebuffer,
                             uint64_t left, uint64_t top,
                             uint64_t width, uint64_t height,
                             uint32_t colour) {
    const uint64_t pixels_per_row = framebuffer->pitch / sizeof(uint32_t);
    volatile uint32_t *pixels = (volatile uint32_t *)framebuffer->address;
    const uint64_t right = left + width;
    const uint64_t bottom = top + height;

    for (uint64_t y = top; y < bottom && y < framebuffer->height; y++) {
        for (uint64_t x = left; x < right && x < framebuffer->width; x++) {
            pixels[(y * pixels_per_row) + x] = colour;
        }
    }
}

static void initialise_framebuffer(void) {
    struct limine_framebuffer_response *response = framebuffer_request.response;
    if (response == NULL || response->framebuffer_count == 0U) {
        serial_write("[warn] Framebuffer unavailable; serial console remains active.\n");
        return;
    }

    struct limine_framebuffer *framebuffer = response->framebuffers[0];
    if (framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB || framebuffer->bpp != 32U) {
        serial_write("[warn] Framebuffer format is not 32-bit RGB.\n");
        return;
    }

    const uint32_t background = framebuffer_colour(framebuffer, 13U, 20U, 34U);
    const uint32_t accent = framebuffer_colour(framebuffer, 36U, 170U, 224U);
    const uint32_t panel = framebuffer_colour(framebuffer, 24U, 39U, 62U);

    framebuffer_fill(framebuffer, background);
    framebuffer_rect(framebuffer, 0U, 0U, framebuffer->width, 6U, accent);
    framebuffer_rect(framebuffer, 48U, 48U, framebuffer->width - 96U, 84U, panel);

    serial_write("[ok] Framebuffer initialised: ");
    serial_write_hex64(framebuffer->width);
    serial_write(" x ");
    serial_write_hex64(framebuffer->height);
    serial_write(" pixels\n");
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
    paging_init(hhdm_request.response == NULL ? 0U : hhdm_request.response->offset);
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

    serial_write("\nMyOS 0.4.0-dev — x86_64 kernel\n");
    serial_write("--------------------------------\n");

    report_boot_environment();
    initialise_framebuffer();

    serial_write("[ok] GDT and exception IDT installed.\n");
    serial_write("[ok] PMM free frames: ");
    serial_write_hex64(pmm_free_frame_count());
    serial_write("\n");
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
    serial_write("[next] framebuffer text console and richer keyboard layouts.\n");

    const struct shell_context shell_context = {
        .usable_memory_bytes = usable_memory_bytes(),
        .memory_region_count = memory_region_count(),
        .firmware_name = firmware_name()
    };
    shell_run(&shell_context);
}
