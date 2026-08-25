#include <stdint.h>

#include <syscall.h>

#define GUI_NOTE_PATH "/users/myos/files/notes/note"
#define GUI_NOTE_DIRECTORY "/users/myos/files/notes"
#define GUI_NOTE_PATH_CAPACITY MYOS_VFS_PATH_MAX
#define GUI_VFS_ENTRY_SCAN_LIMIT UINT64_C(64)
#define GUI_APP_DIRECTORY "/apps"
#define GUI_LAUNCHER_APP_NAME_MAX UINT64_C(16)
#define GUI_BROWSER_START_PATH "/users/myos"
#define GUI_BROWSER_PROJECT_PATH "/users/myos/projects"
#define GUI_BROWSER_PAGE_SIZE MYOS_GUI_BROWSER_ENTRY_MAX
#define GUI_BROWSER_READ_TOO_LARGE (UINT64_MAX - UINT64_C(1))
#define GUI_BROWSER_COPY_ENTRY_SCAN_LIMIT UINT64_C(128)
#define GUI_BROWSER_COPY_FILE_MAX UINT64_C(65536)
#define GUI_EDITOR_RESULT_VIEWER 0
#define GUI_EDITOR_RESULT_EXIT 1
#define GUI_EDITOR_RESULT_HOME 2
#define GUI_BROWSER_RESULT_OPENED 2

static char selected_disk_path[GUI_NOTE_PATH_CAPACITY] = GUI_NOTE_PATH;
static char browser_directory[MYOS_VFS_PATH_MAX] = GUI_BROWSER_START_PATH;
static uint64_t browser_page;
static char browser_new_file_name[MYOS_VFS_NAME_MAX];
static uint64_t browser_new_file_length;
static int browser_new_entry_is_directory;
static int browser_prompt_is_remove;
static int browser_remove_confirmation;
static char browser_copy_source_name[MYOS_VFS_NAME_MAX];
static uint64_t browser_copy_source_length;
static int browser_prompt_is_copy;
static int browser_copy_prompt_for_target;
static char browser_rename_source_name[MYOS_VFS_NAME_MAX];
static uint64_t browser_rename_source_length;
static int browser_prompt_is_rename;
static int browser_rename_prompt_for_target;
static char browser_move_source_name[MYOS_VFS_NAME_MAX];
static uint64_t browser_move_source_length;
static char browser_move_target_directory[MYOS_VFS_PATH_MAX];
static uint64_t browser_move_target_length;
static int browser_prompt_is_move;
static int browser_move_prompt_for_target;
static int browser_prompt_is_search;
static int browser_search_results_active;
static uint64_t browser_search_indices[MYOS_GUI_BROWSER_ENTRY_MAX];
static uint64_t browser_search_count;
static uint8_t gui_scratch_data[MYOS_GUI_CONTENT_MAX];
static uint8_t gui_editor_data[MYOS_GUI_CONTENT_MAX];
static struct myos_gui_content_request gui_content_request;

static void load_viewer_file(const char *path);
static int edit_selected_disk_file(void);

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

    while (index + 1U < MYOS_VFS_PATH_MAX && source[index] != '\0') {
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

static int project_name_is_valid(const char *name) {
    uint64_t length = 0U;

    if (name == (const char *)0) { return 0; }
    while (length < 31U && name[length] != '\0') {
        const char character = name[length];

        if (!((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z')
              || (character >= '0' && character <= '9') || character == '-' || character == '_')) {
            return 0;
        }
        length++;
    }
    return length != 0U && name[length] == '\0';
}

static int project_argument_requested(const char *arguments) {
    static const char prefix[] = "project ";

    for (uint64_t index = 0U; index < sizeof(prefix) - 1U; index++) {
        if (arguments[index] == '\0' || arguments[index] != prefix[index]) { return 0; }
    }
    return 1;
}

static int make_project_workspace_path(char *destination, const char *arguments, int *requested_mode,
                                       char *run_arguments) {
    struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };
    char name[32] = { 0 };
    const char *argument_name;
    uint64_t name_length = 0U;
    uint64_t offset = 0U;

    if (destination == (char *)0 || requested_mode == (int *)0 || run_arguments == (char *)0
        || project_argument_requested(arguments) == 0) {
        return 0;
    }
    run_arguments[0] = '\0';
    argument_name = arguments + 8U;
    while (name_length < 31U && argument_name[name_length] != '\0' && argument_name[name_length] != ' ') {
        name[name_length] = argument_name[name_length];
        name_length++;
    }
    if (project_name_is_valid(name) == 0) { return 0; }
    if (argument_name[name_length] == '\0') {
        *requested_mode = 0;
    } else if (text_equal(argument_name + name_length, " edit") != 0) {
        *requested_mode = 1;
    } else if (text_equal(argument_name + name_length, " status") != 0) {
        *requested_mode = 2;
    } else if (text_equal(argument_name + name_length, " build") != 0) {
        *requested_mode = 3;
    } else if (text_equal(argument_name + name_length, " install") != 0) {
        *requested_mode = 5;
    } else if (text_equal(argument_name + name_length, " uninstall") != 0) {
        *requested_mode = 6;
    } else if (text_equal(argument_name + name_length, " clean") != 0) {
        *requested_mode = 7;
    } else if (text_equal(argument_name + name_length, " remove") != 0) {
        *requested_mode = 8;
    } else if (argument_name[name_length] == ' ') {
        uint64_t run_length = 0U;
        const char *tail = argument_name + name_length + 1U;

        if (tail[0] != 'r' || tail[1] != 'u' || tail[2] != 'n' || (tail[3] != '\0' && tail[3] != ' ')) {
            return 0;
        }
        tail += 3U;
        if (tail[0] == ' ') { tail++; }
        while (tail[run_length] != '\0' && run_length + 1U < MYOS_SPAWN_ARGUMENTS_MAX) {
            run_arguments[run_length] = tail[run_length];
            run_length++;
        }
        if (tail[run_length] != '\0') { return 0; }
        run_arguments[run_length] = '\0';
        *requested_mode = 4;
    } else {
        return 0;
    }
    while (GUI_BROWSER_PROJECT_PATH[offset] != '\0') {
        destination[offset] = GUI_BROWSER_PROJECT_PATH[offset];
        offset++;
    }
    destination[offset++] = '/';
    for (uint64_t index = 0U; name[index] != '\0'; index++) { destination[offset++] = name[index]; }
    destination[offset] = '\0';
    if (make_path(request.path, GUI_BROWSER_PROJECT_PATH) == 0) { return 0; }
    for (uint64_t index = 0U; index < UINT64_C(128); index++) {
        request.index = index;
        if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
            return 0;
        }
        if (request.entry.type == MYOS_VFS_OBJECT_DIRECTORY && text_equal(request.entry.name, name) != 0) {
            return 1;
        }
    }
    return 0;
}

static int project_workspace_is_directory(const char *project_path) {
    struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };
    const char *name;
    uint64_t prefix_length = 0U;

    if (project_path == (const char *)0) { return 0; }
    while (GUI_BROWSER_PROJECT_PATH[prefix_length] != '\0') {
        if (project_path[prefix_length] != GUI_BROWSER_PROJECT_PATH[prefix_length]) { return 0; }
        prefix_length++;
    }
    if (project_path[prefix_length] != '/') { return 0; }
    name = project_path + prefix_length + 1U;
    if (project_name_is_valid(name) == 0 || make_path(request.path, GUI_BROWSER_PROJECT_PATH) == 0) { return 0; }
    for (uint64_t index = 0U; index < UINT64_C(128); index++) {
        request.index = index;
        if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
            return 0;
        }
        if (request.entry.type == MYOS_VFS_OBJECT_DIRECTORY && text_equal(request.entry.name, name) != 0) {
            return 1;
        }
    }
    return 0;
}

static int make_project_source_path(char *destination, const char *project_path) {
    uint64_t offset = 0U;

    if (destination == (char *)0 || project_path == (const char *)0) { return 0; }
    while (project_path[offset] != '\0' && offset + 1U < MYOS_VFS_PATH_MAX) {
        destination[offset] = project_path[offset];
        offset++;
    }
    if (project_path[offset] != '\0' || offset + 10U >= MYOS_VFS_PATH_MAX) { return 0; }
    destination[offset++] = '/';
    destination[offset++] = 'm';
    destination[offset++] = 'a';
    destination[offset++] = 'i';
    destination[offset++] = 'n';
    destination[offset++] = '.';
    destination[offset++] = 'm';
    destination[offset++] = 'y';
    destination[offset++] = 'a';
    destination[offset] = '\0';
    return 1;
}

static int project_build_is_regular(const char *project_path) {
    struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };

    if (make_path(request.path, project_path) == 0) { return 0; }
    for (uint64_t index = 0U; index < UINT64_C(128); index++) {
        request.index = index;
        if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
            return 0;
        }
        if (text_equal(request.entry.name, "main.elf") != 0) {
            return request.entry.type == MYOS_VFS_OBJECT_REGULAR;
        }
    }
    return 0;
}

static int make_project_package_directory(char *destination, const char *project_path);

static int project_package_is_regular(const char *project_path) {
    struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };
    char package_directory[MYOS_VFS_PATH_MAX] = { 0 };

    if (make_project_package_directory(package_directory, project_path) == 0
        || make_path(request.path, package_directory) == 0) {
        return 0;
    }
    for (uint64_t index = 0U; index < UINT64_C(128); index++) {
        request.index = index;
        if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
            return 0;
        }
        if (text_equal(request.entry.name, "main.elf") != 0) {
            return request.entry.type == MYOS_VFS_OBJECT_REGULAR;
        }
    }
    return 0;
}

static int project_source_is_regular(const char *project_path) {
    struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };

    if (make_path(request.path, project_path) == 0) { return 0; }
    for (uint64_t index = 0U; index < UINT64_C(128); index++) {
        request.index = index;
        if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
            return 0;
        }
        if (text_equal(request.entry.name, "main.mya") != 0) {
            return request.entry.type == MYOS_VFS_OBJECT_REGULAR;
        }
    }
    return 0;
}

