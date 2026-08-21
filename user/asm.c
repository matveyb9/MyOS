#include <stdint.h>

#include <syscall.h>

#define ASM_SOURCE_CAPACITY UINT64_C(2048)
#define ASM_LITERAL_CAPACITY UINT64_C(2048)
#define ASM_ELF_CAPACITY UINT64_C(8192)
#define ASM_MAX_INSTRUCTIONS UINT64_C(64)
#define ASM_MAX_LABELS UINT64_C(16)
#define ASM_IDENTIFIER_CAPACITY UINT64_C(32)
#define ELF_HEADER_SIZE UINT64_C(64)
#define ELF_PROGRAM_HEADER_SIZE UINT64_C(56)
#define ELF_PAYLOAD_OFFSET UINT64_C(4096)
#define ELF_MACHINE_X86_64 UINT16_C(62)
#define ELF_TYPE_EXEC UINT16_C(2)
#define ELF_PROGRAM_LOAD UINT32_C(1)
#define ELF_FLAG_RX UINT32_C(5)
#define ELF_FLAG_RW UINT32_C(6)
#define USER_IMAGE_BASE UINT64_C(0x400000)
#define USER_IMAGE_DATA_BASE (USER_IMAGE_BASE + UINT64_C(0x1000))
#define USER_IMAGE_DATA_CAPACITY UINT64_C(32)
#define USER_IMAGE_VARIABLE_OFFSET UINT64_C(24)
#define USER_IMAGE_VARIABLE_COUNT UINT64_C(8)
#define NATIVE_ENTRY_PROLOGUE_SIZE UINT64_C(8)

#define NATIVE_INSTRUCTION_WRITE UINT64_C(1)
#define NATIVE_INSTRUCTION_JUMP UINT64_C(2)
#define NATIVE_INSTRUCTION_EXIT UINT64_C(3)
#define NATIVE_INSTRUCTION_SET UINT64_C(4)
#define NATIVE_INSTRUCTION_JUMP_IF_ZERO UINT64_C(5)
#define NATIVE_INSTRUCTION_JUMP_IF_NONZERO UINT64_C(6)
#define NATIVE_INSTRUCTION_INPUT UINT64_C(7)
#define NATIVE_INSTRUCTION_TIME UINT64_C(8)
#define NATIVE_INSTRUCTION_JUMP_IF UINT64_C(9)
#define NATIVE_INSTRUCTION_ARGS UINT64_C(10)
#define NATIVE_INSTRUCTION_STORE UINT64_C(11)
#define NATIVE_INSTRUCTION_LOAD UINT64_C(12)
#define NATIVE_INSTRUCTION_ADD UINT64_C(13)
#define NATIVE_INSTRUCTION_SUB UINT64_C(14)
#define NATIVE_INSTRUCTION_MUL UINT64_C(15)
#define NATIVE_INSTRUCTION_DIV UINT64_C(16)
#define NATIVE_INSTRUCTION_CMP UINT64_C(17)
#define NATIVE_INSTRUCTION_NOT UINT64_C(18)
#define NATIVE_INSTRUCTION_AND UINT64_C(19)
#define NATIVE_INSTRUCTION_OR UINT64_C(20)
#define NATIVE_INSTRUCTION_XOR UINT64_C(21)
#define NATIVE_INSTRUCTION_SHL UINT64_C(22)
#define NATIVE_INSTRUCTION_SHR UINT64_C(23)

struct native_instruction {
    uint64_t kind;
    uint64_t data_offset;
    uint64_t length;
    uint64_t exit_status;
    uint64_t condition_value;
    uint64_t target_label;
    uint64_t target_length;
    uint8_t target[ASM_IDENTIFIER_CAPACITY];
};

struct native_label {
    uint64_t instruction_index;
    uint64_t length;
    uint8_t name[ASM_IDENTIFIER_CAPACITY];
};

static uint8_t source_buffer[ASM_SOURCE_CAPACITY];
static uint8_t literal_buffer[ASM_LITERAL_CAPACITY];
static uint8_t elf_buffer[ASM_ELF_CAPACITY];
static struct native_instruction instructions[ASM_MAX_INSTRUCTIONS];
static struct native_label labels[ASM_MAX_LABELS];

static uint64_t system_call(uint64_t number, uint64_t descriptor, uint64_t buffer, uint64_t length) {
    uint64_t result;
    __asm__ volatile ("syscall" : "=a"(result) : "a"(number), "D"(descriptor), "S"(buffer), "d"(length)
                      : "rcx", "r11", "memory");
    return result;
}

static void write_bytes(const char *text, uint64_t length) {
    while (length != 0U) {
        const uint64_t chunk = length > UINT64_C(256) ? UINT64_C(256) : length;
        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)text, chunk);
        text += chunk;
        length -= chunk;
    }
}

static void write_text(const char *text) {
    uint64_t length = 0U;
    while (text[length] != '\0') { length++; }
    write_bytes(text, length);
}

static void write_number(uint64_t value) {
    char digits[21];
    uint64_t length = 0U;

    if (value == 0U) { write_text("0"); return; }
    while (value != 0U && length < sizeof(digits)) {
        digits[length++] = (char)('0' + value % 10U);
        value /= 10U;
    }
    while (length != 0U) {
        length--;
        write_bytes(&digits[length], 1U);
    }
}

static void put_u16(uint8_t *destination, uint64_t offset, uint16_t value) {
    destination[offset] = (uint8_t)value;
    destination[offset + 1U] = (uint8_t)(value >> 8U);
}

static void put_u32(uint8_t *destination, uint64_t offset, uint32_t value) {
    for (uint64_t byte = 0U; byte < 4U; byte++) {
        destination[offset + byte] = (uint8_t)(value >> (byte * 8U));
    }
}

static void put_u64(uint8_t *destination, uint64_t offset, uint64_t value) {
    for (uint64_t byte = 0U; byte < 8U; byte++) {
        destination[offset + byte] = (uint8_t)(value >> (byte * 8U));
    }
}

