#ifndef MYOS_USER_H
#define MYOS_USER_H

#include <stdint.h>

int user_demo_prepare(void);
void user_demo_enter(void) __attribute__((noreturn));
uint64_t user_demo_code_address(void);
uint64_t user_demo_stack_top(void);

#endif
