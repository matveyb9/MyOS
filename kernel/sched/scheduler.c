#include <stdint.h>

#include <arch.h>
#include <gdt.h>
#include <paging.h>
#include <scheduler.h>

#define SCHEDULER_STACK_SIZE (64U * 1024U)
#define SCHEDULER_CONTEXT_WORDS 21U
#define SCHEDULER_INITIAL_RFLAGS UINT64_C(0x202)

struct task {
    uint64_t id;
    enum task_state state;
    enum task_kind kind;
    const char *name;
    kernel_thread_entry_t kernel_entry;
    void *argument;
    uint64_t user_entry;
    uint64_t user_stack_top;
    struct paging_space address_space;
    uint64_t *saved_context;
    uint64_t run_count;
    uint8_t stack[SCHEDULER_STACK_SIZE] __attribute__((aligned(16)));
};

extern uint8_t __kernel_stack_top[];

static struct task tasks[SCHEDULER_MAX_TASKS];
static uint64_t current_task_index;
static uint64_t context_switches;
static int scheduler_ready;

static uint64_t task_kernel_stack_top(const struct task *task) {
    if (task->id == 0U) {
        return (uint64_t)(uintptr_t)__kernel_stack_top;
    }
    return (uint64_t)(uintptr_t)&task->stack[SCHEDULER_STACK_SIZE];
}

static void task_trampoline(void) {
    struct task *task = &tasks[current_task_index];

    task->kernel_entry(task->argument);
    task->state = TASK_STATE_TERMINATED;
    for (;;) {
        arch_wait_for_interrupt();
    }
}

static uint64_t *empty_context(struct task *task) {
    uint64_t stack_top = task_kernel_stack_top(task);
    uint64_t initial_stack;
    uint64_t *context;

    stack_top &= ~(uint64_t)0x0fU;
    initial_stack = stack_top - sizeof(uint64_t);
    context = (uint64_t *)(uintptr_t)(initial_stack - SCHEDULER_CONTEXT_WORDS * sizeof(uint64_t));
    for (uint64_t index = 0U; index < SCHEDULER_CONTEXT_WORDS; index++) {
        context[index] = 0U;
    }
    context[15] = 0U;
    return context;
}

static uint64_t *initial_kernel_context(struct task *task) {
    uint64_t *context = empty_context(task);

    context[16] = (uint64_t)(uintptr_t)task_trampoline;
    context[17] = GDT_KERNEL_CODE_SELECTOR;
    context[18] = SCHEDULER_INITIAL_RFLAGS;
    context[19] = task_kernel_stack_top(task) - sizeof(uint64_t);
    context[20] = GDT_KERNEL_DATA_SELECTOR;
    return context;
}

static uint64_t *initial_user_context(struct task *task) {
    uint64_t *context = empty_context(task);

    context[16] = task->user_entry;
    context[17] = (uint64_t)(GDT_USER_CODE_SELECTOR | UINT16_C(3));
    context[18] = SCHEDULER_INITIAL_RFLAGS;
    context[19] = task->user_stack_top;
    context[20] = (uint64_t)(GDT_USER_DATA_SELECTOR | UINT16_C(3));
    return context;
}

static int next_ready_task(uint64_t *next_index) {
    for (uint64_t offset = 1U; offset <= SCHEDULER_MAX_TASKS; offset++) {
        const uint64_t candidate = (current_task_index + offset) % SCHEDULER_MAX_TASKS;

        if (tasks[candidate].state == TASK_STATE_READY) {
            *next_index = candidate;
            return 1;
        }
    }
    return 0;
}

static void activate_task_context(const struct task *task) {
    if (task->kind == TASK_KIND_USER) {
        (void)paging_space_activate(&task->address_space);
    } else {
        (void)paging_activate_kernel_space();
    }
    gdt_set_kernel_stack(task_kernel_stack_top(task));
}

static void clear_task(struct task *task, uint64_t id) {
    task->id = id;
    task->state = TASK_STATE_UNUSED;
    task->kind = TASK_KIND_KERNEL;
    task->name = "unused";
    task->kernel_entry = (kernel_thread_entry_t)0;
    task->argument = (void *)0;
    task->user_entry = 0U;
    task->user_stack_top = 0U;
    task->address_space.root_physical = 0U;
    task->address_space.mapping_count = 0U;
    task->saved_context = (uint64_t *)0;
    task->run_count = 0U;
}

