#ifndef ANTI_BINDEXPR_H
#define ANTI_BINDEXPR_H

/* The interface of bindexpr.c: the evaluator of a constant expression of
   C, the text of a float as Anti writes it, and the constants of a
   binding that the evaluator gives the model. */

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

/* The two's complement value of the 64 bits of x. C11 leaves the
   conversion of an unsigned value above INT64_MAX to the implementation,
   so the value is computed rather than converted. */
int64_t bind_signed(uint64_t x);

/* Whether the scalar Anti type is unsigned: u8 to u64, c_uchar to
   c_ulonglong and c_size_t. */
bool bind_is_unsigned_type(const char *type);

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

/* Append a float as Anti writes it, with the digits of type that give
   the same value back. Digits without a point gain `.0`. */
void bind_float_text(double f, const char *type, struct text *out);

/* Add a `pub const` of the value to b. */
bool bind_eval_const(struct bind_module *b, const char *name,
                     const struct bind_eval *v, const char *doc);

/* Add a `pub const` of the struct literal of r whose text after the `{`
   is elements, the `{ 1, 2.5f }` of a define of a description and of a
   compound literal of a header alike. Each element is a constant
   expression, through lookup, of a field that is no bitfield and has a
   scalar type or none Anti can name. The literal names every field.
   Returns false for any other literal. */
bool bind_struct_const(struct bind_module *b, const char *name,
                       const struct bind_record *r, const char *elements,
                       bind_lookup lookup, void *context, const char *doc);

#endif
