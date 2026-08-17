#ifndef MYOS_PCI_H
#define MYOS_PCI_H

#include <stdint.h>

struct pci_device {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t programming_interface;
    uint32_t bar5;
};

uint32_t pci_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
int pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t programming_interface,
                   struct pci_device *result);

#endif
