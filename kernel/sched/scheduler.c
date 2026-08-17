#include <stdint.h>

#include <arch.h>
#include <scheduler.h>

#define SCHEDULER_STACK_SIZE (64U * 1024U)
#define SCHEDULER_CONTEXT_WORDS 21U
#define SCHEDULER_KERNEL_CODE_SELECTOR UINT64_C(0x08)
#define SCHEDULER_KERNEL_DATA_SELECTOR UINT64_C(0x10)
#define SCHEDULER_INITIAL_RFLAGS UINT64_C(0x202)

struct task {
    uint64_t id;
    enum task_state state;
    const char *name;
    kernel_thread_entry_t entry;
    void *argument;
    uint64_t *saved_context;
    uint64_t run_count;
    uint8_t stack[SCHEDULER_STACK_SIZE] __attribute__((aligned(16)));
};

static struct task tasks[SCHEDULER_MAX_TASKS];
static uint64_t current_task_index;
static uint64_t context_switches;
static int scheduler_ready;

static void task_trampoline(void) {
    struct task *task = &tasks[current_task_index];

    task->entry(task->argument);
    task->state = TASK_STATE_TERMINATED;
    for (;;) {
        arch_wait_for_interrupt();
    }
}

static uint64_t *initial_context(struct task *task) {
    uint64_t stack_top = (uint64_t)(uintptr_t)&task->stack[SCHEDULER_STACK_SIZE];
    uint64_t initial_stack;
    uint64_t *context;

    stack_top &= ~(uint64_t)0x0fU;
    initial_stack = stack_top - sizeof(uint64_t);
    context = (uint64_t *)(uintptr_t)(initial_stack - SCHEDULER_CONTEXT_WORDS * sizeof(uint64_t));
    for (uint64_t index = 0U; index < SCHEDULER_CONTEXT_WORDS; index++) {
        context[index] = 0U;
    }
    context[15] = 0U;
    context[16] = (uint64_t)(uintptr_t)task_trampoline;
    context[17] = SCHEDULER_KERNEL_CODE_SELECTOR;
    context[18] = SCHEDULER_INITIAL_RFLAGS;
    context[19] = initial_stack;
    context[20] = SCHEDULER_KERNEL_DATA_SELECTOR;
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

void scheduler_init(void) {
    for (uint64_t index = 0U; index < SCHEDULER_MAX_TASKS; index++) {
        tasks[index].id = index;
        tasks[index].state = TASK_STATE_UNUSED;
        tasks[index].name = "unused";
        tasks[index].entry = (kernel_thread_entry_t)0;
        tasks[index].argument = (void *)0;
        tasks[index].saved_context = (uint64_t *)0;
        tasks[index].run_count = 0U;
    }

    tasks[0].state = TASK_STATE_RUNNING;
    tasks[0].name = "kernel";
    tasks[0].run_count = 1U;
    current_task_index = 0U;
    context_switches = 0U;
    scheduler_ready = 1;
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
        task->name = name;
        task->entry = entry;
        task->argument = argument;
        task->saved_context = initial_context(task);
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

const char *scheduler_task_name(uint64_t task_id) {
    return task_id < SCHEDULER_MAX_TASKS ? tasks[task_id].name : "invalid";
}

uint64_t scheduler_task_run_count(uint64_t task_id) {
    return task_id < SCHEDULER_MAX_TASKS ? tasks[task_id].run_count : 0U;
}
