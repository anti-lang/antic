/* The owning values of lowering. One teardown, one copy and one clear of
   a value of any type, the teardown and the copy the compiler writes for
   each class, and the one preparation of a new object with its defaults.
   sema_needs_teardown is the one rule of what a teardown reaches. */

#include "lower_lowerer.h"

/* What one element or one case gets: a teardown, a copy of what it owns
   into the place at the same position of into, or a clear. */
enum work { WORK_DESTROY, WORK_COPY, WORK_CLEAR };

static void work_on(struct lowerer *l, const struct type *t,
                    struct ir_operand at, struct ir_operand into,
                    struct ir_operand from, bool made_only, enum work what)
{
    if (what == WORK_COPY) {
        lower_copy_owned(l, t, at, into);
    } else if (what == WORK_CLEAR) {
        lower_clear_owned(l, t, at);
    } else {
        lower_destroy_owned(l, t, at, from, made_only);
    }
}

/* The innermost element type of the array t. */
static const struct type *innermost(const struct type *t)
{
    while (t->kind == TYPE_ARRAY) {
        t = t->element;
    }
    return t;
}

struct ir_operand lower_array_count(struct lowerer *l, const struct type *t)
{
    struct ir_operand count = ir_int_op(IR_I64, 1);

    for (; t->kind == TYPE_ARRAY; t = t->element) {
        struct ir_operand length =
            t->length_of != NULL ? ir_sym_operand(l->m,
                                                  lower_sym_of(l, t->length_of))
                                 : ir_int_op(IR_I64, t->length);
        count = lower_temp(l,
                           ir_binary(l->f, l->b, IR_MUL, IR_I64, count,
                                     length));
    }
    return count;
}

/* DESIGN: the one loop over a run of elements. It goes last to first, as
   the locals of a block go, so every sequence is torn down in one order:
   an array at any depth, which is one run of its innermost elements, and
   the buffer of an `own` slice. A copy takes the same loop. count
   elements of type element start at base, and into is the run of a copy,
   at the same offsets. */
static void each_element(struct lowerer *l, const struct type *element,
                         struct ir_operand count, struct ir_operand base,
                         struct ir_operand into, struct ir_operand from,
                         bool made_only, enum work what)
{
    struct ir_operand size = lower_size_operand(l, element);
    struct ir_block *test = lower_new_block(l);
    struct ir_block *body = lower_new_block(l);
    struct ir_block *done = lower_new_block(l);
    uint32_t index = ir_unary(l->f, l->b, IR_COPY, IR_I64, count);
    struct ir_operand offset;
    struct ir_operand at;
    struct ir_operand copy = lower_none();

    ir_jump(l->f, l->b, test);
    l->b = test;
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_SGT, IR_I8,
                                      lower_temp(l, index),
                                      ir_int_op(IR_I64, 0))),
              body, done);
    l->b = body;
    ir_assign(l->f, l->b, index,
              lower_temp(l, ir_binary(l->f, l->b, IR_SUB, IR_I64,
                                      lower_temp(l, index),
                                      ir_int_op(IR_I64, 1))));
    offset = lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64,
                                     lower_temp(l, index), size));
    at = lower_temp(l, ir_ptradd(l->f, l->b, base, offset));
    if (what == WORK_COPY) {
        copy = lower_temp(l, ir_ptradd(l->f, l->b, into, offset));
    }
    work_on(l, element, at, copy, from, made_only, what);
    ir_jump(l->f, l->b, test);
    l->b = done;
}

/* The work on every element of the array t at at. */
static void each_array_element(struct lowerer *l, const struct type *t,
                               struct ir_operand at, struct ir_operand into,
                               struct ir_operand from, bool made_only,
                               enum work what)
{
    struct ir_operand count = lower_array_count(l, t);

    each_element(l, innermost(t), count, at, into, from, made_only, what);
}

static bool lower_copies_parts(const struct type *t);

/* DESIGN: a variant tears down, copies or clears the fields of the case
   its tag names, and nothing else, since the bytes of the other cases are
   never written. The tag is read once, and each case whose struct owns
   something is one branch. A variant at into of a copy holds the same tag
   and bytes already. */
