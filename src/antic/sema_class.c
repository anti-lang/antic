/* The class model: the tables a class fills and the bodies that fill
   them, the interfaces it implements, its contracts, its hooks and
   operators, its `construct` and the nested types its signatures may
   not name. sema.c declares the classes, their fields and their
   members first, and sema_check_classes checks each class once the
   whole module is declared. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sema_checker.h"

/* The class or interface that the qualifier q of a `concrete fn` of t
   names, or NULL. q may name t or a class of its chain. It may also name
   an interface that a class of the chain implements, or a class of the
   chain of that interface. */
static const struct type *qualified_table(const struct type *t,
                                          const struct name *q)
{
    const struct type *up;
    size_t k;

    for (up = t; up != NULL; up = sema_inherited(up)) {
        if (sema_same_name(q, &up->name)) {
            return up;
        }
        for (k = 0; k < up->field_count; k++) {
            const struct type *iface;
            if (up->fields[k].form != FIELD_IMPL) {
                continue;
            }
            for (iface = up->fields[k].type; iface != NULL;
                 iface = sema_inherited(iface)) {
                if (sema_same_name(q, &iface->name)) {
                    return iface;
                }
            }
        }
    }
    return NULL;
}

/* The function name that the primary table of the chain of t holds, or
   NULL. owner receives the class that declares it. */
static const struct item *chain_entry(const struct type *t,
                                      const struct name *name,
                                      const struct type **owner)
{
    const struct item *found = types_primary_member(t, name);
    size_t i;

    for (; found != NULL && t != NULL; t = sema_inherited(t)) {
        for (i = 0; i < t->member_count; i++) {
            if (t->members[i] == found) {
                *owner = t;
                return found;
            }
        }
    }
    return NULL;
}

/* Whether the level t itself fills the table of iface with a body
   qualified by a class of the chain of iface. That body wins in the
   table over an unqualified one of its level, as lowering fills it. A
   level above counts for nothing, because the nearest body wins. */
static bool qualified_body(const struct type *t, const struct type *iface,
                           const struct name *name)
{
    const struct type *chain;
    size_t i;

    for (i = 0; i < t->member_count; i++) {
        const struct item *m = t->members[i];
        if (m->kind != ITEM_FN || m->qualifier.length == 0 ||
            !sema_same_name(&m->name, name)) {
            continue;
        }
        for (chain = iface; chain != NULL; chain = sema_inherited(chain)) {
            if (sema_same_name(&m->qualifier, &chain->name)) {
                return true;
            }
        }
    }
    return false;
}

/* The parameters a signature writes: those of its type without `self`
   and without the out pointer of a `may fail` function. */
static size_t written_params(const struct type *fn, bool has_self)
{
    size_t count = fn->param_count - (has_self ? 1 : 0);
    return fn->may_fail && fn->has_out ? count - 1 : count;
}

/* The result a signature writes, or NULL when it writes none. */
static const struct type *written_result(const struct type *fn)
{
    if (fn->may_fail) {
        return fn->has_out ? fn->params[fn->param_count - 1]->element : NULL;
    }
    return fn->result->kind == TYPE_VOID ? NULL : fn->result;
}

/* A result for a message: the type in backquotes, or `nothing`. */
static void returned_text(char *out, size_t size, const struct type *t)
{
    if (t == NULL) {
        sema_format_to(out, size, "nothing");
    } else {
        sema_format_to(out, size, "`%s`", sema_tn(t));
    }
}

static bool owns_param(const struct symbol *sym, size_t index)
{
    return sym->owned != NULL && index < sym->owned_count && sym->owned[index];
}

/* DESIGN: the root declares `serialize(self, out: *text.Builder)`. The
   checker builds the root before any module and cannot name anti.text.
   The parameter therefore has the type `*Object`, which takes a builder
   as it takes any class. A replacement writes the parameter the
   specification gives, a `*text.Builder`. */
static bool root_builder(const struct item *entry)
{
    return entry->runtime != NULL && sema_name_is(&entry->name, "serialize");
}

static bool builder_pointer(const struct type *t)
{
    return t->kind == TYPE_POINTER && !t->nullable &&
           t->element->kind == TYPE_CLASS &&
           sema_name_is(&t->element->module, "anti.text") &&
           sema_name_is(&t->element->name, "Builder");
}

/* DESIGN: a `concrete fn` fills an entry with the signature of the
   function that declared the entry, exactly: `self`, the type and the
   `own` of each parameter, the result and `may fail`. A call through the
   table passes what that signature says, so a body that took anything
   else would read arguments that are not there. The message names the
   first difference in the order of the text. */
static bool same_signature(struct checker *c, const struct item *m,
                           const struct item *entry, const struct type *owner)
{
    const struct type *mine = m->symbol != NULL ? m->symbol->type : NULL;
    const struct type *theirs =
        entry->symbol != NULL ? entry->symbol->type : NULL;
    size_t extra = m->has_self ? 1 : 0;
    size_t count;
    size_t their_count;
    const struct type *result;
    const struct type *their_result;
    char fn[160];
    char at[160];
    size_t i;

    if (mine == NULL || theirs == NULL || mine->kind != TYPE_FN ||
        theirs->kind != TYPE_FN) {
        return true;
    }
    /* A function of a copy of a generic takes the arguments of the copy
       in place of the parameters. */
    theirs = sema_member_type(c, (struct type *)theirs, owner);
    sema_format_to(fn, sizeof fn, "concrete fn %.*s%s%.*s",
                   (int)m->qualifier.length, m->qualifier.text,
                   m->qualifier.length > 0 ? "::" : "", (int)m->name.length,
                   m->name.text);
    sema_format_to(at, sizeof at, "%s.%.*s", sema_tn(owner),
                   (int)entry->name.length,
                   entry->name.text);
    if (m->has_self != entry->has_self) {
        sema_error_at(c, m->name_pos, "`%s` %s `self`, and `%s` %s", fn,
                      m->has_self ? "takes" : "does not take", at,
                      m->has_self ? "does not" : "does");
        return false;
    }
    count = written_params(mine, m->has_self);
    their_count = written_params(theirs, entry->has_self);
    for (i = 0; i < count && i < their_count && i < m->param_count; i++) {
        const struct param *p = &m->params[i];
        const struct type *got = mine->params[i + extra];
        const struct type *want = theirs->params[i + extra];
        bool owned = owns_param(m->symbol, i + extra);
        bool builder = root_builder(entry);
        if (owned != owns_param(entry->symbol, i + extra)) {
            sema_error_at(c, p->pos,
                          owned ? "`%.*s` of `%s` is `own`, and `%s` "
                                  "does not take it as `own`"
                                : "`%.*s` of `%s` is not `own`, and "
                                  "`%s` takes it as `own`",
                          (int)p->name.length, p->name.text, fn, at);
            return false;
        }
        if (builder ? !builder_pointer(got) : got != want) {
            sema_error_at(c, p->type->pos,
                          "`%.*s` of `%s` has type `%s`, and `%s` "
                          "takes `%s`", (int)p->name.length, p->name.text, fn,
                          sema_tn(got), at,
                          builder ? "*Builder" : sema_tn(want));
            return false;
        }
    }
    if (count != their_count) {
        char takes[48];
        if (count == 0) {
            sema_format_to(takes, sizeof takes, "no parameter");
        } else {
            sema_format_to(takes, sizeof takes, "%zu parameter%s", count,
                           count == 1 ? "" : "s");
        }
        sema_error_at(c, m->name_pos, "`%s` takes %s%s, and `%s` takes %zu", fn,
                      takes, m->has_self ? " besides `self`" : "", at,
                      their_count);
        return false;
    }
    result = written_result(mine);
    their_result = written_result(theirs);
    if (result != their_result) {
        char given[100];
        char wanted[100];
        returned_text(given, sizeof given, result);
        returned_text(wanted, sizeof wanted, their_result);
        sema_error_at(c, m->result != NULL ? m->result->pos : m->name_pos,
                      "`%s` returns %s, and `%s` returns %s", fn, given, at,
                      wanted);
        return false;
    }
    if (mine->may_fail != theirs->may_fail) {
        sema_error_at(c, m->may_fail ? m->may_fail_pos : m->name_pos,
                      mine->may_fail ? "`%s` may fail, and `%s` cannot"
                                     : "`%s` cannot fail, and `%s` may fail",
                      fn, at);
        return false;
    }
    return true;
}

/* Whether two bodies of t with two qualifiers fill one table. Both may
   name a class of the base chain. Both may name a class of the chain of
   one interface that the chain of t implements. */