static int copy_path(char *destination, uint64_t capacity, const char *source, uint64_t *position) {
    uint64_t length = 0U;

    if (destination == (char *)0 || source == (const char *)0 || position == (uint64_t *)0 || source[*position] != '/') {
        return 0;
    }
    while (source[*position] != '\0' && source[*position] != ' ') {
        if (length + 1U >= capacity) { return 0; }
        destination[length++] = source[(*position)++];
    }
    destination[length] = '\0';
    return length != 0U;
}

static void skip_statement_space(const uint8_t *text, uint64_t length, uint64_t *position) {
    while (*position < length && (text[*position] == ' ' || text[*position] == '\t'
                                  || text[*position] == '\r' || text[*position] == '\n'
                                  || text[*position] == ';')) {
        (*position)++;
    }
}

static void skip_inline_space(const uint8_t *text, uint64_t length, uint64_t *position) {
    while (*position < length && (text[*position] == ' ' || text[*position] == '\t')) {
        (*position)++;
    }
}

static int finish_statement(uint64_t source_length, uint64_t *position) {
    skip_inline_space(source_buffer, source_length, position);
    if (*position >= source_length) { return 1; }
    if (source_buffer[*position] == '#') {
        while (*position < source_length && source_buffer[*position] != '\n') { (*position)++; }
        return 1;
    }
    if (source_buffer[*position] == ';' || source_buffer[*position] == '\n' || source_buffer[*position] == '\r') {
        (*position)++;
        return 1;
    }
    return 0;
}

static int word_is(const uint8_t *text, uint64_t start, uint64_t length, const char *word) {
    uint64_t index = 0U;

    while (word[index] != '\0' && index < length) {
        if (text[start + index] != (uint8_t)word[index]) { return 0; }
        index++;
    }
    return index == length && word[index] == '\0';
}

static int identifier_start(uint8_t value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || value == '_';
}

static int identifier_continue(uint8_t value) {
    return identifier_start(value) || (value >= '0' && value <= '9');
}

static int parse_identifier(uint64_t source_length, uint64_t *position, uint8_t *destination, uint64_t *identifier_length) {
    uint64_t length = 0U;

    if (*position >= source_length || identifier_start(source_buffer[*position]) == 0) { return 0; }
    while (*position < source_length && identifier_continue(source_buffer[*position]) != 0) {
        if (length + 1U >= ASM_IDENTIFIER_CAPACITY) { return 0; }
        destination[length++] = source_buffer[(*position)++];
    }
    destination[length] = 0U;
    *identifier_length = length;
    return 1;
}

static int parse_decimal(const uint8_t *text, uint64_t length, uint64_t *position, uint64_t *value) {
    uint64_t result = 0U;
    uint64_t digits = 0U;

    while (*position < length && text[*position] >= '0' && text[*position] <= '9') {
        const uint64_t digit = (uint64_t)(text[*position] - '0');
        if (result > (UINT64_MAX - digit) / 10U) { return 0; }
        result = result * 10U + digit;
        (*position)++;
        digits++;
    }
    if (digits == 0U) { return 0; }
    *value = result;
    return 1;
}

static int parse_string(uint64_t source_length, uint64_t *position, uint64_t *literal_length) {
    uint64_t start;

    if (*position >= source_length || source_buffer[*position] != '"' || *literal_length >= ASM_LITERAL_CAPACITY) {
        return 0;
    }
    (*position)++;
    start = *literal_length;
    while (*position < source_length && source_buffer[*position] != '"') {
        uint8_t value = source_buffer[*position];

        if (value == '\\') {
            (*position)++;
            if (*position >= source_length) { return 0; }
            value = source_buffer[*position];
            if (value == 'n') { value = '\n'; }
            else if (value == 'r') { value = '\r'; }
            else if (value == 't') { value = '\t'; }
            else if (value != '\\' && value != '"') { return 0; }
        }
        if (value < ' ' && value != '\n' && value != '\r' && value != '\t') { return 0; }
        if (*literal_length >= ASM_LITERAL_CAPACITY) { return 0; }
        literal_buffer[(*literal_length)++] = value;
        (*position)++;
    }
    if (*position >= source_length || source_buffer[*position] != '"' || *literal_length == start) { return 0; }
    (*position)++;
    return 1;
}

static int label_matches(uint64_t label_index, const uint8_t *name, uint64_t name_length) {
    if (labels[label_index].length != name_length) { return 0; }
    for (uint64_t index = 0U; index < name_length; index++) {
        if (labels[label_index].name[index] != name[index]) { return 0; }
    }
    return 1;
}

static int add_label(const uint8_t *name, uint64_t name_length, uint64_t instruction_index, uint64_t *label_count) {
    if (*label_count >= ASM_MAX_LABELS) { return 0; }
    for (uint64_t index = 0U; index < *label_count; index++) {
        if (label_matches(index, name, name_length) != 0) { return 0; }
    }
    labels[*label_count].instruction_index = instruction_index;
    labels[*label_count].length = name_length;
    for (uint64_t index = 0U; index <= name_length; index++) { labels[*label_count].name[index] = name[index]; }
    (*label_count)++;
    return 1;
}

static int find_label(const uint8_t *name, uint64_t name_length, uint64_t label_count, uint64_t *label_index) {
    for (uint64_t index = 0U; index < label_count; index++) {
        if (label_matches(index, name, name_length) != 0) {
            *label_index = index;
            return 1;
        }
    }
    return 0;
}

