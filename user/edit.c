#include <stdint.h>

#include <syscall.h>

#define EDIT_DOCUMENT_CAPACITY UINT64_C(4096)
#define EDIT_VIEWPORT_ROWS UINT64_C(18)

static uint8_t document[EDIT_DOCUMENT_CAPACITY];

static uint64_t system_call(uint64_t number, uint64_t argument1, uint64_t argument2, uint64_t argument3) {
    uint64_t result;

    __asm__ volatile ("syscall" : "=a"(result) : "a"(number), "D"(argument1), "S"(argument2), "d"(argument3)
                      : "rcx", "r11", "memory");
    return result;
}

static void write_bytes(const char *text, uint64_t length) {
    while (length != 0U) {
        const uint64_t chunk = length > MYOS_VFS_READ_CHUNK ? MYOS_VFS_READ_CHUNK : length;

        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)text, chunk);
        text += chunk;
        length -= chunk;
    }
}

static void write_text(const char *text) {
    uint64_t length = 0U;

    while (text[length] != '\0') { length++; }
    write_bytes(text, length);
}

static void write_number(uint64_t value) {
    char digits[21];
    uint64_t length = 0U;

    if (value == 0U) {
        write_text("0");
        return;
    }
    while (value != 0U && length < sizeof(digits)) {
        digits[length++] = (char)('0' + value % 10U);
        value /= 10U;
    }
    while (length != 0U) {
        length--;
        write_bytes(&digits[length], 1U);
    }
}

static int copy_path(char *destination, const char *arguments) {
    uint64_t length = 0U;

    if (arguments == (const char *)0 || arguments[0] != '/') { return 0; }
    while (arguments[length] != '\0') {
        if (length + 1U >= MYOS_VFS_PATH_MAX || arguments[length] == ' ') { return 0; }
        destination[length] = arguments[length];
        length++;
    }
    destination[length] = '\0';
    return length != 0U;
}

static int make_path(char *destination, const char *source) {
    uint64_t index = 0U;

    while (index + 1U < MYOS_VFS_PATH_MAX && source[index] != '\0') {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
    return index != 0U && source[index] == '\0';
}

static int load_document(const char *path, uint64_t *length) {
    struct myos_vfs_read_request request = { 0U, { 0 }, { 0 } };
    uint64_t total = 0U;

    if (make_path(request.path, path) == 0) { return 0; }
    for (;;) {
        uint64_t count;

        request.offset = total;
        count = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));
        if (count == UINT64_MAX || count > MYOS_VFS_READ_CHUNK || count > EDIT_DOCUMENT_CAPACITY - total) {
            return 0;
        }
        for (uint64_t index = 0U; index < count; index++) { document[total + index] = request.data[index]; }
        total += count;
        if (count == 0U) {
            *length = total;
            return 1;
        }
    }
}

static int create_empty_file(const char *path) {
    struct myos_vfs_path_request request = { { 0 } };

    return make_path(request.path, path) != 0
           && system_call(MYOS_SYS_VFS_CREATE_FILE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) != UINT64_MAX;
}

static int save_document(const char *path, uint64_t length) {
    struct myos_vfs_path_request path_request = { { 0 } };
    struct myos_vfs_write_request write_request = { 0U, 0U, { 0 }, { 0 } };
    uint64_t offset = 0U;

    if (make_path(path_request.path, path) == 0 || make_path(write_request.path, path) == 0) { return 0; }
    (void)system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&path_request, sizeof(path_request));
    if (system_call(MYOS_SYS_VFS_CREATE_FILE, 0U, (uint64_t)(uintptr_t)&path_request, sizeof(path_request)) == UINT64_MAX) {
        return 0;
    }
    while (offset < length) {
        const uint64_t chunk = length - offset < MYOS_VFS_READ_CHUNK ? length - offset : MYOS_VFS_READ_CHUNK;

        write_request.offset = offset;
        write_request.length = chunk;
        for (uint64_t index = 0U; index < chunk; index++) { write_request.data[index] = document[offset + index]; }
        if (system_call(MYOS_SYS_VFS_WRITE, 0U, (uint64_t)(uintptr_t)&write_request, sizeof(write_request)) != chunk) {
            return 0;
        }
        offset += chunk;
    }
    return 1;
}

static uint64_t line_start(uint64_t length, uint64_t cursor) {
    uint64_t start = cursor > length ? length : cursor;

    while (start != 0U && document[start - 1U] != (uint8_t)'\n') { start--; }
    return start;
}

static uint64_t line_end(uint64_t length, uint64_t start) {
    uint64_t end = start;

    while (end < length && document[end] != (uint8_t)'\n') { end++; }
    return end;
}

static uint64_t previous_line_start(uint64_t length, uint64_t start) {
    return start == 0U ? 0U : line_start(length, start - 1U);
}

static uint64_t viewport_start(uint64_t length, uint64_t cursor) {
    uint64_t viewport = line_start(length, cursor);

    for (uint64_t row = 0U; row < EDIT_VIEWPORT_ROWS / 2U && viewport != 0U; row++) {
        viewport = previous_line_start(length, viewport);
    }
    return viewport;
}

