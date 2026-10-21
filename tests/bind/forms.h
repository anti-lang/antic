/* Forms of a header that tests/bind/layout.h does not hold, for
   `anti bind --clang`: a header found through `-I`, an enum without a
   name, records declared inside a record, macros that name an enumerator,
   each other or nothing, a macro taken back, compound literals the
   generator takes and refuses, the forms of `#pragma pack` and a
   variable. */
#ifndef FORMS_H
#define FORMS_H

#include "forms_unit.h"

enum
{
    FORMS_FIRST,
    FORMS_SECOND = 5
};

typedef enum
{
    FORMS_MODE_A,
    FORMS_MODE_B
} FormsMode;

struct FormsOuter
{
    struct FormsInner
    {
        int a;
    } inner;
    struct
    {
        int b;
    } plain;
    union
    {
        int c;
        float d;
    } either;
};

typedef struct FormsPair
{
    float x;
    float y;
} FormsPair;

typedef struct FormsCount
{
    int n;
    unsigned char flag;
} FormsCount;

#define FORMS_EMPTY
#define FORMS_MODE FORMS_MODE_B
#define FORMS_SECOND_AGAIN FORMS_SECOND
#define FORMS_LOOP_A FORMS_LOOP_B
#define FORMS_LOOP_B FORMS_LOOP_A
#define FORMS_GONE 7
#undef FORMS_GONE
#define FORMS_LITERAL(type) (type)
#define FORMS_PAIR FORMS_LITERAL(FormsPair){ 1.5f, 2 }
#define FORMS_COUNT FORMS_LITERAL(FormsCount){ (2 + 3), 1 }
#define FORMS_TRAILING FORMS_LITERAL(FormsCount){ 4, 5, }
#define FORMS_SHORT FORMS_LITERAL(FormsCount){ 4 }
#define FORMS_LONG FORMS_LITERAL(FormsCount){ 4, 5, 6 }
#define FORMS_TEXT FORMS_LITERAL(FormsCount){ "four", 5 }
#define FORMS_FLOAT_IN_INT FORMS_LITERAL(FormsCount){ 1.5, 5 }
/* A macro whose body is its parameter bare expands as well, and the text
   of its argument may start with a space. A body that is more than its
   parameter does not expand, and neither does a call without its `)`. */
#define FORMS_BARE(type) type
#define FORMS_BARE_PAIR FORMS_BARE((FormsPair)){ 3.5f, 4 }
#define FORMS_SPACED_PAIR FORMS_BARE( (FormsPair)){ 5.5f, 6 }
#define FORMS_SCALED(type) ((type) * 2)
#define FORMS_SCALED_PAIR FORMS_SCALED(FormsPair){ 1, 2 }
#define FORMS_OPEN FORMS_LITERAL(FormsPair

/* A pack without a number sets the default again, so the struct after it
   is not packed. */
#pragma pack(push, 1)
struct FormsTight
{
    char c;
    int i;
};
#pragma pack()
struct FormsLoose
{
    char c;
    int i;
};
#pragma pack(pop)

/* A push without a number keeps the pack, so the struct after it is not
   packed. */
#pragma pack(push)
struct FormsKept
{
    char c;
    int i;
};
#pragma pack(pop)

/* A union that a typedef names without a tag. */
typedef union { int i; float f; } FormsEither;

extern int forms_counter;

forms_unit forms_take(FormsMode m, struct FormsOuter *o, FormsPair p);
int forms_either(FormsEither e);

#endif
