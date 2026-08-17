#include <stdint.h>

#include <ahci.h>
#include <paging.h>
#include <pmm.h>

#define AHCI_MMIO_VIRTUAL_ADDRESS UINT64_C(0xFFFFFFFFC0100000)
#define AHCI_MMIO_PAGES 2U
#define AHCI_GHC_OFFSET UINT64_C(0x04)
#define AHCI_PI_OFFSET UINT64_C(0x0C)
#define AHCI_PORT_BASE UINT64_C(0x100)
#define AHCI_PORT_STRIDE UINT64_C(0x80)
#define AHCI_PORT_CLB_OFFSET UINT64_C(0x00)
#define AHCI_PORT_FB_OFFSET UINT64_C(0x08)
#define AHCI_PORT_IS_OFFSET UINT64_C(0x10)
#define AHCI_PORT_CMD_OFFSET UINT64_C(0x18)
#define AHCI_PORT_TFD_OFFSET UINT64_C(0x20)
#define AHCI_PORT_SIG_OFFSET UINT64_C(0x24)
#define AHCI_PORT_SSTS_OFFSET UINT64_C(0x28)
#define AHCI_PORT_CI_OFFSET UINT64_C(0x38)
#define AHCI_GHC_AE UINT32_C(0x80000000)
#define AHCI_SATA_SIGNATURE UINT32_C(0x00000101)
#define AHCI_PORT_CMD_ST UINT32_C(0x00000001)
#define AHCI_PORT_CMD_FRE UINT32_C(0x00000010)
#define AHCI_TFD_BSY UINT32_C(0x00000080)
#define AHCI_TFD_DRQ UINT32_C(0x00000008)
#define AHCI_INTERRUPT_TFES UINT32_C(0x40000000)
#define AHCI_READ_DMA_EXT UINT8_C(0x25)
#define AHCI_POLL_LIMIT UINT64_C(100000)

static uint64_t active_port_base;

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
    active_port_base = 0U;
    for (uint64_t port = 0U; port < 32U; port++) {
        const uint64_t port_base = AHCI_MMIO_VIRTUAL_ADDRESS + AHCI_PORT_BASE + port * AHCI_PORT_STRIDE;
        const uint32_t status = mmio_read32(port_base + AHCI_PORT_SSTS_OFFSET);
        const uint32_t signature = mmio_read32(port_base + AHCI_PORT_SIG_OFFSET);

        if ((ports & (UINT32_C(1) << port)) != 0U && (status & UINT32_C(0x0F)) == UINT32_C(3)
            && ((status >> 8U) & UINT32_C(0x0F)) == UINT32_C(1)
            && signature == AHCI_SATA_SIGNATURE) {
            probe->sata_ports |= UINT32_C(1) << port;
            if (active_port_base == 0U) {
                active_port_base = port_base;
            }
        }
    }
    return 1;
}

static void zero_frame(uint64_t physical) {
    uint8_t *data = (uint8_t *)paging_physical_to_hhdm(physical);

    if (data != (uint8_t *)0) {
        for (uint64_t index = 0U; index < PMM_PAGE_SIZE; index++) {
            data[index] = 0U;
        }
    }
}

