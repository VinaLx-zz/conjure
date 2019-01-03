.text

.globl ContextSwitch

.intel_syntax

ContextSwitch:
    // Save Context
    mov qword ptr [rsi + 0], rbx
    mov qword ptr [rsi + 8], rbp
    mov qword ptr [rsi + 16], r12
    mov qword ptr [rsi + 24], r13
    mov qword ptr [rsi + 32], r14
    mov qword ptr [rsi + 40], r15

    lea rcx, qword ptr [rsp + 8]
    mov qword ptr [rsi + 48], rcx

    mov rcx, qword ptr [rsp]
    mov qword ptr [rsi + 56], rcx

    // Load Context
    mov rbx, qword ptr [rdx + 0]
    mov rbp, qword ptr [rdx + 8]
    mov r12, qword ptr [rdx + 16]
    mov r13, qword ptr [rdx + 24]
    mov r14, qword ptr [rdx + 32]
    mov r15, qword ptr [rdx + 40]
    mov rsp, qword ptr [rdx + 48]
    mov rax, qword ptr [rdx + 56]

    jmp rax
