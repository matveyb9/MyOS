#include <stddef.h>
#include <stdint.h>

#include <arch.h>
#include <gdt.h>
#include <initramfs.h>
#include <limine.h>
#include <paging.h>
#include <pmm.h>
#include <scheduler.h>
#include <vfs.h>

#define CPIO_HEADER_SIZE 110U
#define ELF64_HEADER_SIZE 64U
#define ELF64_PROGRAM_HEADER_SIZE 56U
#define ELF64_MACHINE_X86_64 UINT16_C(0x003E)
#define ELF64_PT_LOAD UINT32_C(1)
#define ELF64_PF_WRITE UINT32_C(2)
#define INIT_STACK_PAGE UINT64_C(0x00007FFFFFFFF000)
#define INIT_STACK_GUARD UINT64_C(0x00007FFFFFFFE000)
#define INIT_STACK_TOP UINT64_C(0x00007FFFFFFFFFF0)
#define INIT_MAX_PAGES 32U

struct elf64_header {
    uint8_t identification[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t program_header_offset;
    uint64_t section_header_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_header_size;
    uint16_t program_header_count;
    uint16_t section_header_size;
    uint16_t section_header_count;
    uint16_t section_name_index;
} __attribute__((packed));

struct elf64_program_header {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t alignment;
} __attribute__((packed));

struct loaded_page {
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t final_flags;
};

static const uint8_t *archive;
static uint64_t archive_length;
static uint64_t archive_files;
static int init_available;
static int init_started;
static struct loaded_page loaded_pages[INIT_MAX_PAGES];
static uint64_t loaded_page_count;
static struct paging_space init_address_space;
static uint64_t init_stack_frame;

static uint64_t align_up4(uint64_t value) {
    return (value + 3U) & ~UINT64_C(3);
}

static uint64_t align_down_page(uint64_t value) {
    return value & ~(PAGING_PAGE_SIZE - 1U);
}

static uint64_t align_up_page(uint64_t value) {
    return (value + PAGING_PAGE_SIZE - 1U) & ~(PAGING_PAGE_SIZE - 1U);
}

static int text_equal(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return 0;
        }
        left++;
        right++;
    }
    return *left == *right;
}

static int cpio_name_equal(const uint8_t *name, uint64_t name_size, const char *expected) {
    uint64_t index = 0U;

    if (name_size == 0U) {
        return 0;
    }
    while (expected[index] != '\0' && index + 1U < name_size) {
        if (name[index] != (uint8_t)expected[index]) {
            return 0;
        }
        index++;
    }
    return expected[index] == '\0' && index + 1U == name_size && name[index] == '\0';
}

static int cpio_hex8(const uint8_t *text, uint64_t *value) {
    uint64_t result = 0U;

    for (uint64_t index = 0U; index < 8U; index++) {
        uint8_t digit;

        if (text[index] >= '0' && text[index] <= '9') {
            digit = (uint8_t)(text[index] - '0');
        } else if (text[index] >= 'a' && text[index] <= 'f') {
            digit = (uint8_t)(text[index] - 'a' + 10U);
        } else if (text[index] >= 'A' && text[index] <= 'F') {
            digit = (uint8_t)(text[index] - 'A' + 10U);
        } else {
            return 0;
        }
        result = (result << 4U) | digit;
    }
    *value = result;
    return 1;
}

static int cpio_find(const char *path, const uint8_t **data, uint64_t *size) {
    uint64_t offset = 0U;

    if (archive == (const uint8_t *)0 || data == (const uint8_t **)0 || size == (uint64_t *)0) {
        return 0;
    }
    while (offset <= archive_length && archive_length - offset >= CPIO_HEADER_SIZE) {
        const uint8_t *header = archive + offset;
        uint64_t file_size;
        uint64_t name_size;
        uint64_t name_offset;
        uint64_t data_offset;
        uint64_t next_offset;

        if (header[0] != '0' || header[1] != '7' || header[2] != '0' || header[3] != '7'
            || header[4] != '0' || header[5] != '1'
            || cpio_hex8(header + 54U, &file_size) == 0
            || cpio_hex8(header + 94U, &name_size) == 0) {
            return 0;
        }
        name_offset = offset + CPIO_HEADER_SIZE;
        if (name_size == 0U || name_offset > archive_length || name_size > archive_length - name_offset) {
            return 0;
        }
        data_offset = align_up4(name_offset + name_size);
        if (data_offset > archive_length || file_size > archive_length - data_offset) {
            return 0;
        }
        if (cpio_name_equal(archive + name_offset, name_size, "TRAILER!!!") != 0) {
            return 0;
        }
        if (cpio_name_equal(archive + name_offset, name_size, path) != 0) {
            *data = archive + data_offset;
            *size = file_size;
            return 1;
        }
        next_offset = align_up4(data_offset + file_size);
        if (next_offset <= offset || next_offset > archive_length) {
            return 0;
        }
        offset = next_offset;
    }
    return 0;
}

