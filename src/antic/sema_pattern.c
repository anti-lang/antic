/* The methods of `str` and `[]byte` that take a pattern and the match
   they give. They are the calls of `anti.regex` that stand in their
   place, the groups of a literal as fields, a match as a condition and
   `if let` on one. `patch` of `[]byte` and the conversions `to_bytes` and
   `to_text` stand here as well. */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../rt/regex.h"
#include "pattern.h"
#include "sema_checker.h"

/* A method of `str` that takes a pattern, and the counts of arguments a
   program gives it. */
struct method {
    const char *name;
    size_t least;
    size_t most;
};

static const struct method str_methods[] = {
    {"matches", 1, 1},
    {"find_all", 1, 2},
    {"replace", 2, 3},
    {"split", 1, 2},
};

static const struct name regex_module = {REGEX_MODULE,
                                         sizeof REGEX_MODULE - 1};
static const struct name text_module = {TEXT_MODULE,
                                        sizeof TEXT_MODULE - 1};

/* The name of `patch` and of the conversions of `str` and `[]byte`. */
#define METHOD_PATCH "patch"
#define METHOD_TO_BYTES "to_bytes"
#define METHOD_TO_TEXT "to_text"

static const struct method *str_method(const struct name *name)
{
    size_t i;

    for (i = 0; i < sizeof str_methods / sizeof str_methods[0]; i++) {
        if (sema_name_is(name, str_methods[i].name)) {
            return &str_methods[i];
        }
    }
    return NULL;
}

static bool is_match_method(const struct name *name)
{
    return sema_name_is(name, MATCH_GROUP) ||
           sema_name_is(name, MATCH_TOOK_PART);
}

/* Whether t is `[]byte`. */
static bool is_byte_slice(const struct type *t)
{
    return t != NULL && t->kind == TYPE_SLICE && t->element != NULL &&
           t->element->kind == TYPE_U8;
}

/* The function text of the module named module, which the call at pos
   names in the place of what, or NULL after an error. */
static struct symbol *module_function(struct checker *c,
                                      const struct name *module,
                                      struct pos pos, const char *what,
                                      const char *text)
{
    const struct interface *lib = sema_find_library(c, module);
    struct name name;
    struct symbol *f;

    if (lib == NULL) {
        sema_error_at(c, pos, "`%s` calls `%.*s.%s`, so the module imports "
                      "`%.*s`", what, (int)module->length, module->text, text,
                      (int)module->length, module->text);
        return NULL;
    }
    name.text = text;
    name.length = strlen(text);
    f = sema_library_item(c, lib, &name);
    if (f == NULL || f->kind != SYMBOL_FN || f->type == NULL ||
        f->type->kind != TYPE_FN) {
        sema_error_at(c, pos, "`%.*s` has no function `%s`",
                      (int)module->length, module->text, text);
        return NULL;
    }
    return f;
}

static struct symbol *regex_function(struct checker *c, struct pos pos,
                                     const char *what, const char *text)
{
    return module_function(c, &regex_module, pos, what, text);
}

/* Point the call e at the function f with the count arguments args,
   every one of which is checked. */
static void call_of(struct checker *c, struct expr *e, struct symbol *f,
                    struct expr **args, size_t count)
{
    struct expr *callee = sema_new_node(c, EXPR_NAME, e->as.call.callee->pos);

    callee->as.name = f->name;
    callee->symbol = f;
    callee->type = f->type;
    e->as.call.callee = callee;
    e->as.call.args = args;
    e->as.call.arg_count = count;
}

static bool check_arg(struct checker *c, struct expr *arg,
                      struct type *param)
{
    return sema_require(c, arg, sema_check_expr(c, arg, param), param);
}

/* Whether the pattern literal has the group that arg names, when arg is a
   literal. Any other argument is read at run time. */
static bool group_known(struct checker *c, const struct expr *pattern,
                        const struct expr *arg, bool named)
{
    const char *bytes = pattern->as.text.bytes;
    size_t length = pattern->as.text.length;
    bool mode = types_is_byte_regex(pattern->type);
    long count;

    if (named && arg->kind == EXPR_STRING &&
        pattern_group_number(bytes, length, mode, arg->as.text.bytes,
                             arg->as.text.length) < 0) {
        sema_error_at(c, arg->pos, "the pattern has no group `%.*s`",
                      (int)arg->as.text.length, arg->as.text.bytes);
        return false;
    }
    count = pattern_group_count(bytes, length, mode);
    if (!named && arg->kind == EXPR_INT && arg->as.integer > (uint64_t)count) {
        sema_error_at(c, arg->pos, "the pattern has %ld group%s, and %llu is "
                      "none of them", count, count == 1 ? "" : "s",
                      (unsigned long long)arg->as.integer);
        return false;
    }
    return true;
}

/* DESIGN: a template written as a literal beside a pattern literal is
   checked here. The reader the runtime uses takes it apart, so a group
   the pattern lacks is a compile error. */
static bool template_fits(struct checker *c, const struct expr *pattern,
                          const struct expr *t)
{
    const unsigned char *bytes = (const unsigned char *)t->as.text.bytes;
    int64_t length = (int64_t)t->as.text.length;
    bool mode = types_is_byte_regex(pattern->type);
    long count = pattern_group_count(pattern->as.text.bytes,
                                     pattern->as.text.length, mode);
    struct anti_rt_piece piece;
    int64_t offset = 0;
    bool ok = true;

    while (anti_rt_regex_piece(bytes, length, &offset, &piece)) {
        if (piece.kind == ANTI_RT_PIECE_NUMBER && piece.number > count) {
            sema_error_at(c, t->pos, "the template names group %lld, and the "
                          "pattern has %ld group%s", (long long)piece.number,
                          count, count == 1 ? "" : "s");
            ok = false;
        } else if (piece.kind == ANTI_RT_PIECE_NAME &&
                   pattern_group_number(pattern->as.text.bytes,
                                        pattern->as.text.length, mode,
                                        (const char *)bytes + piece.start,
                                        (size_t)piece.length) < 0) {
            sema_error_at(c, t->pos, "the template names the group `%.*s`, "
                          "which the pattern lacks", (int)piece.length,
                          (const char *)bytes + piece.start);
            ok = false;
        }
    }
    return ok;
}

/* A new literal of an integer at pos. */
static struct expr *int_literal(struct checker *c, struct pos pos,
                                uint64_t value)
{
    struct expr *e = sema_new_node(c, EXPR_INT, pos);

    e->as.integer = value;
    e->type = sema_builtin(c, TYPE_I64);
    return e;
}

/* A new literal of the text at bytes at pos. */
static struct expr *text_literal(struct checker *c, struct pos pos,
                                 const char *bytes, size_t length)
{
    struct expr *e = sema_new_node(c, EXPR_STRING, pos);

    e->as.text.bytes = bytes;
    e->as.text.length = length;
    e->spelling = e->as.text;
    e->type = sema_builtin(c, TYPE_STR);
    return e;
}

/* DESIGN: `s.m(args)` with a pattern is the call of a function of
   `anti.regex` with s first. A pattern literal written at the call calls
   the function that ends in `_literal`, which cannot fail and takes the
   text of the literal for its stop at the match limit. Any other pattern
   calls the function of the method's name, which may fail. `replace`
   calls `replace_fn` for a function and `replace` for a template, and a
   `matches` that a condition only tests calls `matched`. The same methods
   of a `[]byte` call the functions whose names start with `bytes_`, and
   take a `ByteRegex`, a `[]byte` template and a function of a
   `ByteMatch`, so a pattern literal there is a byte pattern. Every
   argument is checked here, so the call checks none of them again. */
