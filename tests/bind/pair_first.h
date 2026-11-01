/* The first of two headers that `anti bind --clang` reads into one
   module, as raylib.h is read before raymath.h. It declares the struct
   both headers use and says so with a macro of no value. */
#ifndef PAIR_FIRST_H
#define PAIR_FIRST_H

typedef struct PairPoint
{
    int x;
    int y;
} PairPoint;
#define PAIR_POINT_TYPE

int pair_sum(PairPoint p);

#endif