static void each_case(struct lowerer *l, const struct type *t,
                      struct ir_operand at, struct ir_operand into,
                      struct ir_operand from, bool made_only, enum work what)
{
    struct ir_block *after = lower_new_block(l);
    struct ir_operand tag = lower_load_tag(l, t, at);
    enum ir_type tag_type = lower_ir_type_of(t->base);
    size_t i;

    for (i = 0; i < t->param_count; i++) {
        const struct type *payload = t->params[i];
        struct ir_block *held;
        struct ir_block *next;
        struct ir_operand fields;
        struct ir_operand copy = lower_none();
        if (payload == NULL ||
            (what == WORK_COPY ? !lower_copies_parts(payload)
                               : !sema_needs_teardown(payload))) {
            continue;
        }
        held = lower_new_block(l);
        next = lower_new_block(l);
        ir_branch(l->f, l->b,
                  lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, tag,
                                          ir_int_op(tag_type,
                                                    t->base->fields[i]
                                                        .number))),
                  held, next);
        l->b = held;
        fields = lower_case_address(l, t, at);
        if (what == WORK_COPY) {
            copy = lower_case_address(l, t, into);
        }
        work_on(l, payload, fields, copy, from, made_only, what);
        ir_jump(l->f, l->b, after);
        l->b = next;
    }
    ir_jump(l->f, l->b, after);
    l->b = after;
}

/* Branch to a new block when the flag of the `?T` at at is set, and give
   the block after it, where both paths meet. */
static struct ir_block *when_held(struct lowerer *l, const struct type *t,
                                  struct ir_operand at)
{
    struct ir_block *held = lower_new_block(l);
    struct ir_block *after = lower_new_block(l);

    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_NE, IR_I8,
                                      lower_optional_flag(l, t, at),
                                      ir_int_op(IR_I8, 0))),
              held, after);
    l->b = held;
    return after;
}

/* Branch to a new block when the pointer p is not `none`, and give the
   block after it. */
static struct ir_block *when_set(struct lowerer *l, struct ir_operand p)
{
    struct ir_block *set = lower_new_block(l);
    struct ir_block *after = lower_new_block(l);

    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_NE, IR_I8, p,
                                      ir_int_op(IR_PTR, 0))),
              set, after);
    l->b = set;
    return after;
}

/* Close the block that when_held or when_set opened. */
static void join(struct lowerer *l, struct ir_block *after)
{
    ir_jump(l->f, l->b, after);
    l->b = after;
}

static void lower_free_snapshot(struct lowerer *l, const struct type *t,
                                struct ir_operand pair);

void lower_destroy_owned(struct lowerer *l, const struct type *t,
                         struct ir_operand at, struct ir_operand from,
                         bool made_only)
{
    struct ir_operand args[2];
    struct ir_block *after;
    size_t i;

    switch (t->kind) {
    case TYPE_CLASS:
        after = NULL;
        if (made_only) {
            after = lower_when_made(l, at);
        } else {
            lower_check_table(l, lower_temp(l, ir_load(l->f, l->b, IR_PTR, at)),
                              t);
        }
        args[0] = at;
        args[1] = from;
        ir_call(l->f, l->b, IR_VOID,
                ir_func_op(lower_class_function(l, t, "destroy")), args, 2);
        if (after != NULL) {
            join(l, after);
        }
        return;
    case TYPE_OPTIONAL:
        after = when_held(l, t, at);
        lower_destroy_owned(l, t->element, at, from, made_only);
        join(l, after);
        return;
    case TYPE_ARRAY:
        each_array_element(l, t, at, lower_none(), from, made_only,
                           WORK_DESTROY);
        return;
    case TYPE_FN:
        lower_free_snapshot(l, t, at);
        return;
    case TYPE_STRUCT:
    case TYPE_TUPLE:
        /* The parts go last to first, as the locals of a block do. */
        for (i = t->field_count; i > 0; i--) {
            const struct struct_field *f = &t->fields[i - 1];
            if ((f->form == FIELD_PLAIN || f->form == FIELD_USE) &&
                sema_needs_teardown(f->type)) {
                lower_destroy_owned(
                    l, f->type,
                    lower_offset_address(l, at,
                                         lower_field_offset(l, t, &f->name)),
                    from, made_only);
            }
        }
        return;
    case TYPE_VARIANT:
        each_case(l, t, at, lower_none(), from, made_only, WORK_DESTROY);
        return;
    default:
        return;
    }
}