static bool method_call(struct checker *c, struct expr *e, bool bytes)
{
    struct expr *field = e->as.call.callee;
    const struct method *m = str_method(&field->as.field.name);
    size_t given = e->as.call.arg_count;
    struct expr *pattern;
    const struct expr *literal;
    struct expr *with = NULL;
    bool function = false;
    const char *base = m->name;
    char name[40];
    char what[40];
    struct symbol *f;
    struct expr **args;
    size_t count;
    size_t at;
    size_t i;
    bool ok = true;

    if (given < m->least || given > m->most) {
        if (m->least == m->most) {
            sema_error_at(c, e->pos, "`%s` takes %zu argument%s, found %zu",
                          m->name, m->least, m->least == 1 ? "" : "s", given);
        } else {
            sema_error_at(c, e->pos, "`%s` takes %zu or %zu arguments, found "
                          "%zu", m->name, m->least, m->most, given);
        }
        return false;
    }
    pattern = e->as.call.args[0];
    if (!check_arg(c, pattern, bytes ? types_byte_regex(c->types)
                                     : types_regex(c->types))) {
        return false;
    }
    literal = pattern->kind == EXPR_PATTERN ? pattern : NULL;
    if (sema_name_is(&field->as.field.name, "replace")) {
        with = e->as.call.args[1];
        function = with->kind == EXPR_FN;
        if (!function) {
            struct type *t = sema_check_expr(c, with, NULL);
            if (sema_is_error(t)) {
                return false;
            }
            if (!(bytes ? is_byte_slice(t) : t->kind == TYPE_STR) &&
                t->kind != TYPE_FN) {
                sema_error_at(c, with->pos, "`replace` takes a template "
                              "`%s` or a `fn(%s) -> %s`, found `%s`",
                              bytes ? "[]byte" : "str",
                              bytes ? LANG_BYTE_MATCH : LANG_MATCH,
                              bytes ? "[]byte" : "str", sema_tn(t));
                return false;
            }
            function = t->kind == TYPE_FN;
        }
        base = function ? "replace_fn" : "replace";
    } else if (sema_name_is(&field->as.field.name, "matches") &&
               e->as.call.tested && !e->as.call.handler.none) {
        base = "matched";
    }
    snprintf(name, sizeof name, "%s%s%s", bytes ? "bytes_" : "", base,
             literal != NULL ? "_literal" : "");
    snprintf(what, sizeof what, "%s.%s", bytes ? "data" : "s", m->name);
    if ((f = regex_function(c, field->pos, what, name)) == NULL) {
        return false;
    }
    count = given + 1 + (literal != NULL ? 1 : 0);
    args = types_alloc_array(c->arena, count, sizeof *args);
    args[0] = field->as.field.base;
    args[1] = pattern;
    at = 2;
    if (literal != NULL) {
        args[at++] = text_literal(c, pattern->pos, literal->as.text.bytes,
                                  literal->as.text.length);
    }
    for (i = 1; i < given; i++) {
        struct expr *arg = e->as.call.args[i];
        struct type *param = f->type->params[at];
        if (arg == with && with->kind != EXPR_FN) {
            ok = sema_require(c, arg, arg->type, param) && ok;
        } else {
            ok = check_arg(c, arg, param) && ok;
        }
        args[at++] = arg;
    }
    if (!ok) {
        return false;
    }
    if (literal != NULL && with != NULL && !function &&
        with->kind == (bytes ? EXPR_BYTES : EXPR_STRING) &&
        !template_fits(c, literal, with)) {
        return false;
    }
    call_of(c, e, f, args, count);
    e->as.call.pattern = literal;
    return true;
}

/* The group of the pattern literal that `patch` writes into, when into
   is absent or a literal, or -1 after an error. into 0 takes the one group
   of a pattern with one and the whole match of a pattern without. */
