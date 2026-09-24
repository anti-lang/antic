#include "ast.h"

#include <string.h>

#include "sema.h"
#include "types.h"

/* Node names follow the rules of the chapter 2 grammar, so a dump reads
   like the grammar that produced it. A typed dump adds the checked type
   of a node at column 28. */

enum { TYPE_COLUMN = 27 };

struct dumper {
    struct text *out;
    bool typed;
};

/* Start a line at depth and return its offset in the output. */
static size_t begin(struct dumper *d, int depth)
{
    size_t start = d->out->length;
    int i;

    for (i = 0; i < depth; i++) {
        text_append(d->out, "  ");
    }
    return start;
}

/* End a line, with the type at TYPE_COLUMN in a typed dump. */
static void end(struct dumper *d, size_t start, const struct type *type)
{
    if (d->typed && type != NULL) {
        size_t width = d->out->length - start;
        do {
            text_append(d->out, " ");
            width++;
        } while (width < TYPE_COLUMN);
        type_name(d->out, type);
    }
    text_append(d->out, "\n");
}

static void label_name(struct dumper *d, const char *label,
                       const struct name *module, const struct name *name)
{
    text_appendf(d->out, "%s ", label);
    if (module != NULL && module->length > 0) {
        text_appendf(d->out, "%.*s.", (int)module->length, module->text);
    }
    text_appendf(d->out, "%.*s", (int)name->length, name->text);
}

/* Print doc text in quotes, with newlines, quotes and backslashes escaped
   so the text stays on one line. */
static void dump_doc(struct dumper *d, int depth, const char *label,
                     const struct doc_text *doc)
{
    size_t i;

    if (doc->length == 0) {
        return;
    }
    begin(d, depth);
    text_appendf(d->out, "%s \"", label);
    for (i = 0; i < doc->length; i++) {
        char c = doc->text[i];
        if (c == '\n') {
            text_append(d->out, "\\n");
        } else if (c == '"' || c == '\\') {
            text_appendf(d->out, "\\%c", c);
        } else {
            text_append_bytes(d->out, &c, 1);
        }
    }
    text_append(d->out, "\"\n");
}

static void simple(struct dumper *d, int depth, const char *label,
                   const struct type *type)
{
    size_t start = begin(d, depth);
    text_append(d->out, label);
    end(d, start, type);
}

/* The spelling of an operator token without its backticks. */
static void dump_handler(struct dumper *d, int depth,
                         const struct handler *h);

static void op_name(struct dumper *d, enum token_kind op)
{
    const char *quoted = token_kind_name(op);
    text_appendf(d->out, "%.*s", (int)(strlen(quoted) - 2), quoted + 1);
}

static void dump_expr(struct dumper *d, int depth, const struct expr *e);
static void dump_block(struct dumper *d, int depth, const struct block *b);
static void dump_params(struct dumper *d, int depth, const char *label,
                        const struct param *params, size_t count,
                        const struct type *owner);

static void dump_type(struct dumper *d, int depth, const struct type_expr *t)
{
    size_t start = begin(d, depth);
    size_t i;

    switch (t->kind) {
    case TYPEX_BUILTIN:
        text_append(d->out, "type ");
        op_name(d, t->builtin);
        end(d, start, NULL);
        break;
    case TYPEX_NAMED:
        label_name(d, "type", &t->module, &t->name);
        end(d, start, NULL);
        break;
    case TYPEX_POINTER:
        text_append(d->out, t->nullable ? "type ?*" : "type *");
        end(d, start, NULL);
        dump_type(d, depth + 1, t->element);
        break;
    case TYPEX_SLICE:
        text_append(d->out, "type []");
        end(d, start, NULL);
        dump_type(d, depth + 1, t->element);
        break;
    case TYPEX_ARRAY:
        text_append(d->out, "type [N]");
        end(d, start, NULL);
        dump_expr(d, depth + 1, t->length);
        dump_type(d, depth + 1, t->element);
        break;
    case TYPEX_FN:
        text_append(d->out, t->keep ? "type keep fn"
                            : t->concurrent ? "type concurrent fn"
                                            : "type fn");
        end(d, start, NULL);
        for (i = 0; i < t->param_count; i++) {
            dump_type(d, depth + 1, t->params[i]);
        }
        if (t->result != NULL) {
            simple(d, depth + 1, "result", NULL);
            dump_type(d, depth + 2, t->result);
        }
        break;
    case TYPEX_TUPLE:
        text_append(d->out, "type tuple");
        end(d, start, NULL);
        for (i = 0; i < t->param_count; i++) {
            dump_type(d, depth + 1, t->params[i]);
        }
        break;
    case TYPEX_CHAN:
        text_append(d->out, "type chan");
        end(d, start, NULL);
        dump_type(d, depth + 1, t->element);
        break;
    }
}

