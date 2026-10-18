/* The one rule that folds an integer operation on constants, the three
   cases that have no value, and the widths of the integer types. */

#include "ir_fold.h"

#include "arith.h"

int ir_fold_bits(enum ir_type type)
{
    return type == IR_I8 ? 8 : type == IR_I16 ? 16 : type == IR_I32 ? 32 : 64;
}

uint64_t ir_fold_trim(enum ir_type type, uint64_t v)
{
    int n = ir_fold_bits(type);
    return n == 64 ? v : v & (((uint64_t)1 << n) - 1);
}

int64_t ir_fold_signed(enum ir_type type, uint64_t v)
{
    return arith_signed(v, ir_fold_bits(type));
}

static bool is_shift(enum ir_op op)
{
    return op == IR_SHL || op == IR_SHR_S || op == IR_SHR_U;
}

/* The checks of the operations that have no value for some operands. */
static enum ir_fold defined(enum ir_op op, enum ir_type type, uint64_t a,
                            uint64_t b)
{
    int n = ir_fold_bits(type);

    bool divides = op == IR_SDIV || op == IR_UDIV || op == IR_SREM ||
                   op == IR_UREM;

    if (divides && b == 0) {
        return IR_FOLD_BY_ZERO;
    }
    if ((op == IR_SDIV || op == IR_SREM) && a == (uint64_t)1 << (n - 1) &&
        b == ir_fold_trim(type, UINT64_MAX)) {
        return IR_FOLD_LEAST_BY_MINUS_ONE;
    }
    return IR_FOLDED;
}

enum ir_fold ir_fold_int(enum ir_op op, enum ir_type type, uint64_t a,
                         uint64_t b, uint64_t *out)
{
    int n = ir_fold_bits(type);
    uint64_t count = b;
    int64_t x;
    int64_t y;
    enum ir_fold status;

    a = ir_fold_trim(type, a);
    b = ir_fold_trim(type, b);
    x = ir_fold_signed(type, a);
    y = ir_fold_signed(type, b);
    if (is_shift(op) && count >= (uint64_t)n) {
        return IR_FOLD_SHIFT_RANGE;
    }
    status = defined(op, type, a, b);
    if (status != IR_FOLDED) {
        return status;
    }
    switch (op) {
    case IR_ADD: *out = a + b; break;
    case IR_SUB: *out = a - b; break;
    case IR_MUL: *out = a * b; break;
    case IR_SDIV: *out = (uint64_t)(x / y); break;
    case IR_SREM: *out = (uint64_t)(x % y); break;
    case IR_UDIV: *out = a / b; break;
    case IR_UREM: *out = a % b; break;
    case IR_AND: *out = a & b; break;
    case IR_OR: *out = a | b; break;
    case IR_XOR: *out = a ^ b; break;
    case IR_SHL: *out = a << count; break;
    case IR_SHR_S: *out = (uint64_t)arith_shift_right(x, (unsigned)count); break;
    case IR_SHR_U: *out = a >> count; break;
    case IR_EQ: *out = a == b; return IR_FOLDED;
    case IR_NE: *out = a != b; return IR_FOLDED;
    case IR_SLT: *out = x < y; return IR_FOLDED;
    case IR_SLE: *out = x <= y; return IR_FOLDED;
    case IR_SGT: *out = x > y; return IR_FOLDED;
    case IR_SGE: *out = x >= y; return IR_FOLDED;
    case IR_ULT: *out = a < b; return IR_FOLDED;
    case IR_ULE: *out = a <= b; return IR_FOLDED;
    case IR_UGT: *out = a > b; return IR_FOLDED;
    case IR_UGE: *out = a >= b; return IR_FOLDED;
    case IR_MULH_S:
    case IR_MULH_U: *out = arith_mul_high(a, b, n, op == IR_MULH_S); break;
    case IR_ADD_SAT_S:
    case IR_ADD_SAT_U:
    case IR_SUB_SAT_S:
    case IR_SUB_SAT_U:
    case IR_MUL_SAT_S:
    case IR_MUL_SAT_U:
        *out = arith_saturate(
            op == IR_ADD_SAT_S || op == IR_ADD_SAT_U   ? '+'
            : op == IR_SUB_SAT_S || op == IR_SUB_SAT_U ? '-'
                                                       : '*',
            a, b, n,
            op == IR_ADD_SAT_S || op == IR_SUB_SAT_S || op == IR_MUL_SAT_S);
        break;
    default:
        return IR_FOLD_NOT_BINARY;
    }
    *out = ir_fold_trim(type, *out);
    return IR_FOLDED;
}
