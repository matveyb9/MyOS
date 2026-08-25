#include <myos.h>

static const uint8_t payload[] = "sdk-write: persistent VFS example\n";

static int copy_token(const char *arguments, uint64_t *offset, char *destination, uint64_t capacity) {
    uint64_t length = 0U;

    while (arguments[*offset] == ' ') {
        (*offset)++;
    }
    while (arguments[*offset] != '\0' && arguments[*offset] != ' ' && length + 1U < capacity) {
        destination[length++] = arguments[*offset];
        (*offset)++;
    }
    destination[length] = '\0';
    return length != 0U && (arguments[*offset] == '\0' || arguments[*offset] == ' ');
}

static void copy_path(char *destination, const char *source) {
    uint64_t index = 0U;

    while (index + 1U < MYOS_VFS_PATH_MAX && source[index] != '\0') {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

int myos_main(uint64_t argc, const char *arguments) {
    struct myos_vfs_path_request path_request = { { 0 } };
    struct myos_vfs_write_request write_request = { 0U, 0U, { 0 }, { 0 } };
    uint64_t argument_offset = 0U;
    const uint64_t payload_length = sizeof(payload) - 1U;

    if (argc != 1U
        || copy_token(arguments, &argument_offset, path_request.path, sizeof(path_request.path)) == 0
        || arguments[argument_offset] != '\0'
        || path_request.path[0] != '/') {
        myos_write_text("Usage: run sdk-write <new-absolute-target>\n");
        return 2;
    }
    if (myos_vfs_create_file(&path_request) == MYOS_SYSCALL_ERROR) {
        myos_write_text("sdk-write: target must not exist and parent directory must exist\n");
        return 1;
    }
    copy_path(write_request.path, path_request.path);
    write_request.offset = 0U;
    write_request.length = payload_length;
    for (uint64_t index = 0U; index < payload_length; index++) {
        write_request.data[index] = payload[index];
    }
    if (myos_vfs_write(&write_request) != payload_length) {
        (void)myos_vfs_remove(&path_request);
        myos_write_text("sdk-write: write failed; partial target removed\n");
        return 1;
    }
    myos_write_text("sdk-write: wrote fixed payload to ");
    myos_write_text(path_request.path);
    myos_write_text("\n");
    return 0;
}
