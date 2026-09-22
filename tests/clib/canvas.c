/* A C program that builds Anti classes, calls through the table and ends
   them. The header of com.example.canvas gives the layout, the table type
   and one prototype per public function. */
#include "../binary_stdio.h"
#include <stdio.h>
#include <stdlib.h>

#include "canvas.h"

int main(void)
{
    Square *s = (Square *)malloc(sizeof *s);
    Stamp *stamp = (Stamp *)malloc(sizeof *stamp);
    Shape *base;
    Circle c;
    Tile t;
    struct anti_Error *e;

    anti_Square_init(s);
    s->side = 4;
    Shape_move(&s->base, 3, 5);
    base = &s->base;

    printf("%d %d %d\n", base->x, base->y, anti_Shape_area(base));
    printf("%d %d\n", Square_area(s), Shape_area(base));
    /* An interface sub-object is a field, so C takes its address and
       calls through the table that belongs to it. */
    printf("%d\n", anti_Ink_colour(anti_Square_as_Ink(s)));
    anti_Square_delete(s);
    /* A class whose `construct` takes arguments is made in one call,
       which reports the error of a `construct` that fails. */
    e = anti_Circle_construct(&c, 2);
    printf("%d %d\n", e == NULL, anti_Shape_area(&c.base));
    e = anti_Circle_construct(&c, 0);
    printf("%d\n", e == NULL);
    /* Each qualified body fills the table of its own interface, and the
       table of the class holds neither. */
    anti_Stamp_init(stamp);
    printf("%d %d\n", anti_Ink_colour(anti_Stamp_as_Ink(stamp)),
           anti_Tint_colour(anti_Stamp_as_Tint(stamp), 3));
    anti_Stamp_delete(stamp);
    /* A body qualified by the base fills the table of the class and has
       a symbol of its own. */
    anti_Tile_init(&t);
    printf("%d %d\n", anti_Shape_area(&t.base), Tile_Shape_area(&t));
    return 0;
}
