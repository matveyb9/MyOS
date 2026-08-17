#include <stdint.h>

#include <arch.h>
#include <gdt.h>
#include <initramfs.h>
#include <keyboard.h>
#include <paging.h>
#include <pit.h>
#include <pmm.h>
#include <serial.h>
#include <scheduler.h>
#include <syscall.h>
#include <vfs.h>

#define IA32_EFER UINT32_C(0xC0000080)
#define IA32_STAR UINT32_C(0xC0000081)
#define IA32_LSTAR UINT32_C(0xC0000082)
#define IA32_FMASK UINT32_C(0xC0000084)
#define EFER_SYSCALL_ENABLE UINT64_C(0x0000000000000001)
#define SYSCALL_MASK_FLAGS UINT64_C(0x0000000000000700)
#define SYSCALL_WRITE_LIMIT UINT64_C(256)

extern void syscall_entry(void);

static volatile uint64_t total_syscalls;
static volatile uint64_t write_syscalls;

static int user_buffer_is_valid(uint64_t address, uint64_t length) {
    if (address < PAGING_USER_SPACE_START || address > PAGING_USER_SPACE_END || length > SYSCALL_WRITE_LIMIT) {
        return 0;
    }
    return length <= PAGING_USER_SPACE_END - address + 1U;
}

void syscall_init(void) {
    const uint64_t star = ((uint64_t)GDT_KERNEL_CODE_SELECTOR << 32U)
                          | ((uint64_t)GDT_USER_COMPAT_CODE_SELECTOR << 48U);

    total_syscalls = 0U;
    write_syscalls = 0U;
    arch_write_msr(IA32_EFER, arch_read_msr(IA32_EFER) | EFER_SYSCALL_ENABLE);
    arch_write_msr(IA32_STAR, star);
    arch_write_msr(IA32_LSTAR, (uint64_t)(uintptr_t)syscall_entry);
    arch_write_msr(IA32_FMASK, SYSCALL_MASK_FLAGS);
}

uint64_t syscall_dispatch(uint64_t number, uint64_t descriptor, uint64_t buffer, uint64_t length,
                          uint64_t *user_context) {
    total_syscalls++;
    if (number == MYOS_SYS_WRITE) {
        if (descriptor != 1U || user_buffer_is_valid(buffer, length) == 0) {
            return UINT64_MAX;
        }
        for (uint64_t index = 0U; index < length; index++) {
            serial_write_char(((const char *)(uintptr_t)buffer)[index]);
        }
        write_syscalls++;
        return length;
    }
    if (number == MYOS_SYS_READ) {
        char character;
        uint64_t *next_context;

        if (descriptor != 0U || length == 0U || user_buffer_is_valid(buffer, 1U) == 0) {
            return UINT64_MAX;
        }
        if (serial_input_available() != 0) {
            character = serial_read_char();
        } else if (keyboard_has_char() != 0) {
            character = keyboard_read_char();
        } else {
            next_context = scheduler_wait_console_input(user_context);
            if (next_context == (uint64_t *)0) {
                return UINT64_MAX;
            }
            user_context[14] = 0U;
            arch_resume_context(next_context);
        }
        ((char *)(uintptr_t)buffer)[0] = character;
        return 1U;
    }
    if (number == MYOS_SYS_TICKS) {
        return pit_ticks();
    }
    if (number == MYOS_SYS_FREE_FRAMES) {
        return pmm_free_frame_count();
    }
    if (number == MYOS_SYS_SPAWN) {
        char path[16];
        int task_id;

        if (descriptor != 0U || length == 0U || length >= sizeof(path)
            || user_buffer_is_valid(buffer, length) == 0) {
            return UINT64_MAX;
        }
        for (uint64_t index = 0U; index < length; index++) {
            path[index] = ((const char *)(uintptr_t)buffer)[index];
        }
        path[length] = '\0';
        task_id = initramfs_spawn(path);
        (void)scheduler_activate_current_task();
        return task_id < 0 ? UINT64_MAX : (uint64_t)task_id;
    }
    if (number == MYOS_SYS_WAIT) {
        uint64_t status;
        uint64_t *next_context;

        if (buffer != 0U || length != 0U) {
            return UINT64_MAX;
        }
        if (scheduler_wait_child(descriptor, &status) == 0) {
            return status;
        }
        next_context = scheduler_wait_current(descriptor, user_context);
        if (next_context == (uint64_t *)0) {
            return UINT64_MAX;
        }
        arch_resume_context(next_context);
    }
    if (number == MYOS_SYS_GETPID) {
        if (descriptor != 0U || buffer != 0U || length != 0U) {
            return UINT64_MAX;
        }
        return scheduler_current_task_id();
    }
    if (number == MYOS_SYS_TASK_INFO) {
        struct myos_task_info info;

        if (length != sizeof(info) || user_buffer_is_valid(buffer, sizeof(info)) == 0
            || scheduler_task_info(descriptor, &info) != 0) {
            return UINT64_MAX;
        }
        *((struct myos_task_info *)(uintptr_t)buffer) = info;
        return 0U;
    }
    if (number == MYOS_SYS_VFS_ENTRY) {
        struct myos_vfs_entry entry;

        if (buffer == 0U || length != sizeof(entry) || user_buffer_is_valid(buffer, sizeof(entry)) == 0
            || vfs_get_entry(descriptor, entry.name, sizeof(entry.name), &entry.size) == 0) {
            return UINT64_MAX;
        }
        *((struct myos_vfs_entry *)(uintptr_t)buffer) = entry;
        return 0U;
    }
    if (number == MYOS_SYS_VFS_READ) {
        struct myos_vfs_read_request *request;
        struct vfs_file file;
        uint64_t name_length = 0U;
        uint64_t remaining;
        uint64_t copy_length;

        if (descriptor != 0U || buffer == 0U || length != sizeof(*request)
            || user_buffer_is_valid(buffer, sizeof(*request)) == 0) {
            return UINT64_MAX;
        }
        request = (struct myos_vfs_read_request *)(uintptr_t)buffer;
        while (name_length < MYOS_VFS_NAME_MAX && request->path[name_length] != '\0') {
            name_length++;
        }
        if (name_length == 0U || name_length == MYOS_VFS_NAME_MAX
            || vfs_open(request->path, &file) == 0) {
            return UINT64_MAX;
        }
        if (request->offset >= file.size) {
            return 0U;
        }
        remaining = file.size - request->offset;
        copy_length = remaining < MYOS_VFS_READ_CHUNK ? remaining : MYOS_VFS_READ_CHUNK;
        for (uint64_t index = 0U; index < copy_length; index++) {
            request->data[index] = file.data[request->offset + index];
        }
        return copy_length;
    }
    if (number == MYOS_SYS_SLEEP) {
        uint64_t *next_context;

        if (buffer != 0U || length != 0U) {
            return UINT64_MAX;
        }
        if (descriptor == 0U) {
            return 0U;
        }
        next_context = scheduler_sleep_current(descriptor, user_context);
        if (next_context == (uint64_t *)0) {
            return UINT64_MAX;
        }
        user_context[14] = 0U;
        arch_resume_context(next_context);
    }
    if (number == MYOS_SYS_EXIT) {
        uint64_t *next_context = scheduler_exit_current(descriptor);

        if (next_context == (uint64_t *)0) {
            serial_write("[user] exit failed: no runnable replacement task.\n");
            arch_halt();
        }
        arch_resume_context(next_context);
    }
    return UINT64_MAX;
}

uint64_t syscall_count(void) {
    return total_syscalls;
}

uint64_t syscall_write_count(void) {
    return write_syscalls;
}
