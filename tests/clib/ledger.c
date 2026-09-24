/* A C program that calls a synchronized class of a library from two
   threads. Each call runs under the lock of the object, so no addition is
   lost. */
#include "../binary_stdio.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#include "ledger.h"

static void *add_many(void *ledger)
{
    int i;

    for (i = 0; i < 100000; i++) {
        anti_Ledger_add(ledger, 1);
    }
    return NULL;
}

int main(void)
{
    Ledger ledger;
    pthread_t a;
    pthread_t b;

    anti_Ledger_init(&ledger);
    pthread_create(&a, NULL, add_many, &ledger);
    pthread_create(&b, NULL, add_many, &ledger);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    printf("%lld\n", (long long)anti_Ledger_total(&ledger));
    return 0;
}
