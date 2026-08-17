#ifndef MYOS_SCHEDULER_H
#define MYOS_SCHEDULER_H

#include <stdint.h>

#include <paging.h>

#define SCHEDULER_MAX_TASKS 8U

enum task_state {
    TASK_STATE_UNUSED = 0,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_TERMINATED
};

enum task_kind {
    TASK_KIND_KERNEL = 0,
    TASK_KIND_USER
};

typedef void (*kernel_thread_entry_t)(void *argument);

void scheduler_init(void);
int scheduler_create_kernel_thread(const char *name, kernel_thread_entry_t entry, void *argument);
int scheduler_create_user_task(const char *name, const struct paging_space *address_space,
                               uint64_t entry, uint64_t user_stack_top);
int scheduler_activate_current_task(void);
uint64_t *scheduler_on_timer(uint64_t *interrupted_context);
uint64_t scheduler_current_task_id(void);
uint64_t scheduler_switch_count(void);
uint64_t scheduler_runnable_task_count(void);
enum task_state scheduler_task_state(uint64_t task_id);
enum task_kind scheduler_task_kind(uint64_t task_id);
const char *scheduler_task_name(uint64_t task_id);
uint64_t scheduler_task_run_count(uint64_t task_id);
uint64_t scheduler_task_address_space(uint64_t task_id);

#endif
