/* The one walk of a function body that every analysis of the checked
   tree visits through. */

#include "ast.h"

/* DESIGN: the walk reaches each expression and each statement of a body
   once, in the order of the text. Where the checker wrote a node that
   holds a node of the text, the walk takes the written node and not the
   part again: the test of `x in lo..hi` holds lo and hi, the test of an
   arm of a `switch` on a `str` holds the arm's value, the start of an
   iteration holds the collection, and the current value of a walk in
   place holds the call of `value`. Those parts are walked alone while
   the checker has not written the node. An anonymous function is an
   item of its own, and the walk does not enter its body. The callee of
   a call is walked as any other expression, so a visitor finds the
   function a call reaches there. */

static void walk_list(const struct ast_visitor *v, struct expr *const *list,
                      size_t count)
{
    size_t i;

    for (i = 0; list != NULL && i < count; i++) {
        ast_walk_expr(v, list[i]);
    }
}

static void walk_inits(const struct ast_visitor *v,
                       const struct field_init *inits, size_t count)
{
    size_t i;

    for (i = 0; inits != NULL && i < count; i++) {
        ast_walk_expr(v, inits[i].value);
    }
}

static void walk_iteration(const struct ast_visitor *v,
                           const struct iteration *it)
{
    ast_walk_expr(v, it->start);
    ast_walk_expr(v, it->advance);
    ast_walk_expr(v, it->current);
    ast_walk_expr(v, it->changed);
    ast_walk_expr(v, it->change_file);
    ast_walk_expr(v, it->change_file_length);
    ast_walk_expr(v, it->change_line);
}

static void walk_arms(const struct ast_visitor *v,
                      const struct switch_arm *arms, size_t count)
{
    size_t i;

    for (i = 0; arms != NULL && i < count; i++) {
        ast_walk_expr(v, arms[i].test != NULL ? arms[i].test : arms[i].value);
        ast_walk_stmt(v, arms[i].body);
    }
}

static void walk_children(const struct ast_visitor *v, const struct expr *e)
{
    size_t i;

    switch (e->kind) {
    case EXPR_UNARY:
        ast_walk_expr(v, e->as.unary.operand);
        return;
    case EXPR_BINARY:
        ast_walk_expr(v, e->as.binary.left);
        ast_walk_expr(v, e->as.binary.right);
        walk_list(v, e->as.binary.eq_calls, e->as.binary.eq_count);
        return;
    case EXPR_CAST:
        ast_walk_expr(v, e->as.cast.operand);
        return;
    case EXPR_CALL:
        ast_walk_expr(v, e->as.call.callee);
        walk_list(v, e->as.call.args, e->as.call.arg_count);
        ast_walk_block(v, e->as.call.handler.body);
        walk_list(v, e->as.call.hash_calls, e->as.call.hash_count);
        return;
    case EXPR_INDEX:
        ast_walk_expr(v, e->as.index.base);
        ast_walk_expr(v, e->as.index.index);
        return;
    case EXPR_SLICE:
        ast_walk_expr(v, e->as.slice.base);
        ast_walk_expr(v, e->as.slice.low);
        ast_walk_expr(v, e->as.slice.high);
        return;
    case EXPR_FIELD:
        ast_walk_expr(v, e->as.field.base);
        return;
    case EXPR_STRUCT_LIT:
        walk_inits(v, e->as.struct_lit.fields, e->as.struct_lit.field_count);
        return;
    case EXPR_TUPLE:
        walk_list(v, e->as.tuple.elements, e->as.tuple.count);
        return;
    case EXPR_SLICE_LIT:
        walk_inits(v, e->as.slice_lit.fields, e->as.slice_lit.field_count);
        return;
    case EXPR_ARRAY_LIT:
        walk_list(v, e->as.array_lit.elements, e->as.array_lit.count);
        return;
    case EXPR_ARRAY_REPEAT:
        ast_walk_expr(v, e->as.array_repeat.value);
        ast_walk_expr(v, e->as.array_repeat.count);
        return;
    case EXPR_ALLOC:
        ast_walk_expr(v, e->as.alloc.count);
        ast_walk_expr(v, e->as.alloc.value);
        return;
    case EXPR_FREE:
        ast_walk_expr(v, e->as.free_pointer);
        return;
    case EXPR_OBJECT:
        ast_walk_expr(v, e->as.object.operand);
        ast_walk_expr(v, e->as.object.from);
        return;
    case EXPR_ATOMIC:
        ast_walk_expr(v, e->as.atomic.place);
        ast_walk_expr(v, e->as.atomic.a);
        ast_walk_expr(v, e->as.atomic.b);
        return;
    case EXPR_PARALLEL:
        ast_walk_expr(v, e->as.parallel.array);
        ast_walk_expr(v, e->as.parallel.chunks);
        ast_walk_expr(v, e->as.parallel.call);
        return;
    case EXPR_DISPATCH:
        ast_walk_expr(v, e->as.dispatch.object);
        ast_walk_expr(v, e->as.dispatch.call);
        return;
    case EXPR_JOIN:
        ast_walk_expr(v, e->as.join.job);
        return;
    case EXPR_FORMAT:
        ast_walk_expr(v, e->as.format.start);
        for (i = 0; i < e->as.format.count; i++) {
            const struct format_part *p = &e->as.format.parts[i];
            ast_walk_expr(v, p->text_call);
            ast_walk_expr(v, p->value);
            ast_walk_expr(v, p->value_call);
        }
        ast_walk_expr(v, e->as.format.take);
        return;
    case EXPR_IN:
        ast_walk_expr(v, e->as.in.value);
        if (e->as.in.test != NULL) {
            ast_walk_expr(v, e->as.in.test);
        } else {
            ast_walk_expr(v, e->as.in.low);
            ast_walk_expr(v, e->as.in.high);
        }
        return;
    case EXPR_OPTIONAL:
        ast_walk_expr(v, e->as.optional.base);
        ast_walk_expr(v, e->as.optional.access);
        return;
    case EXPR_SYNC_OP:
        ast_walk_expr(v, e->as.sync_op.target);
        ast_walk_expr(v, e->as.sync_op.value);
        return;
    case EXPR_SIMD:
        walk_list(v, e->as.simd.args, e->as.simd.arg_count);
        return;
    case EXPR_COLLECT:
        walk_iteration(v, &e->as.collect);
        return;
    case EXPR_INT:
    case EXPR_FLOAT:
    case EXPR_CHAR:
    case EXPR_STRING:
    case EXPR_BYTES:
    case EXPR_BOOL:
    case EXPR_NONE:
    case EXPR_NAME:
    case EXPR_SIZE_OF:
    case EXPR_HERE:
    case EXPR_PATTERN:
    case EXPR_DESCRIPTOR:
    case EXPR_FN:
        return;
    }
}