static int load_source(const char *path, uint64_t *source_length) {
    struct myos_vfs_read_request request = { 0U, { 0 }, { 0 } };
    uint64_t path_length = 0U;
    uint64_t total = 0U;

    while (path[path_length] != '\0' && path_length + 1U < sizeof(request.path)) {
        request.path[path_length] = path[path_length];
        path_length++;
    }
    if (path_length == 0U || path[path_length] != '\0') { return 0; }
    request.path[path_length] = '\0';
    for (;;) {
        uint64_t count;

        request.offset = total;
        count = system_call(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)&request, sizeof(request));
        if (count == UINT64_MAX || count > MYOS_VFS_READ_CHUNK || count > ASM_SOURCE_CAPACITY - 1U - total) {
            return 0;
        }
        for (uint64_t index = 0U; index < count; index++) { source_buffer[total + index] = request.data[index]; }
        total += count;
        if (count == 0U) { break; }
    }
    source_buffer[total] = '\0';
    *source_length = total;
    return total != 0U;
}

static int parse_source(uint64_t source_length, uint64_t *instruction_count, uint64_t *label_count,
                        uint64_t *literal_length) {
    uint64_t position = 0U;
    int saw_exit = 0;
    int condition_ready = 0;

    *instruction_count = 0U;
    *label_count = 0U;
    *literal_length = 0U;
    while (position < source_length) {
        uint64_t start;
        uint64_t word_length;

        skip_statement_space(source_buffer, source_length, &position);
        if (position >= source_length) { break; }
        if (source_buffer[position] == '#') {
            while (position < source_length && source_buffer[position] != '\n') { position++; }
            continue;
        }
        if (saw_exit != 0) { return 0; }
        start = position;
        while (position < source_length && identifier_continue(source_buffer[position]) != 0) {
            position++;
        }
        word_length = position - start;
        if (word_length == 0U) { return 0; }
        if (word_is(source_buffer, start, word_length, "label") != 0) {
            uint8_t name[ASM_IDENTIFIER_CAPACITY] = { 0 };
            uint64_t name_length;

            skip_inline_space(source_buffer, source_length, &position);
            if (parse_identifier(source_length, &position, name, &name_length) == 0) { return 0; }
            skip_inline_space(source_buffer, source_length, &position);
            if (position >= source_length || source_buffer[position++] != ':') { return 0; }
            if (add_label(name, name_length, *instruction_count, label_count) == 0
                || finish_statement(source_length, &position) == 0) {
                return 0;
            }
            continue;
        }
        if (*instruction_count >= ASM_MAX_INSTRUCTIONS) { return 0; }
        if (word_is(source_buffer, start, word_length, "write") != 0) {
            struct native_instruction *instruction = &instructions[*instruction_count];
            uint64_t data_start;

            skip_inline_space(source_buffer, source_length, &position);
            data_start = *literal_length;
            if (parse_string(source_length, &position, literal_length) == 0) { return 0; }
            instruction->kind = NATIVE_INSTRUCTION_WRITE;
            instruction->data_offset = data_start;
            instruction->length = *literal_length - data_start;
        } else if (word_is(source_buffer, start, word_length, "set") != 0) {
            struct native_instruction *instruction = &instructions[*instruction_count];

            skip_inline_space(source_buffer, source_length, &position);
            if (parse_decimal(source_buffer, source_length, &position, &instruction->condition_value) == 0
                || instruction->condition_value > UINT64_C(255)) {
                return 0;
            }
            instruction->kind = NATIVE_INSTRUCTION_SET;
            condition_ready = 1;
        } else if (word_is(source_buffer, start, word_length, "add") != 0
                   || word_is(source_buffer, start, word_length, "sub") != 0
                   || word_is(source_buffer, start, word_length, "mul") != 0
                   || word_is(source_buffer, start, word_length, "div") != 0
                   || word_is(source_buffer, start, word_length, "and") != 0
                   || word_is(source_buffer, start, word_length, "or") != 0
                   || word_is(source_buffer, start, word_length, "xor") != 0
                   || word_is(source_buffer, start, word_length, "shl") != 0
                   || word_is(source_buffer, start, word_length, "shr") != 0) {
            struct native_instruction *instruction = &instructions[*instruction_count];

            if (condition_ready == 0) { return 0; }
            skip_inline_space(source_buffer, source_length, &position);
            if (parse_decimal(source_buffer, source_length, &position, &instruction->condition_value) == 0
                || instruction->condition_value > UINT64_C(255)
                || (word_is(source_buffer, start, word_length, "div") != 0 && instruction->condition_value == 0U)
                || ((word_is(source_buffer, start, word_length, "shl") != 0
                     || word_is(source_buffer, start, word_length, "shr") != 0)
                    && (instruction->condition_value == 0U || instruction->condition_value > UINT64_C(7)))) {
                return 0;
            }
            if (word_is(source_buffer, start, word_length, "add") != 0) {
                instruction->kind = NATIVE_INSTRUCTION_ADD;
            } else if (word_is(source_buffer, start, word_length, "sub") != 0) {
                instruction->kind = NATIVE_INSTRUCTION_SUB;
            } else if (word_is(source_buffer, start, word_length, "mul") != 0) {
                instruction->kind = NATIVE_INSTRUCTION_MUL;
            } else if (word_is(source_buffer, start, word_length, "div") != 0) {
                instruction->kind = NATIVE_INSTRUCTION_DIV;
            } else if (word_is(source_buffer, start, word_length, "and") != 0) {
                instruction->kind = NATIVE_INSTRUCTION_AND;
            } else if (word_is(source_buffer, start, word_length, "or") != 0) {
                instruction->kind = NATIVE_INSTRUCTION_OR;
            } else if (word_is(source_buffer, start, word_length, "xor") != 0) {
                instruction->kind = NATIVE_INSTRUCTION_XOR;
            } else if (word_is(source_buffer, start, word_length, "shl") != 0) {
                instruction->kind = NATIVE_INSTRUCTION_SHL;
            } else {
                instruction->kind = NATIVE_INSTRUCTION_SHR;
            }
        } else if (word_is(source_buffer, start, word_length, "not") != 0) {
            if (condition_ready == 0) { return 0; }
            instructions[*instruction_count].kind = NATIVE_INSTRUCTION_NOT;
        } else if (word_is(source_buffer, start, word_length, "store") != 0
                   || word_is(source_buffer, start, word_length, "load") != 0
                   || word_is(source_buffer, start, word_length, "cmp") != 0) {
            struct native_instruction *instruction = &instructions[*instruction_count];

            if (word_is(source_buffer, start, word_length, "cmp") != 0 && condition_ready == 0) { return 0; }
            skip_inline_space(source_buffer, source_length, &position);
            if (parse_decimal(source_buffer, source_length, &position, &instruction->condition_value) == 0
                || instruction->condition_value >= USER_IMAGE_VARIABLE_COUNT) {
                return 0;
            }
            if (word_is(source_buffer, start, word_length, "store") != 0) {
                instruction->kind = NATIVE_INSTRUCTION_STORE;
            } else if (word_is(source_buffer, start, word_length, "load") != 0) {
                instruction->kind = NATIVE_INSTRUCTION_LOAD;
                condition_ready = 1;
            } else {
                instruction->kind = NATIVE_INSTRUCTION_CMP;
            }
        } else if (word_is(source_buffer, start, word_length, "input") != 0) {
            instructions[*instruction_count].kind = NATIVE_INSTRUCTION_INPUT;
            condition_ready = 1;
        } else if (word_is(source_buffer, start, word_length, "time") != 0) {
            instructions[*instruction_count].kind = NATIVE_INSTRUCTION_TIME;
        } else if (word_is(source_buffer, start, word_length, "args") != 0) {
            instructions[*instruction_count].kind = NATIVE_INSTRUCTION_ARGS;
        } else if (word_is(source_buffer, start, word_length, "jump") != 0
                   || word_is(source_buffer, start, word_length, "jump_if_zero") != 0
                   || word_is(source_buffer, start, word_length, "jump_if_nonzero") != 0
                   || word_is(source_buffer, start, word_length, "jump_if") != 0) {
            struct native_instruction *instruction = &instructions[*instruction_count];
            const int conditional = word_is(source_buffer, start, word_length, "jump_if_zero") != 0
                                    || word_is(source_buffer, start, word_length, "jump_if_nonzero") != 0
                                    || word_is(source_buffer, start, word_length, "jump_if") != 0;

            if (conditional != 0 && condition_ready == 0) {
                return 0;
            }
            skip_inline_space(source_buffer, source_length, &position);
            if (word_is(source_buffer, start, word_length, "jump_if") != 0) {
                if (parse_decimal(source_buffer, source_length, &position, &instruction->condition_value) == 0
                    || instruction->condition_value > UINT64_C(255)) {
                    return 0;
                }
                skip_inline_space(source_buffer, source_length, &position);
            }
            if (parse_identifier(source_length, &position, instruction->target, &instruction->target_length) == 0) {
                return 0;
            }
            if (word_is(source_buffer, start, word_length, "jump") != 0) {
                instruction->kind = NATIVE_INSTRUCTION_JUMP;
            } else if (word_is(source_buffer, start, word_length, "jump_if_zero") != 0) {
                instruction->kind = NATIVE_INSTRUCTION_JUMP_IF_ZERO;
            } else if (word_is(source_buffer, start, word_length, "jump_if_nonzero") != 0) {
                instruction->kind = NATIVE_INSTRUCTION_JUMP_IF_NONZERO;
            } else {
                instruction->kind = NATIVE_INSTRUCTION_JUMP_IF;
            }
            instruction->target_label = UINT64_MAX;
        } else if (word_is(source_buffer, start, word_length, "exit") != 0) {
            struct native_instruction *instruction = &instructions[*instruction_count];

            skip_inline_space(source_buffer, source_length, &position);
            if (parse_decimal(source_buffer, source_length, &position, &instruction->exit_status) == 0
                || instruction->exit_status > UINT64_C(255)) {
                return 0;
            }
            instruction->kind = NATIVE_INSTRUCTION_EXIT;
            saw_exit = 1;
        } else {
            return 0;
        }
        if (finish_statement(source_length, &position) == 0) { return 0; }
        (*instruction_count)++;
    }
    return saw_exit != 0;
}

