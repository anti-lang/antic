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

/* The aggregate that carries the arguments every chunk receives, or
   IR_NO_AGG when the worker takes the chunk alone. */
/* The type of parameter i + 1 of the worker that call names, which holds
   argument i of the call. */
static const struct type *worker_param(const struct expr *call, size_t i)
{
    return call->as.call.callee->symbol->type->params[i + 1];
}

static uint32_t context_aggregate(struct lowerer *l, const struct expr *call,
                                  const char *name)
{
    struct ir_field *fields;
    uint32_t agg;
    size_t n = call->as.call.arg_count;
    size_t i;

    fields = alloc_zeroed(n, sizeof *fields);
    for (i = 0; i < n; i++) {
        char *field = alloc_zeroed(24, 1);
        snprintf(field, 24, "a%zu", i);
        fields[i].name = field;
        fields[i].type = lower_vtype_of(l, worker_param(call, i));
        fields[i].bits = 0;
        fields[i].ext = IR_EXT_NONE;
    }
    agg = ir_struct_add(l->m, IR_AGG_STRUCT, name, fields, n, false, 0);
    for (i = 0; i < n; i++) {
        free((char *)fields[i].name);
    }
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
    }
    return lower_temp(l, slot);
}

/* The body the two thunks share, written into the entry block of the
   thunk the lowerer is in. It calls the worker of call with first and
   the arguments the context of parameter 0 holds. What the worker gives
   goes to the address in parameter out. */
static void call_worker(struct lowerer *l, const struct expr *call,
                        uint32_t context, struct ir_operand first,
                        size_t out)
{
    const struct expr *callee = call->kind == EXPR_CALL
                                    ? call->as.call.callee : call;
    const struct type *result = callee->symbol->type->result;
    size_t extra = call->kind == EXPR_CALL ? call->as.call.arg_count : 0;
    struct ir_function *f = l->f;
    struct ir_block *entry = l->b;
    struct ir_operand *args = alloc_zeroed(2 * extra + 1, sizeof *args);
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
        lower_push_argument(
            l, args, &count,
            lower_is_aggregate(t)
                ? at
                : lower_temp(l, ir_load(f, entry, lower_ir_type_of(t), at)),
            t);
    }
    value = ir_call(f, entry, lower_ir_type_of(result),
                    ir_func_op(lower_callee_function(l, callee->symbol)), args,
                    count);
    free(args);
    at_out = lower_temp(l, f->params[out].temp);
    if (result->kind == TYPE_VOID) {
        /* A worker without a result writes nothing. */
    } else if (lower_is_aggregate(result)) {
        ir_memcopy(f, entry, at_out, lower_temp(l, value),
                   lower_vtype_of(l, result));
    } else {
        ir_store(f, entry, lower_ir_type_of(result), lower_temp(l, value),
                 at_out);
    }
    ir_ret(f, entry, IR_VOID, lower_none());
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
    call_worker(l, e->as.parallel.call, context, lower_temp(l, slot), 3);
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
                2);
    l->f = outer_f;
    l->b = outer_b;
    return f;
}

/* DESIGN: `dispatch` is one call of the runtime with the object, the size
   of the result, the thunk and the context. The runtime gives a handle
   back, which the Job holds in its one field. */
struct ir_operand lower_dispatch(struct lowerer *l,
                                 const struct expr *e)
{
    const struct expr *call = e->as.dispatch.call;
    const struct expr *callee = call->kind == EXPR_CALL
                                    ? call->as.call.callee : call;
    const struct type *result = callee->symbol->type->result;
    size_t extra = call->kind == EXPR_CALL ? call->as.call.arg_count : 0;
    struct ir_operand context;
    struct ir_operand args[4];
    struct ir_function *f;
    uint32_t agg;
    struct ir_operand handle;
    uint32_t out;
    char name[32];

    snprintf(name, sizeof name, "dispatch.%zu", next_thunk(l, "dispatch."));
    args[0] = lower_expr(l, e->as.dispatch.object);
    context = pack_context(l, call, extra, name, &agg);
    args[1] = result->kind == TYPE_VOID ? ir_int_op(IR_I64, 0)
                                        : lower_size_operand(l, result);
    f = dispatch_thunk(l, e, name, agg);
    args[2] = lower_temp(l, ir_addr(l->f, l->b, ir_func_op(f)));
    args[3] = context;
    handle = lower_rt_call(l, RT_FN_DISPATCH, args);
    lower_hook_object(l, HOOK_DISPATCHED, args[0]);
    out = ir_slot(l->f, l->b, lower_vtype_of(l, e->type));
    ir_store(l->f, l->b, IR_PTR, handle, lower_temp(l, out));
    return lower_temp(l, out);
}

