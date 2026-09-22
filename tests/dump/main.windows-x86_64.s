    .text
    .globl _A4anti2rt_main
    .set _A4anti2rt_main, _A4main_main
_A4main_scale:
.L_A4main_scale.b0:
    imulq $6, %rcx, %rax
    ret
.Lanti_debug_fn0_end:
_A4main_main:
    .seh_proc _A4main_main
.L_A4main_main.b0:
    pushq %rbp
    .seh_pushreg %rbp
    movq %rsp, %rbp
    subq $32, %rsp
    .seh_stackalloc 32
    .seh_endprologue
    movq $7, %rcx
    call _A4main_scale
    addq $32, %rsp
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
    .long .Lanti_debug_fn0_end - _A4main_scale
    .long 0, 0           /* the ends of the prologue and the epilogue */
    .long 0              /* no type */
    .secrel32 _A4main_scale
    .secidx _A4main_scale
    .byte 0              /* the flags */
    .asciz "_A4main_scale"
    .p2align 2
.Lanti_cv_fn0_end:
    .short 2
    .short 6             /* S_END */
    .short .Lanti_cv_fn1_end - .Lanti_cv_fn1
.Lanti_cv_fn1:
    .short 4367          /* S_LPROC32 */
    .long 0, 0, 0        /* the parent, the end, the next */
    .long .Lanti_debug_fn1_end - _A4main_main
    .long 0, 0           /* the ends of the prologue and the epilogue */
    .long 0              /* no type */
    .secrel32 _A4main_main
    .secidx _A4main_main
    .byte 0              /* the flags */
    .asciz "_A4main_main"
    .p2align 2
.Lanti_cv_fn1_end:
    .short 2
    .short 6             /* S_END */
.Lanti_cv_symbols_end:
