#include "atomic.h"

/* DESIGN: every operation is sequentially consistent, which is the one
   memory order the language offers. The width comes with the address,
   because one symbol per operation keeps the compiler free of a table of
   names per width. A release build holds the instructions the target
   gives: `lock xadd` and `lock cmpxchg` on x86_64, and `ldaddal` and
   `casal` on an ARM64 with LSE, or a load-store-exclusive loop without
   it.

   DESIGN: one form on every target, Windows included: the builtins of
   clang, which compiles the runtime on every host by decision 14 of
   "Decisions of 2026-10-11". gcc takes them as well, for a reader's
   build with the compiler of the system. */

/* One body per width, written once through a macro, because the builtins
   take a typed pointer and the widths differ in nothing else. */
#define WIDTHS(op)                                                            \
    switch (width) {                                                          \
    case 1: op(int8_t);                                                       \
    case 2: op(int16_t);                                                      \
    case 4: op(int32_t);                                                      \
    default: op(int64_t);                                                     \
    }

int64_t anti_rt_atomic_load(const void *address, int64_t width)
{
#define LOAD(T) return __atomic_load_n((const T *)address, __ATOMIC_SEQ_CST)
    WIDTHS(LOAD);
#undef LOAD
}

void anti_rt_atomic_store(void *address, int64_t width, int64_t value)
{
#define STORE(T)                                                              \
    __atomic_store_n((T *)address, (T)value, __ATOMIC_SEQ_CST);               \
    return
    WIDTHS(STORE);
#undef STORE
}

int64_t anti_rt_atomic_swap(void *address, int64_t width, int64_t value)
{
#define SWAP(T)                                                               \
    return __atomic_exchange_n((T *)address, (T)value, __ATOMIC_SEQ_CST)
    WIDTHS(SWAP);
#undef SWAP
}

int64_t anti_rt_atomic_add(void *address, int64_t width, int64_t value)
{
#define ADD(T)                                                                \
    return __atomic_fetch_add((T *)address, (T)value, __ATOMIC_SEQ_CST)
    WIDTHS(ADD);
#undef ADD
}

int64_t anti_rt_atomic_sub(void *address, int64_t width, int64_t value)
{
#define SUB(T)                                                                \
    return __atomic_fetch_sub((T *)address, (T)value, __ATOMIC_SEQ_CST)
    WIDTHS(SUB);
#undef SUB
}

int64_t anti_rt_atomic_and(void *address, int64_t width, int64_t value)
{
#define AND(T)                                                                \
    return __atomic_fetch_and((T *)address, (T)value, __ATOMIC_SEQ_CST)
    WIDTHS(AND);
#undef AND
}

int64_t anti_rt_atomic_or(void *address, int64_t width, int64_t value)
{
#define OR(T)                                                                 \
    return __atomic_fetch_or((T *)address, (T)value, __ATOMIC_SEQ_CST)
    WIDTHS(OR);
#undef OR
}

int8_t anti_rt_atomic_compare_swap(void *address, int64_t width,
                                   int64_t expected, int64_t desired)
{
#define CAS(T) {                                                              \
        T want = (T)expected;                                                 \
        return __atomic_compare_exchange_n((T *)address, &want, (T)desired,    \
                                           0, __ATOMIC_SEQ_CST,               \
                                           __ATOMIC_SEQ_CST);                 \
    }
    WIDTHS(CAS);
#undef CAS
}
