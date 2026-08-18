#include <stdint.h>

#include <syscall.h>

#define GUI_NOTE_PATH "disk/note"
#define GUI_DISK_PATH_CAPACITY UINT64_C(40)
#define GUI_VFS_ENTRY_SCAN_LIMIT UINT64_C(64)

static char selected_disk_path[GUI_DISK_PATH_CAPACITY] = GUI_NOTE_PATH;

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

static int text_equal(const char *left, const char *right) {
    uint64_t index = 0U;

    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) {
            return 0;
        }
        index++;
    }
    return left[index] == right[index];
}

static int disk_path_is_valid(const char *path) {
    uint64_t index;

    if (path == (const char *)0 || path[0] != 'd' || path[1] != 'i' || path[2] != 's' || path[3] != 'k'
        || path[4] != '/') {
        return 0;
    }
    for (index = 5U; index < GUI_DISK_PATH_CAPACITY; index++) {
        const char character = path[index];

        if (character == '\0') {
            return index > 5U;
        }
        if (character == '/' || character < ' ' || character > '~') {
            return 0;
        }
    }
    return 0;
}

static int select_disk_path(const char *path) {
    if (disk_path_is_valid(path) == 0) {
        return 0;
    }
    for (uint64_t index = 0U; index < GUI_DISK_PATH_CAPACITY; index++) {
        selected_disk_path[index] = path[index];
        if (path[index] == '\0') {
            return 1;
        }
    }
    return 0;
}

static void selected_disk_title(char *title) {
    uint64_t destination = 0U;
    uint64_t source = 5U;
    static const char prefix[] = "DISK:";

    while (prefix[destination] != '\0' && destination + 1U < MYOS_GUI_CONTENT_TITLE_MAX) {
        title[destination] = prefix[destination];
        destination++;
    }
    while (selected_disk_path[source] != '\0' && destination + 1U < MYOS_GUI_CONTENT_TITLE_MAX) {
        title[destination++] = selected_disk_path[source++];
    }
    title[destination] = '\0';
}

