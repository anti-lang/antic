#include "select.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "regalloc.h"

/* The x86_64 back end: registers, opcodes and the pattern table of
   instruction selection. Register numbers follow the instruction encoding,
   from rax as 0 to r15 as 15, and xmm0 to xmm15 follow as 16 to 31. */

enum {
    RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13,
    R14, R15, XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7, XMM8, XMM9,
    XMM10, XMM11, XMM12, XMM13, XMM14, XMM15
};

enum x64_op {
    X64_MOV, X64_MOVABS, X64_ADD, X64_SUB, X64_IMUL, X64_IMUL3, X64_AND,
    X64_OR, X64_XOR, X64_NEG, X64_NOT, X64_CMP, X64_TEST, X64_SET, X64_JMP,
    X64_J, X64_CALL, X64_CALLR, X64_RET, X64_LEA, X64_PUSH, X64_POP, X64_CQTO,
    X64_CLTD, X64_IDIV, X64_DIV, X64_SHL, X64_SAR, X64_SHR, X64_MOVSX,
    X64_MOVZX, X64_MOVS, X64_ADDS, X64_SUBS, X64_MULS, X64_DIVS, X64_UCOMIS,
    X64_CVTSI2S, X64_CVTTS2SI, X64_CVTS2S, X64_MOVQX, X64_XORP, X64_ANDP,
    X64_ANDNP, X64_ORP, X64_MOVUPS, X64_CMOV, X64_MOVL, X64_SEH_PUSHREG,
    X64_SEH_STACKALLOC, X64_SEH_SAVEREG, X64_SEH_SAVEXMM, X64_SEH_ENDPROLOGUE,
    X64_CVTPH2PS, X64_CVTPS2PH, X64_IMUL1, X64_MUL1, X64_ADC, X64_SBB, X64_BT,
    /* The vector instructions of simd structs, from the first to the
       last, which the printer writes in their VEX forms at x86-64-v3. */
    X64_ADDPS, X64_ADDPD, X64_SUBPS, X64_SUBPD, X64_MULPS, X64_MULPD,
    X64_DIVPS, X64_DIVPD, X64_MINPS, X64_MINPD, X64_MAXPS, X64_MAXPD,
    X64_CMPPS, X64_CMPPD, X64_XORPS, X64_XORPD, X64_PADDB, X64_PADDW,
    X64_PADDD, X64_PADDQ, X64_PSUBB, X64_PSUBW, X64_PSUBD, X64_PSUBQ,
    X64_PMULLW, X64_PMULLD, X64_PAND, X64_PANDN, X64_POR, X64_PXOR,
    X64_PCMPEQB, X64_PCMPEQW, X64_PCMPEQD, X64_PCMPEQQ, X64_PCMPGTB,
    X64_PCMPGTW, X64_PCMPGTD, X64_PCMPGTQ, X64_PMINSB, X64_PMINSW,
    X64_PMINSD, X64_PMINUB, X64_PMINUW, X64_PMINUD, X64_PMAXSB, X64_PMAXSW,
    X64_PMAXSD, X64_PMAXUB, X64_PMAXUW, X64_PMAXUD, X64_PACKSSDW,
    X64_PACKSSWB, X64_PUNPCKLBW, X64_PUNPCKLWD, X64_PUNPCKLDQ,
    X64_PUNPCKLQDQ, X64_UNPCKLPD, X64_PSHUFD, X64_SHUFPS, X64_PSLLD,
    X64_PSLLQ, X64_PSRLDQ, X64_PMOVSXBW, X64_PMOVSXBD, X64_PMOVSXBQ,
    X64_VBROADCASTSS, X64_VBROADCASTSD, X64_VPBROADCASTB, X64_VPBROADCASTW,
    X64_VPBROADCASTD, X64_VPBROADCASTQ, X64_VEXTRACTF128
};

#define USE ROLE_USE
#define DEF ROLE_DEF

/* How the printer writes a vector instruction. A read-and-write form
   names its destination twice under VEX. An immediate comes first, a
   shift has no source beside its destination, and a VEX name carries
   its own v. */
enum { VEC_RW = 1, VEC_IMM = 2, VEC_SHIFT = 4, VEC_VEX = 8 };

/* Most x86_64 instructions have two operands, and the first one is both
   read and written, as in add dst, src. */
static const struct mach_opcode opcodes[] = {
    [X64_MOV] = {"mov", {DEF, USE}, FLAG_MOVE},
    [X64_MOVABS] = {"movabs", {DEF}, 0},
    [X64_ADD] = {"add", {USE | DEF, USE}, 0},
    [X64_SUB] = {"sub", {USE | DEF, USE}, 0},
    [X64_IMUL] = {"imul", {USE | DEF, USE}, 0},
    [X64_IMUL3] = {"imul", {DEF, USE}, 0},
    [X64_AND] = {"and", {USE | DEF, USE}, 0},
    [X64_OR] = {"or", {USE | DEF, USE}, 0},
    [X64_XOR] = {"xor", {USE | DEF, USE}, 0},
    [X64_NEG] = {"neg", {USE | DEF}, 0},
    [X64_NOT] = {"not", {USE | DEF}, 0},
    [X64_CMP] = {"cmp", {USE, USE}, 0},
    [X64_TEST] = {"test", {USE, USE}, 0},
    [X64_SET] = {"set", {DEF}, 0},
    [X64_JMP] = {"jmp", {0}, FLAG_JUMP},
    [X64_J] = {"j", {0}, FLAG_BRANCH},
    [X64_CALL] = {"call", {0}, FLAG_CALL},
    [X64_CALLR] = {"call", {USE}, FLAG_CALL},
    [X64_RET] = {"ret", {0}, FLAG_RET},
    /* Unwind directives of a Windows prologue, lines of the assembly file
       without an instruction. */
    [X64_SEH_PUSHREG] = {".seh_pushreg", {0}, 0},
    [X64_SEH_STACKALLOC] = {".seh_stackalloc", {0}, 0},
    [X64_SEH_SAVEREG] = {".seh_savereg", {0}, 0},
    [X64_SEH_SAVEXMM] = {".seh_savexmm", {0}, 0},
    [X64_SEH_ENDPROLOGUE] = {".seh_endprologue", {0}, 0},
    [X64_LEA] = {"lea", {DEF}, 0},
    [X64_PUSH] = {"push", {USE}, 0},
    [X64_POP] = {"pop", {DEF}, 0},
    [X64_CQTO] = {"cqto", {0}, 0},
    [X64_CLTD] = {"cltd", {0}, 0},
    [X64_IDIV] = {"idiv", {USE}, 0},
    [X64_DIV] = {"div", {USE}, 0},
    [X64_SHL] = {"shl", {USE | DEF, USE}, 0},
    [X64_SAR] = {"sar", {USE | DEF, USE}, 0},
    [X64_SHR] = {"shr", {USE | DEF, USE}, 0},
    [X64_MOVSX] = {"movs", {DEF, USE}, 0},
    [X64_MOVZX] = {"movz", {DEF, USE}, 0},
    /* The SSE instructions end in sd or ss by the width of the float. */
    [X64_MOVS] = {"movs", {DEF, USE}, FLAG_MOVE},
    [X64_ADDS] = {"adds", {USE | DEF, USE}, 0},
    [X64_SUBS] = {"subs", {USE | DEF, USE}, 0},
    [X64_MULS] = {"muls", {USE | DEF, USE}, 0},
    [X64_DIVS] = {"divs", {USE | DEF, USE}, 0},
    [X64_UCOMIS] = {"ucomis", {USE, USE}, 0},
    [X64_CVTSI2S] = {"cvtsi2s", {DEF, USE}, 0},
    [X64_CVTTS2SI] = {"cvtts", {DEF, USE}, 0},
    [X64_CVTS2S] = {"cvts", {DEF, USE}, 0},
    [X64_MOVQX] = {"mov", {DEF, USE}, 0},
    [X64_XORP] = {"xorp", {USE | DEF, USE}, 0},
    [X64_ANDP] = {"andp", {USE | DEF, USE}, 0},
    [X64_ANDNP] = {"andnp", {USE | DEF, USE}, 0},
    [X64_ORP] = {"orp", {USE | DEF, USE}, 0},
    [X64_MOVUPS] = {"movups", {DEF, USE}, 0},
    [X64_CMOV] = {"cmov", {USE | DEF, USE}, 0},
    /* A 32-bit move that clears the upper 32 bits, even of its own
       register, so it is no move that register allocation may drop. */
    [X64_MOVL] = {"mov", {DEF, USE}, 0},
    /* F16C, which x86-64-v3 has and which exists in VEX form alone. The
       immediate of vcvtps2ph names the rounding, and 4 takes the mode of
       MXCSR, as every other conversion does. */
    [X64_CVTPH2PS] = {"vcvtph2ps", {DEF, USE}, 0},
    [X64_CVTPS2PH] = {"vcvtps2ph", {DEF, USE, 0}, 0},
    /* The one-operand multiplies, which leave the full product of rax and
       their operand in rdx and rax. */
    [X64_IMUL1] = {"imul", {USE}, 0},
    [X64_MUL1] = {"mul", {USE}, 0},
    /* The add and the subtract that take the carry flag in, and the bit
       test that puts a bit of a register there. */
    [X64_ADC] = {"adc", {USE | DEF, USE}, 0},
    [X64_SBB] = {"sbb", {USE | DEF, USE}, 0},
    [X64_BT] = {"bt", {USE, 0}, 0},
    /* The vector instructions. Most read and write their first operand,
       which the VEX form names twice. A shift takes no source beside it,
       and the names that start with v have no other form. */
    [X64_ADDPS] = {"addps", {USE | DEF, USE}, 0},
    [X64_ADDPD] = {"addpd", {USE | DEF, USE}, 0},
    [X64_SUBPS] = {"subps", {USE | DEF, USE}, 0},
    [X64_SUBPD] = {"subpd", {USE | DEF, USE}, 0},
    [X64_MULPS] = {"mulps", {USE | DEF, USE}, 0},
    [X64_MULPD] = {"mulpd", {USE | DEF, USE}, 0},
    [X64_DIVPS] = {"divps", {USE | DEF, USE}, 0},
    [X64_DIVPD] = {"divpd", {USE | DEF, USE}, 0},
    [X64_MINPS] = {"minps", {USE | DEF, USE}, 0},
    [X64_MINPD] = {"minpd", {USE | DEF, USE}, 0},
    [X64_MAXPS] = {"maxps", {USE | DEF, USE}, 0},
    [X64_MAXPD] = {"maxpd", {USE | DEF, USE}, 0},
    [X64_CMPPS] = {"cmpps", {USE | DEF, USE, 0}, 0},
    [X64_CMPPD] = {"cmppd", {USE | DEF, USE, 0}, 0},
    [X64_XORPS] = {"xorps", {USE | DEF, USE}, 0},
    [X64_XORPD] = {"xorpd", {USE | DEF, USE}, 0},
    [X64_PADDB] = {"paddb", {USE | DEF, USE}, 0},
    [X64_PADDW] = {"paddw", {USE | DEF, USE}, 0},
    [X64_PADDD] = {"paddd", {USE | DEF, USE}, 0},
    [X64_PADDQ] = {"paddq", {USE | DEF, USE}, 0},
    [X64_PSUBB] = {"psubb", {USE | DEF, USE}, 0},
    [X64_PSUBW] = {"psubw", {USE | DEF, USE}, 0},
    [X64_PSUBD] = {"psubd", {USE | DEF, USE}, 0},
    [X64_PSUBQ] = {"psubq", {USE | DEF, USE}, 0},
    [X64_PMULLW] = {"pmullw", {USE | DEF, USE}, 0},
    [X64_PMULLD] = {"pmulld", {USE | DEF, USE}, 0},
    [X64_PAND] = {"pand", {USE | DEF, USE}, 0},
    [X64_PANDN] = {"pandn", {USE | DEF, USE}, 0},
    [X64_POR] = {"por", {USE | DEF, USE}, 0},
    [X64_PXOR] = {"pxor", {USE | DEF, USE}, 0},
    [X64_PCMPEQB] = {"pcmpeqb", {USE | DEF, USE}, 0},
    [X64_PCMPEQW] = {"pcmpeqw", {USE | DEF, USE}, 0},
    [X64_PCMPEQD] = {"pcmpeqd", {USE | DEF, USE}, 0},
    [X64_PCMPEQQ] = {"pcmpeqq", {USE | DEF, USE}, 0},
    [X64_PCMPGTB] = {"pcmpgtb", {USE | DEF, USE}, 0},
    [X64_PCMPGTW] = {"pcmpgtw", {USE | DEF, USE}, 0},
    [X64_PCMPGTD] = {"pcmpgtd", {USE | DEF, USE}, 0},
    [X64_PCMPGTQ] = {"pcmpgtq", {USE | DEF, USE}, 0},
    [X64_PMINSB] = {"pminsb", {USE | DEF, USE}, 0},
    [X64_PMINSW] = {"pminsw", {USE | DEF, USE}, 0},
    [X64_PMINSD] = {"pminsd", {USE | DEF, USE}, 0},
    [X64_PMINUB] = {"pminub", {USE | DEF, USE}, 0},
    [X64_PMINUW] = {"pminuw", {USE | DEF, USE}, 0},
    [X64_PMINUD] = {"pminud", {USE | DEF, USE}, 0},
    [X64_PMAXSB] = {"pmaxsb", {USE | DEF, USE}, 0},
    [X64_PMAXSW] = {"pmaxsw", {USE | DEF, USE}, 0},
    [X64_PMAXSD] = {"pmaxsd", {USE | DEF, USE}, 0},
    [X64_PMAXUB] = {"pmaxub", {USE | DEF, USE}, 0},
    [X64_PMAXUW] = {"pmaxuw", {USE | DEF, USE}, 0},
    [X64_PMAXUD] = {"pmaxud", {USE | DEF, USE}, 0},
    [X64_PACKSSDW] = {"packssdw", {USE | DEF, USE}, 0},
    [X64_PACKSSWB] = {"packsswb", {USE | DEF, USE}, 0},
    [X64_PUNPCKLBW] = {"punpcklbw", {USE | DEF, USE}, 0},
    [X64_PUNPCKLWD] = {"punpcklwd", {USE | DEF, USE}, 0},
    [X64_PUNPCKLDQ] = {"punpckldq", {USE | DEF, USE}, 0},
    [X64_PUNPCKLQDQ] = {"punpcklqdq", {USE | DEF, USE}, 0},
    [X64_UNPCKLPD] = {"unpcklpd", {USE | DEF, USE}, 0},
    [X64_PSHUFD] = {"pshufd", {DEF, USE, 0}, 0},
    [X64_SHUFPS] = {"shufps", {USE | DEF, USE, 0}, 0},
    [X64_PSLLD] = {"pslld", {USE | DEF, 0}, 0},
    [X64_PSLLQ] = {"psllq", {USE | DEF, 0}, 0},
    [X64_PSRLDQ] = {"psrldq", {USE | DEF, 0}, 0},
    [X64_PMOVSXBW] = {"pmovsxbw", {DEF, USE}, 0},
    [X64_PMOVSXBD] = {"pmovsxbd", {DEF, USE}, 0},
    [X64_PMOVSXBQ] = {"pmovsxbq", {DEF, USE}, 0},
    [X64_VBROADCASTSS] = {"vbroadcastss", {DEF, USE}, 0},
    [X64_VBROADCASTSD] = {"vbroadcastsd", {DEF, USE}, 0},
    [X64_VPBROADCASTB] = {"vpbroadcastb", {DEF, USE}, 0},
    [X64_VPBROADCASTW] = {"vpbroadcastw", {DEF, USE}, 0},
    [X64_VPBROADCASTD] = {"vpbroadcastd", {DEF, USE}, 0},
    [X64_VPBROADCASTQ] = {"vpbroadcastq", {DEF, USE}, 0},
    [X64_VEXTRACTF128] = {"vextractf128", {DEF, USE, 0}, 0},
};