static int disk_path_is_valid(const char *path) {
    uint64_t index = 0U;

    if (path == (const char *)0 || path[0] != '/') { return 0; }
    if (path[1] == '\0') { return 1; }
    while (index < GUI_NOTE_PATH_CAPACITY) {
        const char character = path[index];

        if (character == '\0') {
            return index != 0U && path[index - 1U] != '/';
        }
        if (character < ' ' || character > '~' || (character == '/' && index != 0U && path[index - 1U] == '/')) {
            return 0;
        }
        index++;
    }
    return 0;
}

static int path_matches_root(const char *path, const char *root) {
    uint64_t index = 0U;

    while (root[index] != '\0') {
        if (path[index] != root[index]) { return 0; }
        index++;
    }
    return path[index] == '\0' || path[index] == '/';
}

static int selected_disk_path_is_writable(void) {
    return path_matches_root(selected_disk_path, "/users/myos") != 0
        || path_matches_root(selected_disk_path, "/temp") != 0
        || path_matches_root(selected_disk_path, "/system/data") != 0
        || path_matches_root(selected_disk_path, "/system/config") != 0;
}

static int select_disk_path(const char *path) {
    if (disk_path_is_valid(path) == 0) {
        return 0;
    }
    for (uint64_t index = 0U; index < GUI_NOTE_PATH_CAPACITY; index++) {
        selected_disk_path[index] = path[index];
        if (path[index] == '\0') {
            return 1;
        }
    }
    return 0;
}

static void selected_disk_title(char *title) {
    uint64_t source = 0U;
    uint64_t destination = 0U;

    while (selected_disk_path[source] != '\0') {
        if (selected_disk_path[source] == '/') { destination = source + 1U; }
        source++;
    }
    source = destination;
    destination = 0U;
    while (selected_disk_path[source] != '\0' && destination + 1U < MYOS_GUI_CONTENT_TITLE_MAX) {
        title[destination++] = selected_disk_path[source++];
    }
    title[destination] = '\0';
}

static int set_viewer_content(const char *title, const uint8_t *data, uint64_t length, uint64_t flags,
                              uint64_t cursor, uint64_t viewport) {
    if (data == (const uint8_t *)0 || length > MYOS_GUI_CONTENT_MAX) {
        return 0;
    }
    copy_title(gui_content_request.title, title);
    for (uint64_t index = 0U; index < length; index++) {
        gui_content_request.data[index] = data[index];
    }
    gui_content_request.length = length;
    gui_content_request.flags = flags;
    gui_content_request.cursor = cursor;
    gui_content_request.viewport = viewport;
    return system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_SET_CONTENT, (uint64_t)(uintptr_t)&gui_content_request,
                       sizeof(gui_content_request)) != UINT64_MAX;
}

static void set_viewer_status(const char *message) {
    uint64_t length = 0U;

    while (length < MYOS_GUI_CONTENT_MAX && message[length] != '\0') {
        gui_scratch_data[length] = (uint8_t)message[length];
        length++;
    }
    (void)set_viewer_content("VIEWER", gui_scratch_data, length, 0U, 0U, 0U);
}

static void show_desktop_home(void) {
    static const uint8_t text[] =
        "MYOS DESKTOP\n"
        "\n"
        "CLICK A TILE TO OPEN\n"
        "INSTALLED APPS APPEAR BELOW\n"
        "ALT-TAB  FOCUS\n"
        "CTRL-Q   EXIT\n";

    (void)set_viewer_content("MYOS DESKTOP", text, sizeof(text) - 1U, MYOS_GUI_CONTENT_FLAG_LAUNCHER, 0U, 0U);
}

static int make_launcher_app_path(char *destination, const char *name) {
    static const char prefix[] = GUI_APP_DIRECTORY "/";
    static const char suffix[] = "/main.elf";
    uint64_t offset = 0U;

    for (uint64_t index = 0U; index + 1U < sizeof(prefix); index++) {
        destination[offset++] = prefix[index];
    }
    for (uint64_t index = 0U; name[index] != '\0' && offset + sizeof(suffix) < MYOS_VFS_PATH_MAX; index++) {
        if (name[index] == '/' || name[index] < ' ' || name[index] > '~' || index + 1U >= GUI_LAUNCHER_APP_NAME_MAX) {
            return 0;
        }
        destination[offset++] = name[index];
    }
    if (offset == sizeof(prefix) - 1U) {
        return 0;
    }
    for (uint64_t index = 0U; index + 1U < sizeof(suffix) && offset + 1U < MYOS_VFS_PATH_MAX; index++) {
        destination[offset++] = suffix[index];
    }
    destination[offset] = '\0';
    return offset + 1U < MYOS_VFS_PATH_MAX;
}

static int launcher_app_path_at(uint8_t action, char *path) {
    uint64_t wanted;
    uint64_t discovered = 0U;

    if (action < MYOS_INPUT_GUI_ACTION_APP_BASE
        || action - MYOS_INPUT_GUI_ACTION_APP_BASE >= MYOS_GUI_LAUNCHER_APP_MAX) {
        return 0;
    }
    wanted = action - MYOS_INPUT_GUI_ACTION_APP_BASE;
    for (uint64_t index = 0U; index < GUI_VFS_ENTRY_SCAN_LIMIT; index++) {
        struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };
        struct myos_vfs_read_request verify = { 0U, { 0 }, { 0 } };

        if (make_path(request.path, GUI_APP_DIRECTORY) == 0) { return 0; }
        request.index = index;
        if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
            break;
        }
        if (request.entry.type != MYOS_VFS_OBJECT_DIRECTORY
            || make_launcher_app_path(verify.path, request.entry.name) == 0) {
            continue;
        }
        const uint64_t verify_length = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&verify, sizeof(verify));
        if (verify_length == UINT64_MAX || verify_length == 0U) {
            continue;
        }
        if (discovered == wanted) {
            for (uint64_t character = 0U; character < MYOS_VFS_PATH_MAX; character++) {
                path[character] = verify.path[character];
                if (verify.path[character] == '\0') { return 1; }
            }
            return 0;
        }
        discovered++;
        if (discovered >= MYOS_GUI_LAUNCHER_APP_MAX) {
            break;
        }
    }
    return 0;
}

static int launch_launcher_app(uint8_t action) {
    struct myos_spawn_request request = { { 0 }, { 0 }, UINT64_MAX, UINT64_MAX };
    uint64_t child;

    if (launcher_app_path_at(action, request.path) == 0) {
        return 0;
    }
    child = system_call(MYOS_SYS_SPAWN, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));
    if (child == UINT64_MAX) {
        return 0;
    }
    (void)system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_END, 0U, 0U);
    (void)system_call(MYOS_SYS_WAIT, child, 0U, 0U);
    return 1;
}

static uint64_t read_viewer_file(const char *path, uint8_t *data, uint64_t capacity) {
    struct myos_vfs_read_request request = { 0U, { 0 }, { 0 } };
    uint64_t total = 0U;

    if (make_path(request.path, path) == 0 || data == (uint8_t *)0 || capacity == 0U) {
        return UINT64_MAX;
    }
    while (total < capacity) {
        const uint64_t count = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));

        if (count == UINT64_MAX || count > MYOS_VFS_READ_CHUNK) {
            return UINT64_MAX;
        }
        if (count > capacity - total) {
            return GUI_BROWSER_READ_TOO_LARGE;
        }
        for (uint64_t index = 0U; index < count; index++) {
            data[total + index] = request.data[index];
        }
        total += count;
        if (count < MYOS_VFS_READ_CHUNK) {
            return total;
        }
        request.offset = total;
    }
    {
        const uint64_t probe = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));

        if (probe == UINT64_MAX) {
            return UINT64_MAX;
        }
        return probe == 0U ? total : GUI_BROWSER_READ_TOO_LARGE;
    }
}

static uint64_t text_length_bounded(const char *text, uint64_t limit) {
    uint64_t length = 0U;

    while (length < limit && text[length] != '\0') { length++; }
    return length;
}

static void content_append_char(uint8_t *data, uint64_t *length, char character) {
    if (*length < MYOS_GUI_CONTENT_MAX) {
        data[*length] = (uint8_t)character;
        (*length)++;
    }
}

static void content_append_text(uint8_t *data, uint64_t *length, const char *text, uint64_t limit) {
    uint64_t index = 0U;

    while (index < limit && text[index] != '\0') {
        content_append_char(data, length, text[index]);
        index++;
    }
}

static void content_append_decimal(uint8_t *data, uint64_t *length, uint64_t value) {
    char reversed[20];
    uint64_t count = 0U;

    do {
        reversed[count++] = (char)('0' + value % UINT64_C(10));
        value /= UINT64_C(10);
    } while (value != 0U && count < sizeof(reversed));
    while (count != 0U) {
        content_append_char(data, length, reversed[--count]);
    }
}

static void content_append_browser_name(uint8_t *data, uint64_t *length, const char *name) {
    uint64_t index = 0U;

    while (index < 12U && name[index] != '\0') {
        content_append_char(data, length, name[index]);
        index++;
    }
    while (index < 12U) {
        content_append_char(data, length, ' ');
        index++;
    }
}

static void content_append_path_tail(uint8_t *data, uint64_t *length, const char *path) {
    const uint64_t path_length = text_length_bounded(path, MYOS_VFS_PATH_MAX);
    const uint64_t start = path_length > 24U ? path_length - 24U : 0U;

    if (start != 0U) { content_append_text(data, length, "...", 3U); }
    for (uint64_t index = start; index < path_length; index++) {
        content_append_char(data, length, path[index]);
    }
}