static bool one_table(const struct type *t, const struct item *a,
                      const struct item *b)
{
    enum body_table a_table = types_body_table(t, a);
    const struct type *up;
    const struct type *chain;
    size_t k;

    if (a_table != types_body_table(t, b) || a_table == BODY_PLAIN) {
        return false;
    }
    if (a_table == BODY_BASE) {
        return true;
    }
    for (up = t; up != NULL; up = sema_inherited(up)) {
        for (k = 0; k < up->field_count; k++) {
            bool has_a = false;
            bool has_b = false;
            if (up->fields[k].form != FIELD_IMPL) {
                continue;
            }
            for (chain = up->fields[k].type; chain != NULL;
                 chain = sema_inherited(chain)) {
                has_a = has_a || sema_same_name(&a->qualifier, &chain->name);
                has_b = has_b || sema_same_name(&b->qualifier, &chain->name);
            }
            if (has_a && has_b) {
                return true;
            }
        }
    }
    return false;
}

/* Whether the `concrete fn` m of the class it may fill entry, which
   owner declares. A `final fn` has no replacement, since a call of one
   is direct and would pass over the body of m. Any other entry takes the
   signature of m as `same_signature` compares it. */
static bool may_fill(struct checker *c, const struct item *it,
                     const struct item *m, const struct item *entry,
                     const struct type *owner)
{
    if (entry->is_final) {
        sema_error_at(c, m->name_pos, "`%.*s.%.*s` replaces `final` "
                      "function `%.*s.%.*s`", (int)it->name.length,
                      it->name.text, (int)m->name.length, m->name.text,
                      (int)owner->name.length, owner->name.text,
                      (int)entry->name.length, entry->name.text);
        return false;
    }
    return same_signature(c, m, entry, owner);
}

/* The entries a `concrete fn` m of t fills, each compared with m. A
   qualified body fills the table its qualifier names. An unqualified one
   fills the entry of the base chain, unless a body qualified by a base
   holds it. It also fills the entry of every interface of the chain that
   no qualified body fills. One that fills none of them is refused. */
static void check_replacement(struct checker *c, const struct item *it,
                              const struct type *t, const struct item *m)
{
    const struct type *owner = NULL;
    const struct item *entry;
    const struct type *up;
    bool filled = false;
    size_t k;

    if (m->qualifier.length > 0 && !sema_same_name(&m->qualifier, &t->name)) {
        const struct type *table = qualified_table(t, &m->qualifier);
        if (table == NULL) {
            return;
        }
        entry = chain_entry(table, &m->name, &owner);
        if (entry == NULL) {
            sema_error_at(c, m->name_pos, "`concrete fn %.*s::%.*s` of `%.*s` "
                          "fills no abstract function",
                          (int)m->qualifier.length,
                          m->qualifier.text, (int)m->name.length, m->name.text,
                          (int)it->name.length, it->name.text);
            return;
        }
        may_fill(c, it, m, entry, owner);
        return;
    }
    entry = types_holds_entry(t, m)
                ? chain_entry(sema_inherited(t), &m->name, &owner)
                : NULL;
    if (entry != NULL && !may_fill(c, it, m, entry, owner)) {
        return;
    }
    filled = entry != NULL;
    for (up = t; up != NULL; up = sema_inherited(up)) {
        for (k = 0; k < up->field_count; k++) {
            const struct type *iface = up->fields[k].type;
            if (up->fields[k].form != FIELD_IMPL ||
                qualified_body(t, iface, &m->name)) {
                continue;
            }
            entry = chain_entry(iface, &m->name, &owner);
            if (entry != NULL && !may_fill(c, it, m, entry, owner)) {
                return;
            }
            filled = filled || entry != NULL;
        }
    }
    if (!filled) {
        sema_error_at(c, m->name_pos, "`concrete fn %.*s` of `%.*s` fills no "
                      "abstract function", (int)m->name.length, m->name.text,
                      (int)it->name.length, it->name.text);
    }
}

/* DESIGN: `implements name: I` places a sub-object of the abstract
   class I inside the class. Only an abstract class may be implemented,
   and one class implements an interface once, so that every name it
   provides has one path. */
static void check_implements(struct checker *c, const struct item *it,
                             const struct type *t)
{
    size_t j;

    for (j = 0; j < t->field_count; j++) {
        const struct type *iface = t->fields[j].type;
        const struct type *chain;
        size_t k;
        if (t->fields[j].form != FIELD_IMPL) {
            continue;
        }
        if (iface == NULL || iface->kind != TYPE_CLASS ||
            !iface->has_abstract) {
            sema_error_at(c, t->fields[j].pos, "`%s` is not abstract and "
                          "cannot be implemented", sema_tn(iface));
            continue;
        }
        for (k = 0; k < j; k++) {
            if (t->fields[k].form == FIELD_IMPL &&
                t->fields[k].type == iface) {
                sema_error_at(c, t->fields[j].pos,
                              "`%.*s` implements `%s` twice",
                              (int)it->name.length, it->name.text,
                              sema_tn(iface));
            }
        }
        for (chain = sema_inherited(t); chain != NULL;
             chain = sema_inherited(chain)) {
            for (k = 0; k < chain->field_count; k++) {
                if (chain->fields[k].form == FIELD_IMPL &&
                    chain->fields[k].type == iface) {
                    sema_error_at(c, t->fields[j].pos, "`%.*s` implements "
                                  "`%s`, which `%.*s` implements already",
                                  (int)it->name.length, it->name.text,
                                  sema_tn(iface), (int)chain->name.length,
                                  chain->name.text);
                }
            }
        }
    }
}

