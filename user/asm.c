#include <stdint.h>

#include <syscall.h>

#define ASM_SOURCE_CAPACITY UINT64_C(2048)
#define ASM_LITERAL_CAPACITY UINT64_C(2048)
#define ASM_ELF_CAPACITY UINT64_C(8192)
#define ASM_MAX_WRITES UINT64_C(32)
#define ELF_HEADER_SIZE UINT64_C(64)
#define ELF_PROGRAM_HEADER_SIZE UINT64_C(56)
#define ELF_PAYLOAD_OFFSET UINT64_C(4096)
#define ELF_MACHINE_X86_64 UINT16_C(62)
#define ELF_TYPE_EXEC UINT16_C(2)
#define ELF_PROGRAM_LOAD UINT32_C(1)
#define ELF_FLAG_RX UINT32_C(5)
#define USER_IMAGE_BASE UINT64_C(0x400000)

struct native_write {
    uint64_t data_offset;
    uint64_t length;
};

static uint8_t source_buffer[ASM_SOURCE_CAPACITY];
static uint8_t literal_buffer[ASM_LITERAL_CAPACITY];
static uint8_t elf_buffer[ASM_ELF_CAPACITY];
static struct native_write writes[ASM_MAX_WRITES];

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

static void skip_space(const uint8_t *text, uint64_t length, uint64_t *position) {
    while (*position < length && (text[*position] == ' ' || text[*position] == '\t'
                                  || text[*position] == '\r' || text[*position] == '\n'
                                  || text[*position] == ';')) {
        (*position)++;
    }
}