static int make_project_build_path(char *destination, const char *project_path) {
    uint64_t offset = 0U;

    if (destination == (char *)0 || project_path == (const char *)0) { return 0; }
    while (project_path[offset] != '\0' && offset + 1U < MYOS_VFS_PATH_MAX) {
        destination[offset] = project_path[offset];
        offset++;
    }
    if (project_path[offset] != '\0' || offset + 10U >= MYOS_VFS_PATH_MAX) { return 0; }
    destination[offset++] = '/';
    destination[offset++] = 'm';
    destination[offset++] = 'a';
    destination[offset++] = 'i';
    destination[offset++] = 'n';
    destination[offset++] = '.';
    destination[offset++] = 'e';
    destination[offset++] = 'l';
    destination[offset++] = 'f';
    destination[offset] = '\0';
    return 1;
}

static int make_project_package_directory(char *destination, const char *project_path) {
    uint64_t name_offset = 0U;
    uint64_t offset = 0U;

    if (destination == (char *)0 || project_path == (const char *)0) { return 0; }
    for (uint64_t index = 0U; project_path[index] != '\0'; index++) {
        if (project_path[index] == '/') { name_offset = index + 1U; }
    }
    if (project_path[name_offset] == '\0') { return 0; }
    destination[offset++] = '/';
    destination[offset++] = 'a';
    destination[offset++] = 'p';
    destination[offset++] = 'p';
    destination[offset++] = 's';
    destination[offset++] = '/';
    for (uint64_t index = name_offset; project_path[index] != '\0' && offset + 1U < MYOS_VFS_PATH_MAX; index++) {
        destination[offset++] = project_path[index];
    }
    destination[offset] = '\0';
    return project_path[name_offset] != '\0' && disk_path_is_valid(destination) != 0;
}

static int make_project_package_path(char *destination, const char *project_path) {
    char directory[MYOS_VFS_PATH_MAX] = { 0 };
    uint64_t offset = 0U;

    if (destination == (char *)0 || make_project_package_directory(directory, project_path) == 0) { return 0; }
    while (directory[offset] != '\0' && offset + 1U < MYOS_VFS_PATH_MAX) {
        destination[offset] = directory[offset];
        offset++;
    }
    if (directory[offset] != '\0' || offset + 10U >= MYOS_VFS_PATH_MAX) { return 0; }
    destination[offset++] = '/';
    destination[offset++] = 'm';
    destination[offset++] = 'a';
    destination[offset++] = 'i';
    destination[offset++] = 'n';
    destination[offset++] = '.';
    destination[offset++] = 'e';
    destination[offset++] = 'l';
    destination[offset++] = 'f';
    destination[offset] = '\0';
    return 1;
}

static void append_project_file_status(uint8_t *data, uint64_t *length, const char *label,
                                       const char *directory, const char *name) {
    struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };

    content_append_text(data, length, label, 16U);
    content_append_text(data, length, ": ", 2U);
    if (make_path(request.path, directory) != 0) {
        for (uint64_t index = 0U; index < UINT64_C(128); index++) {
            request.index = index;
            if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
                break;
            }
            if (text_equal(request.entry.name, name) != 0) {
                if (request.entry.type == MYOS_VFS_OBJECT_REGULAR) {
                    content_append_text(data, length, "READY ", 6U);
                    content_append_decimal(data, length, request.entry.size);
                    content_append_text(data, length, " bytes", 6U);
                } else {
                    content_append_text(data, length, "NOT REGULAR", 11U);
                }
                content_append_char(data, length, '\n');
                return;
            }
        }
    }
    content_append_text(data, length, "MISSING\n", 8U);
}

static int launch_project_build(const char *project_path, uint64_t *child_status) {
    struct myos_spawn_request request = { { 0 }, { 0 }, UINT64_MAX, UINT64_MAX };
    char source[MYOS_VFS_PATH_MAX] = { 0 };
    char output[MYOS_VFS_PATH_MAX] = { 0 };
    uint64_t argument_length = 0U;
    uint64_t source_index = 0U;
    uint64_t output_index = 0U;
    uint64_t child;

    if (child_status == (uint64_t *)0 || make_project_source_path(source, project_path) == 0
        || make_project_build_path(output, project_path) == 0 || project_source_is_regular(project_path) == 0
        || make_path(request.path, "/system/core/apps/asm.elf") == 0) {
        return 0;
    }
    while (source[source_index] != '\0' && argument_length + 1U < MYOS_SPAWN_ARGUMENTS_MAX) {
        request.arguments[argument_length++] = source[source_index++];
    }
    if (source[source_index] != '\0' || argument_length + 1U >= MYOS_SPAWN_ARGUMENTS_MAX) { return 0; }
    request.arguments[argument_length++] = ' ';
    while (output[output_index] != '\0' && argument_length + 1U < MYOS_SPAWN_ARGUMENTS_MAX) {
        request.arguments[argument_length++] = output[output_index++];
    }
    if (output[output_index] != '\0') { return 0; }
    request.arguments[argument_length] = '\0';
    child = system_call(MYOS_SYS_SPAWN, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));
    if (child == UINT64_MAX) { return 0; }
    (void)system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_END, 0U, 0U);
    *child_status = system_call(MYOS_SYS_WAIT, child, 0U, 0U);
    if (*child_status == UINT64_MAX) { *child_status = 1U; }
    return 1;
}

static int launch_project_run(const char *project_path, const char *arguments, uint64_t *child_status) {
    struct myos_spawn_request request = { { 0 }, { 0 }, UINT64_MAX, UINT64_MAX };
    char output[MYOS_VFS_PATH_MAX] = { 0 };
    uint64_t argument_length = 0U;
    uint64_t child;

    if (arguments == (const char *)0 || child_status == (uint64_t *)0 || make_project_build_path(output, project_path) == 0
        || project_build_is_regular(project_path) == 0 || make_path(request.path, output) == 0) {
        return 0;
    }
    while (arguments[argument_length] != '\0' && argument_length + 1U < MYOS_SPAWN_ARGUMENTS_MAX) {
        request.arguments[argument_length] = arguments[argument_length];
        argument_length++;
    }
    if (arguments[argument_length] != '\0') { return 0; }
    request.arguments[argument_length] = '\0';
    child = system_call(MYOS_SYS_SPAWN, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));
    if (child == UINT64_MAX) { return 0; }
    (void)system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_END, 0U, 0U);
    *child_status = system_call(MYOS_SYS_WAIT, child, 0U, 0U);
    if (*child_status == UINT64_MAX) { *child_status = 1U; }
    return 1;
}

static int launch_project_install(const char *project_path, uint64_t *child_status) {
    struct myos_spawn_request request = { { 0 }, { 0 }, UINT64_MAX, UINT64_MAX };
    char source[MYOS_VFS_PATH_MAX] = { 0 };
    char target[MYOS_VFS_PATH_MAX] = { 0 };
    uint64_t argument_length = 0U;
    uint64_t source_index = 0U;
    uint64_t target_index = 0U;
    uint64_t child;

    if (child_status == (uint64_t *)0 || make_project_build_path(source, project_path) == 0
        || make_project_package_path(target, project_path) == 0 || project_build_is_regular(project_path) == 0
        || make_path(request.path, "/system/core/apps/install.elf") == 0) {
        return 0;
    }
    while (source[source_index] != '\0' && argument_length + 1U < MYOS_SPAWN_ARGUMENTS_MAX) {
        request.arguments[argument_length++] = source[source_index++];
    }
    if (source[source_index] != '\0' || argument_length + 1U >= MYOS_SPAWN_ARGUMENTS_MAX) { return 0; }
    request.arguments[argument_length++] = ' ';
    while (target[target_index] != '\0' && argument_length + 1U < MYOS_SPAWN_ARGUMENTS_MAX) {
        request.arguments[argument_length++] = target[target_index++];
    }
    if (target[target_index] != '\0') { return 0; }
    request.arguments[argument_length] = '\0';
    child = system_call(MYOS_SYS_SPAWN, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));
    if (child == UINT64_MAX) { return 0; }
    (void)system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_END, 0U, 0U);
    *child_status = system_call(MYOS_SYS_WAIT, child, 0U, 0U);
    if (*child_status == UINT64_MAX) { *child_status = 1U; }
    return 1;
}

static int remove_project_workspace(const char *project_path) {
    struct myos_vfs_list_request list_request = { 0U, { 0 }, { { 0 }, 0U, 0U } };
    struct myos_vfs_path_request source_request = { { 0 } };
    struct myos_vfs_path_request directory_request = { { 0 } };
    char source_path[MYOS_VFS_PATH_MAX] = { 0 };
    int source_present = 0;

    if (project_workspace_is_directory(project_path) == 0 || make_path(list_request.path, project_path) == 0
        || make_project_source_path(source_path, project_path) == 0 || make_path(source_request.path, source_path) == 0
        || make_path(directory_request.path, project_path) == 0) {
        return 0;
    }
    for (uint64_t index = 0U; index < UINT64_C(128); index++) {
        list_request.index = index;
        if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&list_request, sizeof(list_request)) == UINT64_MAX) {
            break;
        }
        if (text_equal(list_request.entry.name, "main.elf") != 0) {
            return 0;
        }
        if (text_equal(list_request.entry.name, "main.mya") == 0 || list_request.entry.type != MYOS_VFS_OBJECT_REGULAR) {
            return 0;
        }
        source_present = 1;
    }
    if (source_present != 0
        && system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&source_request, sizeof(source_request)) == UINT64_MAX) {
        return 0;
    }
    return system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&directory_request, sizeof(directory_request)) != UINT64_MAX;
}

static int remove_project_build(const char *project_path) {
    struct myos_vfs_path_request request = { { 0 } };
    char build_path[MYOS_VFS_PATH_MAX] = { 0 };

    if (make_project_build_path(build_path, project_path) == 0 || project_build_is_regular(project_path) == 0
        || make_path(request.path, build_path) == 0) {
        return 0;
    }
    return system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) != UINT64_MAX;
}

