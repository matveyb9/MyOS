#include <stdint.h>

#include <acpi.h>
#include <arch.h>

#define ACPI_HEADER_SIZE UINT64_C(36)
#define ACPI_FADT_DSDT_OFFSET UINT64_C(40)
#define ACPI_FADT_PM1A_CONTROL_OFFSET UINT64_C(64)
#define ACPI_FADT_PM1B_CONTROL_OFFSET UINT64_C(68)
#define ACPI_FADT_X_DSDT_OFFSET UINT64_C(140)
#define ACPI_SLP_TYP_SHIFT UINT16_C(10)
#define ACPI_SLP_EN UINT16_C(0x2000)
#define ACPI_QEMU_PM_PORT UINT16_C(0x0604)
#define ACPI_BOCHS_PM_PORT UINT16_C(0xB004)

static uint16_t pm1a_control_port;
static uint16_t pm1b_control_port;
static uint16_t sleep_type_a;
static uint16_t sleep_type_b;
static int power_ready;

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) | ((uint32_t)data[2] << 16U)
           | ((uint32_t)data[3] << 24U);
}

static uint64_t read_u64(const uint8_t *data) {
    uint64_t value = 0U;

    for (uint64_t index = 0U; index < 8U; index++) {
        value |= (uint64_t)data[index] << (index * 8U);
    }
    return value;
}

static int signature_equal(const uint8_t *data, const char *signature) {
    for (uint64_t index = 0U; index < 4U; index++) {
        if (data[index] != (uint8_t)signature[index]) {
            return 0;
        }
    }
    return 1;
}

static const uint8_t *physical_to_hhdm(uint64_t physical, uint64_t hhdm_offset) {
    if (physical == 0U || physical > UINT64_MAX - hhdm_offset) {
        return (const uint8_t *)0;
    }
    return (const uint8_t *)(uintptr_t)(physical + hhdm_offset);
}

static int aml_integer(const uint8_t *aml, uint64_t length, uint64_t *offset, uint8_t *value) {
    uint8_t opcode;

    if (*offset >= length || value == (uint8_t *)0) {
        return 0;
    }
    opcode = aml[*offset];
    if (opcode == 0x00U || opcode == 0x01U) {
        *value = opcode;
        (*offset)++;
        return 1;
    }
    if (opcode != 0x0aU || *offset + 1U >= length) {
        return 0;
    }
    *value = aml[*offset + 1U];
    *offset += 2U;
    return 1;
}

static int find_sleep_types(const uint8_t *dsdt, uint64_t length, uint16_t *type_a, uint16_t *type_b) {
    const uint8_t *aml;
    uint64_t aml_length;

    if (dsdt == (const uint8_t *)0 || length <= ACPI_HEADER_SIZE || type_a == (uint16_t *)0
        || type_b == (uint16_t *)0) {
        return 0;
    }
    aml = dsdt + ACPI_HEADER_SIZE;
    aml_length = length - ACPI_HEADER_SIZE;
    for (uint64_t index = 0U; index + 6U < aml_length; index++) {
        uint64_t offset;
        uint64_t package_length;
        uint64_t package_end;
        uint8_t following;
        uint8_t element_count;
        uint8_t value_a;
        uint8_t value_b;

        if (aml[index] != '_' || aml[index + 1U] != 'S' || aml[index + 2U] != '5'
            || aml[index + 3U] != '_' || aml[index + 4U] != 0x12U) {
            continue;
        }
        offset = index + 5U;
        if (offset >= aml_length) {
            continue;
        }
        following = aml[offset] >> 6U;
        if (following > 3U || offset + 1U + following >= aml_length) {
            continue;
        }
        package_length = (uint64_t)(aml[offset] & 0x3fU);
        for (uint8_t byte = 0U; byte < following; byte++) {
            package_length |= (uint64_t)aml[offset + 1U + byte] << (4U + byte * 8U);
        }
        offset += 1U + following;
        if (package_length == 0U || package_length > aml_length - offset) {
            continue;
        }
        package_end = offset + package_length;
        element_count = aml[offset++];
        if (element_count < 2U || offset >= package_end || aml_integer(aml, package_end, &offset, &value_a) == 0
            || aml_integer(aml, package_end, &offset, &value_b) == 0) {
            continue;
        }
        *type_a = (uint16_t)((uint16_t)value_a << ACPI_SLP_TYP_SHIFT);
        *type_b = (uint16_t)((uint16_t)value_b << ACPI_SLP_TYP_SHIFT);
        return 1;
    }
    return 0;
}