static void unload_pages(void) {
    for (uint64_t index = 0U; index < loaded_page_count; index++) {
        (void)paging_unmap_page(loaded_pages[index].virtual_address);
        (void)pmm_free_frame(loaded_pages[index].physical_address);
    }
    loaded_page_count = 0U;
    (void)paging_unmap_page(INIT_STACK_PAGE);
    (void)paging_unmap_page(INIT_STACK_GUARD);
    if (init_stack_frame != PMM_INVALID_ADDRESS) {
        (void)pmm_free_frame(init_stack_frame);
        init_stack_frame = PMM_INVALID_ADDRESS;
    }
}

static int load_page(uint64_t virtual_address, uint64_t flags) {
    uint64_t physical_address;
    uint8_t *page;

    if (loaded_page_count >= INIT_MAX_PAGES || paging_translate(virtual_address, &physical_address) != 0) {
        return 0;
    }
    physical_address = pmm_allocate_user_frame();
    if (physical_address == PMM_INVALID_ADDRESS
        || paging_map_page(virtual_address, physical_address, PAGING_FLAG_USER | PAGING_FLAG_WRITABLE) == 0) {
        if (physical_address != PMM_INVALID_ADDRESS) {
            (void)pmm_free_frame(physical_address);
        }
        return 0;
    }
    page = (uint8_t *)(uintptr_t)virtual_address;
    for (uint64_t index = 0U; index < PAGING_PAGE_SIZE; index++) {
        page[index] = 0U;
    }
    loaded_pages[loaded_page_count].virtual_address = virtual_address;
    loaded_pages[loaded_page_count].physical_address = physical_address;
    loaded_pages[loaded_page_count].final_flags = flags | PAGING_FLAG_USER;
    loaded_page_count++;
    return 1;
}

static int load_elf_init(const uint8_t *image, uint64_t image_size, uint64_t *entry) {
    const struct elf64_header *header;

    if (image == (const uint8_t *)0 || entry == (uint64_t *)0 || image_size < ELF64_HEADER_SIZE) {
        return 0;
    }
    header = (const struct elf64_header *)image;
    if (header->identification[0] != 0x7FU || header->identification[1] != 'E'
        || header->identification[2] != 'L' || header->identification[3] != 'F'
        || header->identification[4] != 2U || header->identification[5] != 1U
        || header->machine != ELF64_MACHINE_X86_64 || header->program_header_size != ELF64_PROGRAM_HEADER_SIZE
        || header->program_header_offset > image_size
        || header->program_header_count > (image_size - header->program_header_offset) / ELF64_PROGRAM_HEADER_SIZE
        || header->entry < PAGING_USER_SPACE_START || header->entry > PAGING_USER_SPACE_END) {
        return 0;
    }

    for (uint64_t index = 0U; index < header->program_header_count; index++) {
        const struct elf64_program_header *program =
            (const struct elf64_program_header *)(image + header->program_header_offset
                                                  + index * ELF64_PROGRAM_HEADER_SIZE);
        uint64_t page_flags;

        if (program->type != ELF64_PT_LOAD || program->memory_size == 0U) {
            continue;
        }
        if (program->memory_size < program->file_size || program->offset > image_size
            || program->file_size > image_size - program->offset
            || program->virtual_address < PAGING_USER_SPACE_START
            || program->virtual_address > PAGING_USER_SPACE_END
            || program->memory_size > PAGING_USER_SPACE_END - program->virtual_address + 1U) {
            unload_pages();
            return 0;
        }
        page_flags = (program->flags & ELF64_PF_WRITE) != 0U ? PAGING_FLAG_WRITABLE : 0U;
        for (uint64_t page = align_down_page(program->virtual_address);
             page < align_up_page(program->virtual_address + program->memory_size);
             page += PAGING_PAGE_SIZE) {
            if (load_page(page, page_flags) == 0) {
                unload_pages();
                return 0;
            }
        }
        for (uint64_t byte = 0U; byte < program->file_size; byte++) {
            ((uint8_t *)(uintptr_t)program->virtual_address)[byte] = image[program->offset + byte];
        }
    }
    for (uint64_t index = 0U; index < loaded_page_count; index++) {
        if (paging_map_page(loaded_pages[index].virtual_address, loaded_pages[index].physical_address,
                            loaded_pages[index].final_flags) == 0) {
            unload_pages();
            return 0;
        }
    }
    *entry = header->entry;
    return 1;
}

