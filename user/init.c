#include <stdint.h>

#include <syscall.h>
#define USER_WRITE_LIMIT UINT64_C(256)
#define USER_LINE_CAPACITY 128U
#define PIT_HZ UINT64_C(100)

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

static uint64_t read_line(char *line, uint64_t capacity) {
    uint64_t length = 0U;

    while (length + 1U < capacity) {
        char character;
        uint64_t result;

        result = system_call(MYOS_SYS_READ, 0U, (uint64_t)(uintptr_t)&character, 1U);
        if (result == 0U || result == UINT64_MAX) {
            continue;
        }
        if (character == '\r' || character == '\n') {
            write_char('\n');
            break;
        }
        if (character == '\b' || character == 0x7FU) {
            if (length != 0U) {
                length--;
                write_text("\b \b");
            }
            continue;
        }
        if (character >= ' ' && character <= '~') {
            line[length] = character;
            length++;
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

static void command_help(void) {
    write_text("Commands: help echo uname ps meminfo date uptime ls cat touch write rm sleep run spawn wait kill stress reboot poweroff dmesg clear exit\n");
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

static void command_touch(const char *argument) {
    struct myos_tmpfs_path_request request;

    if (make_tmpfs_path_request(&request, argument) == 0) {
        write_text("Usage: touch tmp/<name>\n");
        return;
    }
    if (system_call(MYOS_SYS_TMPFS_CREATE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request))
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

    if (make_tmpfs_path_request(&path_request, argument) == 0 || text[0] == '\0') {
        write_text("Usage: write tmp/<name> <text>\n");
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
    (void)system_call(MYOS_SYS_TMPFS_REMOVE, 0U, (uint64_t)(uintptr_t)&path_request,
                      sizeof(path_request));
    if (system_call(MYOS_SYS_TMPFS_CREATE, 0U, (uint64_t)(uintptr_t)&path_request,
                    sizeof(path_request)) == UINT64_MAX
        || system_call(MYOS_SYS_TMPFS_WRITE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request))
               == UINT64_MAX) {
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

    if (make_tmpfs_path_request(&request, argument) == 0) {
        write_text("Usage: rm tmp/<name>\n");
        return;
    }
    if (system_call(MYOS_SYS_TMPFS_REMOVE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request))
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

static void command_spawn(const char *argument) {
    const uint64_t length = text_length(argument);
    uint64_t result;

    if (length == 0U) {
        write_text("Usage: spawn <program>\n");
        return;
    }
    result = system_call(MYOS_SYS_SPAWN, 0U, (uint64_t)(uintptr_t)argument, length);
    if (result == UINT64_MAX) {
        write_text("Unable to start program.\n");
        return;
    }
    write_text("Started background process ");
    write_number(result);
    write_char('\n');
}

static void command_stress(void) {
    static const char sleeper_name[] = "sleeper";
    uint64_t launched = 0U;

    while (launched < MYOS_TASK_SLOT_COUNT) {
        const uint64_t result = system_call(MYOS_SYS_SPAWN, 0U, (uint64_t)(uintptr_t)sleeper_name,
                                            sizeof(sleeper_name) - 1U);

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

static void command_run(const char *argument) {
    const uint64_t length = text_length(argument);
    uint64_t result;
    uint64_t status;

    if (length == 0U) {
        write_text("Usage: run <program>\n");
        return;
    }
    result = system_call(MYOS_SYS_SPAWN, 0U, (uint64_t)(uintptr_t)argument, length);
    if (result == UINT64_MAX) {
        write_text("Unable to start program.\n");
        return;
    }
    write_text("Started process ");
    write_number(result);
    write_text("; waiting for exit...\n");
    status = system_call(MYOS_SYS_WAIT, result, 0U, 0U);
    if (status == UINT64_MAX) {
        write_text("Wait failed.\n");
        return;
    }
    write_text("Process ");
    write_number(result);
    write_text(" exited with status ");
    write_number(status);
    write_char('\n');
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

    argument = first_argument(line);
    if (line[0] == '\0') {
        return;
    }
    if (text_equal(line, "help")) {
        command_help();
    } else if (text_equal(line, "echo")) {
        write_text(argument);
        write_char('\n');
    } else if (text_equal(line, "uname")) {
        write_text("MyOS 0.11.0-dev x86_64\n");
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
    } else if (text_equal(line, "run")) {
        command_run(argument);
    } else if (text_equal(line, "spawn")) {
        command_spawn(argument);
    } else if (text_equal(line, "wait")) {
        command_wait(argument);
    } else if (text_equal(line, "kill")) {
        command_kill(argument);
    } else if (text_equal(line, "stress")) {
        command_stress();
    } else if (text_equal(line, "sleep")) {
        command_sleep(argument);
    } else if (text_equal(line, "reboot")) {
        command_reboot();
    } else if (text_equal(line, "poweroff")) {
        command_poweroff();
    } else if (text_equal(line, "dmesg")) {
        write_text("MyOS: Limine boot, memory manager, scheduler, ring 3 and initramfs active.\n");
    } else if (text_equal(line, "clear")) {
        for (uint64_t index = 0U; index < 48U; index++) {
            write_char('\n');
        }
    } else if (text_equal(line, "exit")) {
        (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    } else {
        write_text("Unknown command: ");
        write_text(line);
        write_char('\n');
    }
}

void _start(void) __attribute__((noreturn));

void _start(void) {
    char line[USER_LINE_CAPACITY];

    write_text("MyOS user shell 0.11.0-dev\n");
    write_text("Type 'help' for available commands.\n");
    for (;;) {
        write_text("myos$ ");
        (void)read_line(line, USER_LINE_CAPACITY);
        execute_command(line);
    }
}
