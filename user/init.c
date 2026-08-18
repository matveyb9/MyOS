#include <stdint.h>

#include <syscall.h>
#define USER_WRITE_LIMIT UINT64_C(256)
#define USER_LINE_CAPACITY 128U
#define PIT_HZ UINT64_C(100)
#define SHELL_ENV_MAX 8U
#define SHELL_ENV_NAME_MAX 16U
#define SHELL_ENV_VALUE_MAX 64U
#define SHELL_HISTORY_MAX 8U

struct shell_environment_entry {
    int used;
    char name[SHELL_ENV_NAME_MAX];
    char value[SHELL_ENV_VALUE_MAX];
};

static struct shell_environment_entry shell_environment[SHELL_ENV_MAX];
static char shell_history[SHELL_HISTORY_MAX][USER_LINE_CAPACITY];
static uint64_t shell_history_count;
static const char *const shell_commands[] = {
    "help", "echo", "uname", "ps", "meminfo", "date", "uptime", "ls", "cat", "touch", "write", "rm",
    "set", "get", "env", "sleep", "run", "spawn", "pipe", "wait", "kill", "stress", "calc", "startgui",
    "reboot", "poweroff", "dmesg", "clear", "exit"
};

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

static uint64_t text_length(const char *text) {
    uint64_t length = 0U;

    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static int text_equal(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return 0;
        }
        left++;
        right++;
    }
    return *left == *right;
}

static void write_bytes(const char *text, uint64_t length) {
    while (length != 0U) {
        const uint64_t chunk = length > USER_WRITE_LIMIT ? USER_WRITE_LIMIT : length;

        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)text, chunk);
        text += chunk;
        length -= chunk;
    }
}

static void write_text(const char *text) {
    write_bytes(text, text_length(text));
}

static void write_char(char character) {
    write_bytes(&character, 1U);
}

static void write_number(uint64_t value) {
    char digits[21];
    uint64_t count = 0U;

    if (value == 0U) {
        write_char('0');
        return;
    }
    while (value != 0U) {
        digits[count] = (char)('0' + (value % 10U));
        value /= 10U;
        count++;
    }
    while (count != 0U) {
        count--;
        write_char(digits[count]);
    }
}

static int read_input_character(char *character) {
    for (;;) {
        const uint64_t result = system_call(MYOS_SYS_READ, 0U, (uint64_t)(uintptr_t)character, 1U);
        if (result != 0U && result != UINT64_MAX) {
            return 1;
        }
    }
}

static void history_store(const char *line) {
    uint64_t index;

    if (line[0] == '\0' || (shell_history_count != 0U && text_equal(line, shell_history[shell_history_count - 1U]) != 0)) {
        return;
    }
    if (shell_history_count == SHELL_HISTORY_MAX) {
        for (index = 1U; index < SHELL_HISTORY_MAX; index++) {
            for (uint64_t character = 0U; character < USER_LINE_CAPACITY; character++) {
                shell_history[index - 1U][character] = shell_history[index][character];
            }
        }
        shell_history_count--;
    }
    for (index = 0U; index < USER_LINE_CAPACITY; index++) {
        shell_history[shell_history_count][index] = line[index];
        if (line[index] == '\0') {
            break;
        }
    }
    shell_history_count++;
}

static int text_starts_with(const char *text, const char *prefix) {
    uint64_t index = 0U;

    while (prefix[index] != '\0') {
        if (text[index] != prefix[index]) { return 0; }
        index++;
    }
    return 1;
}

static void line_replace(char *line, uint64_t capacity, uint64_t *length, const char *replacement) {
    uint64_t replacement_length = text_length(replacement);

    if (replacement_length + 1U > capacity) { return; }
    while (*length != 0U) { write_text("\b \b"); (*length)--; }
    for (uint64_t index = 0U; index < replacement_length; index++) { line[index] = replacement[index]; }
    line[replacement_length] = '\0';
    *length = replacement_length;
    write_text(replacement);
}

static void history_replace_line(char *line, uint64_t capacity, uint64_t *length, const char *replacement) {
    uint64_t replacement_length = text_length(replacement);

    if (replacement_length + 1U > capacity) {
        return;
    }
    while (*length != 0U) {
        write_text("\b \b");
        (*length)--;
    }
    for (uint64_t index = 0U; index < replacement_length; index++) {
        line[index] = replacement[index];
    }
    line[replacement_length] = '\0';
    *length = replacement_length;
    write_text(replacement);
}