int ahci_read_sector(uint64_t lba, uint8_t *output) {
    uint64_t command_list = PMM_INVALID_ADDRESS;
    uint64_t received_fis = PMM_INVALID_ADDRESS;
    uint64_t command_table = PMM_INVALID_ADDRESS;
    uint64_t data_frame = PMM_INVALID_ADDRESS;
    uint8_t *list;
    uint8_t *table;
    uint8_t *data;

    if (output == (uint8_t *)0 || lba > UINT64_C(0x0000FFFFFFFFFFFF) || active_port_base == 0U) {
        return 0;
    }
    command_list = pmm_allocate_frame();
    received_fis = pmm_allocate_frame();
    command_table = pmm_allocate_frame();
    data_frame = pmm_allocate_frame();
    if (command_list == PMM_INVALID_ADDRESS || received_fis == PMM_INVALID_ADDRESS
        || command_table == PMM_INVALID_ADDRESS || data_frame == PMM_INVALID_ADDRESS) {
        return 0;
    }
    zero_frame(command_list); zero_frame(received_fis); zero_frame(command_table); zero_frame(data_frame);
    list = (uint8_t *)paging_physical_to_hhdm(command_list);
    table = (uint8_t *)paging_physical_to_hhdm(command_table);
    data = (uint8_t *)paging_physical_to_hhdm(data_frame);
    if (list == (uint8_t *)0 || table == (uint8_t *)0 || data == (uint8_t *)0) {
        return 0;
    }
    mmio_write32(active_port_base + AHCI_PORT_CMD_OFFSET,
                 mmio_read32(active_port_base + AHCI_PORT_CMD_OFFSET) & ~(AHCI_PORT_CMD_ST | AHCI_PORT_CMD_FRE));
    for (uint64_t wait = 0U; wait < AHCI_POLL_LIMIT && (mmio_read32(active_port_base + AHCI_PORT_CMD_OFFSET) & UINT32_C(0xC000)) != 0U; wait++) { }
    mmio_write32(active_port_base + AHCI_PORT_CLB_OFFSET, (uint32_t)command_list);
    mmio_write32(active_port_base + AHCI_PORT_CLB_OFFSET + 4U, 0U);
    mmio_write32(active_port_base + AHCI_PORT_FB_OFFSET, (uint32_t)received_fis);
    mmio_write32(active_port_base + AHCI_PORT_FB_OFFSET + 4U, 0U);
    mmio_write32(active_port_base + AHCI_PORT_CMD_OFFSET,
                 mmio_read32(active_port_base + AHCI_PORT_CMD_OFFSET) | AHCI_PORT_CMD_FRE | AHCI_PORT_CMD_ST);
    list[0] = 5U;
    list[2] = 1U;
    list[8] = (uint8_t)command_table; list[9] = (uint8_t)(command_table >> 8U);
    list[10] = (uint8_t)(command_table >> 16U); list[11] = (uint8_t)(command_table >> 24U);
    table[0] = UINT8_C(0x27); table[1] = UINT8_C(0x80); table[2] = AHCI_READ_DMA_EXT;
    table[4] = (uint8_t)lba; table[5] = (uint8_t)(lba >> 8U); table[6] = (uint8_t)(lba >> 16U);
    table[7] = UINT8_C(0x40); table[8] = (uint8_t)(lba >> 24U); table[9] = (uint8_t)(lba >> 32U);
    table[10] = (uint8_t)(lba >> 40U); table[12] = 1U;
    table[128] = (uint8_t)data_frame; table[129] = (uint8_t)(data_frame >> 8U);
    table[130] = (uint8_t)(data_frame >> 16U); table[131] = (uint8_t)(data_frame >> 24U);
    table[140] = UINT8_C(0xFF); table[141] = 1U;
    mmio_write32(active_port_base + AHCI_PORT_IS_OFFSET, UINT32_MAX);
    for (uint64_t wait = 0U; wait < AHCI_POLL_LIMIT && (mmio_read32(active_port_base + AHCI_PORT_TFD_OFFSET) & (AHCI_TFD_BSY | AHCI_TFD_DRQ)) != 0U; wait++) { }
    mmio_write32(active_port_base + AHCI_PORT_CI_OFFSET, 1U);
    for (uint64_t wait = 0U; wait < AHCI_POLL_LIMIT; wait++) {
        if ((mmio_read32(active_port_base + AHCI_PORT_IS_OFFSET) & AHCI_INTERRUPT_TFES) != 0U) { return 0; }
        if ((mmio_read32(active_port_base + AHCI_PORT_CI_OFFSET) & 1U) == 0U) {
            for (uint64_t index = 0U; index < AHCI_SECTOR_SIZE; index++) {
                output[index] = data[index];
            }
            return 1;
        }
    }
    return 0;
}

int ahci_read_boot_signature(uint16_t *signature) {
    uint8_t sector[AHCI_SECTOR_SIZE];

    if (signature == (uint16_t *)0 || ahci_read_sector(0U, sector) == 0) {
        return 0;
    }
    *signature = (uint16_t)sector[510] | ((uint16_t)sector[511] << 8U);
    return 1;
}
