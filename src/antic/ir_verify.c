#include "ir.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "attributes.h"

/* Check what every later stage relies on. Each block ends with exactly
   one terminator. Every operand refers to something that exists, and
   operand types agree with their instruction. A lowering bug then fails
   here, next to its cause, before it becomes wrong assembly. */

struct verifier {
    const struct ir_module *m;
    const struct ir_function *f;
    const struct ir_block *b;
    struct text *errors;
    bool ok;
};

static void fail(struct verifier *v, const char *format, ...)
    ATTRIBUTE_PRINTF(2, 3);

static void fail(struct verifier *v, const char *format, ...)
{
    char message[160];
    va_list args;

    va_start(args, format);
    ir_vformat(message, sizeof message, format, args);
    va_end(args);
    if (v->b != NULL) {
        text_appendf(v->errors, "%s.%s b%" PRIu32 ": %s\n", v->f->module,
                     v->f->name, v->b->index, message);
    } else {
        text_appendf(v->errors, "%s.%s: %s\n", v->f->module, v->f->name,
                     message);
    }
    v->ok = false;
}

static bool is_overflow(enum ir_op op)
{
    return op == IR_ADD_OV || op == IR_SUB_OV || op == IR_MUL_OV;
}

static bool is_flag_operation(enum ir_op op)
{
    return op == IR_ADD_FL || op == IR_SUB_FL || op == IR_MUL_FL ||
           op == IR_SHL_FL || op == IR_SHR_S_FL || op == IR_SHR_U_FL ||
           op == IR_NEG_FL;
}

static bool is_terminator(enum ir_op op)
{
    return op == IR_JUMP || op == IR_BRANCH || op == IR_BRANCH_OV ||
           op == IR_RET;
}

/* The type an operand carries, after checking that it refers to
   something that exists. */
static bool operand_ok(struct verifier *v, const struct ir_inst *inst,
                       const struct ir_operand *o)
{
    switch (o->kind) {
    case IR_TEMP:
        if (o->as.temp >= v->f->temp_count) {
            fail(v, "%s uses %%%" PRIu32 ", which does not exist",
                 ir_op_name(inst->op), o->as.temp);
            return false;
        }
        return true;
    case IR_BLOCK:
        if (o->as.index >= v->f->block_count) {
            fail(v, "%s names b%" PRIu32 ", which does not exist",
                 ir_op_name(inst->op), o->as.index);
            return false;
        }
        return true;
    case IR_FUNC:
        if (o->as.index >= v->m->function_count) {
            fail(v, "%s names function %" PRIu32 ", which does not exist",
                 ir_op_name(inst->op), o->as.index);
            return false;
        }
        return true;
    case IR_GLOBAL:
        if (o->as.index >= v->m->global_count) {
            fail(v, "%s names global %" PRIu32 ", which does not exist",
                 ir_op_name(inst->op), o->as.index);
            return false;
        }
        return true;
    case IR_SYM:
        if (o->as.index >= v->m->sym_count) {
            fail(v, "%s uses a symbolic value that does not exist",
                 ir_op_name(inst->op));
            return false;
        }
        return true;
    default:
        return true;
    }
}

/* Whether every index the instruction holds exists: its result, its
   three operands and the arguments of a call. check_inst and the
   analysis of the definitions index with them. */
static bool indices_ok(struct verifier *v, const struct ir_inst *inst)
{
    bool ok = true;
    size_t i;

    if (inst->result != IR_NO_RESULT && inst->result >= v->f->temp_count) {
        fail(v, "%s writes %%%" PRIu32 ", which does not exist",
             ir_op_name(inst->op), inst->result);
        ok = false;
    }
    ok = operand_ok(v, inst, &inst->a) && ok;
    ok = operand_ok(v, inst, &inst->b) && ok;
    ok = operand_ok(v, inst, &inst->c) && ok;
    for (i = 0; i < inst->arg_count; i++) {
        ok = operand_ok(v, inst, &inst->args[i]) && ok;
    }
    return ok;
}

