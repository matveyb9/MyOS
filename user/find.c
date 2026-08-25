#include <stdint.h>

#include <syscall.h>

#define FIND_DEPTH_MAX UINT64_C(8)
#define FIND_DIRECTORY_ENTRY_MAX UINT64_C(64)
#define FIND_TOTAL_ENTRY_MAX UINT64_C(256)

static char find_query[MYOS_VFS_NAME_MAX];
static char find_path_frames[FIND_DEPTH_MAX][MYOS_VFS_PATH_MAX];
static uint64_t scanned_entries;
static uint64_t matching_entries;

static uint64_t system_call(uint64_t number, uint64_t descriptor, uint64_t buffer, uint64_t length) {
    uint64_t result;

    __asm__ volatile ("syscall"
                      : "=a"(result)
                      : "a"(number), "D"(descriptor), "S"(buffer), "d"(length)
                      : "rcx", "r11", "memory");
    return result;
}

static void write_text(const char *text) {
    uint64_t length = 0U;

    while (text[length] != '\0') { length++; }
    while (length != 0U) {
        const uint64_t chunk = length > UINT64_C(256) ? UINT64_C(256) : length;

        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)text, chunk);
        text += chunk;
        length -= chunk;
    }
}

static void write_number(uint64_t value) {
    char digits[21];
    uint64_t count = 0U;

    if (value == 0U) {
        write_text("0");
        return;
    }
    while (value != 0U) {
        digits[count] = (char)('0' + (value % 10U));
        value /= 10U;
        count++;
    }
    while (count != 0U) {
        count--;
        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)&digits[count], 1U);
    }
}

static char ascii_fold(char value) {
    return value >= 'A' && value <= 'Z' ? (char)(value - 'A' + 'a') : value;
}

static int copy_path(char *destination, const char *source) {
    uint64_t index = 0U;

    if (destination == (char *)0 || source == (const char *)0 || source[0] != '/') {
        return 0;
    }
    while (index + 1U < MYOS_VFS_PATH_MAX && source[index] != '\0') {
        const char character = source[index];

        if (character < ' ' || character > '~') {
            return 0;
        }
        destination[index] = character;
        index++;
    }
    if (source[index] != '\0' || index == 0U || (index > 1U && destination[index - 1U] == '/')) {
        return 0;
    }
    destination[index] = '\0';
    return 1;
}

static int make_child_path(char *destination, const char *parent, const char *name) {
    uint64_t offset = 0U;
    uint64_t name_index = 0U;

    if (destination == (char *)0 || parent == (const char *)0 || name == (const char *)0) {
        return 0;
    }
    while (parent[offset] != '\0' && offset + 1U < MYOS_VFS_PATH_MAX) {
        destination[offset] = parent[offset];
        offset++;
    }
    if (parent[offset] != '\0') {
        return 0;
    }
    if (offset != 1U) {
        if (offset + 1U >= MYOS_VFS_PATH_MAX) {
            return 0;
        }
        destination[offset++] = '/';
    }
    while (name[name_index] != '\0' && offset + 1U < MYOS_VFS_PATH_MAX) {
        const char character = name[name_index++];

        if (character == '/' || character < ' ' || character > '~') {
            return 0;
        }
        destination[offset++] = character;
    }
    if (name[name_index] != '\0' || name_index == 0U) {
        return 0;
    }
    destination[offset] = '\0';
    return 1;
}

static int name_contains_query(const char *name) {
    uint64_t query_length = 0U;

    while (find_query[query_length] != '\0') { query_length++; }
    for (uint64_t start = 0U; name[start] != '\0'; start++) {
        uint64_t index = 0U;

        while (index < query_length && name[start + index] != '\0'
               && ascii_fold(name[start + index]) == ascii_fold(find_query[index])) {
            index++;
        }
        if (index == query_length) {
            return 1;
        }
    }
    return 0;
}

static void write_match(const char *path, const struct myos_vfs_directory_entry *entry) {
    const char *kind = entry->type == MYOS_VFS_OBJECT_DIRECTORY ? "[D] "
        : (entry->type == MYOS_VFS_OBJECT_REGULAR ? "[F] " : "[V] ");

    write_text(kind);
    write_text(path);
    if (entry->type == MYOS_VFS_OBJECT_REGULAR) {
        write_text(" (");
        write_number(entry->size);
        write_text(" bytes)");
    }
    write_text("\n");
}

static void find_walk(const char *path, uint64_t depth) {
    struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };

    if (copy_path(request.path, path) == 0) {
        return;
    }
    for (uint64_t index = 0U; index < FIND_DIRECTORY_ENTRY_MAX; index++) {
        uint64_t result;

        if (scanned_entries >= FIND_TOTAL_ENTRY_MAX) {
            return;
        }
        request.index = index;
        result = system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));
        if (result == UINT64_MAX) {
            return;
        }
        scanned_entries++;
        if (name_contains_query(request.entry.name) != 0
            && make_child_path(find_path_frames[0], path, request.entry.name) != 0) {
            write_match(find_path_frames[0], &request.entry);
            matching_entries++;
        }
        if (request.entry.type == MYOS_VFS_OBJECT_DIRECTORY && depth + 1U < FIND_DEPTH_MAX
            && make_child_path(find_path_frames[depth], path, request.entry.name) != 0) {
            find_walk(find_path_frames[depth], depth + 1U);
        }
    }
}

static int copy_query(const char *arguments, uint64_t *offset) {
    uint64_t length = 0U;

    while (arguments[*offset] != '\0' && arguments[*offset] != ' '
           && length + 1U < sizeof(find_query)) {
        const char character = arguments[*offset];

        if (character < ' ' || character > '~' || character == '/') {
            return 0;
        }
        find_query[length++] = character;
        (*offset)++;
    }
    find_query[length] = '\0';
    return length != 0U;
}

void _start(uint64_t argc, const char *arguments) {
    char path[MYOS_VFS_PATH_MAX] = { 0 };
    uint64_t offset = 0U;

    if ((argc != 1U && argc != 2U) || copy_query(arguments, &offset) == 0) {
        write_text("Usage: run find <name-fragment> [absolute-directory]\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    if (arguments[offset] == '\0') {
        path[0] = '/';
        path[1] = '\0';
    } else {
        offset++;
        if (copy_path(path, arguments + offset) == 0) {
            write_text("Usage: run find <name-fragment> [absolute-directory]\n");
            (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
        }
    }
    write_text("find: ");
    write_text(find_query);
    write_text(" in ");
    write_text(path);
    write_text("\n");
    find_walk(path, 1U);
    if (scanned_entries >= FIND_TOTAL_ENTRY_MAX) {
        write_text("find: entry limit reached\n");
    }
    write_text("find: ");
    write_number(matching_entries);
    write_text(" match(es), ");
    write_number(scanned_entries);
    write_text(" entry(s) scanned\n");
    (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