/* How the printer writes each vector instruction. */
static const uint8_t vector_forms[] = {
    [X64_ADDPS] = VEC_RW, [X64_ADDPD] = VEC_RW, [X64_SUBPS] = VEC_RW,
    [X64_SUBPD] = VEC_RW, [X64_MULPS] = VEC_RW, [X64_MULPD] = VEC_RW,
    [X64_DIVPS] = VEC_RW, [X64_DIVPD] = VEC_RW, [X64_MINPS] = VEC_RW,
    [X64_MINPD] = VEC_RW, [X64_MAXPS] = VEC_RW, [X64_MAXPD] = VEC_RW,
    [X64_CMPPS] = VEC_RW | VEC_IMM, [X64_CMPPD] = VEC_RW | VEC_IMM,
    [X64_XORPS] = VEC_RW, [X64_XORPD] = VEC_RW, [X64_PADDB] = VEC_RW,
    [X64_PADDW] = VEC_RW, [X64_PADDD] = VEC_RW, [X64_PADDQ] = VEC_RW,
    [X64_PSUBB] = VEC_RW, [X64_PSUBW] = VEC_RW, [X64_PSUBD] = VEC_RW,
    [X64_PSUBQ] = VEC_RW, [X64_PMULLW] = VEC_RW, [X64_PMULLD] = VEC_RW,
    [X64_PAND] = VEC_RW, [X64_PANDN] = VEC_RW, [X64_POR] = VEC_RW,
    [X64_PXOR] = VEC_RW, [X64_PCMPEQB] = VEC_RW, [X64_PCMPEQW] = VEC_RW,
    [X64_PCMPEQD] = VEC_RW, [X64_PCMPEQQ] = VEC_RW, [X64_PCMPGTB] = VEC_RW,
    [X64_PCMPGTW] = VEC_RW, [X64_PCMPGTD] = VEC_RW, [X64_PCMPGTQ] = VEC_RW,
    [X64_PMINSB] = VEC_RW, [X64_PMINSW] = VEC_RW, [X64_PMINSD] = VEC_RW,
    [X64_PMINUB] = VEC_RW, [X64_PMINUW] = VEC_RW, [X64_PMINUD] = VEC_RW,
    [X64_PMAXSB] = VEC_RW, [X64_PMAXSW] = VEC_RW, [X64_PMAXSD] = VEC_RW,
    [X64_PMAXUB] = VEC_RW, [X64_PMAXUW] = VEC_RW, [X64_PMAXUD] = VEC_RW,
    [X64_PACKSSDW] = VEC_RW, [X64_PACKSSWB] = VEC_RW,
    [X64_PUNPCKLBW] = VEC_RW, [X64_PUNPCKLWD] = VEC_RW,
    [X64_PUNPCKLDQ] = VEC_RW, [X64_PUNPCKLQDQ] = VEC_RW,
    [X64_UNPCKLPD] = VEC_RW, [X64_PSHUFD] = VEC_IMM,
    [X64_SHUFPS] = VEC_RW | VEC_IMM, [X64_PSLLD] = VEC_SHIFT,
    [X64_PSLLQ] = VEC_SHIFT, [X64_PSRLDQ] = VEC_SHIFT,
    [X64_PMOVSXBW] = 0, [X64_PMOVSXBD] = 0, [X64_PMOVSXBQ] = 0,
    [X64_VBROADCASTSS] = VEC_VEX, [X64_VBROADCASTSD] = VEC_VEX,
    [X64_VPBROADCASTB] = VEC_VEX, [X64_VPBROADCASTW] = VEC_VEX,
    [X64_VPBROADCASTD] = VEC_VEX, [X64_VPBROADCASTQ] = VEC_VEX,
    [X64_VEXTRACTF128] = VEC_VEX | VEC_IMM,
};

#define BIT(r) ((uint64_t)1 << (r))

static const uint8_t sysv_args[] = {RDI, RSI, RDX, RCX, R8, R9};
static const uint8_t windows_args[] = {RCX, RDX, R8, R9};
static const uint8_t sysv_fp_args[] = {
    XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7
};
static const uint8_t windows_fp_args[] = {XMM0, XMM1, XMM2, XMM3};

/* DESIGN: r10 and r11 are the scratch registers of spilled values, and
   rbp holds the frame pointer. Allocation prefers the caller-saved
   registers and uses the callee-saved ones last. */
static const uint8_t sysv_allocatable[] = {
    RAX, RCX, RDX, RSI, RDI, R8, R9, RBX, R12, R13, R14, R15
};
static const uint8_t windows_allocatable[] = {
    RAX, RCX, RDX, R8, R9, RBX, RSI, RDI, R12, R13, R14, R15
};

/* DESIGN: xmm14 and xmm15 are the float scratch registers under System V,
   where every xmm register is caller-saved. Windows preserves xmm6 to
   xmm15, so its float scratch registers are the caller-saved xmm4 and
   xmm5. */
static const uint8_t sysv_fp_allocatable[] = {
    XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7, XMM8, XMM9, XMM10,
    XMM11, XMM12, XMM13
};
static const uint8_t windows_fp_allocatable[] = {
    XMM0, XMM1, XMM2, XMM3, XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12,
    XMM13, XMM14, XMM15
};

#define XMM_ALL (((uint64_t)0xffff) << XMM0)
#define XMM_WINDOWS_CALLEE_SAVED (((uint64_t)0x3ff) << XMM6)

static const struct abi sysv = {
    .int_args = sysv_args,
    .int_arg_count = 6,
    .int_result = RAX,
    .caller_saved = BIT(RAX) | BIT(RCX) | BIT(RDX) | BIT(RSI) | BIT(RDI) |
                    BIT(R8) | BIT(R9) | BIT(R10) | BIT(R11) | XMM_ALL,
    .callee_saved = BIT(RBX) | BIT(R12) | BIT(R13) | BIT(R14) | BIT(R15),
    .allocatable = sysv_allocatable,
    .allocatable_count = sizeof sysv_allocatable,
    .scratch = {R10, R11},
    .shadow_space = 0,
    .probe_stack = false,
    .fp_args = sysv_fp_args,
    .fp_arg_count = 8,
    .fp_result = XMM0,
    .fp_allocatable = sysv_fp_allocatable,
    .fp_allocatable_count = sizeof sysv_fp_allocatable,
    .fp_scratch = {XMM14, XMM15},
    .fp_save_size = 8,
};
static const struct abi windows = {
    .int_args = windows_args,
    .int_arg_count = 4,
    .int_result = RAX,
    .caller_saved = BIT(RAX) | BIT(RCX) | BIT(RDX) | BIT(R8) | BIT(R9) |
                    BIT(R10) | BIT(R11) | (XMM_ALL & ~XMM_WINDOWS_CALLEE_SAVED),
    .callee_saved = BIT(RBX) | BIT(RSI) | BIT(RDI) | BIT(R12) | BIT(R13) |
                    BIT(R14) | BIT(R15) | XMM_WINDOWS_CALLEE_SAVED,
    .allocatable = windows_allocatable,
    .allocatable_count = sizeof windows_allocatable,
    .scratch = {R10, R11},
    .shadow_space = 32,
    .probe_stack = true,
    .fp_args = windows_fp_args,
    .fp_arg_count = 4,
    .fp_result = XMM0,
    .fp_allocatable = windows_fp_allocatable,
    .fp_allocatable_count = sizeof windows_fp_allocatable,
    .fp_scratch = {XMM4, XMM5},
    .fp_save_size = 16,
};

static const struct abi *abi(enum convention convention)
{
    return convention == CONVENTION_WINDOWS_X64 ? &windows : &sysv;
}

static uint8_t width(enum ir_type type)
{
    switch (type) {
    case IR_I8: return 8;
    case IR_I16: return 16;
    case IR_I32:
    case IR_F32: return 32;
    default: return 64;
    }
}

static struct mach_inst *emit1(struct selector *s, enum x64_op op,
                               struct mach_operand a)
{
    return select_emit(s, (uint16_t)op, 1, &a);
}

static void emit2(struct selector *s, enum x64_op op, struct mach_operand a,
                  struct mach_operand b)
{
    struct mach_operand ops[2];
    ops[0] = a;
    ops[1] = b;
    select_emit(s, (uint16_t)op, 2, ops);
}

/* A move between two registers, with movsd or movss for floats. */
static void move(struct selector *s, struct mach_operand dst,
                 struct mach_operand src)
{
    bool fp = dst.kind == MACH_VREG ? s->out->fp[dst.reg] : dst.reg >= XMM0;

    emit2(s, fp ? X64_MOVS : X64_MOV, dst, src);
}

static void move_float(struct selector *s, struct mach_operand dst,
                       struct mach_operand src)
{
    emit2(s, X64_MOVS, dst, src);
}

/* An immediate of a 64-bit instruction is 32 bits, sign-extended. Smaller
   instructions take an immediate of their own width. */
static bool fits_imm(const struct ir_operand *o, uint8_t w)
{
    int64_t v = select_signed(o->as.integer, w);
    return o->kind == IR_INT && (w < 64 || (v >= INT32_MIN && v <= INT32_MAX));
}

static void load(struct selector *s, struct mach_operand dst, uint64_t value)
{
    int64_t v = select_signed(value, dst.width);

    if (dst.width == 64 && (v < INT32_MIN || v > INT32_MAX)) {
        emit2(s, X64_MOVABS, dst, mach_imm(v));
    } else {
        emit2(s, X64_MOV, dst, mach_imm(v));
    }
}

/* Put operand a into register r, unless a is r already. */
static void load_into(struct selector *s, struct mach_operand r,
                      const struct ir_operand *a)
{
    if (a->kind == IR_INT) {
        load(s, r, a->as.integer);
    } else if (a->as.temp != r.reg) {
        move(s, r, select_reg(s, a));
    }
}

/* Patterns */

static bool is_int_type(enum ir_type type)
{
    return type == IR_I8 || type == IR_I16 || type == IR_I32 ||
           type == IR_I64 || type == IR_PTR;
}

/* x86_64 has instructions for every integer width, for arithmetic and
   for a copy. */
static bool match_arith(const struct selector *s, const struct ir_inst *inst)
{
    (void)s;
    return is_int_type(inst->type);
}

static bool match_not_bool(const struct selector *s,
                           const struct ir_inst *inst)
{
    (void)s;
    return inst->type == IR_I8 && inst->b.kind == IR_INT &&
           inst->b.as.integer == 1;
}

static bool match_compare(const struct selector *s,
                          const struct ir_inst *inst)
{
    (void)s;
    return is_int_type(inst->a.type);
}

static bool match_fused(const struct selector *s, const struct ir_inst *inst)
{
    (void)inst;
    return s->fused != NULL;
}

static bool match_call(const struct selector *s, const struct ir_inst *inst)
{
    return (inst->a.kind == IR_FUNC || inst->b.kind == IR_FUNC) &&
           (inst->type == IR_VOID || match_arith(s, inst) ||
            select_is_float(inst->type) || inst->type == IR_AGG);
}

static void emit_copy(struct selector *s, const struct ir_inst *inst)
{
    load_into(s, select_result(s, inst), &inst->a);
}

/* r = a op b becomes mov a, r and op b, r in two-operand form. When r is
   b itself, the mov would destroy b. A commutative operation then applies
   a to r, and another one saves b in a new register first. */
static void two_operand(struct selector *s, const struct ir_inst *inst,
                        enum x64_op op, bool commutative)
{
    struct mach_operand r = select_result(s, inst);
    struct mach_operand b;

    if (fits_imm(&inst->b, r.width)) {
        load_into(s, r, &inst->a);
        emit2(s, op, r, mach_imm(select_signed(inst->b.as.integer, r.width)));
        return;
    }
    if (inst->b.kind == IR_TEMP && inst->b.as.temp == inst->result) {
        if (commutative) {
            b = fits_imm(&inst->a, r.width)
                    ? mach_imm(select_signed(inst->a.as.integer, r.width))
                    : select_reg(s, &inst->a);
            emit2(s, op, r, b);
            return;
        }
        b = select_new_vreg(s, r.width);
        move(s, b, select_reg(s, &inst->b));
    } else {
        b = select_reg(s, &inst->b);
    }
    load_into(s, r, &inst->a);
    emit2(s, op, r, b);
}

static void emit_binary(struct selector *s, const struct ir_inst *inst)
{
    switch (inst->op) {
    case IR_ADD: two_operand(s, inst, X64_ADD, true); break;
    case IR_SUB: two_operand(s, inst, X64_SUB, false); break;
    case IR_AND: two_operand(s, inst, X64_AND, true); break;
    case IR_OR: two_operand(s, inst, X64_OR, true); break;
    default: two_operand(s, inst, X64_XOR, true); break;
    }
}

/* imul has a three-operand form with an immediate. It has no two-operand
   form for 8 bits, so 8-bit and 16-bit products use the 32-bit
   registers, whose low bits hold the same product. */
static void emit_mul(struct selector *s, const struct ir_inst *inst)
{
    struct mach_operand r = select_result(s, inst);
    struct mach_operand ops[3];
    uint8_t w = r.width < 32 ? 32 : r.width;

    if (!fits_imm(&inst->b, r.width)) {
        struct mach_operand b;
        if (inst->b.kind == IR_TEMP && inst->b.as.temp == inst->result) {
            b = mach_widened(select_reg(s, &inst->a), w);
        } else {
            b = mach_widened(select_reg(s, &inst->b), w);
            if (inst->a.kind == IR_INT) {
                load(s, mach_widened(r, w), inst->a.as.integer);
            } else if (inst->a.as.temp != inst->result) {
                move(s, mach_widened(r, w),
                     mach_widened(select_reg(s, &inst->a), w));
            }
        }
        emit2(s, X64_IMUL, mach_widened(r, w), b);
        return;
    }
    ops[0] = mach_widened(r, w);
    ops[1] = mach_widened(select_reg(s, &inst->a), w);
    ops[2] = mach_imm(select_signed(inst->b.as.integer, r.width));
    select_emit(s, X64_IMUL3, 3, ops);
}

/* Extend a value of w bits to the 32-bit register dst, or move it when w
   is 32 or 64 bits already. */
static void extend_into(struct selector *s, struct mach_operand dst,
                        const struct ir_operand *value, bool is_signed)
{
    uint8_t w = width(value->type);

    if (value->kind == IR_INT) {
        uint64_t v = value->as.integer;
        load(s, dst, is_signed ? (uint64_t)select_signed(v, w) : v);
    } else if (w < 32) {
        emit2(s, is_signed ? X64_MOVSX : X64_MOVZX, dst, select_reg(s, value));
    } else {
        move(s, dst, select_reg(s, value));
    }
}

/* Division takes the dividend in rdx:rax and leaves the quotient in rax
   and the remainder in rdx. cqto and cltd sign-extend rax into rdx, and an
   unsigned division clears rdx. 8-bit and 16-bit operands divide as 32-bit
   values after an extension. */
