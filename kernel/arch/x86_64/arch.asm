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

SECTION .note.GNU-stack noalloc noexec nowrite progbits