static void complete_line(char *line, uint64_t capacity, uint64_t *length) {
    char replacement[USER_LINE_CAPACITY];
    const char *token;
    uint64_t token_start = *length;
    uint64_t matches = 0U;
    const char *match = (const char *)0;

    while (token_start != 0U && line[token_start - 1U] != ' ') { token_start--; }
    token = line + token_start;
    if (token_start == 0U) {
        for (uint64_t index = 0U; index < sizeof(shell_commands) / sizeof(shell_commands[0]); index++) {
            if (text_starts_with(shell_commands[index], token) != 0) {
                matches++;
                match = shell_commands[index];
            }
        }
    } else {
        for (uint64_t index = 0U; index < UINT64_C(64); index++) {
            struct myos_vfs_entry entry;

            if (system_call(MYOS_SYS_VFS_ENTRY, index, (uint64_t)(uintptr_t)&entry, sizeof(entry)) == UINT64_MAX) {
                break;
            }
            if (text_starts_with(entry.name, token) != 0) {
                matches++;
                match = entry.name;
            }
        }
    }
    if (matches != 1U || match == (const char *)0) { return; }
    if (token_start + text_length(match) + 1U > capacity) { return; }
    for (uint64_t index = 0U; index < token_start; index++) { replacement[index] = line[index]; }
    for (uint64_t index = 0U; match[index] != '\0'; index++) { replacement[token_start + index] = match[index]; }
    replacement[token_start + text_length(match)] = '\0';
    line_replace(line, capacity, length, replacement);
}

static uint64_t read_line(char *line, uint64_t capacity) {
    uint64_t length = 0U;
    uint64_t history_position = shell_history_count;

    for (;;) {
        char character;

        (void)read_input_character(&character);
        if (character == '\r' || character == '\n') {
            write_char('\n');
            break;
        }
        if (character == '\t') {
            complete_line(line, capacity, &length);
            continue;
        }
        if (character == '\x1B') {
            char bracket;
            char direction;

            (void)read_input_character(&bracket);
            (void)read_input_character(&direction);
            if (bracket == '[' && direction == 'A' && history_position != 0U) {
                history_position--;
                history_replace_line(line, capacity, &length, shell_history[history_position]);
            } else if (bracket == '[' && direction == 'B' && history_position < shell_history_count) {
                history_position++;
                history_replace_line(line, capacity, &length,
                                     history_position == shell_history_count ? "" : shell_history[history_position]);
            }
            continue;
        }
        if (character == '\b' || character == 0x7FU) {
            if (length != 0U) {
                length--;
                line[length] = '\0';
                write_text("\b \b");
            }
            continue;
        }
        if (character >= ' ' && character <= '~' && length + 1U < capacity) {
            line[length] = character;
            length++;
            line[length] = '\0';
            write_char(character);
        }
    }
    line[length] = '\0';
    return length;
}

static char *first_argument(char *line) {
    while (*line != '\0' && *line != ' ') {
        line++;
    }
    if (*line == '\0') {
        return line;
    }
    *line = '\0';
    line++;
    while (*line == ' ') {
        line++;
    }
    return line;
}

static uint64_t parse_decimal(const char *text) {
    uint64_t value = 0U;

    while (*text >= '0' && *text <= '9') {
        value = value * 10U + (uint64_t)(*text - '0');
        text++;
    }
    return value;
}

static int environment_name_equal(const char *left, const char *right) {
    return text_equal(left, right);
}

static const char *environment_lookup(const char *name) {
    for (uint64_t index = 0U; index < SHELL_ENV_MAX; index++) {
        if (shell_environment[index].used != 0 && environment_name_equal(shell_environment[index].name, name) != 0) {
            return shell_environment[index].value;
        }
    }
    return (const char *)0;
}