static int remove_project_package(const char *project_path) {
    struct myos_vfs_path_request request = { { 0 } };
    char package_path[MYOS_VFS_PATH_MAX] = { 0 };

    if (make_project_package_path(package_path, project_path) == 0 || project_package_is_regular(project_path) == 0
        || make_path(request.path, package_path) == 0) {
        return 0;
    }
    return system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) != UINT64_MAX;
}

static void show_project_status(const char *project_path) {
    char package_directory[MYOS_VFS_PATH_MAX] = { 0 };
    uint64_t length = 0U;

    content_append_text(gui_scratch_data, &length, "PROJECT STATUS\n", 15U);
    content_append_path_tail(gui_scratch_data, &length, project_path);
    content_append_char(gui_scratch_data, &length, '\n');
    append_project_file_status(gui_scratch_data, &length, "source", project_path, "main.mya");
    append_project_file_status(gui_scratch_data, &length, "build", project_path, "main.elf");
    if (make_project_package_directory(package_directory, project_path) != 0) {
        append_project_file_status(gui_scratch_data, &length, "package", package_directory, "main.elf");
    } else {
        content_append_text(gui_scratch_data, &length, "package: MISSING\n", 17U);
    }
    (void)set_viewer_content("PROJECT STATUS", gui_scratch_data, length, 0U, 0U, 0U);
}

static int browser_set_directory(const char *path) {
    if (disk_path_is_valid(path) == 0) { return 0; }
    for (uint64_t index = 0U; index < MYOS_VFS_PATH_MAX; index++) {
        browser_directory[index] = path[index];
        if (path[index] == '\0') {
            browser_page = 0U;
            return 1;
        }
    }
    return 0;
}

static void browser_parent_directory(void) {
    uint64_t length = text_length_bounded(browser_directory, MYOS_VFS_PATH_MAX);

    if (length <= 1U) { return; }
    while (length > 1U && browser_directory[length - 1U] != '/') { length--; }
    if (length > 1U) { length--; }
    browser_directory[length] = '\0';
    browser_page = 0U;
}

static int browser_make_child_path(char *destination, const char *name) {
    uint64_t offset = 0U;

    if (name == (const char *)0 || name[0] == '\0') { return 0; }
    while (browser_directory[offset] != '\0' && offset + 1U < MYOS_VFS_PATH_MAX) {
        destination[offset] = browser_directory[offset];
        offset++;
    }
    if (browser_directory[offset] != '\0') { return 0; }
    if (offset != 1U) { destination[offset++] = '/'; }
    for (uint64_t index = 0U; name[index] != '\0' && offset + 1U < MYOS_VFS_PATH_MAX; index++) {
        if (name[index] == '/' || name[index] < ' ' || name[index] > '~') { return 0; }
        destination[offset++] = name[index];
    }
    destination[offset] = '\0';
    return disk_path_is_valid(destination);
}

static int browser_entry_at_index(uint64_t index, struct myos_vfs_directory_entry *entry) {
    struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };

    if (entry == (struct myos_vfs_directory_entry *)0 || make_path(request.path, browser_directory) == 0) { return 0; }
    request.index = index;
    if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) { return 0; }
    *entry = request.entry;
    return 1;
}

static int browser_entry_at(uint64_t slot, struct myos_vfs_directory_entry *entry) {
    if (slot > GUI_BROWSER_PAGE_SIZE) { return 0; }
    return browser_entry_at_index(browser_page * GUI_BROWSER_PAGE_SIZE + slot, entry);
}

static int browser_has_next_page(void) {
    struct myos_vfs_directory_entry entry;

    return browser_entry_at(GUI_BROWSER_PAGE_SIZE, &entry) != 0;
}

static int browser_name_contains_ascii(const char *name, const char *query) {
    uint64_t query_length = 0U;

    while (query[query_length] != '\0') { query_length++; }
    for (uint64_t start = 0U; name[start] != '\0'; start++) {
        uint64_t index = 0U;

        while (index < query_length && name[start + index] != '\0') {
            char left = name[start + index];
            char right = query[index];

            if (left >= 'a' && left <= 'z') { left = (char)(left - 'a' + 'A'); }
            if (right >= 'a' && right <= 'z') { right = (char)(right - 'a' + 'A'); }
            if (left != right) { break; }
            index++;
        }
        if (index == query_length) { return 1; }
    }
    return 0;
}

static void show_browser_search_results(void) {
    struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };
    uint64_t length = 0U;
    uint64_t matches = 0U;

    browser_search_results_active = 1;
    browser_search_count = 0U;
    content_append_text(gui_scratch_data, &length, "SEARCH ", 7U);
    content_append_text(gui_scratch_data, &length, browser_new_file_name, browser_new_file_length);
    content_append_text(gui_scratch_data, &length, "\n[RETURN]\n", 10U);
    if (make_path(request.path, browser_directory) != 0) {
        for (uint64_t index = 0U; index < GUI_BROWSER_COPY_ENTRY_SCAN_LIMIT; index++) {
            if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) { break; }
            if (browser_name_contains_ascii(request.entry.name, browser_new_file_name) != 0) {
                if (browser_search_count < MYOS_GUI_BROWSER_ENTRY_MAX) {
                    browser_search_indices[browser_search_count++] = request.index;
                }
                matches++;
            }
            request.index++;
        }
    }
    content_append_text(gui_scratch_data, &length, matches == 0U ? "NO MATCHES\n" : "MATCHES\n", matches == 0U ? 11U : 8U);
    for (uint64_t slot = 0U; slot < MYOS_GUI_BROWSER_ENTRY_MAX; slot++) {
        struct myos_vfs_directory_entry entry;
        if (slot < browser_search_count && browser_entry_at_index(browser_search_indices[slot], &entry) != 0) {
            const char prefix = entry.type == MYOS_VFS_OBJECT_DIRECTORY ? 'D' : (entry.type == MYOS_VFS_OBJECT_REGULAR ? 'F' : 'V');
            content_append_char(gui_scratch_data, &length, prefix);
            content_append_char(gui_scratch_data, &length, ' ');
            content_append_browser_name(gui_scratch_data, &length, entry.name);
            content_append_char(gui_scratch_data, &length, ' ');
            content_append_decimal(gui_scratch_data, &length, entry.size);
            content_append_char(gui_scratch_data, &length, 'B');
        } else { content_append_char(gui_scratch_data, &length, '-'); }
        content_append_char(gui_scratch_data, &length, '\n');
    }
    content_append_text(gui_scratch_data, &length, "-", 1U);
    (void)set_viewer_content(browser_directory, gui_scratch_data, length, MYOS_GUI_CONTENT_FLAG_BROWSER, 0U, 0U);
}

static void show_file_browser(void) {
    uint64_t length = 0U;

    content_append_text(gui_scratch_data, &length, "PATH ", 5U);
    content_append_path_tail(gui_scratch_data, &length, browser_directory);
    content_append_char(gui_scratch_data, &length, '\n');
    content_append_text(gui_scratch_data, &length, "[..]\n", 5U);
    content_append_text(gui_scratch_data, &length, "[PREV]\n", 7U);
    for (uint64_t slot = 0U; slot < GUI_BROWSER_PAGE_SIZE; slot++) {
        struct myos_vfs_directory_entry entry;

        if (browser_entry_at(slot, &entry) != 0) {
            const char prefix = entry.type == MYOS_VFS_OBJECT_DIRECTORY ? 'D'
                : (entry.type == MYOS_VFS_OBJECT_REGULAR ? 'F'
                   : (entry.type == MYOS_VFS_OBJECT_SYMBOLIC_LINK ? 'L' : 'V'));
            content_append_char(gui_scratch_data, &length, prefix);
            content_append_char(gui_scratch_data, &length, ' ');
            content_append_browser_name(gui_scratch_data, &length, entry.name);
            content_append_char(gui_scratch_data, &length, ' ');
            content_append_decimal(gui_scratch_data, &length, entry.size);
            content_append_char(gui_scratch_data, &length, 'B');
        } else {
            content_append_text(gui_scratch_data, &length, "-", 1U);
        }
        content_append_char(gui_scratch_data, &length, '\n');
    }
    content_append_text(gui_scratch_data, &length, browser_has_next_page() != 0 ? "[NEXT]" : "-", 6U);
    content_append_char(gui_scratch_data, &length, '\n');
    {
        static const char browser_actions[] = "[NEW FILE]\n[NEW FOLDER]\n[DELETE]\n[COPY]\n[RENAME]\n[MOVE]\n[SEARCH]";

        content_append_text(gui_scratch_data, &length, browser_actions, sizeof(browser_actions) - 1U);
    }
    (void)set_viewer_content(browser_directory, gui_scratch_data, length, MYOS_GUI_CONTENT_FLAG_BROWSER, 0U, 0U);
}

static int browser_path_is_writable(const char *path) {
    return path_matches_root(path, "/users/myos") != 0
        || path_matches_root(path, "/temp") != 0
        || path_matches_root(path, "/system/data") != 0
        || path_matches_root(path, "/system/config") != 0;
}
static int browser_directory_is_writable(void) {
    return browser_path_is_writable(browser_directory);
}

