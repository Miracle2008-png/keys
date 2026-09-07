; Real x86-64 assembly (NASM): sections, labels, registers, directives.

section .data
    message     db  "KEYS", 0x0a, 0
    msg_len     equ $ - message
    counter     dq  0

section .bss
    buffer      resb 256

section .text
    global _start
    extern printf

_start:
    mov     rax, 1                  ; sys_write
    mov     rdi, 1                  ; stdout
    lea     rsi, [rel message]
    mov     rdx, msg_len
    syscall

    xor     rcx, rcx
.loop:
    cmp     rcx, 10
    jge     .done
    inc     qword [rel counter]
    add     rcx, 1
    jmp     .loop

.done:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    call    printf
    leave

    mov     rax, 60                 ; sys_exit
    xor     rdi, rdi
    syscall
