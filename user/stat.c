#include <stdint.h>

#include <syscall.h>

#define STAT_DIRECTORY_ENTRY_MAX UINT64_C(128)

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

    while (text[length] != '\0') {
        length++;
    }
    while (length != 0U) {
        const uint64_t chunk = length > MYOS_VFS_READ_CHUNK ? MYOS_VFS_READ_CHUNK : length;

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
        digits[count++] = (char)('0' + value % UINT64_C(10));
        value /= UINT64_C(10);
    }
    while (count != 0U) {
        count--;
        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)&digits[count], 1U);
    }
}

static int copy_path(const char *source, char *destination) {
    uint64_t index = 0U;

    if (source == (const char *)0 || destination == (char *)0 || source[0] != '/') {
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

static char ascii_fold(char value) {
    return value >= 'A' && value <= 'Z' ? (char)(value - 'A' + 'a') : value;
}

static int name_equal_fold(const char *left, const char *right) {
    uint64_t index = 0U;

    if (left == (const char *)0 || right == (const char *)0) {
        return 0;
    }
    while (left[index] != '\0' && right[index] != '\0') {
        if (ascii_fold(left[index]) != ascii_fold(right[index])) {
            return 0;
        }
        index++;
    }
    return left[index] == '\0' && right[index] == '\0';
}

static int split_parent(char *path, char *parent, const char **name) {
    uint64_t index = 0U;
    uint64_t last_slash = 0U;

    if (path == (char *)0 || parent == (char *)0 || name == (const char **)0 || path[0] != '/') {
        return 0;
    }
    while (path[index] != '\0') {
        if (path[index] == '/') {
            last_slash = index;
        }
        index++;
    }
    if (index == 1U) {
        return 0;
    }
    if (last_slash == 0U) {
        parent[0] = '/';
        parent[1] = '\0';
    } else {
        for (uint64_t copy = 0U; copy < last_slash; copy++) {
            parent[copy] = path[copy];
        }
        parent[last_slash] = '\0';
    }
    *name = path + last_slash + 1U;
    return (*name)[0] != '\0';
}

static const char *type_name(uint64_t type) {
    if (type == MYOS_VFS_OBJECT_REGULAR) {
        return "regular";
    }
    if (type == MYOS_VFS_OBJECT_DIRECTORY) {
        return "directory";
    }
    if (type == MYOS_VFS_OBJECT_SYMBOLIC_LINK) {
        return "symbolic-link";
    }
    if (type == MYOS_VFS_OBJECT_VIRTUAL) {
        return "virtual";
    }
    return "unknown";
}

static int lookup_child(const char *parent, const char *name, struct myos_vfs_directory_entry *entry) {
    struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };

    if (copy_path(parent, request.path) == 0) {
        return 0;
    }
    for (uint64_t index = 0U; index < STAT_DIRECTORY_ENTRY_MAX; index++) {
        request.index = index;
        if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
            return 0;
        }
        if (name_equal_fold(request.entry.name, name) != 0) {
            *entry = request.entry;
            return 1;
        }
    }
    return 0;
}

static void print_entry(const char *path, const struct myos_vfs_directory_entry *entry) {
    write_text("stat: ");
    write_text(path);
    write_text("\ntype: ");
    write_text(type_name(entry->type));
    write_text("\nsize: ");
    write_number(entry->size);
    write_text(" bytes\n");
}

void _start(uint64_t argc, const char *arguments) {
    char path[MYOS_VFS_PATH_MAX] = { 0 };
    char parent[MYOS_VFS_PATH_MAX] = { 0 };
    const char *name;
    struct myos_vfs_directory_entry entry = { { 0 }, 0U, 0U };

    if (argc != 1U || copy_path(arguments, path) == 0) {
        write_text("Usage: run stat <absolute-path>\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    if (path[1] == '\0') {
        entry.type = MYOS_VFS_OBJECT_DIRECTORY;
        entry.size = 0U;
        print_entry(path, &entry);
    } else if (split_parent(path, parent, &name) == 0 || lookup_child(parent, name, &entry) == 0) {
        write_text("stat: path not found\n");
        (void)system_call(MYOS_SYS_EXIT, 1U, 0U, 0U);
    } else {
        print_entry(path, &entry);
    }
    (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