static void show_browser_new_file_prompt(void) {
    uint64_t length = 0U;

    if (browser_prompt_is_search != 0) {
        content_append_text(gui_scratch_data, &length, "SEARCH\nNAME: ", 13U);
    } else if (browser_prompt_is_copy != 0) {
        content_append_text(gui_scratch_data, &length,
                            browser_copy_prompt_for_target != 0 ? "COPY\nTO: " : "COPY\nFROM: ",
                            browser_copy_prompt_for_target != 0 ? 9U : 11U);
    } else if (browser_prompt_is_rename != 0) {
        content_append_text(gui_scratch_data, &length,
                            browser_rename_prompt_for_target != 0 ? "RENAME\nTO: " : "RENAME\nFROM: ",
                            browser_rename_prompt_for_target != 0 ? 11U : 13U);
    } else if (browser_prompt_is_move != 0) {
        content_append_text(gui_scratch_data, &length,
                            browser_move_prompt_for_target != 0 ? "MOVE\nTO DIR: " : "MOVE\nFROM: ",
                            browser_move_prompt_for_target != 0 ? 13U : 11U);
    } else if (browser_prompt_is_remove != 0 && browser_remove_confirmation != 0) {
        content_append_text(gui_scratch_data, &length, "DELETE ", 7U);
        for (uint64_t index = 0U; index < browser_new_file_length; index++) {
            content_append_char(gui_scratch_data, &length, browser_new_file_name[index]);
        }
        content_append_text(gui_scratch_data, &length, "\nENTER TO CONFIRM", 17U);
    } else if (browser_prompt_is_remove != 0) {
        content_append_text(gui_scratch_data, &length, "DELETE\nNAME: ", 13U);
    } else if (browser_new_entry_is_directory != 0) {
        content_append_text(gui_scratch_data, &length, "NEW FOLDER\nNAME: ", 17U);
    } else {
        content_append_text(gui_scratch_data, &length, "NEW FILE\nNAME: ", 15U);
    }
    if (browser_prompt_is_move != 0 && browser_move_prompt_for_target != 0) {
        for (uint64_t index = 0U; index < browser_move_target_length; index++) {
            content_append_char(gui_scratch_data, &length, browser_move_target_directory[index]);
        }
    } else {
        for (uint64_t index = 0U; index < browser_new_file_length; index++) {
            content_append_char(gui_scratch_data, &length, browser_new_file_name[index]);
        }
    }
    (void)set_viewer_content(browser_directory, gui_scratch_data, length, MYOS_GUI_CONTENT_FLAG_EDITABLE, length, 0U);
}

static int browser_create_empty_entry(void) {
    struct myos_vfs_path_request request = { { 0 } };
    char path[MYOS_VFS_PATH_MAX] = { 0 };

    if (browser_new_file_length == 0U || browser_directory_is_writable() == 0
        || browser_make_child_path(path, browser_new_file_name) == 0 || make_path(request.path, path) == 0) {
        return 0;
    }
    if (browser_new_entry_is_directory != 0) {
        return system_call(MYOS_SYS_VFS_CREATE_DIRECTORY, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) != UINT64_MAX;
    }
    if (select_disk_path(path) == 0) { return 0; }
    return system_call(MYOS_SYS_VFS_CREATE_FILE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) != UINT64_MAX;
}

static int browser_remove_named_entry(void) {
    struct myos_vfs_path_request request = { { 0 } };
    char path[MYOS_VFS_PATH_MAX] = { 0 };

    if (browser_new_file_length == 0U || browser_directory_is_writable() == 0
        || browser_make_child_path(path, browser_new_file_name) == 0 || make_path(request.path, path) == 0) {
        return 0;
    }
    return system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) != UINT64_MAX;
}
static int browser_rename_named_object(void) {
    struct myos_vfs_rename_request request = { { 0 }, { 0 } };
    char source_path[MYOS_VFS_PATH_MAX] = { 0 };
    char target_path[MYOS_VFS_PATH_MAX] = { 0 };

    if (browser_directory_is_writable() == 0 || browser_rename_source_length == 0U
        || browser_new_file_length == 0U
        || browser_make_child_path(source_path, browser_rename_source_name) == 0
        || browser_make_child_path(target_path, browser_new_file_name) == 0
        || make_path(request.source, source_path) == 0 || make_path(request.target, target_path) == 0) {
        return 0;
    }
    return system_call(MYOS_SYS_VFS_RENAME, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) != UINT64_MAX;
}

static int browser_move_named_file(void) {
    struct myos_vfs_rename_request request = { { 0 }, { 0 } };
    char source_path[MYOS_VFS_PATH_MAX] = { 0 };
    char target_path[MYOS_VFS_PATH_MAX] = { 0 };
    uint64_t target_offset;

    if (browser_directory_is_writable() == 0 || browser_move_source_length == 0U
        || browser_move_target_length == 0U || disk_path_is_valid(browser_move_target_directory) == 0
        || browser_path_is_writable(browser_move_target_directory) == 0
        || browser_make_child_path(source_path, browser_move_source_name) == 0
        || make_path(target_path, browser_move_target_directory) == 0) {
        return 0;
    }
    target_offset = text_length_bounded(target_path, sizeof(target_path));
    if (target_offset == 0U
        || target_offset + (target_offset != 1U ? 1U : 0U) + browser_move_source_length + 1U > sizeof(target_path)) {
        return 0;
    }
    if (target_offset != 1U) { target_path[target_offset++] = '/'; }
    for (uint64_t index = 0U; index < browser_move_source_length; index++) {
        target_path[target_offset++] = browser_move_source_name[index];
    }
    target_path[target_offset] = '\0';
    if (disk_path_is_valid(target_path) == 0
        || make_path(request.source, source_path) == 0 || make_path(request.target, target_path) == 0) {
        return 0;
    }
    return system_call(MYOS_SYS_VFS_MOVE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) != UINT64_MAX;
}

static int browser_names_equal_ascii(const char *left, const char *right) {
    uint64_t index = 0U;

    while (left[index] != '\0' && right[index] != '\0') {
        char left_character = left[index];
        char right_character = right[index];

        if (left_character >= 'a' && left_character <= 'z') { left_character = (char)(left_character - 'a' + 'A'); }
        if (right_character >= 'a' && right_character <= 'z') { right_character = (char)(right_character - 'a' + 'A'); }
        if (left_character != right_character) { return 0; }
        index++;
    }
    return left[index] == right[index];
}

static int browser_copy_source_entry(struct myos_vfs_directory_entry *entry) {
    struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };

    if (entry == (struct myos_vfs_directory_entry *)0 || browser_copy_source_length == 0U
        || make_path(request.path, browser_directory) == 0) {
        return 0;
    }
    for (uint64_t index = 0U; index < GUI_BROWSER_COPY_ENTRY_SCAN_LIMIT; index++) {
        request.index = index;
        if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
            return 0;
        }
        if (browser_names_equal_ascii(request.entry.name, browser_copy_source_name) != 0) {
            if (request.entry.type != MYOS_VFS_OBJECT_REGULAR) { return 0; }
            *entry = request.entry;
            return 1;
        }
    }
    return 0;
}

static int browser_copy_named_file(void) {
    struct myos_vfs_directory_entry source_entry;
    struct myos_vfs_read_request read_request = { 0U, { 0 }, { 0 } };
    struct myos_vfs_write_request write_request = { 0U, 0U, { 0 }, { 0 } };
    struct myos_vfs_path_request remove_request = { { 0 } };
    char source_path[MYOS_VFS_PATH_MAX] = { 0 };
    char target_path[MYOS_VFS_PATH_MAX] = { 0 };
    uint64_t copied = 0U;
    int target_created = 0;

    if (browser_directory_is_writable() == 0 || browser_copy_source_length == 0U || browser_new_file_length == 0U
        || browser_copy_source_entry(&source_entry) == 0 || source_entry.size > GUI_BROWSER_COPY_FILE_MAX
        || browser_make_child_path(source_path, browser_copy_source_name) == 0
        || browser_make_child_path(target_path, browser_new_file_name) == 0
        || make_path(read_request.path, source_path) == 0 || make_path(write_request.path, target_path) == 0
        || make_path(remove_request.path, target_path) == 0) {
        return 0;
    }
    if (system_call(MYOS_SYS_VFS_CREATE_FILE, 0U, (uint64_t)(uintptr_t)&remove_request, sizeof(remove_request)) == UINT64_MAX) {
        return 0;
    }
    target_created = 1;
    while (copied < source_entry.size) {
        const uint64_t wanted = source_entry.size - copied > MYOS_VFS_READ_CHUNK
            ? MYOS_VFS_READ_CHUNK : source_entry.size - copied;
        uint64_t count;

        read_request.offset = copied;
        count = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&read_request, sizeof(read_request));
        if (count == UINT64_MAX || count != wanted) { break; }
        write_request.offset = copied;
        write_request.length = count;
        for (uint64_t index = 0U; index < count; index++) { write_request.data[index] = read_request.data[index]; }
        if (system_call(MYOS_SYS_VFS_WRITE, 0U, (uint64_t)(uintptr_t)&write_request, sizeof(write_request)) != count) { break; }
        copied += count;
    }
    if (copied == source_entry.size) { return 1; }
    if (target_created != 0) {
        (void)system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&remove_request, sizeof(remove_request));
    }
    return 0;
}

static int browser_open_entry(uint8_t action) {
    struct myos_vfs_directory_entry entry;
    char path[MYOS_VFS_PATH_MAX] = { 0 };
    const uint64_t slot = action - MYOS_INPUT_GUI_ACTION_BROWSER_ENTRY_BASE;

    if (action < MYOS_INPUT_GUI_ACTION_BROWSER_ENTRY_BASE || slot >= GUI_BROWSER_PAGE_SIZE
        || (browser_search_results_active != 0
            ? (slot >= browser_search_count || browser_entry_at_index(browser_search_indices[slot], &entry) == 0)
            : browser_entry_at(slot, &entry) == 0)
        || browser_make_child_path(path, entry.name) == 0) {
        return 0;
    }
    browser_search_results_active = 0;
    if (entry.type == MYOS_VFS_OBJECT_DIRECTORY) {
        if (browser_set_directory(path) == 0) { return 0; }
        show_file_browser();
        return GUI_BROWSER_RESULT_OPENED;
    }
    if (entry.type == MYOS_VFS_OBJECT_REGULAR || entry.type == MYOS_VFS_OBJECT_VIRTUAL) {
        if (select_disk_path(path) == 0) { return 0; }
        if (selected_disk_path_is_writable() != 0) {
            const int editor_result = edit_selected_disk_file();

            return editor_result == GUI_EDITOR_RESULT_EXIT ? GUI_EDITOR_RESULT_EXIT : 1;
        }
        load_viewer_file(path);
        return 1;
    }
    return 0;
}