static uint64_t instruction_size(uint64_t kind) {
    if (kind == NATIVE_INSTRUCTION_WRITE) { return UINT64_C(32); }
    if (kind == NATIVE_INSTRUCTION_JUMP) { return UINT64_C(5); }
    if (kind == NATIVE_INSTRUCTION_SET) { return UINT64_C(5); }
    if (kind == NATIVE_INSTRUCTION_JUMP_IF_ZERO || kind == NATIVE_INSTRUCTION_JUMP_IF_NONZERO) { return UINT64_C(8); }
    if (kind == NATIVE_INSTRUCTION_INPUT) { return UINT64_C(63); }
    if (kind == NATIVE_INSTRUCTION_TIME) { return UINT64_C(142); }
    if (kind == NATIVE_INSTRUCTION_JUMP_IF) { return UINT64_C(12); }
    if (kind == NATIVE_INSTRUCTION_ARGS) { return UINT64_C(42); }
    if (kind == NATIVE_INSTRUCTION_STORE) { return UINT64_C(7); }
    if (kind == NATIVE_INSTRUCTION_LOAD) { return UINT64_C(8); }
    if (kind == NATIVE_INSTRUCTION_ADD || kind == NATIVE_INSTRUCTION_SUB) { return UINT64_C(3); }
    if (kind == NATIVE_INSTRUCTION_MUL) { return UINT64_C(11); }
    if (kind == NATIVE_INSTRUCTION_DIV) { return UINT64_C(14); }
    if (kind == NATIVE_INSTRUCTION_CMP) { return UINT64_C(13); }
    if (kind == NATIVE_INSTRUCTION_NOT) { return UINT64_C(2); }
    if (kind == NATIVE_INSTRUCTION_AND || kind == NATIVE_INSTRUCTION_OR || kind == NATIVE_INSTRUCTION_XOR
        || kind == NATIVE_INSTRUCTION_SHL || kind == NATIVE_INSTRUCTION_SHR) { return UINT64_C(3); }
    if (kind == NATIVE_INSTRUCTION_EXIT) { return UINT64_C(20); }
    return 0U;
}