static void emit_div(struct selector *s, const struct ir_inst *inst)
{
    bool is_signed = inst->op == IR_SDIV || inst->op == IR_SREM;
    bool remainder = inst->op == IR_SREM || inst->op == IR_UREM;
    struct mach_operand r = select_result(s, inst);
    uint8_t w = r.width < 32 ? 32 : r.width;
    struct mach_operand divisor;
    struct mach_inst *div;

    if (inst->b.kind == IR_TEMP && r.width >= 32) {
        divisor = select_reg(s, &inst->b);
    } else {
        divisor = select_new_vreg(s, w);
        extend_into(s, divisor, &inst->b, is_signed);
    }
    extend_into(s, mach_preg(RAX, w), &inst->a, is_signed);
    if (is_signed) {
        select_emit(s, w == 64 ? X64_CQTO : X64_CLTD, 0, NULL)->uses =
            BIT(RAX);
        s->b->insts[s->b->count - 1].defs = BIT(RDX);
    } else {
        load(s, mach_preg(RDX, w), 0);
    }
    div = emit1(s, is_signed ? X64_IDIV : X64_DIV, divisor);
    div->uses = BIT(RAX) | BIT(RDX);
    div->defs = BIT(RAX) | BIT(RDX);
    move(s, r, mach_preg(remainder ? RDX : RAX, r.width));
}

/* A 32-bit value to the 64-bit register dst, extended by is_signed. A
   32-bit move clears the upper half, and movslq extends the sign. */
static void widen_32(struct selector *s, struct mach_operand dst,
                     const struct ir_operand *value, bool is_signed)
{
    if (value->kind == IR_INT) {
        load(s, dst, is_signed ? (uint64_t)select_signed(value->as.integer, 32)
                               : value->as.integer & 0xffffffff);
    } else if (is_signed) {
        emit2(s, X64_MOVSX, dst, select_reg(s, value));
    } else {
        emit2(s, X64_MOVL, mach_widened(dst, 32), select_reg(s, value));
    }
}

/* DESIGN: the upper half of a 64-bit product comes from the one-operand
   imul or mul, which leave the whole product in rdx and rax. A narrower
   product is exact in a register twice as wide, or in 32 bits for 8 and
   16, and its upper half is a shift away. */
static void emit_mul_high(struct selector *s, const struct ir_inst *inst)
{
    bool is_signed = inst->op == IR_MULH_S;
    struct mach_operand r = select_result(s, inst);
    struct mach_operand a;
    struct mach_operand b;
    struct mach_inst *mul;

    if (r.width == 64) {
        b = select_reg(s, &inst->b);
        extend_into(s, mach_preg(RAX, 64), &inst->a, is_signed);
        mul = emit1(s, is_signed ? X64_IMUL1 : X64_MUL1, b);
        mul->uses = BIT(RAX);
        mul->defs = BIT(RAX) | BIT(RDX);
        move(s, r, mach_preg(RDX, 64));
        return;
    }
    a = select_new_vreg(s, r.width == 32 ? 64 : 32);
    b = select_new_vreg(s, a.width);
    if (r.width == 32) {
        widen_32(s, a, &inst->a, is_signed);
        widen_32(s, b, &inst->b, is_signed);
    } else {
        extend_into(s, a, &inst->a, is_signed);
        extend_into(s, b, &inst->b, is_signed);
    }
    emit2(s, X64_IMUL, a, b);
    emit2(s, is_signed ? X64_SAR : X64_SHR, a, mach_imm(r.width));
    move(s, r, mach_widened(a, r.width));
}

/* A variable shift count must be in cl. */
static void emit_shift(struct selector *s, const struct ir_inst *inst)
{
    enum x64_op op = inst->op == IR_SHL     ? X64_SHL
                     : inst->op == IR_SHR_S ? X64_SAR
                                            : X64_SHR;
    struct mach_operand r = select_result(s, inst);

    if (inst->b.kind == IR_INT) {
        load_into(s, r, &inst->a);
        emit2(s, op, r, mach_imm((int64_t)(inst->b.as.integer & 0xff)));
        return;
    }
    move(s, mach_preg(RCX, 8), mach_widened(select_reg(s, &inst->b), 8));
    load_into(s, r, &inst->a);
    emit2(s, op, r, mach_preg(RCX, 8));
}

/* trunc keeps the low bits with a move of the smaller width. sext uses
   movsx. zext uses movzx, and from 32 bits a 32-bit move, whose result the
   processor zero-extends to 64 bits. */
static void emit_convert(struct selector *s, const struct ir_inst *inst)
{
    struct mach_operand r = select_result(s, inst);
    uint8_t from = width(inst->a.type);

    if (inst->a.kind == IR_INT) {
        load(s, r, inst->op == IR_SEXT
                       ? (uint64_t)select_signed(inst->a.as.integer, from)
                       : inst->a.as.integer);
    } else if (inst->op == IR_TRUNC) {
        move(s, r, mach_widened(select_reg(s, &inst->a), r.width));
    } else if (inst->op == IR_ZEXT && from == 32) {
        emit2(s, X64_MOVL, mach_widened(r, 32), select_reg(s, &inst->a));
    } else {
        emit2(s, inst->op == IR_SEXT ? X64_MOVSX : X64_MOVZX, r,
              select_reg(s, &inst->a));
    }
}

static bool match_convert(const struct selector *s, const struct ir_inst *inst)
{
    (void)s;
    return is_int_type(inst->type) && is_int_type(inst->a.type);
}

/* The address of a function or a global, relative to rip. A C function
   under System V loads its address from the GOT entry. */
static void emit_addr(struct selector *s, const struct ir_inst *inst)
{
    struct mach_operand symbol = mach_imm(inst->a.as.index);

    symbol.kind = inst->a.kind == IR_FUNC ? MACH_FUNC : MACH_GLOBAL;
    symbol.pc_relative = true;
    symbol.got = select_uses_got(s, &inst->a);
    emit2(s, symbol.got ? X64_MOV : X64_LEA, select_result(s, inst), symbol);
}

/* Floats */


static bool match_float(const struct selector *s, const struct ir_inst *inst)
{
    (void)s;
    return select_is_float(inst->type);
}

static uint64_t float_bits(enum ir_type type, double value)
{
    uint64_t pattern;

    if (type == IR_F32) {
        float narrow = (float)value;
        uint32_t word;
        memcpy(&word, &narrow, sizeof word);
        return word;
    }
    memcpy(&pattern, &value, sizeof pattern);
    return pattern;
}

/* A float constant has no immediate form. Its bits go into an integer
   register and from there with movq or movd into the xmm register. */
static void load_float(struct selector *s, struct mach_operand dst,
                       enum ir_type type, double value)
{
    struct mach_operand bits = select_new_vreg(s, width(type));

    load(s, bits, float_bits(type, value));
    emit2(s, X64_MOVQX, dst, bits);
}

static void emit_float_copy(struct selector *s, const struct ir_inst *inst)
{
    move_float(s, select_result(s, inst), select_reg(s, &inst->a));
}

/* r = a op b in two-operand form, as two_operand does for integers. */
static void emit_float_binary(struct selector *s, const struct ir_inst *inst)
{
    enum x64_op op = inst->op == IR_FADD   ? X64_ADDS
                     : inst->op == IR_FSUB ? X64_SUBS
                     : inst->op == IR_FMUL ? X64_MULS
                                           : X64_DIVS;
    bool commutative = op == X64_ADDS || op == X64_MULS;
    struct mach_operand r = select_result(s, inst);
    struct mach_operand b;

    if (inst->b.kind == IR_TEMP && inst->b.as.temp == inst->result) {
        if (commutative) {
            emit2(s, op, r, select_reg(s, &inst->a));
            return;
        }
        b = select_new_fp_vreg(s, r.width);
        move_float(s, b, select_reg(s, &inst->b));
    } else {
        b = select_reg(s, &inst->b);
    }
    if (inst->a.kind != IR_TEMP || inst->a.as.temp != inst->result) {
        move_float(s, r, select_reg(s, &inst->a));
    }
    emit2(s, op, r, b);
}

/* SSE has no negation. xorpd flips the sign bit with a mask, which
   arrives through an integer register like a constant. */
static void emit_float_neg(struct selector *s, const struct ir_inst *inst)
{
    struct mach_operand r = select_result(s, inst);
    struct mach_operand bits = select_new_vreg(s, r.width);
    struct mach_operand mask = select_new_fp_vreg(s, r.width);

    load(s, bits, (uint64_t)1 << (r.width - 1));
    emit2(s, X64_MOVQX, mask, bits);
    if (inst->a.kind != IR_TEMP || inst->a.as.temp != inst->result) {
        move_float(s, r, select_reg(s, &inst->a));
    }
    emit2(s, X64_XORP, r, mask);
}

/* ucomisd sets the flags of an unsigned comparison: CF for below, ZF for
   equal, and ZF, PF and CF together for NaN. a > b and a >= b are the
   conditions a and ae, which NaN fails, and a < b compares b with a.
   Equality needs PF clear as well, and inequality accepts PF set. */
static void emit_float_compare(struct selector *s, const struct ir_inst *inst)
{
    struct mach_operand r = select_result(s, inst);
    bool swap = inst->op == IR_FLT || inst->op == IR_FLE;
    struct mach_operand a = select_reg(s, swap ? &inst->b : &inst->a);
    struct mach_operand b = select_reg(s, swap ? &inst->a : &inst->b);
    struct mach_operand parity;

    emit2(s, X64_UCOMIS, a, b);
    switch (inst->op) {
    case IR_FGT:
    case IR_FLT:
        emit2(s, X64_SET, r, mach_cond_op(COND_HI));
        break;
    case IR_FGE:
    case IR_FLE:
        emit2(s, X64_SET, r, mach_cond_op(COND_HS));
        break;
    default:
        parity = select_new_vreg(s, 8);
        emit2(s, X64_SET, r,
              mach_cond_op(inst->op == IR_FEQ ? COND_EQ : COND_NE));
        emit2(s, X64_SET, parity,
              mach_cond_op(inst->op == IR_FEQ ? COND_NP : COND_P));
        emit2(s, inst->op == IR_FEQ ? X64_AND : X64_OR, r, parity);
        break;
    }
}


/* A u64 of 2^63 or more has no signed conversion. The value (a >> 1) |
   (a & 1) converts and doubles. The sign bit of a, spread into a mask,
   picks between that result and the plain signed conversion. */
static void unsigned_to_float(struct selector *s, struct mach_operand r,
                              struct mach_operand a)
{
    struct mach_operand half = select_new_vreg(s, 64);
    struct mach_operand low = select_new_vreg(s, 32);
    struct mach_operand large = select_new_fp_vreg(s, r.width);
    struct mach_operand small = select_new_fp_vreg(s, r.width);
    struct mach_operand mask = select_new_fp_vreg(s, r.width);
    struct mach_operand sign = select_new_vreg(s, 64);

    move(s, half, a);
    emit2(s, X64_SHR, half, mach_imm(1));
    move(s, low, mach_widened(a, 32));
    emit2(s, X64_AND, low, mach_imm(1));
    emit2(s, X64_OR, half, mach_widened(low, 64));
    emit2(s, X64_CVTSI2S, large, half);
    emit2(s, X64_ADDS, large, large);
    emit2(s, X64_CVTSI2S, small, a);
    move(s, sign, a);
    emit2(s, X64_SAR, sign, mach_imm(63));
    emit2(s, X64_MOVQX, mask, sign);
    emit2(s, X64_ANDP, large, mask);
    emit2(s, X64_ANDNP, mask, small);
    emit2(s, X64_ORP, large, mask);
    move_float(s, r, large);
}

/* A float of 2^63 or more has no signed conversion to u64. It converts
   after subtracting 2^63, the sign bit of the result comes back with xor,
   and cmovae picks that value when the float is not below 2^63. */
static void float_to_unsigned(struct selector *s, struct mach_operand r,
                              struct mach_operand f)
{
    struct mach_operand bits = select_new_vreg(s, f.width);
    struct mach_operand limit = select_new_fp_vreg(s, f.width);
    struct mach_operand reduced = select_new_fp_vreg(s, f.width);
    struct mach_operand high = select_new_vreg(s, 64);
    struct mach_operand sign = select_new_vreg(s, 64);
    struct mach_operand ops[3];

    emit2(s, X64_CVTTS2SI, r, f);
    load(s, bits,
         float_bits(f.width == 64 ? IR_F64 : IR_F32, 9223372036854775808.0));
    emit2(s, X64_MOVQX, limit, bits);
    move_float(s, reduced, f);
    emit2(s, X64_SUBS, reduced, limit);
    emit2(s, X64_CVTTS2SI, high, reduced);
    load(s, sign, (uint64_t)1 << 63);
    emit2(s, X64_XOR, high, sign);
    emit2(s, X64_UCOMIS, f, limit);
    ops[0] = r;
    ops[1] = high;
    ops[2] = mach_cond_op(COND_HS);
    select_emit(s, X64_CMOV, 3, ops);
}

/* cvtsi2sd converts a signed 32-bit or 64-bit integer and cvttsd2si
   truncates toward zero. Integers of 8 or 16 bits extend to 32 bits and
   a u32 to 64 bits first. A float to an unsigned integer below 64 bits
   converts to the next wider signed integer. */
static void emit_float_convert(struct selector *s, const struct ir_inst *inst)
{
    struct mach_operand r = select_result(s, inst);
    uint8_t from = width(inst->a.type);
    struct mach_operand wide;

    switch (inst->op) {
    case IR_SITOF:
    case IR_UITOF:
        if (inst->op == IR_UITOF && from == 64) {
            unsigned_to_float(s, r, select_reg(s, &inst->a));
        } else if (inst->op == IR_UITOF && from == 32) {
            wide = select_new_vreg(s, 64);
            emit2(s, X64_MOVL, mach_widened(wide, 32), select_reg(s, &inst->a));
            emit2(s, X64_CVTSI2S, r, wide);
        } else if (from < 32) {
            wide = select_new_vreg(s, 32);
            extend_into(s, wide, &inst->a, inst->op == IR_SITOF);
            emit2(s, X64_CVTSI2S, r, wide);
        } else {
            emit2(s, X64_CVTSI2S, r, select_reg(s, &inst->a));
        }
        break;
    case IR_FTOSI:
    case IR_FTOUI:
        if (inst->op == IR_FTOUI && r.width == 64) {
            float_to_unsigned(s, r, select_reg(s, &inst->a));
        } else if (r.width < 32 || (inst->op == IR_FTOUI && r.width == 32)) {
            wide = select_new_vreg(s, r.width == 32 ? 64 : 32);
            emit2(s, X64_CVTTS2SI, wide, select_reg(s, &inst->a));
            move(s, r, mach_widened(wide, r.width));
        } else {
            emit2(s, X64_CVTTS2SI, r, select_reg(s, &inst->a));
        }
        break;
    default:
        emit2(s, X64_CVTS2S, r, select_reg(s, &inst->a));
        break;
    }
}

static void emit_unary(struct selector *s, const struct ir_inst *inst)
{
    struct mach_operand r = select_result(s, inst);

    load_into(s, r, &inst->a);
    emit1(s, inst->op == IR_NEG ? X64_NEG : X64_NOT, r);
}

static void compare(struct selector *s, const struct ir_inst *inst)
{
    struct mach_operand a = select_reg(s, &inst->a);

    if (fits_imm(&inst->b, a.width)) {
        emit2(s, X64_CMP, a, mach_imm(select_signed(inst->b.as.integer,
                                                   a.width)));
    } else {
        emit2(s, X64_CMP, a, select_reg(s, &inst->b));
    }
}

static void emit_set(struct selector *s, const struct ir_inst *inst)
{
    compare(s, inst);
    emit2(s, X64_SET, select_result(s, inst),
          mach_cond_op(select_cond(inst->op)));
}