static long patch_group(struct checker *c, const struct expr *literal,
                        const struct expr *into, struct pos pos)
{
    const char *bytes = literal->as.text.bytes;
    size_t length = literal->as.text.length;
    long count = pattern_group_count(bytes, length, true);
    long group;

    if (into != NULL && into->kind == EXPR_STRING) {
        group = pattern_group_number(bytes, length, true, into->as.text.bytes,
                                     into->as.text.length);
        if (group < 0) {
            sema_error_at(c, into->pos, "the pattern has no group `%.*s`",
                          (int)into->as.text.length, into->as.text.bytes);
        }
        return group;
    }
    group = into != NULL ? (long)into->as.integer : 0;
    if (group > count) {
        sema_error_at(c, into->pos, "the pattern has %ld group%s, and %ld is "
                      "none of them", count, count == 1 ? "" : "s", group);
        return -1;
    }
    if (group == 0 && count > 1) {
        sema_error_at(c, into != NULL ? into->pos : pos, "the pattern has %ld "
                      "groups, so `patch` names the one it writes into with "
                      "`into`", count);
        return -1;
    }
    return group == 0 ? count : group;
}

/* DESIGN: `data.patch(find, with, into, at, limit)` is a call of a
   function of `anti.regex` with the data first, and every argument the
   call leaves out written as its default, 0. A `[]byte` find calls
   `patch_bytes`, and a pattern calls `patch`. Named arguments are not
   built, so `at` and `limit` come by position, after `into`, which a
   byte sequence takes as 0 alone. When find, with and at are all
   literals, the fit is checked here and the call is the one that ends
   in `_fixed`, which cannot fail. A pattern literal otherwise calls
   `patch_literal`, which fails with `LengthMismatch` alone. The span of a
   literal pattern can be as short as the fewest bytes its group matches,
   and that is what with must fit. */
