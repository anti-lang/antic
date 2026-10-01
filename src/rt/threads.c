/* The worker pool behind `parallel`. antic lowers the construct to one
   call of anti_rt_parallel with a thunk that runs a worker over one
   chunk. The pool is built at the first call and lives for the program.

   DESIGN: the caller blocks until every chunk is done, so the array it
   splits cannot change while the workers read it. The runtime does the
   splitting, which is what makes the construct free of data races. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "hooks.h"
#include "object.h"
#include "platform.h"
#include "rt.h"
#include "std.h"
#include "threads.h"

/* DESIGN: one dispatched object. The pool runs it and `join` waits for
   it. The in-flight map holds the address of the object from the moment
   of the dispatch until the worker returns. A second dispatch of the
   same object gives no job. */
struct one {
    anti_rt_dispatch_body *run;
    void *context;
    void *object;
    unsigned char *result;
    struct one *next;           /* the queue of jobs not yet taken */
    struct one *live;           /* the list of jobs in flight */
    int done;
};

/* One call of `parallel`, shared by the caller and the pool. */
struct jobs {
    anti_rt_parallel_body *run;
    void *context;
    const unsigned char *base;
    int64_t element_size;
    int64_t result_size;
    int64_t count;
    int64_t chunks;
    unsigned char *results;
};

/* The pool stands on the monitor ANTI_RT_LOCK_POOL. The workers wait on
   WORK_READY, and the callers of `parallel` and `join` on WORK_DONE. */
enum { WORK_READY, WORK_DONE };

static void hold(void) { anti_rt_lock_hold(ANTI_RT_LOCK_POOL); }
static void release(void) { anti_rt_lock_release(ANTI_RT_LOCK_POOL); }
static void wait_ready(void)
{
    anti_rt_monitor_wait(anti_rt_monitor_of(ANTI_RT_LOCK_POOL), WORK_READY);
}
static void wait_done(void)
{
    anti_rt_monitor_wait(anti_rt_monitor_of(ANTI_RT_LOCK_POOL), WORK_DONE);
}
static void wake_all(void)
{
    anti_rt_monitor_wake_all(anti_rt_monitor_of(ANTI_RT_LOCK_POOL),
                             WORK_READY);
}
static void wake_caller(void)
{
    anti_rt_monitor_wake_all(anti_rt_monitor_of(ANTI_RT_LOCK_POOL),
                             WORK_DONE);
}

/* DESIGN: two kinds of thread wait on WORK_DONE, the caller of
   `parallel` and each caller of `join`. The end of a chunk or of a job
   wakes all of them, and each checks its own condition. One wake-up
   could go to a waiter whose work is not done, and the other would
   sleep for good. */

/* The state the lock guards. started is 1 once the workers run. */
static int started;
static struct jobs *active;
static int64_t taken;
static int64_t finished;

/* DESIGN: the count comes from the machine that runs the program, not
   from the machine that compiled it. The `threads` key of the runtime
   configuration overrides it, so a measurement can pin the count
   without a rebuild. `--anti.threads` and the configuration file set
   that key, and the runtime reads no environment variable of its own
   but ANTI_CONF. */
static int worker_count(void)
{
    int n = (int)anti_rt_conf_threads(
        anti_rt_conf_get((const unsigned char *)"threads", 7));

    if (n > 0) {
        return n;
    }
    return (int)anti_rt_processors();
}

/* The first element and the length of chunk index. The first
   count % chunks chunks hold one element more than the others, so every
   element belongs to exactly one chunk. */
static void slice_of(const struct jobs *j, int64_t index, int64_t *first,
                     int64_t *length)
{
    int64_t base = j->count / j->chunks;
    int64_t extra = j->count % j->chunks;

    *length = base + (index < extra ? 1 : 0);
    *first = index * base + (index < extra ? index : extra);
}

static void run_chunk(const struct jobs *j, int64_t index)
{
    int64_t first;
    int64_t length;

    slice_of(j, index, &first, &length);
    j->run(j->context, j->base + first * j->element_size, length,
           j->results + index * j->result_size);
}

