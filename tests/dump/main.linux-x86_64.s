    .text
    .globl anti.rt.main
    .set anti.rt.main, main.main
main.scale:
.Lmain.scale.b0:
    imulq $6, %rdi, %rax
    ret
main.main:
.Lmain.main.b0:
    pushq %rbp
    movq %rsp, %rbp
    movq $7, %rdi
    call main.scale
    popq %rbp
    ret
    .section .rodata
    .p2align 3
    .globl anti_rt_injectable
anti_rt_injectable:
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .section .note.GNU-stack,"",@progbits
