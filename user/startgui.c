#include <stdint.h>

#include <syscall.h>

#define GUI_NOTE_PATH "disk/note"

static uint64_t system_call(uint64_t number, uint64_t argument1, uint64_t argument2, uint64_t argument3) {
    register uint64_t rax __asm__("rax") = number;
    register uint64_t rdi __asm__("rdi") = argument1;
    register uint64_t rsi __asm__("rsi") = argument2;
    register uint64_t rdx __asm__("rdx") = argument3;

    __asm__ volatile ("syscall"
                      : "+a"(rax)
                      : "D"(rdi), "S"(rsi), "d"(rdx)
                      : "rcx", "r11", "memory");
    return rax;
}

static void copy_title(char *destination, const char *source) {
    uint64_t index = 0U;

    while (index + 1U < MYOS_GUI_CONTENT_TITLE_MAX && source[index] != '\0') {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
    while (++index < MYOS_GUI_CONTENT_TITLE_MAX) {
        destination[index] = '\0';
    }
}

static int make_path(char *destination, const char *source) {
    uint64_t index = 0U;

    while (index + 1U < MYOS_VFS_NAME_MAX && source[index] != '\0') {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
    return index != 0U && source[index] == '\0';
}

static int set_viewer_content(const char *title, const uint8_t *data, uint64_t length) {
    struct myos_gui_content_request content = { 0U, { 0 }, { 0 } };

    copy_title(content.title, title);
    for (uint64_t index = 0U; index < length; index++) {
        content.data[index] = data[index];
    }
    content.length = length;
    return system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_SET_CONTENT, (uint64_t)(uintptr_t)&content,
                       sizeof(content)) != UINT64_MAX;
}

static void set_viewer_status(const char *message) {
    uint8_t data[MYOS_GUI_CONTENT_MAX] = { 0 };
    uint64_t length = 0U;

    while (length < MYOS_GUI_CONTENT_MAX && message[length] != '\0') {
        data[length] = (uint8_t)message[length];
        length++;
    }
    (void)set_viewer_content("VIEWER", data, length);
}

static uint64_t read_viewer_file(const char *path, uint8_t *data) {
    struct myos_vfs_read_request request = { 0U, { 0 }, { 0 } };
    uint64_t count;

    if (make_path(request.path, path) == 0) {
        return UINT64_MAX;
    }
    count = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));
    if (count == UINT64_MAX) {
        return UINT64_MAX;
    }
    for (uint64_t index = 0U; index < count; index++) {
        data[index] = request.data[index];
    }
    return count;
}

static void load_viewer_file(const char *path) {
    uint8_t data[MYOS_GUI_CONTENT_MAX] = { 0 };
    const uint64_t count = read_viewer_file(path, data);

    if (count == UINT64_MAX) {
        set_viewer_status("UNABLE TO READ FILE");
        return;
    }
    if (set_viewer_content("FILE VIEWER", data, count) == 0) {
        set_viewer_status("VIEWER UPDATE FAILED");
    }
}

static int save_disk_note(const uint8_t *data, uint64_t length) {
    struct myos_tmpfs_path_request path_request = { { 0 } };
    struct myos_tmpfs_write_request write_request = { 0U, 0U, { 0 }, { 0 } };

    if (make_path(path_request.path, GUI_NOTE_PATH) == 0 || make_path(write_request.path, GUI_NOTE_PATH) == 0) {
        return 0;
    }
    for (uint64_t index = 0U; index < length; index++) {
        write_request.data[index] = data[index];
    }
    write_request.length = length;
    (void)system_call(MYOS_SYS_PERSIST_REMOVE, 0U, (uint64_t)(uintptr_t)&path_request, sizeof(path_request));
    if (system_call(MYOS_SYS_PERSIST_CREATE, 0U, (uint64_t)(uintptr_t)&path_request, sizeof(path_request))
            == UINT64_MAX
        || system_call(MYOS_SYS_PERSIST_WRITE, 0U, (uint64_t)(uintptr_t)&write_request, sizeof(write_request))
            == UINT64_MAX) {
        return 0;
    }
    return 1;
}

static void edit_disk_note(void) {
    uint8_t data[MYOS_GUI_CONTENT_MAX] = { 0 };
    uint64_t length = read_viewer_file(GUI_NOTE_PATH, data);

    if (length == UINT64_MAX) {
        length = 0U;
    }
    for (;;) {
        char character;
        uint64_t read_result;

        if (set_viewer_content("EDIT NOTE", data, length) == 0) {
            return;
        }
        read_result = system_call(MYOS_SYS_READ, 0U, (uint64_t)(uintptr_t)&character, 1U);
        if (read_result == UINT64_MAX || read_result == 0U) {
            continue;
        }
        if (character == '\x1b') {
            load_viewer_file(GUI_NOTE_PATH);
            return;
        }
        if ((uint8_t)character == UINT8_C(0x13)) {
            if (save_disk_note(data, length) != 0) {
                load_viewer_file(GUI_NOTE_PATH);
            } else {
                set_viewer_status("SAVE FAILED");
            }
            return;
        }
        if (character == '\b' || (uint8_t)character == UINT8_C(0x7F)) {
            if (length != 0U) {
                length--;
            }
        } else if (character == '\r' || character == '\n') {
            if (length < MYOS_GUI_CONTENT_MAX) {
                data[length++] = (uint8_t)'\n';
            }
        } else if ((uint8_t)character >= 32U && (uint8_t)character <= 126U && length < MYOS_GUI_CONTENT_MAX) {
            data[length++] = (uint8_t)character;
        }
    }
}

void _start(uint64_t argc, const char *arguments) {
    uint64_t status = 0U;
    const char *initial_path = arguments[0] == '\0' ? "motd.txt" : arguments;

    (void)argc;
    if (system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_BEGIN, 0U, 0U) == UINT64_MAX) {
        status = 1U;
    } else {
        load_viewer_file(initial_path);
        for (;;) {
            char character;
            const uint64_t read_result = system_call(MYOS_SYS_READ, 0U, (uint64_t)(uintptr_t)&character, 1U);

            if (read_result == UINT64_MAX || read_result == 0U) {
                continue;
            }
            if (character == '\x1b' || character == 'q' || character == 'Q') {
                (void)system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_END, 0U, 0U);
                break;
            }
            if (character == 'e' || character == 'E') {
                edit_disk_note();
            } else if (character == 'm' || character == 'M') {
                load_viewer_file("motd.txt");
            } else if (character == 'D') {
                load_viewer_file(GUI_NOTE_PATH);
            } else {
                (void)system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_INPUT, (uint64_t)(uint8_t)character, 0U);
            }
        }
    }
    (void)system_call(MYOS_SYS_EXIT, status, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
