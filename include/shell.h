#ifndef MYOS_SHELL_H
#define MYOS_SHELL_H

#include <stdint.h>

struct shell_context {
    uint64_t usable_memory_bytes;
    uint64_t memory_region_count;
    const char *firmware_name;
};

void shell_run(const struct shell_context *context) __attribute__((noreturn));

#endif