void lower_clear_owned(struct lowerer *l, const struct type *t,
                       struct ir_operand at)
{
    size_t i;

    switch (t->kind) {
    case TYPE_CLASS:
        ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0), at);
        return;
    case TYPE_OPTIONAL:
        lower_set_optional(l, t, NULL, at, 0);
        return;
    case TYPE_FN:
        if (t->owned) {
            ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0),
                     lower_context_word(l, t, at));
        }
        return;
    case TYPE_ARRAY:
        each_array_element(l, t, at, lower_none(), lower_none(), false,
                           WORK_CLEAR);
        return;
    case TYPE_STRUCT:
    case TYPE_TUPLE:
        for (i = 0; i < t->field_count; i++) {
            const struct struct_field *f = &t->fields[i];
            if ((f->form == FIELD_PLAIN || f->form == FIELD_USE) &&
                sema_needs_teardown(f->type)) {
                lower_clear_owned(
                    l, f->type,
                    lower_offset_address(l, at,
                                         lower_field_offset(l, t, &f->name)));
            }
        }
        return;
    case TYPE_VARIANT:
        each_case(l, t, at, lower_none(), lower_none(), false, WORK_CLEAR);
        return;
    default:
        return;
    }
}

/* The copy of the class value t, the one its chain declares or the one
   the compiler writes. */
static struct ir_function *copy_of(struct lowerer *l, const struct type *t)
{
    const struct item *m = lower_declared_copy(t);

    return m != NULL ? lower_callee_function(l, m->symbol)
                     : lower_class_function(l, t, "copy");
}

void lower_copy_owned(struct lowerer *l, const struct type *t,
                      struct ir_operand from, struct ir_operand into)
{
    struct ir_operand args[2];
    struct ir_block *after;
    size_t i;

    switch (t->kind) {
    case TYPE_CLASS:
        lower_check_table(l, lower_temp(l, ir_load(l->f, l->b, IR_PTR, from)),
                          t);
        args[0] = from;
        args[1] = into;
        ir_call(l->f, l->b, IR_VOID, ir_func_op(copy_of(l, t)), args, 2);
        return;
    case TYPE_OPTIONAL:
        after = when_held(l, t, from);
        lower_copy_owned(l, t->element, from, into);
        join(l, after);
        return;
    case TYPE_ARRAY:
        each_array_element(l, t, from, into, lower_none(), false, WORK_COPY);
        return;
    case TYPE_FN:
        if (t->owned) {
            lower_dup_snapshot(l, t, from, into);
        }
        return;
    case TYPE_STRUCT:
    case TYPE_TUPLE:
        for (i = 0; i < t->field_count; i++) {
            const struct struct_field *f = &t->fields[i];
            struct ir_operand offset;
            struct ir_operand part;
            struct ir_operand copy;
            if ((f->form != FIELD_PLAIN && f->form != FIELD_USE) ||
                !lower_copies_parts(f->type)) {
                continue;
            }
            /* Each address is bound first, since C leaves the order of
               two calls in one argument list open. */
            offset = lower_field_offset(l, t, &f->name);
            part = lower_offset_address(l, from, offset);
            copy = lower_offset_address(l, into, offset);
            lower_copy_owned(l, f->type, part, copy);
        }
        return;
    case TYPE_VARIANT:
        each_case(l, t, from, into, lower_none(), false, WORK_COPY);
        return;
    default:
        return;
    }
}

/* DESIGN: a copy reaches more parts than a teardown. A class value is
   copied by the copy of its class wherever it stands, whether or not it
   owns something, since its chain may declare `copy` and the copy gives
   a lock and a `transient` field fresh values. An `own fn` copies its
   snapshot. Every other part follows sema_needs_teardown: a struct, a
   tuple, a variant, an array or a `?T` copies what its parts copy, and a
   union copies its bytes alone. */