static bool patch_call(struct checker *c, struct expr *e)
{
    struct expr *field = e->as.call.callee;
    size_t given = e->as.call.arg_count;
    struct type *bytes = types_slice(c->types, sema_builtin(c, TYPE_U8));
    struct type *word = sema_builtin(c, TYPE_I64);
    struct expr *find;
    struct expr *with;
    struct expr *into;
    struct expr *offset;
    struct expr *limit;
    struct expr *name;
    struct expr **args;
    struct type *t;
    struct symbol *f;
    bool fixed;
    bool pattern;
    size_t i;
    bool ok = true;

    if (given < 2 || given > 5) {
        sema_error_at(c, e->pos, "`" METHOD_PATCH "` takes 2 to 5 arguments, "
                      "found %zu", given);
        return false;
    }
    find = e->as.call.args[0];
    with = e->as.call.args[1];
    into = given > 2 ? e->as.call.args[2] : NULL;
    offset = given > 3 ? e->as.call.args[3] : int_literal(c, e->pos, 0);
    limit = given > 4 ? e->as.call.args[4] : int_literal(c, e->pos, 0);
    if (find->kind == EXPR_PATTERN) {
        t = sema_check_expr(c, find, types_byte_regex(c->types));
    } else {
        t = sema_check_expr(c, find, NULL);
    }
    if (sema_is_error(t)) {
        return false;
    }
    pattern = types_is_regex(t);
    if (pattern && !sema_require(c, find, t, types_byte_regex(c->types))) {
        return false;
    }
    if (!pattern && !is_byte_slice(t)) {
        sema_error_at(c, find->pos, "`" METHOD_PATCH "` finds a `[]byte` or a "
                      "`" LANG_BYTE_REGEX "`, found `%s`", sema_tn(t));
        return false;
    }
    ok = check_arg(c, with, bytes);
    name = text_literal(c, e->pos, "", 0);
    if (into != NULL) {
        t = sema_check_expr(c, into, word);
        if (sema_is_error(t)) {
            ok = false;
        } else if (pattern && t->kind == TYPE_STR) {
            name = into;
            into = NULL;
        } else if (!sema_require(c, into, t, word)) {
            ok = false;
        } else if (!pattern &&
                   (into->kind != EXPR_INT || into->as.integer != 0)) {
            sema_error_at(c, into->pos, "`into` names a group of a pattern, "
                          "and a byte sequence has none, so it is 0");
            ok = false;
        }
    }
    ok = check_arg(c, offset, word) && ok;
    ok = check_arg(c, limit, word) && ok;
    if (!ok) {
        return false;
    }
    fixed = with->kind == EXPR_BYTES && offset->kind == EXPR_INT &&
            (find->kind == EXPR_PATTERN || find->kind == EXPR_BYTES);
    if (!pattern) {
        if (fixed && offset->as.integer + with->as.text.length >
                         find->as.text.length) {
            sema_error_at(c, with->pos, "`with` of %zu byte%s at offset %llu "
                          "does not fit the %zu byte%s of `find`",
                          with->as.text.length,
                          with->as.text.length == 1 ? "" : "s",
                          (unsigned long long)offset->as.integer,
                          find->as.text.length,
                          find->as.text.length == 1 ? "" : "s");
            return false;
        }
        f = regex_function(c, field->pos, "data." METHOD_PATCH,
                           fixed ? "patch_bytes_fixed" : "patch_bytes");
        if (f == NULL) {
            return false;
        }
        args = types_alloc_array(c->arena, 5, sizeof *args);
        args[0] = field->as.field.base;
        args[1] = find;
        args[2] = with;
        args[3] = offset;
        args[4] = limit;
        call_of(c, e, f, args, 5);
        return true;
    }
    if (find->kind == EXPR_PATTERN) {
        /* The group is known when into is absent or a literal. */
        bool known = name->kind == EXPR_STRING &&
                     (into == NULL || into->kind == EXPR_INT);
        long group = -1;
        fixed = fixed && known;
        if (known) {
            group = patch_group(c, find, name->as.text.length > 0 ? name
                                                                 : into,
                                e->pos);
            if (group < 0) {
                return false;
            }
            into = int_literal(c, e->pos, (uint64_t)group);
            name = text_literal(c, e->pos, "", 0);
        }
        if (fixed) {
            long least = pattern_least_bytes(find->as.text.bytes,
                                             find->as.text.length, group);
            if (offset->as.integer + with->as.text.length >
                (uint64_t)least) {
                if (group == 0) {
                    sema_error_at(c, with->pos, "`with` of %zu byte%s at "
                                  "offset %llu does not fit the match, which "
                                  "can be %ld byte%s long",
                                  with->as.text.length,
                                  with->as.text.length == 1 ? "" : "s",
                                  (unsigned long long)offset->as.integer,
                                  least, least == 1 ? "" : "s");
                } else {
                    sema_error_at(c, with->pos, "`with` of %zu byte%s at "
                                  "offset %llu does not fit group %ld of the "
                                  "pattern, which can be %ld byte%s long",
                                  with->as.text.length,
                                  with->as.text.length == 1 ? "" : "s",
                                  (unsigned long long)offset->as.integer,
                                  group, least, least == 1 ? "" : "s");
                }
                return false;
            }
        }
    }
    if (into == NULL) {
        into = int_literal(c, e->pos, 0);
    }
    f = regex_function(c, field->pos, "data." METHOD_PATCH,
                       find->kind != EXPR_PATTERN ? "patch"
                       : fixed                    ? "patch_fixed"
                                                  : "patch_literal");
    if (f == NULL) {
        return false;
    }
    args = types_alloc_array(c->arena, 8, sizeof *args);
    i = 0;
    args[i++] = field->as.field.base;
    args[i++] = find;
    if (find->kind == EXPR_PATTERN) {
        args[i++] = text_literal(c, find->pos, find->as.text.bytes,
                                 find->as.text.length);
    }
    args[i++] = with;
    args[i++] = into;
    args[i++] = name;
    args[i++] = offset;
    args[i++] = limit;
    call_of(c, e, f, args, i);
    return true;
}

