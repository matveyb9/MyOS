#ifndef MYOS_FRAMEBUFFER_H
#define MYOS_FRAMEBUFFER_H

#include <stdint.h>

struct limine_framebuffer;

int framebuffer_console_init(const struct limine_framebuffer *framebuffer);
int framebuffer_console_available(void);
void framebuffer_console_putc(char character);
void framebuffer_console_clear(void);
uint64_t framebuffer_console_columns(void);
uint64_t framebuffer_console_rows(void);
uint64_t framebuffer_console_scroll_count(void);

#endif
