/* `parallel` and `dispatch` in lowering. The context a worker reads, the
   thunk the runtime calls for each chunk or each job, and the calls that
   start the jobs and join them. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "sema.h"
#include "text.h"
#include "types.h"
#include "lower_lowerer.h"

/* The count of `parallel` thunks already written here, which names the
   next one. */
static size_t next_thunk(const struct lowerer *l, const char *prefix)
{
    size_t length = strlen(prefix);
    size_t count = 0;
    size_t i;

    for (i = 0; i < l->m->function_count; i++) {
        const struct ir_function *g = l->m->functions[i];
        if (g->module != NULL && strcmp(g->module, l->module_name) == 0 &&
            strncmp(g->name, prefix, length) == 0) {
            count++;
        }
    }
    return count;
}

/* The type of parameter i + 1 of the worker that call names, which holds
   argument i of the call. */
static const struct type *worker_param(const struct expr *call, size_t i)
{
    return call->as.call.callee->symbol->type->params[i + 1];
}

/* The aggregate that carries the arguments every chunk receives, or
   IR_NO_AGG when the worker takes the chunk alone. */
static uint32_t context_aggregate(struct lowerer *l, const struct expr *call,
                                  const char *name)
{
    struct ir_field *fields;
    char **names;
    uint32_t agg;
    size_t n = call->as.call.arg_count;
    size_t i;

    fields = alloc_zeroed(n, sizeof *fields);
    names = alloc_zeroed(n, sizeof *names);
    for (i = 0; i < n; i++) {
        names[i] = alloc_zeroed(24, 1);
        text_format(names[i], 24, "a%zu", i);
        fields[i].name = names[i];
        fields[i].type = lower_vtype_of(l, worker_param(call, i));
        fields[i].bits = 0;
        fields[i].ext = IR_EXT_NONE;
    }
    agg = ir_struct_add(l->m, IR_AGG_STRUCT, name, fields, n, false, 0);
    for (i = 0; i < n; i++) {
        free(names[i]);
    }
    free(names);
    free(fields);
    return agg;
}

/* Pack the arguments of call into a context in a slot of the frame,
   named `<name>.context`. Returns the address of the slot, or a null
   pointer for a call without arguments, and *agg receives the aggregate
   of the context or IR_NO_AGG. */
static struct ir_operand pack_context(struct lowerer *l,
                                      const struct expr *call, size_t extra,
                                      const char *name, uint32_t *agg)
{
    struct text context_name = {0};
    uint32_t slot;
    size_t i;

    *agg = IR_NO_AGG;
    if (extra == 0) {
        return ir_int_op(IR_PTR, 0);
    }
    text_appendf(&context_name, "%s.context", name);
    *agg = context_aggregate(l, call, text_cstr(&context_name));
    text_free(&context_name);
    slot = ir_slot(l->f, l->b, ir_aggregate(*agg));
    for (i = 0; i < extra; i++) {
        const struct expr *arg = call->as.call.args[i];
        struct ir_operand at =
            lower_offset_address(l, lower_temp(l, slot),
                                 ir_sym_operand(l->m,
                                                ir_sym_offset_of(l->m, *agg,
                                                                 (uint32_t)i)));
        lower_store_value(l, worker_param(call, i), arg, at);
        /* A local that moves into an `own` parameter of the worker is
           the job's now. The error a handler binds holds `none` after
           it, and any other local a cleared table, so the teardown of
           the caller passes over it. */
        if (arg->moves && arg->kind == EXPR_NAME && arg->symbol != NULL &&
            arg->symbol->caught) {
            ir_assign(l->f, l->b, arg->symbol->ir, ir_int_op(IR_PTR, 0));
        } else {
            lower_clear_moved(l, arg);
        }
    }
    return lower_temp(l, slot);
}

/* The body the two thunks share, written into the entry block of the
   thunk the lowerer is in. It calls the worker of call with first and
   the arguments the context of parameter 0 holds. What the worker gives
   goes to the address in parameter out.

   DESIGN: a fresh argument of a parameter that is not `own` has no owner
   but the job of a `dispatch`, as the argument of a call has no owner but
   its caller. Its thunk tears it down in the context once the worker has
   returned, when drop is true, and the worker copies what it keeps. A
   fresh argument of an `own` parameter is the worker's, which tears it
   down. Every chunk of a `parallel` reads one context, so no chunk tears
   it down. */