/* The queue of dispatched jobs and the list of the ones in flight. Both
   are read and written under the lock of the pool. */
static struct one *queued;
static struct one *queued_last;
static struct one *in_flight;

/* Take the job off the list of the ones in flight and mark it done. */
static void finish_one(struct one *job)
{
    struct one **at = &in_flight;

    while (*at != NULL && *at != job) {
        at = &(*at)->live;
    }
    if (*at == job) {
        *at = job->live;
    }
    job->done = 1;
}

/* DESIGN: a worker of the pool runs until exit. The leak check of a
   program of --memory-checks finds no pointer to the block of its
   thread-local variables where the system keeps them on the heap, so
   the worker marks the block as kept as it starts. */
static void worker_main(void)
{
    const void *block = anti_rt_thread_block();

    if (block != NULL) {
        anti_rt_memory_kept(block);
    }
    for (;;) {
        struct jobs *j;
        struct one *single;
        int64_t index;

        hold();
        while (queued == NULL &&
               (active == NULL || taken == active->chunks)) {
            wait_ready();
        }
        /* A dispatched job comes first, because its caller may already
           be waiting for it while a `parallel` still has chunks left. */
        if (queued != NULL) {
            single = queued;
            queued = single->next;
            if (queued == NULL) {
                queued_last = NULL;
            }
            release();
            single->run(single->context, single->object, single->result);
            hold();
            finish_one(single);
            wake_caller();
            release();
            continue;
        }
        j = active;
        index = taken++;
        release();

        run_chunk(j, index);

        hold();
        finished++;
        wake_caller();
        release();
    }
}

/* Start the workers on the first call. The caller holds the lock, so a
   worker waits for it before it looks for work. */
static void start_locked(void)
{
    int n;
    int i;

    if (started) {
        return;
    }
    started = 1;
    n = worker_count() - 1;
    for (i = 0; i < n; i++) {
        if (anti_rt_thread_start(worker_main) != 0) {
            break;
        }
    }
}

/* Split base into chunks, run one worker over each through the pool, and
   write the pointer and the count of the results. Blocks until every
   chunk is done. The results are malloc memory, and the program that
   wrote `parallel` frees them. */
void anti_rt_parallel(const void *base, int64_t count, int64_t element_size,
                      int64_t chunks, int64_t result_size,
                      anti_rt_parallel_body *run, void *context,
                      void **results_out, int64_t *chunks_out)
{
    struct jobs j;
    int64_t index;

    if (chunks <= 0) {
        chunks = worker_count();
    }
    if (chunks > count) {
        chunks = count;
    }
    j.run = run;
    j.context = context;
    j.base = (const unsigned char *)base;
    j.element_size = element_size;
    j.result_size = result_size;
    j.count = count;
    j.chunks = chunks;
    j.results = NULL;
    if (chunks > 0) {
        /* The bound is the one of anti_rt_chan_new. A product past it
           would wrap, and the workers would write past the block. */
        if (result_size < 0 ||
            (result_size > 0 && chunks > (INT64_MAX / 2) / result_size)) {
            anti_rt_fail_abort("anti: the results of %lld chunks of %lld "
                               "bytes each do not fit in memory",
                               (long long)chunks, (long long)result_size);
        }
        j.results = malloc((size_t)(chunks * result_size));
        if (j.results == NULL) {
            /* DESIGN: the language has no way to report this, and a
               program that cannot hold its results cannot go on. */
            anti_rt_fail_abort("anti: no memory for the results of %lld "
                               "chunks", (long long)chunks);
        }
    }
    *results_out = j.results;
    *chunks_out = chunks;
    if (chunks == 0) {
        return;
    }

    hold();
    start_locked();
    /* DESIGN: one call of `parallel` uses the pool at a time. A second
       one comes from a worker or from another thread. It runs its chunks
       in the thread that asked for them, rather than waiting for a pool
       that is already busy. */
    if (active != NULL) {
        release();
        for (index = 0; index < chunks; index++) {
            run_chunk(&j, index);
        }
        return;
    }
    active = &j;
    taken = 0;
    finished = 0;
    wake_all();
    /* The caller takes chunks as well, so a pool of one thread still
       runs every chunk. */
    while (taken < chunks) {
        index = taken++;
        release();
        run_chunk(&j, index);
        hold();
        finished++;
    }
    while (finished < chunks) {
        wait_done();
    }
    active = NULL;
    release();
}

