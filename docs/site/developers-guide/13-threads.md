---
title: "Threads"
description: "Threads, dispatch and join, parallel, channels and select, atomics, locks, and what the checker refuses."
summary: "`worker fn`, `dispatch` and `join`, `parallel`, channels and `select`, atomics and locks, and what the checker refuses of each, with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 130
tags: [compilers, programming-languages]
keywords: [threads, workers, thread pool, parallel, channels, select, atomics, mutex]
---

## Threads, dispatch and join

### What you know

`Thread` and the executors of Java, `std::thread` of C++, and the pthreads of C.

### The feature

`worker fn`, `dispatch`, `join` and `join_all`, `Job`, the pool and its `threads` key, what a worker may touch, and the refusals.

### How antic builds it

A worker as a function the pool calls, and `dispatch` and `join` as runtime functions, with excerpts.

### What it costs

The runtime calls of a dispatch and of a join, the allocations of a job, and the threads of the pool.

### Pitfalls

Where a worker takes a pointer to a thread-safe object alone, other than a Java, C or C++ programmer expects.

## parallel

### What you know

The parallel streams of Java, OpenMP of C and C++, and a loop split by hand.

### The feature

`parallel` over a range and over a collection, what its body may touch, `concurrent` parameters, and the refusals.

### How antic builds it

The body as a worker and the split of the range in the runtime, with excerpts.

### What it costs

The runtime calls per `parallel`, the jobs it makes, and the time of a loop from `tests/bench`.

### Pitfalls

Where a change of a captured variable is refused, other than a Java, C or C++ programmer expects.

## Channels and select

### What you know

`BlockingQueue` of Java, the condition variables of C and C++, and waiting on more than one queue in each.

### The feature

`chan int(16)`, `send`, `recv` that gives `?*T`, closing a channel, `select`, and the refusals.

### How antic builds it

The channel of `src/rt/sync.c` and the futex or the lock it waits on, with excerpts.

### What it costs

The bytes of a channel, its allocations, the locks per operation, and the blocking of `send` and `recv`.

### Pitfalls

Where `recv` gives `none`, and `select` picks, other than a Java, C or C++ programmer expects.

## Atomics

### What you know

`AtomicInteger` of Java, `std::atomic` of C++, and `_Atomic` of C11.

### The feature

`atomic int` as a local, a field and a static field, `load store add sub and or swap compare_swap`, all sequentially consistent, and the refusals.

### How antic builds it

An atomic operation as an atomic instruction of LLVM, one instruction at `armv8.2` and above, with excerpts per level.

### What it costs

The atomic instruction per operation, and the loop below `armv8.2`.

### Pitfalls

Where every operation is sequentially consistent, and a plain read of an atomic is refused, other than a Java, C or C++ programmer expects.

## Locks

### What you know

`synchronized` and `ReentrantLock` of Java, `std::mutex` of C++, and `pthread_mutex_t` of C.

### The feature

`Mutex.new()`, `sync m { }` that unlocks on every exit, the refusal of a nested `sync` on the same place, `sync a, b { }`, and the other refusals.

### How antic builds it

A Mutex as one word, a futex word, an `os_unfair_lock` or an `SRWLOCK`, and the runtime calls of `src/rt/lock.c`, with excerpts.

### What it costs

The bytes of a Mutex by `size_of`, the runtime call per lock and unlock, and the atomic operation of an uncontended lock.

### Pitfalls

Where a Mutex cannot be copied, and `sync` unlocks on every exit, other than a Java, C or C++ programmer expects.