/* DESIGN: `s.to_bytes()` and `data.to_text()` are the calls `to_bytes(s)`
   and `to_text(data)` of `anti.text`, which the module then imports, as
   an `f"..."` does. Both copy, and `to_text` fails when the bytes are no
   valid UTF-8. */
static bool convert_call(struct checker *c, struct expr *e, bool bytes)
{
    struct expr *field = e->as.call.callee;
    const char *function = bytes ? METHOD_TO_TEXT : METHOD_TO_BYTES;
    char what[32];
    struct symbol *f;
    struct expr **args;

    if (e->as.call.arg_count != 0) {
        sema_error_at(c, e->pos, "`%s` takes no arguments", function);
        return false;
    }
    snprintf(what, sizeof what, "%s.%s", bytes ? "data" : "s", function);
    f = module_function(c, &text_module, field->pos, what, function);
    if (f == NULL) {
        return false;
    }
    args = types_alloc_array(c->arena, 1, sizeof *args);
    args[0] = field->as.field.base;
    call_of(c, e, f, args, 1);
    return true;
}

/* `m.group(n)`, `m.group("name")` and the same of `took_part`: the calls
   of `anti.regex` with the match first. */
static bool match_call(struct checker *c, struct expr *e, struct type *t)
{
    struct expr *field = e->as.call.callee;
    const char *method = sema_name_is(&field->as.field.name, MATCH_GROUP)
                             ? MATCH_GROUP
                             : MATCH_TOOK_PART;
    struct type *word = sema_builtin(c, TYPE_I64);
    struct expr *arg;
    struct type *got;
    struct symbol *f;
    struct expr **args;
    char name[32];
    char what[32];
    bool named;

    if (type_is_nullable(t)) {
        sema_usable_pointer(c, field->as.field.base, t);
        return false;
    }
    if (e->as.call.arg_count != 1) {
        sema_error_at(c, e->pos, "`%s` takes 1 argument, found %zu", method,
                      e->as.call.arg_count);
        return false;
    }
    arg = e->as.call.args[0];
    got = sema_check_expr(c, arg, word);
    if (sema_is_error(got)) {
        return false;
    }
    named = got->kind == TYPE_STR;
    if (!named && !sema_require(c, arg, got, word)) {
        return false;
    }
    if (t->pattern != NULL && !group_known(c, t->pattern, arg, named)) {
        return false;
    }
    snprintf(name, sizeof name, "%s%s%s",
             types_is_byte_match(t) ? "bytes_" : "", method,
             named ? "_named" : "");
    snprintf(what, sizeof what, "m.%s", method);
    if ((f = regex_function(c, field->pos, what, name)) == NULL) {
        return false;
    }
    args = types_alloc_array(c->arena, 2, sizeof *args);
    args[0] = field->as.field.base;
    args[1] = arg;
    call_of(c, e, f, args, 2);
    return true;
}

/* Whether name is a method of `str`, `[]byte` or a match that the
   checker writes as a call of a library function. */
static bool is_method(const struct name *name)
{
    return str_method(name) != NULL || is_match_method(name) ||
           sema_name_is(name, METHOD_PATCH) ||
           sema_name_is(name, METHOD_TO_BYTES) ||
           sema_name_is(name, METHOD_TO_TEXT);
}

