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

#endif
