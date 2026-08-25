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
    "help", "echo", "uname", "sysinfo", "ps", "meminfo", "date", "uptime", "ls", "cat", "cp", "wc", "grep", "tree", "find", "head", "sort", "tail", "stat", "touch", "mkdir", "write", "rm",
    "set", "get", "env", "sleep", "run", "spawn", "install", "build", "newproj", "editproj", "buildproj", "runproj", "installproj", "uninstallproj", "projlist", "projstatus", "cleanproj", "rmproj", "pipe", "wait", "kill", "stress", "calc", "edit", "startgui",
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

static int built_in_program(const char *name) {
    static const char *const names[] = {
        "init", "hello", "sleeper", "orphaner", "safety", "argshow", "calc", "pipewrite",
        "piperead", "wc", "grep", "edit", "startgui", "install", "asm", "tree", "find", "stackprobe", "head", "stat", "tail", "sort", "cp"
    };

    for (uint64_t index = 0U; index < sizeof(names) / sizeof(names[0]); index++) {
        if (text_equal(name, names[index]) != 0) { return 1; }
    }
    return 0;
}

static int append_text(char *destination, uint64_t capacity, uint64_t *length, const char *source) {
    if (destination == (char *)0 || length == (uint64_t *)0 || source == (const char *)0) { return 0; }
    while (*source != '\0') {
        if (*length + 1U >= capacity) { return 0; }
        destination[(*length)++] = *source++;
    }
    destination[*length] = '\0';
    return 1;
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
    if (line[0] == '/') {
        if (append_text(request->path, sizeof(request->path), &path_length, line) == 0) { return 0; }
    } else if (built_in_program(line) != 0) {
        if (append_text(request->path, sizeof(request->path), &path_length, "/system/core/apps/") == 0
            || append_text(request->path, sizeof(request->path), &path_length, line) == 0
            || append_text(request->path, sizeof(request->path), &path_length, ".elf") == 0) { return 0; }
    } else {
        if (append_text(request->path, sizeof(request->path), &path_length, "/apps/") == 0
            || append_text(request->path, sizeof(request->path), &path_length, line) == 0
            || append_text(request->path, sizeof(request->path), &path_length, "/main.elf") == 0) { return 0; }
    }
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
    if (text_equal(topic, "cp")) {
        write_text("cp <absolute-source> <new-absolute-target>\n");
        write_text("Copies one file through the bounded native cp app; target must not exist and parent directory must exist.\n");
        write_text("run cp remains a compatibility form.\n");
        return;
    }
    if (text_equal(topic, "wc")) {
        write_text("wc <absolute-file>\n");
        write_text("Counts newline-terminated lines, space/tab/CR/LF-delimited words and bytes in bounded 256-byte VFS chunks.\n");
        write_text("run wc remains a compatibility form.\n");
        return;
    }
    if (text_equal(topic, "grep")) {
        write_text("grep <text> <absolute-file>\n");
        write_text("Prints lines up to 127 bytes containing the unspaced text; longer lines are skipped while the file is read in bounded 256-byte VFS chunks.\n");
        write_text("run grep remains a compatibility form.\n");
        return;
    }
    if (text_equal(topic, "tree")) {
        write_text("tree [absolute-directory]\n");
        write_text("Recursively lists logical VFS entries without mutation; limits: 8 levels, 64 entries/directory, 256 entries total.\n");
        write_text("run tree remains a compatibility form.\n");
        return;
    }
    if (text_equal(topic, "find")) {
        write_text("find <name-fragment> [absolute-directory]\n");
        write_text("Case-insensitive read-only name search; limits: 8 levels, 64 entries/directory, 256 entries total.\n");
        write_text("run find remains a compatibility form.\n");
        return;
    }
    if (text_equal(topic, "head")) {
        write_text("head <absolute-file> [1..64 lines]\n");
        write_text("Prints the first 10 lines by default; output is read in 256-byte chunks and capped at 4096 bytes.\n");
        write_text("run head remains a compatibility form.\n");
        return;
    }
    if (text_equal(topic, "sort")) {
        write_text("sort <absolute-file>\n");
        write_text("Sorts up to 64 lines in bytewise ASCII ascending order; each retained line is capped at 127 bytes.\n");
        write_text("run sort remains a compatibility form.\n");
        return;
    }
    if (text_equal(topic, "tail")) {
        write_text("tail <absolute-file> [1..64 lines]\n");
        write_text("Prints the last 10 lines by default; retains only the final 4096 bytes read through 256-byte chunks.\n");
        write_text("run tail remains a compatibility form.\n");
        return;
    }
    if (text_equal(topic, "stat")) {
        write_text("stat <absolute-path>\n");
        write_text("Reports logical VFS entry type and size by bounded parent-directory enumeration; it never modifies storage.\n");
        write_text("run stat remains a compatibility form.\n");
        return;
    }
    if (text_equal(topic, "startgui")) {
        write_text("startgui [absolute-file]\n");
        write_text("Without a file it opens MYOS DESKTOP; click SYSTEM, NOTES or EDIT NOTE. Click top-bar X to exit.\n");
        write_text("Hotkeys: Alt-Tab focus, Alt-F4 close, Esc back/cancel, Ctrl-Q exit and Ctrl-S save. 'home' is an alias.\n");
        return;
    }
    if (text_equal(topic, "sysinfo")) {
        write_text("sysinfo\n");
        write_text("Prints read-only boot, compiled-in driver and detected-device inventory from /system/live.\n");
        return;
    }
    if (text_equal(topic, "newproj")) {
        write_text("newproj <project-name> [hello|args]\n");
        write_text("Creates /users/myos/projects/<project-name>/main.mya from one fixed runnable template.\n");
        write_text("hello is the default; args writes the bounded native argument string. Existing projects are never overwritten.\n");
        write_text("Names are 1..31 ASCII letters, digits, '-' or '_'; unknown templates create nothing.\n");
        return;
    }
    if (text_equal(topic, "editproj")) {
        write_text("editproj <project-name>\n");
        write_text("Opens /users/myos/projects/<project-name>/main.mya in the bounded editor.\n");
        return;
    }
    if (text_equal(topic, "buildproj")) {
        write_text("buildproj <project-name>\n");
        write_text("Builds /users/myos/projects/<project-name>/main.mya to main.elf.\n");
        return;
    }
    if (text_equal(topic, "runproj")) {
        write_text("runproj <project-name> [arguments]\n");
        write_text("Runs only the regular generated /users/myos/projects/<project-name>/main.elf without installation.\n");
        write_text("Arguments use the existing native ABI and are limited to 127 visible bytes.\n");
        return;
    }
    if (text_equal(topic, "installproj")) {
        write_text("installproj <project-name>\n");
        write_text("Installs project main.elf as /apps/<project-name>/main.elf; an existing package target is replaced.\n");
        return;
    }
    if (text_equal(topic, "projlist")) {
        write_text("projlist\n");
        write_text("Lists up to 128 valid project directories with read-only source, build and installed package status rows.\n");
        return;
    }
    if (text_equal(topic, "projstatus")) {
        write_text("projstatus <project-name>\n");
        write_text("Shows regular-file state and size for fixed source, build and installed package paths.\n");
        return;
    }
    if (text_equal(topic, "uninstallproj")) {
        write_text("uninstallproj <project-name>\n");
        write_text("Removes only the regular installed /apps/<project-name>/main.elf; project source and build stay unchanged.\n");
        return;
    }
    if (text_equal(topic, "cleanproj")) {
        write_text("cleanproj <project-name>\n");
        write_text("Removes only the regular generated <project>/main.elf; source and installed package stay unchanged.\n");
        return;
    }
    if (text_equal(topic, "rmproj")) {
        write_text("rmproj <project-name>\n");
        write_text("After cleanproj, removes only the regular project source and empty project directory; installed package stays unchanged.\n");
        return;
    }
    if (text_equal(topic, "asm")) {
        write_text("run asm <source.mya> <output.elf>\n");
        write_text("Source: input; time; args; set <0..255>; not; neg; inc; dec; parity; clz; add/sub/mul/and/or/xor/test <0..255>; shl/shr/rol/ror <1..7>; div/mod <1..255>; store/load/cmp/swap <0..7>; label name:; write \"text\"; jump[_if_zero|_if_nonzero] name; jump_if <0..255> name; exit <0..255>\n");
        write_text("input reads one non-CR/LF byte; time prints RTC HH:MM:SS; args writes run arguments; not/neg/inc/dec/and/or/xor update one initialized byte, parity normalizes even byte parity to zero or one, clz counts leading zero bits (zero becomes eight), test normalizes its bitwise intersection with one byte to zero or one, shl/shr shift it logically and rol/ror rotate it circularly by 1..7 positions, neg/inc/dec/add/sub/mul wrap one byte, div returns an unsigned quotient, mod returns its unsigned remainder, and both reject zero; cmp compares the accumulator with one private slot and yields zero when equal; swap exchanges it with one private slot. Bitwise operations, parity, clz, test, shifts, arithmetic, cmp and swap require input/set/load. Conditional jumps use that result and target a later label.\n");
        write_text("Escape \\n, \\r, \\t, \\\\ and \\\" inside text.\n");
        return;
    }
    if (text_equal(topic, "edit")) {
        write_text("edit <absolute-file>\n");
        write_text("Multi-line text editor for ordinary files and .mya source.\n");
        write_text("Ctrl-S saves+exits; Ctrl-Q or Esc discards; arrows/Home/End, Del and Backspace edit.\n");
        write_text("Current document limit: 4096 bytes.\n");
        return;
    }
    if (topic[0] != '\0') {
        write_text("No detailed help for: ");
        write_text(topic);
        write_text(". Try help or help calc.\n");
        return;
    }
    write_text("MYOS SHELL QUICK START\n");
    write_text("Files: ls [path] cat touch mkdir write rm | Processes: ps run spawn install wait kill sleep\n");
    write_text("Tools: calc <a> <op> <b>; edit <absolute-file>; tree [absolute-directory]; find <name-fragment> [absolute-directory]; head <absolute-file> [1..64 lines]; stat <absolute-path>; tail <absolute-file> [1..64 lines]; sort <absolute-file>; run <program-or-absolute-path> [arguments]; help cp/tree/find/head/stat/tail/sort/startgui\n");
    write_text("Native: newproj/editproj/buildproj/runproj/installproj/uninstallproj/projlist/projstatus/cleanproj/rmproj <name>; build <source.mya> <output.elf>; help newproj/editproj/buildproj/runproj/installproj/uninstallproj/projlist/projstatus/cleanproj/rmproj/asm/edit\n");
    write_text("Install: install <source> </apps/name/main.elf>; GUI: startgui [absolute-file]\n");
    write_text("System: uname sysinfo meminfo date uptime reboot poweroff clear dmesg\n");
    write_text("Input: Tab completes a unique name; Up/Down navigates history.\n");
    write_text("Root: /system /apps /users/myos /temp; paths are case-insensitive.\n");
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

static int copy_vfs_path(char *destination, const char *path) {
    uint64_t length = 0U;

    if (destination == (char *)0 || path == (const char *)0) { return 0; }
    while (path[length] != '\0' && length + 1U < MYOS_VFS_PATH_MAX) {
        destination[length] = path[length];
        length++;
    }
    if (length == 0U || path[length] != '\0') { return 0; }
    destination[length] = '\0';
    return 1;
}

static int make_vfs_path_request(struct myos_vfs_path_request *request, const char *path) {
    return request != (struct myos_vfs_path_request *)0 && copy_vfs_path(request->path, path) != 0;
}

static void command_ls(const char *argument) {
    struct myos_vfs_list_request request;
    const char *path = argument[0] == '\0' ? "/" : argument;

    for (uint64_t index = 0U; index < UINT64_C(128); index++) {
        if (copy_vfs_path(request.path, path) == 0) {
            write_text("Usage: ls [absolute-directory]\n");
            return;
        }
        request.index = index;
        if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
            return;
        }
        if (request.entry.type == MYOS_VFS_OBJECT_DIRECTORY) { write_text("[dir] "); }
        else if (request.entry.type == MYOS_VFS_OBJECT_VIRTUAL) { write_text("[live] "); }
        else { write_text("[file] "); }
        write_text(request.entry.name);
        if (request.entry.type == MYOS_VFS_OBJECT_REGULAR) {
            write_text("  ");
            write_number(request.entry.size);
        }
        write_char('\n');
    }
}

