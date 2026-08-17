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

#define MYOS_TASK_SLOT_COUNT UINT64_C(8)
#define MYOS_TASK_NAME_MAX UINT64_C(16)

#define MYOS_TASK_STATE_UNUSED UINT64_C(0)
#define MYOS_TASK_STATE_READY UINT64_C(1)
#define MYOS_TASK_STATE_RUNNING UINT64_C(2)
#define MYOS_TASK_STATE_SLEEPING UINT64_C(3)
#define MYOS_TASK_STATE_ZOMBIE UINT64_C(4)

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

void syscall_init(void);
uint64_t syscall_count(void);
uint64_t syscall_write_count(void);

#endif