static int environment_set(char *name, const char *value) {
    struct shell_environment_entry *entry = (struct shell_environment_entry *)0;
    uint64_t name_length = 0U;
    uint64_t value_length = 0U;

    while (name[name_length] != '\0' && name_length + 1U < SHELL_ENV_NAME_MAX) {
        const char character = name[name_length];

        if (!((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z')
              || (character >= '0' && character <= '9') || character == '_')) {
            return 0;
        }
        name_length++;
    }
    if (name_length == 0U || name[name_length] != '\0') {
        return 0;
    }
    while (value[value_length] != '\0' && value_length + 1U < SHELL_ENV_VALUE_MAX) {
        value_length++;
    }
    if (value[value_length] != '\0') {
        return 0;
    }
    for (uint64_t index = 0U; index < SHELL_ENV_MAX; index++) {
        if (shell_environment[index].used != 0 && environment_name_equal(shell_environment[index].name, name) != 0) {
            entry = &shell_environment[index];
        }
        if (entry == (struct shell_environment_entry *)0 && shell_environment[index].used == 0) {
            entry = &shell_environment[index];
        }
    }
    if (entry == (struct shell_environment_entry *)0) {
        return 0;
    }
    for (uint64_t index = 0U; index <= name_length; index++) {
        entry->name[index] = name[index];
    }
    for (uint64_t index = 0U; index <= value_length; index++) {
        entry->value[index] = value[index];
    }
    entry->used = 1;
    return 1;
}

static void environment_expand(const char *source, char *destination, uint64_t capacity) {
    uint64_t write_index = 0U;

    for (uint64_t read_index = 0U; source[read_index] != '\0' && write_index + 1U < capacity;) {
        if (source[read_index] == '$') {
            char name[SHELL_ENV_NAME_MAX];
            uint64_t name_length = 0U;
            const char *value;

            read_index++;
            while (source[read_index] != '\0' && source[read_index] != ' ' && name_length + 1U < sizeof(name)) {
                name[name_length++] = source[read_index++];
            }
            name[name_length] = '\0';
            value = environment_lookup(name);
            if (value != (const char *)0) {
                for (uint64_t index = 0U; value[index] != '\0' && write_index + 1U < capacity; index++) {
                    destination[write_index++] = value[index];
                }
            }
        } else {
            destination[write_index++] = source[read_index++];
        }
    }
    destination[write_index] = '\0';
}

static int make_spawn_request(struct myos_spawn_request *request, char *line) {
    char *arguments;
    uint64_t path_length = 0U;
    uint64_t arguments_length = 0U;

    if (request == (struct myos_spawn_request *)0 || line == (char *)0 || line[0] == '\0') {
        return 0;
    }
    request->input_pipe_id = UINT64_MAX;
    request->output_pipe_id = UINT64_MAX;
    arguments = first_argument(line);
    while (line[path_length] != '\0' && path_length + 1U < MYOS_SPAWN_PATH_MAX) {
        request->path[path_length] = line[path_length];
        path_length++;
    }
    if (path_length == 0U || line[path_length] != '\0') {
        return 0;
    }
    request->path[path_length] = '\0';
    while (arguments[arguments_length] != '\0'
           && arguments_length + 1U < MYOS_SPAWN_ARGUMENTS_MAX) {
        request->arguments[arguments_length] = arguments[arguments_length];
        arguments_length++;
    }
    if (arguments[arguments_length] != '\0') {
        return 0;
    }
    request->arguments[arguments_length] = '\0';
    return 1;
}

static void command_help(const char *topic) {
    if (text_equal(topic, "calc")) {
        write_text("calc <signed-integer> <+|-|*|/> <signed-integer>\n");
        write_text("Examples: calc -5 + 2   calc -7 * -6\n");
        write_text("Uses signed 64-bit integers; division truncates toward zero.\n");
        return;
    }
    if (topic[0] != '\0') {
        write_text("No detailed help for: ");
        write_text(topic);
        write_text(". Try help or help calc.\n");
        return;
    }
    write_text("MYOS SHELL QUICK START\n");
    write_text("Files: ls cat touch write rm | Processes: ps run spawn wait kill sleep\n");
    write_text("Tools: calc <a> <op> <b>; run <program> [arguments]; pipe <text>\n");
    write_text("GUI: startgui [file]; E edits disk/note; arrows/Home/End/Delete move; Ctrl-S saves.\n");
    write_text("System: uname meminfo date uptime reboot poweroff clear dmesg\n");
    write_text("Input: Tab completes a unique name; Up/Down navigates history.\n");
    write_text("Files: tmp/<name> is temporary; disk/<name> persists across reboots.\n");
    write_text("For calculator details, type: help calc\n");
}

static const char *task_state_name(uint64_t state) {
    if (state == MYOS_TASK_STATE_READY) {
        return "ready";
    }
    if (state == MYOS_TASK_STATE_RUNNING) {
        return "running";
    }
    if (state == MYOS_TASK_STATE_SLEEPING) {
        return "sleeping";
    }
    if (state == MYOS_TASK_STATE_ZOMBIE) {
        return "zombie";
    }
    if (state == MYOS_TASK_STATE_WAITING) {
        return "waiting";
    }
    if (state == MYOS_TASK_STATE_INPUT) {
        return "input";
    }
    return "unused";
}

static const char *task_kind_name(uint64_t kind) {
    return kind == MYOS_TASK_KIND_USER ? "user" : "kernel";
}

static void command_ps(void) {
    const uint64_t current_pid = system_call(MYOS_SYS_GETPID, 0U, 0U, 0U);

    write_text("PID  STATE     KIND    RUNS  COMMAND\n");
    for (uint64_t task_id = 0U; task_id < MYOS_TASK_SLOT_COUNT; task_id++) {
        struct myos_task_info info;

        if (system_call(MYOS_SYS_TASK_INFO, task_id, (uint64_t)(uintptr_t)&info, sizeof(info))
            == UINT64_MAX || info.state == MYOS_TASK_STATE_UNUSED) {
            continue;
        }
        write_number(info.id);
        write_text("    ");
        write_text(task_state_name(info.state));
        write_text("    ");
        write_text(task_kind_name(info.kind));
        write_text("    ");
        write_number(info.run_count);
        write_text("     ");
        write_text(info.name);
        if (info.id == current_pid) {
            write_text(" [current]");
        }
        write_char('\n');
    }
}

static void write_padded_two(uint64_t value) {
    if (value < 10U) {
        write_char('0');
    }
    write_number(value);
}

static void command_date(void) {
    struct myos_rtc_time time;

    if (system_call(MYOS_SYS_RTC_TIME, 0U, (uint64_t)(uintptr_t)&time, sizeof(time)) == UINT64_MAX) {
        write_text("RTC time unavailable.\n");
        return;
    }
    write_number(time.year);
    write_char('-');
    write_padded_two(time.month);
    write_char('-');
    write_padded_two(time.day);
    write_char(' ');
    write_padded_two(time.hour);
    write_char(':');
    write_padded_two(time.minute);
    write_char(':');
    write_padded_two(time.second);
    write_text(" UTC\n");
}

static void command_uptime(void) {
    const uint64_t ticks = system_call(MYOS_SYS_UPTIME, 0U, 0U, 0U);
    const uint64_t total_seconds = ticks / PIT_HZ;
    const uint64_t days = total_seconds / UINT64_C(86400);
    const uint64_t hours = (total_seconds / UINT64_C(3600)) % UINT64_C(24);
    const uint64_t minutes = (total_seconds / UINT64_C(60)) % UINT64_C(60);
    const uint64_t seconds = total_seconds % UINT64_C(60);

    write_text("Uptime: ");
    write_number(days);
    write_text("d ");
    write_padded_two(hours);
    write_text("h ");
    write_padded_two(minutes);
    write_text("m ");
    write_padded_two(seconds);
    write_text("s (");
    write_number(ticks);
    write_text(" ticks)\n");
}

static void command_ls(void) {
    for (uint64_t index = 0U; index < UINT64_C(64); index++) {
        struct myos_vfs_entry entry;

        if (system_call(MYOS_SYS_VFS_ENTRY, index, (uint64_t)(uintptr_t)&entry, sizeof(entry))
            == UINT64_MAX) {
            return;
        }
        write_text(entry.name);
        write_text("  ");
        write_number(entry.size);
        write_char('\n');
    }
}

static void command_cat(const char *argument) {
    struct myos_vfs_read_request request;
    uint64_t path_length = 0U;

    while (argument[path_length] != '\0' && path_length + 1U < MYOS_VFS_NAME_MAX) {
        request.path[path_length] = argument[path_length];
        path_length++;
    }
    if (path_length == 0U || argument[path_length] != '\0') {
        write_text("Usage: cat <file>\n");
        return;
    }
    request.path[path_length] = '\0';
    request.offset = 0U;
    for (;;) {
        const uint64_t count = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&request,
                                           sizeof(request));

        if (count == UINT64_MAX) {
            write_text("Unable to read file.\n");
            return;
        }
        if (count == 0U) {
            return;
        }
        write_bytes((const char *)request.data, count);
        if (request.offset > UINT64_MAX - count) {
            write_text("File is too large.\n");
            return;
        }
        request.offset += count;
    }
}

