    .build_version macos, 11, 0
    .text
    .globl _com.example.scale.scale
    .private_extern _com.example.scale.scale
    .p2align 2
_com.example.scale.scale:
L_com.example.scale.scale.b0:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #16
    str x19, [sp, #8]
    mov x19, x0
    mov x9, #6
    mul x9, x19, x9
    mov x10, #6
    smulh x10, x19, x10
    asr x9, x9, #63
    cmp x10, x9
    b.eq L_com.example.scale.scale.b2
L_com.example.scale.scale.b1:
    adrp x0, _com.example.scale.0@PAGE
    add x0, x0, _com.example.scale.0@PAGEOFF
    mov x1, #47
    mov w2, #1
    mov x3, x19
    mov x4, #6
    bl _anti_rt_check_failed
L_com.example.scale.scale.b2:
    mov x9, #6
    mul x0, x19, x9
    ldr x19, [sp, #8]
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .section __TEXT,__const
    .globl _com.example.scale.0
    .private_extern _com.example.scale.0
_com.example.scale.0:
    .byte 0x6d, 0x6f, 0x64, 0x75, 0x6c, 0x65, 0x73, 0x2f, 0x63, 0x6f, 0x6d, 0x2f, 0x65, 0x78, 0x61, 0x6d
    .byte 0x70, 0x6c, 0x65, 0x2f, 0x73, 0x63, 0x61, 0x6c, 0x65, 0x2e, 0x61, 0x6e, 0x74, 0x69, 0x3a, 0x34
    .byte 0x3a, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x66, 0x6c, 0x6f, 0x77, 0x20, 0x69, 0x6e, 0x20, 0x2a, 0x00
