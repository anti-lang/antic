/* The second of the two headers. It declares the struct of the first
   only where the first did not, as raymath.h declares Vector2 only where
   raylib.h did not, so the module holds the struct once. Its inline
   function goes through the shim, and its constant is a constant of the
   one module. */
#ifndef PAIR_SECOND_H
#define PAIR_SECOND_H

#if !defined(PAIR_POINT_TYPE)
typedef struct PairPoint
{
    int x;
    int y;
} PairPoint;
#endif

#define PAIR_LIMIT 7

PairPoint pair_scale(PairPoint p, int by);

inline int pair_twice(int v)
{
    return v * 2;
}

#endif