static int resolve_jumps(uint64_t instruction_count, uint64_t label_count) {
    for (uint64_t index = 0U; index < instruction_count; index++) {
        uint64_t label_index;

        if (instructions[index].kind != NATIVE_INSTRUCTION_JUMP
            && instructions[index].kind != NATIVE_INSTRUCTION_JUMP_IF_ZERO
            && instructions[index].kind != NATIVE_INSTRUCTION_JUMP_IF_NONZERO
            && instructions[index].kind != NATIVE_INSTRUCTION_JUMP_IF) { continue; }
        if (find_label(instructions[index].target, instructions[index].target_length, label_count, &label_index) == 0
            || labels[label_index].instruction_index <= index) {
            return 0;
        }
        instructions[index].target_label = label_index;
    }
    return 1;
}

static int build_elf(uint64_t instruction_count, uint64_t literal_length, uint64_t *image_length) {
    uint64_t instruction_offsets[ASM_MAX_INSTRUCTIONS + 1U];
    uint64_t code_length = NATIVE_ENTRY_PROLOGUE_SIZE;
    uint64_t payload_length;

    for (uint64_t index = 0U; index < instruction_count; index++) {
        const uint64_t size = instruction_size(instructions[index].kind);

        if (size == 0U || code_length > ASM_ELF_CAPACITY - ELF_PAYLOAD_OFFSET - size) { return 0; }
        instruction_offsets[index] = code_length;
        code_length += size;
    }
    instruction_offsets[instruction_count] = code_length;
    payload_length = code_length + literal_length;
    if (payload_length > ASM_ELF_CAPACITY - ELF_PAYLOAD_OFFSET) { return 0; }

    for (uint64_t index = 0U; index < ASM_ELF_CAPACITY; index++) { elf_buffer[index] = 0U; }
    elf_buffer[0] = 0x7FU; elf_buffer[1] = 'E'; elf_buffer[2] = 'L'; elf_buffer[3] = 'F';
    elf_buffer[4] = 2U; elf_buffer[5] = 1U; elf_buffer[6] = 1U;
    put_u16(elf_buffer, 16U, ELF_TYPE_EXEC);
    put_u16(elf_buffer, 18U, ELF_MACHINE_X86_64);
    put_u32(elf_buffer, 20U, 1U);
    put_u64(elf_buffer, 24U, USER_IMAGE_BASE);
    put_u64(elf_buffer, 32U, ELF_HEADER_SIZE);
    put_u16(elf_buffer, 52U, (uint16_t)ELF_HEADER_SIZE);
    put_u16(elf_buffer, 54U, (uint16_t)ELF_PROGRAM_HEADER_SIZE);
    put_u16(elf_buffer, 56U, 2U);
    put_u32(elf_buffer, ELF_HEADER_SIZE, ELF_PROGRAM_LOAD);
    put_u32(elf_buffer, ELF_HEADER_SIZE + 4U, ELF_FLAG_RX);
    put_u64(elf_buffer, ELF_HEADER_SIZE + 8U, ELF_PAYLOAD_OFFSET);
    put_u64(elf_buffer, ELF_HEADER_SIZE + 16U, USER_IMAGE_BASE);
    put_u64(elf_buffer, ELF_HEADER_SIZE + 24U, USER_IMAGE_BASE);
    put_u64(elf_buffer, ELF_HEADER_SIZE + 32U, payload_length);
    put_u64(elf_buffer, ELF_HEADER_SIZE + 40U, payload_length);
    put_u64(elf_buffer, ELF_HEADER_SIZE + 48U, UINT64_C(4096));
    put_u32(elf_buffer, ELF_HEADER_SIZE + ELF_PROGRAM_HEADER_SIZE, ELF_PROGRAM_LOAD);
    put_u32(elf_buffer, ELF_HEADER_SIZE + ELF_PROGRAM_HEADER_SIZE + 4U, ELF_FLAG_RW);
    put_u64(elf_buffer, ELF_HEADER_SIZE + ELF_PROGRAM_HEADER_SIZE + 8U, ELF_PAYLOAD_OFFSET + payload_length);
    put_u64(elf_buffer, ELF_HEADER_SIZE + ELF_PROGRAM_HEADER_SIZE + 16U, USER_IMAGE_DATA_BASE);
    put_u64(elf_buffer, ELF_HEADER_SIZE + ELF_PROGRAM_HEADER_SIZE + 24U, USER_IMAGE_DATA_BASE);
    put_u64(elf_buffer, ELF_HEADER_SIZE + ELF_PROGRAM_HEADER_SIZE + 32U, 0U);
    put_u64(elf_buffer, ELF_HEADER_SIZE + ELF_PROGRAM_HEADER_SIZE + 40U, USER_IMAGE_DATA_CAPACITY);
    put_u64(elf_buffer, ELF_HEADER_SIZE + ELF_PROGRAM_HEADER_SIZE + 48U, UINT64_C(4096));

    elf_buffer[ELF_PAYLOAD_OFFSET] = 0x48U;
    elf_buffer[ELF_PAYLOAD_OFFSET + 1U] = 0x89U;
    elf_buffer[ELF_PAYLOAD_OFFSET + 2U] = 0x34U;
    elf_buffer[ELF_PAYLOAD_OFFSET + 3U] = 0x25U;
    put_u32(elf_buffer, ELF_PAYLOAD_OFFSET + 4U, (uint32_t)USER_IMAGE_DATA_BASE);

    for (uint64_t index = 0U; index < instruction_count; index++) {
        uint64_t code = ELF_PAYLOAD_OFFSET + instruction_offsets[index];

        if (instructions[index].kind == NATIVE_INSTRUCTION_WRITE) {
            const uint64_t address = USER_IMAGE_BASE + code_length + instructions[index].data_offset;

            elf_buffer[code++] = 0xB8U; put_u32(elf_buffer, code, 1U); code += 4U;
            elf_buffer[code++] = 0xBFU; put_u32(elf_buffer, code, 1U); code += 4U;
            elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0xBEU; put_u64(elf_buffer, code, address); code += 8U;
            elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0xBAU; put_u64(elf_buffer, code, instructions[index].length); code += 8U;
            elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0x05U;
        } else if (instructions[index].kind == NATIVE_INSTRUCTION_JUMP) {
            const uint64_t target_offset = instruction_offsets[labels[instructions[index].target_label].instruction_index];
            const uint64_t next_offset = instruction_offsets[index] + UINT64_C(5);
            const int64_t relative = (int64_t)(target_offset - next_offset);

            elf_buffer[code++] = 0xE9U;
            put_u32(elf_buffer, code, (uint32_t)relative);
            code += 4U;
        } else if (instructions[index].kind == NATIVE_INSTRUCTION_SET) {
            elf_buffer[code++] = 0xBBU;
            put_u32(elf_buffer, code, (uint32_t)instructions[index].condition_value);
            code += 4U;
        } else if (instructions[index].kind == NATIVE_INSTRUCTION_ADD
                   || instructions[index].kind == NATIVE_INSTRUCTION_SUB) {
            elf_buffer[code++] = 0x80U;
            elf_buffer[code++] = instructions[index].kind == NATIVE_INSTRUCTION_ADD ? 0xC3U : 0xEBU;
            elf_buffer[code++] = (uint8_t)instructions[index].condition_value;
        } else if (instructions[index].kind == NATIVE_INSTRUCTION_NOT) {
            elf_buffer[code++] = 0xF6U; elf_buffer[code++] = 0xD3U;
        } else if (instructions[index].kind == NATIVE_INSTRUCTION_AND
                   || instructions[index].kind == NATIVE_INSTRUCTION_OR
                   || instructions[index].kind == NATIVE_INSTRUCTION_XOR) {
            elf_buffer[code++] = 0x80U;
            if (instructions[index].kind == NATIVE_INSTRUCTION_AND) {
                elf_buffer[code++] = 0xE3U;
            } else if (instructions[index].kind == NATIVE_INSTRUCTION_OR) {
                elf_buffer[code++] = 0xCBU;
            } else {
                elf_buffer[code++] = 0xF3U;
            }
            elf_buffer[code++] = (uint8_t)instructions[index].condition_value;
        } else if (instructions[index].kind == NATIVE_INSTRUCTION_SHL
                   || instructions[index].kind == NATIVE_INSTRUCTION_SHR) {
            elf_buffer[code++] = 0xC0U;
            elf_buffer[code++] = instructions[index].kind == NATIVE_INSTRUCTION_SHL ? 0xE3U : 0xEBU;
            elf_buffer[code++] = (uint8_t)instructions[index].condition_value;
        } else if (instructions[index].kind == NATIVE_INSTRUCTION_MUL) {
            elf_buffer[code++] = 0x89U; elf_buffer[code++] = 0xD8U;
            elf_buffer[code++] = 0x69U; elf_buffer[code++] = 0xC0U;
            put_u32(elf_buffer, code, (uint32_t)instructions[index].condition_value); code += 4U;
            elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0xB6U; elf_buffer[code++] = 0xD8U;
        } else if (instructions[index].kind == NATIVE_INSTRUCTION_DIV) {
            elf_buffer[code++] = 0x89U; elf_buffer[code++] = 0xD8U;
            elf_buffer[code++] = 0x31U; elf_buffer[code++] = 0xD2U;
            elf_buffer[code++] = 0xB9U; put_u32(elf_buffer, code, (uint32_t)instructions[index].condition_value); code += 4U;
            elf_buffer[code++] = 0xF7U; elf_buffer[code++] = 0xF1U;
            elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0xB6U; elf_buffer[code++] = 0xD8U;
        } else if (instructions[index].kind == NATIVE_INSTRUCTION_CMP) {
            elf_buffer[code++] = 0x3AU; elf_buffer[code++] = 0x1CU; elf_buffer[code++] = 0x25U;
            put_u32(elf_buffer, code, (uint32_t)(USER_IMAGE_DATA_BASE + USER_IMAGE_VARIABLE_OFFSET
                                                  + instructions[index].condition_value));
            code += 4U;
            elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0x95U; elf_buffer[code++] = 0xC3U;
            elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0xB6U; elf_buffer[code++] = 0xDBU;
        } else if (instructions[index].kind == NATIVE_INSTRUCTION_JUMP_IF_ZERO
                   || instructions[index].kind == NATIVE_INSTRUCTION_JUMP_IF_NONZERO) {
            const uint64_t target_offset = instruction_offsets[labels[instructions[index].target_label].instruction_index];
            const uint64_t next_offset = instruction_offsets[index] + UINT64_C(8);
            const int64_t relative = (int64_t)(target_offset - next_offset);

            elf_buffer[code++] = 0x85U;
            elf_buffer[code++] = 0xDBU;
            elf_buffer[code++] = 0x0FU;
            elf_buffer[code++] = instructions[index].kind == NATIVE_INSTRUCTION_JUMP_IF_ZERO ? 0x84U : 0x85U;
            put_u32(elf_buffer, code, (uint32_t)relative);
            code += 4U;
        } else if (instructions[index].kind == NATIVE_INSTRUCTION_JUMP_IF) {
            const uint64_t target_offset = instruction_offsets[labels[instructions[index].target_label].instruction_index];
            const uint64_t next_offset = instruction_offsets[index] + UINT64_C(12);
            const int64_t relative = (int64_t)(target_offset - next_offset);

            elf_buffer[code++] = 0x81U; elf_buffer[code++] = 0xFBU;
            put_u32(elf_buffer, code, (uint32_t)instructions[index].condition_value); code += 4U;
            elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0x84U;
            put_u32(elf_buffer, code, (uint32_t)relative); code += 4U;
        } else if (instructions[index].kind == NATIVE_INSTRUCTION_ARGS) {
            elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0x8BU; elf_buffer[code++] = 0x34U; elf_buffer[code++] = 0x25U;
            put_u32(elf_buffer, code, (uint32_t)USER_IMAGE_DATA_BASE); code += 4U;
            elf_buffer[code++] = 0x31U; elf_buffer[code++] = 0xD2U;
            elf_buffer[code++] = 0x80U; elf_buffer[code++] = 0x3CU; elf_buffer[code++] = 0x16U; elf_buffer[code++] = 0U;
            elf_buffer[code++] = 0x74U; elf_buffer[code++] = 9U;
            elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0xFFU; elf_buffer[code++] = 0xC2U;
            elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0x83U; elf_buffer[code++] = 0xFAU; elf_buffer[code++] = 127U;
            elf_buffer[code++] = 0x72U; elf_buffer[code++] = (uint8_t)-15;
            elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0x85U; elf_buffer[code++] = 0xD2U;
            elf_buffer[code++] = 0x74U; elf_buffer[code++] = 12U;
            elf_buffer[code++] = 0xB8U; put_u32(elf_buffer, code, 1U); code += 4U;
            elf_buffer[code++] = 0xBFU; put_u32(elf_buffer, code, 1U); code += 4U;
            elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0x05U;
        } else if (instructions[index].kind == NATIVE_INSTRUCTION_STORE) {
            elf_buffer[code++] = 0x88U; elf_buffer[code++] = 0x1CU; elf_buffer[code++] = 0x25U;
            put_u32(elf_buffer, code, (uint32_t)(USER_IMAGE_DATA_BASE + USER_IMAGE_VARIABLE_OFFSET
                                                  + instructions[index].condition_value));
            code += 4U;
        } else if (instructions[index].kind == NATIVE_INSTRUCTION_LOAD) {
            elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0xB6U;
            elf_buffer[code++] = 0x1CU; elf_buffer[code++] = 0x25U;
            put_u32(elf_buffer, code, (uint32_t)(USER_IMAGE_DATA_BASE + USER_IMAGE_VARIABLE_OFFSET
                                                  + instructions[index].condition_value));
            code += 4U;
        } else if (instructions[index].kind == NATIVE_INSTRUCTION_INPUT) {
            const uint64_t input_start = ELF_PAYLOAD_OFFSET + instruction_offsets[index];
            int64_t relative;

            elf_buffer[code++] = 0xB8U; put_u32(elf_buffer, code, 3U); code += 4U;
            elf_buffer[code++] = 0x31U; elf_buffer[code++] = 0xFFU;
            elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0xBEU; put_u64(elf_buffer, code, USER_IMAGE_DATA_BASE + UINT64_C(8)); code += 8U;
            elf_buffer[code++] = 0xBAU; put_u32(elf_buffer, code, 1U); code += 4U;
            elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0x05U;
            elf_buffer[code++] = 0x85U; elf_buffer[code++] = 0xC0U;
            elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0x84U;
            relative = (int64_t)input_start - (int64_t)(code + 4U);
            put_u32(elf_buffer, code, (uint32_t)relative); code += 4U;
            elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0xBEU; put_u64(elf_buffer, code, USER_IMAGE_DATA_BASE + UINT64_C(8)); code += 8U;
            elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0xB6U; elf_buffer[code++] = 0x1EU;
            elf_buffer[code++] = 0x83U; elf_buffer[code++] = 0xFBU; elf_buffer[code++] = '\n';
            elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0x84U;
            relative = (int64_t)input_start - (int64_t)(code + 4U);
            put_u32(elf_buffer, code, (uint32_t)relative); code += 4U;
            elf_buffer[code++] = 0x83U; elf_buffer[code++] = 0xFBU; elf_buffer[code++] = '\r';
            elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0x84U;
            relative = (int64_t)input_start - (int64_t)(code + 4U);
            put_u32(elf_buffer, code, (uint32_t)relative); code += 4U;
        } else if (instructions[index].kind == NATIVE_INSTRUCTION_TIME) {
            const uint64_t field_offsets[3] = { 4U, 5U, 6U };
            const uint64_t text_offsets[3] = { 8U, 11U, 14U };
            const uint8_t separators[2] = { ':', ':' };
            const uint64_t separator_offsets[2] = { 10U, 13U };

            elf_buffer[code++] = 0xB8U; put_u32(elf_buffer, code, 16U); code += 4U;
            elf_buffer[code++] = 0x31U; elf_buffer[code++] = 0xFFU;
            elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0xBEU; put_u64(elf_buffer, code, USER_IMAGE_DATA_BASE + UINT64_C(8)); code += 8U;
            elf_buffer[code++] = 0xBAU; put_u32(elf_buffer, code, 8U); code += 4U;
            elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0x05U;
            elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0xBEU; put_u64(elf_buffer, code, USER_IMAGE_DATA_BASE + UINT64_C(8)); code += 8U;
            for (uint64_t field = 0U; field < 3U; field++) {
                elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0xB6U; elf_buffer[code++] = 0x46U;
                elf_buffer[code++] = (uint8_t)field_offsets[field];
                elf_buffer[code++] = 0xB9U; put_u32(elf_buffer, code, 10U); code += 4U;
                elf_buffer[code++] = 0x31U; elf_buffer[code++] = 0xD2U;
                elf_buffer[code++] = 0xF7U; elf_buffer[code++] = 0xF1U;
                elf_buffer[code++] = 0x80U; elf_buffer[code++] = 0xC0U; elf_buffer[code++] = 0x30U;
                elf_buffer[code++] = 0x88U; elf_buffer[code++] = 0x46U; elf_buffer[code++] = (uint8_t)text_offsets[field];
                elf_buffer[code++] = 0x80U; elf_buffer[code++] = 0xC2U; elf_buffer[code++] = 0x30U;
                elf_buffer[code++] = 0x88U; elf_buffer[code++] = 0x56U; elf_buffer[code++] = (uint8_t)(text_offsets[field] + 1U);
            }
            for (uint64_t separator = 0U; separator < 2U; separator++) {
                elf_buffer[code++] = 0xC6U; elf_buffer[code++] = 0x46U;
                elf_buffer[code++] = (uint8_t)separator_offsets[separator]; elf_buffer[code++] = separators[separator];
            }
            elf_buffer[code++] = 0xC6U; elf_buffer[code++] = 0x46U; elf_buffer[code++] = 16U; elf_buffer[code++] = '\n';
            elf_buffer[code++] = 0xB8U; put_u32(elf_buffer, code, 1U); code += 4U;
            elf_buffer[code++] = 0xBFU; put_u32(elf_buffer, code, 1U); code += 4U;
            elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0x8DU; elf_buffer[code++] = 0x76U; elf_buffer[code++] = 8U;
            elf_buffer[code++] = 0xBAU; put_u32(elf_buffer, code, 9U); code += 4U;
            elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0x05U;
        } else {
            elf_buffer[code++] = 0xB8U; put_u32(elf_buffer, code, 2U); code += 4U;
            elf_buffer[code++] = 0xBFU; put_u32(elf_buffer, code, (uint32_t)instructions[index].exit_status); code += 4U;
            elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0x31U; elf_buffer[code++] = 0xF6U;
            elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0x31U; elf_buffer[code++] = 0xD2U;
            elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0x05U;
            elf_buffer[code++] = 0xEBU; elf_buffer[code++] = 0xFEU;
        }
        if (code != ELF_PAYLOAD_OFFSET + instruction_offsets[index + 1U]) { return 0; }
    }
    for (uint64_t index = 0U; index < literal_length; index++) {
        elf_buffer[ELF_PAYLOAD_OFFSET + code_length + index] = literal_buffer[index];
    }
    *image_length = ELF_PAYLOAD_OFFSET + payload_length;
    return 1;
}

