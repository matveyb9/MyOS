#ifndef MYOS_SYSCALL_H
#define MYOS_SYSCALL_H

#include <stdint.h>

#define MYOS_SYS_WRITE UINT64_C(1)
#define MYOS_SYS_EXIT UINT64_C(2)
#define MYOS_SYS_READ UINT64_C(3)
#define MYOS_SYS_TICKS UINT64_C(4)
#define MYOS_SYS_FREE_FRAMES UINT64_C(5)
#define MYOS_SYS_SPAWN UINT64_C(6)
#define MYOS_SYS_WAIT UINT64_C(7)
#define MYOS_SYS_GETPID UINT64_C(8)
#define MYOS_SYS_SLEEP UINT64_C(9)
#define MYOS_SYS_TASK_INFO UINT64_C(10)
#define MYOS_SYS_VFS_ENTRY UINT64_C(11)
#define MYOS_SYS_VFS_READ UINT64_C(12)
#define MYOS_SYS_REBOOT UINT64_C(13)
#define MYOS_SYS_POWEROFF UINT64_C(14)

#define MYOS_TASK_SLOT_COUNT UINT64_C(8)
#define MYOS_TASK_NAME_MAX UINT64_C(16)
#define MYOS_VFS_NAME_MAX UINT64_C(64)
#define MYOS_VFS_READ_CHUNK UINT64_C(128)

#define MYOS_TASK_STATE_UNUSED UINT64_C(0)
#define MYOS_TASK_STATE_READY UINT64_C(1)
#define MYOS_TASK_STATE_RUNNING UINT64_C(2)
#define MYOS_TASK_STATE_SLEEPING UINT64_C(3)
#define MYOS_TASK_STATE_ZOMBIE UINT64_C(4)
#define MYOS_TASK_STATE_WAITING UINT64_C(5)
#define MYOS_TASK_STATE_INPUT UINT64_C(6)

#define MYOS_TASK_KIND_KERNEL UINT64_C(0)
#define MYOS_TASK_KIND_USER UINT64_C(1)

struct myos_task_info {
    uint64_t id;
    uint64_t state;
    uint64_t kind;
    uint64_t run_count;
    uint64_t exit_status;
    char name[MYOS_TASK_NAME_MAX];
};

struct myos_vfs_entry {
    uint64_t size;
    char name[MYOS_VFS_NAME_MAX];
};

struct myos_vfs_read_request {
    uint64_t offset;
    char path[MYOS_VFS_NAME_MAX];
    uint8_t data[MYOS_VFS_READ_CHUNK];
};

void syscall_init(void);
uint64_t syscall_count(void);
uint64_t syscall_write_count(void);

#endif
