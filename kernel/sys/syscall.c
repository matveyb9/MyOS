#include <stddef.h>
#include <stdint.h>

#include <acpi.h>
#include <arch.h>
#include <gdt.h>
#include <framebuffer.h>
#include <initramfs.h>
#include <keyboard.h>
#include <paging.h>
#include <pit.h>
#include <pmm.h>
#include <pipe.h>
#include <serial.h>
#include <scheduler.h>
#include <rtc.h>
#include <syscall.h>
#include <vfs.h>

#define IA32_EFER UINT32_C(0xC0000080)
#define IA32_STAR UINT32_C(0xC0000081)
#define IA32_LSTAR UINT32_C(0xC0000082)
#define IA32_FMASK UINT32_C(0xC0000084)
#define EFER_SYSCALL_ENABLE UINT64_C(0x0000000000000001)
#define SYSCALL_MASK_FLAGS UINT64_C(0x0000000000000700)
#define SYSCALL_WRITE_LIMIT UINT64_C(512)

extern void syscall_entry(void);

static volatile uint64_t total_syscalls;
static volatile uint64_t write_syscalls;
static uint64_t gui_owner_task_id;

static int user_buffer_is_valid(uint64_t address, uint64_t length, int writable) {
    if (length == 0U || length > SYSCALL_WRITE_LIMIT) {
        return 0;
    }
    return paging_user_range_is_mapped(address, length, writable);
}

static int copy_from_user(void *destination, uint64_t source_address, uint64_t length) {
    uint8_t *destination_bytes = (uint8_t *)destination;
    const uint8_t *source_bytes = (const uint8_t *)(uintptr_t)source_address;

    if (destination == (void *)0 || user_buffer_is_valid(source_address, length, 0) == 0) {
        return 0;
    }
    for (uint64_t index = 0U; index < length; index++) {
        destination_bytes[index] = source_bytes[index];
    }
    return 1;
}

static int copy_gui_content_from_user(struct myos_gui_content_request *destination, uint64_t source_address) {
    const uint8_t *source = (const uint8_t *)(uintptr_t)source_address;

    if (destination == (struct myos_gui_content_request *)0
        || paging_user_range_is_mapped(source_address, sizeof(*destination), 0) == 0) {
        return 0;
    }
    for (uint64_t index = 0U; index < sizeof(*destination); index++) {
        ((uint8_t *)destination)[index] = source[index];
    }
    return 1;
}

static int copy_to_user(uint64_t destination_address, const void *source, uint64_t length) {
    uint8_t *destination_bytes = (uint8_t *)(uintptr_t)destination_address;
    const uint8_t *source_bytes = (const uint8_t *)source;

    if (source == (const void *)0 || user_buffer_is_valid(destination_address, length, 1) == 0) {
        return 0;
    }
    for (uint64_t index = 0U; index < length; index++) {
        destination_bytes[index] = source_bytes[index];
    }
    return 1;
}

static int request_string_is_terminated(const char *text, uint64_t capacity, int require_nonempty) {
    if (text == (const char *)0 || capacity == 0U) {
        return 0;
    }
    for (uint64_t index = 0U; index < capacity; index++) {
        if (text[index] == '\0') {
            return require_nonempty == 0 || index != 0U;
        }
    }
    return 0;
}

void syscall_init(void) {
    const uint64_t star = ((uint64_t)GDT_KERNEL_CODE_SELECTOR << 32U)
                          | ((uint64_t)GDT_USER_COMPAT_CODE_SELECTOR << 48U);

    total_syscalls = 0U;
    write_syscalls = 0U;
    gui_owner_task_id = UINT64_MAX;
    arch_write_msr(IA32_EFER, arch_read_msr(IA32_EFER) | EFER_SYSCALL_ENABLE);
    arch_write_msr(IA32_STAR, star);
    arch_write_msr(IA32_LSTAR, (uint64_t)(uintptr_t)syscall_entry);
    arch_write_msr(IA32_FMASK, SYSCALL_MASK_FLAGS);
}