static int word_is(const uint8_t *text, uint64_t start, uint64_t length, const char *word) {
    uint64_t index = 0U;

    while (word[index] != '\0' && index < length) {
        if (text[start + index] != (uint8_t)word[index]) { return 0; }
        index++;
    }
    return index == length && word[index] == '\0';
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

static int parse_source(uint64_t source_length, uint64_t *write_count, uint64_t *literal_length,
                        uint64_t *exit_status) {
    uint64_t position = 0U;
    int saw_exit = 0;

    *write_count = 0U;
    *literal_length = 0U;
    *exit_status = 0U;
    while (position < source_length) {
        uint64_t start;
        uint64_t word_length;

        skip_space(source_buffer, source_length, &position);
        if (position >= source_length) { break; }
        if (source_buffer[position] == '#') {
            while (position < source_length && source_buffer[position] != '\n') { position++; }
            continue;
        }
        if (saw_exit != 0) { return 0; }
        start = position;
        while (position < source_length && ((source_buffer[position] >= 'a' && source_buffer[position] <= 'z')
                                            || (source_buffer[position] >= 'A' && source_buffer[position] <= 'Z'))) {
            position++;
        }
        word_length = position - start;
        if (word_is(source_buffer, start, word_length, "write") != 0) {
            uint64_t data_start;

            if (*write_count >= ASM_MAX_WRITES) { return 0; }
            skip_space(source_buffer, source_length, &position);
            data_start = *literal_length;
            if (parse_string(source_length, &position, literal_length) == 0) { return 0; }
            writes[*write_count].data_offset = data_start;
            writes[*write_count].length = *literal_length - data_start;
            (*write_count)++;
        } else if (word_is(source_buffer, start, word_length, "exit") != 0) {
            skip_space(source_buffer, source_length, &position);
            if (parse_decimal(source_buffer, source_length, &position, exit_status) == 0 || *exit_status > UINT64_C(255)) {
                return 0;
            }
            saw_exit = 1;
        } else {
            return 0;
        }
        while (position < source_length && (source_buffer[position] == ' ' || source_buffer[position] == '\t')) {
            position++;
        }
        if (position < source_length) {
            if (source_buffer[position] == '#') {
                while (position < source_length && source_buffer[position] != '\n') { position++; }
            } else if (source_buffer[position] == ';' || source_buffer[position] == '\n'
                       || source_buffer[position] == '\r') {
                position++;
            } else {
                return 0;
            }
        }
    }
    return saw_exit != 0;
}

static int build_elf(uint64_t write_count, uint64_t literal_length, uint64_t exit_status, uint64_t *image_length) {
    uint64_t code_length = write_count * UINT64_C(32) + UINT64_C(20);
    uint64_t payload_length = code_length + literal_length;
    uint64_t code = ELF_PAYLOAD_OFFSET;

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
    put_u16(elf_buffer, 56U, 1U);
    put_u32(elf_buffer, ELF_HEADER_SIZE, ELF_PROGRAM_LOAD);
    put_u32(elf_buffer, ELF_HEADER_SIZE + 4U, ELF_FLAG_RX);
    put_u64(elf_buffer, ELF_HEADER_SIZE + 8U, ELF_PAYLOAD_OFFSET);
    put_u64(elf_buffer, ELF_HEADER_SIZE + 16U, USER_IMAGE_BASE);
    put_u64(elf_buffer, ELF_HEADER_SIZE + 24U, USER_IMAGE_BASE);
    put_u64(elf_buffer, ELF_HEADER_SIZE + 32U, payload_length);
    put_u64(elf_buffer, ELF_HEADER_SIZE + 40U, payload_length);
    put_u64(elf_buffer, ELF_HEADER_SIZE + 48U, UINT64_C(4096));

    for (uint64_t index = 0U; index < write_count; index++) {
        const uint64_t address = USER_IMAGE_BASE + code_length + writes[index].data_offset;

        elf_buffer[code++] = 0xB8U; put_u32(elf_buffer, code, 1U); code += 4U;
        elf_buffer[code++] = 0xBFU; put_u32(elf_buffer, code, 1U); code += 4U;
        elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0xBEU; put_u64(elf_buffer, code, address); code += 8U;
        elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0xBAU; put_u64(elf_buffer, code, writes[index].length); code += 8U;
        elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0x05U;
    }
    elf_buffer[code++] = 0xB8U; put_u32(elf_buffer, code, 2U); code += 4U;
    elf_buffer[code++] = 0xBFU; put_u32(elf_buffer, code, (uint32_t)exit_status); code += 4U;
    elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0x31U; elf_buffer[code++] = 0xF6U;
    elf_buffer[code++] = 0x48U; elf_buffer[code++] = 0x31U; elf_buffer[code++] = 0xD2U;
    elf_buffer[code++] = 0x0FU; elf_buffer[code++] = 0x05U;
    elf_buffer[code++] = 0xEBU; elf_buffer[code++] = 0xFEU;
    for (uint64_t index = 0U; index < literal_length; index++) { elf_buffer[ELF_PAYLOAD_OFFSET + code_length + index] = literal_buffer[index]; }
    *image_length = ELF_PAYLOAD_OFFSET + payload_length;
    return code == ELF_PAYLOAD_OFFSET + code_length;
}

static int store_elf(const char *path, uint64_t image_length) {
    struct myos_vfs_path_request path_request = { { 0 } };
    struct myos_vfs_write_request request = { 0U, 0U, { 0 }, { 0 } };
    uint64_t path_length = 0U;
    uint64_t offset = 0U;

    while (path[path_length] != '\0' && path_length + 1U < sizeof(path_request.path)) {
        path_request.path[path_length] = path[path_length];
        request.path[path_length] = path[path_length];
        path_length++;
    }
    if (path_length == 0U || path[path_length] != '\0') { return 0; }
    path_request.path[path_length] = '\0';
    request.path[path_length] = '\0';
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
    uint64_t write_count;
    uint64_t literal_length;
    uint64_t exit_status;
    uint64_t image_length;

    if (argc != 1U || copy_path(source_path, sizeof(source_path), arguments, &argument_position) == 0) {
        write_text("Usage: run asm <source.mya> <output.elf>\n");
        write_text("Source: write \"text\"; exit <0..255>\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    while (arguments[argument_position] == ' ') { argument_position++; }
    if (copy_path(output_path, sizeof(output_path), arguments, &argument_position) == 0 || arguments[argument_position] != '\0'
        || load_source(source_path, &source_length) == 0) {
        write_text("asm: unable to read absolute source path\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    if (parse_source(source_length, &write_count, &literal_length, &exit_status) == 0) {
        write_text("asm: syntax error; use write \"text\"; exit <0..255>\n");
        (void)system_call(MYOS_SYS_EXIT, 2U, 0U, 0U);
    }
    if (build_elf(write_count, literal_length, exit_status, &image_length) == 0
        || store_elf(output_path, image_length) == 0) {
        write_text("asm: unable to create ELF output\n");
        (void)system_call(MYOS_SYS_EXIT, 1U, 0U, 0U);
    }
    write_text("asm: built "); write_number(image_length); write_text(" bytes\n");
    (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    for (;;) { __asm__ volatile ("pause"); }
}