static int make_tmpfs_path_request(struct myos_tmpfs_path_request *request, const char *argument) {
    uint64_t length = 0U;

    if (request == (struct myos_tmpfs_path_request *)0 || argument == (const char *)0) {
        return 0;
    }
    for (uint64_t index = 0U; index < MYOS_VFS_NAME_MAX; index++) {
        request->path[index] = '\0';
    }
    while (argument[length] != '\0' && length + 1U < MYOS_VFS_NAME_MAX) {
        request->path[length] = argument[length];
        length++;
    }
    return length != 0U && argument[length] == '\0';
}

static int persistent_path_requested(const char *path) {
    return path != (const char *)0 && path[0] == 'd' && path[1] == 'i' && path[2] == 's'
        && path[3] == 'k' && path[4] == '/';
}

static void command_touch(const char *argument) {
    struct myos_tmpfs_path_request request;
    uint64_t create_number;

    if (make_tmpfs_path_request(&request, argument) == 0) {
        write_text("Usage: touch tmp/<name> | disk/<name>\n");
        return;
    }
    create_number = persistent_path_requested(request.path) != 0 ? MYOS_SYS_PERSIST_CREATE : MYOS_SYS_TMPFS_CREATE;
    if (system_call(create_number, 0U, (uint64_t)(uintptr_t)&request, sizeof(request))
        == UINT64_MAX) {
        write_text("Unable to create file.\n");
        return;
    }
    write_text("Created ");
    write_text(request.path);
    write_char('\n');
}