static void jump(struct selector *s, const struct ir_operand *target)
{
    if (!select_is_next(s, target)) {
        emit1(s, X64_JMP, mach_block_op(target));
    }
}

static void emit_jump(struct selector *s, const struct ir_inst *inst)
{
    jump(s, &inst->a);
}

static void conditional(struct selector *s, const struct ir_inst *inst,
                        enum mach_cond c)
{
    if (select_is_next(s, &inst->b)) {
        emit2(s, X64_J, mach_cond_op(select_negate(c)),
              mach_block_op(&inst->c));
        return;
    }
    emit2(s, X64_J, mach_cond_op(c), mach_block_op(&inst->b));
    jump(s, &inst->c);
}

/* The condition that holds when the operation left the range of its
   type. add, sub and imul leave it in the overflow flag. A product
   narrower than 32 bits goes through the wider registers, whose flag is
   of the wrong width, and ends in a compare instead. */
static enum mach_cond overflow_cond(const struct selector *s,
                                    const struct ir_inst *inst)
{
    return inst->op == IR_MUL_OV && s->target->width(inst->a.type) < 32
               ? COND_NE
               : COND_VS;
}

/* DESIGN: the operation gives its result and leaves whether it
   overflowed, so the arithmetic is emitted once. add, sub and the
   two-operand imul each set the overflow flag for their own width. imul
   has no two-operand form below 32 bits, so a narrow product goes
   through the 32-bit registers and is compared with its own sign
   extension. */
static void emit_overflow(struct selector *s, const struct ir_inst *inst)
{
    uint8_t w = s->target->width(inst->a.type);
    struct mach_operand r = select_result(s, inst);
    struct mach_operand wide;
    struct mach_operand back;

    if (inst->op == IR_MUL_OV && w < 32) {
        wide = select_new_vreg(s, 32);
        back = select_new_vreg(s, 32);
        emit2(s, X64_MOVSX, wide, select_reg(s, &inst->a));
        emit2(s, X64_MOVSX, back, select_reg(s, &inst->b));
        emit2(s, X64_IMUL, wide, back);
        move(s, r, mach_widened(wide, w));
        emit2(s, X64_MOVSX, back, mach_widened(wide, w));
        emit2(s, X64_CMP, wide, back);
        return;
    }
    if (inst->op == IR_MUL_OV) {
        emit_mul(s, inst);
        return;
    }
    two_operand(s, inst, inst->op == IR_ADD_OV ? X64_ADD : X64_SUB,
                inst->op == IR_ADD_OV);
}

/* DESIGN: add, adc, sub, sbb and neg set the four flags for their own
   width, 8 bits included. Every flag operation of an add, a subtract and
   a negation is therefore an instruction. The carry is CF after each of
   them, since x86_64 keeps the borrow of a subtraction there. bt puts
   bit 0 of the bool of a carry in into CF. A move between them leaves the
   flags alone. */
static bool flags_native(const struct ir_inst *inst)
{
    return (inst->op == IR_ADD_FL || inst->op == IR_SUB_FL ||
            inst->op == IR_NEG_FL) &&
           is_int_type(inst->type);
}

static void emit_flag_op(struct selector *s, const struct ir_inst *inst)
{
    bool add = inst->op == IR_ADD_FL;
    struct mach_operand r;

    if (inst->op == IR_NEG_FL) {
        r = select_result(s, inst);
        load_into(s, r, &inst->a);
        emit1(s, X64_NEG, r);
        return;
    }
    if (inst->c.kind != IR_NONE) {
        emit2(s, X64_BT, mach_widened(select_reg(s, &inst->c), 32),
              mach_imm(0));
    }
    two_operand(s, inst,
                inst->c.kind == IR_NONE ? (add ? X64_ADD : X64_SUB)
                : add                   ? X64_ADC
                                        : X64_SBB,
                add);
}

/* One set per flag, right after the instruction that set it. */
static void emit_flag(struct selector *s, const struct ir_inst *inst)
{
    static const enum mach_cond conds[] = {
        [IR_FLAG_OVERFLOW] = COND_VS, [IR_FLAG_CARRY] = COND_LO,
        [IR_FLAG_ZERO] = COND_EQ, [IR_FLAG_NEGATIVE] = COND_MI,
    };

    emit2(s, X64_SET, select_result(s, inst), mach_cond_op(conds[inst->field]));
}

/* The operation right before this one left the flags, so the branch
   needs no compare of its own. */
static void emit_branch_ov(struct selector *s, const struct ir_inst *inst)
{
    conditional(s, inst, overflow_cond(s, s->overflow));
}

static void emit_fused_branch(struct selector *s, const struct ir_inst *inst)
{
    compare(s, s->fused);
    conditional(s, inst, select_cond(s->fused->op));
}

static void emit_branch(struct selector *s, const struct ir_inst *inst)
{
    struct mach_operand c = select_reg(s, &inst->a);

    emit2(s, X64_TEST, c, c);
    conditional(s, inst, COND_NE);
}

static struct mach_operand stack(int64_t offset);
static void copy_memory(struct selector *s, struct mach_operand dst,
                        struct mach_operand src, uint64_t size);
static void slot_address(struct selector *s, struct mach_operand dst,
                         uint32_t slot);

/* The eightbytes of a System V aggregate of at most 16 bytes. Each one
   goes to an integer register unless all of its members are floats. An
   eightbyte without a member is NO_CLASS and takes no register. Returns
   false for a larger aggregate or one with a field off its alignment,
   whose class is MEMORY. */
static bool sysv_classify(const struct layout *agg, struct arg_location *loc,
                          bool fp[2])
{
    size_t count = (size_t)(agg->size + 7) / 8;
    size_t parts = 0;
    size_t k;
    size_t i;

    if (agg->size > 16 || agg->size == 0 || agg->unaligned) {
        return false;
    }
    /* A vector is one eightbyte of class SSE and one of SSEUP, the whole
       of one xmm register. */
    if (agg->vector) {
        loc->parts[0].offset = 0;
        loc->parts[0].bytes = 16;
        loc->part_count = 1;
        fp[0] = true;
        return true;
    }
    for (k = 0; k < count; k++) {
        bool used = false;
        fp[parts] = true;
        for (i = 0; i < agg->member_count; i++) {
            if (agg->members[i].offset / 8 == k) {
                used = true;
                fp[parts] = fp[parts] && select_is_float(agg->members[i].type);
            }
        }
        if (!used) {
            continue;
        }
        loc->parts[parts].offset = (uint8_t)(8 * k);
        loc->parts[parts].bytes = (uint8_t)(agg->size - 8 * k < 8
                                                ? agg->size - 8 * k
                                                : 8);
        parts++;
    }
    loc->part_count = parts;
    return true;
}

/* Windows passes an aggregate of 1, 2, 4 or 8 bytes like an integer of
   that size, and any other aggregate as a pointer to a copy. */
static bool windows_by_value(const struct layout *agg)
{
    return agg->size == 1 || agg->size == 2 || agg->size == 4 ||
           agg->size == 8;
}

static void locate_result(const struct selector *s,
                          const struct ir_function *f,
                          struct arg_location *out)
{
    bool fp[2];
    size_t ints = 0;
    size_t floats = 0;
    size_t k;

    memset(out, 0, sizeof *out);
    out->copy = -1;
    out->reg = select_is_float(f->result) ? s->abi->fp_result
                                          : s->abi->int_result;
    if (f->result != IR_AGG) {
        return;
    }
    if (s->abi == &sysv && sysv_classify(select_layout(s, f->result_agg), out, fp)) {
        static const uint8_t int_results[] = {RAX, RDX};
        static const uint8_t fp_results[] = {XMM0, XMM1};
        for (k = 0; k < out->part_count; k++) {
            out->parts[k].reg = fp[k] ? fp_results[floats++]
                                      : int_results[ints++];
        }
        return;
    }
    /* Windows returns a vector in xmm0 and passes one as a pointer to a
       copy, as MSVC does with __m128. */
    if (s->abi == &windows && select_layout(s, f->result_agg)->vector) {
        out->part_count = 1;
        out->parts[0].reg = XMM0;
        out->parts[0].bytes = 16;
        out->parts[0].offset = 0;
        return;
    }
    if (s->abi == &windows && windows_by_value(select_layout(s, f->result_agg))) {
        out->part_count = 1;
        out->parts[0].reg = RAX;
        out->parts[0].bytes = (uint8_t)select_layout(s, f->result_agg)->size;
        out->parts[0].offset = 0;
        return;
    }
    memset(out, 0, sizeof *out);
    out->indirect = true;
    out->reg = s->abi == &sysv ? RDI : RCX;
    out->copy = RAX;
}

/* DESIGN: System V counts integer and float arguments separately, and
   every argument after its registers goes to the stack in 8 bytes, the
   first one lowest. An aggregate of class MEMORY, or one whose eightbytes
   find no registers, goes to the stack as a whole. Windows gives argument
   i the register at position i of its class and the stack above the
   shadow store from the fifth on. A memory result takes the first integer
   register on both. */
static void locate(const struct selector *s, const struct ir_function *callee,
                   const enum ir_type *types, size_t count,
                   struct arg_location *out)
{
    const struct abi *a = s->abi;
    struct arg_location result;
    size_t ints = 0;
    size_t floats = 0;
    int64_t offset = a->shadow_space;
    size_t i;
    size_t k;

    locate_result(s, callee, &result);
    if (result.indirect) {
        ints = 1;
    }
    for (i = 0; i < count; i++) {
        bool fp = select_is_float(types[i]);
        size_t position = i + ints * (a == &windows);
        const struct layout *agg =
            i < callee->param_count ? select_layout(s, callee->params[i].agg) : NULL;
        bool reg_fp[2];
        memset(&out[i], 0, sizeof out[i]);
        out[i].copy = -1;
        if (types[i] == IR_AGG && a == &sysv &&
            sysv_classify(agg, &out[i], reg_fp)) {
            size_t need_fp = 0;
            for (k = 0; k < out[i].part_count; k++) {
                need_fp += reg_fp[k];
            }
            if (ints + out[i].part_count - need_fp <= a->int_arg_count &&
                floats + need_fp <= a->fp_arg_count) {
                for (k = 0; k < out[i].part_count; k++) {
                    out[i].parts[k].reg = reg_fp[k] ? a->fp_args[floats++]
                                                    : a->int_args[ints++];
                }
                continue;
            }
            out[i].part_count = 0;
        }
        if (types[i] == IR_AGG && a == &sysv) {
            /* DESIGN: System V gives a stack argument eightbytes, and 16
               bytes of alignment when the type needs more than 8. clang
               passes a 16-aligned aggregate at the next multiple of 16. */
            int64_t align = agg->align > 8 ? 16 : 8;
            out[i].stack = true;
            out[i].size = agg->size;
            offset = (offset + align - 1) / align * align;
            out[i].offset = offset;
            offset += (int64_t)(agg->size + 7) / 8 * 8;
        } else if (types[i] == IR_AGG && position < a->int_arg_count &&
                   windows_by_value(agg)) {
            out[i].part_count = 1;
            out[i].parts[0].reg = a->int_args[position];
            out[i].parts[0].bytes = (uint8_t)agg->size;
        } else if (types[i] == IR_AGG && windows_by_value(agg)) {
            out[i].stack = true;
            out[i].size = agg->size;
            out[i].offset = offset;
            offset += 8;
        } else if (types[i] == IR_AGG && position < a->int_arg_count) {
            out[i].indirect = true;
            out[i].reg = a->int_args[position];
        } else if (types[i] == IR_AGG) {
            out[i].indirect = true;
            out[i].stack = true;
            out[i].offset = offset;
            offset += 8;
        } else if (a == &windows && position < a->int_arg_count) {
            out[i].reg = fp ? a->fp_args[position] : a->int_args[position];
            if (fp && i >= callee->param_count) {
                out[i].copy = a->int_args[position];
            }
        } else if (a == &sysv && fp && floats < a->fp_arg_count) {
            out[i].reg = a->fp_args[floats++];
        } else if (a == &sysv && !fp && ints < a->int_arg_count) {
            out[i].reg = a->int_args[ints++];
        } else {
            out[i].stack = true;
            out[i].offset = offset;
            offset += 8;
        }
    }
}

/* A value into the register of its class, a float constant through an
   integer register. */
static void load_value(struct selector *s, struct mach_operand reg,
                       const struct ir_operand *value)
{
    if (value->kind == IR_INT) {
        load(s, reg, value->as.integer);
    } else if (select_is_float(value->type)) {
        move_float(s, reg, select_reg(s, value));
    } else {
        move(s, reg, select_reg(s, value));
    }
}

/* An aggregate argument on the stack is copied into the argument area. An
   indirect one is copied into a slot of the caller, and its address goes
   to the stack or returns for a register. */
static struct mach_operand copy_argument(struct selector *s,
                                         const struct arg_location *loc,
                                         const struct layout *agg,
                                         struct mach_operand value)
{
    struct mach_operand address = select_new_vreg(s, 64);
    struct mach_operand place;

    if (loc->indirect) {
        slot_address(s, address, mach_slot_add(s->out, agg->size, 16));
        copy_memory(s, address, value, agg->size);
        if (loc->stack) {
            place = stack(loc->offset);
            emit2(s, X64_MOV, place, address);
        }
        return address;
    }
    emit2(s, X64_LEA, address, stack(loc->offset));
    copy_memory(s, address, value, agg->size);
    return address;
}

/* Stack arguments go to the area at the bottom of the frame, then the
   register arguments into their registers. A variadic call under System V
   sets al to the number of float registers in use. Windows passes a
   variadic float in the integer register of its position as well. */