/* Whether the temporary of every parameter exists. */
static bool params_ok(struct verifier *v)
{
    bool ok = true;
    size_t i;

    for (i = 0; i < v->f->param_count; i++) {
        if (v->f->params[i].temp >= v->f->temp_count) {
            fail(v, "parameter %zu is %%%" PRIu32 ", which does not exist", i,
                 v->f->params[i].temp);
            ok = false;
        }
    }
    return ok;
}

/* A jump or a branch goes to a block, which the optimizer and the
   analysis of the definitions index with. */
static void target_ok(struct verifier *v, const struct ir_inst *inst,
                      const struct ir_operand *o)
{
    if (o->kind != IR_BLOCK) {
        fail(v, "%s goes to an operand that is not a block",
             ir_op_name(inst->op));
    }
}

static void vtype_ok(struct verifier *v, const struct ir_inst *inst)
{
    if (inst->of.type == IR_VOID ||
        (inst->of.type == IR_AGG && inst->of.agg >= v->m->agg_count)) {
        fail(v, "%s names a type that does not exist", ir_op_name(inst->op));
    }
}

static enum ir_type type_of(const struct verifier *v, const struct ir_operand *o)
{
    return o->kind == IR_TEMP ? v->f->temps[o->as.temp] : o->type;
}

static void same_type(struct verifier *v, const struct ir_inst *inst,
                      const struct ir_operand *o, enum ir_type expected)
{
    if (o->kind != IR_NONE && operand_ok(v, inst, o) &&
        type_of(v, o) != expected) {
        fail(v, "%s %s has an operand of type %s", ir_op_name(inst->op),
             ir_type_name(expected), ir_type_name(type_of(v, o)));
    }
}

/* Whether op is an operation of one lane that the simd operation kind
   takes on lanes of type lane. */
static bool lane_op_ok(enum ir_op kind, enum ir_op op, enum ir_type lane)
{
    bool is_float = lane == IR_F32 || lane == IR_F64;

    switch (kind) {
    case IR_VBINARY:
        if (op >= IR_EQ && op <= IR_UGE) {
            return !is_float;
        }
        if (op >= IR_FEQ && op <= IR_FGE) {
            return is_float;
        }
        return is_float ? op >= IR_FADD && op <= IR_FDIV
                        : op == IR_ADD || op == IR_SUB || op == IR_MUL ||
                              op == IR_AND || op == IR_OR || op == IR_XOR;
    case IR_VUNARY:
        return is_float ? op == IR_FNEG : op == IR_NEG || op == IR_NOT;
    case IR_VREDUCE:
        if (op == IR_OR || op == IR_AND) {
            return lane == IR_I8;
        }
        return is_float ? op == IR_FADD || op == IR_FLT || op == IR_FGT
                        : op == IR_ADD || op == IR_SLT || op == IR_ULT ||
                              op == IR_SGT || op == IR_UGT;
    default:
        return op == IR_COPY;
    }
}

/* A simd operation names a simd struct, the type of its lanes and an
   operation of one lane, and takes addresses of values. */
