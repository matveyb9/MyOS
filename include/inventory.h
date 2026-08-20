#ifndef MYOS_INVENTORY_H
#define MYOS_INVENTORY_H

#include <stdint.h>

#define SYSTEM_INVENTORY_NAME_MAX 32U

struct system_inventory_boot_state {
    char firmware[SYSTEM_INVENTORY_NAME_MAX];
    char bootloader[SYSTEM_INVENTORY_NAME_MAX];
    char bootloader_version[SYSTEM_INVENTORY_NAME_MAX];
    uint64_t usable_memory_bytes;
    uint64_t memory_region_count;
    uint64_t initramfs_bytes;
    uint64_t initramfs_files;
    uint8_t initramfs_ready;
    uint8_t framebuffer_ready;
    uint8_t persistent_ready;
    uint8_t acpi_ready;
    uint8_t ahci_controller_ready;
    uint8_t ahci_probe_ready;
    uint8_t keyboard_ready;
    uint8_t mouse_ready;
};

void system_inventory_set_boot_state(const struct system_inventory_boot_state *state);
const struct system_inventory_boot_state *system_inventory_boot_state(void);

#endif