static void call_worker(struct lowerer *l, const struct expr *call,
                        uint32_t context, struct ir_operand first,
                        size_t out, bool drop)
{
    const struct expr *callee = call->kind == EXPR_CALL
                                    ? call->as.call.callee : call;
    const struct type *result = callee->symbol->type->result;
    size_t extra = call->kind == EXPR_CALL ? call->as.call.arg_count : 0;
    struct ir_function *f = l->f;
    struct ir_operand *args =
        alloc_zeroed(alloc_sum(alloc_product(extra, 2), 1), sizeof *args);
    struct ir_operand *values = alloc_zeroed(extra + 1, sizeof *values);
    struct ir_operand at_out;
    uint32_t value;
    size_t count = 1;
    size_t i;

    args[0] = first;
    for (i = 0; i < extra; i++) {
        const struct type *t = worker_param(call, i);
        struct ir_operand at =
            lower_offset_address(l, lower_temp(l, f->params[0].temp),
                                 ir_sym_operand(l->m,
                                                ir_sym_offset_of(l->m, context,
                                                                 (uint32_t)i)));
        values[i] = lower_is_aggregate(t)
                        ? at
                        : lower_temp(l, ir_load(f, l->b, lower_ir_type_of(t),
                                                at));
        lower_push_argument(l, args, &count, values[i], t);
    }
    value = ir_call(f, l->b, lower_ir_type_of(result),
                    ir_func_op(lower_callee_function(l, callee->symbol)), args,
                    count);
    free(args);
    at_out = lower_temp(l, f->params[out].temp);
    if (result->kind == TYPE_VOID) {
        /* A worker without a result writes nothing. */
    } else if (lower_is_aggregate(result)) {
        ir_memcopy(f, l->b, at_out, lower_temp(l, value),
                   lower_vtype_of(l, result));
    } else {
        ir_store(f, l->b, lower_ir_type_of(result), lower_temp(l, value),
                 at_out);
    }
    if (drop && extra > 0) {
        lower_drop_arguments(l, callee->symbol, callee->symbol->type,
                             (const struct expr *const *)call->as.call.args,
                             values, extra, 1);
    }
    free(values);
    ir_ret(f, l->b, IR_VOID, lower_none());
}

/* DESIGN: the worker pool calls one C signature, and a worker has the
   signature its own declaration gives. antic writes a thunk for each
   `parallel` that joins the two. The thunk rebuilds the chunk from the
   pointer and the length. It then reads the arguments that every chunk
   shares out of the context, calls the worker and stores its result. */
static struct ir_function *parallel_thunk(struct lowerer *l,
                                          const struct expr *e,
                                          const char *name, uint32_t context)
{
    const struct type *slice = e->as.parallel.array->type;
    struct ir_function *outer_f = l->f;
    struct ir_block *outer_b = l->b;
    struct ir_function *f;
    struct ir_block *entry;
    uint32_t slot;

    f = ir_function_add(l->m, l->module_name, name, IR_VOID, IR_NO_AGG);
    ir_param_add(f, IR_PTR, IR_NO_AGG);     /* the context */
    ir_param_add(f, IR_PTR, IR_NO_AGG);     /* the first element */
    ir_param_add(f, IR_I64, IR_NO_AGG);     /* the element count */
    ir_param_add(f, IR_PTR, IR_NO_AGG);     /* where the result goes */
    entry = ir_block_add(f);
    l->f = f;
    l->b = entry;

    slot = ir_slot(f, entry, lower_vtype_of(l, slice));
    ir_store(f, entry, IR_PTR, lower_temp(l, f->params[1].temp),
             lower_temp(l, slot));
    ir_store(f, entry, IR_I64, lower_temp(l, f->params[2].temp),
             lower_offset_address(l, lower_temp(l, slot),
                                  lower_field_offset(l, slice,
                                                     &lower_len_name)));
    call_worker(l, e->as.parallel.call, context, lower_temp(l, slot), 3,
                false);
    l->f = outer_f;
    l->b = outer_b;
    return f;
}

/* DESIGN: the thunk of a dispatch has the shape the runtime calls: the
   context, the object and the address of the result. It unpacks the
   context and calls the worker with the object first. */