static void command_write(char *argument) {
    struct myos_tmpfs_path_request path_request;
    struct myos_tmpfs_write_request request;
    char *text = first_argument(argument);
    uint64_t text_length_value;
    uint64_t create_number;
    uint64_t remove_number;
    uint64_t write_number_value;

    if (make_tmpfs_path_request(&path_request, argument) == 0 || text[0] == '\0') {
        write_text("Usage: write tmp/<name> | disk/<name> <text>\n");
        return;
    }
    text_length_value = text_length(text);
    if (text_length_value > MYOS_TMPFS_WRITE_CHUNK) {
        write_text("Text is too long.\n");
        return;
    }
    for (uint64_t index = 0U; index < MYOS_VFS_NAME_MAX; index++) {
        request.path[index] = path_request.path[index];
    }
    for (uint64_t index = 0U; index < MYOS_TMPFS_WRITE_CHUNK; index++) {
        request.data[index] = 0U;
    }
    for (uint64_t index = 0U; index < text_length_value; index++) {
        request.data[index] = (uint8_t)text[index];
    }
    request.offset = 0U;
    request.length = text_length_value;
    remove_number = persistent_path_requested(path_request.path) != 0 ? MYOS_SYS_PERSIST_REMOVE : MYOS_SYS_TMPFS_REMOVE;
    create_number = persistent_path_requested(path_request.path) != 0 ? MYOS_SYS_PERSIST_CREATE : MYOS_SYS_TMPFS_CREATE;
    write_number_value = persistent_path_requested(path_request.path) != 0 ? MYOS_SYS_PERSIST_WRITE : MYOS_SYS_TMPFS_WRITE;
    (void)system_call(remove_number, 0U, (uint64_t)(uintptr_t)&path_request, sizeof(path_request));
    if (system_call(create_number, 0U, (uint64_t)(uintptr_t)&path_request, sizeof(path_request)) == UINT64_MAX
        || system_call(write_number_value, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
        write_text("Unable to write file.\n");
        return;
    }
    write_text("Wrote ");
    write_number(text_length_value);
    write_text(" byte(s) to ");
    write_text(request.path);
    write_char('\n');
}

static void command_rm(const char *argument) {
    struct myos_tmpfs_path_request request;
    uint64_t remove_number;

    if (make_tmpfs_path_request(&request, argument) == 0) {
        write_text("Usage: rm tmp/<name> | disk/<name>\n");
        return;
    }
    remove_number = persistent_path_requested(request.path) != 0 ? MYOS_SYS_PERSIST_REMOVE : MYOS_SYS_TMPFS_REMOVE;
    if (system_call(remove_number, 0U, (uint64_t)(uintptr_t)&request, sizeof(request))
        == UINT64_MAX) {
        write_text("Unable to remove file.\n");
        return;
    }
    write_text("Removed ");
    write_text(request.path);
    write_char('\n');
}

static void command_meminfo(void) {
    const uint64_t frames = system_call(MYOS_SYS_FREE_FRAMES, 0U, 0U, 0U);

    write_text("Free physical frames: ");
    write_number(frames);
    write_text(" (bytes: ");
    write_number(frames * UINT64_C(4096));
    write_text(")\n");
}

static void command_spawn(char *argument) {
    struct myos_spawn_request request = { { 0 }, { 0 }, UINT64_MAX, UINT64_MAX };
    uint64_t result;

    if (make_spawn_request(&request, argument) == 0) {
        write_text("Usage: spawn <program> [arguments]\n");
        return;
    }
    result = system_call(MYOS_SYS_SPAWN, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));
    if (result == UINT64_MAX) {
        write_text("Unable to start program.\n");
        return;
    }
    write_text("Started background process ");
    write_number(result);
    write_char('\n');
}