int initramfs_init(const struct limine_module_response *modules) {
    const struct limine_file *module = (const struct limine_file *)0;

    archive = (const uint8_t *)0;
    archive_length = 0U;
    archive_files = 0U;
    init_available = 0;
    init_started = 0;
    loaded_page_count = 0U;
    init_address_space.root_physical = 0U;
    init_address_space.mapping_count = 0U;
    init_stack_frame = PMM_INVALID_ADDRESS;
    if (modules == (const struct limine_module_response *)0) {
        return 0;
    }
    for (uint64_t index = 0U; index < modules->module_count; index++) {
        const struct limine_file *candidate = modules->modules[index];

        if (candidate != (const struct limine_file *)0 && candidate->address != (void *)0
            && candidate->size != 0U
            && (module == (const struct limine_file *)0
                || text_equal(candidate->string, "initramfs") != 0)) {
            module = candidate;
        }
    }
    if (module == (const struct limine_file *)0) {
        return 0;
    }
    archive = (const uint8_t *)module->address;
    archive_length = module->size;
    {
        struct vfs_file init_file;

        init_available = vfs_mount_newc(archive, archive_length) != 0
                         && vfs_open("init", &init_file) != 0;
    }
    archive_files = vfs_file_count();
    return init_available;
}

uint64_t initramfs_size(void) {
    return archive_length;
}

uint64_t initramfs_file_count(void) {
    return archive_files;
}

int initramfs_has_init(void) {
    return init_available;
}

int initramfs_start_init(void) {
    const uint8_t *image;
    uint64_t image_size;
    uint64_t entry;
    int task_id;

    if (init_available == 0 || init_started != 0 || cpio_find("init", &image, &image_size) == 0) {
        return 0;
    }
    arch_disable_interrupts();
    if (paging_space_create_user(&init_address_space) == 0
        || paging_space_activate(&init_address_space) == 0
        || load_elf_init(image, image_size, &entry) == 0) {
        goto cleanup;
    }
    init_stack_frame = pmm_allocate_user_frame();
    if (init_stack_frame == PMM_INVALID_ADDRESS
        || paging_space_map_guard(&init_address_space, INIT_STACK_GUARD) == 0
        || paging_map_page(INIT_STACK_PAGE, init_stack_frame, PAGING_FLAG_USER | PAGING_FLAG_WRITABLE) == 0) {
        goto cleanup;
    }
    task_id = scheduler_create_user_task("init", &init_address_space, entry, INIT_STACK_TOP);
    if (task_id < 0) {
        goto cleanup;
    }
    (void)paging_activate_kernel_space();
    init_started = 1;
    arch_enable_interrupts();
    return 1;

cleanup:
    unload_pages();
    (void)paging_activate_kernel_space();
    if (init_address_space.root_physical != 0U) {
        (void)paging_space_destroy_user(&init_address_space);
    }
    arch_enable_interrupts();
    return 0;
}

int initramfs_spawn(const char *path) {
    const uint8_t *image;
    uint64_t image_size;
    uint64_t entry;
    uint64_t stack_frame = PMM_INVALID_ADDRESS;
    struct paging_space address_space = { 0U, 0U };
    int task_id;

    if (path == (const char *)0 || init_available == 0 || cpio_find(path, &image, &image_size) == 0) {
        return -1;
    }
    arch_disable_interrupts();
    /* The running init task owns the previous pages; new pages are tracked only for this spawn attempt. */
    loaded_page_count = 0U;
    init_stack_frame = PMM_INVALID_ADDRESS;
    if (paging_space_create_user(&address_space) == 0
        || paging_space_activate(&address_space) == 0
        || load_elf_init(image, image_size, &entry) == 0) {
        goto cleanup;
    }
    stack_frame = pmm_allocate_user_frame();
    if (stack_frame == PMM_INVALID_ADDRESS
        || paging_space_map_guard(&address_space, INIT_STACK_GUARD) == 0
        || paging_map_page(INIT_STACK_PAGE, stack_frame, PAGING_FLAG_USER | PAGING_FLAG_WRITABLE) == 0) {
        goto cleanup;
    }
    task_id = scheduler_create_user_task(path, &address_space, entry, INIT_STACK_TOP);
    if (task_id < 0) {
        goto cleanup;
    }
    (void)paging_activate_kernel_space();
    arch_enable_interrupts();
    return task_id;

cleanup:
    for (uint64_t index = 0U; index < loaded_page_count; index++) {
        (void)paging_unmap_page(loaded_pages[index].virtual_address);
        (void)pmm_free_frame(loaded_pages[index].physical_address);
    }
    if (stack_frame != PMM_INVALID_ADDRESS) {
        (void)paging_unmap_page(INIT_STACK_PAGE);
        (void)pmm_free_frame(stack_frame);
    }
    (void)paging_activate_kernel_space();
    if (address_space.root_physical != 0U) {
        (void)paging_space_destroy_user(&address_space);
    }
    arch_enable_interrupts();
    return -1;
}