static struct ir_function *dispatch_thunk(struct lowerer *l,
                                          const struct expr *e,
                                          const char *name, uint32_t context)
{
    struct ir_function *outer_f = l->f;
    struct ir_block *outer_b = l->b;
    struct ir_function *f;

    f = ir_function_add(l->m, l->module_name, name, IR_VOID, IR_NO_AGG);
    ir_param_add(f, IR_PTR, IR_NO_AGG);     /* the context */
    ir_param_add(f, IR_PTR, IR_NO_AGG);     /* the object */
    ir_param_add(f, IR_PTR, IR_NO_AGG);     /* where the result goes */
    l->f = f;
    l->b = ir_block_add(f);
    call_worker(l, e->as.dispatch.call, context,
                lower_temp(l, f->params[1].temp),
                2, true);
    l->f = outer_f;
    l->b = outer_b;
    return f;
}

/* DESIGN: `dispatch` is one call of the runtime with the object, the size
   of the result, the thunk, the context and its size. The runtime gives
   a handle back, which the Job holds in its one field. The context lies
   in a slot of the frame, which a later turn of a loop writes again and
   a return ends, so the runtime copies it into the job: by decision 23
   of "Decisions of 2026-10-11" each job keeps the arguments of its own
   `dispatch`. */
struct ir_operand lower_dispatch(struct lowerer *l,
                                 const struct expr *e)
{
    const struct expr *call = e->as.dispatch.call;
    const struct expr *callee = call->kind == EXPR_CALL
                                    ? call->as.call.callee : call;
    const struct type *result = callee->symbol->type->result;
    size_t extra = call->kind == EXPR_CALL ? call->as.call.arg_count : 0;
    struct ir_operand context;
    struct ir_operand args[5];
    struct ir_function *f;
    uint32_t agg;
    struct ir_operand handle;
    uint32_t out;
    char name[TEXT_NAME_SIZE];

    text_format_name(name, sizeof name, "dispatch.%zu",
                     next_thunk(l, "dispatch."));
    args[0] = lower_expr(l, e->as.dispatch.object);
    context = pack_context(l, call, extra, name, &agg);
    args[1] = result->kind == TYPE_VOID ? ir_int_op(IR_I64, 0)
                                        : lower_size_operand(l, result);
    f = dispatch_thunk(l, e, name, agg);
    args[2] = lower_temp(l, ir_addr(l->f, l->b, ir_func_op(f)));
    args[3] = context;
    args[4] = agg == IR_NO_AGG
                  ? ir_int_op(IR_I64, 0)
                  : ir_sym_operand(l->m, ir_sym_size_of(l->m,
                                                        ir_aggregate(agg)));
    handle = lower_rt_call(l, RT_FN_DISPATCH, args);
    lower_hook_object(l, HOOK_DISPATCHED, args[0]);
    out = ir_slot(l->f, l->b, lower_vtype_of(l, e->type));
    ir_store(l->f, l->b, IR_PTR, handle, lower_temp(l, out));
    return lower_temp(l, out);
}

/* `join` waits for one job and writes its result into a slot, and
   `join_all` walks the slice of jobs. */
/* DESIGN: by decision 23 of "Decisions of 2026-10-11" a second `join`
   of one job traps in a dev build where the checker does not see it. The
   runtime frees a job when it is joined, so a dev build writes the
   handle JOB_JOINED into the place that held it, and tests for it before
   every `join`. No allocation gives the address 1, and `none` is the job
   of a dispatch that found its object in flight, which a `join` takes.
   A release build writes and tests nothing, and a second `join` there
   reads a freed job. */
#define JOB_JOINED 1

static void check_not_joined(struct lowerer *l, struct ir_operand handle,
                             int line)
{
    struct ir_operand ok = lower_temp(
        l, ir_binary(l->f, l->b, IR_NE, IR_I8, handle,
                     ir_int_op(IR_PTR, JOB_JOINED)));

    lower_check_branch(l, ok, false,
                       lower_check_text(l, line,
                                        "`join` of a job joined already"),
                       CHECK_TEXT, ir_int_op(IR_I64, 0), lower_none(), NULL);
}

/* `join_all(jobs)` of a dev build: each job in turn, as `join` takes
   one, so each is tested and marked. */
