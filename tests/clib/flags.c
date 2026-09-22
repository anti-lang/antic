/* A C program that calls a library whose signatures name Flags. C builds
   the struct of the header, passes it in and reads the one it gets back,
   alone and as a field. */
#include "../binary_stdio.h"
#include <stdint.h>
#include <stdio.h>

#include "flags.h"

static void show(struct anti_Flags f)
{
    printf(" o%d c%d z%d n%d\n", f.overflow, f.carry, f.zero, f.negative);
}

int main(void)
{
    struct anti_Flags clear = {false, false, false, false};
    struct anti_Flags carry = {false, true, false, false};
    FlagsStep s;

    printf("max + 1");
    show(flags_carry(UINT64_MAX, 1, clear));
    printf("max + 0 + carry");
    show(flags_carry(UINT64_MAX, 0, carry));
    s = flags_step(1, 2, carry);
    printf("1 + 2 + carry %llu", (unsigned long long)s.sum);
    show(s.flags);
    s = flags_step(UINT64_MAX / 2, 1, clear);
    printf("top + 1 %llu", (unsigned long long)s.sum);
    show(s.flags);
    return 0;
}
