#include <stdint.h>

#include <syscall.h>

#define SIGNED_MAX_MAGNITUDE UINT64_C(9223372036854775807)
#define SIGNED_MIN_MAGNITUDE UINT64_C(9223372036854775808)

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

static void write_unsigned_number(uint64_t value) {
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

static uint64_t signed_magnitude(int64_t value) {
    if (value >= 0) {
        return (uint64_t)value;
    }
    return (uint64_t)(-(value + 1)) + 1U;
}

static void write_signed_number(int64_t value) {
    if (value < 0) {
        write_text("-");
    }
    write_unsigned_number(signed_magnitude(value));
}

static const char *skip_spaces(const char *text) {
    while (*text == ' ') {
        text++;
    }
    return text;
}

static int parse_number(const char **text, int64_t *value) {
    const char *cursor = *text;
    uint64_t magnitude = 0U;
    uint64_t limit = SIGNED_MAX_MAGNITUDE;
    int negative = 0;
    int seen_digit = 0;

    if (*cursor == '+' || *cursor == '-') {
        negative = *cursor == '-';
        cursor++;
        if (negative != 0) {
            limit = SIGNED_MIN_MAGNITUDE;
        }
    }
    while (*cursor >= '0' && *cursor <= '9') {
        const uint64_t digit = (uint64_t)(*cursor - '0');

        if (magnitude > (limit - digit) / 10U) {
            return 0;
        }
        magnitude = magnitude * 10U + digit;
        cursor++;
        seen_digit = 1;
    }
    if (seen_digit == 0) {
        return 0;
    }
    if (negative != 0) {
        *value = magnitude == SIGNED_MIN_MAGNITUDE ? INT64_MIN : -(int64_t)magnitude;
    } else {
        *value = (int64_t)magnitude;
    }
    *text = cursor;
    return 1;
}

static int multiply_numbers(int64_t left, int64_t right, int64_t *result) {
    const uint64_t left_magnitude = signed_magnitude(left);
    const uint64_t right_magnitude = signed_magnitude(right);
    const int negative = (left < 0) != (right < 0);
    const uint64_t limit = negative != 0 ? SIGNED_MIN_MAGNITUDE : SIGNED_MAX_MAGNITUDE;
    uint64_t magnitude;

    if (left_magnitude != 0U && right_magnitude > limit / left_magnitude) {
        return 0;
    }
    magnitude = left_magnitude * right_magnitude;
    if (negative == 0) {
        *result = (int64_t)magnitude;
    } else {
        *result = magnitude == SIGNED_MIN_MAGNITUDE ? INT64_MIN : -(int64_t)magnitude;
    }
    return 1;
}

void _start(uint64_t argc, const char *arguments) {
    const char *cursor = skip_spaces(arguments);
    int64_t left;
    int64_t right;
    int64_t result = 0;
    char operation;
    const char *error = (const char *)0;
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
        write_text("Usage: calc <signed-integer> <+|-|*|/> <signed-integer>\n");
        write_text("Examples: calc -5 + 2   calc 7 * -6\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    if (operation == '+') {
        if ((right > 0 && left > INT64_MAX - right) || (right < 0 && left < INT64_MIN - right)) {
            error = "calc: addition overflows signed 64-bit integer\n";
        } else {
            result = left + right;
        }
    } else if (operation == '-') {
        if ((right > 0 && left < INT64_MIN + right) || (right < 0 && left > INT64_MAX + right)) {
            error = "calc: subtraction overflows signed 64-bit integer\n";
        } else {
            result = left - right;
        }
    } else if (operation == '*') {
        if (multiply_numbers(left, right, &result) == 0) {
            error = "calc: multiplication overflows signed 64-bit integer\n";
        }
    } else if (right == 0) {
        error = "calc: division by zero\n";
    } else if (left == INT64_MIN && right == -1) {
        error = "calc: division overflows signed 64-bit integer\n";
    } else {
        result = left / right;
    }
    if (error != (const char *)0) {
        write_text(error);
        (void)system_call(MYOS_SYS_EXIT, 3U, 0U, 0U);
    }
    write_text("[calc] result: ");
    write_signed_number(result);
    write_text("\n");
    (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