/* The grammar rule of a binary operator's level. */
static const char *binary_rule(enum token_kind op)
{
    switch (op) {
    case TOKEN_OR_OR: return "or_expr";
    case TOKEN_AND_AND: return "and_expr";
    case TOKEN_PIPE: return "bit_or";
    case TOKEN_CARET: return "bit_xor";
    case TOKEN_AMP: return "bit_and";
    case TOKEN_EQ:
    case TOKEN_NE: return "equality";
    case TOKEN_LT:
    case TOKEN_LE:
    case TOKEN_GT:
    case TOKEN_GE: return "relation";
    case TOKEN_QUESTION_QUESTION: return "coalesce";
    case TOKEN_SHL:
    case TOKEN_SHL_WRAP:
    case TOKEN_SHR: return "shift";
    case TOKEN_PLUS:
    case TOKEN_PLUS_WRAP:
    case TOKEN_PLUS_SAT:
    case TOKEN_MINUS:
    case TOKEN_MINUS_WRAP:
    case TOKEN_MINUS_SAT: return "additive";
    case TOKEN_MUL_HIGH: return "mul_high";
    default: return "multiplicative";
    }
}

static void dump_fields(struct dumper *d, int depth,
                        const struct field_init *fields, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        size_t start = begin(d, depth);
        label_name(d, "field_init", NULL, &fields[i].name);
        end(d, start, NULL);
        dump_expr(d, depth + 1, fields[i].value);
    }
}

