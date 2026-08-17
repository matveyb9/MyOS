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

        do {
            result = system_call(MYOS_SYS_READ, 0U, (uint64_t)(uintptr_t)&character, 1U);
            __asm__ volatile ("pause");
        } while (result == 0U);
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
    write_text("Commands: help echo uname ps meminfo sleep run dmesg clear exit\n");
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

static void command_meminfo(void) {
    const uint64_t frames = system_call(MYOS_SYS_FREE_FRAMES, 0U, 0U, 0U);

    write_text("Free physical frames: ");
    write_number(frames);
    write_text(" (bytes: ");
    write_number(frames * UINT64_C(4096));
    write_text(")\n");
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
    } else if (text_equal(line, "run")) {
        command_run(argument);
    } else if (text_equal(line, "sleep")) {
        command_sleep(argument);
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