static bool lower_copies_parts(const struct type *t)
{
    size_t i;

    while (t->kind == TYPE_ARRAY || t->kind == TYPE_OPTIONAL) {
        t = t->element;
    }
    if (t->kind == TYPE_CLASS) {
        return true;
    }
    if (t->kind == TYPE_FN) {
        return t->owned;
    }
    if (t->kind == TYPE_VARIANT) {
        for (i = 0; i < t->param_count; i++) {
            if (t->params[i] != NULL && lower_copies_parts(t->params[i])) {
                return true;
            }
        }
        return false;
    }
    if ((t->kind != TYPE_STRUCT && t->kind != TYPE_TUPLE) || t->is_union) {
        return false;
    }
    for (i = 0; i < t->field_count; i++) {
        const struct struct_field *f = &t->fields[i];
        if ((f->form == FIELD_PLAIN || f->form == FIELD_USE) &&
            lower_copies_parts(f->type)) {
            return true;
        }
    }
    return false;
}

/* DESIGN: an `own fn` frees its snapshot with its owner and leaves
   `none` in the context word. A function that captures nothing holds
   `none` there, and the runtime frees nothing for it. The snapshot is
   memory of the C library whatever allocator the owner came from. */
static void lower_free_snapshot(struct lowerer *l, const struct type *t,
                                struct ir_operand pair)
{
    struct ir_operand word = lower_context_word(l, t, pair);
    struct ir_operand snapshot =
        lower_temp(l, ir_load(l->f, l->b, IR_PTR, word));

    lower_rt_call(l, RT_FN_SNAPSHOT_FREE, &snapshot);
    ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0), word);
}

/* The copy of the `own fn` of type t at from into the pair at into: the
   code, and a copy of the snapshot, byte for byte. */
void lower_dup_snapshot(struct lowerer *l, const struct type *t,
                        struct ir_operand from, struct ir_operand into)
{
    struct ir_operand snapshot = lower_temp(
        l, ir_load(l->f, l->b, IR_PTR, lower_context_word(l, t, from)));
    struct ir_operand made;

    ir_store(l->f, l->b, IR_PTR,
             lower_temp(l, ir_load(l->f, l->b, IR_PTR, from)), into);
    made = lower_rt_call(l, RT_FN_SNAPSHOT_DUP, &snapshot);
    ir_store(l->f, l->b, IR_PTR, made, lower_context_word(l, t, into));
}

/* Whether the field f of a class is an `own` pointer or slice, which
   owns the buffer it points at. */
static bool owns_buffer(const struct struct_field *f)
{
    return f->owned &&
           (f->type->kind == TYPE_POINTER || f->type->kind == TYPE_SLICE);
}

/* DESIGN: an `own` pointer or slice owns its buffer and what each
   element owns. An object behind a pointer to a class is deleted through
   its table, since it may be of a class below the one the field names.
   Any other element is torn down in place by the one teardown, the
   elements of a slice last to first, and the buffer then goes back to
   from. A pointer that is `none` holds nothing. The field holds `none`
   afterwards, so a second teardown frees nothing. */
static void destroy_buffer(struct lowerer *l, const struct type *t,
                           struct ir_operand at, struct ir_operand from)
{
    const struct type *element = t->element;
    struct ir_operand v = lower_temp(l, ir_load(l->f, l->b, IR_PTR, at));
    struct ir_operand args[3];

    if (t->kind == TYPE_POINTER && element->kind == TYPE_CLASS) {
        args[0] = v;
        args[1] = lower_static_descriptor(l, element);
        args[2] = from;
        lower_rt_call(l, RT_FN_DELETE_FROM, args);
    } else {
        if (sema_needs_teardown(element)) {
            struct ir_block *after = when_set(l, v);
            struct ir_operand count = t->kind == TYPE_SLICE
                                          ? lower_slice_length(l, at, t)
                                          : ir_int_op(IR_I64, 1);
            each_element(l, element, count, v, lower_none(), from, false,
                         WORK_DESTROY);
            join(l, after);
        }
        args[0] = from;
        args[1] = v;
        lower_rt_call(l, RT_FN_GIVE, args);
    }
    ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0), at);
}

/* DESIGN: the copy of an `own` pointer or slice gets a buffer of its own.
   An object behind a pointer to a class is copied through its table. Any
   other buffer is copied byte for byte, and then each element copies
   what it owns by the one copy, as an element of an array does. A buffer
   that is `none` or empty gives `none`. */
static void copy_buffer(struct lowerer *l, const struct type *t,
                        struct ir_operand from, struct ir_operand into)
{
    const struct type *element = t->element;
    struct ir_operand v = lower_temp(l, ir_load(l->f, l->b, IR_PTR, from));
    struct ir_operand made;