static void dump_expr(struct dumper *d, int depth, const struct expr *e)
{
    size_t start = begin(d, depth);
    const struct type *type = e->type;
    size_t i;

    switch (e->kind) {
    case EXPR_INT:
        text_appendf(d->out, "int_lit %llu", (unsigned long long)e->as.integer);
        end(d, start, type);
        break;
    case EXPR_FLOAT:
        text_appendf(d->out, "float_lit %.*s", (int)e->as.text.length,
                     e->as.text.bytes);
        end(d, start, type);
        break;
    case EXPR_CHAR:
        text_appendf(d->out, "char_lit %.*s", (int)e->spelling.length,
                     e->spelling.bytes);
        end(d, start, type);
        break;
    case EXPR_DESCRIPTOR:
        text_appendf(d->out, "descriptor %.*s",
                     (int)e->as.descriptor_of->name.length,
                     e->as.descriptor_of->name.text);
        end(d, start, type);
        break;
    /* An anonymous function shows its parameters, its result and its
       body, as a function item does. */
    case EXPR_FN:
        text_append(d->out, "fn_expr");
        end(d, start, type);
        dump_params(d, depth + 1, "param", e->as.fn->params,
                    e->as.fn->param_count, NULL);
        if (e->as.fn->result != NULL) {
            simple(d, depth + 1, "result", NULL);
            dump_type(d, depth + 2, e->as.fn->result);
        }
        dump_block(d, depth + 1, e->as.fn->body);
        break;
    /* The iterator that `to_slice` walks follows as a child. */
    case EXPR_COLLECT:
        text_append(d->out, "to_slice");
        end(d, start, type);
        dump_expr(d, depth + 1, e->as.collect.start);
        break;
    case EXPR_STRING:
    case EXPR_BYTES:
        text_appendf(d->out, "string_lit %.*s", (int)e->spelling.length,
                     e->spelling.bytes);
        end(d, start, type);
        break;
    case EXPR_PATTERN:
        text_appendf(d->out, "pattern_lit %.*s", (int)e->spelling.length,
                     e->spelling.bytes);
        end(d, start, type);
        break;
    case EXPR_BOOL:
        text_append(d->out, e->as.boolean ? "true" : "false");
        end(d, start, type);
        break;
    case EXPR_NONE:
        text_append(d->out, "none");
        end(d, start, type);
        break;
    case EXPR_HERE:
        text_append(d->out, "here");
        end(d, start, type);
        break;
    /* The values of the `{expr}` parts follow as children, in order, and
       the allocator of `f"..."(from)` after them. */
    case EXPR_FORMAT:
        text_appendf(d->out, "format_lit %.*s", (int)e->spelling.length,
                     e->spelling.bytes);
        end(d, start, type);
        for (i = 0; i < e->as.format.count; i++) {
            if (e->as.format.parts[i].value != NULL) {
                dump_expr(d, depth + 1, e->as.format.parts[i].value);
            }
        }
        break;
    case EXPR_NAME:
        label_name(d, "ident", NULL, &e->as.name);
        end(d, start, type);
        break;
    case EXPR_UNARY:
        text_append(d->out, "unary ");
        op_name(d, e->as.unary.op);
        end(d, start, type);
        dump_expr(d, depth + 1, e->as.unary.operand);
        break;
    case EXPR_BINARY:
        text_appendf(d->out, "%s ", binary_rule(e->as.binary.op));
        op_name(d, e->as.binary.op);
        end(d, start, type);
        dump_expr(d, depth + 1, e->as.binary.left);
        dump_expr(d, depth + 1, e->as.binary.right);
        break;
    case EXPR_IN:
        text_append(d->out, "in_expr");
        end(d, start, type);
        dump_expr(d, depth + 1, e->as.in.value);
        dump_expr(d, depth + 1, e->as.in.low);
        dump_expr(d, depth + 1, e->as.in.high);
        break;
    case EXPR_CAST:
        text_append(d->out, "cast");
        end(d, start, type);
        dump_expr(d, depth + 1, e->as.cast.operand);
        dump_type(d, depth + 1, e->as.cast.type);
        break;
    case EXPR_CALL:
        text_append(d->out, "call");
        end(d, start, type);
        dump_expr(d, depth + 1, e->as.call.callee);
        for (i = 0; i < e->as.call.arg_count; i++) {
            dump_expr(d, depth + 1, e->as.call.args[i]);
        }
        dump_handler(d, depth + 1, &e->as.call.handler);
        break;
    case EXPR_INDEX:
        text_append(d->out, "index");
        end(d, start, type);
        dump_expr(d, depth + 1, e->as.index.base);
        dump_expr(d, depth + 1, e->as.index.index);
        break;
    case EXPR_SLICE:
        text_append(d->out, "slice");
        end(d, start, type);
        dump_expr(d, depth + 1, e->as.slice.base);
        dump_expr(d, depth + 1, e->as.slice.low);
        dump_expr(d, depth + 1, e->as.slice.high);
        break;
    case EXPR_FIELD:
        label_name(d, e->as.field.optional ? "optional_field" : "field", NULL,
                   &e->as.field.name);
        end(d, start, type);
        dump_expr(d, depth + 1, e->as.field.base);
        break;
    /* The checked form of `?.`: the value, then the field or the call
       that reads it. */
    case EXPR_OPTIONAL:
        text_append(d->out, "optional");
        end(d, start, type);
        dump_expr(d, depth + 1, e->as.optional.base);
        dump_expr(d, depth + 1, e->as.optional.access);
        break;
    case EXPR_STRUCT_LIT:
        label_name(d, "struct_lit", &e->as.struct_lit.module,
                   &e->as.struct_lit.name);
        end(d, start, type);
        dump_fields(d, depth + 1, e->as.struct_lit.fields,
                    e->as.struct_lit.field_count);
        break;
    case EXPR_SLICE_LIT: {
        size_t inner;
        text_append(d->out, "slice_lit");
        end(d, start, type);
        inner = begin(d, depth + 1);
        text_append(d->out, "type []");
        end(d, inner, NULL);
        dump_type(d, depth + 2, e->as.slice_lit.element);
        dump_fields(d, depth + 1, e->as.slice_lit.fields,
                    e->as.slice_lit.field_count);
        break;
    }
    case EXPR_ARRAY_LIT:
        text_append(d->out, "array_lit");
        end(d, start, type);
        for (i = 0; i < e->as.array_lit.count; i++) {
            dump_expr(d, depth + 1, e->as.array_lit.elements[i]);
        }
        break;
    case EXPR_TUPLE:
        text_append(d->out, "tuple");
        end(d, start, type);
        for (i = 0; i < e->as.tuple.count; i++) {
            dump_expr(d, depth + 1, e->as.tuple.elements[i]);
        }
        break;
    case EXPR_ARRAY_REPEAT:
        text_append(d->out, "array_lit ;");
        end(d, start, type);
        dump_expr(d, depth + 1, e->as.array_repeat.value);
        dump_expr(d, depth + 1, e->as.array_repeat.count);
        break;
    case EXPR_ALLOC:
        text_append(d->out, "alloc");
        end(d, start, type);
        dump_type(d, depth + 1, e->as.alloc.type);
        dump_expr(d, depth + 1, e->as.alloc.count);
        break;
    case EXPR_FREE:
        text_append(d->out, "free");
        end(d, start, NULL);
        dump_expr(d, depth + 1, e->as.free_pointer);
        break;
    case EXPR_ATOMIC: {
        static const char *const names[] = {
            "load", "store", "swap", "add", "sub", "and", "or",
            "compare_swap"
        };
        text_appendf(d->out, "atomic %s", names[e->as.atomic.op]);
        end(d, start, type);
        dump_expr(d, depth + 1, e->as.atomic.place);
        if (e->as.atomic.a != NULL) {
            dump_expr(d, depth + 1, e->as.atomic.a);
        }
        if (e->as.atomic.b != NULL) {
            dump_expr(d, depth + 1, e->as.atomic.b);
        }
        break;
    }
    case EXPR_OBJECT:
        text_append(d->out, e->as.object.op == TOKEN_DUP      ? "dup"
                            : e->as.object.op == TOKEN_DELETE ? "delete"
                                                              : "destroy");
        end(d, start, type);
        dump_expr(d, depth + 1, e->as.object.operand);
        if (e->as.object.from != NULL) {
            dump_expr(d, depth + 1, e->as.object.from);
        }
        break;
    case EXPR_SIZE_OF:
        text_append(d->out, "size_of");
        end(d, start, type);
        dump_type(d, depth + 1, e->as.size_of);
        break;
    case EXPR_PARALLEL:
        text_append(d->out,
                    e->as.parallel.chunks != NULL ? "parallel by"
                                                  : "parallel");
        end(d, start, type);
        dump_expr(d, depth + 1, e->as.parallel.array);
        if (e->as.parallel.chunks != NULL) {
            dump_expr(d, depth + 1, e->as.parallel.chunks);
        }
        dump_expr(d, depth + 1, e->as.parallel.call);
        break;
    case EXPR_DISPATCH:
        text_append(d->out, "dispatch");
        end(d, start, type);
        dump_expr(d, depth + 1, e->as.dispatch.object);
        dump_expr(d, depth + 1, e->as.dispatch.call);
        break;
    case EXPR_JOIN:
        text_append(d->out, e->as.join.all ? "join_all" : "join");
        end(d, start, type);
        dump_expr(d, depth + 1, e->as.join.job);
        break;
    case EXPR_SYNC_OP: {
        static const char *const names[] = {
            "mutex_new", "mutex_destroy", "chan", "send", "recv", "close",
            "delete"
        };
        text_append(d->out, names[e->as.sync_op.op]);
        end(d, start, type);
        if (e->as.sync_op.element != NULL) {
            dump_type(d, depth + 1, e->as.sync_op.element);
        }
        if (e->as.sync_op.target != NULL) {
            dump_expr(d, depth + 1, e->as.sync_op.target);
        }
        if (e->as.sync_op.value != NULL) {
            dump_expr(d, depth + 1, e->as.sync_op.value);
        }
        break;
    }
    case EXPR_SIMD: {
        static const char *const names[] = {
            "splat", "load", "store", "shuffle", "sum", "min", "max", "dot",
            "select", "any", "all"
        };
        text_appendf(d->out, "simd %s", names[e->as.simd.op]);
        if (e->as.simd.lanes != NULL) {
            for (i = 0; i < e->as.simd.simd->field_count; i++) {
                text_appendf(d->out, " %u", e->as.simd.lanes[i]);
            }
        }
        end(d, start, type);
        for (i = 0; i < e->as.simd.arg_count; i++) {
            dump_expr(d, depth + 1, e->as.simd.args[i]);
        }
        break;
    }
    }
}