static void join_all_checked(struct lowerer *l, const struct expr *e,
                             struct ir_operand handles,
                             struct ir_operand count, struct ir_operand size)
{
    struct ir_block *test = lower_new_block(l);
    struct ir_block *body = lower_new_block(l);
    struct ir_block *done = lower_new_block(l);
    uint32_t index = ir_unary(l->f, l->b, IR_COPY, IR_I64,
                              ir_int_op(IR_I64, 0));
    struct ir_operand at;
    struct ir_operand args[3];

    ir_jump(l->f, l->b, test);
    l->b = test;
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_SLT, IR_I8,
                                      lower_temp(l, index), count)),
              body, done);
    l->b = body;
    at = lower_temp(
        l, ir_ptradd(l->f, l->b, handles,
                     lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64,
                                             lower_temp(l, index),
                                             size))));
    args[0] = lower_temp(l, ir_load(l->f, l->b, IR_PTR, at));
    check_not_joined(l, args[0], e->pos.line);
    args[1] = ir_int_op(IR_I64, 0);
    args[2] = ir_int_op(IR_PTR, 0);
    lower_rt_call(l, l->hooks ? RT_FN_JOIN_HOOKED : RT_FN_JOIN, args);
    ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, JOB_JOINED), at);
    ir_assign(l->f, l->b, index,
              lower_temp(l, ir_binary(l->f, l->b, IR_ADD, IR_I64,
                                      lower_temp(l, index),
                                      ir_int_op(IR_I64, 1))));
    ir_jump(l->f, l->b, test);
    l->b = done;
}

struct ir_operand lower_join(struct lowerer *l, const struct expr *e)
{
    const struct type *job = e->as.join.job->type;
    struct ir_operand value = lower_address(l, e->as.join.job);
    struct ir_operand args[3];
    struct ir_operand result = lower_none();
    uint32_t out;

    if (e->as.join.all) {
        args[0] = lower_temp(l, ir_load(l->f, l->b, IR_PTR, value));
        args[1] = lower_temp(
            l, ir_load(l->f, l->b, IR_I64,
                       lower_offset_address(
                           l, value,
                           lower_field_offset(l, job, &lower_len_name))));
        if (l->dev) {
            join_all_checked(l, e, args[0], args[1],
                             lower_size_operand(l, job->element));
            return lower_none();
        }
        lower_rt_call(l,
                      l->hooks ? RT_FN_JOIN_ALL_HOOKED : RT_FN_JOIN_ALL,
                      args);
        return lower_none();
    }
    args[0] = lower_temp(l, ir_load(l->f, l->b, IR_PTR, value));
    if (l->dev) {
        check_not_joined(l, args[0], e->pos.line);
    }
    if (e->type->kind == TYPE_VOID) {
        args[1] = ir_int_op(IR_I64, 0);
        args[2] = ir_int_op(IR_PTR, 0);
        lower_rt_call(l, l->hooks ? RT_FN_JOIN_HOOKED : RT_FN_JOIN, args);
    } else {
        out = ir_slot(l->f, l->b, lower_vtype_of(l, e->type));
        args[1] = lower_size_operand(l, e->type);
        args[2] = lower_temp(l, out);
        lower_rt_call(l, l->hooks ? RT_FN_JOIN_HOOKED : RT_FN_JOIN, args);
        result = lower_is_aggregate(e->type)
                     ? lower_temp(l, out)
                     : lower_temp(l, ir_load(l->f, l->b,
                                             lower_ir_type_of(e->type),
                                             lower_temp(l, out)));
    }
    if (l->dev) {
        ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, JOB_JOINED), value);
    }
    return result;
}

/* DESIGN: the count after `by`, which a dev build checks against zero, by
   R087 of "Rules of 2026-10-13". Zero asks the runtime for the count of
   the machine, and the runtime of a build without the check takes a
   count below zero as zero. */
static struct ir_operand chunk_count(struct lowerer *l, const struct expr *e)
{
    const struct expr *n = e->as.parallel.chunks;
    struct ir_operand count = lower_expr(l, n);
    struct ir_operand ok =
        lower_temp(l, ir_binary(l->f, l->b, IR_SGE, IR_I8, count,
                                ir_int_op(IR_I64, 0)));