static void vector_ok(struct verifier *v, const struct ir_inst *inst)
{
    const char *name = ir_op_name(inst->op);
    const struct ir_aggtype *t;
    enum ir_type lane;
    size_t i;

    if (inst->of.type != IR_AGG || inst->of.agg >= v->m->agg_count ||
        !v->m->aggs[inst->of.agg]->simd) {
        fail(v, "%s names a type that is not a simd struct", name);
        return;
    }
    t = v->m->aggs[inst->of.agg];
    lane = t->fields[0].type.type;
    if (inst->type != lane) {
        fail(v, "%s works on lanes of %s in a simd struct of %s", name,
             ir_type_name(inst->type), ir_type_name(lane));
    }
    if (!lane_op_ok(inst->op, (enum ir_op)inst->field, lane)) {
        fail(v, "%s takes no %s on lanes of %s", name,
             inst->field <= IR_RET ? ir_op_name((enum ir_op)inst->field)
                                   : "operation",
             ir_type_name(lane));
    }
    same_type(v, inst, &inst->a, IR_PTR);
    switch (inst->op) {
    case IR_VSPLAT:
        same_type(v, inst, &inst->b, lane);
        break;
    case IR_VSELECT:
        same_type(v, inst, &inst->b, IR_PTR);
        same_type(v, inst, &inst->c, IR_PTR);
        if (inst->arg_count != 1) {
            fail(v, "vselect takes one operand after its mask and value");
        } else {
            same_type(v, inst, &inst->args[0], IR_PTR);
        }
        break;
    case IR_VSHUFFLE:
        same_type(v, inst, &inst->b, IR_PTR);
        if (inst->arg_count != t->field_count) {
            fail(v, "vshuffle names %zu lanes of a simd struct of %zu",
                 inst->arg_count, t->field_count);
        }
        for (i = 0; i < inst->arg_count; i++) {
            if (inst->args[i].kind != IR_INT ||
                inst->args[i].as.integer >= t->field_count) {
                fail(v, "vshuffle names a lane that is not a constant index");
            }
        }
        break;
    case IR_VREDUCE:
        break;
    default:
        same_type(v, inst, &inst->b, IR_PTR);
        same_type(v, inst, &inst->c, IR_PTR);
        break;
    }
}

