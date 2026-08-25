#include <stdint.h>

#include <myos.h>

void _start(uint64_t argc, const char *arguments) __attribute__((noreturn));

void _start(uint64_t argc, const char *arguments) {
    myos_exit((uint64_t)myos_main(argc, arguments));
}
