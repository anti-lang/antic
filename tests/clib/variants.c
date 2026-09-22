/* A C program that calls a library whose signatures name a variant. C
   sets the tag and the fields of one case, passes the variant by value,
   and reads the tag and the case of one it is given, alone and as a
   field. */
#include "../binary_stdio.h"
#include <stdio.h>

#include "variants.h"

_Static_assert(sizeof(Shape) == 12, "a tag and a union of two floats");
_Static_assert(sizeof(Mark) == 5, "packed");
_Static_assert(sizeof(Wide) == 16, "align(16)");

static void show(Shape s)
{
    switch (s.tag) {
    case Shape_Circle:
        printf("circle %.2f\n", (double)s.u.Circle.r);
        break;
    case Shape_Rect:
        printf("rect %.2f %.2f\n", (double)s.u.Rect.w, (double)s.u.Rect.h);
        break;
    case Shape_Empty:
        printf("empty\n");
        break;
    default:
        printf("tag %d\n", (int)s.tag);
        break;
    }
}

int main(void)
{
    Shape r;
    Shape e;
    Tile t;

    r.tag = Shape_Rect;
    r.u.Rect.w = 2.0f;
    r.u.Rect.h = 3.0f;
    e.tag = Shape_Empty;
    printf("area %.2f %.2f\n", (double)shape_area(r), (double)shape_area(e));
    show(shape_circle(1.5f));
    printf("area %.2f\n", (double)shape_area(shape_circle(2.0f)));
    t = shape_tile(r, 2.0f);
    show(t.shape);
    printf("count %d\n", (int)t.count);
    show(shape_tile(e, 3.0f).shape);
    return 0;
}
