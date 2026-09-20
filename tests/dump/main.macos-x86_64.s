    .build_version macos, 11, 0
    .text
    .globl _anti.rt.main
    .set _anti.rt.main, _main.main
    .section __TEXT,__const
    .globl _anti_cpu_required
    .p2align 2
_anti_cpu_required:
    .long 3
    .text
_main.scale:
L_main.scale.b0:
    imulq $6, %rdi, %rax
    ret
_main.main:
L_main.main.b0:
    pushq %rbp
    movq %rsp, %rbp
    movq $7, %rdi
    call _main.scale
    popq %rbp
    ret