static void command_stress(void) {
    char sleeper_line[] = "sleeper";
    struct myos_spawn_request request = { { 0 }, { 0 }, UINT64_MAX, UINT64_MAX };
    uint64_t launched = 0U;

    if (make_spawn_request(&request, sleeper_line) == 0) {
        write_text("Unable to prepare stress program.\n");
        return;
    }
    while (launched < MYOS_TASK_SLOT_COUNT) {
        const uint64_t result = system_call(MYOS_SYS_SPAWN, 0U, (uint64_t)(uintptr_t)&request,
                                            sizeof(request));

        if (result == UINT64_MAX) {
            break;
        }
        launched++;
    }
    write_text("Stress started ");
    write_number(launched);
    write_text(" sleeper task(s).\n");
}

static void command_wait(const char *argument) {
    const uint64_t task_id = parse_decimal(argument);
    uint64_t status;

    if (argument[0] == '\0') {
        write_text("Usage: wait <pid>\n");
        return;
    }
    status = system_call(MYOS_SYS_WAIT, task_id, 0U, 0U);
    if (status == UINT64_MAX) {
        write_text("Wait failed.\n");
        return;
    }
    write_text("Process ");
    write_number(task_id);
    write_text(" exited with status ");
    write_number(status);
    write_char('\n');
}

static void command_kill(const char *argument) {
    const uint64_t task_id = parse_decimal(argument);

    if (argument[0] == '\0') {
        write_text("Usage: kill <pid>\n");
        return;
    }
    if (system_call(MYOS_SYS_KILL, task_id, 0U, 0U) == UINT64_MAX) {
        write_text("Kill failed.\n");
        return;
    }
    write_text("Process ");
    write_number(task_id);
    write_text(" terminated; use wait to reap it.\n");
}

static void command_set(char *argument) {
    char *value = first_argument(argument);

    if (argument[0] == '\0' || value[0] == '\0' || environment_set(argument, value) == 0) {
        write_text("Usage: set <NAME> <value up to 63 bytes>\n");
        return;
    }
}

static void command_get(const char *argument) {
    const char *value = environment_lookup(argument);

    if (argument[0] == '\0' || value == (const char *)0) {
        write_text("Variable not found.\n");
        return;
    }
    write_text(value);
    write_char('\n');
}

static void command_env(void) {
    for (uint64_t index = 0U; index < SHELL_ENV_MAX; index++) {
        if (shell_environment[index].used != 0) {
            write_text(shell_environment[index].name);
            write_char('=');
            write_text(shell_environment[index].value);
            write_char('\n');
        }
    }
}