bool sema_pattern_call(struct checker *c, struct expr *e, struct type **fn)
{
    struct expr *callee = e->as.call.callee;
    const struct name *name = &callee->as.field.name;
    struct type *t;
    bool ok;

    if (!is_method(name)) {
        return false;
    }
    t = callee->as.field.checked
            ? callee->as.field.base->type
            : sema_check_expr(c, callee->as.field.base, NULL);
    callee->as.field.checked = true;
    if (sema_is_error(t)) {
        *fn = t;
        return true;
    }
    if ((t->kind == TYPE_STR || is_byte_slice(t)) &&
        str_method(name) != NULL) {
        ok = method_call(c, e, t->kind != TYPE_STR);
    } else if (is_byte_slice(t) && sema_name_is(name, METHOD_PATCH)) {
        ok = patch_call(c, e);
    } else if ((t->kind == TYPE_STR && sema_name_is(name, METHOD_TO_BYTES)) ||
               (is_byte_slice(t) && sema_name_is(name, METHOD_TO_TEXT))) {
        ok = convert_call(c, e, t->kind != TYPE_STR);
    } else if ((types_is_match(t) || types_is_maybe_match(t)) &&
               is_match_method(name)) {
        ok = match_call(c, e, t);
    } else {
        return false;
    }
    *fn = ok ? e->as.call.callee->type : sema_builtin(c, TYPE_ERROR);
    return true;
}

/* DESIGN: a group of a pattern literal is a field of its match, `m.1` or
   `m.year`, which is the call `m.group(1)` or `m.group("year")`. The
   checker knows the groups of the literal, so a group it lacks is an
   error here. A match of any other pattern has `group` alone. */
struct type *sema_match_field(struct checker *c, struct expr *e,
                              struct type *t)
{
    struct expr *base = e->as.field.base;
    struct name name = e->as.field.name;
    struct expr *callee;
    struct expr *arg;
    struct pos pos = e->pos;