void scheduler_init(void) {
    for (uint64_t index = 0U; index < SCHEDULER_MAX_TASKS; index++) {
        clear_task(&tasks[index], index);
    }

    tasks[0].state = TASK_STATE_RUNNING;
    tasks[0].kind = TASK_KIND_KERNEL;
    tasks[0].name = "kernel";
    tasks[0].run_count = 1U;
    current_task_index = 0U;
    context_switches = 0U;
    scheduler_ready = 1;
    activate_task_context(&tasks[0]);
}

int scheduler_create_kernel_thread(const char *name, kernel_thread_entry_t entry, void *argument) {
    if (scheduler_ready == 0 || entry == (kernel_thread_entry_t)0 || name == (const char *)0) {
        return -1;
    }

    for (uint64_t index = 1U; index < SCHEDULER_MAX_TASKS; index++) {
        struct task *task = &tasks[index];

        if (task->state != TASK_STATE_UNUSED) {
            continue;
        }
        task->kind = TASK_KIND_KERNEL;
        task->name = name;
        task->kernel_entry = entry;
        task->argument = argument;
        task->saved_context = initial_kernel_context(task);
        task->run_count = 0U;
        task->state = TASK_STATE_READY;
        return (int)index;
    }
    return -1;
}

int scheduler_create_user_task(const char *name, const struct paging_space *address_space,
                               uint64_t entry, uint64_t user_stack_top) {
    if (scheduler_ready == 0 || name == (const char *)0 || address_space == (const struct paging_space *)0
        || address_space->root_physical == 0U || entry < PAGING_USER_SPACE_START
        || entry > PAGING_USER_SPACE_END || user_stack_top < PAGING_USER_SPACE_START
        || user_stack_top > PAGING_USER_SPACE_END) {
        return -1;
    }

    for (uint64_t index = 1U; index < SCHEDULER_MAX_TASKS; index++) {
        struct task *task = &tasks[index];

        if (task->state != TASK_STATE_UNUSED) {
            continue;
        }
        task->kind = TASK_KIND_USER;
        task->name = name;
        task->user_entry = entry;
        task->user_stack_top = user_stack_top;
        task->address_space = *address_space;
        task->saved_context = initial_user_context(task);
        task->run_count = 0U;
        task->state = TASK_STATE_READY;
        return (int)index;
    }
    return -1;
}

uint64_t *scheduler_on_timer(uint64_t *interrupted_context) {
    struct task *current;
    uint64_t next_index;

    if (scheduler_ready == 0 || interrupted_context == (uint64_t *)0) {
        return interrupted_context;
    }

    current = &tasks[current_task_index];
    current->saved_context = interrupted_context;
    if (current->state == TASK_STATE_RUNNING) {
        current->state = TASK_STATE_READY;
    }

    if (next_ready_task(&next_index) == 0) {
        if (current->state == TASK_STATE_READY) {
            current->state = TASK_STATE_RUNNING;
        }
        return interrupted_context;
    }

    current_task_index = next_index;
    tasks[current_task_index].state = TASK_STATE_RUNNING;
    tasks[current_task_index].run_count++;
    context_switches++;
    activate_task_context(&tasks[current_task_index]);
    return tasks[current_task_index].saved_context;
}

uint64_t scheduler_current_task_id(void) {
    return current_task_index;
}

uint64_t scheduler_switch_count(void) {
    return context_switches;
}

uint64_t scheduler_runnable_task_count(void) {
    uint64_t count = 0U;

    for (uint64_t index = 0U; index < SCHEDULER_MAX_TASKS; index++) {
        if (tasks[index].state == TASK_STATE_READY || tasks[index].state == TASK_STATE_RUNNING) {
            count++;
        }
    }
    return count;
}

enum task_state scheduler_task_state(uint64_t task_id) {
    return task_id < SCHEDULER_MAX_TASKS ? tasks[task_id].state : TASK_STATE_UNUSED;
}

enum task_kind scheduler_task_kind(uint64_t task_id) {
    return task_id < SCHEDULER_MAX_TASKS ? tasks[task_id].kind : TASK_KIND_KERNEL;
}

const char *scheduler_task_name(uint64_t task_id) {
    return task_id < SCHEDULER_MAX_TASKS ? tasks[task_id].name : "invalid";
}

uint64_t scheduler_task_run_count(uint64_t task_id) {
    return task_id < SCHEDULER_MAX_TASKS ? tasks[task_id].run_count : 0U;
}

uint64_t scheduler_task_address_space(uint64_t task_id) {
    if (task_id >= SCHEDULER_MAX_TASKS || tasks[task_id].kind != TASK_KIND_USER) {
        return paging_kernel_root_physical();
    }
    return tasks[task_id].address_space.root_physical;
}
