#include <inventory.h>

static struct system_inventory_boot_state boot_state;

static void copy_text(char *destination, uint64_t capacity, const char *source) {
    uint64_t index = 0U;

    if (destination == (char *)0 || capacity == 0U) {
        return;
    }
    while (source != (const char *)0 && source[index] != '\0' && index + 1U < capacity) {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

void system_inventory_set_boot_state(const struct system_inventory_boot_state *state) {
    if (state == (const struct system_inventory_boot_state *)0) {
        return;
    }
    copy_text(boot_state.firmware, sizeof(boot_state.firmware), state->firmware);
    copy_text(boot_state.bootloader, sizeof(boot_state.bootloader), state->bootloader);
    copy_text(boot_state.bootloader_version, sizeof(boot_state.bootloader_version), state->bootloader_version);
    boot_state.usable_memory_bytes = state->usable_memory_bytes;
    boot_state.memory_region_count = state->memory_region_count;
    boot_state.initramfs_bytes = state->initramfs_bytes;
    boot_state.initramfs_files = state->initramfs_files;
    boot_state.initramfs_ready = state->initramfs_ready;
    boot_state.framebuffer_ready = state->framebuffer_ready;
    boot_state.persistent_ready = state->persistent_ready;
    boot_state.acpi_ready = state->acpi_ready;
    boot_state.ahci_controller_ready = state->ahci_controller_ready;
    boot_state.ahci_probe_ready = state->ahci_probe_ready;
    boot_state.keyboard_ready = state->keyboard_ready;
    boot_state.mouse_ready = state->mouse_ready;
}

const struct system_inventory_boot_state *system_inventory_boot_state(void) {
    return &boot_state;
}