static void command_pipe(char *argument) {
    struct myos_spawn_request writer = { { 0 }, { 0 }, UINT64_MAX, UINT64_MAX };
    struct myos_spawn_request reader = { { 0 }, { 0 }, UINT64_MAX, UINT64_MAX };
    uint64_t length = text_length(argument);
    uint64_t pipe_id;
    uint64_t writer_id;
    uint64_t reader_id;

    if (length == 0U || length >= MYOS_SPAWN_ARGUMENTS_MAX) {
        write_text("Usage: pipe <text up to 127 bytes>\n");
        return;
    }
    writer.path[0] = 'p'; writer.path[1] = 'i'; writer.path[2] = 'p'; writer.path[3] = 'e';
    writer.path[4] = 'w'; writer.path[5] = 'r'; writer.path[6] = 'i'; writer.path[7] = 't';
    writer.path[8] = 'e'; writer.path[9] = '\0';
    reader.path[0] = 'p'; reader.path[1] = 'i'; reader.path[2] = 'p'; reader.path[3] = 'e';
    reader.path[4] = 'r'; reader.path[5] = 'e'; reader.path[6] = 'a'; reader.path[7] = 'd';
    reader.path[8] = '\0';
    for (uint64_t index = 0U; index <= length; index++) {
        writer.arguments[index] = argument[index];
    }
    pipe_id = system_call(MYOS_SYS_PIPE_CREATE, 0U, 0U, 0U);
    if (pipe_id == UINT64_MAX) {
        write_text("Unable to create pipe.\n");
        return;
    }
    writer.output_pipe_id = pipe_id;
    reader.input_pipe_id = pipe_id;
    writer_id = system_call(MYOS_SYS_SPAWN, 0U, (uint64_t)(uintptr_t)&writer, sizeof(writer));
    if (writer_id == UINT64_MAX) {
        (void)system_call(MYOS_SYS_PIPE_SEAL, pipe_id, 0U, 0U);
        write_text("Unable to start pipe producer.\n");
        return;
    }
    reader_id = system_call(MYOS_SYS_SPAWN, 0U, (uint64_t)(uintptr_t)&reader, sizeof(reader));
    (void)system_call(MYOS_SYS_PIPE_SEAL, pipe_id, 0U, 0U);
    if (reader_id == UINT64_MAX) {
        (void)system_call(MYOS_SYS_WAIT, writer_id, 0U, 0U);
        write_text("Unable to start pipe consumer.\n");
        return;
    }
    (void)system_call(MYOS_SYS_WAIT, writer_id, 0U, 0U);
    (void)system_call(MYOS_SYS_WAIT, reader_id, 0U, 0U);
    write_char('\n');
}

static int run_foreground(char *argument, int verbose) {
    struct myos_spawn_request request = { { 0 }, { 0 }, UINT64_MAX, UINT64_MAX };
    uint64_t result;
    uint64_t status;

    if (make_spawn_request(&request, argument) == 0) {
        if (verbose != 0) {
            write_text("Usage: run <program> [arguments]\n");
        } else {
            write_text("calc: unable to prepare expression\n");
        }
        return 0;
    }
    result = system_call(MYOS_SYS_SPAWN, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));
    if (result == UINT64_MAX) {
        if (verbose != 0) {
            write_text("Unable to start program.\n");
        } else {
            write_text("calc: unable to start calculator\n");
        }
        return 0;
    }
    if (verbose != 0) {
        write_text("Started process ");
        write_number(result);
        write_text("; waiting for exit...\n");
    }
    status = system_call(MYOS_SYS_WAIT, result, 0U, 0U);
    if (status == UINT64_MAX) {
        if (verbose != 0) {
            write_text("Wait failed.\n");
        } else {
            write_text("calc: wait failed\n");
        }
        return 0;
    }
    if (verbose != 0) {
        write_text("Process ");
        write_number(result);
        write_text(" exited with status ");
        write_number(status);
        write_char('\n');
    }
    return 1;
}

static void command_run(char *argument) {
    (void)run_foreground(argument, 1);
}

static void command_calc(const char *argument) {
    char program[USER_LINE_CAPACITY] = "calc";
    uint64_t length = 4U;

    if (argument[0] == '\0') {
        command_help("calc");
        return;
    }
    if (length + 1U >= sizeof(program)) {
        write_text("Calculator expression is too long.\n");
        return;
    }
    program[length++] = ' ';
    for (uint64_t index = 0U; argument[index] != '\0'; index++) {
        if (length + 1U >= sizeof(program)) {
            write_text("Calculator expression is too long.\n");
            return;
        }
        program[length++] = argument[index];
    }
    program[length] = '\0';
    (void)run_foreground(program, 0);
}

static void command_startgui(const char *argument) {
    char program[USER_LINE_CAPACITY] = "startgui";
    uint64_t length = 8U;

    if (argument[0] != '\0') {
        if (length + 1U >= sizeof(program)) {
            write_text("GUI file path is too long.\n");
            return;
        }
        program[length++] = ' ';
        for (uint64_t index = 0U; argument[index] != '\0'; index++) {
            if (length + 1U >= sizeof(program)) {
                write_text("GUI file path is too long.\n");
                return;
            }
            program[length++] = argument[index];
        }
        program[length] = '\0';
    }
    command_run(program);
}