static int make_note_path(char *destination, const char *name) {
    uint64_t offset = 0U;

    while (GUI_NOTE_DIRECTORY[offset] != '\0') { destination[offset] = GUI_NOTE_DIRECTORY[offset]; offset++; }
    destination[offset++] = '/';
    for (uint64_t index = 0U; name[index] != '\0' && offset + 1U < GUI_NOTE_PATH_CAPACITY; index++) {
        destination[offset++] = name[index];
    }
    destination[offset] = '\0';
    return disk_path_is_valid(destination);
}

static int select_next_disk_file(void) {
    char first_path[GUI_NOTE_PATH_CAPACITY] = { 0 };
    int have_first = 0;
    int select_following = 0;

    for (uint64_t index = 0U; index < GUI_VFS_ENTRY_SCAN_LIMIT; index++) {
        struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };
        char candidate[GUI_NOTE_PATH_CAPACITY] = { 0 };

        if (make_path(request.path, GUI_NOTE_DIRECTORY) == 0) { return 0; }
        request.index = index;
        if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
            break;
        }
        if (request.entry.type != MYOS_VFS_OBJECT_REGULAR || make_note_path(candidate, request.entry.name) == 0) {
            continue;
        }
        if (have_first == 0) {
            for (uint64_t character = 0U; character < GUI_NOTE_PATH_CAPACITY; character++) {
                first_path[character] = candidate[character];
                if (candidate[character] == '\0') { break; }
            }
            have_first = 1;
        }
        if (select_following != 0) { return select_disk_path(candidate); }
        if (text_equal(candidate, selected_disk_path) != 0) { select_following = 1; }
    }
    return have_first != 0 ? select_disk_path(first_path) : 0;
}

static void load_viewer_file(const char *path) {
    char title[MYOS_GUI_CONTENT_TITLE_MAX] = { 0 };
    const uint64_t count = read_viewer_file(path, gui_scratch_data, sizeof(gui_scratch_data));

    if (count == GUI_BROWSER_READ_TOO_LARGE) {
        set_viewer_status("FILE TOO LARGE FOR GUI");
        return;
    }
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
    if (set_viewer_content(title, gui_scratch_data, count, 0U, 0U, 0U) == 0) {
        set_viewer_status("VIEWER UPDATE FAILED");
    }
}

static int save_selected_disk_file(const uint8_t *data, uint64_t length) {
    struct myos_vfs_path_request path_request = { { 0 } };
    struct myos_vfs_write_request write_request = { 0U, 0U, { 0 }, { 0 } };

    if (length > MYOS_GUI_CONTENT_MAX || selected_disk_path_is_writable() == 0
        || make_path(path_request.path, selected_disk_path) == 0
        || make_path(write_request.path, selected_disk_path) == 0) {
        return 0;
    }
    (void)system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&path_request, sizeof(path_request));
    if (system_call(MYOS_SYS_VFS_CREATE_FILE, 0U, (uint64_t)(uintptr_t)&path_request, sizeof(path_request)) == UINT64_MAX) {
        return 0;
    }
    for (uint64_t offset = 0U; offset < length; offset += MYOS_VFS_READ_CHUNK) {
        const uint64_t chunk = length - offset > MYOS_VFS_READ_CHUNK ? MYOS_VFS_READ_CHUNK : length - offset;

        write_request.offset = offset;
        write_request.length = chunk;
        for (uint64_t index = 0U; index < chunk; index++) {
            write_request.data[index] = data[offset + index];
        }
        if (system_call(MYOS_SYS_VFS_WRITE, 0U, (uint64_t)(uintptr_t)&write_request, sizeof(write_request)) == UINT64_MAX) {
            return 0;
        }
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

static int edit_selected_disk_file(void) {
    uint64_t length = read_viewer_file(selected_disk_path, gui_editor_data, sizeof(gui_editor_data));
    uint64_t cursor;

    if (selected_disk_path_is_writable() == 0) {
        set_viewer_status("READ ONLY FILE");
        return GUI_EDITOR_RESULT_VIEWER;
    }
    if (length == GUI_BROWSER_READ_TOO_LARGE) {
        set_viewer_status("FILE TOO LARGE FOR GUI");
        return GUI_EDITOR_RESULT_VIEWER;
    }
    if (length == UINT64_MAX) {
        length = 0U;
    }
    cursor = length;
    for (;;) {
        char character;
        uint64_t read_result;
        const uint64_t viewport = editor_viewport_for_cursor(gui_editor_data, length, cursor);

        if (set_viewer_content("EDIT NOTE", gui_editor_data, length, MYOS_GUI_CONTENT_FLAG_EDITABLE, cursor, viewport) == 0) {
            return GUI_EDITOR_RESULT_VIEWER;
        }
        read_result = system_call(MYOS_SYS_READ, 0U, (uint64_t)(uintptr_t)&character, 1U);
        if (read_result == UINT64_MAX || read_result == 0U) {
            continue;
        }
        if ((uint8_t)character == MYOS_INPUT_KEY_CTRL_Q
            || (uint8_t)character == MYOS_INPUT_GUI_ACTION_EXIT) {
            return GUI_EDITOR_RESULT_EXIT;
        }
        if ((uint8_t)character == MYOS_INPUT_KEY_ALT_F4) {
            const uint64_t action = system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_INPUT,
                                                (uint64_t)(uint8_t)character, 0U);

            if (action == (uint64_t)(uint8_t)'\x1b') {
                load_viewer_file(selected_disk_path);
                return GUI_EDITOR_RESULT_VIEWER;
            }
            if (action == MYOS_INPUT_GUI_ACTION_HOME) {
                return GUI_EDITOR_RESULT_HOME;
            }
            continue;
        }
        if (character == '\x1b') {
            load_viewer_file(selected_disk_path);
            return GUI_EDITOR_RESULT_VIEWER;
        }
        if ((uint8_t)character == UINT8_C(0x13)) {
            if (save_selected_disk_file(gui_editor_data, length) != 0) {
                load_viewer_file(selected_disk_path);
            } else {
                set_viewer_status("SAVE FAILED");
            }
            return GUI_EDITOR_RESULT_VIEWER;
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
            cursor = editor_line_start(gui_editor_data, length, cursor);
        } else if ((uint8_t)character == MYOS_INPUT_KEY_END) {
            cursor = editor_line_end(gui_editor_data, length, editor_line_start(gui_editor_data, length, cursor));
        } else if ((uint8_t)character == MYOS_INPUT_KEY_UP) {
            const uint64_t current_start = editor_line_start(gui_editor_data, length, cursor);

            if (current_start != 0U) {
                const uint64_t previous_start = editor_line_start(gui_editor_data, length, current_start - 1U);
                const uint64_t previous_end = editor_line_end(gui_editor_data, length, previous_start);
                const uint64_t column = cursor - current_start;
                const uint64_t previous_length = previous_end - previous_start;

                cursor = previous_start + (column < previous_length ? column : previous_length);
            }
        } else if ((uint8_t)character == MYOS_INPUT_KEY_DOWN) {
            const uint64_t current_start = editor_line_start(gui_editor_data, length, cursor);
            const uint64_t current_end = editor_line_end(gui_editor_data, length, current_start);

            if (current_end < length) {
                const uint64_t next_start = current_end + 1U;
                const uint64_t next_end = editor_line_end(gui_editor_data, length, next_start);
                const uint64_t column = cursor - current_start;
                const uint64_t next_length = next_end - next_start;

                cursor = next_start + (column < next_length ? column : next_length);
            }
        } else if ((uint8_t)character == MYOS_INPUT_KEY_DELETE) {
            editor_delete_at_cursor(gui_editor_data, &length, cursor);
        } else if (character == '\b' || (uint8_t)character == UINT8_C(0x7F)) {
            if (cursor != 0U) {
                cursor--;
                editor_delete_at_cursor(gui_editor_data, &length, cursor);
            }
        } else if (character == '\r' || character == '\n') {
            editor_insert_byte(gui_editor_data, &length, &cursor, (uint8_t)'\n');
        } else if ((uint8_t)character >= 32U && (uint8_t)character <= 126U) {
            editor_insert_byte(gui_editor_data, &length, &cursor, (uint8_t)character);
        }
    }
}

