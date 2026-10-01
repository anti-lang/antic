/* The calls of `parallel`, `dispatch` and `join` that generated code
   makes, in src/rt/threads.c. RT_FUNCTIONS of src/antic/rt_abi.h lists
   the same functions, and the unit test runtime_functions compares the
   two. */
#ifndef ANTI_RT_THREADS_H
#define ANTI_RT_THREADS_H

#include <stdint.h>

/* The worker of one chunk of `parallel`: the elements at ptr, len of
   them, with the result written to out. */
typedef void anti_rt_parallel_body(void *context, const void *ptr,
                                   int64_t len, void *out);

/* The worker of `dispatch`, run on the object with its result written
   to out. */
typedef void anti_rt_dispatch_body(void *context, void *object, void *out);

/* Split base into chunks, run one worker over each through the pool, and
   write the pointer and the count of the results. Blocks until every
   chunk is done. The results are malloc memory, and the program that
   wrote `parallel` frees them. */
void anti_rt_parallel(const void *base, int64_t count, int64_t element_size,
                      int64_t chunks, int64_t result_size,
                      anti_rt_parallel_body *run, void *context,
                      void **results_out, int64_t *chunks_out);

/* Give the object to the pool and give back the job that runs it, or
   NULL when the object is in flight already. anti_rt_join frees the
   job. */
void *anti_rt_dispatch(void *object, int64_t result_size,
                       anti_rt_dispatch_body *run, void *context);

/* Wait for the job, write its result into out and end the job. A null
   job carries no work, and `join` of one writes nothing. */
void anti_rt_join(void *handle, int64_t result_size, void *out);

/* Wait for every job of the array, in the order it holds them. */
void anti_rt_join_all(void *const *handles, int64_t count);

/* anti_rt_join and anti_rt_join_all with the `joined` hook of the object
   the pool ran, which the compiler calls where hooks are compiled. */
void anti_rt_join_hooked(void *handle, int64_t result_size, void *out);
void anti_rt_join_all_hooked(void *const *handles, int64_t count);

#endif
