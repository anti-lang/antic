    .text
    .globl twice
    .p2align 2
twice:
.Ltwice.b0:
    lsl x0, x0, #1
    ret
.Lanti_debug_fn0_end:
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
    .long .Lanti_debug_fn0_end - twice
    .long 0, 0           /* the ends of the prologue and the epilogue */
    .long 0              /* no type */
    .secrel32 twice
    .secidx twice
    .byte 0              /* the flags */
    .asciz "twice"
    .p2align 2
.Lanti_cv_fn0_end:
    .short 2
    .short 6             /* S_END */
.Lanti_cv_symbols_end:
    .section .CRT$XCU,"dr"
    .p2align 3
    .quad anti_rt_init