static void command_cat(const char *argument) {
    struct myos_vfs_read_request request;
    uint64_t path_length = 0U;

    while (argument[path_length] != '\0' && path_length + 1U < MYOS_VFS_PATH_MAX) {
        request.path[path_length] = argument[path_length];
        path_length++;
    }
    if (path_length == 0U || argument[path_length] != '\0' || argument[0] != '/') {
        write_text("Usage: cat <absolute-file>\n");
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

static void command_sysinfo(void) {
    write_text("MYOS SYSTEM INVENTORY\n");
    write_text("[boot]\n");
    command_cat("/system/live/boot/info");
    write_text("[drivers]\n");
    command_cat("/system/live/drivers/framebuffer");
    command_cat("/system/live/drivers/keyboard");
    command_cat("/system/live/drivers/mouse");
    command_cat("/system/live/drivers/ahci");
    command_cat("/system/live/drivers/acpi");
    command_cat("/system/live/drivers/pit");
    command_cat("/system/live/drivers/rtc");
    command_cat("/system/live/drivers/pci");
    write_text("[devices]\n");
    command_cat("/system/live/devices/storage");
    command_cat("/system/live/devices/display");
    command_cat("/system/live/devices/input");
    command_cat("/system/live/devices/clock");
}

static void command_touch(const char *argument) {
    struct myos_vfs_path_request request;

    if (make_vfs_path_request(&request, argument) == 0 || argument[0] != '/') {
        write_text("Usage: touch <absolute-file>\n");
        return;
    }
    if (system_call(MYOS_SYS_VFS_CREATE_FILE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
        write_text("Unable to create file.\n");
        return;
    }
    write_text("Created "); write_text(request.path); write_char('\n');
}

static void command_mkdir(const char *argument) {
    struct myos_vfs_path_request request;

    if (make_vfs_path_request(&request, argument) == 0 || argument[0] != '/') {
        write_text("Usage: mkdir <absolute-directory>\n");
        return;
    }
    if (system_call(MYOS_SYS_VFS_CREATE_DIRECTORY, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
        write_text("Unable to create directory.\n");
        return;
    }
    write_text("Created directory "); write_text(request.path); write_char('\n');
}

static void command_write(char *argument) {
    struct myos_vfs_path_request path_request;
    struct myos_vfs_write_request request;
    char *text = first_argument(argument);
    uint64_t text_length_value;

    if (make_vfs_path_request(&path_request, argument) == 0 || argument[0] != '/' || text[0] == '\0') {
        write_text("Usage: write <absolute-file> <text>\n");
        return;
    }
    text_length_value = text_length(text);
    if (text_length_value > MYOS_VFS_READ_CHUNK) { write_text("Text is too long.\n"); return; }
    for (uint64_t index = 0U; index < MYOS_VFS_PATH_MAX; index++) { request.path[index] = path_request.path[index]; }
    for (uint64_t index = 0U; index < MYOS_VFS_READ_CHUNK; index++) { request.data[index] = 0U; }
    for (uint64_t index = 0U; index < text_length_value; index++) { request.data[index] = (uint8_t)text[index]; }
    request.offset = 0U;
    request.length = text_length_value;
    (void)system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&path_request, sizeof(path_request));
    if (system_call(MYOS_SYS_VFS_CREATE_FILE, 0U, (uint64_t)(uintptr_t)&path_request, sizeof(path_request)) == UINT64_MAX
        || system_call(MYOS_SYS_VFS_WRITE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
        write_text("Unable to write file.\n");
        return;
    }
    write_text("Wrote "); write_number(text_length_value); write_text(" byte(s) to "); write_text(request.path); write_char('\n');
}

static int project_name_is_valid(const char *name) {
    uint64_t length = 0U;

    while (name[length] != '\0' && length < 31U) {
        const char character = name[length];

        if (!((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z')
              || (character >= '0' && character <= '9') || character == '-' || character == '_')) {
            return 0;
        }
        length++;
    }
    return length != 0U && name[length] == '\0';
}

static int run_foreground(char *argument, int verbose);
static int vfs_lookup_child(const char *parent, const char *name, struct myos_vfs_directory_entry *entry);

static void command_newproj(char *argument) {
    static const char hello_template[] =
        "# MyOS project template\n"
        "write \"Hello from MyOS project\\n\"\n"
        "exit 0\n";
    static const char args_template[] =
        "# MyOS argument project template\n"
        "write \"[\"\n"
        "args\n"
        "write \"]\\n\"\n"
        "exit 0\n";
    const char *template_name = first_argument(argument);
    const char *project_template = (const char *)0;
    char directory[MYOS_VFS_PATH_MAX];
    char source[MYOS_VFS_PATH_MAX];
    struct myos_vfs_path_request directory_request;
    struct myos_vfs_path_request source_request;
    struct myos_vfs_write_request write_request;
    uint64_t directory_length = 0U;
    uint64_t source_length = 0U;
    uint64_t template_length;

    if (template_name[0] == '\0' || text_equal(template_name, "hello") != 0) {
        project_template = hello_template;
    } else if (text_equal(template_name, "args") != 0) {
        project_template = args_template;
    } else {
        write_text("Usage: newproj <project-name> [hello|args]\n");
        return;
    }
    template_length = text_length(project_template);
    if (project_name_is_valid(argument) == 0
        || append_text(directory, sizeof(directory), &directory_length, "/users/myos/projects/") == 0
        || append_text(directory, sizeof(directory), &directory_length, argument) == 0
        || append_text(source, sizeof(source), &source_length, directory) == 0
        || append_text(source, sizeof(source), &source_length, "/main.mya") == 0
        || make_vfs_path_request(&directory_request, directory) == 0
        || make_vfs_path_request(&source_request, source) == 0
        || template_length > MYOS_VFS_READ_CHUNK) {
        write_text("Usage: newproj <project-name> [hello|args]\n");
        return;
    }
    if (system_call(MYOS_SYS_VFS_CREATE_DIRECTORY, 0U, (uint64_t)(uintptr_t)&directory_request,
                    sizeof(directory_request)) == UINT64_MAX) {
        write_text("Unable to create project.\n");
        return;
    }
    if (system_call(MYOS_SYS_VFS_CREATE_FILE, 0U, (uint64_t)(uintptr_t)&source_request,
                    sizeof(source_request)) == UINT64_MAX) {
        (void)system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&directory_request, sizeof(directory_request));
        write_text("Unable to create project template.\n");
        return;
    }
    for (uint64_t index = 0U; index < MYOS_VFS_PATH_MAX; index++) { write_request.path[index] = source_request.path[index]; }
    for (uint64_t index = 0U; index < MYOS_VFS_READ_CHUNK; index++) { write_request.data[index] = 0U; }
    for (uint64_t index = 0U; index < template_length; index++) { write_request.data[index] = (uint8_t)project_template[index]; }
    write_request.offset = 0U;
    write_request.length = template_length;
    if (system_call(MYOS_SYS_VFS_WRITE, 0U, (uint64_t)(uintptr_t)&write_request, sizeof(write_request)) == UINT64_MAX) {
        (void)system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&source_request, sizeof(source_request));
        (void)system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&directory_request, sizeof(directory_request));
        write_text("Unable to write project template.\n");
        return;
    }
    write_text("Created project ");
    write_text(directory);
    write_text("\nTemplate: ");
    write_text(template_name[0] == '\0' ? "hello" : template_name);
    write_text("\nSource: ");
    write_text(source);
    write_text("\nNext: editproj, buildproj, runproj, installproj and run.\n");
}

static void command_editproj(const char *argument) {
    char source[MYOS_VFS_PATH_MAX];
    char program[USER_LINE_CAPACITY] = "edit";
    uint64_t source_length = 0U;
    uint64_t program_length = 4U;

    if (project_name_is_valid(argument) == 0
        || append_text(source, sizeof(source), &source_length, "/users/myos/projects/") == 0
        || append_text(source, sizeof(source), &source_length, argument) == 0
        || append_text(source, sizeof(source), &source_length, "/main.mya") == 0
        || append_text(program, sizeof(program), &program_length, " ") == 0
        || append_text(program, sizeof(program), &program_length, source) == 0) {
        write_text("Usage: editproj <project-name>\n");
        return;
    }
    (void)run_foreground(program, 1);
}

static void command_buildproj(const char *argument) {
    char source[MYOS_VFS_PATH_MAX];
    char output[MYOS_VFS_PATH_MAX];
    char program[USER_LINE_CAPACITY] = "asm";
    uint64_t source_length = 0U;
    uint64_t output_length = 0U;
    uint64_t program_length = 3U;

    if (project_name_is_valid(argument) == 0
        || append_text(source, sizeof(source), &source_length, "/users/myos/projects/") == 0
        || append_text(source, sizeof(source), &source_length, argument) == 0
        || append_text(source, sizeof(source), &source_length, "/main.mya") == 0
        || append_text(output, sizeof(output), &output_length, "/users/myos/projects/") == 0
        || append_text(output, sizeof(output), &output_length, argument) == 0
        || append_text(output, sizeof(output), &output_length, "/main.elf") == 0
        || append_text(program, sizeof(program), &program_length, " ") == 0
        || append_text(program, sizeof(program), &program_length, source) == 0
        || append_text(program, sizeof(program), &program_length, " ") == 0
        || append_text(program, sizeof(program), &program_length, output) == 0) {
        write_text("Usage: buildproj <project-name>\n");
        return;
    }
    (void)run_foreground(program, 1);
}

static void command_runproj(char *argument) {
    char project_directory[MYOS_VFS_PATH_MAX];
    char output[MYOS_VFS_PATH_MAX];
    char program[USER_LINE_CAPACITY];
    char *arguments = first_argument(argument);
    struct myos_vfs_directory_entry entry = { { 0 }, 0U, 0U };
    uint64_t project_length = 0U;
    uint64_t output_length = 0U;
    uint64_t program_length = 0U;
    uint64_t arguments_length = 0U;

    while (arguments[arguments_length] != '\0' && arguments_length + 1U < MYOS_SPAWN_ARGUMENTS_MAX) {
        arguments_length++;
    }
    if (project_name_is_valid(argument) == 0
        || arguments[arguments_length] != '\0'
        || append_text(project_directory, sizeof(project_directory), &project_length, "/users/myos/projects/") == 0
        || append_text(project_directory, sizeof(project_directory), &project_length, argument) == 0
        || append_text(output, sizeof(output), &output_length, project_directory) == 0
        || append_text(output, sizeof(output), &output_length, "/main.elf") == 0
        || append_text(program, sizeof(program), &program_length, output) == 0
        || (arguments_length != 0U
            && (append_text(program, sizeof(program), &program_length, " ") == 0
                || append_text(program, sizeof(program), &program_length, arguments) == 0))) {
        write_text("Usage: runproj <project-name> [arguments] (arguments: at most 127 bytes)\n");
        return;
    }
    if (vfs_lookup_child(project_directory, "main.elf", &entry) == 0) {
        write_text("Build output is missing. Run buildproj first.\n");
        return;
    }
    if (entry.type != MYOS_VFS_OBJECT_REGULAR) {
        write_text("Build output is not a regular file.\n");
        return;
    }
    (void)run_foreground(program, 1);
}

static void command_installproj(const char *argument) {
    char source[MYOS_VFS_PATH_MAX];
    char target[MYOS_VFS_PATH_MAX];
    char program[USER_LINE_CAPACITY] = "install";
    uint64_t source_length = 0U;
    uint64_t target_length = 0U;
    uint64_t program_length = 7U;

    if (project_name_is_valid(argument) == 0
        || append_text(source, sizeof(source), &source_length, "/users/myos/projects/") == 0
        || append_text(source, sizeof(source), &source_length, argument) == 0
        || append_text(source, sizeof(source), &source_length, "/main.elf") == 0
        || append_text(target, sizeof(target), &target_length, "/apps/") == 0
        || append_text(target, sizeof(target), &target_length, argument) == 0
        || append_text(target, sizeof(target), &target_length, "/main.elf") == 0
        || append_text(program, sizeof(program), &program_length, " ") == 0
        || append_text(program, sizeof(program), &program_length, source) == 0
        || append_text(program, sizeof(program), &program_length, " ") == 0
        || append_text(program, sizeof(program), &program_length, target) == 0) {
        write_text("Usage: installproj <project-name>\n");
        return;
    }
    (void)run_foreground(program, 1);
}

static int vfs_lookup_child(const char *parent, const char *name, struct myos_vfs_directory_entry *entry) {
    struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };

    if (copy_vfs_path(request.path, parent) == 0 || name == (const char *)0
        || entry == (struct myos_vfs_directory_entry *)0) {
        return 0;
    }
    for (uint64_t index = 0U; index < UINT64_C(128); index++) {
        request.index = index;
        if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
            return 0;
        }
        if (text_equal(request.entry.name, name) != 0) {
            *entry = request.entry;
            return 1;
        }
    }
    return 0;
}