    lower_check_branch(l, ok, false,
                       lower_check_text(l, n->pos.line,
                                        "count of chunks below zero"),
                       CHECK_VALUE, count, lower_none(), NULL);
    return count;
}

/* The caller of a `parallel` tears down the fresh arguments of the
   parameters that are not `own` in context, of aggregate agg, once every
   chunk has run, as it tears down the arguments of a call. */
static void drop_context(struct lowerer *l, const struct expr *call,
                         struct ir_operand context, uint32_t agg)
{
    size_t extra = call->kind == EXPR_CALL ? call->as.call.arg_count : 0;
    struct ir_operand *values;
    size_t i;

    if (extra == 0) {
        return;
    }
    values = alloc_zeroed(extra, sizeof *values);
    for (i = 0; i < extra; i++) {
        const struct type *t = worker_param(call, i);
        struct ir_operand at = lower_offset_address(
            l, context,
            ir_sym_operand(l->m, ir_sym_offset_of(l->m, agg, (uint32_t)i)));
        values[i] = lower_is_aggregate(t)
                        ? at
                        : lower_temp(l, ir_load(l->f, l->b,
                                                lower_ir_type_of(t), at));
    }
    lower_drop_arguments(l, call->as.call.callee->symbol,
                         call->as.call.callee->symbol->type,
                         (const struct expr *const *)call->as.call.args,
                         values, extra, 1);
    free(values);
}

/* DESIGN: `parallel a by n -> f(x)` becomes one call of the runtime. The
   runtime decides the chunk count when n is absent. It therefore
   allocates the array of results and writes back the pointer and the
   count, and the expression is the slice of those results. */
struct ir_operand lower_parallel(struct lowerer *l,
                                 const struct expr *e)
{
    const struct expr *call = e->as.parallel.call;
    const struct expr *callee = call->kind == EXPR_CALL
                                    ? call->as.call.callee : call;
    const struct type *slice = e->as.parallel.array->type;
    const struct type *result = callee->symbol->type->result;
    size_t extra = call->kind == EXPR_CALL ? call->as.call.arg_count : 0;
    struct ir_operand context;
    struct ir_operand args[9];
    struct ir_operand array;
    struct ir_operand length;
    struct ir_operand length_at;
    uint32_t agg;
    uint32_t results;
    uint32_t count;
    uint32_t out;
    char name[TEXT_NAME_SIZE];

    text_format_name(name, sizeof name, "parallel.%zu",
                     next_thunk(l, "parallel."));
    array = lower_address(l, e->as.parallel.array);
    context = pack_context(l, call, extra, name, &agg);
    results = ir_slot(l->f, l->b, ir_scalar(IR_PTR));
    count = ir_slot(l->f, l->b, ir_scalar(IR_I64));
    args[0] = lower_temp(l, ir_load(l->f, l->b, IR_PTR, array));
    args[1] = lower_temp(
        l, ir_load(l->f, l->b, IR_I64,
                   lower_offset_address(
                       l, array,
                       lower_field_offset(l, slice, &lower_len_name))));
    args[2] = lower_size_operand(l, slice->element);
    args[3] = e->as.parallel.chunks != NULL ? chunk_count(l, e)
                                            : ir_int_op(IR_I64, 0);
    args[4] = lower_size_operand(l, result);
    struct ir_function *thunk = parallel_thunk(l, e, name, agg);
    args[5] = lower_temp(l, ir_addr(l->f, l->b, ir_func_op(thunk)));
    args[6] = context;
    args[7] = lower_temp(l, results);
    args[8] = lower_temp(l, count);
    lower_rt_call(l, RT_FN_PARALLEL, args);
    drop_context(l, call, context, agg);
    out = ir_slot(l->f, l->b, lower_vtype_of(l, e->type));
    ir_store(l->f, l->b, IR_PTR, lower_temp(l, ir_load(l->f, l->b, IR_PTR,
                                                       lower_temp(l, results))),
             lower_temp(l, out));
    length = lower_temp(l, ir_load(l->f, l->b, IR_I64, lower_temp(l, count)));
    length_at = lower_offset_address(l, lower_temp(l, out),
                                     lower_field_offset(l, e->type,
                                                        &lower_len_name));
    ir_store(l->f, l->b, IR_I64, length, length_at);
    return lower_temp(l, out);
}
