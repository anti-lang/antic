/* A C program that reads the `own fn` field of a class from the header of
   com.example.handlers. It calls the code with the snapshot, and the
   snapshot goes with the object. */
#include "../binary_stdio.h"
#include <stdio.h>

#include "handlers.h"

int main(void)
{
    Button *b = make_button();
    b->handler.code(1, b->handler.snapshot);
    printf("quiet has %s snapshot\n", b->handler.snapshot == NULL ? "no" : "a");
    anti_Button_listen(b, 40);
    b->handler.code(2, b->handler.snapshot);
    printf("%lld alive\n", (long long)snapshots());
    anti_Button_delete(b);
    printf("%lld alive\n", (long long)snapshots());
    return 0;
}