static void check_inst(struct verifier *v, const struct ir_inst *inst)
{
    enum ir_type result = v->f->result == IR_AGG ? IR_PTR : v->f->result;

    switch (inst->op) {
    case IR_ADD: case IR_SUB: case IR_MUL: case IR_SDIV: case IR_UDIV:
    case IR_SREM: case IR_UREM: case IR_AND: case IR_OR: case IR_XOR:
    case IR_SHL: case IR_SHR_S: case IR_SHR_U: case IR_FADD: case IR_FSUB:
    case IR_FMUL: case IR_FDIV:
        same_type(v, inst, &inst->a, inst->type);
        same_type(v, inst, &inst->b, inst->type);
        break;
    case IR_NEG: case IR_NOT: case IR_FNEG: case IR_COPY: case IR_NEG_FL:
        same_type(v, inst, &inst->a, inst->type);
        break;
    /* The carry of + and the borrow of - is a bool, or none. */
    case IR_ADD_FL: case IR_SUB_FL:
        same_type(v, inst, &inst->a, inst->type);
        same_type(v, inst, &inst->b, inst->type);
        same_type(v, inst, &inst->c, IR_I8);
        break;
    case IR_FLAG:
        if (inst->a.kind != IR_TEMP || inst->type != IR_I8 ||
            inst->field > IR_FLAG_NEGATIVE) {
            fail(v, "flag reads a flag of a temporary into an i8");
        }
        break;
    /* An overflow operation gives the result of the arithmetic, and
       IR_BRANCH_OV reads whether it left the range. */
    case IR_ADD_OV: case IR_SUB_OV: case IR_MUL_OV:
    case IR_MULH_S: case IR_MULH_U: case IR_ADD_SAT_S: case IR_ADD_SAT_U:
    case IR_SUB_SAT_S: case IR_SUB_SAT_U: case IR_MUL_SAT_S:
    case IR_MUL_SAT_U: case IR_MUL_FL: case IR_SHL_FL: case IR_SHR_S_FL:
    case IR_SHR_U_FL:
        same_type(v, inst, &inst->a, inst->type);
        same_type(v, inst, &inst->b, inst->type);
        break;
    case IR_EQ: case IR_NE: case IR_SLT: case IR_SLE: case IR_SGT:
    case IR_SGE: case IR_ULT: case IR_ULE: case IR_UGT: case IR_UGE:
    case IR_FEQ: case IR_FNE: case IR_FLT: case IR_FLE: case IR_FGT:
    case IR_FGE:
        if (operand_ok(v, inst, &inst->a) && operand_ok(v, inst, &inst->b) &&
            type_of(v, &inst->a) != type_of(v, &inst->b)) {
            fail(v, "%s compares %s with %s", ir_op_name(inst->op),
                 ir_type_name(type_of(v, &inst->a)),
                 ir_type_name(type_of(v, &inst->b)));
        }
        if (inst->type != IR_I8) {
            fail(v, "%s gives i8, not %s", ir_op_name(inst->op),
                 ir_type_name(inst->type));
        }
        break;
    case IR_HEXT:
        same_type(v, inst, &inst->a, IR_I16);
        if (inst->type != IR_F32) {
            fail(v, "hext gives f32, not %s", ir_type_name(inst->type));
        }
        break;
    case IR_HTRUNC:
        same_type(v, inst, &inst->a, IR_F32);
        if (inst->type != IR_I16) {
            fail(v, "htrunc gives i16, not %s", ir_type_name(inst->type));
        }
        break;
    case IR_LOAD:
        same_type(v, inst, &inst->a, IR_PTR);
        break;
    case IR_STORE:
        same_type(v, inst, &inst->a, inst->type);
        same_type(v, inst, &inst->b, IR_PTR);
        break;
    case IR_PTRADD:
        same_type(v, inst, &inst->a, IR_PTR);
        same_type(v, inst, &inst->b, IR_I64);
        break;
    case IR_MEMCOPY:
        same_type(v, inst, &inst->a, IR_PTR);
        same_type(v, inst, &inst->b, IR_PTR);
        vtype_ok(v, inst);
        break;
    case IR_SLOT:
        vtype_ok(v, inst);
        break;
    case IR_BITLOAD:
    case IR_BITSTORE:
        same_type(v, inst, inst->op == IR_BITLOAD ? &inst->a : &inst->b,
                  IR_PTR);
        if (inst->op == IR_BITSTORE) {
            same_type(v, inst, &inst->a, inst->type);
        }
        if (inst->of.type != IR_AGG || inst->of.agg >= v->m->agg_count ||
            inst->field >= v->m->aggs[inst->of.agg]->field_count ||
            v->m->aggs[inst->of.agg]->fields[inst->field].bits == 0) {
            fail(v, "%s names a field that is not a bitfield",
                 ir_op_name(inst->op));
        }
        break;
    case IR_VBINARY:
    case IR_VUNARY:
    case IR_VSPLAT:
    case IR_VSELECT:
    case IR_VSHUFFLE:
    case IR_VREDUCE:
        vector_ok(v, inst);
        break;
    case IR_CALL: {
        /* A direct call names its callee in a, a call through a pointer
           its signature in b. */
        const struct ir_operand *named =
            inst->b.kind == IR_FUNC ? &inst->b : &inst->a;
        if (inst->a.kind != IR_FUNC) {
            same_type(v, inst, &inst->a, IR_PTR);
        }
        if (named->kind == IR_FUNC && operand_ok(v, inst, named)) {
            const struct ir_function *callee = v->m->functions[named->as.index];
            size_t k;
            if (callee->variadic ? inst->arg_count < callee->param_count
                                 : inst->arg_count != callee->param_count) {
                fail(v, "call passes %zu arguments to %s, which takes %zu",
                     inst->arg_count, callee->name, callee->param_count);
            }
            /* The checker passes a scalar alone past the parameters, and
               the back ends read no parameter record there. */
            for (k = callee->param_count; k < inst->arg_count; k++) {
                if (inst->args[k].type == IR_AGG) {
                    fail(v, "call passes an aggregate as variadic argument "
                            "%zu of %s", k, callee->name);
                    break;
                }
            }
        }
        /* A call through a table names the descriptor of its class and
           a slot after the descriptor's own. */
        if (inst->c.kind != IR_NONE &&
            (inst->c.kind != IR_GLOBAL || !operand_ok(v, inst, &inst->c) ||
             inst->field == 0)) {
            fail(v, "call names a table that is not a class and a slot");
        }
        break;
    }
    case IR_JUMP:
        target_ok(v, inst, &inst->a);
        break;
    case IR_BRANCH:
        same_type(v, inst, &inst->a, IR_I8);
        target_ok(v, inst, &inst->b);
        target_ok(v, inst, &inst->c);
        break;
    case IR_BRANCH_OV:
        if (inst->a.kind != IR_TEMP) {
            fail(v, "branchov reads %s and not a temporary",
                 ir_op_name(inst->op));
        }
        target_ok(v, inst, &inst->b);
        target_ok(v, inst, &inst->c);
        break;
    case IR_RET:
        if (inst->type != result) {
            fail(v, "ret %s in a function that returns %s",
                 ir_type_name(inst->type), ir_type_name(result));
        } else if (inst->type != IR_VOID) {
            same_type(v, inst, &inst->a, inst->type);
        }
        break;
    default:
        operand_ok(v, inst, &inst->a);
        break;
    }
}