static void print_project_file_status(const char *label, const char *parent, const char *name) {
    struct myos_vfs_directory_entry entry = { { 0 }, 0U, 0U };

    write_text(label);
    write_text(": ");
    if (vfs_lookup_child(parent, name, &entry) == 0) {
        write_text("MISSING\n");
    } else if (entry.type != MYOS_VFS_OBJECT_REGULAR) {
        write_text("NOT REGULAR\n");
    } else {
        write_text("READY ");
        write_number(entry.size);
        write_text(" bytes\n");
    }
}

static void command_projlist(const char *argument) {
    struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };
    uint64_t project_count = 0U;

    if (argument[0] != '\0' || copy_vfs_path(request.path, "/users/myos/projects") == 0) {
        write_text("Usage: projlist\n");
        return;
    }
    write_text("PROJECTS\n");
    for (uint64_t index = 0U; index < UINT64_C(128); index++) {
        char project_directory[MYOS_VFS_PATH_MAX];
        char package_directory[MYOS_VFS_PATH_MAX];
        uint64_t project_length = 0U;
        uint64_t package_length = 0U;

        request.index = index;
        if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
            break;
        }
        if (request.entry.type != MYOS_VFS_OBJECT_DIRECTORY || project_name_is_valid(request.entry.name) == 0
            || append_text(project_directory, sizeof(project_directory), &project_length, "/users/myos/projects/") == 0
            || append_text(project_directory, sizeof(project_directory), &project_length, request.entry.name) == 0
            || append_text(package_directory, sizeof(package_directory), &package_length, "/apps/") == 0
            || append_text(package_directory, sizeof(package_directory), &package_length, request.entry.name) == 0) {
            continue;
        }
        write_text("PROJECT ");
        write_text(request.entry.name);
        write_char('\n');
        print_project_file_status("  source", project_directory, "main.mya");
        print_project_file_status("  build", project_directory, "main.elf");
        print_project_file_status("  package", package_directory, "main.elf");
        project_count++;
    }
    if (project_count == 0U) { write_text("No bounded projects.\n"); }
}

static void command_projstatus(const char *argument) {
    char project_directory[MYOS_VFS_PATH_MAX];
    char package_directory[MYOS_VFS_PATH_MAX];
    uint64_t project_length = 0U;
    uint64_t package_length = 0U;

    if (project_name_is_valid(argument) == 0
        || append_text(project_directory, sizeof(project_directory), &project_length, "/users/myos/projects/") == 0
        || append_text(project_directory, sizeof(project_directory), &project_length, argument) == 0
        || append_text(package_directory, sizeof(package_directory), &package_length, "/apps/") == 0
        || append_text(package_directory, sizeof(package_directory), &package_length, argument) == 0) {
        write_text("Usage: projstatus <project-name>\n");
        return;
    }
    write_text("PROJECT ");
    write_text(argument);
    write_char('\n');
    print_project_file_status("source", project_directory, "main.mya");
    print_project_file_status("build", project_directory, "main.elf");
    print_project_file_status("package", package_directory, "main.elf");
}

static void command_cleanproj(const char *argument) {
    char project_directory[MYOS_VFS_PATH_MAX];
    char output[MYOS_VFS_PATH_MAX];
    struct myos_vfs_directory_entry entry = { { 0 }, 0U, 0U };
    struct myos_vfs_path_request request;
    uint64_t project_length = 0U;
    uint64_t output_length = 0U;

    if (project_name_is_valid(argument) == 0
        || append_text(project_directory, sizeof(project_directory), &project_length, "/users/myos/projects/") == 0
        || append_text(project_directory, sizeof(project_directory), &project_length, argument) == 0
        || append_text(output, sizeof(output), &output_length, project_directory) == 0
        || append_text(output, sizeof(output), &output_length, "/main.elf") == 0
        || make_vfs_path_request(&request, output) == 0) {
        write_text("Usage: cleanproj <project-name>\n");
        return;
    }
    if (vfs_lookup_child(project_directory, "main.elf", &entry) == 0) {
        write_text("Build output is already absent.\n");
        return;
    }
    if (entry.type != MYOS_VFS_OBJECT_REGULAR
        || system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
        write_text("Unable to remove build output.\n");
        return;
    }
    write_text("Removed build output ");
    write_text(request.path);
    write_char('\n');
}

