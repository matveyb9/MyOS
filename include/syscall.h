#ifndef MYOS_SYSCALL_H
#define MYOS_SYSCALL_H

#include <stdint.h>

#define MYOS_SYS_WRITE UINT64_C(1)
#define MYOS_SYS_EXIT UINT64_C(2)
#define MYOS_SYS_READ UINT64_C(3)
#define MYOS_SYS_TICKS UINT64_C(4)
#define MYOS_SYS_FREE_FRAMES UINT64_C(5)

void syscall_init(void);
uint64_t syscall_count(void);
uint64_t syscall_write_count(void);

#endif