static void render(const char *path, uint64_t length, uint64_t cursor) {
    uint64_t position = viewport_start(length, cursor);
    uint64_t rows = 0U;

    write_text("\x1B[2J\x1B[H");
    write_text("MYOS TEXT EDITOR\n");
    write_text("File: ");
    write_text(path);
    write_text("\nCtrl-S save+exit | Ctrl-Q or Esc discard | arrows/Home/End move | Del/Backspace edit\n");
    write_text("----------------------------------------------------------------\n");
    while (position < length && rows < EDIT_VIEWPORT_ROWS) {
        uint8_t value;

        if (position == cursor) { write_text("|"); }
        value = document[position++];
        if (value == (uint8_t)'\r') {
            continue;
        }
        if (value == (uint8_t)'\t') {
            write_text("    ");
        } else {
            write_bytes((const char *)&value, 1U);
        }
        if (value == (uint8_t)'\n') { rows++; }
    }
    if (cursor == length) { write_text("|"); }
    if (rows == 0U || (position != 0U && document[position - 1U] != (uint8_t)'\n')) { write_text("\n"); }
    write_text("----------------------------------------------------------------\n");
    write_text("Cursor ");
    write_number(cursor);
    write_text(" / ");
    write_number(length);
    write_text(" bytes");
    if (length == EDIT_DOCUMENT_CAPACITY) { write_text(" (document limit reached)"); }
    write_text("\n");
}

static void insert_byte(uint64_t *length, uint64_t *cursor, uint8_t value) {
    if (*length >= EDIT_DOCUMENT_CAPACITY) { return; }
    for (uint64_t index = *length; index > *cursor; index--) { document[index] = document[index - 1U]; }
    document[*cursor] = value;
    (*length)++;
    (*cursor)++;
}

static void delete_at_cursor(uint64_t *length, uint64_t cursor) {
    if (cursor >= *length) { return; }
    for (uint64_t index = cursor; index + 1U < *length; index++) { document[index] = document[index + 1U]; }
    (*length)--;
}

static void move_up(uint64_t length, uint64_t *cursor) {
    const uint64_t current_start = line_start(length, *cursor);

    if (current_start != 0U) {
        const uint64_t previous_start = previous_line_start(length, current_start);
        const uint64_t previous_end = line_end(length, previous_start);
        const uint64_t column = *cursor - current_start;
        const uint64_t previous_length = previous_end - previous_start;

        *cursor = previous_start + (column < previous_length ? column : previous_length);
    }
}

static void move_down(uint64_t length, uint64_t *cursor) {
    const uint64_t current_start = line_start(length, *cursor);
    const uint64_t current_end = line_end(length, current_start);

    if (current_end < length) {
        const uint64_t next_start = current_end + 1U;
        const uint64_t next_end = line_end(length, next_start);
        const uint64_t column = *cursor - current_start;
        const uint64_t next_length = next_end - next_start;

        *cursor = next_start + (column < next_length ? column : next_length);
    }
}

void _start(uint64_t argc, const char *arguments) {
    char path[MYOS_VFS_PATH_MAX] = { 0 };
    uint64_t length = 0U;
    uint64_t cursor;

    if (argc != 1U || copy_path(path, arguments) == 0) {
        write_text("Usage: edit <absolute-file>\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    if (load_document(path, &length) == 0) {
        if (create_empty_file(path) == 0 || load_document(path, &length) == 0) {
            write_text("edit: unable to open file\n");
            (void)system_call(MYOS_SYS_EXIT, 1U, 0U, 0U);
        }
    }
    cursor = length;
    for (;;) {
        char character;
        uint64_t result;

        render(path, length, cursor);
        result = system_call(MYOS_SYS_READ, 0U, (uint64_t)(uintptr_t)&character, 1U);
        if (result == UINT64_MAX || result == 0U) { continue; }
        if (character == '\x1B' || (uint8_t)character == UINT8_C(0x11)) {
            write_text("edit: discarded changes\n");
            (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
        }
        if ((uint8_t)character == UINT8_C(0x13)) {
            if (save_document(path, length) == 0) {
                write_text("edit: save failed\n");
                continue;
            }
            write_text("edit: saved ");
            write_number(length);
            write_text(" byte(s)\n");
            (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
        }
        if ((uint8_t)character == MYOS_INPUT_KEY_LEFT) {
            if (cursor != 0U) { cursor--; }
        } else if ((uint8_t)character == MYOS_INPUT_KEY_RIGHT) {
            if (cursor < length) { cursor++; }
        } else if ((uint8_t)character == MYOS_INPUT_KEY_HOME) {
            cursor = line_start(length, cursor);
        } else if ((uint8_t)character == MYOS_INPUT_KEY_END) {
            cursor = line_end(length, line_start(length, cursor));
        } else if ((uint8_t)character == MYOS_INPUT_KEY_UP) {
            move_up(length, &cursor);
        } else if ((uint8_t)character == MYOS_INPUT_KEY_DOWN) {
            move_down(length, &cursor);
        } else if ((uint8_t)character == MYOS_INPUT_KEY_DELETE) {
            delete_at_cursor(&length, cursor);
        } else if (character == '\b' || (uint8_t)character == UINT8_C(0x7F)) {
            if (cursor != 0U) {
                cursor--;
                delete_at_cursor(&length, cursor);
            }
        } else if (character == '\r' || character == '\n') {
            insert_byte(&length, &cursor, (uint8_t)'\n');
        } else if ((uint8_t)character >= 32U && (uint8_t)character <= 126U) {
            insert_byte(&length, &cursor, (uint8_t)character);
        }
    }
}