void ast_walk_expr(const struct ast_visitor *v, const struct expr *e)
{
    if (e != NULL && (v->expr == NULL || v->expr(v->data, e))) {
        walk_children(v, e);
    }
}

void ast_walk_stmt(const struct ast_visitor *v, const struct stmt *s)
{
    size_t i;

    if (s == NULL || (v->stmt != NULL && !v->stmt(v->data, s))) {
        return;
    }
    switch (s->kind) {
    case STMT_LET:
    case STMT_CONST:
        ast_walk_expr(v, s->as.let.value);
        ast_walk_block(v, s->as.let.otherwise);
        ast_walk_block(v, s->as.let.guard.body);
        return;
    case STMT_EXPR:
        ast_walk_expr(v, s->as.expr);
        return;
    case STMT_ASSIGN:
        ast_walk_expr(v, s->as.assign.target);
        ast_walk_expr(v, s->as.assign.value);
        return;
    case STMT_IF:
        for (i = 0; i < s->as.if_chain.count; i++) {
            ast_walk_expr(v, s->as.if_chain.branches[i].cond);
            ast_walk_block(v, s->as.if_chain.branches[i].body);
        }
        ast_walk_block(v, s->as.if_chain.else_body);
        return;
    case STMT_WHILE:
    case STMT_DO_WHILE:
        ast_walk_expr(v, s->as.loop.cond);
        ast_walk_block(v, s->as.loop.body);
        return;
    case STMT_FOR:
        ast_walk_expr(v, s->as.for_loop.low);
        ast_walk_expr(v, s->as.for_loop.high);
        ast_walk_expr(v, s->as.for_loop.step);
        if (s->as.for_loop.hooks.start == NULL) {
            ast_walk_expr(v, s->as.for_loop.over);
        }
        walk_iteration(v, &s->as.for_loop.hooks);
        ast_walk_block(v, s->as.for_loop.body);
        return;
    case STMT_DEFER:
    case STMT_UNDO:
        ast_walk_stmt(v, s->as.deferred);
        return;
    case STMT_FAIL:
        ast_walk_expr(v, s->as.fail.value);
        return;
    case STMT_SWITCH:
        ast_walk_expr(v, s->as.switch_stmt.value);
        walk_arms(v, s->as.switch_stmt.arms, s->as.switch_stmt.count);
        ast_walk_stmt(v, s->as.switch_stmt.otherwise);
        return;
    case STMT_ASSERT:
        ast_walk_expr(v, s->as.assertion.cond);
        return;
    case STMT_RETURN:
        ast_walk_expr(v, s->as.return_value);
        return;
    case STMT_YIELD:
        ast_walk_expr(v, s->as.yielded);
        return;
    case STMT_TRY:
        ast_walk_block(v, s->as.try_block.body);
        ast_walk_block(v, s->as.try_block.handler.body);
        return;
    case STMT_BLOCK:
        ast_walk_block(v, s->as.block);
        return;
    case STMT_SYNC:
        ast_walk_expr(v, s->as.sync.mutex);
        ast_walk_expr(v, s->as.sync.second);
        ast_walk_block(v, s->as.sync.body);
        return;
    case STMT_SELECT:
        walk_arms(v, s->as.select.arms, s->as.select.count);
        return;
    case STMT_BREAK:
    case STMT_CONTINUE:
    case STMT_FALLTHROUGH:
        return;
    }
}

void ast_walk_block(const struct ast_visitor *v, const struct block *b)
{
    size_t i;

    for (i = 0; b != NULL && i < b->count; i++) {
        ast_walk_stmt(v, b->stmts[i]);
    }
}