    if (type_is_nullable(base->type)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (t->pattern == NULL) {
        sema_error_at(c, pos, "`Match` has no field `%.*s`, and a match of a "
                      "pattern that is no literal gives its groups by "
                      "`group`", (int)(name.length - (e->as.field.element
                                                          ? 1 : 0)),
                      name.text + (e->as.field.element ? 1 : 0));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (e->as.field.element) {
        size_t i;
        arg = sema_new_node(c, EXPR_INT, pos);
        for (i = 1; i < name.length; i++) {
            arg->as.integer = arg->as.integer * 10 +
                              (uint64_t)(name.text[i] - '0');
        }
        arg->spelling.bytes = name.text + 1;
        arg->spelling.length = name.length - 1;
    } else {
        arg = sema_new_node(c, EXPR_STRING, pos);
        arg->as.text.bytes = name.text;
        arg->as.text.length = name.length;
        arg->spelling = arg->as.text;
    }
    callee = sema_new_node(c, EXPR_FIELD, pos);
    callee->as.field.base = base;
    callee->as.field.name.text = MATCH_GROUP;
    callee->as.field.name.length = sizeof MATCH_GROUP - 1;
    callee->as.field.checked = true;
    memset(&e->as, 0, sizeof e->as);
    e->kind = EXPR_CALL;
    e->as.call.callee = callee;
    e->as.call.args = types_alloc_array(c->arena, 1, sizeof *e->as.call.args);
    e->as.call.args[0] = arg;
    e->as.call.arg_count = 1;
    return sema_check_call(c, e, NULL);
}

/* The test of the checked value, a match, as `value != none`, written
   over e. */
static void test_of(struct checker *c, struct expr *e, struct type *t)
{
    struct expr *value = sema_new_node(c, e->kind, e->pos);
    struct expr *none = sema_new_node(c, EXPR_NONE, e->pos);
    struct pos pos = e->pos;

    *value = *e;
    none->type = type_is_nullable(t) ? t : types_with_none(c->types, t);
    memset(e, 0, sizeof *e);
    e->kind = EXPR_BINARY;
    e->pos = pos;
    e->as.binary.op = TOKEN_NE;
    e->as.binary.left = value;
    e->as.binary.right = none;
    e->type = sema_builtin(c, TYPE_BOOL);
}

/* DESIGN: a match stands as a condition on its own, in `if`, `while`,
   `&&`, `||` and `!`, and means that it matched. The checker writes it as
   the comparison with `none`, which the narrowing rule reads, so
   `if m { }` narrows m as `if m != none { }` does. A `matches` that is
   only tested is marked first, so its call asks for no groups. */
struct type *sema_check_test(struct checker *c, struct expr *e)
{
    struct type *t;

    if (e->kind == EXPR_CALL && e->as.call.callee->kind == EXPR_FIELD &&
        sema_name_is(&e->as.call.callee->as.field.name, "matches")) {
        e->as.call.tested = true;
    }
    t = sema_check_expr(c, e, NULL);
    if (types_is_match(t)) {
        t = sema_whole_optional(t, e);
    }
    if (sema_is_error(t) || !types_is_maybe_match(t)) {
        return t;
    }
    test_of(c, e, t);
    return e->type;
}

/* A new statement of kind at pos. */
static struct stmt *new_stmt(struct checker *c, enum stmt_kind kind,
                             struct pos pos)
{
    struct stmt *s = arena_alloc(c->arena, sizeof *s);

    memset(s, 0, sizeof *s);
    s->kind = kind;
    s->pos = pos;
    return s;
}

/* DESIGN: `if let m = e { } else { }` on a value that may be `none`, a
   match among them, binds m in the first block, where it holds one, and
   runs the `else` where it does not. The checker writes it as the block
   `{ let m = e; if m != none { } else { } }` with m narrowed in the first
   block, so lowering reads a `let` and an `if`. The `else` is checked
   before m is declared, so it does not see m. */
void sema_if_let_none(struct checker *c, struct stmt *s, struct type *t)
{
    struct switch_arm *arm = &s->as.switch_stmt.arms[0];
    struct expr *value = s->as.switch_stmt.value;
    struct block *otherwise = s->as.switch_stmt.otherwise->as.block;
    struct block *body = arm->body->as.block;
    struct stmt *let = new_stmt(c, STMT_LET, s->pos);
    struct stmt *test = new_stmt(c, STMT_IF, s->pos);
    struct if_branch *branch = arena_alloc(c->arena, sizeof *branch);
    struct block *both = arena_alloc(c->arena, sizeof *both);
    struct expr *cond = sema_new_node(c, EXPR_NAME, arm->pos);
    struct scope scope;
    struct scope inner;
    struct symbol *sym;

    sema_check_block(c, otherwise);
    let->as.let.name = arm->value->as.name;
    let->as.let.name_pos = arm->pos;
    let->as.let.value = value;
    sema_enter_scope(c, &scope);
    sym = sema_declare(c, SYMBOL_LOCAL, &let->as.let.name, arm->pos,
                       "`%.*s` is already declared in this block");
    if (sym != NULL) {
        sym->type = t;
        sym->stmt = let;
        let->as.let.symbol = sym;
        /* A failing call writes its result through a pointer into the
           local, as it does for any `let`. */
        if (value->kind == EXPR_CALL && value->as.call.out != NULL) {
            sym->address_taken = true;
        }
        cond->as.name = let->as.let.name;
        cond->symbol = sym;
        cond->type = t;
        test_of(c, cond, t);
        sema_enter_scope(c, &inner);
        sema_narrow(c, sym, types_without_none(c->types, t));
        sema_check_block(c, body);
        sema_leave_scope(c, &inner);
    }
    sema_leave_scope(c, &scope);
    branch->cond = cond;
    branch->body = body;
    test->as.if_chain.branches = branch;
    test->as.if_chain.count = 1;
    test->as.if_chain.else_body = otherwise;
    both->pos = s->pos;
    both->end = s->pos;
    both->stmts = types_alloc_array(c->arena, 2, sizeof *both->stmts);
    both->stmts[0] = let;
    both->stmts[1] = test;
    both->count = 2;
    s->kind = STMT_BLOCK;
    memset(&s->as, 0, sizeof s->as);
    s->as.block = both;
}