    if (t->kind == TYPE_POINTER && element->kind == TYPE_CLASS) {
        made = lower_object_call(l, RT_FN_DUP, v, element);
    } else {
        struct ir_operand count = t->kind == TYPE_SLICE
                                      ? lower_slice_length(l, from, t)
                                      : ir_int_op(IR_I64, 1);
        struct ir_operand args[2];
        args[0] = v;
        args[1] = lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64, count,
                                          lower_size_operand(l, element)));
        made = lower_rt_call(l, RT_FN_COPY_BUFFER, args);
        if (lower_copies_parts(element)) {
            struct ir_block *after = when_set(l, made);
            each_element(l, element, count, v, made, lower_none(), false,
                         WORK_COPY);
            join(l, after);
        }
    }
    ir_store(l->f, l->b, IR_PTR, made, into);
}

/* The address of the field f of level up in the object at self. */
static struct ir_operand field_at(struct lowerer *l, const struct type *up,
                                  const struct struct_field *f,
                                  struct ir_operand self)
{
    return lower_offset_address(l, self, lower_field_offset(l, up, &f->name));
}

/* The teardown of the field f of level up, in the object at self. Owned
   memory goes back to the allocator from. */
static void teardown_field(struct lowerer *l, const struct type *up,
                           const struct struct_field *f,
                           struct ir_operand self, struct ir_operand from)
{
    if (f->form != FIELD_PLAIN && f->form != FIELD_USE) {
        return;
    }
    if (owns_buffer(f)) {
        destroy_buffer(l, f->type, field_at(l, up, f, self), from);
        return;
    }
    if (sema_needs_teardown(f->type)) {
        lower_destroy_owned(l, f->type, field_at(l, up, f, self), from,
                            false);
    }
}

/* DESIGN: the teardown runs the destruct body of every level, concrete
   first. It then destroys what each level owns, so a body still reads
   what it owns. The fields go last to first, the concrete level first,
   which is the reverse of the order of the object in memory and the
   order the parts of a struct go in. Every field takes the one teardown
   of its type, and an `own` pointer or slice its buffer as well. A class
   value held inline runs its own teardown. Its table is never zero in an
   object a literal or `construct` made, so a zero one traps, as every
   zero table does. */
void lower_class_teardown(struct lowerer *l, const struct type *t)
{
    static const struct name destruct_name = {"destruct", 8};
    struct ir_function *f = lower_class_function(l, t, "destroy");
    const struct type *up;
    struct ir_operand self;
    struct ir_operand from;
    size_t i;

    l->f = f;
    l->b = ir_block_add(f);
    self = lower_temp(l, f->params[0].temp);
    from = lower_temp(l, f->params[1].temp);
    lower_hook_object(l, HOOK_DESTROYED, self);
    for (up = t; up->base != NULL; up = up->base) {
        const struct item *m = lower_level_fn(up, &destruct_name);
        if (m != NULL) {
            struct ir_operand arg = self;
            ir_call(l->f, l->b, IR_VOID,
                    ir_func_op(lower_callee_function(l, m->symbol)), &arg, 1);
        }
    }
    for (up = t; up->base != NULL; up = up->base) {
        for (i = up->field_count; i > 0; i--) {
            teardown_field(l, up, &up->fields[i - 1], self, from);
        }
    }
    ir_ret(l->f, l->b, IR_VOID, lower_none());
}

/* The copy of the field f of level up, from the object at self into the
   one at to, whose bytes are already the same. */
static void copy_field(struct lowerer *l, const struct type *up,
                       const struct struct_field *f, struct ir_operand self,
                       struct ir_operand to)
{
    struct ir_operand offset;
    struct ir_operand from;
    struct ir_operand into;

    if (f->form != FIELD_PLAIN && f->form != FIELD_USE) {
        return;
    }
    /* A lock of the object is its own, and the copy gets one that is
       free, whatever the lock of the original held. */
    if (types_is_mutex(f->type) || types_is_object_lock(f->type)) {
        lower_zero_lock(l, f->type, field_at(l, up, f, to));
        return;
    }
    /* A `transient` field is derived state, and the copy derives its
       own. */
    if (f->transient) {
        ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0),
                 field_at(l, up, f, to));
        return;
    }
    if (!owns_buffer(f) && !lower_copies_parts(f->type)) {
        return;
    }
    /* Each address is bound first, since C leaves the order of two calls
       in one argument list open. */
    offset = lower_field_offset(l, up, &f->name);
    from = lower_offset_address(l, self, offset);
    into = lower_offset_address(l, to, offset);
    if (owns_buffer(f)) {
        copy_buffer(l, f->type, from, into);
        return;
    }
    lower_copy_owned(l, f->type, from, into);
}

