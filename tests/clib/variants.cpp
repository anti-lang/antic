/* The header of a library with variants compiles as C++17. */
#include "../binary_stdio.h"
#include "variants.h"

int main(void)
{
    Shape s;
    s.tag = Shape_Circle;
    s.u.Circle.r = 1.0f;
    return s.tag == Shape_Circle ? 0 : 1;
}