static bool is_set(const uint64_t *set, uint32_t temp)
{
    return (set[temp / 64] >> (temp % 64)) & 1;
}

static void set_bit(uint64_t *set, uint32_t temp)
{
    set[temp / 64] |= (uint64_t)1 << (temp % 64);
}

static void use(struct verifier *v, const struct ir_inst *inst,
                const struct ir_operand *o, const uint64_t *defined)
{
    if (o->kind == IR_TEMP && o->as.temp < v->f->temp_count &&
        !is_set(defined, o->as.temp)) {
        fail(v, "%s uses %%%" PRIu32 " before a definition on some path",
             ir_op_name(inst->op), o->as.temp);
    }
}

/* The temporaries defined on every path into block b: the parameters at
   the entry, intersected with what each predecessor defines on exit. */
static void entry_set(const struct ir_function *f, size_t b,
                      const uint64_t *out, size_t words, uint64_t *in)
{
    size_t i;
    size_t k;

    memset(in, b == 0 ? 0 : 0xff, words * sizeof *in);
    if (b == 0) {
        for (i = 0; i < f->param_count; i++) {
            set_bit(in, f->params[i].temp);
        }
    }
    for (i = 0; i < f->block_count; i++) {
        const struct ir_block *pred = f->blocks[i];
        const struct ir_inst *last;
        if (pred->count == 0) {
            continue;
        }
        last = &pred->insts[pred->count - 1];
        if ((last->op == IR_JUMP && last->a.as.index == b) ||
            ((last->op == IR_BRANCH || last->op == IR_BRANCH_OV) &&
             (last->b.as.index == b || last->c.as.index == b))) {
            for (k = 0; k < words; k++) {
                in[k] &= out[i * words + k];
            }
        }
    }
}

/* Every path from the entry to a use of a temporary passes a definition
   of it. The optimizer relies on that. A forward analysis starts every
   block with the full set and shrinks the sets until they stop changing.
   A block that no path reaches keeps the full set and reports nothing. */
static void check_definitions(struct verifier *v)
{
    const struct ir_function *f = v->f;
    size_t words = f->temp_count / 64 + 1;
    size_t n = f->block_count;
    uint64_t *out = ir_alloc(ir_product(n + 1, words), sizeof *out);
    uint64_t *in = ir_alloc(words, sizeof *in);
    bool changed = true;
    size_t b;
    size_t i;
    size_t k;

    memset(out, 0xff, n * words * sizeof *out);
    while (changed) {
        changed = false;
        for (b = 0; b < n; b++) {
            entry_set(f, b, out, words, in);
            for (i = 0; i < f->blocks[b]->count; i++) {
                if (f->blocks[b]->insts[i].result < f->temp_count) {
                    set_bit(in, f->blocks[b]->insts[i].result);
                }
            }
            if (memcmp(in, out + b * words, words * sizeof *in) != 0) {
                memcpy(out + b * words, in, words * sizeof *in);
                changed = true;
            }
        }
    }
    for (b = 0; b < n; b++) {
        v->b = f->blocks[b];
        entry_set(f, b, out, words, in);
        for (i = 0; i < v->b->count; i++) {
            const struct ir_inst *inst = &v->b->insts[i];
            use(v, inst, &inst->a, in);
            use(v, inst, &inst->b, in);
            use(v, inst, &inst->c, in);
            for (k = 0; k < inst->arg_count; k++) {
                use(v, inst, &inst->args[k], in);
            }
            if (inst->result < f->temp_count) {
                set_bit(in, inst->result);
            }
        }
    }
    free(out);
    free(in);
}

