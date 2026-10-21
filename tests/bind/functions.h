/* Forms of a function that tests/bind/layout.h does not hold, for
   `anti bind --clang`: no parameter, `...` alone and after a parameter,
   a function as a result and as a parameter, an array as a parameter, a
   volatile parameter, a name that is a word of Anti, parameters without
   names, a function declared twice under two names of its parameter, and
   the same forms as inline functions of the shim. Two structs hold a
   signed bitfield of one bit and an array of two levels. */
#ifndef FUNCTIONS_H
#define FUNCTIONS_H

struct FnBits
{
    unsigned int one : 1;
    int signed_one : 1;
    unsigned int three : 3;
};

struct FnHolder
{
    int (*call)(int);
    int grid[2][3];
};

void fn_none(void);
int fn_variadic(const char *format, ...);
int (*fn_gives_function(int which))(int);
void fn_takes_function(int (*call)(int, float), void (*done)(void));
void fn_takes_array(int values[4], int grid[2][3]);
void fn_volatile(volatile int *p);
int defer(int x);
int fn_unnamed(int, float);
int fn_twice(int first);
int fn_twice(int second);

static inline int fn_inline_none(void)
{
    return 7;
}

static inline void fn_inline_void(int x)
{
    (void)x;
}

static inline int fn_inline_variadic(int n, ...)
{
    return n;
}

static inline int fn_inline_array(int values[4])
{
    return values[0];
}

static inline int fn_inline_function(int (*call)(int))
{
    return call(1);
}

#endif
