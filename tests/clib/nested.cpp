/* The header of an export class with nested types compiles as C++17. */
#include "../binary_stdio.h"
#include "nested.h"

int main(void)
{
    PeopleList_Node n;
    n.age = 1;
    n.next = nullptr;
    return n.age == 1 && PeopleList_State_Filled == 1 ? 0 : 1;
}
