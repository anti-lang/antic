/* A C program that passes enum values below zero to a library and reads
   one back as a result. An enum over a signed integer extends as that
   integer does, as the C convention of the target asks. */
#include "../binary_stdio.h"
#include <stdio.h>

#include "levels.h"

int main(void)
{
    printf("is_low %d\n", (int)is_low(Level_Low));
    printf("is_minus %d\n", (int)is_minus(Sign_Minus));
    printf("lower %d\n", (int)lower(Level_Mid));
    printf("low %d\n", lower(Level_Mid) == Level_Low);
    return 0;
}