static bool global_ok(const struct ir_module *m, uint32_t g, bool optional)
{
    return (optional && g == IR_NO_INDEX) || g < m->global_count;
}

/* Every global, function, aggregate and field a class record names
   exists. */
static bool check_class(const struct ir_module *m, const struct ir_class *c,
                        struct text *errors)
{
    bool ok = global_ok(m, c->descriptor, false) &&
              global_ok(m, c->base, false) && global_ok(m, c->table, true) &&
              (c->init == IR_NO_INDEX || c->init < m->function_count) &&
              (c->mutable_count == 0 || c->agg < m->agg_count);
    size_t i;

    for (i = 0; ok && i < c->subtable_count; i++) {
        ok = global_ok(m, c->subtables[i].interface, false) &&
             global_ok(m, c->subtables[i].table, false);
    }
    for (i = 0; ok && i < c->mutable_count; i++) {
        ok = c->mutable_fields[i] < m->aggs[c->agg]->field_count;
    }
    if (!ok) {
        text_appendf(errors, "class %s.%s names what the module does not "
                             "hold\n", c->module, c->name);
    }
    return ok;
}

bool ir_verify(const struct ir_module *m, struct text *errors)
{
    struct verifier v = {m, NULL, NULL, errors, true};
    size_t i;
    size_t j;
    size_t k;

    for (i = 0; i < m->class_count; i++) {
        v.ok = check_class(m, m->classes[i], errors) && v.ok;
    }

    for (i = 0; i < m->function_count; i++) {
        v.f = m->functions[i];
        v.b = NULL;
        if (v.f->is_extern) {
            continue;
        }
        params_ok(&v);
        for (j = 0; j < v.f->block_count; j++) {
            const struct ir_block *b = v.f->blocks[j];
            v.b = b;
            for (k = 0; k < b->count; k++) {
                if (indices_ok(&v, &b->insts[k])) {
                    check_inst(&v, &b->insts[k]);
                }
                /* A branch on overflow reads the flags of the
                   instruction right before it, so nothing may come
                   between the two. */
                if (b->insts[k].op == IR_BRANCH_OV &&
                    (k == 0 ||
                     !is_overflow(b->insts[k - 1].op) ||
                     b->insts[k - 1].result != b->insts[k].a.as.temp)) {
                    fail(&v, "branchov does not follow its operation");
                }
                /* The reads of the flags follow their operation, with
                   other reads alone between. */
                if (b->insts[k].op == IR_FLAG) {
                    size_t at = k;
                    while (at > 0 && b->insts[at - 1].op == IR_FLAG) {
                        at--;
                    }
                    if (at == 0 || !is_flag_operation(b->insts[at - 1].op) ||
                        b->insts[at - 1].result != b->insts[k].a.as.temp) {
                        fail(&v, "flag does not follow its operation");
                    }
                }
                if (is_terminator(b->insts[k].op) && k + 1 < b->count) {
                    fail(&v, "%s is not the last instruction",
                         ir_op_name(b->insts[k].op));
                }
            }
            if (b->count == 0 || !is_terminator(b->insts[b->count - 1].op)) {
                fail(&v, "the block does not end with a terminator");
            }
        }
        /* The analysis follows block operands, which must be valid. */
        if (v.ok) {
            check_definitions(&v);
        }
    }
    return v.ok;
}