/* DESIGN: the copy starts from every byte of the object. It then gives
   each `own` field fresh memory with a copy of its contents, and each
   part the one copy of its type. The object behind a pointer is copied,
   and so is what each element of a buffer owns. A class value held
   inline copies itself with its own copy. Other pointers keep the
   address. */
void lower_class_copy(struct lowerer *l, const struct type *t)
{
    struct ir_function *f = lower_class_function(l, t, "copy");
    const struct type *up;
    struct ir_operand self;
    struct ir_operand to;
    size_t i;

    l->f = f;
    l->b = ir_block_add(f);
    self = lower_temp(l, f->params[0].temp);
    to = lower_temp(l, f->params[1].temp);
    ir_memcopy(l->f, l->b, to, self, lower_vtype_of(l, t));
    for (up = t; up->base != NULL; up = up->base) {
        for (i = 0; i < up->field_count; i++) {
            copy_field(l, up, &up->fields[i], self, to);
        }
    }
    ir_ret(l->f, l->b, IR_VOID, lower_none());
}

/* Whether the literal lit names the field name. */
static bool literal_gives(const struct expr *lit, const struct name *name)
{
    size_t k;

    for (k = 0; lit != NULL && k < lit->as.struct_lit.field_count; k++) {
        if (lower_same_name(&lit->as.struct_lit.fields[k].name, name)) {
            return true;
        }
    }
    return false;
}

/* DESIGN: every new object is prepared by this one function, whatever
   made it: a literal, `T(args)`, the init that an inline field, C and
   `reflect.new` call, and the `get` of a singleton. It writes the table
   pointers, then the fields the literal lit names, then the default of
   every other field of the chain. A bitfield goes into its unit. An
   `own fn` field without a default holds no snapshot, since `=` into it
   in `construct` frees the one it held, and the memory of a frame or of
   `malloc` holds whatever it held. A struct literal takes the same path
   without the tables. lit is NULL where no literal names a field. */
void lower_prepare_object(struct lowerer *l, const struct type *t,
                          const struct expr *lit, struct ir_operand dest)
{
    const struct type *up;
    size_t i;

    /* The table pointer is the first word of every object, and the base
       of a class sits at offset 0, so it goes at dest. */
    if (t->kind == TYPE_CLASS) {
        ir_store(l->f, l->b, IR_PTR,
                 lower_temp(l, ir_addr(l->f, l->b,
                                       ir_global_op(lower_class_table(l, t)))),
                 dest);
        lower_store_interface_tables(l, t, dest);
    }
    for (i = 0; lit != NULL && i < lit->as.struct_lit.field_count; i++) {
        const struct field_init *init = &lit->as.struct_lit.fields[i];
        const struct type *at = lower_field_owner(t, &init->name);
        const struct struct_field *field = type_find_field(at, &init->name);
        if (field->bits != 0) {
            struct ir_operand v = lower_expr(l, init->value);
            ir_bitstore(l->f, l->b, lower_ir_type_of(field->type), v, dest,
                        lower_agg_of(l, at), (uint32_t)(field - at->fields));
            continue;
        }
        lower_store_value(l, field->type, init->value,
                          field_at(l, at, field, dest));
    }
    /* A field the literal leaves out has a default, which the checker
       required, so the value is complete however it was made. */
    for (up = t; up != NULL; up = up->kind == TYPE_CLASS ? up->base : NULL) {
        for (i = 0; i < up->field_count; i++) {
            const struct struct_field *field = &up->fields[i];
            if (literal_gives(lit, &field->name)) {
                continue;
            }
            if (lower_has_default(field)) {
                lower_store_field_default(l, up, i, dest);
            } else if (field->type->kind == TYPE_FN && field->type->owned) {
                struct ir_operand at = field_at(l, up, field, dest);
                ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0), at);
                ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0),
                         lower_context_word(l, field->type, at));
            }
        }
    }
}