static int store_elf(const char *path, uint64_t image_length) {
    struct myos_vfs_path_request path_request = { { 0 } };
    struct myos_vfs_write_request request = { 0U, 0U, { 0 }, { 0 } };
    uint64_t path_length = 0U;
    uint64_t offset = 0U;

    while (path[path_length] != '\0' && path_length + 1U < sizeof(path_request.path)) {
        request.path[path_length] = path[path_length];
        path_request.path[path_length] = path[path_length];
        path_length++;
    }
    if (path_length == 0U || path[path_length] != '\0') { return 0; }
    request.path[path_length] = '\0';
    path_request.path[path_length] = '\0';
    (void)system_call(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)&path_request, sizeof(path_request));
    if (system_call(MYOS_SYS_VFS_CREATE_FILE, 0U, (uint64_t)(uintptr_t)&path_request, sizeof(path_request)) == UINT64_MAX) {
        return 0;
    }
    while (offset < image_length) {
        const uint64_t chunk = image_length - offset < MYOS_VFS_READ_CHUNK ? image_length - offset : MYOS_VFS_READ_CHUNK;

        request.offset = offset;
        request.length = chunk;
        for (uint64_t index = 0U; index < chunk; index++) { request.data[index] = elf_buffer[offset + index]; }
        if (system_call(MYOS_SYS_VFS_WRITE, 0U, (uint64_t)(uintptr_t)&request, sizeof(request)) != chunk) {
            return 0;
        }
        offset += chunk;
    }
    return 1;
}

