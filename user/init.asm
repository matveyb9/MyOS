BITS 64
DEFAULT REL

GLOBAL _start

SECTION .text
_start:
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel message]
    mov rdx, message_end - message
    syscall

.wait:
    jmp .wait

SECTION .rodata
message: db 'MyOS /init: userspace is live.', 10
message_end:
