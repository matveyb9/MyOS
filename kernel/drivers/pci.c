#include <stdint.h>

#include <arch.h>
#include <pci.h>

#define PCI_CONFIG_ADDRESS UINT16_C(0xCF8)
#define PCI_CONFIG_DATA UINT16_C(0xCFC)

uint32_t pci_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    const uint32_t address = UINT32_C(0x80000000) | ((uint32_t)bus << 16U)
                             | ((uint32_t)device << 11U) | ((uint32_t)function << 8U)
                             | ((uint32_t)offset & UINT32_C(0xFC));

    arch_out32(PCI_CONFIG_ADDRESS, address);
    return arch_in32(PCI_CONFIG_DATA);
}

int pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t programming_interface,
                   struct pci_device *result) {
    if (result == (struct pci_device *)0) {
        return 0;
    }
    for (uint64_t device = 0U; device < 32U; device++) {
        const uint32_t id = pci_read32(0U, (uint8_t)device, 0U, 0U);
        const uint8_t header_type = (uint8_t)(pci_read32(0U, (uint8_t)device, 0U, UINT8_C(0x0C)) >> 16U);
        const uint64_t functions = (header_type & UINT8_C(0x80)) != 0U ? 8U : 1U;

        if ((uint16_t)id == UINT16_C(0xFFFF)) {
            continue;
        }
        for (uint64_t function = 0U; function < functions; function++) {
            const uint32_t function_id = pci_read32(0U, (uint8_t)device, (uint8_t)function, 0U);
            const uint32_t class_info = pci_read32(0U, (uint8_t)device, (uint8_t)function, UINT8_C(0x08));

            if ((uint16_t)function_id == UINT16_C(0xFFFF)
                || (uint8_t)(class_info >> 24U) != class_code
                || (uint8_t)(class_info >> 16U) != subclass
                || (uint8_t)(class_info >> 8U) != programming_interface) {
                continue;
            }
            result->bus = 0U;
            result->device = (uint8_t)device;
            result->function = (uint8_t)function;
            result->vendor_id = (uint16_t)function_id;
            result->device_id = (uint16_t)(function_id >> 16U);
            result->class_code = class_code;
            result->subclass = subclass;
            result->programming_interface = programming_interface;
            result->bar5 = pci_read32(0U, (uint8_t)device, (uint8_t)function, UINT8_C(0x24));
            return 1;
        }
    }
    return 0;
}