/* A plain or `use` field of an abstract class is refused here, after
   every class knows whether a contract reaches it. The base and an
   interface sub-object are the two places an abstract class is a
   value, so they are skipped. */
static void refuse_abstract_fields(struct checker *c, const struct type *t)
{
    size_t j;

    for (j = 0; j < t->field_count; j++) {
        char what[96];
        if (t->fields[j].form == FIELD_BASE ||
            t->fields[j].form == FIELD_TABLE ||
            t->fields[j].form == FIELD_IMPL) {
            continue;
        }
        sema_format_to(what, sizeof what, "the field `%.*s`",
                       (int)t->fields[j].name.length, t->fields[j].name.text);
        sema_refuse_abstract_value(c, t->fields[j].pos, what,
                                   t->fields[j].type);
    }
}

/* The function of the chain or of an implemented interface that the
   member m of t matches by name, or NULL. */
static const struct item *matched_above(const struct type *t,
                                        const struct item *m)
{
    const struct item *above = NULL;
    const struct type *base;

    if (sema_inherited(t) != NULL) {
        above = types_primary_member(sema_inherited(t), &m->name);
    }
    /* An interface declares functions the class fills, so a
       `concrete fn` matches there as well as in the base chain. An
       interface a base implements counts, because a body below replaces
       the one of the base in its table. */
    for (base = t; above == NULL && base != NULL; base = sema_inherited(base)) {
        size_t k;
        for (k = 0; above == NULL && k < base->field_count; k++) {
            const struct type *iface;
            if (base->fields[k].form != FIELD_IMPL) {
                continue;
            }
            for (iface = base->fields[k].type;
                 iface != NULL &&
                 above == NULL; iface = sema_inherited(iface)) {
                const struct item *found = sema_find_member(iface, &m->name);
                if (found != NULL && found->kind == ITEM_FN) {
                    above = found;
                }
            }
        }
    }
    return above;
}

/* DESIGN: a contract is declared with `abstract fn` and filled with
   `concrete fn` of the same signature. The checker walks the chain of
   `inherits` fields of every struct and refuses one that leaves a
   contract unfilled, and `check_replacement` compares each `concrete
   fn` with the entries it fills. */
static void check_contracts(struct checker *c, const struct item *it,
                            const struct type *t)
{
    size_t j;

    for (j = 0; j < it->member_count; j++) {
        struct item *m = it->members[j];
        const struct item *above;
        /* A function with type parameters of its own that is abstract
           or replaced is refused where it is declared. It fills and
           leaves open nothing here. */
        if (m->kind != ITEM_FN ||
            (m->type_param_count > 0 && m->contract != FN_PLAIN)) {
            continue;
        }
        above = matched_above(t, m);
        if (m->contract == FN_ABSTRACT) {
            continue;
        }
        if (m->contract == FN_CONCRETE && above == NULL) {
            sema_error_at(c, m->name_pos, "`concrete fn %.*s` of `%.*s` fills "
                          "no abstract function", (int)m->name.length,
                          m->name.text, (int)it->name.length, it->name.text);
        } else if (m->contract == FN_CONCRETE) {
            check_replacement(c, it, t, m);
        } else if (m->contract == FN_PLAIN && above != NULL &&
                   above->contract != FN_PLAIN) {
            sema_error_at(c, m->name_pos, "`%.*s` of `%.*s` matches an "
                          "abstract function and needs `concrete`",
                          (int)m->name.length, m->name.text,
                          (int)it->name.length, it->name.text);
        }
    }
}

/* DESIGN: a struct may not redeclare a name that its chain already
   has. A `concrete fn` is the exception, because it fills the abstract
   function of that name. */