/* DESIGN: `dispatch` gives one object to the pool and gives a job back.
   The object is in flight from here until the worker returns. A second
   dispatch of the same object gives no job, so two workers never hold
   one object. The result waits in the job until `join` takes it. The
   job is on the heap, and anti_rt_join frees it, which `join` calls. */
void *anti_rt_dispatch(void *object, int64_t result_size,
                       anti_rt_dispatch_body *run, void *context)
{
    struct one *job = calloc(1, sizeof *job);
    struct one *at;

    if (job == NULL) {
        anti_rt_fail_abort("anti: no memory for a job of dispatch");
    }
    job->run = run;
    job->context = context;
    job->object = object;
    if (result_size > 0) {
        job->result = malloc((size_t)result_size);
        if (job->result == NULL) {
            anti_rt_fail_abort("anti: no memory for the result of a job");
        }
    }
    hold();
    start_locked();
    for (at = in_flight; at != NULL; at = at->live) {
        if (at->object == object) {
            release();
            free(job->result);
            free(job);
            return NULL;
        }
    }
    job->live = in_flight;
    in_flight = job;
    if (queued_last == NULL) {
        queued = job;
    } else {
        queued_last->next = job;
    }
    queued_last = job;
    wake_all();
    release();
    return job;
}

/* Wait for the job, write its result into out and end the job. A null
   job carries no work, and `join` of one writes nothing. */
void anti_rt_join(void *handle, int64_t result_size, void *out)
{
    struct one *job = handle;

    /* A null job is the answer to a dispatch of an object already in
       flight. It carries no work, and its result is zero. */
    if (job == NULL) {
        if (result_size > 0 && out != NULL) {
            memset(out, 0, (size_t)result_size);
        }
        return;
    }
    hold();
    /* DESIGN: the caller takes the job itself when no worker has, so a
       pool of one thread still finishes it. */
    while (!job->done) {
        struct one **at = &queued;
        while (*at != NULL && *at != job) {
            at = &(*at)->next;
        }
        if (*at == job) {
            *at = job->next;
            if (queued_last == job) {
                queued_last = NULL;
                for (at = &queued; *at != NULL; at = &(*at)->next) {
                    queued_last = *at;
                }
            }
            release();
            job->run(job->context, job->object, job->result);
            hold();
            finish_one(job);
            wake_caller();
            break;
        }
        wait_done();
    }
    release();
    if (result_size > 0 && out != NULL) {
        memcpy(out, job->result, (size_t)result_size);
    }
    free(job->result);
    free(job);
}

/* Wait for every job of the array, in the order it holds them. */
void anti_rt_join_all(void *const *handles, int64_t count)
{
    int64_t i;

    for (i = 0; i < count; i++) {
        anti_rt_join(handles[i], 0, NULL);
    }
}

/* DESIGN: `join` fires the `joined` hook of the object the pool ran,
   which the site of the call cannot name: a Job holds the handle alone.
   The compiler calls these two where hooks are compiled and the plain
   pair under `--no-hooks`, so the option removes the site as it removes
   every other. */
void anti_rt_join_hooked(void *handle, int64_t result_size, void *out)
{
    struct one *job = handle;
    void *object = job != NULL ? job->object : NULL;

    anti_rt_join(handle, result_size, out);
    if (object != NULL) {
        anti_rt_hook(object, ANTI_HOOK_JOINED);
    }
}

void anti_rt_join_all_hooked(void *const *handles, int64_t count)
{
    int64_t i;

    for (i = 0; i < count; i++) {
        anti_rt_join_hooked(handles[i], 0, NULL);
    }
}
