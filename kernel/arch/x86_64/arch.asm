BITS 64
DEFAULT REL

SECTION .text

global arch_halt
arch_halt:
    cli
.hang:
    hlt
    jmp .hang

global arch_out8
arch_out8:
    mov dx, di
    mov al, sil
    out dx, al
    ret

global arch_in8
arch_in8:
    mov dx, di
    xor eax, eax
    in al, dx
    ret

global arch_enable_interrupts
arch_enable_interrupts:
    sti
    ret

global arch_wait_for_interrupt
arch_wait_for_interrupt:
    hlt
    ret

global arch_read_rflags
arch_read_rflags:
    pushfq
    pop rax
    ret

global arch_trigger_divide_by_zero
arch_trigger_divide_by_zero:
    mov rax, 1
    xor rdx, rdx
    xor rcx, rcx
    div rcx
    jmp arch_halt

global arch_load_gdt
arch_load_gdt:
    lgdt [rdi]
    push qword 0x08
    lea rax, [rel .reload_code_segment]
    push rax
    retfq
.reload_code_segment:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

global arch_load_idt
arch_load_idt:
    lidt [rdi]
    ret

extern idt_handle_exception
extern irq_dispatch

irq_common:
    cld
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, [rsp + 120] ; IRQ number pushed by the stub
    mov rax, rsp
    and rsp, -16
    sub rsp, 16
    mov [rsp], rax
    call irq_dispatch
    mov rsp, [rsp]

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    add rsp, 8
    iretq

isr_common:
    cld
    mov rdi, [rsp]       ; exception vector
    mov rsi, [rsp + 8]   ; CPU error code, or zero inserted by stub
    mov rdx, [rsp + 16]  ; saved RIP
    sub rsp, 8           ; align stack before System V ABI call
    call idt_handle_exception
    jmp arch_halt

%macro ISR_NO_ERROR 1
global isr%1
isr%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro

%macro ISR_WITH_ERROR 1
global isr%1
isr%1:
    push qword %1
    jmp isr_common
%endmacro

ISR_NO_ERROR 0
ISR_NO_ERROR 1
ISR_NO_ERROR 2
ISR_NO_ERROR 3
ISR_NO_ERROR 4
ISR_NO_ERROR 5
ISR_NO_ERROR 6
ISR_NO_ERROR 7
ISR_WITH_ERROR 8
ISR_NO_ERROR 9
ISR_WITH_ERROR 10
ISR_WITH_ERROR 11
ISR_WITH_ERROR 12
ISR_WITH_ERROR 13
ISR_WITH_ERROR 14
ISR_NO_ERROR 15
ISR_NO_ERROR 16
ISR_WITH_ERROR 17
ISR_NO_ERROR 18
ISR_NO_ERROR 19
ISR_NO_ERROR 20
ISR_WITH_ERROR 21
ISR_NO_ERROR 22
ISR_NO_ERROR 23
ISR_NO_ERROR 24
ISR_NO_ERROR 25
ISR_NO_ERROR 26
ISR_NO_ERROR 27
ISR_NO_ERROR 28
ISR_WITH_ERROR 29
ISR_WITH_ERROR 30
ISR_NO_ERROR 31

%macro IRQ 1
global irq%1
irq%1:
    push qword %1
    jmp irq_common
%endmacro

IRQ 0
IRQ 1
IRQ 2
IRQ 3
IRQ 4
IRQ 5
IRQ 6
IRQ 7
IRQ 8
IRQ 9
IRQ 10
IRQ 11
IRQ 12
IRQ 13
IRQ 14
IRQ 15

SECTION .note.GNU-stack noalloc noexec nowrite progbits