static void command_uninstallproj(const char *argument) {
    char package_directory[MYOS_VFS_PATH_MAX];
    char output[MYOS_VFS_PATH_MAX];
    struct myos_vfs_directory_entry entry = { { 0 }, 0U, 0U };
    struct myos_vfs_path_request request;
    uint64_t package_length = 0U;
    uint64_t output_length = 0U;

    if (project_name_is_valid(argument) == 0
        || append_text(package_directory, sizeof(package_directory), &package_length, "/apps/") == 0
        || append_text(package_directory, sizeof(package_directory), &package_length, argument) == 0
        || append_text(output, sizeof(output), &output_length, package_directory) == 0
        || append_text(output, sizeof(output), &output_length, "/main.elf") == 0
        || make_vfs_path_request(&request, output) == 0) {
        write_text("Usage: uninstallproj <project-name>\n");
        return;
    }
    if (vfs_lookup_child(package_directory, "main.elf", &entry) == 0) {
        write_text("Package output is already absent.\n");
        return;
    }
    if (entry.type != MYOS_VFS_OBJECT_REGULAR
        || system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
        write_text("Unable to remove package output.\n");
        return;
    }
    write_text("Removed package output ");
    write_text(request.path);
    write_char('\n');
}

static int project_directory_has_only_known_entries(const char *project_directory) {
    struct myos_vfs_list_request request = { 0U, { 0 }, { { 0 }, 0U, 0U } };

    if (copy_vfs_path(request.path, project_directory) == 0) { return 0; }
    for (uint64_t index = 0U; index < UINT64_C(128); index++) {
        request.index = index;
        if (system_call(MYOS_SYS_VFS_LIST, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
            return 1;
        }
        if (text_equal(request.entry.name, "main.mya") == 0 && text_equal(request.entry.name, "main.elf") == 0) {
            return 0;
        }
    }
    return 0;
}

static void command_rmproj(const char *argument) {
    char project_directory[MYOS_VFS_PATH_MAX];
    char source[MYOS_VFS_PATH_MAX];
    struct myos_vfs_directory_entry project_entry = { { 0 }, 0U, 0U };
    struct myos_vfs_directory_entry entry = { { 0 }, 0U, 0U };
    struct myos_vfs_path_request request;
    uint64_t project_length = 0U;
    uint64_t source_length = 0U;

    if (project_name_is_valid(argument) == 0
        || append_text(project_directory, sizeof(project_directory), &project_length, "/users/myos/projects/") == 0
        || append_text(project_directory, sizeof(project_directory), &project_length, argument) == 0
        || append_text(source, sizeof(source), &source_length, project_directory) == 0
        || append_text(source, sizeof(source), &source_length, "/main.mya") == 0
        || make_vfs_path_request(&request, project_directory) == 0) {
        write_text("Usage: rmproj <project-name>\n");
        return;
    }
    if (vfs_lookup_child("/users/myos/projects", argument, &project_entry) == 0) {
        write_text("Project is already absent.\n");
        return;
    }
    if (project_entry.type != MYOS_VFS_OBJECT_DIRECTORY || project_directory_has_only_known_entries(project_directory) == 0) {
        write_text("Project directory is not removable.\n");
        return;
    }
    if (vfs_lookup_child(project_directory, "main.elf", &entry) != 0) {
        if (entry.type != MYOS_VFS_OBJECT_REGULAR) {
            write_text("Build output is not a regular file.\n");
        } else {
            write_text("Build output is present. Run cleanproj first.\n");
        }
        return;
    }
    if (vfs_lookup_child(project_directory, "main.mya", &entry) != 0) {
        if (entry.type != MYOS_VFS_OBJECT_REGULAR || make_vfs_path_request(&request, source) == 0
            || system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
            write_text("Unable to remove project source.\n");
            return;
        }
    }
    if (make_vfs_path_request(&request, project_directory) == 0
        || system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
        write_text("Unable to remove empty project directory.\n");
        return;
    }
    write_text("Removed project directory ");
    write_text(request.path);
    write_char('\n');
}

static void command_rm(const char *argument) {
    struct myos_vfs_path_request request;

    if (make_vfs_path_request(&request, argument) == 0 || argument[0] != '/') {
        write_text("Usage: rm <absolute-file-or-empty-directory>\n");
        return;
    }
    if (system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) == UINT64_MAX) {
        write_text("Unable to remove object.\n");
        return;
    }
    write_text("Removed "); write_text(request.path); write_char('\n');
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
    {
        uint64_t writer_length = 0U;
        uint64_t reader_length = 0U;
        (void)append_text(writer.path, sizeof(writer.path), &writer_length, "/system/core/apps/pipewrite.elf");
        (void)append_text(reader.path, sizeof(reader.path), &reader_length, "/system/core/apps/piperead.elf");
    }
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
            write_text("Unable to prepare command.\n");
        }
        return 0;
    }
    result = system_call(MYOS_SYS_SPAWN, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));
    if (result == UINT64_MAX) {
        if (verbose != 0) {
            write_text("Unable to start program.\n");
        } else {
            write_text("Unable to start command.\n");
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
            write_text("Command wait failed.\n");
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

static void command_install(const char *argument) {
    char program[USER_LINE_CAPACITY] = "install";
    uint64_t length = 7U;

    if (argument[0] == '\0') {
        write_text("Usage: install <source> </apps/name/main.elf>\n");
        return;
    }
    if (length + 1U >= sizeof(program)) {
        write_text("Install command is too long.\n");
        return;
    }
    program[length++] = ' ';
    for (uint64_t index = 0U; argument[index] != '\0'; index++) {
        if (length + 1U >= sizeof(program)) {
            write_text("Install command is too long.\n");
            return;
        }
        program[length++] = argument[index];
    }
    program[length] = '\0';
    (void)run_foreground(program, 1);
}

static void command_cp(const char *argument) {
    char program[USER_LINE_CAPACITY] = "cp";
    uint64_t length = 2U;

    if (argument[0] == '\0') {
        command_help("cp");
        return;
    }
    if (length + 1U >= sizeof(program)) {
        write_text("Copy command is too long.\n");
        return;
    }
    program[length++] = ' ';
    for (uint64_t index = 0U; argument[index] != '\0'; index++) {
        if (length + 1U >= sizeof(program)) {
            write_text("Copy command is too long.\n");
            return;
        }
        program[length++] = argument[index];
    }
    program[length] = '\0';
    (void)run_foreground(program, 0);
}

static void command_wc(const char *argument) {
    char program[USER_LINE_CAPACITY] = "wc";
    uint64_t length = 2U;

    if (argument[0] == '\0') {
        command_help("wc");
        return;
    }
    if (length + 1U >= sizeof(program)) {
        write_text("Word-count command is too long.\n");
        return;
    }
    program[length++] = ' ';
    for (uint64_t index = 0U; argument[index] != '\0'; index++) {
        if (length + 1U >= sizeof(program)) {
            write_text("Word-count command is too long.\n");
            return;
        }
        program[length++] = argument[index];
    }
    program[length] = '\0';
    (void)run_foreground(program, 0);
}

static void command_tree(const char *argument) {
    char program[USER_LINE_CAPACITY] = "tree";
    uint64_t length = 4U;

    if (argument[0] == '\0') {
        (void)run_foreground(program, 0);
        return;
    }
    if (length + 1U >= sizeof(program)) {
        write_text("Tree command is too long.\n");
        return;
    }
    program[length++] = ' ';
    for (uint64_t index = 0U; argument[index] != '\0'; index++) {
        if (length + 1U >= sizeof(program)) {
            write_text("Tree command is too long.\n");
            return;
        }
        program[length++] = argument[index];
    }
    program[length] = '\0';
    (void)run_foreground(program, 0);
}

static void command_find(const char *argument) {
    char program[USER_LINE_CAPACITY] = "find";
    uint64_t length = 4U;

    if (argument[0] == '\0') {
        command_help("find");
        return;
    }
    if (length + 1U >= sizeof(program)) {
        write_text("Find command is too long.\n");
        return;
    }
    program[length++] = ' ';
    for (uint64_t index = 0U; argument[index] != '\0'; index++) {
        if (length + 1U >= sizeof(program)) {
            write_text("Find command is too long.\n");
            return;
        }
        program[length++] = argument[index];
    }
    program[length] = '\0';
    (void)run_foreground(program, 0);
}

static void command_head(const char *argument) {
    char program[USER_LINE_CAPACITY] = "head";
    uint64_t length = 4U;

    if (argument[0] == '\0') {
        command_help("head");
        return;
    }
    if (length + 1U >= sizeof(program)) {
        write_text("Head command is too long.\n");
        return;
    }
    program[length++] = ' ';
    for (uint64_t index = 0U; argument[index] != '\0'; index++) {
        if (length + 1U >= sizeof(program)) {
            write_text("Head command is too long.\n");
            return;
        }
        program[length++] = argument[index];
    }
    program[length] = '\0';
    (void)run_foreground(program, 0);
}

static void command_sort(const char *argument) {
    char program[USER_LINE_CAPACITY] = "sort";
    uint64_t length = 4U;

    if (argument[0] == '\0') {
        command_help("sort");
        return;
    }
    if (length + 1U >= sizeof(program)) {
        write_text("Sort command is too long.\n");
        return;
    }
    program[length++] = ' ';
    for (uint64_t index = 0U; argument[index] != '\0'; index++) {
        if (length + 1U >= sizeof(program)) {
            write_text("Sort command is too long.\n");
            return;
        }
        program[length++] = argument[index];
    }
    program[length] = '\0';
    (void)run_foreground(program, 0);
}

static void command_tail(const char *argument) {
    char program[USER_LINE_CAPACITY] = "tail";
    uint64_t length = 4U;

    if (argument[0] == '\0') {
        command_help("tail");
        return;
    }
    if (length + 1U >= sizeof(program)) {
        write_text("Tail command is too long.\n");
        return;
    }
    program[length++] = ' ';
    for (uint64_t index = 0U; argument[index] != '\0'; index++) {
        if (length + 1U >= sizeof(program)) {
            write_text("Tail command is too long.\n");
            return;
        }
        program[length++] = argument[index];
    }
    program[length] = '\0';
    (void)run_foreground(program, 0);
}

static void command_stat(const char *argument) {
    char program[USER_LINE_CAPACITY] = "stat";
    uint64_t length = 4U;

    if (argument[0] == '\0') {
        command_help("stat");
        return;
    }
    if (length + 1U >= sizeof(program)) {
        write_text("Status command is too long.\n");
        return;
    }
    program[length++] = ' ';
    for (uint64_t index = 0U; argument[index] != '\0'; index++) {
        if (length + 1U >= sizeof(program)) {
            write_text("Status command is too long.\n");
            return;
        }
        program[length++] = argument[index];
    }
    program[length] = '\0';
    (void)run_foreground(program, 0);
}

static void command_grep(const char *argument) {
    char program[USER_LINE_CAPACITY] = "grep";
    uint64_t length = 4U;

    if (argument[0] == '\0') {
        command_help("grep");
        return;
    }
    if (length + 1U >= sizeof(program)) {
        write_text("Search command is too long.\n");
        return;
    }
    program[length++] = ' ';
    for (uint64_t index = 0U; argument[index] != '\0'; index++) {
        if (length + 1U >= sizeof(program)) {
            write_text("Search command is too long.\n");
            return;
        }
        program[length++] = argument[index];
    }
    program[length] = '\0';
    (void)run_foreground(program, 0);
}

static void command_build(const char *argument) {
    char program[USER_LINE_CAPACITY] = "asm";
    uint64_t length = 3U;

    if (argument[0] == '\0') {
        write_text("Usage: build <source.mya> <output.elf>\n");
        return;
    }
    if (length + 1U >= sizeof(program)) {
        write_text("Build command is too long.\n");
        return;
    }
    program[length++] = ' ';
    for (uint64_t index = 0U; argument[index] != '\0'; index++) {
        if (length + 1U >= sizeof(program)) {
            write_text("Build command is too long.\n");
            return;
        }
        program[length++] = argument[index];
    }
    program[length] = '\0';
    (void)run_foreground(program, 1);
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

static void command_edit(const char *argument) {
    char program[USER_LINE_CAPACITY] = "edit";
    uint64_t length = 4U;

    if (argument[0] == '\0') {
        command_help("edit");
        return;
    }
    if (length + 1U >= sizeof(program)) {
        write_text("Editor file path is too long.\n");
        return;
    }
    program[length++] = ' ';
    for (uint64_t index = 0U; argument[index] != '\0'; index++) {
        if (length + 1U >= sizeof(program)) {
            write_text("Editor file path is too long.\n");
            return;
        }
        program[length++] = argument[index];
    }
    program[length] = '\0';
    command_run(program);
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
        write_text("MyOS 0.13.1-gui-preview.1 x86_64\n");
    } else if (text_equal(line, "sysinfo")) {
        command_sysinfo();
    } else if (text_equal(line, "ps")) {
        command_ps();
    } else if (text_equal(line, "meminfo")) {
        command_meminfo();
    } else if (text_equal(line, "date")) {
        command_date();
    } else if (text_equal(line, "uptime")) {
        command_uptime();
    } else if (text_equal(line, "ls")) {
        command_ls(argument);
    } else if (text_equal(line, "cat")) {
        command_cat(argument);
    } else if (text_equal(line, "cp")) {
        command_cp(argument);
    } else if (text_equal(line, "wc")) {
        command_wc(argument);
    } else if (text_equal(line, "grep")) {
        command_grep(argument);
    } else if (text_equal(line, "tree")) {
        command_tree(argument);
    } else if (text_equal(line, "find")) {
        command_find(argument);
    } else if (text_equal(line, "head")) {
        command_head(argument);
    } else if (text_equal(line, "sort")) {
        command_sort(argument);
    } else if (text_equal(line, "tail")) {
        command_tail(argument);
    } else if (text_equal(line, "stat")) {
        command_stat(argument);
    } else if (text_equal(line, "touch")) {
        command_touch(argument);
    } else if (text_equal(line, "mkdir")) {
        command_mkdir(argument);
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
    } else if (text_equal(line, "install")) {
        command_install(argument);
    } else if (text_equal(line, "build")) {
        command_build(argument);
    } else if (text_equal(line, "newproj")) {
        command_newproj(argument);
    } else if (text_equal(line, "editproj")) {
        command_editproj(argument);
    } else if (text_equal(line, "buildproj")) {
        command_buildproj(argument);
    } else if (text_equal(line, "runproj")) {
        command_runproj(argument);
    } else if (text_equal(line, "installproj")) {
        command_installproj(argument);
    } else if (text_equal(line, "uninstallproj")) {
        command_uninstallproj(argument);
    } else if (text_equal(line, "projlist")) {
        command_projlist(argument);
    } else if (text_equal(line, "projstatus")) {
        command_projstatus(argument);
    } else if (text_equal(line, "cleanproj")) {
        command_cleanproj(argument);
    } else if (text_equal(line, "rmproj")) {
        command_rmproj(argument);
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
    } else if (text_equal(line, "edit")) {
        command_edit(argument);
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
    write_text("| MYOS USER SHELL 0.13.1-gui-preview.1                  |\n");
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