/* `join` waits for one job and writes its result into a slot, and
   `join_all` walks the slice of jobs. */
struct ir_operand lower_join(struct lowerer *l, const struct expr *e)
{
    const struct type *job = e->as.join.job->type;
    struct ir_operand value = lower_address(l, e->as.join.job);
    struct ir_operand args[3];
    uint32_t out;

    if (e->as.join.all) {
        args[0] = lower_temp(l, ir_load(l->f, l->b, IR_PTR, value));
        args[1] = lower_temp(
            l, ir_load(l->f, l->b, IR_I64,
                       lower_offset_address(
                           l, value,
                           lower_field_offset(l, job, &lower_len_name))));
        lower_rt_call(l,
                      l->hooks ? RT_FN_JOIN_ALL_HOOKED : RT_FN_JOIN_ALL,
                      args);
        return lower_none();
    }
    args[0] = lower_temp(l, ir_load(l->f, l->b, IR_PTR, value));
    if (e->type->kind == TYPE_VOID) {
        args[1] = ir_int_op(IR_I64, 0);
        args[2] = ir_int_op(IR_PTR, 0);
        lower_rt_call(l, l->hooks ? RT_FN_JOIN_HOOKED : RT_FN_JOIN, args);
        return lower_none();
    }
    out = ir_slot(l->f, l->b, lower_vtype_of(l, e->type));
    args[1] = lower_size_operand(l, e->type);
    args[2] = lower_temp(l, out);
    lower_rt_call(l, l->hooks ? RT_FN_JOIN_HOOKED : RT_FN_JOIN, args);
    if (lower_is_aggregate(e->type)) {
        return lower_temp(l, out);
    }
    return lower_temp(l,
                      ir_load(l->f, l->b, lower_ir_type_of(e->type),
                              lower_temp(l, out)));
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
    char name[32];

    snprintf(name, sizeof name, "parallel.%zu", next_thunk(l, "parallel."));
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
    args[3] = e->as.parallel.chunks != NULL
                  ? lower_expr(l, e->as.parallel.chunks)
                  : ir_int_op(IR_I64, 0);
    args[4] = lower_size_operand(l, result);
    struct ir_function *thunk = parallel_thunk(l, e, name, agg);
    args[5] = lower_temp(l, ir_addr(l->f, l->b, ir_func_op(thunk)));
    args[6] = context;
    args[7] = lower_temp(l, results);
    args[8] = lower_temp(l, count);
    lower_rt_call(l, RT_FN_PARALLEL, args);
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

/* Locking and channels */

/* DESIGN: a channel is a struct of one handle, which names the object
   the runtime made. Each operation passes the handle to a function of
   the runtime, which holds the queue. A Mutex is the lock word itself,
   and each operation passes its address. A value that `send` puts and `recv` takes goes through a slot
   of the frame. The IR then names no size but the one of the element
   type. */

/* The handle that the channel e holds. A pointer to a channel points at
   its handle. */
struct ir_operand lower_load_handle(struct lowerer *l, const struct expr *e)
{
    struct ir_operand at = e->type->kind == TYPE_POINTER ? lower_expr(l, e)
                                                          : lower_address(l, e);

    return lower_temp(l, ir_load(l->f, l->b, IR_PTR, at));
}
