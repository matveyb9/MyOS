#ifndef MYOS_PIC_H
#define MYOS_PIC_H

#include <stdint.h>

#define PIC_IRQ_COUNT 16U
#define PIC_IRQ_BASE 0x20U

void pic_init(void);
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);
void pic_send_eoi(uint8_t irq);
uint16_t pic_current_mask(void);

#endif
