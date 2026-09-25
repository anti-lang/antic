/* A C program that uses the copy of a generic an `export type` names.
   The header of com.example.stacks writes it as the export class Ints,
   with its layout, its table and one prototype per public function. */
#include "../binary_stdio.h"
#include <stdio.h>
#include <stdlib.h>

#include "stacks.h"

int main(void)
{
    Ints *s = (Ints *)malloc(sizeof *s);
    Ints local;

    anti_Ints_init(s);
    Ints_push(s, 3);
    Ints_push(s, 4);
    anti_Ints_push(s, 5);
    printf("%d %d %d\n", anti_Ints_size(s), s->count, total(s));
    printf("%d %d\n", Ints_pop(s), anti_Ints_pop(s));
    printf("%d\n", Ints_size(s));
    anti_Ints_delete(s);
    /* A value on the stack of C takes the same init. */
    anti_Ints_init(&local);
    Ints_push(&local, 9);
    printf("%d %d\n", local.items[0], total(&local));
    return 0;
}