static void command_poweroff(void) {
    uint64_t result;

    write_text("Powering off MyOS...\n");
    result = system_call(MYOS_SYS_POWEROFF, 0U, 0U, 0U);
    if (result == UINT64_MAX) {
        write_text("Poweroff failed.\n");
    }
}

static void command_reboot(void) {
    uint64_t result;

    write_text("Rebooting MyOS...\n");
    result = system_call(MYOS_SYS_REBOOT, 0U, 0U, 0U);
    if (result == UINT64_MAX) {
        write_text("Reboot failed.\n");
    }
}

static void command_sleep(const char *argument) {
    const uint64_t seconds = parse_decimal(argument);
    uint64_t result;

    if (seconds > UINT64_MAX / PIT_HZ) {
        write_text("Sleep duration is too large.\n");
        return;
    }
    result = system_call(MYOS_SYS_SLEEP, seconds * PIT_HZ, 0U, 0U);
    if (result == UINT64_MAX) {
        write_text("Sleep failed.\n");
    }
}

static void execute_command(char *line) {
    char *argument;
    char expanded_argument[USER_LINE_CAPACITY];

    argument = first_argument(line);
    environment_expand(argument, expanded_argument, sizeof(expanded_argument));
    argument = expanded_argument;
    if (line[0] == '\0') {
        return;
    }
    if (text_equal(line, "help")) {
        command_help(argument);
    } else if (text_equal(line, "echo")) {
        write_text(argument);
        write_char('\n');
    } else if (text_equal(line, "uname")) {
        write_text("MyOS 0.12.2-dev x86_64\n");
    } else if (text_equal(line, "ps")) {
        command_ps();
    } else if (text_equal(line, "meminfo")) {
        command_meminfo();
    } else if (text_equal(line, "date")) {
        command_date();
    } else if (text_equal(line, "uptime")) {
        command_uptime();
    } else if (text_equal(line, "ls")) {
        command_ls();
    } else if (text_equal(line, "cat")) {
        command_cat(argument);
    } else if (text_equal(line, "touch")) {
        command_touch(argument);
    } else if (text_equal(line, "write")) {
        command_write(argument);
    } else if (text_equal(line, "rm")) {
        command_rm(argument);
    } else if (text_equal(line, "set")) {
        command_set(argument);
    } else if (text_equal(line, "get")) {
        command_get(argument);
    } else if (text_equal(line, "env")) {
        command_env();
    } else if (text_equal(line, "run")) {
        command_run(argument);
    } else if (text_equal(line, "spawn")) {
        command_spawn(argument);
    } else if (text_equal(line, "pipe")) {
        command_pipe(argument);
    } else if (text_equal(line, "wait")) {
        command_wait(argument);
    } else if (text_equal(line, "kill")) {
        command_kill(argument);
    } else if (text_equal(line, "stress")) {
        command_stress();
    } else if (text_equal(line, "sleep")) {
        command_sleep(argument);
    } else if (text_equal(line, "calc")) {
        command_calc(argument);
    } else if (text_equal(line, "startgui")) {
        command_startgui(argument);
    } else if (text_equal(line, "reboot")) {
        command_reboot();
    } else if (text_equal(line, "poweroff")) {
        command_poweroff();
    } else if (text_equal(line, "dmesg")) {
        write_text("MyOS: Limine boot, memory manager, scheduler, ring 3 and initramfs active.\n");
    } else if (text_equal(line, "clear")) {
        write_text("\x1B[2J\x1B[H");
    } else if (text_equal(line, "exit")) {
        (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    } else {
        write_text("Unknown command: ");
        write_text(line);
        write_text(". Type 'help' to see available commands.\n");
    }
}

void _start(void) __attribute__((noreturn));

void _start(void) {
    char line[USER_LINE_CAPACITY];

    write_text("\x1B[2J\x1B[H");
    write_text("+----------------------------------------------+\n");
    write_text("| MYOS USER SHELL 0.12.2-dev                  |\n");
    write_text("| Ready. Type help for available commands.    |\n");
    write_text("| Tab: complete   |  Up/Down: history         |\n");
    write_text("+----------------------------------------------+\n");
    for (;;) {
        write_text("[myos]$ ");
        (void)read_line(line, USER_LINE_CAPACITY);
        history_store(line);
        execute_command(line);
    }
}
