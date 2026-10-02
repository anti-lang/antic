/* Operating system signals for anti.os. The platform layer routes a
   signal to deliver below on an ordinary thread, which calls the
   function the program registered. See the DESIGN comment of
   anti_rt_signal_route in platform.h. */
#include <stdint.h>

#include "atomic.h"
#include "platform.h"
#include "signals.h"

/* The signals a program may wait for. The numbers are the C ones, which
   every platform the compiler targets spells the same way. */
#define ANTI_SIGNAL_MAX 32

/* DESIGN: the registered functions and the route are state of the
   process, because a signal is. Any thread may register, while the
   thread of the route reads the table. ANTI_RT_LOCK_SIGNALS guards both.
   deliver copies the function under it and calls the copy after, so a
   function that registers another does not wait for itself. pending is
   atomic instead, because a handler of raise writes it. */
static void (*handlers[ANTI_SIGNAL_MAX])(int64_t sig);
static int64_t pending;

/* Record sig and call the function registered for it. */
static int deliver(int64_t sig)
{
    void (*f)(int64_t) = NULL;

    anti_rt_atomic_store(&pending, (int64_t)sizeof pending, sig);
    if (sig <= 0 || sig >= ANTI_SIGNAL_MAX) {
        return 0;
    }
    anti_rt_lock_hold(ANTI_RT_LOCK_SIGNALS);
    f = handlers[sig];
    anti_rt_lock_release(ANTI_RT_LOCK_SIGNALS);
    if (f == NULL) {
        return 0;
    }
    f(sig);
    return 1;
}

void anti_rt_on_signal(int64_t sig, void (*f)(int64_t))
{
    int routed;

    if (sig <= 0 || sig >= ANTI_SIGNAL_MAX) {
        return;
    }
    anti_rt_lock_hold(ANTI_RT_LOCK_SIGNALS);
    handlers[sig] = f;
    routed = anti_rt_signal_route(deliver);
    anti_rt_lock_release(ANTI_RT_LOCK_SIGNALS);
    if (routed == 0) {
        anti_rt_signal_catch(sig);
    }
}

int64_t anti_rt_signal_pending(void)
{
    return anti_rt_atomic_swap(&pending, (int64_t)sizeof pending, 0);
}