static void emit_call(struct selector *s, const struct ir_inst *inst)
{
    const struct ir_function *callee = select_callee(s, inst);
    bool indirect = inst->b.kind == IR_FUNC;
    struct mach_operand target = indirect ? select_reg(s, &inst->a)
                                          : mach_imm(0);
    enum ir_type *types = ir_alloc(inst->arg_count, sizeof *types);
    struct arg_location *locations =
        ir_alloc(inst->arg_count, sizeof *locations);
    struct mach_operand *copies = ir_alloc(inst->arg_count, sizeof *copies);
    struct arg_location result;
    struct mach_operand result_address;
    struct mach_operand f = mach_imm(inst->a.as.index);
    struct mach_inst *call;
    uint64_t uses = 0;
    uint64_t outgoing = s->abi->shadow_space;
    size_t floats = 0;
    size_t i;

    memset(&result_address, 0, sizeof result_address);
    for (i = 0; i < inst->arg_count; i++) {
        types[i] = i < callee->param_count ? callee->params[i].type
                                           : inst->args[i].type;
    }
    locate(s, callee, types, inst->arg_count, locations);
    locate_result(s, callee, &result);
    if (callee->result == IR_AGG && result.indirect) {
        result_address = select_result_slot(s, inst, select_layout(s, callee->result_agg));
    }
    for (i = 0; i < inst->arg_count; i++) {
        const struct ir_operand *arg = &inst->args[i];
        uint8_t w = width(arg->type);
        struct mach_operand slot = stack(locations[i].offset);
        if (types[i] == IR_AGG && (locations[i].indirect ||
                                   locations[i].stack)) {
            copies[i] = copy_argument(s, &locations[i],
                                      select_layout(s, callee->params[i].agg),
                                      select_reg(s, arg));
        }
        if (types[i] == IR_AGG && locations[i].stack) {
            uint64_t end = (uint64_t)locations[i].offset +
                           (locations[i].indirect
                                ? 8
                                : (select_layout(s, callee->params[i].agg)->size + 7) / 8 * 8);
            if (end > outgoing) {
                outgoing = end;
            }
        }
        if (types[i] == IR_AGG || !locations[i].stack) {
            continue;
        }
        slot.width = w;
        if (select_is_float(arg->type)) {
            move_float(s, slot, select_reg(s, arg));
        } else {
            emit2(s, X64_MOV, slot,
                  fits_imm(arg, w) ? mach_imm(select_signed(arg->as.integer, w))
                                   : select_reg(s, arg));
        }
        if ((uint64_t)locations[i].offset + 8 > outgoing) {
            outgoing = (uint64_t)locations[i].offset + 8;
        }
    }
    if (outgoing > s->out->outgoing) {
        s->out->outgoing = outgoing;
    }
    for (i = 0; i < inst->arg_count; i++) {
        const struct ir_operand *arg = &inst->args[i];
        struct mach_operand reg = mach_preg(locations[i].reg, width(arg->type));
        if (locations[i].stack) {
            continue;
        }
        if (types[i] == IR_AGG && locations[i].indirect) {
            move(s, mach_preg(locations[i].reg, 64), copies[i]);
            uses |= BIT(locations[i].reg);
            continue;
        }
        if (types[i] == IR_AGG) {
            uses |= select_load_parts(s, &locations[i], select_reg(s, arg));
            continue;
        }
        /* DESIGN: clang callees on System V read an 8-bit or 16-bit
           register argument as 32 bits, so the caller extends it. */
        if (s->abi == &sysv && i < callee->param_count &&
            callee->params[i].ext != IR_EXT_NONE) {
            extend_into(s, mach_preg(locations[i].reg, 32), arg,
                        callee->params[i].ext == IR_EXT_SIGN);
        } else {
            load_value(s, reg, arg);
        }
        uses |= BIT(locations[i].reg);
        if (locations[i].reg >= XMM0) {
            floats++;
        }
        if (locations[i].copy >= 0) {
            emit2(s, X64_MOVQX, mach_preg((uint32_t)locations[i].copy, 64),
                  mach_preg(locations[i].reg, 64));
            uses |= BIT(locations[i].copy);
        }
    }
    if (callee->result == IR_AGG && result.indirect) {
        move(s, mach_preg(result.reg, 64), result_address);
        uses |= BIT(result.reg);
    }
    if (callee->variadic && s->abi == &sysv) {
        load(s, mach_preg(RAX, 32), floats);
        uses |= BIT(RAX);
    }
    free(types);
    free(locations);
    free(copies);
    f.kind = MACH_FUNC;
    call = emit1(s, indirect ? X64_CALLR : X64_CALL, indirect ? target : f);
    call->uses = uses;
    call->defs = s->abi->caller_saved;
    if (inst->result != IR_NO_RESULT && callee->result == IR_AGG) {
        if (!result.indirect) {
            select_store_parts(s, &result,
                               select_result_slot(s, inst,
                                                  select_layout(s, callee->result_agg)));
        }
    } else if (inst->result != IR_NO_RESULT && select_is_float(inst->type)) {
        move_float(s, select_result(s, inst),
                   mach_preg(s->abi->fp_result, width(inst->type)));
    } else if (inst->result != IR_NO_RESULT) {
        move(s, select_result(s, inst),
             mach_preg(s->abi->int_result, width(inst->type)));
    }
}

static void emit_ret(struct selector *s, const struct ir_inst *inst)
{
    struct mach_inst *ret;

    uint8_t result = select_is_float(inst->type) ? s->abi->fp_result
                                                  : s->abi->int_result;
    struct arg_location loc;
    uint64_t uses = 0;

    if (s->f->result == IR_AGG) {
        locate_result(s, s->f, &loc);
        if (loc.indirect) {
            copy_memory(s, s->result_address, select_reg(s, &inst->a),
                        select_layout(s, s->f->result_agg)->size);
            move(s, mach_preg((uint32_t)loc.copy, 64), s->result_address);
            uses = BIT(loc.copy);
        } else {
            uses = select_load_parts(s, &loc, select_reg(s, &inst->a));
        }
    } else if (inst->type != IR_VOID) {
        load_value(s, mach_preg(result, width(inst->type)), &inst->a);
        uses = BIT(result);
    }
    ret = select_emit(s, X64_RET, 0, NULL);
    ret->uses = uses;
}

static struct mach_operand memory(struct mach_operand base, enum ir_type type)
{
    struct mach_operand m = base;

    m.kind = MACH_MEM;
    m.base_vreg = base.kind == MACH_VREG;
    m.width = width(type);
    m.value = 0;
    return m;
}

static bool match_scalar(const struct selector *s, const struct ir_inst *inst)
{
    enum ir_type type = inst->op == IR_STORE ? inst->a.type : inst->type;

    (void)s;
    return type != IR_AGG;
}

/* A slot is a memory operand on rsp that frame layout fills in, and lea
   takes its address. */
static void emit_slot(struct selector *s, const struct ir_inst *inst)
{
    struct mach_operand slot = mach_imm(mach_slot_add(s->out, select_size(s, inst->of),
                                                      select_align(s, inst->of)));

    slot.kind = MACH_SLOT;
    emit2(s, X64_LEA, select_result(s, inst), slot);
}

/* The memory operand of a load or a store: the folded address that
   selection found, or the pointer register itself. */
static struct mach_operand address_of(struct selector *s,
                                      const struct ir_operand *pointer,
                                      enum ir_type type)
{
    struct mach_operand m;

    if (!s->has_address) {
        return memory(select_reg(s, pointer), type);
    }
    m = memory(select_reg(s, s->address.base), type);
    m.value = s->address.offset;
    if (s->address.index != NULL) {
        m.index_reg = s->address.index->as.temp;
        m.index_vreg = true;
        m.scale = (uint8_t)(1 << s->address.shift);
    }
    return m;
}

/* A displacement holds 32 bits and a scale is 1, 2, 4 or 8. A store with
   an index takes only an immediate value, so that no instruction reads
   more than two registers. */
static bool fits_address(const struct selector *s, const struct address *a,
                         const struct ir_inst *use)
{
    (void)s;
    if (a->offset < INT32_MIN || a->offset > INT32_MAX) {
        return false;
    }
    if (a->index != NULL && use->op == IR_STORE) {
        return fits_imm(&use->a, width(use->a.type));
    }
    return true;
}

static void emit_load(struct selector *s, const struct ir_inst *inst)
{
    emit2(s, select_is_float(inst->type) ? X64_MOVS : X64_MOV,
          select_result(s, inst), address_of(s, &inst->a, inst->type));
}

/* A constant that fits the immediate is stored without a register. */
static void emit_store(struct selector *s, const struct ir_inst *inst)
{
    uint8_t w = width(inst->a.type);
    bool fp = select_is_float(inst->a.type);
    struct mach_operand value =
        fits_imm(&inst->a, w) ? mach_imm(select_signed(inst->a.as.integer, w))
                              : select_reg(s, &inst->a);

    emit2(s, fp ? X64_MOVS : X64_MOV, address_of(s, &inst->b, inst->a.type),
          value);
}

/* A parameter on the stack sits above the saved rbp and the return
   address, at the offset that locate gives it in the argument area. */
static void stack_param(struct selector *s, int64_t offset,
                        struct mach_operand dst)
{
    struct mach_operand m = mach_preg(RBP, 64);

    m.kind = MACH_MEM;
    m.width = dst.width;
    m.value = 16 + offset;
    s->out->stack_params = true;
    emit2(s, s->out->fp[dst.reg] ? X64_MOVS : X64_MOV, dst, m);
}

/* A call of a C library function by name with integer arguments. */
/* The call of the C function name, whose arguments the caller has moved
   into the registers of uses. */
static void call_named(struct selector *s, const char *name, uint64_t uses)
{
    struct mach_operand f;
    struct mach_inst *call;

    if (s->abi->shadow_space > s->out->outgoing) {
        s->out->outgoing = s->abi->shadow_space;
    }
    memset(&f, 0, sizeof f);
    f.kind = MACH_NAME;
    f.name = name;
    call = emit1(s, X64_CALL, f);
    call->uses = uses;
    call->defs = s->abi->caller_saved;
}

static void call_c(struct selector *s, const char *name,
                   const struct mach_operand *args, size_t count)
{
    uint64_t uses = 0;
    size_t i;

    for (i = 0; i < count; i++) {
        move(s, mach_preg(s->abi->int_args[i], 64), args[i]);
        uses |= BIT(s->abi->int_args[i]);
    }
    call_named(s, name, uses);
}

/* DESIGN: an f16 is its bits in the low sixteen of an integer register.
   x86-64-v3 has F16C, which converts the low lanes of an xmm register,
   so the bits cross with movd. v1 and v2 have no instruction for it and
   call the runtime. Its routines take the half in a 32-bit integer
   register and the f32 in the first float register. */
static void emit_half_convert(struct selector *s, const struct ir_inst *inst)
{
    bool widen = inst->op == IR_HEXT;
    struct mach_operand r = select_result(s, inst);
    struct mach_operand a = select_reg(s, &inst->a);
    struct mach_operand ops[3];
    uint8_t in = widen ? s->abi->int_args[0] : s->abi->fp_args[0];

    if (cpu_has(s->cpu, CPU_F16C)) {
        struct mach_operand lanes = select_new_fp_vreg(s, 32);
        if (widen) {
            emit2(s, X64_MOVQX, lanes, mach_widened(a, 32));
            emit2(s, X64_CVTPH2PS, r, lanes);
        } else {
            ops[0] = lanes;
            ops[1] = a;
            ops[2] = mach_imm(4);
            select_emit(s, (uint16_t)X64_CVTPS2PH, 3, ops);
            emit2(s, X64_MOVQX, mach_widened(r, 32), lanes);
        }
        return;
    }
    if (widen) {
        move(s, mach_preg(in, 32), mach_widened(a, 32));
    } else {
        move_float(s, mach_preg(in, 32), a);
    }
    call_named(s, widen ? "anti_rt_f16_to_f32" : "anti_rt_f32_to_f16",
               BIT(in));
    if (widen) {
        move_float(s, r, mach_preg(s->abi->fp_result, 32));
    } else {
        move(s, r, mach_preg(s->abi->int_result, 16));
    }
}

/* DESIGN: a copy of up to 64 bytes moves 8, 4, 2 and 1 bytes at a time
   through an integer register. A larger copy calls memcpy. */
static void copy_memory(struct selector *s, struct mach_operand dst,
                        struct mach_operand src, uint64_t size)
{
    uint64_t offset = 0;

    if (size > 64) {
        struct mach_operand args[3];
        args[0] = dst;
        args[1] = src;
        args[2] = select_new_vreg(s, 64);
        load(s, args[2], size);
        call_c(s, "memcpy", args, 3);
        return;
    }
    while (offset < size) {
        uint64_t left = size - offset;
        uint8_t bits = left >= 8 ? 64 : left >= 4 ? 32 : left >= 2 ? 16 : 8;
        struct mach_operand t = select_new_vreg(s, bits);
        emit2(s, X64_MOV, t, mach_mem(src, (int64_t)offset, bits));
        emit2(s, X64_MOV, mach_mem(dst, (int64_t)offset, bits), t);
        offset += bits / 8;
    }
}

static bool is_float_register(const struct selector *s,
                              struct mach_operand o)
{
    return o.kind == MACH_VREG ? s->out->fp[o.reg] : o.reg >= XMM0;
}

/* Load bytes of an aggregate at offset after base into register dst. An
   integer part of 3, 5, 6 or 7 bytes combines its pieces with shifts. */
static void load_bytes(struct selector *s, struct mach_operand dst,
                       struct mach_operand base, int64_t offset,
                       unsigned bytes)
{
    unsigned low = bytes >= 4 ? 4 : 2;
    struct mach_operand rest;

    if (is_float_register(s, dst) && bytes == 16) {
        emit2(s, X64_MOVUPS, mach_widened(dst, 128),
              mach_mem(base, offset, 128));
    } else if (is_float_register(s, dst)) {
        move_float(s, mach_widened(dst, (uint8_t)(bytes * 8)),
                   mach_mem(base, offset, (uint8_t)(bytes * 8)));
    } else if (bytes == 8 || bytes == 4) {
        emit2(s, X64_MOV, mach_widened(dst, (uint8_t)(bytes * 8)),
              mach_mem(base, offset, (uint8_t)(bytes * 8)));
    } else if (bytes == 2 || bytes == 1) {
        emit2(s, X64_MOVZX, mach_widened(dst, 32),
              mach_mem(base, offset, (uint8_t)(bytes * 8)));
    } else {
        load_bytes(s, dst, base, offset, low);
        rest = select_new_vreg(s, 64);
        load_bytes(s, rest, base, offset + low, bytes - low);
        emit2(s, X64_SHL, rest, mach_imm(8 * low));
        emit2(s, X64_OR, mach_widened(dst, 64), rest);
    }
}

/* Store the low bytes of register src at offset after base. */
static void store_bytes(struct selector *s, struct mach_operand src,
                        struct mach_operand base, int64_t offset,
                        unsigned bytes)
{
    unsigned low = bytes >= 4 ? 4 : 2;
    struct mach_operand rest;

    if (is_float_register(s, src) && bytes == 16) {
        emit2(s, X64_MOVUPS, mach_mem(base, offset, 128),
              mach_widened(src, 128));
    } else if (is_float_register(s, src)) {
        move_float(s, mach_mem(base, offset, (uint8_t)(bytes * 8)),
                   mach_widened(src, (uint8_t)(bytes * 8)));
    } else if (bytes == 8 || bytes == 4 || bytes == 2 || bytes == 1) {
        emit2(s, X64_MOV, mach_mem(base, offset, (uint8_t)(bytes * 8)),
              mach_widened(src, (uint8_t)(bytes * 8)));
    } else {
        store_bytes(s, src, base, offset, low);
        rest = select_new_vreg(s, 64);
        move(s, rest, mach_widened(src, 64));
        emit2(s, X64_SHR, rest, mach_imm(8 * low));
        store_bytes(s, rest, base, offset + low, bytes - low);
    }
}

static void slot_address(struct selector *s, struct mach_operand dst,
                         uint32_t slot)
{
    struct mach_operand o = mach_imm(slot);

    o.kind = MACH_SLOT;
    emit2(s, X64_LEA, mach_widened(dst, 64), o);
}

/* The address of an aggregate in the argument area of the caller. */
static void incoming_address(struct selector *s, struct mach_operand dst,
                             int64_t offset)
{
    s->out->stack_params = true;
    emit2(s, X64_LEA, mach_widened(dst, 64),
          mach_mem(mach_preg(RBP, 64), 16 + offset, 64));
}

static void emit_memcopy(struct selector *s, const struct ir_inst *inst)
{
    struct mach_operand to = select_reg(s, &inst->a);
    struct mach_operand from = select_reg(s, &inst->b);

    copy_memory(s, to, from, select_size(s, inst->of));
}

static void emit_ptradd(struct selector *s, const struct ir_inst *inst)
{
    two_operand(s, inst, X64_ADD, true);
}

/* Simd structs */

/* DESIGN: a simd operation works on its values in memory, one register
   at a time. A register holds 16 bytes, 32 at x86-64-v3, whose AVX2
   gives the wide forms, and 8 for a simd struct of 8 bytes. A wide value
   spans more than one register. Each one takes the instruction of the
   operation, so an f32x8 is one vaddps at v3 and two addps below it. The
   registers are the two the spills use and the four below them, which
   the instructions name. No value of the allocator lives in them across
   the operation. A comparison narrows its lanes of all ones to one byte
   of 0 or 1 per lane of the mask, and a choice widens the mask back. */