static void refuse_redeclared(struct checker *c, const struct item *it,
                              const struct type *t)
{
    const struct type *base;
    size_t j;

    for (base = sema_inherited(t); base != NULL; base = sema_inherited(base)) {
        for (j = 0; j < t->field_count; j++) {
            if (t->fields[j].form == FIELD_BASE ||
                t->fields[j].form == FIELD_TABLE) {
                continue;
            }
            if (sema_find_field(base, &t->fields[j].name) != NULL ||
                sema_find_member(base, &t->fields[j].name) != NULL) {
                sema_error_at(c, t->fields[j].pos, "`%.*s` already has `%.*s`",
                              (int)base->name.length, base->name.text,
                              (int)t->fields[j].name.length,
                              t->fields[j].name.text);
            }
        }
        for (j = 0; j < it->member_count; j++) {
            const struct item *m = it->members[j];
            const struct item *shadowed;
            /* `construct` and `destruct` repeat down a chain by design,
               because the compiler runs one body per level. A `concrete
               fn` replaces an entry, and an `abstract fn` re-opens one,
               which an interface does when it names a function of the
               root. */
            if (m->contract != FN_PLAIN ||
                sema_name_is(&m->name, "construct") ||
                sema_name_is(&m->name, "destruct")) {
                continue;
            }
            /* `operator fn hash` has a refusal of its own, which names
               the form that replaces `hash`. */
            if (m->is_operator && sema_name_is(&m->name, LANG_HOOK_HASH) &&
                t->kind == TYPE_CLASS) {
                continue;
            }
            /* DESIGN: a static function is namespaced by its class and
               reached as `Class.f`, never through a value and never
               through a table. Two statics of one name in a chain name
               two functions and no call is ambiguous, so the rule leaves
               them. A function that takes `self` is another matter, and
               so is a field. */
            shadowed = sema_find_member(base, &m->name);
            if (!m->has_self && sema_find_field(base, &m->name) == NULL &&
                shadowed != NULL && shadowed->kind == ITEM_FN &&
                !shadowed->has_self) {
                continue;
            }
            if (sema_find_field(base, &m->name) != NULL || shadowed != NULL) {
                sema_error_at(c, m->name_pos, "`%.*s` already has `%.*s`",
                              (int)base->name.length, base->name.text,
                              (int)m->name.length, m->name.text);
            }
        }
    }
}

/* DESIGN: every abstract function of the chain needs a concrete one at
   or below the class that declares it. An abstract class may leave one
   open. It is never a complete value, and every class below it is
   checked here. */
static void require_filled_chain(struct checker *c, const struct item *it,
                                 const struct type *t)
{
    const struct type *base;
    size_t j;

    for (base = sema_inherited(t); base != NULL && !it->is_abstract;
         base = sema_inherited(base)) {
        for (j = 0; j < base->member_count; j++) {
            const struct item *a = base->members[j];
            const struct item *filled;
            if (a->kind != ITEM_FN || a->contract != FN_ABSTRACT ||
                a->type_param_count > 0) {
                continue;
            }
            filled = types_primary_member(t, &a->name);
            if (filled == NULL || filled->contract != FN_CONCRETE) {
                sema_error_at(c, it->name_pos, "`%.*s` lacks `concrete fn "
                              "%.*s`", (int)it->name.length, it->name.text,
                              (int)a->name.length, a->name.text);
            }
        }
    }
}

/* DESIGN: a language hook has the signature its construct calls, and
   the message states that signature. No hook may fail, since the
   construct that calls it has no place for a handler. `set_index` takes
   the index and the element that `index` of the same type reads, so
   `e[i] = e[i]` holds for every type that has both. A class writes the
   receiver `self`, and a free function of a struct its first parameter
   as declared.

   DESIGN: every class has `hash` from `anti.lang.Object`, and replaces
   it with `concrete fn hash(self) -> u64`, as it replaces every function
   of the root. `operator fn hash` in a class body would declare a second
   function of that name, so it is refused with the form that replaces
   it. A free `operator fn hash` of the module of a class, beside its
   free `operator fn eq`, is the class's hash, as a collection declares
   it. */
static void check_hook(struct checker *c, const struct item *m,
                       struct type *owner)
{
    const struct type *sig = m->symbol != NULL ? m->symbol->type : NULL;
    char self[128];
    bool ok;

