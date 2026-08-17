#ifndef MYOS_AHCI_H
#define MYOS_AHCI_H

#include <stdint.h>

#include <pci.h>

struct ahci_probe {
    uint32_t abar_physical;
    uint32_t ports_implemented;
    uint32_t sata_ports;
};

int ahci_probe_controller(const struct pci_device *device, struct ahci_probe *probe);
#define AHCI_SECTOR_SIZE 512U
#define AHCI_DATA_LBA_START UINT64_C(67584)
#define AHCI_DATA_LBA_END UINT64_C(262110)
#define AHCI_DATA_TEST_LBA AHCI_DATA_LBA_END

int ahci_data_lba_valid(uint64_t lba);
int ahci_read_sector(uint64_t lba, uint8_t *data);
int ahci_write_data_sector(uint64_t lba, const uint8_t *data);
int ahci_read_boot_signature(uint16_t *signature);

#endif
