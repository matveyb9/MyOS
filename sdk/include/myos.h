#ifndef MYOS_SDK_MYOS_H
#define MYOS_SDK_MYOS_H

#include <stdint.h>

#define MYOS_ABI_VERSION UINT64_C(0x00010000)

#define MYOS_SYS_WRITE UINT64_C(1)
#define MYOS_SYS_EXIT UINT64_C(2)
#define MYOS_SYS_READ UINT64_C(3)
#define MYOS_SYS_TICKS UINT64_C(4)
#define MYOS_SYS_GETPID UINT64_C(8)
#define MYOS_SYS_SLEEP UINT64_C(9)
#define MYOS_SYS_VFS_READ UINT64_C(12)
#define MYOS_SYS_VFS_CREATE_FILE UINT64_C(30)
#define MYOS_SYS_VFS_WRITE UINT64_C(32)
#define MYOS_SYS_VFS_REMOVE UINT64_C(33)

#define MYOS_STDOUT UINT64_C(1)
#define MYOS_SYSCALL_ERROR UINT64_MAX
#define MYOS_VFS_PATH_MAX UINT64_C(112)
#define MYOS_VFS_READ_CHUNK UINT64_C(256)

struct myos_vfs_read_request {
    uint64_t offset;
    char path[MYOS_VFS_PATH_MAX];
    uint8_t data[MYOS_VFS_READ_CHUNK];
};

struct myos_vfs_path_request {
    char path[MYOS_VFS_PATH_MAX];
};

struct myos_vfs_write_request {
    uint64_t offset;
    uint64_t length;
    char path[MYOS_VFS_PATH_MAX];
    uint8_t data[MYOS_VFS_READ_CHUNK];
};

static inline uint64_t myos_syscall(uint64_t number, uint64_t descriptor, uint64_t buffer,
                                    uint64_t length) {
    uint64_t result;

    __asm__ volatile ("syscall"
                      : "=a"(result)
                      : "a"(number), "D"(descriptor), "S"(buffer), "d"(length)
                      : "rcx", "r11", "memory");
    return result;
}

static inline uint64_t myos_write(uint64_t descriptor, const void *data, uint64_t length) {
    return myos_syscall(MYOS_SYS_WRITE, descriptor, (uint64_t)(uintptr_t)data, length);
}

static inline uint64_t myos_text_length(const char *text) {
    uint64_t length = 0U;

    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static inline void myos_write_text(const char *text) {
    uint64_t remaining = myos_text_length(text);

    while (remaining != 0U) {
        const uint64_t chunk = remaining > UINT64_C(256) ? UINT64_C(256) : remaining;

        (void)myos_write(MYOS_STDOUT, text, chunk);
        text += chunk;
        remaining -= chunk;
    }
}

static inline uint64_t myos_vfs_read(struct myos_vfs_read_request *request) {
    return myos_syscall(MYOS_SYS_VFS_READ, 0U, (uint64_t)(uintptr_t)request, sizeof(*request));
}

static inline uint64_t myos_vfs_create_file(const struct myos_vfs_path_request *request) {
    return myos_syscall(MYOS_SYS_VFS_CREATE_FILE, 0U, (uint64_t)(uintptr_t)request, sizeof(*request));
}

static inline uint64_t myos_vfs_write(const struct myos_vfs_write_request *request) {
    return myos_syscall(MYOS_SYS_VFS_WRITE, 0U, (uint64_t)(uintptr_t)request, sizeof(*request));
}

static inline uint64_t myos_vfs_remove(const struct myos_vfs_path_request *request) {
    return myos_syscall(MYOS_SYS_VFS_REMOVE, 0U, (uint64_t)(uintptr_t)request, sizeof(*request));
}

static inline uint64_t myos_getpid(void) {
    return myos_syscall(MYOS_SYS_GETPID, 0U, 0U, 0U);
}

static inline uint64_t myos_ticks(void) {
    return myos_syscall(MYOS_SYS_TICKS, 0U, 0U, 0U);
}

static inline void myos_exit(uint64_t status) __attribute__((noreturn));

static inline void myos_exit(uint64_t status) {
    (void)myos_syscall(MYOS_SYS_EXIT, status, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}

int myos_main(uint64_t argc, const char *arguments);

#endif