/* `catch e { }`, `catch fatal` or the `try` form. */
static void dump_handler(struct dumper *d, int depth,
                         const struct handler *h)
{
    size_t start;

    switch (h->kind) {
    case HANDLE_NONE:
        return;
    case HANDLE_ENCLOSING:
        simple(d, depth, "caught_by_block", NULL);
        return;
    case HANDLE_FATAL:
        simple(d, depth, "catch_fatal", NULL);
        return;
    case HANDLE_TRY:
        simple(d, depth, "try", NULL);
        return;
    case HANDLE_BLOCK:
        start = begin(d, depth);
        if (h->name.length > 0) {
            label_name(d, "catch", NULL, &h->name);
        } else {
            text_append(d->out, "catch");
        }
        end(d, start, NULL);
        dump_block(d, depth + 1, h->body);
        return;
    }
}

/* The name a `for` gives its element, empty when it gives none. The
   element is the last of the names, so `for i, x in items` binds x. */
static struct name loop_name(const struct stmt *s)
{
    struct name empty = {NULL, 0};

    return s->as.for_loop.name_count > 0
               ? s->as.for_loop.names[s->as.for_loop.name_count - 1].name
               : empty;
}

/* The type of that element, or NULL before the checker ran. */
static const struct type *loop_element(const struct stmt *s)
{
    const struct symbol *sym =
        s->as.for_loop.name_count > 0
            ? s->as.for_loop.names[s->as.for_loop.name_count - 1].symbol
            : NULL;

    return sym != NULL ? sym->type : NULL;
}

