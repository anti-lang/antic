    .text
    .globl _A4anti2rt_main
    .set _A4anti2rt_main, _A7strings_main
_A7strings_tail:
    .seh_proc _A7strings_tail
.L_A7strings_tail.b0:
    pushq %rbp
    .seh_pushreg %rbp
    movq %rsp, %rbp
    subq $16, %rsp
    .seh_stackalloc 16
    .seh_endprologue
    leaq (%rsp), %rax
    movq (%rdx), %r9
    movq 8(%rdx), %rdx
    addq %r8, %r9
    movq %r9, (%rax)
    subq %r8, %rdx
    movq %rdx, 8(%rax)
    movq (%rax), %rdx
    movq %rdx, (%rcx)
    movq 8(%rax), %rax
    movq %rax, 8(%rcx)
    movq %rcx, %rax
    addq $16, %rsp
    popq %rbp
    ret
.Lanti_debug_fn0_end:
    .seh_endproc
_A7strings_main:
    .seh_proc _A7strings_main
.L_A7strings_main.b0:
    pushq %rbp
    .seh_pushreg %rbp
    movq %rsp, %rbp
    subq $112, %rsp
    .seh_stackalloc 112
    movq %rbx, 104(%rsp)
    .seh_savereg %rbx, 104
    movq %rsi, 96(%rsp)
    .seh_savereg %rsi, 96
    .seh_endprologue
    leaq 32(%rsp), %rbx
    leaq 48(%rsp), %rax
    leaq _A7strings_0(%rip), %rcx
    movq %rcx, (%rax)
    movq $5, 8(%rax)
    leaq 64(%rsp), %rsi
    leaq 80(%rsp), %rdx
    movq (%rax), %rcx
    movq %rcx, (%rdx)
    movq 8(%rax), %rax
    movq %rax, 8(%rdx)
    movq $1, %r8
    movq %rsi, %rcx
    call _A7strings_tail
    movq (%rsi), %rax
    movq %rax, (%rbx)
    movq 8(%rsi), %rax
    movq %rax, 8(%rbx)
    leaq _A7strings_1(%rip), %rcx
    call puts
    movq (%rbx), %rax
    movb (%rax), %al
    movzbq %al, %rax
    movq 104(%rsp), %rbx
    movq 96(%rsp), %rsi
    addq $112, %rsp
    popq %rbp
    ret
.Lanti_debug_fn1_end:
    .seh_endproc
    .section .debug$S,"dr"
    .p2align 2
    .long 4              /* the section starts with a 4 */
    .long 241            /* DEBUG_S_SYMBOLS */
    .long .Lanti_cv_symbols_end - .Lanti_cv_symbols
.Lanti_cv_symbols:
    .short .Lanti_cv_fn0_end - .Lanti_cv_fn0
.Lanti_cv_fn0:
    .short 4367          /* S_LPROC32 */
    .long 0, 0, 0        /* the parent, the end, the next */
    .long .Lanti_debug_fn0_end - _A7strings_tail
    .long 0, 0           /* the ends of the prologue and the epilogue */
    .long 0              /* no type */
    .secrel32 _A7strings_tail
    .secidx _A7strings_tail
    .byte 0              /* the flags */
    .asciz "strings.tail"
    .p2align 2
.Lanti_cv_fn0_end:
    .short 2
    .short 6             /* S_END */
    .short .Lanti_cv_fn1_end - .Lanti_cv_fn1
.Lanti_cv_fn1:
    .short 4367          /* S_LPROC32 */
    .long 0, 0, 0        /* the parent, the end, the next */
    .long .Lanti_debug_fn1_end - _A7strings_main
    .long 0, 0           /* the ends of the prologue and the epilogue */
    .long 0              /* no type */
    .secrel32 _A7strings_main
    .secidx _A7strings_main
    .byte 0              /* the flags */
    .asciz "strings.main"
    .p2align 2
.Lanti_cv_fn1_end:
    .short 2
    .short 6             /* S_END */
.Lanti_cv_symbols_end:
    .section .rdata,"dr"
_A7strings_0:
    .byte 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x00
_A7strings_1:
    .byte 0x68, 0x69, 0x00
    .p2align 3
    .globl anti_rt_slots
anti_rt_slots:
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .p2align 3
    .globl anti_rt_injectable
anti_rt_injectable:
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
