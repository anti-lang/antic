#ifndef ANTI_BINDEXPR_H
#define ANTI_BINDEXPR_H

#include <stdbool.h>
#include <stdint.h>

#include "bindmodel.h"

/* The value of a constant expression of C. */
enum bind_eval_kind { BIND_EVAL_INT, BIND_EVAL_FLOAT, BIND_EVAL_STRING };

struct bind_eval {
    enum bind_eval_kind kind;
    int64_t i;
    double f;
    const char *text;           /* BIND_EVAL_STRING */
    const char *type;           /* the Anti type: c_int, c_float, str */
    /* An integer that is one enumerator alone keeps its enum, so the
       constant takes the enum's type. */
    const char *enum_type;
    const char *enum_value;
};

/* The value of a name inside an expression, a macro or an enumerator
   evaluated before. Returns false for a name it does not know. */
typedef bool (*bind_lookup)(void *context, const char *name,
                            struct bind_eval *out);

/* Evaluate expr, the text of a macro or of a define. It may hold the
   integer, float and string literals of C, names through lookup and casts
   to an arithmetic type. It may hold the unary operators - + ~ and the
   binary operators of arithmetic, shifts and bits. Returns false when
   expr is anything else, or divides by zero. */
bool bind_eval(struct bind_module *b, const char *expr, bind_lookup lookup,
               void *context, struct bind_eval *out);

/* Add a `pub const` of the value to b. */
bool bind_eval_const(struct bind_module *b, const char *name,
                     const struct bind_eval *v, const char *doc);

#endif