/* The names of a destructuring, one line each. */
static void dump_bindings(struct dumper *d, int depth,
                          const struct binding *names, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        size_t start = begin(d, depth);
        label_name(d, "name", NULL, &names[i].name);
        end(d, start, names[i].symbol != NULL ? names[i].symbol->type : NULL);
    }
}

static void dump_stmt(struct dumper *d, int depth, const struct stmt *s)
{
    struct name element;
    size_t start;
    size_t i;

    switch (s->kind) {
    case STMT_YIELD:
        simple(d, depth, "yield_stmt", NULL);
        if (s->as.yielded != NULL) {
            dump_expr(d, depth + 1, s->as.yielded);
        }
        break;
    case STMT_TRY:
        simple(d, depth, "try_stmt", NULL);
        dump_block(d, depth + 1, s->as.try_block.body);
        dump_handler(d, depth + 1, &s->as.try_block.handler);
        break;
    case STMT_LET:
    case STMT_CONST:
        start = begin(d, depth);
        /* A destructuring binds no name of its own, so the line carries
           the statement alone and the names stand below it. */
        if (s->as.let.name_count > 0) {
            text_append(d->out, "let_stmt");
        } else {
            label_name(d, s->kind == STMT_LET ? "let_stmt" : "const_decl",
                       NULL, &s->as.let.name);
        }
        end(d, start, s->as.let.symbol != NULL ? s->as.let.symbol->type : NULL);
        dump_bindings(d, depth + 1, s->as.let.names, s->as.let.name_count);
        if (s->as.let.type != NULL) {
            dump_type(d, depth + 1, s->as.let.type);
        }
        dump_expr(d, depth + 1, s->as.let.value);
        break;
    case STMT_FOR:
        start = begin(d, depth);
        element = loop_name(s);
        label_name(d, "for_stmt", NULL, &element);
        end(d, start, loop_element(s));
        dump_bindings(d, depth + 1, s->as.for_loop.names,
                      s->as.for_loop.name_count - (element.length > 0 ? 1 : 0));
        if (s->as.for_loop.over != NULL) {
            simple(d, depth + 1,
                   s->as.for_loop.by_pointer ? "over_pointer" : "over", NULL);
            dump_expr(d, depth + 2, s->as.for_loop.over);
        } else {
            simple(d, depth + 1, "from", NULL);
            dump_expr(d, depth + 2, s->as.for_loop.low);
            simple(d, depth + 1, "to", NULL);
            dump_expr(d, depth + 2, s->as.for_loop.high);
        }
        dump_block(d, depth + 1, s->as.for_loop.body);
        break;
    case STMT_ASSERT:
        simple(d, depth, "assert_stmt", NULL);
        dump_expr(d, depth + 1, s->as.assertion.cond);
        break;
    case STMT_SWITCH:
        simple(d, depth, s->as.switch_stmt.if_let ? "if_let" : "switch_stmt",
               NULL);
        dump_expr(d, depth + 1, s->as.switch_stmt.value);
        for (i = 0; i < s->as.switch_stmt.count; i++) {
            const struct switch_arm *arm = &s->as.switch_stmt.arms[i];
            simple(d, depth + 1, "arm", NULL);
            dump_expr(d, depth + 2, arm->value);
            if (arm->binds.length > 0) {
                size_t at = begin(d, depth + 2);
                label_name(d, "binds", NULL, &arm->binds);
                end(d, at, arm->bound != NULL ? arm->bound->type : NULL);
            }
            dump_stmt(d, depth + 2, arm->body);
        }
        if (s->as.switch_stmt.otherwise != NULL) {
            simple(d, depth + 1, "else", NULL);
            dump_stmt(d, depth + 2, s->as.switch_stmt.otherwise);
        }
        break;
    case STMT_SYNC:
        simple(d, depth, "sync_stmt", NULL);
        dump_expr(d, depth + 1, s->as.sync.mutex);
        dump_block(d, depth + 1, s->as.sync.body);
        break;
    case STMT_SELECT:
        simple(d, depth, "select_stmt", NULL);
        for (i = 0; i < s->as.select.count; i++) {
            const struct switch_arm *arm = &s->as.select.arms[i];
            simple(d, depth + 1, "arm", NULL);
            dump_expr(d, depth + 2, arm->value);
            if (arm->binds.length > 0) {
                size_t at = begin(d, depth + 2);
                label_name(d, "binds", NULL, &arm->binds);
                end(d, at, arm->bound != NULL ? arm->bound->type : NULL);
            }
            dump_stmt(d, depth + 2, arm->body);
        }
        break;
    case STMT_DEFER:
        simple(d, depth, "defer_stmt", NULL);
        dump_stmt(d, depth + 1, s->as.deferred);
        break;
    case STMT_UNDO:
        simple(d, depth, "undo_stmt", NULL);
        dump_stmt(d, depth + 1, s->as.deferred);
        break;
    case STMT_FAIL:
        simple(d, depth, "fail_stmt", NULL);
        dump_expr(d, depth + 1, s->as.fail.value);
        break;
    case STMT_EXPR:
        simple(d, depth, "simple_stmt", NULL);
        dump_expr(d, depth + 1, s->as.expr);
        break;
    case STMT_ASSIGN:
        start = begin(d, depth);
        text_append(d->out, "simple_stmt ");
        op_name(d, s->as.assign.op);
        end(d, start, NULL);
        dump_expr(d, depth + 1, s->as.assign.target);
        dump_expr(d, depth + 1, s->as.assign.value);
        break;
    case STMT_IF:
        simple(d, depth, "if_stmt", NULL);
        for (i = 0; i < s->as.if_chain.count; i++) {
            simple(d, depth + 1, "branch", NULL);
            dump_expr(d, depth + 2, s->as.if_chain.branches[i].cond);
            dump_block(d, depth + 2, s->as.if_chain.branches[i].body);
        }
        if (s->as.if_chain.else_body != NULL) {
            simple(d, depth + 1, "else", NULL);
            dump_block(d, depth + 2, s->as.if_chain.else_body);
        }
        break;
    case STMT_WHILE:
        simple(d, depth, "while_stmt", NULL);
        dump_expr(d, depth + 1, s->as.loop.cond);
        dump_block(d, depth + 1, s->as.loop.body);
        break;
    case STMT_DO_WHILE:
        simple(d, depth, "do_stmt", NULL);
        dump_block(d, depth + 1, s->as.loop.body);
        dump_expr(d, depth + 1, s->as.loop.cond);
        break;
    case STMT_BREAK:
        simple(d, depth, "break", NULL);
        break;
    case STMT_CONTINUE:
        simple(d, depth, "continue", NULL);
        break;
    case STMT_FALLTHROUGH:
        simple(d, depth, "fallthrough", NULL);
        break;
    case STMT_RETURN:
        simple(d, depth, "return_stmt", NULL);
        if (s->as.return_value != NULL) {
            dump_expr(d, depth + 1, s->as.return_value);
        }
        break;
    case STMT_BLOCK:
        dump_block(d, depth, s->as.block);
        break;
    }
}

