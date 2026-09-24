/* A C program that calls an export class whose layout holds types nested
   in it. It reads the node the class points to, the enum and the class
   it holds by value, and the struct inside that one. It names each as
   the header does. */
#include "../binary_stdio.h"
#include <stdio.h>

#include "nested.h"

int main(void)
{
    PeopleList list;
    const PeopleList_Node *n;

    anti_PeopleList_init(&list);
    printf("%d %d %d %d\n", (int)(list.state == PeopleList_State_Empty),
           (int)list.count.total, (int)list.count.step.by, (int)list.size);
    anti_PeopleList_add(&list, 20);
    anti_PeopleList_add(&list, 30);
    for (n = list.head; n != NULL; n = n->next) {
        printf("age %d\n", (int)n->age);
    }
    printf("%d %d %d\n", (int)(list.state == PeopleList_State_Filled),
           (int)list.count.total, (int)list.size);
    anti_PeopleList_clear(&list);
    printf("%d %d\n", (int)(list.head == NULL),
           (int)(list.state == PeopleList_State_Empty));
    anti_PeopleList_destroy(&list);
    return 0;
}