void _start(uint64_t argc, const char *arguments) {
    char source_path[MYOS_VFS_PATH_MAX];
    char output_path[MYOS_VFS_PATH_MAX];
    uint64_t argument_position = 0U;
    uint64_t source_length;
    uint64_t instruction_count;
    uint64_t label_count;
    uint64_t literal_length;
    uint64_t image_length;

    if (argc != 1U || copy_path(source_path, sizeof(source_path), arguments, &argument_position) == 0) {
        write_text("Usage: run asm <source.mya> <output.elf>\n");
        write_text("Source: set <0..255>; not; add/sub/mul/and/or/xor <0..255>; shl/shr <1..7>; div <1..255>; store/load/cmp <0..7>; input; time; args; label name:; write \"text\"; jump[_if_zero|_if_nonzero] name; jump_if <0..255> name; exit <0..255>\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    while (arguments[argument_position] == ' ') { argument_position++; }
    if (copy_path(output_path, sizeof(output_path), arguments, &argument_position) == 0 || arguments[argument_position] != '\0'
        || load_source(source_path, &source_length) == 0) {
        write_text("asm: unable to read absolute source path\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    if (parse_source(source_length, &instruction_count, &label_count, &literal_length) == 0
        || resolve_jumps(instruction_count, label_count) == 0) {
        write_text("asm: syntax error; set/load/input must precede not/add/sub/mul/div/and/or/xor/shl/shr/cmp and conditional jumps, add/sub/mul/and/or/xor are byte values 0..255, shl/shr are 1..7, div is 1..255, store/load/cmp slots are 0..7, labels need ':' and jumps must target a later label\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    if (build_elf(instruction_count, literal_length, &image_length) == 0
        || store_elf(output_path, image_length) == 0) {
        write_text("asm: unable to create ELF output\n");
        (void)system_call(MYOS_SYS_EXIT, 1U, 0U, 0U);
    }
    write_text("asm: built "); write_number(image_length); write_text(" bytes\n");
    (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    for (;;) { __asm__ volatile ("pause"); }
}