    if (sig == NULL || sig->kind != TYPE_FN) {
        return;
    }
    if (m->has_self || m->param_count == 0) {
        snprintf(self, sizeof self, "self");
    } else {
        snprintf(self, sizeof self, "%.*s: %s", (int)m->params[0].name.length,
                 m->params[0].name.text, sema_tn(sig->params[0]));
    }
    if (sema_name_is(&m->name, LANG_HOOK_NEXT)) {
        ok = !sig->may_fail && sig->param_count == 1 &&
             sig->result->kind == TYPE_BOOL;
        if (!ok) {
            sema_error_at(c, m->name_pos, "`operator fn next` is written "
                          "`operator fn next(%s) -> bool`", self);
        }
    } else if (sema_name_is(&m->name, LANG_HOOK_VALUE)) {
        ok = !sig->may_fail && sig->param_count == 1 &&
             sig->result->kind != TYPE_VOID;
        if (!ok) {
            sema_error_at(c, m->name_pos, "`operator fn value` is written "
                          "`operator fn value(%s) -> T`", self);
        }
    } else if (sema_name_is(&m->name, LANG_HOOK_ITER)) {
        ok = !sig->may_fail && sig->param_count == 1 &&
             sema_is_iterator(c, sig->result);
        if (!ok) {
            sema_error_at(c, m->name_pos, "`operator fn iter` is written "
                          "`operator fn iter(%s) -> I`, where `I` has "
                          "`operator fn next` and `operator fn value`", self);
        }
    } else if (sema_name_is(&m->name, LANG_HOOK_INDEX)) {
        ok = !sig->may_fail && sig->param_count >= 2 &&
             sig->result->kind != TYPE_VOID;
        if (!ok) {
            sema_error_at(c, m->name_pos, "`operator fn index` is written "
                          "`operator fn index(%s, i: I) -> T`", self);
        }
    } else if (sema_name_is(&m->name, LANG_HOOK_HASH)) {
        if (owner != NULL && owner->kind == TYPE_CLASS && m->has_self) {
            sema_error_at(c, m->name_pos, "a class replaces `hash` of "
                          "`Object` with `concrete fn hash(self) -> u64`");
            return;
        }
        ok = !sig->may_fail && sig->param_count == 1 &&
             sig->result->kind == TYPE_U64;
        if (!ok) {
            sema_error_at(c, m->name_pos, "`operator fn hash` is written "
                          "`operator fn hash(%s) -> u64`", self);
        }
    } else if (sema_name_is(&m->name, LANG_HOOK_SET_INDEX)) {
        struct symbol *index = sema_hook(c, owner, LANG_HOOK_INDEX);
        const struct type *read = index != NULL ? index->type : NULL;
        bool paired = read != NULL && read->kind == TYPE_FN &&
                      !read->may_fail && read->param_count >= 2 &&
                      read->result->kind != TYPE_VOID;
        size_t i;
        ok = !sig->may_fail && sig->param_count >= 3 &&
             sig->result->kind == TYPE_VOID;
        if (ok && paired) {
            ok = sig->param_count == read->param_count + 1 &&
                 sig->params[read->param_count] == read->result;
            for (i = 1; ok && i < read->param_count; i++) {
                ok = sig->params[i] == read->params[i];
            }
        }
        if (!ok && paired) {
            /* The indices of `index` in order, then the element. */
            struct text indices = {0};
            for (i = 1; i < read->param_count; i++) {
                if (read->param_count == 2) {
                    text_appendf(&indices, "i: %s, ", sema_tn(read->params[i]));
                } else {
                    text_appendf(&indices, "i%zu: %s, ", i,
                                 sema_tn(read->params[i]));
                }
            }
            sema_error_at(c, m->name_pos, "`operator fn set_index` is "
                          "written `operator fn set_index(%s, %sv: %s)`, as "
                          "`index` reads", self, text_cstr(&indices),
                          sema_tn(read->result));
            text_free(&indices);
        } else if (!ok) {
            sema_error_at(c, m->name_pos, "`operator fn set_index` is "
                          "written `operator fn set_index(%s, i: I, v: T)`",
                          self);
        }
    }
}

/* An `operator fn` carries one of the twenty names the table holds,
   and nothing else, and a hook the signature its construct calls. owner
   is the type the functions belong to. */
static void check_operator_item(struct checker *c, const struct item *m,
                                struct type *owner)
{
    if (m->kind != ITEM_FN || !m->is_operator) {
        return;
    }
    if (!sema_operator_named(&m->name)) {
        sema_error_at(c, m->name_pos, "`operator fn` takes one of `add sub "
                      "mul div rem neg eq lt and or xor shl shr not iter "
                      "next value index set_index hash`");
        return;
    }
    check_hook(c, m, owner);
}

static void check_operator_names(struct checker *c, const struct item *it,
                                 struct type *t)
{
    size_t j;

    for (j = 0; j < it->member_count; j++) {
        check_operator_item(c, it->members[j], t);
    }
}

