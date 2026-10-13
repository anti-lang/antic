/* An enum whose one value needs 64 bits without a sign, and a struct that
   holds it, for `anti bind --clang`. C23 allows such an enumerator and
   C11 leaves it to the compiler, which warns under -Wpedantic, as for
   LAYOUT_LARGE of tests/bind/layout.h. The warning is silenced at that
   line alone. */
#ifndef HUGE_H
#define HUGE_H

enum Huge
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#pragma clang diagnostic ignored "-Wmicrosoft-enum-value"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
    HUGE_TOP = 0xFFFFFFFFFFFFFFFFull
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
};

struct HugeHolder
{
    enum Huge top;
    int n;
};

void huge_take(struct HugeHolder *h);

#endif