/* A vector register operand, with the 256-bit form in its value. */
static struct mach_operand vector_of(uint8_t reg, bool wide)
{
    struct mach_operand o = mach_preg(reg, 128);

    o.value = wide;
    return o;
}

/* The width in bits of a lane of type type. */
static unsigned lane_width(enum ir_type type)
{
    switch (type) {
    case IR_I8: return 8;
    case IR_I16: return 16;
    case IR_I32:
    case IR_F32: return 32;
    default: return 64;
    }
}

/* The lanes of a simd operation on the target and the registers of one
   value. */
struct shape {
    enum ir_type lane;
    unsigned lane_bytes;
    unsigned chunk;                 /* bytes of one register */
    unsigned chunks;                /* registers of one value */
    unsigned lanes;                 /* lanes of one register */
    bool wide;                      /* the registers are 256 bits */
};

static struct shape shape_of(const struct ir_aggtype *agg, uint64_t size,
                             enum cpu_level cpu)
{
    unsigned width = cpu_has(cpu, CPU_AVX) ? 32 : 16;
    struct shape sh;

    sh.lane = agg->fields[0].type.type;
    sh.lane_bytes = (unsigned)lane_width(sh.lane) / 8;
    sh.chunk = size < width ? (unsigned)size : width;
    sh.chunks = (unsigned)(size / sh.chunk);
    sh.lanes = sh.chunk / sh.lane_bytes;
    sh.wide = sh.chunk == 32;
    return sh;
}

static struct shape inst_shape(const struct selector *s,
                               const struct ir_inst *inst)
{
    return shape_of(s->m->aggs[inst->of.agg],
                    select_layout(s, inst->of.agg)->size, s->cpu);
}

/* The registers of the vector instructions: the two of the spills and
   the four below them, which every convention leaves to the caller. */
static const uint8_t sysv_vector_registers[] = {
    XMM15, XMM14, XMM13, XMM12, XMM11, XMM10
};
static const uint8_t windows_vector_registers[] = {
    XMM5, XMM4, XMM3, XMM2, XMM1, XMM0
};

static uint8_t vector_register(const struct selector *s, unsigned n)
{
    return s->abi == &windows ? windows_vector_registers[n]
                              : sysv_vector_registers[n];
}

static void emit_vec(struct selector *s, enum x64_op op, size_t count,
                     const struct mach_operand *operands)
{
    select_emit(s, (uint16_t)op, count, operands);
}

static void vec2(struct selector *s, enum x64_op op, struct mach_operand a,
                 struct mach_operand b)
{
    struct mach_operand ops[2];

    ops[0] = a;
    ops[1] = b;
    emit_vec(s, op, 2, ops);
}

static void vec3(struct selector *s, enum x64_op op, struct mach_operand a,
                 struct mach_operand b, struct mach_operand c)
{
    struct mach_operand ops[3];

    ops[0] = a;
    ops[1] = b;
    ops[2] = c;
    emit_vec(s, op, 3, ops);
}

/* Load and store the bytes of one register. A part of 1 or 2 bytes of a
   mask goes through a general register. */
static void vector_load(struct selector *s, uint8_t reg,
                        struct mach_operand base, int64_t offset,
                        unsigned bytes)
{
    struct mach_operand at = mach_mem(base, offset, (uint8_t)(bytes * 8));

    if (bytes < 4) {
        struct mach_operand r = select_new_vreg(s, 32);
        emit2(s, X64_MOVZX, r, at);
        vec2(s, X64_MOVQX, mach_preg(reg, 128), r);
        return;
    }
    if (bytes == 4 || bytes == 8) {
        vec2(s, X64_MOVQX, mach_preg(reg, (uint8_t)(bytes * 8)), at);
        return;
    }
    vec2(s, X64_MOVUPS, vector_of(reg, bytes == 32), at);
}

static void vector_store(struct selector *s, uint8_t reg,
                         struct mach_operand base, int64_t offset,
                         unsigned bytes)
{
    struct mach_operand at = mach_mem(base, offset, (uint8_t)(bytes * 8));

    if (bytes < 4) {
        struct mach_operand r = select_new_vreg(s, 32);
        vec2(s, X64_MOVQX, r, mach_preg(reg, 128));
        emit2(s, X64_MOV, mach_mem(base, offset, (uint8_t)(bytes * 8)),
              mach_widened(r, (uint8_t)(bytes * 8)));
        return;
    }
    if (bytes == 4 || bytes == 8) {
        vec2(s, X64_MOVQX, at, mach_preg(reg, (uint8_t)(bytes * 8)));
        return;
    }
    vec2(s, X64_MOVUPS, at, vector_of(reg, bytes == 32));
}

/* The instruction of the lane operation op on lanes of lane_bytes. */
static enum x64_op lane_opcode(enum ir_op op, unsigned lane_bytes,
                               bool is_float)
{
    if (is_float) {
        bool wide = lane_bytes == 8;
        switch (op) {
        case IR_FADD: return wide ? X64_ADDPD : X64_ADDPS;
        case IR_FSUB: return wide ? X64_SUBPD : X64_SUBPS;
        case IR_FMUL: return wide ? X64_MULPD : X64_MULPS;
        default: return wide ? X64_DIVPD : X64_DIVPS;
        }
    }
    switch (op) {
    case IR_ADD:
        return lane_bytes == 1   ? X64_PADDB
               : lane_bytes == 2 ? X64_PADDW
               : lane_bytes == 4 ? X64_PADDD
                                 : X64_PADDQ;
    case IR_SUB:
        return lane_bytes == 1   ? X64_PSUBB
               : lane_bytes == 2 ? X64_PSUBW
               : lane_bytes == 4 ? X64_PSUBD
                                 : X64_PSUBQ;
    case IR_MUL:
        return lane_bytes == 2 ? X64_PMULLW : X64_PMULLD;
    case IR_AND: return X64_PAND;
    case IR_OR: return X64_POR;
    default: return X64_PXOR;
    }
}

/* All ones in reg, which pcmpeqd of a register with itself gives. */
static void all_ones(struct selector *s, uint8_t reg, bool wide)
{
    vec2(s, X64_PCMPEQD, vector_of(reg, wide), vector_of(reg, wide));
}

/* The lanes of a op b, all ones where the comparison holds. a and b may
   swap, so both registers hold a copy the caller may not keep. */
static void compare_lanes(struct selector *s, enum ir_op op, uint8_t a,
                          uint8_t b, uint8_t temp, const struct shape *sh)
{
    bool is_float = select_is_float(sh->lane);
    unsigned n = sh->lane_bytes;
    /* pcmpgt gives a > b, so a < b swaps the operands. a >= b is the
       negation of b > a and a <= b the negation of a > b. */
    bool swap = op == IR_FGT || op == IR_FGE || op == IR_SLT || op == IR_SGE;
    bool invert = op == IR_NE || op == IR_SGE || op == IR_SLE;
    enum x64_op code;
    int64_t predicate = 0;

    if (is_float) {
        predicate = op == IR_FEQ                    ? 0
                    : op == IR_FLT || op == IR_FGT  ? 1
                    : op == IR_FLE || op == IR_FGE  ? 2
                                                    : 4;
        vec3(s, n == 8 ? X64_CMPPD : X64_CMPPS, vector_of(swap ? b : a, sh->wide),
             vector_of(swap ? a : b, sh->wide), mach_imm(predicate));
        if (swap) {
            vec2(s, X64_MOVUPS, vector_of(a, sh->wide), vector_of(b, sh->wide));
        }
        return;
    }
    if (op == IR_EQ || op == IR_NE) {
        code = n == 1   ? X64_PCMPEQB
               : n == 2 ? X64_PCMPEQW
               : n == 4 ? X64_PCMPEQD
                        : X64_PCMPEQQ;
    } else {
        code = n == 1   ? X64_PCMPGTB
               : n == 2 ? X64_PCMPGTW
               : n == 4 ? X64_PCMPGTD
                        : X64_PCMPGTQ;
    }
    vec2(s, code, vector_of(swap ? b : a, sh->wide),
         vector_of(swap ? a : b, sh->wide));
    if (swap) {
        vec2(s, X64_MOVUPS, vector_of(a, sh->wide), vector_of(b, sh->wide));
    }
    if (invert) {
        all_ones(s, temp, sh->wide);
        vec2(s, X64_PXOR, vector_of(a, sh->wide), vector_of(temp, sh->wide));
    }
}

/* Narrow the lanes of all ones or zero in a to one byte of 1 or 0 each.
   The upper half of a wide register comes down first, since the packs
   work inside each half. Returns the register with the bytes. */
static uint8_t narrow_mask(struct selector *s, uint8_t a, uint8_t temp,
                           const struct shape *sh)
{
    unsigned n = sh->lane_bytes;
    bool bytes = sh->wide && n == 1;

    if (sh->wide && n > 1) {
        vec3(s, X64_VEXTRACTF128, mach_preg(temp, 128), vector_of(a, true),
             mach_imm(1));
    }
    if (n == 8 && sh->wide) {
        vec3(s, X64_SHUFPS, mach_preg(a, 128), mach_preg(temp, 128),
             mach_imm(0x88));
    } else if (n == 8) {
        vec3(s, X64_PSHUFD, mach_preg(a, 128), mach_preg(a, 128),
             mach_imm(0x08));
    }
    if (n >= 4) {
        vec2(s, X64_PACKSSDW, mach_preg(a, 128),
             mach_preg(sh->wide && n == 4 ? temp : a, 128));
    }
    if (n >= 2) {
        vec2(s, X64_PACKSSWB, mach_preg(a, 128),
             mach_preg(sh->wide && n == 2 ? temp : a, 128));
    }
    /* The bytes of all ones become 1, which zero minus them gives. */
    vec2(s, X64_PXOR, vector_of(temp, bytes), vector_of(temp, bytes));
    vec2(s, X64_PSUBB, vector_of(temp, bytes), vector_of(a, bytes));
    return temp;
}

/* Widen the bytes of 1 or 0 of a mask in reg to lanes of all ones or
   zero, with temp for the zero it subtracts from. */
static void widen_mask(struct selector *s, uint8_t reg, uint8_t temp,
                       const struct shape *sh)
{
    unsigned n = sh->lane_bytes;
    bool bytes = sh->wide && n == 1;

    vec2(s, X64_PXOR, vector_of(temp, bytes), vector_of(temp, bytes));
    vec2(s, X64_PSUBB, vector_of(temp, bytes), vector_of(reg, bytes));
    vec2(s, X64_MOVUPS, vector_of(reg, bytes), vector_of(temp, bytes));
    if (sh->wide) {
        if (n > 1) {
            vec2(s, n == 2   ? X64_PMOVSXBW
                     : n == 4 ? X64_PMOVSXBD
                              : X64_PMOVSXBQ,
                 vector_of(reg, true), mach_preg(reg, 128));
        }
        return;
    }
    if (n >= 2) {
        vec2(s, X64_PUNPCKLBW, mach_preg(reg, 128), mach_preg(reg, 128));
    }
    if (n >= 4) {
        vec2(s, X64_PUNPCKLWD, mach_preg(reg, 128), mach_preg(reg, 128));
    }
    if (n == 8) {
        vec2(s, X64_PUNPCKLDQ, mach_preg(reg, 128), mach_preg(reg, 128));
    }
}

static void emit_vbinary(struct selector *s, const struct ir_inst *inst)
{
    struct shape sh = inst_shape(s, inst);
    enum ir_op op = (enum ir_op)inst->field;
    bool compare = (op >= IR_EQ && op <= IR_UGE) ||
                   (op >= IR_FEQ && op <= IR_FGE);
    uint8_t a = vector_register(s, 0);
    uint8_t b = vector_register(s, 1);
    uint8_t temp = vector_register(s, 2);
    struct mach_operand dst = select_reg(s, &inst->a);
    struct mach_operand x = select_reg(s, &inst->b);
    struct mach_operand y = select_reg(s, &inst->c);
    unsigned k;

    for (k = 0; k < sh.chunks; k++) {
        vector_load(s, a, x, (int64_t)k * sh.chunk, sh.chunk);
        vector_load(s, b, y, (int64_t)k * sh.chunk, sh.chunk);
        if (compare) {
            uint8_t mask;
            compare_lanes(s, op, a, b, temp, &sh);
            mask = narrow_mask(s, a, temp, &sh);
            vector_store(s, mask, dst, (int64_t)k * sh.lanes, sh.lanes);
            continue;
        }
        vec2(s, lane_opcode(op, sh.lane_bytes, select_is_float(sh.lane)),
             vector_of(a, sh.wide), vector_of(b, sh.wide));
        vector_store(s, a, dst, (int64_t)k * sh.chunk, sh.chunk);
    }
}

static void emit_vunary(struct selector *s, const struct ir_inst *inst)
{
    struct shape sh = inst_shape(s, inst);
    enum ir_op op = (enum ir_op)inst->field;
    uint8_t a = vector_register(s, 0);
    uint8_t temp = vector_register(s, 1);
    struct mach_operand dst = select_reg(s, &inst->a);
    struct mach_operand x = select_reg(s, &inst->b);
    unsigned k;

    for (k = 0; k < sh.chunks; k++) {
        vector_load(s, a, x, (int64_t)k * sh.chunk, sh.chunk);
        if (op == IR_FNEG) {
            /* The sign bit of every lane, which the shift of all ones
               into place gives. */
            all_ones(s, temp, sh.wide);
            vec2(s, sh.lane_bytes == 8 ? X64_PSLLQ : X64_PSLLD,
                 vector_of(temp, sh.wide),
                 mach_imm(sh.lane_bytes == 8 ? 63 : 31));
            vec2(s, sh.lane_bytes == 8 ? X64_XORPD : X64_XORPS,
                 vector_of(a, sh.wide), vector_of(temp, sh.wide));
        } else if (op == IR_NEG) {
            vec2(s, X64_PXOR, vector_of(temp, sh.wide),
                 vector_of(temp, sh.wide));
            vec2(s, lane_opcode(IR_SUB, sh.lane_bytes, false),
                 vector_of(temp, sh.wide), vector_of(a, sh.wide));
            vec2(s, X64_MOVUPS, vector_of(a, sh.wide),
                 vector_of(temp, sh.wide));
        } else {
            all_ones(s, temp, sh.wide);
            vec2(s, X64_PXOR, vector_of(a, sh.wide), vector_of(temp, sh.wide));
        }
        vector_store(s, a, dst, (int64_t)k * sh.chunk, sh.chunk);
    }
}