/* The operators of a struct are free functions of its module, and the
   type they belong to is the one their first parameter names. */
static void check_free_operators(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        const struct item *it = module->items[i];
        struct type *owner = NULL;
        if (it->kind != ITEM_FN || !it->is_operator || it->symbol == NULL ||
            it->symbol->type == NULL || it->symbol->type->kind != TYPE_FN) {
            continue;
        }
        if (it->symbol->type->param_count > 0) {
            owner = it->symbol->type->params[0];
        }
        check_operator_item(c, it, owner);
    }
}

/* DESIGN: one `construct` per class, and every alternative is a static
   function with a name of its own. A `construct` without arguments
   cannot fail and returns nothing. */
static void check_one_construct(struct checker *c, const struct item *it)
{
    const struct item *first = NULL;
    size_t j;

    for (j = 0; j < it->member_count; j++) {
        struct item *m = it->members[j];
        if (m->kind != ITEM_FN || !sema_name_is(&m->name, "construct")) {
            continue;
        }
        if (first != NULL) {
            sema_error_at(c, m->name_pos, "`%.*s` has one `construct`, "
                          "and every other maker is a static function",
                          (int)it->name.length, it->name.text);
        }
        first = m;
        if (!m->has_self) {
            sema_error_at(c, m->name_pos,
                          "`construct` takes `self` as its first parameter");
        } else if (m->param_count == 0 && m->result != NULL) {
            sema_error_at(c, m->name_pos, "a `construct` without "
                          "arguments cannot fail and returns nothing");
        }
    }
}

/* DESIGN: the qualifier of a `concrete fn` names the table it fills:
   the class itself, a class of its chain, or an interface it
   implements. Any other name reaches no table. Two qualifiers that
   reach one table fill it twice, which is refused as a name declared
   twice. */
static void check_qualifiers(struct checker *c, const struct item *it,
                             const struct type *t)
{
    size_t j;

    for (j = 0; j < it->member_count; j++) {
        const struct item *m = it->members[j];
        size_t k;
        if (m->kind != ITEM_FN || m->qualifier.length == 0) {
            continue;
        }
        if (qualified_table(t, &m->qualifier) == NULL) {
            sema_error_at(c, m->qualifier_pos, "`%.*s` is no base and no "
                          "interface of `%.*s`", (int)m->qualifier.length,
                          m->qualifier.text, (int)it->name.length,
                          it->name.text);
            continue;
        }
        for (k = 0; k < j; k++) {
            const struct item *other = it->members[k];
            if (other->kind == ITEM_FN &&
                sema_same_name(&other->name, &m->name) &&
                other->qualifier.length > 0 &&
                !sema_same_name(&other->qualifier, &m->qualifier) &&
                one_table(t, other, m)) {
                sema_error_at(c, m->name_pos, "`%.*s` declares `%.*s` twice",
                              (int)it->name.length, it->name.text,
                              (int)m->name.length, m->name.text);
            }
        }
    }
}

/* An interface leaves its functions open, and the class that
   implements it fills them. The chain of the interface counts, because
   an interface may inherit another abstract class. */
static void require_filled_interfaces(struct checker *c,
                                      const struct item *it,
                                      const struct type *t)
{
    size_t j;

    for (j = 0; j < t->field_count && !it->is_abstract; j++) {
        const struct type *iface;
        if (t->fields[j].form != FIELD_IMPL) {
            continue;
        }
        for (iface = t->fields[j].type; iface != NULL;
             iface = sema_inherited(iface)) {
            size_t k;
            for (k = 0; k < iface->member_count; k++) {
                const struct item *a = iface->members[k];
                const struct item *filled;
                if (a->kind != ITEM_FN || a->contract != FN_ABSTRACT) {
                    continue;
                }
                filled = types_interface_member(t, t->fields[j].type,
                                                &a->name);
                if (filled == NULL || filled->contract != FN_CONCRETE) {
                    sema_error_at(c, it->name_pos, "`%.*s` lacks `concrete "
                                  "fn %.*s` of `%s`", (int)it->name.length,
                                  it->name.text, (int)a->name.length,
                                  a->name.text, sema_tn(iface));
                }
            }
        }
    }
}

/* The type nested directly in cls that t names, or NULL. t may reach
   it through a pointer, an array, a slice, a channel, a Job, a tuple or
   a function type. A type nested deeper has no name in the body of
   cls. */