static int set_viewer_content(const char *title, const uint8_t *data, uint64_t length, uint64_t flags,
                              uint64_t cursor, uint64_t viewport) {
    struct myos_gui_content_request content = { 0U, 0U, 0U, 0U, { 0 }, { 0 } };

    copy_title(content.title, title);
    for (uint64_t index = 0U; index < length; index++) {
        content.data[index] = data[index];
    }
    content.length = length;
    content.flags = flags;
    content.cursor = cursor;
    content.viewport = viewport;
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
    (void)set_viewer_content("VIEWER", data, length, 0U, 0U, 0U);
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

static int read_vfs_entry(uint64_t index, struct myos_vfs_entry *entry) {
    return system_call(MYOS_SYS_VFS_ENTRY, index, (uint64_t)(uintptr_t)entry, sizeof(*entry)) != UINT64_MAX;
}

static int select_next_disk_file(void) {
    char first_path[GUI_DISK_PATH_CAPACITY] = { 0 };
    int have_first = 0;
    int select_following = 0;

    for (uint64_t index = 0U; index < GUI_VFS_ENTRY_SCAN_LIMIT; index++) {
        struct myos_vfs_entry entry = { 0U, { 0 } };

        if (read_vfs_entry(index, &entry) == 0) {
            break;
        }
        if (disk_path_is_valid(entry.name) == 0) {
            continue;
        }
        if (have_first == 0) {
            for (uint64_t character = 0U; character < GUI_DISK_PATH_CAPACITY; character++) {
                first_path[character] = entry.name[character];
                if (entry.name[character] == '\0') {
                    break;
                }
            }
            have_first = 1;
        }
        if (select_following != 0) {
            return select_disk_path(entry.name);
        }
        if (text_equal(entry.name, selected_disk_path) != 0) {
            select_following = 1;
        }
    }
    return have_first != 0 ? select_disk_path(first_path) : 0;
}

static void load_viewer_file(const char *path) {
    uint8_t data[MYOS_GUI_CONTENT_MAX] = { 0 };
    char title[MYOS_GUI_CONTENT_TITLE_MAX] = { 0 };
    const uint64_t count = read_viewer_file(path, data);

    if (count == UINT64_MAX) {
        set_viewer_status("UNABLE TO READ FILE");
        return;
    }
    if (disk_path_is_valid(path) != 0) {
        (void)select_disk_path(path);
        selected_disk_title(title);
    } else {
        copy_title(title, "FILE VIEWER");
    }
    if (set_viewer_content(title, data, count, 0U, 0U, 0U) == 0) {
        set_viewer_status("VIEWER UPDATE FAILED");
    }
}

static int save_selected_disk_file(const uint8_t *data, uint64_t length) {
    struct myos_tmpfs_path_request path_request = { { 0 } };
    struct myos_tmpfs_write_request write_request = { 0U, 0U, { 0 }, { 0 } };

    if (make_path(path_request.path, selected_disk_path) == 0
        || make_path(write_request.path, selected_disk_path) == 0) {
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

static uint64_t editor_line_start(const uint8_t *data, uint64_t length, uint64_t position) {
    uint64_t start = position > length ? length : position;

    while (start != 0U && data[start - 1U] != (uint8_t)'\n') {
        start--;
    }
    return start;
}

static uint64_t editor_line_end(const uint8_t *data, uint64_t length, uint64_t start) {
    uint64_t end = start;

    while (end < length && data[end] != (uint8_t)'\n') {
        end++;
    }
    return end;
}

static uint64_t editor_next_line_start(const uint8_t *data, uint64_t length, uint64_t start) {
    const uint64_t end = editor_line_end(data, length, start);

    return end < length ? end + 1U : length;
}

static uint64_t editor_viewport_for_cursor(const uint8_t *data, uint64_t length, uint64_t cursor) {
    uint64_t viewport = 0U;
    uint64_t current = 0U;
    uint64_t rows_to_cursor = 0U;
    const uint64_t target = editor_line_start(data, length, cursor);

    while (current != target) {
        const uint64_t next = editor_next_line_start(data, length, current);

        if (next == current) {
            break;
        }
        current = next;
        rows_to_cursor++;
    }
    while (rows_to_cursor >= 20U) {
        const uint64_t next = editor_next_line_start(data, length, viewport);

        if (next == viewport) {
            break;
        }
        viewport = next;
        rows_to_cursor--;
    }
    return viewport;
}

static void editor_delete_at_cursor(uint8_t *data, uint64_t *length, uint64_t cursor) {
    if (cursor >= *length) {
        return;
    }
    for (uint64_t index = cursor; index + 1U < *length; index++) {
        data[index] = data[index + 1U];
    }
    (*length)--;
    data[*length] = 0U;
}

static void editor_insert_byte(uint8_t *data, uint64_t *length, uint64_t *cursor, uint8_t value) {
    if (*length >= MYOS_GUI_CONTENT_MAX) {
        return;
    }
    for (uint64_t index = *length; index > *cursor; index--) {
        data[index] = data[index - 1U];
    }
    data[*cursor] = value;
    (*length)++;
    (*cursor)++;
}

static void edit_selected_disk_file(void) {
    uint8_t data[MYOS_GUI_CONTENT_MAX] = { 0 };
    uint64_t length = read_viewer_file(selected_disk_path, data);
    uint64_t cursor;

    if (length == UINT64_MAX) {
        length = 0U;
    }
    cursor = length;
    for (;;) {
        char character;
        uint64_t read_result;
        const uint64_t viewport = editor_viewport_for_cursor(data, length, cursor);

        if (set_viewer_content("EDIT NOTE", data, length, MYOS_GUI_CONTENT_FLAG_EDITABLE, cursor, viewport) == 0) {
            return;
        }
        read_result = system_call(MYOS_SYS_READ, 0U, (uint64_t)(uintptr_t)&character, 1U);
        if (read_result == UINT64_MAX || read_result == 0U) {
            continue;
        }
        if (character == '\x1b') {
            load_viewer_file(selected_disk_path);
            return;
        }
        if ((uint8_t)character == UINT8_C(0x13)) {
            if (save_selected_disk_file(data, length) != 0) {
                load_viewer_file(selected_disk_path);
            } else {
                set_viewer_status("SAVE FAILED");
            }
            return;
        }
        if ((uint8_t)character == MYOS_INPUT_KEY_LEFT) {
            if (cursor != 0U) {
                cursor--;
            }
        } else if ((uint8_t)character == MYOS_INPUT_KEY_RIGHT) {
            if (cursor < length) {
                cursor++;
            }
        } else if ((uint8_t)character == MYOS_INPUT_KEY_HOME) {
            cursor = editor_line_start(data, length, cursor);
        } else if ((uint8_t)character == MYOS_INPUT_KEY_END) {
            cursor = editor_line_end(data, length, editor_line_start(data, length, cursor));
        } else if ((uint8_t)character == MYOS_INPUT_KEY_UP) {
            const uint64_t current_start = editor_line_start(data, length, cursor);

            if (current_start != 0U) {
                const uint64_t previous_start = editor_line_start(data, length, current_start - 1U);
                const uint64_t previous_end = editor_line_end(data, length, previous_start);
                const uint64_t column = cursor - current_start;
                const uint64_t previous_length = previous_end - previous_start;

                cursor = previous_start + (column < previous_length ? column : previous_length);
            }
        } else if ((uint8_t)character == MYOS_INPUT_KEY_DOWN) {
            const uint64_t current_start = editor_line_start(data, length, cursor);
            const uint64_t current_end = editor_line_end(data, length, current_start);

            if (current_end < length) {
                const uint64_t next_start = current_end + 1U;
                const uint64_t next_end = editor_line_end(data, length, next_start);
                const uint64_t column = cursor - current_start;
                const uint64_t next_length = next_end - next_start;

                cursor = next_start + (column < next_length ? column : next_length);
            }
        } else if ((uint8_t)character == MYOS_INPUT_KEY_DELETE) {
            editor_delete_at_cursor(data, &length, cursor);
        } else if (character == '\b' || (uint8_t)character == UINT8_C(0x7F)) {
            if (cursor != 0U) {
                cursor--;
                editor_delete_at_cursor(data, &length, cursor);
            }
        } else if (character == '\r' || character == '\n') {
            editor_insert_byte(data, &length, &cursor, (uint8_t)'\n');
        } else if ((uint8_t)character >= 32U && (uint8_t)character <= 126U) {
            editor_insert_byte(data, &length, &cursor, (uint8_t)character);
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
        if (disk_path_is_valid(initial_path) != 0) {
            (void)select_disk_path(initial_path);
        }
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
                edit_selected_disk_file();
            } else if (character == 'm' || character == 'M') {
                load_viewer_file("motd.txt");
            } else if (character == 'D') {
                (void)select_disk_path(GUI_NOTE_PATH);
                load_viewer_file(selected_disk_path);
            } else if (character == 'n' || character == 'N') {
                if (select_next_disk_file() != 0) {
                    load_viewer_file(selected_disk_path);
                } else {
                    set_viewer_status("NO DISK FILES");
                }
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
