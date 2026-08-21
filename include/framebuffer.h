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

int framebuffer_gui_begin(void);
void framebuffer_gui_end(void);
int framebuffer_gui_active(void);
void framebuffer_gui_on_timer_tick(void);
char framebuffer_gui_handle_input(char character);
char framebuffer_gui_handle_mouse(int64_t delta_x, int64_t delta_y, int left_pressed, int left_was_pressed);
int framebuffer_gui_set_content(const char *title, const uint8_t *data, uint64_t length, uint64_t flags,
                                uint64_t cursor, uint64_t viewport);

#endif