static const struct type *names_nested(const struct type *t,
                                       const struct item *cls)
{
    const struct type *found;
    size_t i;

    if (t == NULL) {
        return NULL;
    }
    for (i = 0; i < cls->nested_count; i++) {
        if (cls->nested[i]->symbol != NULL &&
            cls->nested[i]->symbol->type == t) {
            return t;
        }
    }
    switch (t->kind) {
    case TYPE_POINTER:
    case TYPE_ARRAY:
    case TYPE_SLICE:
        return names_nested(t->element, cls);
    case TYPE_FN:
    case TYPE_TUPLE:
        for (i = 0; i < t->param_count; i++) {
            if ((found = names_nested(t->params[i], cls)) != NULL) {
                return found;
            }
        }
        return names_nested(t->result, cls);
    case TYPE_STRUCT:
        /* A channel keeps its element and a Job its result. */
        if ((found = names_nested(t->element, cls)) != NULL) {
            return found;
        }
        return names_nested(t->result, cls);
    default:
        return NULL;
    }
}

static const char *level_word(enum visibility vis)
{
    return vis == VIS_PROTECTED ? "protected" : "public";
}

/* DESIGN: a type nested in a class is private to it, so code outside
   can neither create one nor receive one. A signature that code outside
   the class reaches therefore names none of them: a `pub` or `protected`
   function or field, a `pub` constant, and a `construct` that takes
   arguments, which `T(args)` calls from outside and which C reaches as
   `anti_<Class>_construct`. */
static void refuse_nested_in_signatures(struct checker *c,
                                        const struct item *it,
                                        const struct type *t)
{
    const struct type *found;
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        const struct struct_field *f = &t->fields[i];
        if (f->form != FIELD_PLAIN || f->vis == VIS_PRIVATE ||
            (found = names_nested(f->type, it)) == NULL) {
            continue;
        }
        sema_error_at(c, f->pos, "the %s field `%.*s` of `%.*s` names "
                      "`%s`, which is private to `%.*s`",
                      level_word(f->vis), (int)f->name.length, f->name.text,
                      (int)it->name.length, it->name.text, sema_tn(found),
                      (int)it->name.length, it->name.text);
    }
    for (i = 0; i < it->member_count; i++) {
        const struct item *m = it->members[i];
        const struct type *mt = NULL;
        bool made = m->kind == ITEM_FN && m->has_self &&
                    m->param_count > 0 && sema_name_is(&m->name, "construct");
        if (m->symbol == NULL || (m->vis == VIS_PRIVATE && !made)) {
            continue;
        }
        if (m->kind == ITEM_FN) {
            mt = m->symbol->type;
        } else if (m->type != NULL) {
            struct context quiet;
            sema_enter_quiet(c, &quiet);
            mt = sema_resolve_type(c, m->type);
            sema_leave(c, &quiet);
        }
        if ((found = names_nested(mt, it)) == NULL) {
            continue;
        }
        sema_error_at(c, m->name_pos, "the %s %s `%.*s` of `%.*s` names "
                      "`%s`, which is private to `%.*s`",
                      made ? "public" : level_word(m->vis),
                      m->kind == ITEM_FN ? "function" : "constant",
                      (int)m->name.length, m->name.text,
                      (int)it->name.length, it->name.text, sema_tn(found),
                      (int)it->name.length, it->name.text);
    }
}

/* The checks of every type with fields: its interfaces, its abstract
   fields, then its contracts and the functions of its body. */
void sema_check_classes(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        struct type *t = it->symbol != NULL && it->kind != ITEM_TYPE
                             ? it->symbol->type
                             : NULL;
        if (type_has_fields(t)) {
            check_implements(c, it, t);
        }
    }
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        struct type *t = it->symbol != NULL && it->kind != ITEM_TYPE
                             ? it->symbol->type
                             : NULL;
        if (type_has_fields(t)) {
            refuse_abstract_fields(c, t);
        }
    }
    check_free_operators(c);
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        struct type *t = it->symbol != NULL && it->kind != ITEM_TYPE
                             ? it->symbol->type
                             : NULL;
        if (!type_has_fields(t)) {
            continue;
        }
        check_contracts(c, it, t);
        refuse_redeclared(c, it, t);
        require_filled_chain(c, it, t);
        check_operator_names(c, it, t);
        check_one_construct(c, it);
        check_qualifiers(c, it, t);
        require_filled_interfaces(c, it, t);
        if (it->kind == ITEM_CLASS && it->nested_count > 0) {
            struct context outer;
            sema_enter_within(c, it, &outer);
            refuse_nested_in_signatures(c, it, t);
            sema_leave(c, &outer);
        }
    }
}
