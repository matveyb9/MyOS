#include <stdint.h>

#include <syscall.h>

#define INVALID_USER_POINTER UINT64_C(0x0000000000002000)

static uint64_t system_call(uint64_t number, uint64_t descriptor, uint64_t buffer, uint64_t length) {
    uint64_t result;

    __asm__ volatile ("syscall"
                      : "=a"(result)
                      : "a"(number), "D"(descriptor), "S"(buffer), "d"(length)
                      : "rcx", "r11", "memory");
    return result;
}

static int rejected(uint64_t result) {
    return result == UINT64_MAX;
}

void _start(void) {
    static const char passed[] = "[safety] invalid user pointers rejected\n";
    static const char failed[] = "[safety] invalid user pointer accepted\n";
    int passed_all = 1;

    passed_all &= rejected(system_call(MYOS_SYS_WRITE, 1U, INVALID_USER_POINTER, 1U));
    passed_all &= rejected(system_call(MYOS_SYS_READ, 0U, INVALID_USER_POINTER, 1U));
    passed_all &= rejected(system_call(MYOS_SYS_TASK_INFO, 0U, INVALID_USER_POINTER,
                                      sizeof(struct myos_task_info)));
    passed_all &= rejected(system_call(MYOS_SYS_RTC_TIME, 0U, INVALID_USER_POINTER,
                                      sizeof(struct myos_rtc_time)));
    passed_all &= rejected(system_call(MYOS_SYS_VFS_ENTRY, 0U, INVALID_USER_POINTER,
                                      sizeof(struct myos_vfs_entry)));
    passed_all &= rejected(system_call(MYOS_SYS_SPAWN, 0U, INVALID_USER_POINTER,
                                      sizeof(struct myos_spawn_request)));
    passed_all &= rejected(system_call(MYOS_SYS_TMPFS_CREATE, 0U, INVALID_USER_POINTER,
                                      sizeof(struct myos_tmpfs_path_request)));
    passed_all &= rejected(system_call(MYOS_SYS_TMPFS_WRITE, 0U, INVALID_USER_POINTER,
                                      sizeof(struct myos_tmpfs_write_request)));
    passed_all &= rejected(system_call(MYOS_SYS_TMPFS_REMOVE, 0U, INVALID_USER_POINTER,
                                      sizeof(struct myos_tmpfs_path_request)));
    if (passed_all != 0) {
        (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)passed, sizeof(passed) - 1U);
        (void)system_call(MYOS_SYS_EXIT, 0U, 0U, 0U);
    }
    (void)system_call(MYOS_SYS_WRITE, 1U, (uint64_t)(uintptr_t)failed, sizeof(failed) - 1U);
    (void)system_call(MYOS_SYS_EXIT, 1U, 0U, 0U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
