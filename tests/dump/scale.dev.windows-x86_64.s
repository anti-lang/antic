    .text
    .globl _A3com7example5scale_scale
_A3com7example5scale_scale:
    .seh_proc _A3com7example5scale_scale
.L_A3com7example5scale_scale.b0:
    pushq %rbp
    .seh_pushreg %rbp
    movq %rsp, %rbp
    subq $48, %rsp
    .seh_stackalloc 48
    movq %rbx, 40(%rsp)
    .seh_savereg %rbx, 40
    .seh_endprologue
    movq %rcx, %rax
    imulq $6, %rax, %rbx
    jno .L_A3com7example5scale_scale.b2
.L_A3com7example5scale_scale.b1:
    leaq _A3com7example5scale_0(%rip), %rcx
    movq $6, 32(%rsp)
    movq $39, %rdx
    movl $1, %r8d
    movq %rax, %r9
    call anti_rt_check_failed
.L_A3com7example5scale_scale.b2:
    movq %rbx, %rax
    movq 40(%rsp), %rbx
    addq $48, %rsp
    popq %rbp
    ret
    .seh_endproc
    .section .rdata,"dr"
    .globl _A3com7example5scale_0
_A3com7example5scale_0:
    .byte 0x63, 0x6f, 0x6d, 0x2f, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2f, 0x73, 0x63, 0x61, 0x6c
    .byte 0x65, 0x2e, 0x61, 0x6e, 0x74, 0x69, 0x3a, 0x34, 0x3a, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x66, 0x6c
    .byte 0x6f, 0x77, 0x20, 0x69, 0x6e, 0x20, 0x2a, 0x00
