#include <stdint.h>

#include <ahci.h>
#include <paging.h>

#define AHCI_MMIO_VIRTUAL_ADDRESS UINT64_C(0xFFFFFFFFC0100000)
#define AHCI_MMIO_PAGES 2U
#define AHCI_GHC_OFFSET UINT64_C(0x04)
#define AHCI_PI_OFFSET UINT64_C(0x0C)
#define AHCI_PORT_BASE UINT64_C(0x100)
#define AHCI_PORT_STRIDE UINT64_C(0x80)
#define AHCI_PORT_SIG_OFFSET UINT64_C(0x24)
#define AHCI_PORT_SSTS_OFFSET UINT64_C(0x28)
#define AHCI_GHC_AE UINT32_C(0x80000000)
#define AHCI_SATA_SIGNATURE UINT32_C(0x00000101)

static uint32_t mmio_read32(uint64_t address) {
    return *(volatile uint32_t *)(uintptr_t)address;
}

static void mmio_write32(uint64_t address, uint32_t value) {
    *(volatile uint32_t *)(uintptr_t)address = value;
}

int ahci_probe_controller(const struct pci_device *device, struct ahci_probe *probe) {
    const uint64_t abar = device == (const struct pci_device *)0 ? 0U : (uint64_t)(device->bar5 & UINT32_C(0xFFFFFFF0));
    uint64_t virtual_address;
    uint32_t ports;

    if (device == (const struct pci_device *)0 || probe == (struct ahci_probe *)0 || abar == 0U) {
        return 0;
    }
    for (uint64_t page = 0U; page < AHCI_MMIO_PAGES; page++) {
        virtual_address = AHCI_MMIO_VIRTUAL_ADDRESS + page * PAGING_PAGE_SIZE;
        if (paging_map_mmio_page(virtual_address, abar + page * PAGING_PAGE_SIZE) == 0) {
            return 0;
        }
    }
    mmio_write32(AHCI_MMIO_VIRTUAL_ADDRESS + AHCI_GHC_OFFSET,
                 mmio_read32(AHCI_MMIO_VIRTUAL_ADDRESS + AHCI_GHC_OFFSET) | AHCI_GHC_AE);
    ports = mmio_read32(AHCI_MMIO_VIRTUAL_ADDRESS + AHCI_PI_OFFSET);
    probe->abar_physical = (uint32_t)abar;
    probe->ports_implemented = ports;
    probe->sata_ports = 0U;
    for (uint64_t port = 0U; port < 32U; port++) {
        const uint64_t port_base = AHCI_MMIO_VIRTUAL_ADDRESS + AHCI_PORT_BASE + port * AHCI_PORT_STRIDE;
        const uint32_t status = mmio_read32(port_base + AHCI_PORT_SSTS_OFFSET);
        const uint32_t signature = mmio_read32(port_base + AHCI_PORT_SIG_OFFSET);

        if ((ports & (UINT32_C(1) << port)) != 0U && (status & UINT32_C(0x0F)) == UINT32_C(3)
            && ((status >> 8U) & UINT32_C(0x0F)) == UINT32_C(1)
            && signature == AHCI_SATA_SIGNATURE) {
            probe->sata_ports |= UINT32_C(1) << port;
        }
    }
    return 1;
}
