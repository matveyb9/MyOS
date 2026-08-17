#include <stdint.h>

#include <syscall.h>

static uint64_t system_call(uint64_t number, uint64_t descriptor, uint64_t buffer, uint64_t length) {
    uint64_t result;

    __asm__ volatile ("syscall"
                      : "=a"(result)
                      : "a"(number), "D"(descriptor), "S"(buffer), "d"(length)
                      : "rcx", "r11", "memory");
    return result;
}

static uint64_t text_length(const char *text) {
    uint64_t length = 0U;

    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static void write_text(const char *text) {
    uint64_t length = text_length(text);

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

static const char *skip_spaces(const char *text) {
    while (*text == ' ') {
        text++;
    }
    return text;
}

static int parse_number(const char **text, uint64_t *value) {
    const char *cursor = *text;
    uint64_t result = 0U;
    int seen_digit = 0;

    while (*cursor >= '0' && *cursor <= '9') {
        const uint64_t digit = (uint64_t)(*cursor - '0');

        if (result > (UINT64_MAX - digit) / 10U) {
            return 0;
        }
        result = result * 10U + digit;
        cursor++;
        seen_digit = 1;
    }
    if (seen_digit == 0) {
        return 0;
    }
    *text = cursor;
    *value = result;
    return 1;
}

void _start(uint64_t argc, const char *arguments) {
    const char *cursor = skip_spaces(arguments);
    uint64_t left;
    uint64_t right;
    uint64_t result;
    char operation;
    int valid = argc == 1U;

    if (valid != 0 && parse_number(&cursor, &left) != 0) {
        cursor = skip_spaces(cursor);
        operation = *cursor;
        if (operation == '+' || operation == '-' || operation == '*' || operation == '/') {
            cursor++;
            cursor = skip_spaces(cursor);
            if (parse_number(&cursor, &right) == 0 || *skip_spaces(cursor) != '\0') {
                valid = 0;
            }
        } else {
            valid = 0;
        }
    } else {
        valid = 0;
    }
    if (valid == 0) {
        write_text("Usage: run calc <non-negative-integer> <+|-|*|/> <non-negative-integer>\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    if (operation == '+') {
        if (left > UINT64_MAX - right) {
            valid = 0;
        } else {
            result = left + right;
        }
    } else if (operation == '-') {
        if (left < right) {
            valid = 0;
        } else {
            result = left - right;
        }
    } else if (operation == '*') {
        if (right != 0U && left > UINT64_MAX / right) {
            valid = 0;
        } else {
            result = left * right;
        }
    } else if (right == 0U) {
        valid = 0;
    } else {
        result = left / right;
    }
    if (valid == 0) {
        write_text("calc: invalid or overflowing operation\n");
        (void)system_call(MYOS_SYS_EXIT, 3U, 0U, 0U);
    }
    write_text("[calc] result: ");
    write_number(result);
    write_text("\n");
    (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