static void emit_vsplat(struct selector *s, const struct ir_inst *inst)
{
    struct shape sh = inst_shape(s, inst);
    uint8_t a = vector_register(s, 0);
    struct mach_operand value = select_reg(s, &inst->b);
    struct mach_operand dst = select_reg(s, &inst->a);
    unsigned n = sh.lane_bytes;
    unsigned k;

    if (select_is_float(sh.lane)) {
        vec2(s, X64_MOVUPS, mach_preg(a, 128), mach_widened(value, 128));
    } else {
        vec2(s, X64_MOVQX, mach_preg(a, (uint8_t)(n == 8 ? 64 : 32)),
             mach_widened(value, (uint8_t)(n == 8 ? 64 : 32)));
    }
    if (sh.wide) {
        vec2(s, select_is_float(sh.lane) ? (n == 8 ? X64_VBROADCASTSD
                                                   : X64_VBROADCASTSS)
             : n == 1   ? X64_VPBROADCASTB
             : n == 2   ? X64_VPBROADCASTW
             : n == 4   ? X64_VPBROADCASTD
                        : X64_VPBROADCASTQ,
             vector_of(a, true), mach_preg(a, 128));
    } else if (select_is_float(sh.lane) && n == 8) {
        vec2(s, X64_UNPCKLPD, mach_preg(a, 128), mach_preg(a, 128));
    } else if (select_is_float(sh.lane)) {
        vec3(s, X64_SHUFPS, mach_preg(a, 128), mach_preg(a, 128),
             mach_imm(0));
    } else {
        if (n == 1) {
            vec2(s, X64_PUNPCKLBW, mach_preg(a, 128), mach_preg(a, 128));
        }
        if (n <= 2) {
            vec2(s, X64_PUNPCKLWD, mach_preg(a, 128), mach_preg(a, 128));
        }
        if (n <= 4) {
            vec3(s, X64_PSHUFD, mach_preg(a, 128), mach_preg(a, 128),
                 mach_imm(0));
        } else {
            vec2(s, X64_PUNPCKLQDQ, mach_preg(a, 128), mach_preg(a, 128));
        }
    }
    for (k = 0; k < sh.chunks; k++) {
        vector_store(s, a, dst, (int64_t)k * sh.chunk, sh.chunk);
    }
}

static void emit_vselect(struct selector *s, const struct ir_inst *inst)
{
    struct shape sh = inst_shape(s, inst);
    uint8_t a = vector_register(s, 0);
    uint8_t b = vector_register(s, 1);
    uint8_t m = vector_register(s, 2);
    uint8_t temp = vector_register(s, 3);
    struct mach_operand dst = select_reg(s, &inst->a);
    struct mach_operand mask = select_reg(s, &inst->b);
    struct mach_operand x = select_reg(s, &inst->c);
    struct mach_operand y = select_reg(s, &inst->args[0]);
    unsigned k;

    for (k = 0; k < sh.chunks; k++) {
        vector_load(s, m, mask, (int64_t)k * sh.lanes, sh.lanes);
        widen_mask(s, m, temp, &sh);
        vector_load(s, a, x, (int64_t)k * sh.chunk, sh.chunk);
        vector_load(s, b, y, (int64_t)k * sh.chunk, sh.chunk);
        vec2(s, X64_PAND, vector_of(a, sh.wide), vector_of(m, sh.wide));
        vec2(s, X64_PANDN, vector_of(m, sh.wide), vector_of(b, sh.wide));
        vec2(s, X64_POR, vector_of(a, sh.wide), vector_of(m, sh.wide));
        vector_store(s, a, dst, (int64_t)k * sh.chunk, sh.chunk);
    }
}

/* A shuffle of one register of lanes of 4 or 8 bytes is one pshufd,
   whose immediate names the source of each 32-bit part. */
static void emit_vshuffle(struct selector *s, const struct ir_inst *inst)
{
    struct shape sh = inst_shape(s, inst);
    uint8_t a = vector_register(s, 0);
    struct mach_operand dst = select_reg(s, &inst->a);
    struct mach_operand x = select_reg(s, &inst->b);
    int64_t control = 0;
    unsigned i;

    for (i = 0; i < sh.lanes; i++) {
        unsigned from = (unsigned)inst->args[i].as.integer;
        if (sh.lane_bytes == 8) {
            control |= (int64_t)(2 * from) << (4 * i);
            control |= (int64_t)(2 * from + 1) << (4 * i + 2);
        } else {
            control |= (int64_t)from << (2 * i);
        }
    }
    vector_load(s, a, x, 0, sh.chunk);
    vec3(s, X64_PSHUFD, mach_preg(a, 128), mach_preg(a, 128),
         mach_imm(control));
    vector_store(s, a, dst, 0, sh.chunk);
}

/* lo becomes lo op hi, lane by lane. The least keeps hi where it is less
   than lo, which minps gives with hi as its destination. */
static void fold_lanes(struct selector *s, enum ir_op op, uint8_t lo,
                       uint8_t hi, const struct shape *sh)
{
    unsigned n = sh->lane_bytes;

    switch (op) {
    case IR_FLT:
    case IR_FGT:
        vec2(s, op == IR_FLT ? (n == 8 ? X64_MINPD : X64_MINPS)
                             : (n == 8 ? X64_MAXPD : X64_MAXPS),
             vector_of(hi, sh->wide), vector_of(lo, sh->wide));
        vec2(s, X64_MOVUPS, vector_of(lo, sh->wide), vector_of(hi, sh->wide));
        return;
    case IR_SLT:
    case IR_ULT:
    case IR_SGT:
    case IR_UGT:
        vec2(s,
             op == IR_SLT   ? (n == 1 ? X64_PMINSB : n == 2 ? X64_PMINSW
                                                            : X64_PMINSD)
             : op == IR_ULT ? (n == 1 ? X64_PMINUB : n == 2 ? X64_PMINUW
                                                            : X64_PMINUD)
             : op == IR_SGT ? (n == 1 ? X64_PMAXSB : n == 2 ? X64_PMAXSW
                                                            : X64_PMAXSD)
                            : (n == 1 ? X64_PMAXUB : n == 2 ? X64_PMAXUW
                                                            : X64_PMAXUD),
             vector_of(lo, sh->wide), vector_of(hi, sh->wide));
        return;
    default:
        vec2(s, lane_opcode(op, n, select_is_float(sh->lane)),
             vector_of(lo, sh->wide), vector_of(hi, sh->wide));
        return;
    }
}

static void fold_chunks(struct selector *s, enum ir_op op,
                        struct mach_operand x, const struct shape *sh,
                        unsigned first, unsigned stride, unsigned count,
                        unsigned level)
{
    if (count == 1) {
        vector_load(s, vector_register(s, level), x,
                    (int64_t)first * sh->chunk, sh->chunk);
        return;
    }
    fold_chunks(s, op, x, sh, first, stride * 2, count / 2, level);
    fold_chunks(s, op, x, sh, first + stride, stride * 2, count / 2,
                level + 1);
    fold_lanes(s, op, vector_register(s, level), vector_register(s, level + 1),
               sh);
}

/* A fold of the lanes: the registers fold first, then the halves of the
   one left. The upper half of a wide register comes down whole, and a
   byte shift moves every other half. */
static void emit_vreduce(struct selector *s, const struct ir_inst *inst)
{
    struct shape sh = inst_shape(s, inst);
    enum ir_op op = (enum ir_op)inst->field;
    uint8_t a = vector_register(s, 0);
    uint8_t b = vector_register(s, 1);
    struct mach_operand x = select_reg(s, &inst->a);
    struct mach_operand result;
    struct shape half = sh;
    unsigned bytes;

    fold_chunks(s, op, x, &sh, 0, 1, sh.chunks, 0);
    for (bytes = sh.chunk; bytes > sh.lane_bytes; bytes /= 2) {
        half.wide = bytes == 32;
        if (bytes == 32) {
            vec3(s, X64_VEXTRACTF128, mach_preg(b, 128),
                 vector_of(a, true), mach_imm(1));
            half.wide = false;
        } else {
            vec2(s, X64_MOVUPS, mach_preg(b, 128), mach_preg(a, 128));
            vec2(s, X64_PSRLDQ, mach_preg(b, 128), mach_imm(bytes / 2));
        }
        fold_lanes(s, op, a, b, &half);
    }
    result = select_result(s, inst);
    if (select_is_float(sh.lane)) {
        vec2(s, X64_MOVS, result, mach_preg(a, result.width));
    } else {
        vec2(s, X64_MOVQX,
             mach_widened(result, (uint8_t)(sh.lane_bytes == 8 ? 64 : 32)),
             mach_preg(a, (uint8_t)(sh.lane_bytes == 8 ? 64 : 32)));
    }
}

/* DESIGN: a simd operation is native where the level has the
   instruction of its lanes. SSE2, which every level has, holds the float
   operations and the integer ones of 8, 16 and 32-bit lanes. SSE4.1 of
   x86-64-v2 adds the product of 32-bit lanes, the comparisons of
   64-bit lanes and the rest of the least and greatest. An unsigned
   comparison, a product of 8 or 64-bit lanes and the least or greatest
   of 64-bit lanes have no instruction. */
static bool vector_native(const struct ir_inst *inst,
                          const struct ir_aggtype *agg, uint64_t size,
                          enum cpu_level cpu)
{
    struct shape sh = shape_of(agg, size, cpu);
    enum ir_op op = (enum ir_op)inst->field;
    bool sse4 = cpu_has(cpu, CPU_SSE4);
    unsigned n = sh.lane_bytes;

    if (sh.lanes < 2) {
        return false;
    }
    switch (inst->op) {
    case IR_VBINARY:
        if (op == IR_MUL) {
            return n == 2 || (n == 4 && sse4);
        }
        if (op >= IR_ULT && op <= IR_UGE) {
            return false;
        }
        if (op >= IR_EQ && op <= IR_SGE) {
            return n < 8 || sse4;
        }
        return true;
    case IR_VSHUFFLE:
        /* pshufd moves the 32-bit parts of one register of 16 bytes, and
           inside each half of a wide one, so a wide shuffle has none. */
        return sh.chunks == 1 && n >= 4 && !sh.wide;
    case IR_VREDUCE:
        if (op == IR_SLT || op == IR_ULT || op == IR_SGT || op == IR_UGT) {
            if (n == 8) {
                return false;
            }
            if (n == 4) {
                return sse4;
            }
            return sse4 || (n == 1 && (op == IR_ULT || op == IR_UGT)) ||
                   (n == 2 && (op == IR_SLT || op == IR_SGT));
        }
        return true;
    default:
        return true;
    }
}

static const struct pattern patterns[] = {
    {IR_COPY, match_arith, emit_copy},
    {IR_COPY, match_float, emit_float_copy},
    {IR_FADD, NULL, emit_float_binary},
    {IR_FSUB, NULL, emit_float_binary},
    {IR_FMUL, NULL, emit_float_binary},
    {IR_FDIV, NULL, emit_float_binary},
    {IR_FNEG, NULL, emit_float_neg},
    {IR_SITOF, NULL, emit_float_convert},
    {IR_UITOF, NULL, emit_float_convert},
    {IR_FTOSI, NULL, emit_float_convert},
    {IR_FTOUI, NULL, emit_float_convert},
    {IR_FEXT, NULL, emit_float_convert},
    {IR_FTRUNC, NULL, emit_float_convert},
    {IR_HEXT, NULL, emit_half_convert},
    {IR_HTRUNC, NULL, emit_half_convert},
    {IR_FEQ, NULL, emit_float_compare},
    {IR_FNE, NULL, emit_float_compare},
    {IR_FLT, NULL, emit_float_compare},
    {IR_FLE, NULL, emit_float_compare},
    {IR_FGT, NULL, emit_float_compare},
    {IR_FGE, NULL, emit_float_compare},
    {IR_ADD, match_arith, emit_binary},
    {IR_SUB, match_arith, emit_binary},
    {IR_MUL, match_arith, emit_mul},
    {IR_AND, match_arith, emit_binary},
    {IR_OR, match_arith, emit_binary},
    {IR_XOR, match_not_bool, emit_binary},
    {IR_XOR, match_arith, emit_binary},
    {IR_NEG, match_arith, emit_unary},
    {IR_NOT, match_arith, emit_unary},
    {IR_EQ, match_compare, emit_set},
    {IR_NE, match_compare, emit_set},
    {IR_SLT, match_compare, emit_set},
    {IR_SLE, match_compare, emit_set},
    {IR_SGT, match_compare, emit_set},
    {IR_SGE, match_compare, emit_set},
    {IR_ULT, match_compare, emit_set},
    {IR_ULE, match_compare, emit_set},
    {IR_UGT, match_compare, emit_set},
    {IR_UGE, match_compare, emit_set},
    {IR_JUMP, NULL, emit_jump},
    {IR_BRANCH, match_fused, emit_fused_branch},
    {IR_BRANCH, NULL, emit_branch},
    {IR_VBINARY, NULL, emit_vbinary},
    {IR_VUNARY, NULL, emit_vunary},
    {IR_VSPLAT, NULL, emit_vsplat},
    {IR_VSELECT, NULL, emit_vselect},
    {IR_VSHUFFLE, NULL, emit_vshuffle},
    {IR_VREDUCE, NULL, emit_vreduce},
    {IR_CALL, match_call, emit_call},
    {IR_RET, NULL, emit_ret},
    {IR_SLOT, NULL, emit_slot},
    {IR_LOAD, match_scalar, emit_load},
    {IR_STORE, match_scalar, emit_store},
    {IR_PTRADD, NULL, emit_ptradd},
    {IR_MEMCOPY, NULL, emit_memcopy},
    {IR_SDIV, match_arith, emit_div},
    {IR_UDIV, match_arith, emit_div},
    {IR_SREM, match_arith, emit_div},
    {IR_UREM, match_arith, emit_div},
    {IR_SHL, match_arith, emit_shift},
    {IR_SHR_S, match_arith, emit_shift},
    {IR_SHR_U, match_arith, emit_shift},
    {IR_TRUNC, match_convert, emit_convert},
    {IR_SEXT, match_convert, emit_convert},
    {IR_ZEXT, match_convert, emit_convert},
    {IR_ADDR, NULL, emit_addr},
    {IR_ADD_OV, NULL, emit_overflow},
    {IR_SUB_OV, NULL, emit_overflow},
    {IR_MUL_OV, NULL, emit_overflow},
    {IR_BRANCH_OV, NULL, emit_branch_ov},
    {IR_MULH_S, match_arith, emit_mul_high},
    {IR_MULH_U, match_arith, emit_mul_high},
    {IR_ADD_FL, match_arith, emit_flag_op},
    {IR_SUB_FL, match_arith, emit_flag_op},
    {IR_NEG_FL, match_arith, emit_flag_op},
    {IR_FLAG, NULL, emit_flag},
};

/* Printing in AT&T syntax: the source comes before the destination, a
   register has the prefix % and an immediate the prefix $. */

static const char *const names64[] = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
};
static const char *const names32[] = {
    "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
    "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d",
};
static const char *const names16[] = {
    "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
    "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w",
};
static const char *const names8[] = {
    "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b",
};

static const char *const cond_names[] = {
    [COND_EQ] = "e", [COND_NE] = "ne", [COND_LT] = "l", [COND_LE] = "le",
    [COND_GT] = "g", [COND_GE] = "ge", [COND_LO] = "b", [COND_LS] = "be",
    [COND_HI] = "a", [COND_HS] = "ae", [COND_P] = "p", [COND_NP] = "np",
    [COND_VS] = "o", [COND_VC] = "no", [COND_MI] = "s", [COND_PL] = "ns",
};

static const char *const xmm_names[] = {
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
    "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
};

/* The 256-bit form of the same registers, which a vector operand names
   with a value of 1. */
static const char *const ymm_names[] = {
    "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7",
    "ymm8", "ymm9", "ymm10", "ymm11", "ymm12", "ymm13", "ymm14", "ymm15",
};

static void print_register(struct text *out, uint32_t reg, bool vreg)
{
    if (vreg) {
        text_appendf(out, "%%t%" PRIu32, reg);
    } else {
        text_appendf(out, "%%%s", names64[reg]);
    }
}