static void dump_block(struct dumper *d, int depth, const struct block *b)
{
    size_t i;

    simple(d, depth, "block", NULL);
    for (i = 0; i < b->count; i++) {
        dump_stmt(d, depth + 1, b->stmts[i]);
    }
}

static void dump_params(struct dumper *d, int depth, const char *label,
                        const struct param *params, size_t count,
                        const struct type *owner)
{
    size_t i;

    for (i = 0; i < count; i++) {
        size_t start = begin(d, depth);
        const struct type *type = NULL;

        label_name(d, label, NULL, &params[i].name);
        if (params[i].symbol != NULL) {
            type = params[i].symbol->type;
        } else if (owner != NULL && owner->kind == TYPE_STRUCT &&
                   i < owner->field_count) {
            type = owner->fields[i].type;
        }
        end(d, start, type);
        dump_doc(d, depth + 1, "doc", &params[i].doc);
        dump_doc(d, depth + 1, "note", &params[i].note);
        /* An enum value carries a name and no type. */
        if (params[i].type != NULL) {
            dump_type(d, depth + 1, params[i].type);
        }
        if (params[i].bits != NULL) {
            simple(d, depth + 1, "bits", NULL);
            dump_expr(d, depth + 2, params[i].bits);
        }
        if (params[i].form == FIELD_USE) {
            simple(d, depth + 1, "use", NULL);
        }
        if (params[i].form == FIELD_IMPL) {
            simple(d, depth + 1, "implements", NULL);
        }
        if (params[i].vis == VIS_PUB) {
            simple(d, depth + 1, "pub", NULL);
        } else if (params[i].vis == VIS_PROTECTED) {
            simple(d, depth + 1, "protected", NULL);
        }
        if (params[i].writable) {
            simple(d, depth + 1, "mutable", NULL);
        }
        if (params[i].keep) {
            simple(d, depth + 1, "keep", NULL);
        }
        if (params[i].concurrent) {
            simple(d, depth + 1, "concurrent", NULL);
        }
        if (params[i].value != NULL) {
            simple(d, depth + 1, "default", NULL);
            dump_expr(d, depth + 2, params[i].value);
        }
    }
}

