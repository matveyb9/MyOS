#include <stdint.h>

#include <syscall.h>

#define TREE_DEPTH_MAX UINT64_C(8)
#define TREE_DIRECTORY_ENTRY_MAX UINT64_C(64)
#define TREE_TOTAL_ENTRY_MAX UINT64_C(256)

static uint64_t printed_entries;
static char tree_path_frames[TREE_DEPTH_MAX][MYOS_VFS_PATH_MAX];

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

static void write_indent(uint64_t depth) {
    while (depth != 0U) {
        write_text("  ");
        depth--;
    }
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

static void write_entry(uint64_t depth, const struct myos_vfs_directory_entry *entry) {
    const char *kind = entry->type == MYOS_VFS_OBJECT_DIRECTORY ? "[D] "
        : (entry->type == MYOS_VFS_OBJECT_REGULAR ? "[F] " : "[V] ");

    write_indent(depth);
    write_text(kind);
    write_text(entry->name);
    if (entry->type == MYOS_VFS_OBJECT_REGULAR) {
        write_text(" (");
        write_number(entry->size);
        write_text(" bytes)");
    }
    write_text("\n");
}

static void tree_walk(const char *path, uint64_t depth) {
    struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };

    if (copy_path(request.path, path) == 0) {
        return;
    }
    for (uint64_t index = 0U; index < TREE_DIRECTORY_ENTRY_MAX; index++) {
        uint64_t result;

        if (printed_entries >= TREE_TOTAL_ENTRY_MAX) {
            write_indent(depth);
            write_text("... entry limit reached\n");
            return;
        }
        request.index = index;
        result = system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));
        if (result == UINT64_MAX) {
            return;
        }
        write_entry(depth, &request.entry);
        printed_entries++;
        if (request.entry.type == MYOS_VFS_OBJECT_DIRECTORY) {
            if (depth + 1U >= TREE_DEPTH_MAX) {
                write_indent(depth + 1U);
                write_text("... depth limit reached\n");
                continue;
            }
            if (make_child_path(tree_path_frames[depth], path, request.entry.name) != 0) {
                tree_walk(tree_path_frames[depth], depth + 1U);
            }
        }
    }
    write_indent(depth);
    write_text("... directory entry limit reached\n");
}

void _start(uint64_t argc, const char *arguments) {
    char path[MYOS_VFS_PATH_MAX] = { 0 };

    if (argc == 0U && arguments[0] == '\0') {
        path[0] = '/';
        path[1] = '\0';
    } else if (argc != 1U || copy_path(path, arguments) == 0) {
        write_text("Usage: run tree [absolute-directory]\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    write_text(path);
    write_text("\n");
    tree_walk(path, 1U);
    write_text("tree: ");
    write_number(printed_entries);
    write_text(" entry(s)\n");
    (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