static void print_operand(struct text *out, const struct ir_module *m,
                          const struct names *names,
                          const struct mach_operand *o)
{

    switch (o->kind) {
    case MACH_VREG:
        text_appendf(out, "%%t%" PRIu32, o->reg);
        break;
    case MACH_PREG:
        text_appendf(out, "%%%s",
                     o->reg >= XMM0 && o->value == 1
                         ? ymm_names[o->reg - XMM0]
                     : o->reg >= XMM0 ? xmm_names[o->reg - XMM0]
                     : o->width == 64 ? names64[o->reg]
                     : o->width == 32 ? names32[o->reg]
                     : o->width == 16 ? names16[o->reg]
                                      : names8[o->reg]);
        break;
    case MACH_IMM:
        text_appendf(out, "$%" PRId64, o->value);
        break;
    case MACH_BLOCK:
    case MACH_NAME:
        mach_symbol(out, m, names, o);
        break;
    case MACH_FUNC:
        mach_symbol(out, m, names, o);
        text_append(out, o->got           ? "@GOTPCREL(%rip)"
                         : o->pc_relative ? "(%rip)"
                                          : "");
        break;
    case MACH_SLOT:
        text_appendf(out, "slot%" PRId64, o->value);
        break;
    case MACH_MEM:
        if (o->value != 0) {
            text_appendf(out, "%" PRId64, o->value);
        }
        text_append(out, "(");
        print_register(out, o->reg, o->base_vreg);
        if (o->scale != 0) {
            text_append(out, ",");
            print_register(out, o->index_reg, o->index_vreg);
            text_appendf(out, ",%u", o->scale);
        }
        text_append(out, ")");
        break;
    case MACH_GLOBAL:
        mach_symbol(out, m, names, o);
        text_append(out, "(%rip)");
        break;
    default:
        break;
    }
}

/* sd for a 64-bit float, ss for a 32-bit one. */
static char float_suffix(uint8_t width)
{
    return width == 64 ? 'd' : 's';
}

/* The size suffix comes from the first register or memory operand. */
static char width_suffix(uint8_t width)
{
    return width == 64 ? 'q' : width == 32 ? 'l' : width == 16 ? 'w' : 'b';
}

static char suffix(const struct mach_inst *inst)
{
    size_t i;

    for (i = 0; i < inst->count; i++) {
        const struct mach_operand *o = &inst->operands[i];
        if (o->kind == MACH_VREG || o->kind == MACH_PREG ||
            o->kind == MACH_MEM) {
            return width_suffix(o->width);
        }
    }
    return 'q';
}

/* DESIGN: x86-64-v3 has AVX, so the scalar float instructions take their
   VEX forms. VEX gives a two-operand SSE instruction a separate first
   source. An instruction that reads and writes one register therefore
   names that register twice. A move between two registers cannot be
   written that way with two operands. It takes vmovaps instead, which
   copies the whole register rather than merging one lane. */
static void print_vex_three(struct text *out, const struct ir_module *m,
                            const struct mach_inst *inst,
                            const struct names *names)
{
    text_append(out, " ");
    print_operand(out, m, names, &inst->operands[1]);
    text_append(out, ", ");
    print_operand(out, m, names, &inst->operands[0]);
    text_append(out, ", ");
    print_operand(out, m, names, &inst->operands[0]);
}

/* A vector instruction in AT&T order: the immediate first, then the
   source, then the destination, which a VEX form of a read-and-write
   instruction names twice. */
static void print_vector(struct text *out, const struct ir_module *m,
                         const struct mach_inst *inst,
                         const struct names *names, const char *v)
{
    uint8_t form = vector_forms[inst->op];
    bool three = *v != '\0' && (form & (VEC_RW | VEC_SHIFT)) != 0;

    text_appendf(out, "%s%s ", (form & VEC_VEX) ? "" : v,
                 opcodes[inst->op].name);
    if ((form & (VEC_IMM | VEC_SHIFT)) != 0) {
        text_appendf(out, "$%" PRId64 ", ",
                     inst->operands[inst->count - 1].value);
    }
    if ((form & VEC_SHIFT) == 0) {
        print_operand(out, m, names, &inst->operands[1]);
        text_append(out, ", ");
    }
    if (three) {
        print_operand(out, m, names, &inst->operands[0]);
        text_append(out, ", ");
    }
    print_operand(out, m, names, &inst->operands[0]);
}

static void print(struct text *out, enum cpu_level cpu,
                  const struct ir_module *m, const struct mach_inst *inst,
                  const struct names *names)
{
    const char *v = cpu_has(cpu, CPU_AVX) ? "v" : "";
    size_t i;
    bool first = true;

    if (inst->op >= X64_ADDPS) {
        print_vector(out, m, inst, names, v);
        return;
    }
    switch (inst->op) {
    case X64_SEH_PUSHREG:
    case X64_SEH_STACKALLOC:
    case X64_SEH_SAVEREG:
    case X64_SEH_SAVEXMM:
    case X64_SEH_ENDPROLOGUE:
        /* A directive takes a register and a number without $. */
        text_append(out, opcodes[inst->op].name);
        for (i = 0; i < inst->count; i++) {
            text_append(out, i == 0 ? " " : ", ");
            if (inst->operands[i].kind == MACH_IMM) {
                text_appendf(out, "%" PRId64, inst->operands[i].value);
            } else {
                print_operand(out, m, names, &inst->operands[i]);
            }
        }
        return;
    case X64_SET:
        text_appendf(out, "set%s ", cond_names[inst->operands[1].value]);
        print_operand(out, m, names, &inst->operands[0]);
        return;
    case X64_J:
        text_appendf(out, "j%s ", cond_names[inst->operands[0].value]);
        print_operand(out, m, names, &inst->operands[1]);
        return;
    case X64_CMOV:
        text_appendf(out, "cmov%s%c ", cond_names[inst->operands[2].value],
                     width_suffix(inst->operands[0].width));
        print_operand(out, m, names, &inst->operands[1]);
        text_append(out, ", ");
        print_operand(out, m, names, &inst->operands[0]);
        return;
    case X64_CALLR:
        text_append(out, "call *");
        print_operand(out, m, names, &inst->operands[0]);
        return;
    case X64_JMP:
    case X64_CALL:
    case X64_RET:
    case X64_CQTO:
    case X64_CLTD:
        text_append(out, opcodes[inst->op].name);
        break;
    case X64_MOVSX:
    case X64_MOVZX:
        /* movsbq and movzwl name the source width and the target width. */
        text_appendf(out, "%s%c%c", opcodes[inst->op].name,
                     width_suffix(inst->operands[1].width),
                     width_suffix(inst->operands[0].width));
        break;
    case X64_ADDS:
    case X64_SUBS:
    case X64_MULS:
    case X64_DIVS:
    case X64_XORP:
    case X64_ANDP:
    case X64_ANDNP:
    case X64_ORP:
        text_appendf(out, "%s%s%c", v, opcodes[inst->op].name,
                     float_suffix(inst->operands[0].width));
        if (*v != '\0') {
            print_vex_three(out, m, inst, names);
            return;
        }
        break;
    case X64_MOVS:
        if (*v != '\0' && inst->operands[0].kind != MACH_MEM &&
            inst->operands[1].kind != MACH_MEM) {
            text_append(out, "vmovaps");
            break;
        }
        text_appendf(out, "%s%s%c", v, opcodes[inst->op].name,
                     float_suffix(inst->operands[0].width));
        break;
    case X64_UCOMIS:
        text_appendf(out, "%s%s%c", v, opcodes[inst->op].name,
                     float_suffix(inst->operands[0].width));
        break;
    case X64_CVTSI2S:
        /* cvtsi2sdq names the float width and the integer width. */
        text_appendf(out, "%s%s%c%c", v, opcodes[inst->op].name,
                     float_suffix(inst->operands[0].width),
                     width_suffix(inst->operands[1].width));
        if (*v != '\0') {
            print_vex_three(out, m, inst, names);
            return;
        }
        break;
    case X64_CVTTS2SI:
        text_appendf(out, "%s%s%c2si", v, opcodes[inst->op].name,
                     float_suffix(inst->operands[1].width));
        break;
    case X64_CVTS2S:
        text_appendf(out, "%s%s%c2s%c", v, opcodes[inst->op].name,
                     float_suffix(inst->operands[1].width),
                     float_suffix(inst->operands[0].width));
        if (*v != '\0') {
            print_vex_three(out, m, inst, names);
            return;
        }
        break;
    case X64_MOVQX:
        text_appendf(out, "%s%s", v,
                     inst->operands[0].width == 64 ? "movq" : "movd");
        break;
    case X64_MOVUPS:
        text_appendf(out, "%s%s", v, opcodes[inst->op].name);
        break;
    case X64_CVTPH2PS:
    case X64_CVTPS2PH:
        text_append(out, opcodes[inst->op].name);
        break;
    default:
        text_appendf(out, "%s%c", opcodes[inst->op].name, suffix(inst));
        break;
    }
    for (i = inst->count; i-- > 0;) {
        text_append(out, first ? " " : ", ");
        print_operand(out, m, names, &inst->operands[i]);
        first = false;
    }
}

/* Frames */

static struct mach_operand stack(int64_t offset)
{
    struct mach_operand m = mach_preg(RSP, 64);

    m.kind = MACH_MEM;
    m.value = offset;
    return m;
}


/* Windows requires a frame of a page or more to probe its pages first.
   __chkstk takes the size in rax and leaves every register but r10 and
   r11 unchanged. */
static void allocate_frame(struct mach_block *b, const struct frame *frame)
{
    struct mach_operand ops[2];
    struct mach_inst *call;

    if (!frame->probe) {
        ops[0] = mach_preg(RSP, 64);
        ops[1] = mach_imm((int64_t)frame->size);
        mach_add(b, X64_SUB, 2, ops);
        return;
    }
    ops[0] = mach_preg(RAX, 32);
    ops[1] = mach_imm((int64_t)frame->size);
    mach_add(b, X64_MOV, 2, ops);
    memset(ops, 0, sizeof ops);
    ops[0].kind = MACH_NAME;
    ops[0].name = "__chkstk";
    mach_add(b, X64_CALL, 1, ops);
    call = &b->insts[b->count - 1];
    call->uses = BIT(RAX);
    ops[0] = mach_preg(RSP, 64);
    ops[1] = mach_preg(RAX, 64);
    mach_add(b, X64_SUB, 2, ops);
}

/* A spilled float takes 8 bytes and moves with movsd. */
static void load_spill(struct mach_block *b, uint8_t reg, int64_t offset)
{
    struct mach_operand ops[2];

    ops[0] = mach_preg(reg, 64);
    ops[1] = stack(offset);
    mach_add(b, reg >= XMM0 ? X64_MOVS : X64_MOV, 2, ops);
}

static void store_spill(struct mach_block *b, uint8_t reg, int64_t offset)
{
    struct mach_operand ops[2];

    ops[0] = stack(offset);
    ops[1] = mach_preg(reg, 64);
    mach_add(b, reg >= XMM0 ? X64_MOVS : X64_MOV, 2, ops);
}

/* Windows preserves all 128 bits of xmm6 to xmm15, so a callee saves them
   with movups. */
static void save_register(struct mach_block *b, uint8_t reg, int64_t offset,
                          bool restore)
{
    struct mach_operand ops[2];

    if (reg < XMM0) {
        if (restore) {
            load_spill(b, reg, offset);
        } else {
            store_spill(b, reg, offset);
        }
        return;
    }
    ops[restore ? 1 : 0] = stack(offset);
    ops[restore ? 0 : 1] = mach_preg(reg, 64);
    mach_add(b, X64_MOVUPS, 2, ops);
}

static void resolve_slot(struct mach_operand *o, int64_t offset)
{
    *o = stack(offset);
}

/* Append the unwind directive op with count operands to a prologue that
   carries directives. */
static void unwind(struct mach_block *b, const struct frame *frame,
                   enum x64_op op, size_t count,
                   const struct mach_operand *operands)
{
    if (frame->unwind) {
        mach_add(b, op, count, operands);
    }
}

/* rbp points to the saved rbp above the frame. Callee-saved registers
   take the top of the frame, slots its bottom.
   DESIGN: the Windows unwind data names no frame register. With one, each
   save offset would count from rbp, above the saves, and a save offset is
   unsigned. Anti has no alloca, so rsp alone locates the frame. If Anti
   gets dynamic stack allocation, the Windows prologue must set a frame
   register and the epilogue must switch to lea. */
static void prologue(struct mach_block *b, const struct frame *frame)
{
    struct mach_operand ops[2];
    size_t i;

    if (!frame->needed) {
        return;
    }
    ops[0] = mach_preg(RBP, 64);
    mach_add(b, X64_PUSH, 1, ops);
    unwind(b, frame, X64_SEH_PUSHREG, 1, ops);
    ops[1] = mach_preg(RSP, 64);
    mach_add(b, X64_MOV, 2, ops);
    if (frame->size > 0) {
        allocate_frame(b, frame);
        ops[0] = mach_imm((int64_t)frame->size);
        unwind(b, frame, X64_SEH_STACKALLOC, 1, ops);
    }
    for (i = 0; i < frame->saved_count; i++) {
        save_register(b, frame->saved[i], frame->saved_offset[i], false);
        ops[0] = mach_preg(frame->saved[i], 64);
        ops[1] = mach_imm(frame->saved_offset[i]);
        unwind(b, frame,
               frame->saved[i] >= XMM0 ? X64_SEH_SAVEXMM : X64_SEH_SAVEREG,
               2, ops);
    }
    unwind(b, frame, X64_SEH_ENDPROLOGUE, 0, ops);
}

/* DESIGN: Windows x64 unwinding recognises an epilogue by its code. It
   is add rsp, or lea rsp from the frame register of the unwind data, then
   pops and ret. The Windows unwind data names no frame register, so the
   Windows epilogue frees the frame with add. */
static void epilogue(struct mach_block *b, const struct frame *frame)
{
    struct mach_operand ops[2];
    size_t i;

    if (!frame->needed) {
        return;
    }
    for (i = 0; i < frame->saved_count; i++) {
        save_register(b, frame->saved[i], frame->saved_offset[i], true);
    }
    if (frame->size > 0) {
        ops[0] = mach_preg(RSP, 64);
        ops[1] = mach_preg(RBP, 64);
        if (frame->unwind) {
            ops[1] = mach_imm((int64_t)frame->size);
            mach_add(b, X64_ADD, 2, ops);
        } else {
            mach_add(b, X64_MOV, 2, ops);
        }
    }
    ops[0] = mach_preg(RBP, 64);
    mach_add(b, X64_POP, 1, ops);
}

static const struct target_desc desc = {
    .name = "x86_64",
    .chapter = 14,
    .opcodes = opcodes,
    .patterns = patterns,
    .pattern_count = sizeof patterns / sizeof patterns[0],
    .abi = abi,
    .width = width,
    .move = move,
    .load = load,
    .print = print,
    .fits_address = fits_address,
    .stack_param = stack_param,
    .locate = locate,
    .load_float = load_float,
    .locate_result = locate_result,
    .load_bytes = load_bytes,
    .store_bytes = store_bytes,
    .slot_address = slot_address,
    .incoming_address = incoming_address,
    .copy_memory = copy_memory,
    /* DESIGN: sub and memory displacements hold signed 32 bits. A larger
       frame is an error rather than a movabs sequence, see docs/decisions.md. */
    .frame_limit = 0x7fffffff,
    .flags_native = flags_native,
    .vector_native = vector_native,
    .load_spill = load_spill,
    .store_spill = store_spill,
    .resolve_slot = resolve_slot,
    .expand = NULL,
    .prologue = prologue,
    .epilogue = epilogue,
};

const struct target_desc *target_desc_x86_64(void)
{
    return &desc;
}