/* The functions and constants declared in the body of a struct or enum. */
static void dump_members(struct dumper *d, const struct item *it)
{
    size_t i;

    for (i = 0; i < it->member_count; i++) {
        const struct item *m = it->members[i];
        size_t start = begin(d, 1);

        if (m->exported) {
            text_append(d->out, "export ");
        } else if (m->pub) {
            text_append(d->out, "pub ");
        } else if (m->vis == VIS_PROTECTED) {
            text_append(d->out, "protected ");
        }
        label_name(d, m->kind == ITEM_CONST ? "const_decl" : "function", NULL,
                   &m->name);
        end(d, start, m->symbol != NULL ? m->symbol->type : NULL);
        if (m->kind == ITEM_CONST) {
            dump_type(d, 2, m->type);
            dump_expr(d, 2, m->value);
            continue;
        }
        if (m->is_operator) {
            simple(d, 2, "operator", NULL);
        }
        if (m->contract != FN_PLAIN && m->qualifier.length > 0) {
            size_t at = begin(d, 2);
            label_name(d, "concrete", NULL, &m->qualifier);
            end(d, at, NULL);
        } else if (m->contract != FN_PLAIN) {
            simple(d, 2, m->contract == FN_ABSTRACT ? "abstract" : "concrete",
                   NULL);
        }
        if (m->has_self) {
            simple(d, 2, "self", NULL);
        }
        dump_params(d, 2, "param", m->params, m->param_count, NULL);
        if (m->result != NULL) {
            simple(d, 2, "result", NULL);
            dump_type(d, 3, m->result);
        }
        if (m->may_fail) {
            simple(d, 2, "may_fail", NULL);
        }
        if (m->body != NULL) {
            dump_block(d, 2, m->body);
        }
    }
}

