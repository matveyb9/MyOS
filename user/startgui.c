#include <stdint.h>

#include <syscall.h>

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

static void set_viewer_status(const char *message) {
    struct myos_gui_content_request content = { 0U, { 0 }, { 0 } };
    uint64_t index = 0U;

    copy_title(content.title, "VIEWER");
    while (index < MYOS_GUI_CONTENT_MAX && message[index] != '\0') {
        content.data[index] = (uint8_t)message[index];
        index++;
    }
    content.length = index;
    (void)system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_SET_CONTENT, (uint64_t)(uintptr_t)&content,
                      sizeof(content));
}

static void load_viewer_file(const char *path) {
    struct myos_vfs_read_request read_request = { 0U, { 0 }, { 0 } };
    struct myos_gui_content_request content = { 0U, { 0 }, { 0 } };
    uint64_t count;

    if (make_path(read_request.path, path) == 0) {
        set_viewer_status("INVALID FILE PATH");
        return;
    }
    count = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&read_request, sizeof(read_request));
    if (count == UINT64_MAX) {
        set_viewer_status("UNABLE TO READ FILE");
        return;
    }
    copy_title(content.title, "FILE VIEWER");
    for (uint64_t index = 0U; index < count; index++) {
        content.data[index] = read_request.data[index];
    }
    content.length = count;
    if (system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_SET_CONTENT, (uint64_t)(uintptr_t)&content,
                    sizeof(content)) == UINT64_MAX) {
        set_viewer_status("VIEWER UPDATE FAILED");
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
            if (character == 'm' || character == 'M') {
                load_viewer_file("motd.txt");
            } else if (character == 'D') {
                load_viewer_file("disk/note");
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