void _start(uint64_t argc, const char *arguments) {
    char project_path[MYOS_VFS_PATH_MAX] = { 0 };
    char project_source_path[MYOS_VFS_PATH_MAX] = { 0 };
    char project_run_arguments[MYOS_SPAWN_ARGUMENTS_MAX] = { 0 };
    uint64_t status = 0U;
    int home_mode = arguments[0] == '\0' || text_equal(arguments, "home");
    const int project_mode = text_equal(arguments, "projects");
    const int direct_project_request = project_argument_requested(arguments);
    int direct_project_view = 0;
    const int direct_project_mode = make_project_workspace_path(project_path, arguments, &direct_project_view,
                                                                 project_run_arguments);
    int browser_mode = 0;
    int browser_new_file_mode = 0;
    int session_finished = 0;
    const char *initial_path = arguments;

    (void)argc;
    if (system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_BEGIN, 0U, 0U) == UINT64_MAX) {
        status = 1U;
    } else {
        if (home_mode != 0) {
            show_desktop_home();
        } else if (project_mode != 0 || direct_project_mode != 0) {
            browser_mode = 1;
            (void)browser_set_directory(direct_project_mode != 0 ? project_path : GUI_BROWSER_PROJECT_PATH);
            if (direct_project_mode != 0 && direct_project_view == 1) {
                if (make_project_source_path(project_source_path, project_path) == 0
                    || project_source_is_regular(project_path) == 0 || select_disk_path(project_source_path) == 0) {
                    set_viewer_status("UNABLE TO OPEN PROJECT SOURCE");
                } else {
                    const int editor_result = edit_selected_disk_file();

                    if (editor_result == GUI_EDITOR_RESULT_EXIT) {
                        (void)system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_END, 0U, 0U);
                        session_finished = 1;
                    } else if (editor_result == GUI_EDITOR_RESULT_HOME) {
                        show_file_browser();
                    }
                }
            } else if (direct_project_mode != 0 && direct_project_view == 2) {
                show_project_status(project_path);
            } else if (direct_project_mode != 0 && direct_project_view == 3) {
                if (project_source_is_regular(project_path) == 0) {
                    set_viewer_status("UNABLE TO OPEN PROJECT SOURCE");
                } else if (launch_project_build(project_path, &status) != 0) {
                    session_finished = 1;
                } else {
                    set_viewer_status("UNABLE TO BUILD PROJECT");
                }
            } else if (direct_project_mode != 0 && direct_project_view == 4) {
                if (project_build_is_regular(project_path) == 0) {
                    set_viewer_status("UNABLE TO OPEN PROJECT OUTPUT");
                } else if (launch_project_run(project_path, project_run_arguments, &status) != 0) {
                    session_finished = 1;
                } else {
                    set_viewer_status("UNABLE TO RUN PROJECT");
                }
            } else if (direct_project_mode != 0 && direct_project_view == 5) {
                if (project_build_is_regular(project_path) == 0) {
                    set_viewer_status("UNABLE TO OPEN PROJECT OUTPUT");
                } else if (launch_project_install(project_path, &status) != 0) {
                    session_finished = 1;
                } else {
                    set_viewer_status("UNABLE TO INSTALL PROJECT");
                }
            } else if (direct_project_mode != 0 && direct_project_view == 6) {
                if (project_package_is_regular(project_path) == 0) {
                    set_viewer_status("UNABLE TO OPEN PROJECT PACKAGE");
                } else if (remove_project_package(project_path) != 0) {
                    set_viewer_status("PROJECT PACKAGE REMOVED");
                } else {
                    set_viewer_status("UNABLE TO REMOVE PROJECT PACKAGE");
                }
            } else if (direct_project_mode != 0 && direct_project_view == 7) {
                if (project_build_is_regular(project_path) == 0) {
                    set_viewer_status("UNABLE TO OPEN PROJECT OUTPUT");
                } else if (remove_project_build(project_path) != 0) {
                    set_viewer_status("PROJECT BUILD REMOVED");
                } else {
                    set_viewer_status("UNABLE TO REMOVE PROJECT BUILD");
                }
            } else if (direct_project_mode != 0 && direct_project_view == 8) {
                if (remove_project_workspace(project_path) != 0) {
                    set_viewer_status("PROJECT WORKSPACE REMOVED");
                } else {
                    set_viewer_status("UNABLE TO REMOVE PROJECT");
                }
            } else {
                show_file_browser();
            }
        } else if (direct_project_request != 0) {
            home_mode = 0;
            set_viewer_status("UNABLE TO OPEN PROJECT");
        } else {
            if (disk_path_is_valid(initial_path) != 0) {
                (void)select_disk_path(initial_path);
            }
            load_viewer_file(initial_path);
        }
        while (session_finished == 0) {
            char character;
            const uint64_t read_result = system_call(MYOS_SYS_READ, 0U, (uint64_t)(uintptr_t)&character, 1U);

            if (read_result == UINT64_MAX || read_result == 0U) {
                continue;
            }
            if ((uint8_t)character == MYOS_INPUT_KEY_CTRL_Q
                || (uint8_t)character == MYOS_INPUT_GUI_ACTION_EXIT) {
                (void)system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_END, 0U, 0U);
                break;
            }
            if (browser_new_file_mode != 0) {
                if (character == '\x1b' || (uint8_t)character == MYOS_INPUT_GUI_ACTION_HOME) {
                    browser_new_file_mode = 0;
                    show_file_browser();
                } else if ((uint8_t)character == MYOS_INPUT_KEY_ALT_F4) {
                    const uint64_t action = system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_INPUT,
                                                        (uint64_t)(uint8_t)character, 0U);

                    if (action == (uint64_t)(uint8_t)'\x1b' || action == MYOS_INPUT_GUI_ACTION_HOME) {
                        browser_new_file_mode = 0;
                        show_file_browser();
                    }
                } else if (character == '\r' || character == '\n') {
                    const int create_directory = browser_new_entry_is_directory;
                    const int remove_entry = browser_prompt_is_remove;
                    const int copy_entry = browser_prompt_is_copy;
                    const int rename_entry = browser_prompt_is_rename;
                    const int move_entry = browser_prompt_is_move;
                    const int search_entry = browser_prompt_is_search;
                    int editor_result;

                    if (search_entry != 0) {
                        browser_new_file_mode = 0;
                        browser_prompt_is_search = 0;
                        if (browser_new_file_length == 0U) {
                            set_viewer_status("UNABLE TO SEARCH");
                        } else {
                            show_browser_search_results();
                        }
                        continue;
                    }

                    if (copy_entry != 0 && browser_copy_prompt_for_target == 0) {
                        if (browser_new_file_length == 0U) {
                            browser_new_file_mode = 0;
                            browser_prompt_is_copy = 0;
                            set_viewer_status("UNABLE TO COPY");
                            continue;
                        }
                        for (uint64_t index = 0U; index <= browser_new_file_length; index++) {
                            browser_copy_source_name[index] = browser_new_file_name[index];
                        }
                        browser_copy_source_length = browser_new_file_length;
                        browser_new_file_length = 0U;
                        browser_new_file_name[0] = '\0';
                        browser_copy_prompt_for_target = 1;
                        show_browser_new_file_prompt();
                        continue;
                    }
                    if (rename_entry != 0 && browser_rename_prompt_for_target == 0) {
                        if (browser_new_file_length == 0U) {
                            browser_new_file_mode = 0;
                            browser_prompt_is_rename = 0;
                            set_viewer_status("UNABLE TO RENAME");
                            continue;
                        }
                        for (uint64_t index = 0U; index <= browser_new_file_length; index++) {
                            browser_rename_source_name[index] = browser_new_file_name[index];
                        }
                        browser_rename_source_length = browser_new_file_length;
                        browser_new_file_length = 0U;
                        browser_new_file_name[0] = '\0';
                        browser_rename_prompt_for_target = 1;
                        show_browser_new_file_prompt();
                        continue;
                    }
                    if (move_entry != 0 && browser_move_prompt_for_target == 0) {
                        if (browser_new_file_length == 0U) {
                            browser_new_file_mode = 0;
                            browser_prompt_is_move = 0;
                            set_viewer_status("UNABLE TO MOVE");
                            continue;
                        }
                        for (uint64_t index = 0U; index <= browser_new_file_length; index++) {
                            browser_move_source_name[index] = browser_new_file_name[index];
                        }
                        browser_move_source_length = browser_new_file_length;
                        browser_new_file_length = 0U;
                        browser_new_file_name[0] = '\0';
                        browser_move_target_length = 0U;
                        browser_move_target_directory[0] = '\0';
                        browser_move_prompt_for_target = 1;
                        show_browser_new_file_prompt();
                        continue;
                    }
                    if (remove_entry != 0 && browser_remove_confirmation == 0) {
                        if (browser_new_file_length == 0U || browser_directory_is_writable() == 0) {
                            browser_new_file_mode = 0;
                            set_viewer_status("UNABLE TO DELETE");
                            continue;
                        }
                        browser_remove_confirmation = 1;
                        show_browser_new_file_prompt();
                        continue;
                    }
                    browser_new_file_mode = 0;
                    browser_remove_confirmation = 0;
                    browser_prompt_is_copy = 0;
                    browser_prompt_is_rename = 0;
                    browser_rename_prompt_for_target = 0;
                    browser_prompt_is_move = 0;
                    browser_move_prompt_for_target = 0;
                    browser_prompt_is_search = 0;
                    if ((copy_entry != 0 ? browser_copy_named_file()
                         : (rename_entry != 0 ? browser_rename_named_object()
                            : (move_entry != 0 ? browser_move_named_file()
                               : (remove_entry != 0 ? browser_remove_named_entry() : browser_create_empty_entry())))) == 0) {
                        set_viewer_status(copy_entry != 0 ? "UNABLE TO COPY"
                                          : (rename_entry != 0 ? "UNABLE TO RENAME"
                                             : (move_entry != 0 ? "UNABLE TO MOVE"
                                                : (remove_entry != 0 ? "UNABLE TO DELETE"
                                                   : (create_directory != 0 ? "UNABLE TO CREATE DIRECTORY" : "UNABLE TO CREATE FILE")))));
                        continue;
                    }
                    if (copy_entry != 0 || rename_entry != 0 || move_entry != 0 || remove_entry != 0 || create_directory != 0) {
                        home_mode = 0;
                        show_file_browser();
                        continue;
                    }
                    editor_result = edit_selected_disk_file();
                    if (editor_result == GUI_EDITOR_RESULT_EXIT) {
                        (void)system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_END, 0U, 0U);
                        break;
                    }
                    if (editor_result == GUI_EDITOR_RESULT_HOME) {
                        home_mode = 0;
                        show_file_browser();
                    } else {
                        home_mode = 0;
                    }
                } else if (browser_remove_confirmation == 0
                           && (character == '\b' || (uint8_t)character == UINT8_C(0x7F))) {
                    if (browser_prompt_is_move != 0 && browser_move_prompt_for_target != 0) {
                        if (browser_move_target_length != 0U) {
                            browser_move_target_length--;
                            browser_move_target_directory[browser_move_target_length] = '\0';
                        }
                    } else if (browser_new_file_length != 0U) {
                        browser_new_file_length--;
                        browser_new_file_name[browser_new_file_length] = '\0';
                    }
                    show_browser_new_file_prompt();
                } else if (browser_remove_confirmation == 0 && browser_prompt_is_move != 0
                           && browser_move_prompt_for_target != 0 && (uint8_t)character >= 32U
                           && (uint8_t)character <= 126U
                           && browser_move_target_length + 1U < sizeof(browser_move_target_directory)) {
                    browser_move_target_directory[browser_move_target_length++] = character;
                    browser_move_target_directory[browser_move_target_length] = '\0';
                    show_browser_new_file_prompt();
                } else if (browser_remove_confirmation == 0 && (uint8_t)character >= 32U
                           && (uint8_t)character <= 126U && character != '/'
                           && browser_new_file_length + 1U < sizeof(browser_new_file_name)) {
                    browser_new_file_name[browser_new_file_length++] = character;
                    browser_new_file_name[browser_new_file_length] = '\0';
                    show_browser_new_file_prompt();
                }
            } else if ((uint8_t)character == MYOS_INPUT_KEY_ALT_F4) {
                const uint64_t action = system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_INPUT,
                                                    (uint64_t)(uint8_t)character, 0U);

                if (action == MYOS_INPUT_GUI_ACTION_HOME) {
                    if (browser_mode != 0) {
                        home_mode = 0;
                        show_file_browser();
                    } else {
                        home_mode = 1;
                        show_desktop_home();
                    }
                }
            } else if (character == '\x1b' && home_mode == 0) {
                if (browser_mode != 0) {
                    browser_search_results_active = 0;
                    show_file_browser();
                } else {
                    home_mode = 1;
                    show_desktop_home();
                }
            } else if ((uint8_t)character == MYOS_INPUT_GUI_ACTION_HOME) {
                if (browser_mode != 0) {
                    home_mode = 0;
                    show_file_browser();
                } else {
                    home_mode = 1;
                    show_desktop_home();
                }
            } else if ((uint8_t)character >= MYOS_INPUT_GUI_ACTION_APP_BASE
                       && (uint8_t)character - MYOS_INPUT_GUI_ACTION_APP_BASE < MYOS_GUI_LAUNCHER_APP_MAX) {
                if (launch_launcher_app((uint8_t)character) != 0) {
                    break;
                }
                home_mode = 0;
                set_viewer_status("APP LAUNCH FAILED");
            } else if ((uint8_t)character == MYOS_INPUT_GUI_ACTION_EDITOR) {
                const int editor_result = edit_selected_disk_file();

                if (editor_result == GUI_EDITOR_RESULT_EXIT) {
                    (void)system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_END, 0U, 0U);
                    break;
                }
                if (editor_result == GUI_EDITOR_RESULT_HOME) {
                    if (browser_mode != 0) {
                        home_mode = 0;
                        show_file_browser();
                    } else {
                        home_mode = 1;
                        show_desktop_home();
                    }
                } else {
                    home_mode = 0;
                }
            } else if ((uint8_t)character == MYOS_INPUT_GUI_ACTION_SYSTEM) {
                browser_mode = 0;
                home_mode = 0;
                load_viewer_file("/system/core/resources/motd.txt");
            } else if ((uint8_t)character == MYOS_INPUT_GUI_ACTION_NOTES) {
                browser_mode = 0;
                home_mode = 0;
                if (select_next_disk_file() != 0) {
                    load_viewer_file(selected_disk_path);
                } else {
                    set_viewer_status("NO NOTES");
                }
            } else if ((uint8_t)character == MYOS_INPUT_GUI_ACTION_FILES) {
                browser_mode = 1;
                home_mode = 0;
                (void)browser_set_directory(GUI_BROWSER_START_PATH);
                show_file_browser();
            } else if ((uint8_t)character == MYOS_INPUT_GUI_ACTION_BROWSER_PARENT) {
                if (browser_mode != 0) {
                    if (browser_search_results_active != 0) {
                        browser_search_results_active = 0;
                    } else {
                        browser_parent_directory();
                    }
                    show_file_browser();
                }
            } else if ((uint8_t)character == MYOS_INPUT_GUI_ACTION_BROWSER_PREVIOUS) {
                if (browser_mode != 0) {
                    if (browser_page != 0U) { browser_page--; }
                    show_file_browser();
                }
            } else if ((uint8_t)character >= MYOS_INPUT_GUI_ACTION_BROWSER_ENTRY_BASE
                       && (uint8_t)character - MYOS_INPUT_GUI_ACTION_BROWSER_ENTRY_BASE < GUI_BROWSER_PAGE_SIZE) {
                if (browser_mode != 0) {
                    const int result = browser_open_entry((uint8_t)character);

                    if (result == GUI_EDITOR_RESULT_EXIT) {
                        (void)system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_END, 0U, 0U);
                        break;
                    }
                    if (result == 0) { set_viewer_status("UNABLE TO OPEN FILE"); }
                }
            } else if ((uint8_t)character == MYOS_INPUT_GUI_ACTION_BROWSER_NEXT) {
                if (browser_mode != 0 && browser_has_next_page() != 0) {
                    browser_page++;
                    show_file_browser();
                }
            } else if ((uint8_t)character == MYOS_INPUT_GUI_ACTION_BROWSER_CREATE
                       || (uint8_t)character == MYOS_INPUT_GUI_ACTION_BROWSER_CREATE_DIRECTORY) {
                if (browser_mode != 0 && browser_directory_is_writable() != 0) {
                    browser_new_file_length = 0U;
                    browser_new_file_name[0] = '\0';
                    browser_new_entry_is_directory = (uint8_t)character == MYOS_INPUT_GUI_ACTION_BROWSER_CREATE_DIRECTORY;
                    browser_prompt_is_remove = 0;
                    browser_prompt_is_copy = 0;
                    browser_prompt_is_rename = 0;
                    browser_prompt_is_move = 0;
                    browser_prompt_is_search = 0;
                    browser_remove_confirmation = 0;
                    browser_new_file_mode = 1;
                    show_browser_new_file_prompt();
                } else if (browser_mode != 0) {
                    set_viewer_status("READ ONLY DIRECTORY");
                }
            } else if ((uint8_t)character == MYOS_INPUT_GUI_ACTION_BROWSER_REMOVE) {
                if (browser_mode != 0 && browser_directory_is_writable() != 0) {
                    browser_new_file_length = 0U;
                    browser_new_file_name[0] = '\0';
                    browser_new_entry_is_directory = 0;
                    browser_prompt_is_remove = 1;
                    browser_prompt_is_copy = 0;
                    browser_prompt_is_rename = 0;
                    browser_prompt_is_move = 0;
                    browser_prompt_is_search = 0;
                    browser_remove_confirmation = 0;
                    browser_new_file_mode = 1;
                    show_browser_new_file_prompt();
                } else if (browser_mode != 0) {
                    set_viewer_status("READ ONLY DIRECTORY");
                }
            } else if ((uint8_t)character == MYOS_INPUT_GUI_ACTION_BROWSER_COPY) {
                if (browser_mode != 0 && browser_directory_is_writable() != 0) {
                    browser_new_file_length = 0U;
                    browser_new_file_name[0] = '\0';
                    browser_copy_source_length = 0U;
                    browser_copy_source_name[0] = '\0';
                    browser_new_entry_is_directory = 0;
                    browser_prompt_is_remove = 0;
                    browser_prompt_is_copy = 1;
                    browser_prompt_is_rename = 0;
                    browser_prompt_is_search = 0;
                    browser_copy_prompt_for_target = 0;
                    browser_remove_confirmation = 0;
                    browser_new_file_mode = 1;
                    show_browser_new_file_prompt();
                } else if (browser_mode != 0) {
                    set_viewer_status("READ ONLY DIRECTORY");
                }
            } else if ((uint8_t)character == MYOS_INPUT_GUI_ACTION_BROWSER_RENAME) {
                if (browser_mode != 0 && browser_directory_is_writable() != 0) {
                    browser_new_file_length = 0U;
                    browser_new_file_name[0] = '\0';
                    browser_rename_source_length = 0U;
                    browser_rename_source_name[0] = '\0';
                    browser_new_entry_is_directory = 0;
                    browser_prompt_is_remove = 0;
                    browser_prompt_is_copy = 0;
                    browser_prompt_is_rename = 1;
                    browser_prompt_is_search = 0;
                    browser_rename_prompt_for_target = 0;
                    browser_remove_confirmation = 0;
                    browser_new_file_mode = 1;
                    show_browser_new_file_prompt();
                } else if (browser_mode != 0) {
                    set_viewer_status("READ ONLY DIRECTORY");
                }
            } else if ((uint8_t)character == MYOS_INPUT_GUI_ACTION_BROWSER_MOVE) {
                if (browser_mode != 0 && browser_directory_is_writable() != 0) {
                    browser_new_file_length = 0U;
                    browser_new_file_name[0] = '\0';
                    browser_move_source_length = 0U;
                    browser_move_source_name[0] = '\0';
                    browser_move_target_length = 0U;
                    browser_move_target_directory[0] = '\0';
                    browser_new_entry_is_directory = 0;
                    browser_prompt_is_remove = 0;
                    browser_prompt_is_copy = 0;
                    browser_prompt_is_rename = 0;
                    browser_prompt_is_move = 1;
                    browser_prompt_is_search = 0;
                    browser_move_prompt_for_target = 0;
                    browser_remove_confirmation = 0;
                    browser_new_file_mode = 1;
                    show_browser_new_file_prompt();
                } else if (browser_mode != 0) {
                    set_viewer_status("READ ONLY DIRECTORY");
                }
            } else if ((uint8_t)character == MYOS_INPUT_KEY_ALT_TAB && home_mode == 0) {
                (void)system_call(MYOS_SYS_GUI_SESSION, MYOS_GUI_INPUT, (uint64_t)(uint8_t)character, 0U);
            } else if ((uint8_t)character == MYOS_INPUT_GUI_ACTION_BROWSER_SEARCH) {
                if (browser_mode != 0) {
                    browser_new_file_length = 0U;
                    browser_new_file_name[0] = '\0';
                    browser_new_entry_is_directory = 0;
                    browser_prompt_is_remove = 0;
                    browser_prompt_is_copy = 0;
                    browser_prompt_is_search = 1;
                    browser_remove_confirmation = 0;
                    browser_new_file_mode = 1;
                    show_browser_new_file_prompt();
                }
            }
        }
    }
    (void)system_call(MYOS_SYS_EXIT, status, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