static void dump_module(struct dumper *d, const struct module *module)
{
    size_t i;
    size_t j;

    dump_doc(d, 0, "module_doc", &module->doc);
    dump_doc(d, 0, "module_note", &module->note);
    for (i = 0; i < module->import_count; i++) {
        const struct import *imp = &module->imports[i];
        text_appendf(d->out, "import %.*s", (int)imp->module.length,
                     imp->module.text);
        if (imp->alias.length > 0) {
            text_appendf(d->out, " as %.*s", (int)imp->alias.length,
                         imp->alias.text);
        }
        text_append(d->out, "\n");
    }
    for (i = 0; i < module->provides_count; i++) {
        const struct provides *pr = &module->provides[i];
        text_append(d->out, "provides ");
        if (pr->qualifier.length > 0) {
            text_appendf(d->out, "%.*s.", (int)pr->qualifier.length,
                         pr->qualifier.text);
        }
        text_appendf(d->out, "%.*s as %.*s\n", (int)pr->interface.length,
                     pr->interface.text, (int)pr->class_name.length,
                     pr->class_name.text);
    }
    for (i = 0; i < module->item_count; i++) {
        const struct item *it = module->items[i];
        const struct type *type = it->symbol != NULL ? it->symbol->type : NULL;
        const char *label = it->kind == ITEM_FN          ? "function"
                            : it->kind == ITEM_EXTERN_FN ? "extern_fn"
                            : it->kind == ITEM_STRUCT    ? "struct_decl"
                            : it->kind == ITEM_UNION     ? "union_decl"
                            : it->kind == ITEM_ENUM      ? "enum_decl"
                            : it->kind == ITEM_CLASS     ? "class_decl"
                            : it->kind == ITEM_VARIANT   ? "variant_decl"
                                                         : "const_decl";
        size_t start = begin(d, 0);

        if (it->exported) {
            text_append(d->out, "export ");
        } else if (it->pub) {
            text_append(d->out, "pub ");
        }
        if (it->worker) {
            text_append(d->out, "worker ");
        }
        label_name(d, label, NULL, &it->name);
        end(d, start,
            it->kind == ITEM_STRUCT || it->kind == ITEM_UNION ? NULL : type);
        dump_doc(d, 1, "doc", &it->doc);
        dump_doc(d, 1, "note", &it->note);
        if (it->packed) {
            simple(d, 1, "packed", NULL);
        }
        if (it->simd) {
            simple(d, 1, "simd", NULL);
        }
        if (it->align != NULL) {
            simple(d, 1, "align", NULL);
            dump_expr(d, 2, it->align);
        }
        switch (it->kind) {
        case ITEM_FN:
        case ITEM_EXTERN_FN:
            if (it->contract != FN_PLAIN) {
                simple(d, 1,
                       it->contract == FN_ABSTRACT ? "abstract" : "concrete",
                       NULL);
            }
            if (it->has_self) {
                simple(d, 1, "self", NULL);
            }
            dump_params(d, 1, "param", it->params, it->param_count, NULL);
            if (it->variadic) {
                simple(d, 1, "variadic", NULL);
            }
            if (it->result != NULL) {
                simple(d, 1, "result", NULL);
                dump_type(d, 2, it->result);
            }
            if (it->may_fail) {
                simple(d, 1, "may_fail", NULL);
            }
            if (it->body != NULL) {
                dump_block(d, 1, it->body);
            }
            break;
        case ITEM_STRUCT:
        case ITEM_UNION:
            dump_params(d, 1, "field", it->params, it->param_count, type);
            break;
        case ITEM_CLASS:
            if (it->is_abstract) {
                simple(d, 1, "abstract", NULL);
            }
            if (it->is_final) {
                simple(d, 1, "final", NULL);
            }
            if (it->is_singleton) {
                simple(d, 1, "singleton", NULL);
            }
            if (it->base_name.length > 0) {
                size_t at = begin(d, 1);
                label_name(d, "inherits", &it->base_module, &it->base_name);
                end(d, at, NULL);
            }
            dump_params(d, 1, "field", it->params, it->param_count, type);
            dump_members(d, it);
            break;
        case ITEM_VARIANT:
            for (j = 0; j < it->case_count; j++) {
                const struct variant_case *one = &it->cases[j];
                size_t at = begin(d, 1);
                const struct type *payload =
                    type != NULL && j < type->param_count ? type->params[j]
                                                          : NULL;
                label_name(d, "case", NULL, &one->name);
                end(d, at, NULL);
                dump_doc(d, 2, "doc", &one->doc);
                dump_params(d, 2, "field", one->fields, one->field_count,
                            payload);
            }
            break;
        case ITEM_ENUM:
            if (it->base != NULL) {
                simple(d, 1, "base", NULL);
                dump_type(d, 2, it->base);
            }
            dump_params(d, 1, "value", it->params, it->param_count, NULL);
            dump_members(d, it);
            break;
        case ITEM_CONST:
            dump_type(d, 1, it->type);
            dump_expr(d, 1, it->value);
            break;
        }
    }
}

void ast_dump(struct text *out, const struct module *module)
{
    struct dumper d = {out, false};
    dump_module(&d, module);
}

void ast_dump_typed(struct text *out, const struct module *module)
{
    struct dumper d = {out, true};
    dump_module(&d, module);
}
