    .text
    .globl anti.rt.main
    .set anti.rt.main, letters.main
letters.main:
.Lletters.main.b0:
    pushq %rbp
    movq %rsp, %rbp
    subq $16, %rsp
    movq %rbx, 8(%rsp)
    movq $0, %rbx
.Lletters.main.b1:
    cmpq $3, %rbx
    jge .Lletters.main.b3
.Lletters.main.b2:
    movl %ebx, %eax
    movl %eax, %edi
    addl $65, %edi
    call putchar
    addq $1, %rbx
    jmp .Lletters.main.b1
.Lletters.main.b3:
    movq %rbx, %rax
    movq 8(%rsp), %rbx
    movq %rbp, %rsp
    popq %rbp
    ret
    .section .rodata
    .p2align 3
    .globl anti_rt_slots
anti_rt_slots:
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .p2align 3
    .globl anti_rt_injectable
anti_rt_injectable:
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .section .note.GNU-stack,"",@progbits
