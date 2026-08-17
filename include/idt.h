#ifndef MYOS_IDT_H
#define MYOS_IDT_H

void idt_init(void);
void idt_install_irq_gates(void);
void idt_handle_exception(unsigned long vector, unsigned long error_code,
                          unsigned long instruction_pointer) __attribute__((noreturn));

#endif