static const uint8_t *find_fadt(const uint8_t *root, uint64_t entry_size, uint64_t hhdm_offset) {
    const uint64_t length = read_u32(root + 4U);

    if (length < ACPI_HEADER_SIZE || (length - ACPI_HEADER_SIZE) % entry_size != 0U) {
        return (const uint8_t *)0;
    }
    for (uint64_t offset = ACPI_HEADER_SIZE; offset < length; offset += entry_size) {
        const uint64_t physical = entry_size == 8U ? read_u64(root + offset) : read_u32(root + offset);
        const uint8_t *table = physical_to_hhdm(physical, hhdm_offset);

        if (table != (const uint8_t *)0 && signature_equal(table, "FACP") != 0) {
            return table;
        }
    }
    return (const uint8_t *)0;
}

int acpi_power_init(const void *rsdp, uint64_t hhdm_offset) {
    const uint8_t *root;
    const uint8_t *fadt;
    const uint8_t *dsdt;
    uint64_t root_physical;
    uint64_t root_entry_size;
    uint64_t dsdt_physical;
    uint64_t dsdt_length;

    power_ready = 0;
    pm1a_control_port = 0U;
    pm1b_control_port = 0U;
    sleep_type_a = 0U;
    sleep_type_b = 0U;
    if (rsdp == (const void *)0) {
        return 0;
    }
    if (((const uint8_t *)rsdp)[15] >= 2U && read_u64((const uint8_t *)rsdp + 24U) != 0U) {
        root_physical = read_u64((const uint8_t *)rsdp + 24U);
        root_entry_size = 8U;
    } else {
        root_physical = read_u32((const uint8_t *)rsdp + 16U);
        root_entry_size = 4U;
    }
    root = physical_to_hhdm(root_physical, hhdm_offset);
    if (root == (const uint8_t *)0 || (root_entry_size == 8U && signature_equal(root, "XSDT") == 0)
        || (root_entry_size == 4U && signature_equal(root, "RSDT") == 0)) {
        return 0;
    }
    fadt = find_fadt(root, root_entry_size, hhdm_offset);
    if (fadt == (const uint8_t *)0 || read_u32(fadt + 4U) < ACPI_FADT_X_DSDT_OFFSET + 8U) {
        return 0;
    }
    dsdt_physical = read_u64(fadt + ACPI_FADT_X_DSDT_OFFSET);
    if (dsdt_physical == 0U) {
        dsdt_physical = read_u32(fadt + ACPI_FADT_DSDT_OFFSET);
    }
    dsdt = physical_to_hhdm(dsdt_physical, hhdm_offset);
    if (dsdt == (const uint8_t *)0 || signature_equal(dsdt, "DSDT") == 0) {
        return 0;
    }
    dsdt_length = read_u32(dsdt + 4U);
    pm1a_control_port = (uint16_t)read_u32(fadt + ACPI_FADT_PM1A_CONTROL_OFFSET);
    pm1b_control_port = (uint16_t)read_u32(fadt + ACPI_FADT_PM1B_CONTROL_OFFSET);
    if (pm1a_control_port == 0U || find_sleep_types(dsdt, dsdt_length, &sleep_type_a, &sleep_type_b) == 0) {
        return 0;
    }
    power_ready = 1;
    return 1;
}

int acpi_power_is_ready(void) {
    return power_ready;
}

void acpi_poweroff(void) {
    arch_disable_interrupts();
    if (power_ready != 0) {
        arch_out16(pm1a_control_port, (uint16_t)(sleep_type_a | ACPI_SLP_EN));
        if (pm1b_control_port != 0U) {
            arch_out16(pm1b_control_port, (uint16_t)(sleep_type_b | ACPI_SLP_EN));
        }
    }
    arch_out16(ACPI_QEMU_PM_PORT, ACPI_SLP_EN);
    arch_out16(ACPI_BOCHS_PM_PORT, ACPI_SLP_EN);
    arch_halt();
}