uint64_t syscall_dispatch(uint64_t number, uint64_t descriptor, uint64_t buffer, uint64_t length,
                          uint64_t *user_context) {
    total_syscalls++;
    if (number == MYOS_SYS_WRITE) {
        char text[SYSCALL_WRITE_LIMIT];
        uint64_t result;

        if ((descriptor != 1U && descriptor != 2U) || copy_from_user(text, buffer, length) == 0) {
            return UINT64_MAX;
        }
        if (descriptor == 2U) {
            result = pipe_write(scheduler_current_task_id(), (const uint8_t *)text, length);
            return result;
        }
        for (uint64_t index = 0U; index < length; index++) {
            serial_write_char(text[index]);
        }
        write_syscalls++;
        return length;
    }
    if (number == MYOS_SYS_READ) {
        char data[SYSCALL_WRITE_LIMIT];
        char character;
        uint64_t *next_context;
        uint64_t result;

        if ((descriptor != 0U && descriptor != 1U) || length == 0U || length > SYSCALL_WRITE_LIMIT
            || user_buffer_is_valid(buffer, descriptor == 1U ? length : 1U, 1) == 0) {
            return UINT64_MAX;
        }
        if (descriptor == 1U) {
            result = pipe_read(scheduler_current_task_id(), (uint8_t *)data, length);
            if (result == PIPE_INVALID_ID || (result != 0U && copy_to_user(buffer, data, result) == 0)) {
                return UINT64_MAX;
            }
            return result;
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
        if (copy_to_user(buffer, &character, 1U) == 0) {
            return UINT64_MAX;
        }
        return 1U;
    }
    if (number == MYOS_SYS_GUI_SESSION) {
        const uint64_t current_task_id = scheduler_current_task_id();

        if (descriptor == MYOS_GUI_BEGIN) {
            if (buffer != 0U || length != 0U || gui_owner_task_id != UINT64_MAX
                || framebuffer_gui_begin() == 0) {
                return UINT64_MAX;
            }
            gui_owner_task_id = current_task_id;
            return 0U;
        }
        if (gui_owner_task_id != current_task_id || framebuffer_gui_active() == 0) {
            return UINT64_MAX;
        }
        if (descriptor == MYOS_GUI_INPUT) {
            if (length != 0U || buffer > UINT64_C(127)) {
                return UINT64_MAX;
            }
            return (uint64_t)(uint8_t)framebuffer_gui_handle_input((char)buffer);
        }
        if (descriptor == MYOS_GUI_SET_CONTENT) {
            struct myos_gui_content_request request;

            if (buffer == 0U || length != sizeof(request) || copy_gui_content_from_user(&request, buffer) == 0
                || request.length > MYOS_GUI_CONTENT_MAX
                || (request.flags & ~(MYOS_GUI_CONTENT_FLAG_EDITABLE | MYOS_GUI_CONTENT_FLAG_LAUNCHER
                                      | MYOS_GUI_CONTENT_FLAG_BROWSER)) != 0U
                || ((request.flags & MYOS_GUI_CONTENT_FLAG_LAUNCHER) != 0U
                    && (request.flags & (MYOS_GUI_CONTENT_FLAG_EDITABLE | MYOS_GUI_CONTENT_FLAG_BROWSER)) != 0U)
                || ((request.flags & MYOS_GUI_CONTENT_FLAG_EDITABLE) != 0U
                    && (request.flags & MYOS_GUI_CONTENT_FLAG_BROWSER) != 0U)
                || request.cursor > request.length
                || request.viewport > request.length
                || request_string_is_terminated(request.title, MYOS_GUI_CONTENT_TITLE_MAX, 1) == 0
                || framebuffer_gui_set_content(request.title, request.data, request.length, request.flags,
                                               request.cursor, request.viewport) == 0) {
                return UINT64_MAX;
            }
            return 0U;
        }
        if (descriptor == MYOS_GUI_END && buffer == 0U && length == 0U) {
            framebuffer_gui_end();
            gui_owner_task_id = UINT64_MAX;
            return 0U;
        }
        return UINT64_MAX;
    }
    if (number == MYOS_SYS_TICKS) {
        return pit_ticks();
    }
    if (number == MYOS_SYS_FREE_FRAMES) {
        return pmm_free_frame_count();
    }
    if (number == MYOS_SYS_SPAWN) {
        struct myos_spawn_request request;
        int task_id;

        if (descriptor != 0U || length != sizeof(request)
            || copy_from_user(&request, buffer, sizeof(request)) == 0
            || request_string_is_terminated(request.path, MYOS_SPAWN_PATH_MAX, 1) == 0
            || request_string_is_terminated(request.arguments, MYOS_SPAWN_ARGUMENTS_MAX, 0) == 0) {
            return UINT64_MAX;
        }
        task_id = initramfs_spawn(request.path, request.arguments, request.input_pipe_id,
                                  request.output_pipe_id, scheduler_current_task_id());
        (void)scheduler_activate_current_task();
        return task_id < 0 ? UINT64_MAX : (uint64_t)task_id;
    }
    if (number == MYOS_SYS_PIPE_CREATE) {
        if (descriptor != 0U || buffer != 0U || length != 0U) {
            return UINT64_MAX;
        }
        return pipe_create(scheduler_current_task_id());
    }
    if (number == MYOS_SYS_PIPE_ATTACH_READER || number == MYOS_SYS_PIPE_ATTACH_WRITER) {
        int attached;

        if (length != 0U) {
            return UINT64_MAX;
        }
        attached = number == MYOS_SYS_PIPE_ATTACH_READER
                       ? pipe_attach_reader(scheduler_current_task_id(), descriptor, buffer)
                       : pipe_attach_writer(scheduler_current_task_id(), descriptor, buffer);
        return attached != 0 ? 0U : UINT64_MAX;
    }
    if (number == MYOS_SYS_PIPE_SEAL) {
        if (buffer != 0U || length != 0U) {
            return UINT64_MAX;
        }
        return pipe_seal(scheduler_current_task_id(), descriptor) != 0 ? 0U : UINT64_MAX;
    }
    if (number == MYOS_SYS_KILL) {
        if (buffer != 0U || length != 0U) {
            return UINT64_MAX;
        }
        if (descriptor == gui_owner_task_id) {
            framebuffer_gui_end();
            gui_owner_task_id = UINT64_MAX;
        }
        return scheduler_kill_child(descriptor, MYOS_EXIT_STATUS_KILLED) == 0 ? 0U : UINT64_MAX;
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

        if (length != sizeof(info) || scheduler_task_info(descriptor, &info) != 0
            || copy_to_user(buffer, &info, sizeof(info)) == 0) {
            return UINT64_MAX;
        }
        return 0U;
    }
    if (number == MYOS_SYS_UPTIME) {
        if (descriptor != 0U || buffer != 0U || length != 0U) {
            return UINT64_MAX;
        }
        return pit_ticks();
    }
    if (number == MYOS_SYS_RTC_TIME) {
        struct rtc_time time;
        struct myos_rtc_time result;

        if (descriptor != 0U || length != sizeof(result) || rtc_read_time(&time) == 0) {
            return UINT64_MAX;
        }
        result.year = time.year;
        result.month = time.month;
        result.day = time.day;
        result.hour = time.hour;
        result.minute = time.minute;
        result.second = time.second;
        return copy_to_user(buffer, &result, sizeof(result)) != 0 ? 0U : UINT64_MAX;
    }
    if (number == MYOS_SYS_VFS_ENTRY) {
        struct myos_vfs_entry entry;

        if (buffer == 0U || length != sizeof(entry)
            || vfs_get_entry(descriptor, entry.name, sizeof(entry.name), &entry.size) == 0
            || copy_to_user(buffer, &entry, sizeof(entry)) == 0) {
            return UINT64_MAX;
        }
        return 0U;
    }
    if (number == MYOS_SYS_VFS_READ) {
        struct myos_vfs_read_request request;
        uint64_t read_length;
        uint64_t data_address;

        if (descriptor != 0U || buffer == 0U || length != sizeof(request)
            || copy_from_user(&request, buffer, sizeof(request)) == 0
            || request_string_is_terminated(request.path, MYOS_VFS_PATH_MAX, 1) == 0
            || vfs_read(request.path, request.offset, request.data, MYOS_VFS_READ_CHUNK, &read_length) == 0) {
            return UINT64_MAX;
        }
        data_address = buffer + offsetof(struct myos_vfs_read_request, data);
        return read_length == 0U || copy_to_user(data_address, request.data, read_length) != 0 ? read_length : UINT64_MAX;
    }
    if (number == MYOS_SYS_VFS_LIST) {
        struct myos_vfs_list_request request;
        struct vfs_directory_entry entry;
        uint64_t entry_address;

        if (descriptor != 0U || buffer == 0U || length != sizeof(request)
            || copy_from_user(&request, buffer, sizeof(request)) == 0
            || request_string_is_terminated(request.path, MYOS_VFS_PATH_MAX, 1) == 0
            || vfs_list(request.path, request.index, &entry) == 0) {
            return UINT64_MAX;
        }
        for (uint64_t index = 0U; index < MYOS_VFS_NAME_MAX; index++) { request.entry.name[index] = entry.name[index]; }
        request.entry.size = entry.size;
        request.entry.type = entry.type;
        entry_address = buffer + offsetof(struct myos_vfs_list_request, entry);
        return copy_to_user(entry_address, &request.entry, sizeof(request.entry)) != 0 ? 0U : UINT64_MAX;
    }
    if (number == MYOS_SYS_VFS_CREATE_FILE || number == MYOS_SYS_VFS_CREATE_DIRECTORY
        || number == MYOS_SYS_VFS_REMOVE) {
        struct myos_vfs_path_request request;
        int result;

        if (descriptor != 0U || buffer == 0U || length != sizeof(request)
            || copy_from_user(&request, buffer, sizeof(request)) == 0
            || request_string_is_terminated(request.path, MYOS_VFS_PATH_MAX, 1) == 0) {
            return UINT64_MAX;
        }
        if (number == MYOS_SYS_VFS_CREATE_FILE) {
            result = vfs_create_file(request.path);
        } else if (number == MYOS_SYS_VFS_CREATE_DIRECTORY) {
            result = vfs_create_directory(request.path);
        } else {
            result = vfs_remove_object(request.path);
        }
        return result != 0 ? 0U : UINT64_MAX;
    }
    if (number == MYOS_SYS_VFS_RENAME) {
        struct myos_vfs_rename_request request;

        if (descriptor != 0U || buffer == 0U || length != sizeof(request)
            || copy_from_user(&request, buffer, sizeof(request)) == 0
            || request_string_is_terminated(request.source, MYOS_VFS_PATH_MAX, 1) == 0
            || request_string_is_terminated(request.target, MYOS_VFS_PATH_MAX, 1) == 0
            || vfs_rename_object(request.source, request.target) == 0) {
            return UINT64_MAX;
        }
        return 0U;
    }
    if (number == MYOS_SYS_VFS_MOVE) {
        struct myos_vfs_rename_request request;

        if (descriptor != 0U || buffer == 0U || length != sizeof(request)
            || copy_from_user(&request, buffer, sizeof(request)) == 0
            || request_string_is_terminated(request.source, MYOS_VFS_PATH_MAX, 1) == 0
            || request_string_is_terminated(request.target, MYOS_VFS_PATH_MAX, 1) == 0
            || vfs_move_object(request.source, request.target) == 0) {
            return UINT64_MAX;
        }
        return 0U;
    }
    if (number == MYOS_SYS_VFS_WRITE) {
        struct myos_vfs_write_request request;

        if (descriptor != 0U || buffer == 0U || length != sizeof(request)
            || copy_from_user(&request, buffer, sizeof(request)) == 0
            || request_string_is_terminated(request.path, MYOS_VFS_PATH_MAX, 1) == 0
            || request.length > MYOS_VFS_READ_CHUNK
            || vfs_write_file(request.path, request.offset, request.data, request.length) == 0) {
            return UINT64_MAX;
        }
        return request.length;
    }
    if (number == MYOS_SYS_TMPFS_CREATE || number == MYOS_SYS_TMPFS_REMOVE
        || number == MYOS_SYS_PERSIST_CREATE || number == MYOS_SYS_PERSIST_REMOVE) {
        struct myos_tmpfs_path_request request;
        int result;

        if (descriptor != 0U || length != sizeof(request)
            || copy_from_user(&request, buffer, sizeof(request)) == 0
            || request_string_is_terminated(request.path, MYOS_VFS_NAME_MAX, 1) == 0) {
            return UINT64_MAX;
        }
        if (number == MYOS_SYS_TMPFS_CREATE) {
            result = vfs_tmpfs_create(request.path);
        } else if (number == MYOS_SYS_TMPFS_REMOVE) {
            result = vfs_tmpfs_remove(request.path);
        } else if (number == MYOS_SYS_PERSIST_CREATE) {
            result = vfs_persistent_create(request.path);
        } else {
            result = vfs_persistent_remove(request.path);
        }
        return result != 0 ? 0U : UINT64_MAX;
    }
    if (number == MYOS_SYS_TMPFS_WRITE || number == MYOS_SYS_PERSIST_WRITE) {
        struct myos_tmpfs_write_request request;

        if (descriptor != 0U || length != sizeof(request)
            || copy_from_user(&request, buffer, sizeof(request)) == 0
            || request_string_is_terminated(request.path, MYOS_VFS_NAME_MAX, 1) == 0
            || request.length > MYOS_TMPFS_WRITE_CHUNK) {
            return UINT64_MAX;
        }
        if (number == MYOS_SYS_TMPFS_WRITE) {
            if (vfs_tmpfs_write(request.path, request.offset, request.data, request.length) == 0) {
                return UINT64_MAX;
            }
        } else if (request.length > MYOS_PERSIST_WRITE_CHUNK
                   || vfs_persistent_write(request.path, request.offset, request.data, request.length) == 0) {
            return UINT64_MAX;
        }
        return request.length;
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
    if (number == MYOS_SYS_POWEROFF) {
        if (descriptor != 0U || buffer != 0U || length != 0U) {
            return UINT64_MAX;
        }
        serial_write("[system] poweroff requested by user task.\n");
        acpi_poweroff();
    }
    if (number == MYOS_SYS_REBOOT) {
        if (descriptor != 0U || buffer != 0U || length != 0U) {
            return UINT64_MAX;
        }
        serial_write("[system] reboot requested by user task.\n");
        arch_reboot();
    }
    if (number == MYOS_SYS_EXIT) {
        uint64_t *next_context;

        if (scheduler_current_task_id() == gui_owner_task_id) {
            framebuffer_gui_end();
            gui_owner_task_id = UINT64_MAX;
        }
        next_context = scheduler_exit_current(descriptor);

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
