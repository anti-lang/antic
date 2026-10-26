---
title: "Threads"
description: "Threads, dispatch and join, parallel, channels and select, atomics, locks, waits under a lock, and what the checker refuses."
summary: "`worker fn`, `dispatch` and `join`, `parallel`, channels and `select`, atomics, locks and the warning `blocking-under-lock`, and what the checker refuses of each, with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-26T12:00:00+01:00
draft: true
weight: 130
tags: [compilers, programming-languages]
keywords: [threads, workers, thread pool, parallel, channels, select, atomics, mutex, blocking under a lock]
---

A program starts no thread by name. It marks a function `worker fn` and hands work to one pool of threads. `dispatch` hands over one object, and `parallel` the chunks of a slice. Each is one call of the runtime. A channel is a bounded queue behind a lock, and an atomic operation is one instruction of the processor. A `Mutex` is one word of the program's memory. The checker warns of a thread that may wait while it holds a lock.

## Threads, dispatch and join

### What you know

Java starts a `Thread` or submits a task to an `ExecutorService` and gets a `Future`. C++ has `std::thread` and `std::async` with `std::future`, and C has `pthread_create` with a `void *` argument. All three let a thread reach any memory it has an address of, and no compiler of the three checks what two threads share.

### The feature

A marker and three forms run a function on another thread.

| Form | What it does |
|---|---|
| `worker fn f(o: *T, args) -> R` | declares a function the pool may run |
| `dispatch obj -> f(args)` | gives the object `obj` to the pool for `f` and yields a job |
| `dispatch obj -> f` | the same for a worker that takes the object alone |
| `join(job)` | waits for the job and yields the result of the worker |
| `join_all(jobs)` | waits for every job of a slice and yields nothing |

`worker` stands directly before `fn`, after `pub` or `export`. A worker is an ordinary function as well, and a plain call `f(obj, args)` runs it on the calling thread.

The arrow names the worker as a call names a function: `f` of the module, a name of a direct import, or `m.f` of a module the file imports. A `dispatch` fills in the default value of every parameter the call leaves out, as a call does, and `parallel` does the same.

The object of a `dispatch` is a pointer to a class, and its type is the type of the first parameter of the worker. A pointer to a derived class is refused where the worker names the base.

Every other argument and the result hold no pointer. The checker reads that off the types:

- A number, a `bool`, a `char`, an enum and a `str` hold none. A `str` never changes, so two threads may read one.
- A struct, a tuple, a variant, an array and a `?T` hold none when their parts hold none.
- A class value holds none when its fields hold none. The table pointer and the `own` fields do not count.
- A pointer, a slice and a function value hold one, and are refused.
- A `chan T` passes by value, since a copy names the same channel.
- A pointer to a thread-safe object passes: a `*Mutex` and a pointer to a `synchronized class` or a `concurrent class` of [chapter 14]({{% relref "14-concurrent-classes" %}}).
- A parameter of function type takes a named function. It takes a closure only when it is marked `concurrent`, which [chapter 5]({{% relref "05-functions" %}}) describes.

A job has no type a program can write. A `let` infers it from a `dispatch`, an array literal and a tuple literal of jobs hold it as an element, and a closure that captures a job may join it. `job.handle` reads its one field, a `?*byte`. A job therefore never stands in a field, a parameter or a result.

The job holds its arguments as a call holds its own. A local passed to an `own` parameter of the worker moves into the job, and the function that wrote the `dispatch` tears it down no more. A fresh value passed to a parameter that is not `own` is torn down after the worker returns, on the thread that ran it.

The safety check `escaping-local` reads the object and every argument of a `dispatch`. The address of a local and a slice of a local array are references, and the job may keep one after the block ends. The check refuses it unless a later statement of the same block joins the job on every path. [Chapter 11]({{% relref "11-ownership#references-to-locals-and-uses-after-the-end" %}}) gives the rule.

A `dispatch` is an expression whose value must be bound. A statement that is a `dispatch` alone is refused.

The pool holds each object from its `dispatch` until its worker returns. That time is the flight of the object. A `dispatch` of an object in flight starts nothing: its job has the handle `none`, and `join` of that job gives a result of zero bytes. Two workers therefore never hold one object.

The checker walks every function a worker reaches. It refuses a `delete` of an object or a channel there, since another worker may hold it. It refuses a read or a write of a `mutable` field of a singleton as well. The walk follows the calls by name through the functions of the module and does not enter a closure the worker receives, so a closure that a `dispatch` passes on may `delete`.

A worker that `parallel` or `dispatch` runs cannot fail, since a job has no handler. A `worker fn` that may fail is called directly, with a handler.

The pool has one thread fewer than the machine has processors, since the thread that joins runs work as well. The key `threads` of the runtime configuration sets the count, as `--anti.threads=4` on the command line of the program. [Chapter 15]({{% relref "15-hooks" %}}) describes the configuration.

```anti
//! name: workers
import anti.io;
import anti.text;

export class Image
{
	pub width: int = 0,
	pub rows: int = 0,
	pub sum: int = 0,
}

worker fn fill(img: *Image, seed: int) -> int
{
	let total = 0;
	for y in 0..img.rows {
		total += y * img.width + seed;
	}
	img.sum = total;
	return img.rows;
}

worker fn clear(img: *Image)
{
	img.sum = 0;
}

worker fn twice(img: *Image, seed: int) -> int
{
	let inner = dispatch img -> fill(seed);
	return join(inner);
}

export fn start(img: *Image, seed: int) -> int
{
	let job = dispatch img -> fill(seed);
	return join(job);
}

fn main() -> int
{
	let a = alloc Image { width: 4, rows: 3 };
	let b = alloc Image { width: 8, rows: 5 };
	let ja = dispatch a -> fill(1);
	let jb = dispatch b -> fill(2);
	let rows_a = join(ja);
	let rows_b = join(jb);
	let line = f"a: {rows_a} rows, sum {a.sum}. b: {rows_b} rows, sum {b.sum}";
	io.println(line);
	free(line.ptr);

	let nested = dispatch a -> twice(7);
	let got = join(nested);
	line = f"a dispatch of an object in flight gives {got}, and the sum stays {a.sum}";
	io.println(line);
	free(line.ptr);

	let jobs = [dispatch a -> clear, dispatch b -> clear];
	join_all(jobs[0..2]);
	line = f"after clear: {a.sum} {b.sum}, start gives {start(a, 3)}, a plain call of fill gives {fill(b, 0)}";
	io.println(line);
	free(line.ptr);
	free(a);
	free(b);
	return 0;
}
```

```text output
a: 3 rows, sum 15. b: 5 rows, sum 90
a dispatch of an object in flight gives 0, and the sum stays 15
after clear: 0 0, start gives 3, a plain call of fill gives 5
```

`twice` runs on `a` and dispatches `a` again. The object is in flight, so the inner job is empty, `fill` does not run, and the sum of `a` stays 15.

The second program shows the defaults, the arguments a job holds and the places a job may stand.

```anti
//! name: jobs
import anti.io;
import anti.text;

class Cell { pub n: int = 0, }

class Key
{
	pub id: int = 0,

	fn destruct(self)
	{
		let line = f"key {self.id} torn down";
		io.println(line);
		free(line.ptr);
	}
}

worker fn add(c: *Cell, k: int, times: int = 3) -> int
{
	for 0..times {
		c.n += k;
	}
	return c.n;
}

worker fn keep(c: *Cell, own k: Key) -> int
{
	return k.id;
}

worker fn look(c: *Cell, k: Key) -> int
{
	return k.id + c.n;
}

worker fn apply(c: *Cell, concurrent f: fn(int) -> int) -> int
{
	return f(c.n);
}

fn main() -> int
{
	let a = alloc Cell { };
	let b = alloc Cell { n: 100 };
	let pair = (dispatch a -> add(2), dispatch b -> add(1, 1));
	let first = join(pair.0);
	let second = join(pair.1);
	let line = f"defaults: {first} {second}";
	io.println(line);
	free(line.ptr);

	let k = Key { id: 7 };
	let moved = dispatch a -> keep(k);
	let kept = join(moved);
	let fresh = dispatch a -> look(Key { id: 9 });
	let looked = join(fresh);
	line = f"keys: {kept} {looked}";
	io.println(line);
	free(line.ptr);

	let factor = 3;
	let job = dispatch b -> apply(fn(v) { return v * factor; });
	let wait = fn() -> int { return join(job); };
	line = f"a closure joins the job it captured: {wait()}";
	io.println(line);
	free(line.ptr);
	free(a);
	free(b);
	return 0;
}
```

```text output
defaults: 6 101
key 7 torn down
key 9 torn down
keys: 7 15
a closure joins the job it captured: 303
```

- `add(2)` leaves out `times`, so the job runs three turns, and `add(1, 1)` runs one.
- `k` moves into the job of `keep`. The worker tears it down at its end, on a thread of the pool, and `main` tears it down no more.
- The `Key` literal passed to `look` is fresh. The job tears it down after `look` returns, before `join` gives the result.
- The two jobs of the tuple are joined through its elements, and the anonymous function `wait` joins the job it captured.

The checker refuses each of the following.

```anti refused
//! error: `dispatch` submits a class pointer, and this is `*Point`
struct Point { x: int }

worker fn read(p: *Point) -> int { return p.x; }

fn run(p: *Point) -> int
{
	let job = dispatch p -> read;
	return join(job);
}
```

```anti refused
//! error: an argument of a worker has type `*int`, which holds a pointer. A worker takes and returns values alone
class Cell { pub n: int = 0, }

worker fn copy(c: *Cell, out: *int) { *out = c.n; }

fn run(c: *Cell, out: *int)
{
	let job = dispatch c -> copy(out);
	join(job);
}
```

```anti refused
//! error: the result of a worker has type `*int`, which holds a pointer. A worker takes and returns values alone
class Cell { pub n: int = 0, }

worker fn inside(c: *Cell) -> *int { return &c.n; }

fn run(c: *Cell)
{
	let job = dispatch c -> inside;
	join(job);
}
```

```anti refused
//! error: `check` may fail, and `dispatch` runs a worker that cannot fail
class Cell { pub n: int = 0, }

worker fn check(c: *Cell) may fail { fail "empty"; }

fn run(c: *Cell)
{
	let job = dispatch c -> check;
	join(job);
}
```

```anti refused
//! error: `read` is not a `worker fn`
class Cell { pub n: int = 0, }

fn read(c: *Cell) -> int { return c.n; }

fn run(c: *Cell) -> int
{
	let job = dispatch c -> read;
	return join(job);
}
```

```anti refused
//! error: `read` takes `*Cell` as its object, and this is `*Wide`
class Cell { pub n: int = 0, }
class Wide inherits Cell { }

worker fn read(c: *Cell) -> int { return c.n; }

fn run(w: *Wide) -> int
{
	let job = dispatch w -> read;
	return join(job);
}
```

```anti refused
//! error: `add` takes 1 argument beside the object, found 0
class Cell { pub n: int = 0, }

worker fn add(c: *Cell, k: int) -> int { return c.n + k; }

fn run(c: *Cell) -> int
{
	let job = dispatch c -> add;
	return join(job);
}
```

```anti refused
//! error: expected a call or an assignment
class Cell { pub n: int = 0, }

worker fn set(c: *Cell, k: int) { c.n = k; }

fn run(c: *Cell)
{
	dispatch c -> set(1);
}
```

```anti refused
//! error: unknown type `Job`
class Cell { pub n: int = 0, }

worker fn read(c: *Cell) -> int { return c.n; }

fn run(c: *Cell) -> int
{
	let job: Job = dispatch c -> read;
	return join(job);
}
```

```anti refused
//! error: `join` waits for a job of `dispatch`, and this is `int`
fn run(n: int) -> int
{
	return join(n);
}
```

```anti refused
//! error: `join_all` waits for a slice of jobs, and this is `Job`
class Cell { pub n: int = 0, }

worker fn read(c: *Cell) -> int { return c.n; }

fn run(c: *Cell)
{
	let job = dispatch c -> read;
	join_all(job);
}
```

```anti refused
//! error: `finish` is a `worker fn` and cannot `delete` an object another one may hold
class Cell { pub n: int = 0, }

fn end(own c: *Cell) { delete(c); }

worker fn finish(c: *Cell) { end(c); }

fn run(c: *Cell)
{
	let job = dispatch c -> finish;
	join(job);
}
```

```anti refused
//! error: `level` is `mutable` in singleton `Config` and `worker fn read` reaches it
singleton class Config { pub mutable level: int = 0, }

class Cell { pub n: int = 0, }

worker fn read(c: *Cell) { c.n = Config.get().level; }

fn run(c: *Cell)
{
	let job = dispatch c -> read;
	join(job);
}
```

```anti refused
//! error: a worker takes a closure through a `concurrent` parameter alone
class Cell { pub n: int = 0, }

worker fn apply(c: *Cell, f: fn(int) -> int) { c.n = f(c.n); }

fn run(c: *Cell)
{
	let k = 2;
	let job = dispatch c -> apply(fn(x) { return x * k; });
	join(job);
}
```

```anti refused
//! error: `worker` stands before `fn`
worker struct Point { x: int }
```

```anti refused
//! error: `k` was moved into `keep` on line
class Cell { pub n: int = 0, }

class Key { pub id: int = 0, fn destruct(self) { } }

worker fn keep(c: *Cell, own k: Key) -> int { return k.id; }

fn run(c: *Cell) -> int
{
	let k = Key { id: 7 };
	let job = dispatch c -> keep(k);
	let n = join(job);
	return n + k.id;
}
```

```anti refused
//! error: a reference to `c` is passed to `dispatch`
class Cell { pub n: int = 0, }

worker fn bump(c: *Cell, k: int) { c.n += k; }

fn start() -> int
{
	let c = Cell { };
	let job = dispatch &c -> bump(1);
	return c.n;
}
```

### How antic builds it

A `dispatch` is one call of `anti_rt_dispatch`, and a `join` one call of `anti_rt_join_hooked`. The module below is `start` of the example after opt.

```llvm excerpt=workers target=linux-x86_64 mode=release
define internal noundef i64 @start(ptr noundef nonnull dereferenceable(32) %p0, i64 noundef %p1)
b0:
  %s3 = alloca [8 x i8], align 8
  %s9 = alloca [8 x i8], align 8
  store i64 %p1, ptr %s3, align 8
  %v7 = call ptr @anti_rt_dispatch(ptr nonnull %p0, i64 8, ptr nonnull @workers.dispatch.1, ptr nonnull %s3, i64 8)
  call void @anti_rt_hook(ptr nonnull %p0, i64 3)
  call void @anti_rt_join_hooked(ptr %v7, i64 8, ptr nonnull %s9)
  %v18 = load i64, ptr %s9, align 8
  ret i64 %v18
```

- `%s3` is the context of the `dispatch`, a slot of the frame of `start` that holds the arguments after the object. Here it holds `seed`.
- `anti_rt_dispatch` takes the object, the 8 bytes of the result, a thunk, the context and its 8 bytes. It copies the context into the job. A later turn of a loop or a return of `start` then leaves the arguments of the job as they were. It returns the job, one pointer.
- `anti_rt_hook` with 3 is the hook `dispatched` of the object, which [chapter 15]({{% relref "15-hooks" %}}) describes.
- `anti_rt_join_hooked` takes the job, the size of the result and the slot `%s9` that receives it. It calls the hook `joined` of the object after the wait.
- The `load` reads the result out of the slot.

The thunk is a function antic writes per `dispatch` in the source, named `dispatch.N` in its module. The pool calls it with the context, the object and the address of the result. The dev build keeps it apart from the worker.

```llvm excerpt=workers target=linux-x86_64 mode=dev
define hidden void @workers.dispatch.1(ptr noundef %p0, ptr noundef %p1, ptr noundef %p2) #0 {
...
  %v2 = load ptr, ptr %t3, align 8
  %v3 = load i64, ptr %v2, align 8
...
  %v6 = call i64 (ptr, i64) @workers.fill(ptr %v4, i64 %v5)
...
  %v8 = load ptr, ptr %t2, align 8
  store i64 %v7, ptr %v8, align 8
  ret void
```

- `%p0` is the context, `%p1` the object and `%p2` the address of the result.
- The first `load i64` reads `seed` out of the context.
- The call runs the worker with the object first.
- The `store` writes what the worker gave to the address in `%p2`.

A release build inlines the worker into its thunk, so the pool calls the loop of `fill` directly.

A worker with a fresh argument of a parameter that is not `own` has a thunk that tears the argument down after the call. The thunk of `look` of the second example calls the teardown of the `Key` in the job's copy of the context.

```llvm excerpt=jobs target=linux-x86_64 mode=dev
define hidden void @jobs.dispatch.3(ptr noundef %p0, ptr noundef %p1, ptr noundef %p2) #0 {
...
  %v7 = call i64 (ptr, i64, i64) @jobs.look(ptr %v2, i64 %v4, i64 %v6)
...
  call void (ptr, ptr) @jobs.Key.destroy(ptr %v18, ptr null)
  ret void
```

- The call passes the object and then the `Key` by value, its 16 bytes as two words copied out of the context. arm64 passes the two words as one `[2 x i64]`.
- `jobs.Key.destroy` runs after the worker returned, on the thread of the pool that ran the job. It is the teardown that calls `destruct`.

The runtime keeps the rest. `anti_rt_dispatch` allocates the job, a copy of the context and the room of the result. It takes the lock of the pool and walks the list of the objects in flight. It returns no job when it finds the object there. Otherwise it puts the job on a queue and wakes the threads of the pool. A thread of the pool takes the job, calls the thunk and marks the job done. `anti_rt_join` waits for that mark, copies the result out and frees the three blocks. A job that no thread took yet is run by the joining thread itself, so a pool of no threads still finishes every job.

The pool starts at the first `dispatch` or `parallel` of the program. Its threads run until the program ends, and nothing joins them.

The arguments of a job live in the job from the `dispatch` to the `join`. The next turn of a loop writes the slot of the frame again, and a return ends it. Neither reaches a job that has started.

arm64 passes the same four arguments in `x0` to `x3`, and nothing else differs.

A dev build marks a job that it joined. It tests the job before the `join` and writes the address 1 into the variable after it, an address that no allocation gives.

```ir excerpt=workers target=linux-x86_64 mode=dev
export fn workers.start(%0: ptr nonnull deref(size_of workers.Image), %1: i64) -> i64 {
...
    %8 = load ptr %2
    %9 = ne i8 %8, 1
    branch %9, b2, b1
b1:
...
    call void @anti_rt_check_failed(%10, 47, 7, 0, 0)
    unreachable
b2:
    %11 = slot i64
    call void @anti_rt_join_hooked(%8, size_of i64, %11)
    %12 = load i64 %11
    store ptr 1, %2
```

- `%8` is the job, loaded from its variable `%2`. `ne` compares it with the mark 1, and `b1` stops the program with `` `join` of a job joined already ``.
- `store ptr 1, %2` marks the variable after the `join`. A release build writes neither the test nor the mark.
- `join_all(jobs)` of a dev build joins the jobs one at a time in a loop, each with the same test and mark. A release build makes one call of `anti_rt_join_all_hooked`.

Where it lives: `sema_check_dispatch`, `sema_check_join`, `worker_callee` and `worker_args` of `src/antic/sema_thread.c`, `sema_walk_worker` of `src/antic/sema_stmt.c`, `types_pointer_free` of `src/antic/types.c`, `lower_dispatch`, `dispatch_thunk` and `lower_join` of `src/antic/lower_worker.c`, and `anti_rt_dispatch` and `anti_rt_join` of `src/rt/threads.c`.

### What it costs

The costs are the same in dev and in release, but for the row of the checks.

| Cost | Count |
|---|---|
| Bytes of a job | 8, the one pointer `anti_rt_dispatch` returns |
| `dispatch` | one call of the runtime and one of `anti_rt_hook` |
| Allocations of a `dispatch` | one `calloc` of 56 bytes for the job, one `malloc` of the context when the worker takes arguments beside the object, and one `malloc` of the result when the worker returns a value |
| Copies of a `dispatch` | the context, once into the job |
| Locks of a `dispatch` | the lock of the pool, taken once |
| `join` | one call of the runtime, the lock of the pool, one copy of the result, three calls of `free`, and the hook `joined` |
| Atomic operations | one atomic load per hook call, of the installed trace handler: one for a `dispatch` and one for a `join`. 0 with `--no-hooks` |
| Threads | the processors of the machine less one, or the key `threads` less one, started once |
| Checks | the test for an object in flight, in both modes. In a dev build, one compare and one branch before each `join` and one store of the mark after it |
| Copies of code | one thunk per `dispatch` in the source |

The walk of the objects in flight is linear in their number.

`--no-hooks` drops the call of `anti_rt_hook` after a `dispatch` and makes a `join` a call of `anti_rt_join`. A program built that way gives up the hooks `dispatched` and `joined`, and with them the `ThreadMonitor` of [chapter 15]({{% relref "15-hooks" %}}).

Three forms run code of a function somewhere, and the counts above choose between them.

| Form | Allocations | Locks | Use it for |
|---|---|---|---|
| A plain call `f(obj, args)` | 0 | 0 | work that is short beside a `calloc`, a lock and the wake of a thread |
| `dispatch obj -> f(args)` | 2 or 3 per job | the pool's, at the `dispatch` and at the `join` | separate objects, each with work of its own, and a result per object |
| `parallel s -> f` | 1 per call, whatever the count of chunks | the pool's, once and twice per chunk | one slice split into parts that take the same work |

Each `dispatch` pays its allocations and its lock again, so a thousand small jobs cost a thousand of each. One `parallel` over a slice of a thousand elements pays one allocation for the results.

### Pitfalls

For a Java programmer:

- No thread has a name or a handle. A program reaches the pool through `dispatch` and `parallel` alone.
- A job is no `Future` that travels. It has no type name and stays in the function that made it.
- A second `dispatch` of an object in flight is no error and no wait. Its `join` gives zero.

For a C programmer:

- The job holds a copy of its arguments. A function may return before it joins, and a `dispatch` written once in a loop gives each job the arguments of its own turn.
- `join` frees the job. A second `join` of a job is refused when a later statement of the block that declares the job holds it. A dev build stops one the checker cannot see with `` `join` of a job joined already ``, since a `join` there marks the job it held. A release build does not, and a copy of the job made before the first `join` carries no mark in either build. Both read freed memory.

For a C++ programmer:

- The checker follows the parameters of a worker and stops at the object. A pointer in a field of the object passes, and two objects that share one target race.
- A job that is never joined leaks its block and its result. Nothing detaches.
- On a machine of one processor the pool has no thread, and a job runs when it is joined. A program that waits for a `send` of a job before it joins the job waits for ever there.
- A worker cannot fail and cannot return an error. It writes its outcome into the object.

## parallel

### What you know

Java splits work with parallel streams or a `ForkJoinPool`. C and C++ write `#pragma omp parallel for` or split the range by hand and start a thread per part. In each, the body may write any variable in reach, and a race on one is the programmer's to find.

### The feature

`parallel` splits a slice into chunks, runs one worker over each chunk through the pool and waits for all of them.

| Form | What it does |
|---|---|
| `parallel s -> f` | as many chunks as the pool has workers |
| `parallel s by n -> f` | `n` chunks |
| `parallel s by n -> f(args)` | every chunk receives `args` after the chunk itself |

`s` is a slice, and an array is written `a[0..n]`. The first parameter of the worker is a slice of the same element type. The element type, every other argument and the result hold no pointer, by the rule of the section above.

The expression has the type `[]R`, one result per chunk in the order of the chunks. The results are memory of the heap that the program frees with `free(r.ptr)`.

The chunks are contiguous, disjoint and cover the slice. With `count` elements and `n` chunks, the first `count % n` chunks hold one element more than the others. A count of chunks above the count of elements becomes the count of elements. An empty slice gives an empty result and does not touch the pool. `by 0` takes the count of the machine: the key `threads`, or the count of its processors. A constant count below zero is refused. A dev build stops the program at a count below zero that it computes, and a release build takes one as zero.

`by` sets the count of the chunks and never the count of the threads. The thread that wrote `parallel` takes chunks beside the pool and returns when every chunk is done.

A worker may write the elements of its chunk. No other worker reaches them.

Every chunk reads the same arguments. A fresh value passed to a parameter that is not `own` is torn down by the function that wrote the `parallel`, once every chunk has run.

Only one `parallel` uses the pool at a time. A `parallel` that finds the pool busy, from a worker or from another thread, runs its chunks one after the other on its own thread.

```anti
//! name: chunks
import anti.io;
import anti.text;

struct Span { low: int, high: int }

worker fn sum(chunk: []int) -> int
{
	let total = 0;
	for x in chunk {
		total += x;
	}
	return total;
}

worker fn above(chunk: []int, limit: int, step: int) -> int
{
	let count = 0;
	for x in chunk {
		if x * step > limit {
			count += 1;
		}
	}
	return count;
}

worker fn span(chunk: []int) -> Span
{
	return Span { low: chunk[0], high: chunk[chunk.len - 1] };
}

worker fn double(chunk: []int) -> int
{
	for i in 0..chunk.len {
		chunk[i] = chunk[i] * 2;
	}
	return chunk.len;
}

export fn total(first: *int, n: int) -> int
{
	let data = []int { ptr: first, len: n };
	let parts = parallel data -> sum;
	let all = 0;
	for p in parts {
		all += p;
	}
	free(parts.ptr);
	return all;
}

export fn count(first: *int, n: int, limit: int) -> int
{
	let data = []int { ptr: first, len: n };
	let parts = parallel data by 4 -> above(limit, 2);
	let all = 0;
	for p in parts {
		all += p;
	}
	free(parts.ptr);
	return all;
}

fn main() -> int
{
	let numbers: [10]int = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
	let data = numbers[0..10];
	let spans = parallel data by 4 -> span;
	for s in spans {
		let line = f"chunk {s.low}..{s.high}";
		io.println(line);
		free(line.ptr);
	}
	free(spans.ptr);

	let many = parallel data by 25 -> sum;
	let none_at_all = parallel data[0..0] by 4 -> sum;
	let line = f"by 25 gives {many.len} chunks, an empty slice gives {none_at_all.len}";
	io.println(line);
	free(line.ptr);
	free(many.ptr);

	line = f"above 9 when doubled: {count(&numbers[0], 10, 9)}";
	io.println(line);
	free(line.ptr);

	let lens = parallel data by 2 -> double;
	free(lens.ptr);
	line = f"total after doubling {total(&numbers[0], 10)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
chunk 1..3
chunk 4..6
chunk 7..8
chunk 9..10
by 25 gives 10 chunks, an empty slice gives 0
above 9 when doubled: 6
total after doubling 110
```

Ten elements in four chunks give 3, 3, 2 and 2. `total` writes no `by`, so its count of chunks is the count of workers of the machine. Its sum is the same for every count.

The checker refuses each of the following.

```anti refused
//! error: `parallel` splits a slice, and this is `[4]int`
worker fn sum(chunk: []int) -> int { return chunk.len; }

fn run()
{
	let a: [4]int = [1, 2, 3, 4];
	let r = parallel a -> sum;
	free(r.ptr);
}
```

```anti refused
//! error: `sum` takes `[]i32` as its chunk, and the slice is `[]int`
worker fn sum(chunk: []i32) -> int { return chunk.len; }

fn run(a: []int)
{
	let r = parallel a -> sum;
	free(r.ptr);
}
```

```anti refused
//! error: the chunk has type `*int`, which holds a pointer. A worker takes and returns values alone
worker fn count(chunk: []*int) -> int { return chunk.len; }

fn run(a: []*int)
{
	let r = parallel a -> count;
	free(r.ptr);
}
```

```anti refused
//! error: an argument of a worker has type `*int`, which holds a pointer. A worker takes and returns values alone
worker fn sum(chunk: []int, out: *int) -> int { return chunk.len; }

fn run(a: []int, out: *int)
{
	let r = parallel a -> sum(out);
	free(r.ptr);
}
```

```anti refused
//! error: `sum` is not a `worker fn`
fn sum(chunk: []int) -> int { return chunk.len; }

fn run(a: []int)
{
	let r = parallel a -> sum;
	free(r.ptr);
}
```

```anti refused
//! error: expected `int`, found a float literal
worker fn sum(chunk: []int) -> int { return chunk.len; }

fn run(a: []int)
{
	let r = parallel a by 2.5 -> sum;
	free(r.ptr);
}
```

```anti refused
//! error: expected a call or an assignment
worker fn sum(chunk: []int) -> int { return chunk.len; }

fn run(a: []int)
{
	parallel a -> sum;
}
```

```anti refused
//! error: the count of chunks -1 is below zero
worker fn sum(chunk: []int) -> int { return chunk.len; }

fn run(a: []int)
{
	let r = parallel a by -1 -> sum;
	free(r.ptr);
}
```

A count that the checker cannot compute passes, and a dev build stops at it with a line such as the one below. The release build of the same program runs the slice in as many chunks as the machine has threads.

```text
main.anti:8: count of chunks below zero: value -2
```

### How antic builds it

A `parallel` is one call of `anti_rt_parallel`. The module below is `count` of the example after opt.

```llvm excerpt=chunks target=linux-x86_64 mode=release
define internal noundef i64 @count(ptr noundef nonnull dereferenceable(8) %p0, i64 noundef %p1, i64 noundef %p2)
b0:
  %s4 = alloca [16 x i8], align 8
  %s7 = alloca [8 x i8], align 8
  %s8 = alloca [8 x i8], align 8
  store i64 %p2, ptr %s4, align 8
  %v5 = getelementptr inbounds nuw i8, ptr %s4, i64 8
  store i64 2, ptr %v5, align 8
  call void @anti_rt_parallel(ptr nonnull %p0, i64 %p1, i64 8, i64 4, i64 8, ptr nonnull @chunks.parallel.1, ptr nonnull %s4, ptr nonnull %s7, ptr nonnull %s8)
  %v14 = load ptr, ptr %s7, align 8
  %v18 = load i64, ptr %s8, align 8
```

- `%s4` is the context, 16 bytes for `limit` and the constant 2 of `step`. The two `store` lines fill it.
- The call passes the first element and the count of the slice, the 8 bytes of an element and the 4 chunks of `by`. The 8 bytes of a result, the thunk and the context follow.
- `%s7` and `%s8` receive the pointer and the count of the results. The runtime writes both, since it decides the count when the source has no `by`. A `parallel` without `by` passes 0 for the chunks.
- The two `load` lines build the slice `parts` from them.

The thunk of a `parallel` is named `parallel.N`. The pool calls it once per chunk with the context, the first element of the chunk, its length and the address of its result.

```llvm excerpt=chunks target=linux-x86_64 mode=dev
define hidden void @chunks.parallel.1(ptr noundef %p0, ptr noundef %p1, i64 noundef %p2, ptr noundef %p3) #0 {
...
  %v0 = load ptr, ptr %t1, align 8
  %v1 = load ptr, ptr %t4, align 8
  store ptr %v0, ptr %v1, align 8
...
  %v4 = load i64, ptr %t2, align 8
  %v5 = load ptr, ptr %t5, align 8
  store i64 %v4, ptr %v5, align 8
...
  %v20 = call i64 (i64, i64, i64, i64) @chunks.above(i64 %v15, i64 %v17, i64 %v18, i64 %v19)
...
  %v22 = load ptr, ptr %t3, align 8
  store i64 %v21, ptr %v22, align 8
  ret void
```

- The first two `store` lines build the slice of the chunk in a slot of the thunk: the pointer `%p1` and the length `%p2`.
- The call passes the slice as two words, then `limit` and `step` out of the context.
- The last `store` writes the result of the worker to the address in `%p3`, the place of this chunk in the block of results.

`anti_rt_parallel` allocates the block of results with one `malloc`, takes the lock of the pool and wakes its threads. Each thread, the caller among them, takes the next chunk number under the lock and calls the thunk outside it. The caller sleeps on a condition of the pool until the count of finished chunks reaches the count of chunks.

The context stays in the frame of the caller, where `dispatch` copies it into the job. The caller waits in that frame until every chunk is done, so every worker reads a live context, and no copy is made.

Where it lives: `sema_check_parallel` and `chunks_below_zero` of `src/antic/sema_thread.c`, `lower_parallel`, `parallel_thunk`, `chunk_count` and `drop_context` of `src/antic/lower_worker.c`, and `anti_rt_parallel`, `slice_of` and `worker_main` of `src/rt/threads.c`.

### What it costs

The costs are the same in dev and in release, but for the row of the checks.

| Cost | Count |
|---|---|
| `parallel` | one call of the runtime |
| Allocations | one `malloc` of the results, chunks times the bytes of a result, which the program frees. None for a slice of no elements |
| Calls per chunk | one indirect call of the thunk, and in a dev build one call of the worker from it |
| Locks | the lock of the pool, taken once at the start and twice per chunk |
| Atomic operations | 0 |
| Checks | the bound of the results, in both modes. In a dev build, a count of chunks below zero that the checker could not compute |
| Copies of code | one thunk per `parallel` in the source |

The bound of the results stops the program when the chunks times the bytes of a result pass `INT64_MAX / 2`.

A worker that returns a struct writes it into the block of results in place. The `Span` of the example is 16 bytes per chunk.

The thread that wrote `parallel` returns when the slowest chunk is done, and a chunk runs on one thread from its start to its end. A larger `by` makes smaller chunks, which balance uneven work. Each chunk more costs one indirect call and two takes of the lock of the pool. `dispatch` is the form for work that is not one slice, as the choice of the section before shows. A plain loop costs none of the above. It is the form for a slice whose work is shorter than the wake of the threads of the pool.

### Pitfalls

For a Java programmer:

- The result is a slice of one value per chunk, and the program folds it. No reduction is built in.
- The results are not collected. A `parallel` in a loop without `free(r.ptr)` leaks one block per turn.

For a C programmer:

- `by 4` is four chunks, on however many threads the pool has. The thread count is the `threads` key of the runtime configuration.
- A worker receives values. A shared total goes through the results or through a `static atomic` field.

For a C++ programmer:

- A `parallel` inside a worker of a `parallel` runs on one thread. The pool does not nest.
- The split is fixed before the first chunk runs. A slow chunk is not split again, so chunks of uneven work need a larger `by`.

## Channels and select

### What you know

Java has `BlockingQueue` with `put` and `take`. C++ and C build the same from a mutex, two condition variables and a ring. Waiting on two queues at once needs a third condition or a poll in each.

### The feature

A channel is a bounded queue of values that threads share.

| Form | What it does |
|---|---|
| `chan T(n)` | makes a channel of `T` with room for `n` values |
| `send(c, v)` | puts a copy of `v` into the channel and waits while it is full |
| `recv(c)` | takes the oldest value and waits while the channel is empty and open |
| `close(c)` | ends what the channel takes |
| `delete(c)` | tears down the values the channel still holds and frees it |
| `select { a x => stmt, b y => stmt }` | takes from whichever channel is ready |

`chan T` is a type, written in a parameter or a field as it is written in the expression. A value of it is one handle, and a copy names the same channel.

`T` holds no pointer, by the rule of the first section. `n` is an `int` of 1 or more, and a channel made for less stops the program with a message.

`recv(c)` gives a `?*T`. It points at the value, or is `none` once the channel is closed and empty. What a channel holds at its `close` is still received. The pointer goes into a slot of the frame of the function that wrote the `recv`. Each `recv` in the source has one slot and fills it again each time it runs.

A value that owns something belongs to the block that holds the `recv`, which for `let v = recv(c)` is the block of `v`. The end of that block tears it down on every exit, so the program writes no `destroy`. `destroy(v)` ends it before, and the block then tears nothing down. A `recv` in the condition of a loop tears down the value of the turn before when it runs again. The value an arm of `select` takes belongs to the block that holds the `select` in the same way.

`close` twice changes nothing. A `send` on a closed channel stops the program with the message `` `send` on a closed channel ``, a waiting one included. `send`, `recv`, `close` and `select` cannot fail, so none takes a handler.

A worker takes a channel beside its values and shares it with the thread that made it. It cannot `delete` one.

An arm of a `select` names a channel and, before the arrow, the name that receives what `recv` of that channel would give. The name may be left out. An arm is a call, an assignment or a block, as an arm of a `switch` is. A `select` has one arm or more and no `else`, so it always waits.

A closed and empty channel counts as ready and gives `none`. Among the channels that are ready, `select` takes the first from a place that moves one arm further for each `select` of the thread.

`send`, `recv`, `close` and `select` take a channel value. A pointer `p` to one is written `*p`.

```anti
//! name: pipes
import anti.io;
import anti.text;

struct Reading { sensor: int, value: f64 }

class Producer
{
	pub first: int = 0,
	pub count: int = 0,
}

worker fn produce(p: *Producer, out: chan int) -> int
{
	for i in 0..p.count {
		send(out, p.first + i);
	}
	close(out);
	return p.count;
}

export fn relay(n: int) -> int
{
	let c = chan int(4);
	send(c, n);
	close(c);
	let v = recv(c) else {
		return 0;
	};
	let got = *v;
	delete(c);
	return got;
}

fn drain(a: chan int, b: chan int) -> (int, int)
{
	let from_a = 0;
	let from_b = 0;
	let a_open = true;
	let b_open = true;
	while a_open || b_open do {
		select {
			a x => {
				let v = x else {
					a_open = false;
					continue;
				};
				from_a += *v;
			},
			b y => {
				let v = y else {
					b_open = false;
					continue;
				};
				from_b += *v;
			},
		}
	}
	return (from_a, from_b);
}

fn main() -> int
{
	let c = chan Reading(2);
	send(c, Reading { sensor: 1, value: 20.5 });
	send(c, Reading { sensor: 2, value: 21.0 });
	close(c);
	close(c);
	let seen = 0;
	let warmest = 0.0;
	while true do {
		let r = recv(c) else {
			break;
		};
		seen += 1;
		if r.value > warmest {
			warmest = r.value;
		}
	}
	let line = f"{seen} readings after close, warmest {warmest}, relay gives {relay(7)}";
	io.println(line);
	free(line.ptr);
	delete(c);

	let a = chan int(2);
	let b = chan int(2);
	let pa = alloc Producer { first: 1, count: 300 };
	let pb = alloc Producer { first: 1000, count: 200 };
	let ja = dispatch pa -> produce(a);
	let jb = dispatch pb -> produce(b);
	let (sum_a, sum_b) = drain(a, b);
	let sent_a = join(ja);
	let sent_b = join(jb);
	line = f"a sent {sent_a} with sum {sum_a}, b sent {sent_b} with sum {sum_b}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: chan int {size_of(chan int)}, chan Reading {size_of(chan Reading)}";
	io.println(line);
	free(line.ptr);
	delete(a);
	delete(b);
	free(pa);
	free(pb);
	return 0;
}
```

```text output
2 readings after close, warmest 21, relay gives 7
a sent 300 with sum 45150, b sent 200 with sum 219900
sizes: chan int 8, chan Reading 8
```

The two producers run in the pool and wait whenever their channel of two values is full. `drain` runs on the main thread and takes from whichever channel has a value. Each producer closes its channel, and `drain` ends when both arms have seen `none`.

The checker refuses each of the following.

```anti refused
//! error: a channel carries values alone, and `*int` holds a pointer
fn run()
{
	let c = chan *int(4);
}
```

```anti refused
//! error: expected `int`, found `str`
fn run()
{
	let c = chan int(4);
	send(c, "one");
}
```

```anti refused
//! error: `v` may be `none`, check it or use `?*T`
fn run() -> int
{
	let c = chan int(4);
	let v = recv(c);
	return *v;
}
```

```anti refused
//! error: `send` takes a channel, found `int`
fn run()
{
	let n = 4;
	send(n, 1);
}
```

```anti refused
//! error: `close` takes 1 argument, found 2
fn run()
{
	let c = chan int(4);
	close(c, 1);
}
```

```anti refused
//! error: a `select` waits on one channel or more
fn run(a: chan int)
{
	select { }
}
```

```anti refused
//! error: a `select` has no `else`
fn run(a: chan int) -> int
{
	select {
		a x => { return 1; },
		else => { return 0; }
	}
}
```

```anti refused
//! error: an arm of a `select` names a channel, found `int`
fn run(a: chan int, n: int)
{
	select {
		n x => { },
	}
}
```

```anti refused
//! error: `finish` is a `worker fn` and cannot `delete` an object another one may hold
class Cell { pub n: int = 0, }

worker fn finish(c: *Cell, own out: chan int) { delete(out); }

fn run(c: *Cell)
{
	let out = chan int(1);
	let job = dispatch c -> finish(out);
	join(job);
}
```

```anti refused
//! error: an argument of a worker has type `*chan int`, which holds a pointer. A worker takes and returns values alone
class Cell { pub n: int = 0, }

worker fn report(c: *Cell, out: *chan int) { send(*out, c.n); }

fn run(c: *Cell)
{
	let out = chan int(1);
	let job = dispatch c -> report(&out);
	join(job);
	delete(out);
}
```

```anti refused
//! error: the parameter `c` of export fn `feed` has type `chan int`, which C cannot represent
export fn feed(c: chan int) { }
```

### How antic builds it

Every operation of a channel is one call of the runtime with the handle. The module below is `relay` of the example after opt.

```llvm excerpt=pipes target=linux-x86_64 mode=release
define internal noundef i64 @relay(i64 noundef %p0)
b0:
  %s3 = alloca [8 x i8], align 8
  %s4 = alloca [8 x i8], align 8
  %v0 = tail call ptr @anti_rt_chan_new(i64 8, i64 4)
  store i64 %p0, ptr %s3, align 8
  call void @anti_rt_chan_send(ptr %v0, ptr nonnull %s3)
  call void @anti_rt_chan_close(ptr %v0)
  %v18 = call ptr @anti_rt_chan_recv(ptr %v0, ptr nonnull %s4)
  %v20 = icmp eq ptr %v18, null
```

- `anti_rt_chan_new` takes the 8 bytes of an `int` and the capacity 4. It returns the handle, a pointer to the channel on the heap.
- `send` passes the address of the value. `%s3` is a slot that holds `n`, and the runtime copies its 8 bytes into the ring.
- `recv` passes the slot `%s4` of this `recv`. The runtime copies the oldest value into it and returns the address of the slot, or null.
- The `icmp` is the test of `let v = recv(c) else`.

The channel in memory is one block: a header and the ring of `n` values after it.

| Field of the header | Bytes | Holds |
|---|---|---|
| `monitor` | 8 | a lock and two conditions of the system, in a block of their own |
| `size` | 8 | the bytes of one value |
| `capacity` | 8 | the room `n` |
| `head` | 8 | the index of the oldest value |
| `count` | 8 | the values in the ring |
| `closed` | 4, and 4 of padding | 1 after `close` |
| `values` | 8 | the address of the ring |

A sender waits on the condition "not full" and a receiver on "not empty". `close` wakes both sides.

A `select` passes two arrays of its frame: the handles of its channels and the slot of each arm.

```llvm excerpt=pipes target=linux-x86_64 mode=dev
  %v22 = load ptr, ptr %t2, align 8
  %v23 = load ptr, ptr %t3, align 8
  %v24 = load ptr, ptr %t4, align 8
  %v25 = call i64 (ptr, ptr, i64, ptr) @anti_rt_select(ptr %v22, ptr %v23, i64 2, ptr %v24)
  store i64 %v25, ptr %t18, align 8
  %v26 = load i64, ptr %t18, align 8
  %v27 = icmp eq i64 %v26, 0
```

- `%v22` is the array of the two handles, `%v23` the array of the two slots and 2 the count of the arms.
- `%v24` is the address that receives what the arm binds: the slot of the arm, filled, or null.
- The call returns the index of the arm that was ready, and the `icmp` starts the chain of tests that picks its code.

`anti_rt_select` tries each channel once under its lock, starting at the arm the thread's counter names. When none is ready it counts itself in one atomic counter of the program and takes one lock that all selects share. It looks at every channel again and sleeps. A `send` or a `close` reads that counter after its own work and wakes the selects only when it is above zero.

Where it lives: `sema_check_sync_op` and `sema_check_close` of `src/antic/sema_thread.c`, `lower_sync_op` and `lower_select` of `src/antic/lower_sync.c`, and `anti_rt_chan_new`, `anti_rt_chan_send`, `anti_rt_chan_recv` and `anti_rt_select` of `src/rt/sync.c`.

### What it costs

The costs are the same in dev and in release.

| Cost | Count |
|---|---|
| Bytes of a channel value, `size_of(chan int)` | 8, the handle |
| Bytes on the heap | 56 for the header and `n` times the bytes of a value, in one block, and one block for the monitor |
| `chan T(n)` | one call of the runtime and two allocations |
| `send` | one call, the lock of the channel once, one copy of the value, one atomic load |
| `recv` | one call, the lock of the channel once, one copy of the value, no allocation |
| `close` | one call, the lock of the channel once, one atomic load |
| `select` that finds a value | one call and the lock of each channel it tries |
| `select` that waits | two atomic operations, the lock of all selects and the lock of each channel twice, per wake |
| `delete` | one call and two calls of `free`. A channel of values that own something first takes each value it holds with one call of `anti_rt_chan_held` and tears it down, and one call more finds it empty |
| Checks | the capacity at `chan T(n)`, the closed flag at `send` and a null handle at every operation, in both modes |

A `send` that finds the channel full and a `recv` that finds it empty sleep in the system until the other side wakes them.

Every `send` and every `close` of the program wakes every waiting `select`, whatever channels it waits on. Each looks at its channels and sleeps again when none is ready.

Three other forms move values between threads, each for a narrower case.

| Form | Per value | Use it for |
|---|---|---|
| A channel | one call, the lock of the channel, one copy | any count of senders and receivers, waits when full or empty, and `select` over more than one |
| The result of `join` or of `parallel` | one copy out of the job or the block | one value per job or per chunk, handed back once at the end |
| `SpscRing<T, N>` of [chapter 14]({{% relref "14-concurrent-classes" %}}) | one atomic load and one atomic store, no call | one producing thread and one consuming thread that may poll in place of a wait |
| A field under a `Mutex` | one lock call and one unlock call | a state that threads read and change in place, with no queue |

### Pitfalls

For a Java programmer:

- `recv` gives a pointer or `none`, and throws nothing. A closed and empty channel is the `none`.
- A `send` after `close` ends the program. No exception reaches a handler.
- A channel is not collected. `delete(c)` frees it, and a channel that is never deleted leaks two blocks.

For a C programmer:

- The pointer of a `recv` is valid until the same `recv` runs again. A loop that keeps the pointer of an earlier turn reads the value of the latest one.
- A copy of a channel is a copy of its handle. `delete` through one copy leaves the others with a dead pointer.

For a C++ programmer:

- A channel carries no pointer and no slice, so no `unique_ptr` travels through one. A channel of indexes or of small structs is the form.
- A `select` has no default arm and no timeout. It returns when a channel is ready.
- A `select` on a closed channel returns at once, every time. The arm tests for `none` and the loop stops asking.

## Atomics

### What you know

Java has `AtomicInteger` and `AtomicReference` as objects with methods. C++ has `std::atomic<T>` with a memory order per operation, and C11 has `_Atomic` with the functions of `stdatomic.h`. In C and C++ the operators `++` and `=` work on an atomic and are sequentially consistent.

### The feature

`atomic` marks a variable that is read and written by atomic operations alone.

| Place | Written |
|---|---|
| A field of a class | `pub atomic hits: int = 0,` |
| A static field of a class | `static atomic total: int = 0;` |
| A local | `let seen: atomic int = 0;` |

An atomic local holds an integer of 8, 16, 32 or 64 bits, a `bool`, a `char` or a pointer. An atomic field holds the same, an `f16` and a plain function value as well, which is one C function pointer. A struct takes no atomic field, and neither a parameter nor a result is atomic.

Every static field is atomic. The parser refuses one without the word, since a static field is a global that every thread reaches. [Chapter 9]({{% relref "09-classes" %}}) describes static fields.

Eight operations exist, written as calls on the variable.

| Operation | Gives | Does |
|---|---|---|
| `x.load()` | the value | reads |
| `x.store(v)` | nothing | writes |
| `x.swap(v)` | the value before | writes `v` |
| `x.add(v)` | the value before | adds |
| `x.sub(v)` | the value before | subtracts |
| `x.and(v)` | the value before | bitwise and |
| `x.or(v)` | the value before | bitwise or |
| `x.compare_swap(old, new)` | a `bool` | writes `new` when the value is `old`, and says whether it did |

Every operation is sequentially consistent. No form names a weaker order.

Not every type takes all eight.

| Type | Operations |
|---|---|
| An integer, a `char` | all eight |
| `bool` | `load`, `store`, `swap`, `and`, `or` and `compare_swap` |
| A pointer, a nullable pointer, a function value | `load`, `store`, `swap` and `compare_swap` |
| `f16` | `load`, `store`, `swap` and `compare_swap`, which move its 16 bits |

A `bool` has no arithmetic, so `and` and `or` keep it 0 or 1. A pointer has none either, so it never holds an address the program did not take, nor `none` where its type holds none.

A plain read, an assignment and `&x` of an atomic are refused. `add` and `sub` wrap in both modes and have no overflow check.

`compare_swap` also takes a plain field of one word in a concurrent class when `unchecked(unguarded-field, "reason")` marks it. [Chapter 14]({{% relref "14-concurrent-classes" %}}) describes that form.

```anti
//! name: counters
import anti.io;
import anti.text;

export class Stats
{
	pub atomic hits: int = 0,
	pub atomic busy: bool = false,
	pub atomic small: i8 = 0,
	pub plain: int = 0,

	pub static atomic total: int = 0;
}

worker fn tally(chunk: []int) -> int
{
	for x in chunk {
		Stats.total.add(x);
	}
	return chunk.len;
}

export fn bump(s: *Stats) -> int
{
	return s.hits.add(1);
}

export fn read(s: *Stats) -> int
{
	return s.hits.load();
}

export fn write(s: *Stats, v: int)
{
	s.hits.store(v);
}

export fn claim(s: *Stats) -> bool
{
	return s.busy.compare_swap(false, true);
}

export fn mask(s: *Stats, m: int) -> int
{
	return s.hits.and(m);
}

fn main() -> int
{
	let s = alloc Stats { };
	let after = bump(s);
	let before_swap = s.hits.swap(40);
	let sub = s.hits.sub(8);
	let and = s.hits.and(48);
	let or = s.hits.or(3);
	let line = f"add {after}, swap {before_swap}, sub {sub}, and {and}, or {or}, load {read(s)}";
	io.println(line);
	free(line.ptr);

	let first = claim(s);
	let second = claim(s);
	let moved = s.hits.compare_swap(35, 100);
	let stale = s.hits.compare_swap(35, 200);
	line = f"claims {first} {second}, compare_swap {moved} {stale}, hits {s.hits.load()}";
	io.println(line);
	free(line.ptr);

	s.small.store(127);
	let old = s.small.add(1);
	line = f"an i8 at 127: add gives {old} and leaves {s.small.load()}";
	io.println(line);
	free(line.ptr);

	let numbers: [8]int = [1, 2, 3, 4, 5, 6, 7, 8];
	let lens = parallel numbers[0..8] by 4 -> tally;
	free(lens.ptr);
	let seen: atomic int = 0;
	seen.add(Stats.total.load());
	line = f"total {seen.load()}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Stats {size_of(Stats)}";
	io.println(line);
	free(line.ptr);
	free(s);
	return 0;
}
```

```text output
add 0, swap 1, sub 40, and 32, or 32, load 35
claims true false, compare_swap true false, hits 100
an i8 at 127: add gives 127 and leaves -128
total 36
sizes: Stats 32
```

Each of `add`, `swap`, `sub`, `and` and `or` gives the value before it. The four workers of `tally` add to one static field from the threads of the pool, and the total is 36 on every run.

The checker refuses each of the following.

```anti refused
//! error: `n` is atomic, so it is read with `load()` and written with `store(v)`
fn run() -> int
{
	let n: atomic int = 0;
	return n;
}
```

```anti refused
//! error: `n` is atomic, so it is read with `load()` and written with `store(v)`
fn run()
{
	let n: atomic int = 0;
	n += 1;
}
```

```anti refused
//! error: `n` is atomic, so it is read with `load()` and written with `store(v)`
fn run() -> *int
{
	let n: atomic int = 0;
	return &n;
}
```

```anti refused
//! error: `hits` is atomic, so it is read with `load()` and written with `store(v)`
class Stats { pub atomic hits: int = 0, }

fn next(s: *Stats) -> int
{
	return s.hits + 1;
}
```

```anti refused
//! error: an atomic local holds an integer, a `bool`, a `char` or a pointer, found `f64`
fn run()
{
	let n: atomic f64 = 0.0;
}
```

```anti refused
//! error: an atomic field holds an integer, a `bool`, a `char` or a pointer, found `f64`
class Gauge { pub atomic level: f64 = 0.0, }
```

```anti refused
//! error: `add` takes an atomic integer, and `on` holds `bool`
class Flags { pub atomic on: bool = false, }

fn flip(f: *Flags) -> bool
{
	return f.on.add(true);
}
```

```anti refused
//! error: `add` takes an atomic integer, and `last` holds `?*int`
class Slot { pub atomic last: ?*int = none, }

fn bump(s: *Slot, p: *int)
{
	s.last.add(p);
}
```

```anti refused
//! error: `f16` has no arithmetic, convert with `as f32`
class Gauge { pub atomic level: f16 = 0.0 as f16, }

fn raise(g: *Gauge)
{
	g.level.add(1.0 as f16);
}
```

```anti refused
//! error: an atomic local has no `fetch`
fn run() -> int
{
	let n: atomic int = 0;
	return n.fetch();
}
```

```anti refused
//! error: `add` takes 1 argument
fn run() -> int
{
	let n: atomic int = 0;
	return n.add();
}
```

```anti refused
//! error: `compare_swap` takes an atomic field, and `n` is a plain one
class Cell { pub n: int = 0, }

fn take(c: *Cell) -> bool
{
	return c.n.compare_swap(0, 1);
}
```

```anti refused
//! error: `total` is `static` and must be `atomic`
class Stats
{
	static total: int = 0;
}
```

### How antic builds it

An atomic operation is a call of the runtime in antic's own IR, `anti_rt_atomic_add` and its seven siblings, with the address, the width and the value. The emitter writes each as the atomic instruction of LLVM. The module below holds four functions of the example after opt.

```llvm excerpt=counters target=linux-x86_64 mode=release
  %v1 = getelementptr inbounds nuw i8, ptr %p0, i64 8
  %v3 = atomicrmw add ptr %v1, i64 1 seq_cst, align 8
  ret i64 %v3
...
  %v3 = load atomic i64, ptr %v1 seq_cst, align 8
...
  store atomic i64 %p1, ptr %v1 seq_cst, align 8
...
  %v1 = getelementptr inbounds nuw i8, ptr %p0, i64 16
  %v5 = cmpxchg ptr %v1, i8 0, i8 1 seq_cst seq_cst, align 1
  %v6 = extractvalue { i8, i1 } %v5, 1
  %v7 = zext i1 %v6 to i8
  ret i8 %v7
```

- `hits` is at offset 8 of a `Stats`, after the table pointer. `atomicrmw add` adds 1 and gives the value before, which `bump` returns.
- `load atomic` and `store atomic` are `read` and `write`.
- `busy` is one byte at offset 16. `cmpxchg` compares it with 0 and writes 1 on a match. It gives the value it found and a flag, and `claim` returns the flag as a byte.
- `seq_cst` on every line is the sequentially consistent order.

x86-64 has one instruction for each.

```asm excerpt=counters target=linux-x86_64 mode=release
<bump>:
movl $0x1, %eax
lock
xaddq %rax, 0x8(%rdi)
retq
...
<read>:
movq 0x8(%rdi), %rax
retq
...
<write>:
xchgq %rsi, 0x8(%rdi)
retq
...
<claim>:
movb $0x1, %cl
xorl %eax, %eax
lock
cmpxchgb %cl, 0x10(%rdi)
sete %al
```

- `lock xaddq` adds and leaves the old value in `%rax`.
- A load is a plain `movq`, which is sequentially consistent on x86-64.
- A store is `xchgq`, which locks the bus without the prefix and is a full barrier.
- `lock cmpxchgb` compares with `%al` and sets the zero flag, which `sete` copies.

x86-64 has no instruction that gives the value before an `and` or an `or`. `mask` returns it, so its `and` is a loop of `lock cmpxchgq`.

```asm excerpt=counters target=linux-x86_64 mode=release
<mask>:
movq 0x8(%rdi), %rax
...
movq %rax, %rcx
andq %rsi, %rcx
lock
cmpxchgq %rcx, 0x8(%rdi)
jne 0x10 <mask+0x10>
retq
```

- The `nopw` left out aligns the head of the loop. The loop computes the new value in `%rcx` from the value in `%rax` and stores it when the word still holds `%rax`.
- A failed `cmpxchgq` loads what it found into `%rax`, and `jne` goes round again.
- An `and` or an `or` whose value the program drops is one `lock andq` or `lock orq`, since it needs no value back.

linux-arm64 builds at the level `armv8.0`, which has no atomic add in one instruction. Each operation that reads and writes is a loop there.

```asm excerpt=counters target=linux-arm64 mode=release
<bump>:
add x8, x0, #0x8
ldaxr x0, [x8]
add x9, x0, #0x1
stlxr w10, x9, [x8]
cbnz w10, 0x4 <bump+0x4>
ret
...
<read>:
add x8, x0, #0x8
ldar x0, [x8]
ret
...
<write>:
add x8, x0, #0x8
stlr x1, [x8]
ret
```

- `ldaxr` loads the value and marks the address. `stlxr` stores the sum when no other thread wrote in between and sets `w10` to zero. `cbnz` tries again otherwise.
- `ldar` and `stlr` are the load and the store with the barrier.

macos-arm64 builds at `armv8.5` and windows-arm64 at `armv8.2`. Both levels have the instructions of LSE, so the loop is one instruction.

```asm excerpt=counters target=macos-arm64 mode=release
<_bump>:
add x8, x0, #0x8
mov w9, #0x1
ldaddal x9, x0, [x8]
ret
...
<_claim>:
mov w8, #0x0
add x9, x0, #0x10
mov w10, #0x1
casalb w8, w10, [x9]
cmp w8, #0x0
cset w0, eq
ret
```

- `ldaddal` adds `x9` to the value at `[x8]` and leaves the value before in `x0`.
- `casalb` compares the byte with `w8` and writes `w10` on a match. `w8` then holds what it found, and `cmp` with `cset` makes the flag.

[Chapter 20]({{% relref "20-targets" %}}) describes the levels and `--cpu`, which chooses one.

The runtime keeps `anti_rt_atomic_load` and its siblings as C functions for its own code. No program calls them for an atomic of 1, 2, 4 or 8 bytes.

Where it lives: `sema_atomic_call` of `src/antic/sema_thread.c`, the case `EXPR_ATOMIC` of `src/antic/lower_expr.c`, `atomic_call` of `src/antic/llvm_emit_call.c`, and `src/rt/atomic.c`.

### What it costs

The costs are the same in dev and in release.

| Cost | Count |
|---|---|
| Bytes of an atomic | the bytes of its type. `Stats` is 32: the table pointer, `hits`, `busy` and `small` in one word, and `plain` |
| An operation on x86-64 | one instruction, with the `lock` prefix where it reads and writes. `and` and `or` whose value the program uses: a loop of a load, the operation, a `lock cmpxchg` and a branch |
| An operation on arm64 at `armv8.2` and above | one instruction, after a `neg` for `sub` and an `mvn` for `and` |
| `add`, `sub`, `and`, `or`, `swap` and `compare_swap` on arm64 at `armv8.0` | a loop of a load, the operation, a store and a branch |
| Calls of the runtime | 0 |
| Allocations and locks | 0 |
| Checks | 0, in both modes |

A dev build adds its overflow check to a plain `+` and none to `add` of an atomic.

An atomic is the cheapest shared state for one word that each operation changes alone, such as a counter, a flag or a pointer published once. Two words that must change together need one lock around both: a `Mutex` field with a `sync` of the next section costs two calls of the runtime, and a free lock on Linux one compare-and-swap and one exchange inside them. A run of `load` and `store` is two operations, and another thread may run between them. A `compare_swap` loop in a `concurrent class` of [chapter 14]({{% relref "14-concurrent-classes" %}}) is the form for a change that reads the value first.

### Pitfalls

For a Java programmer:

- `add` gives the value before, as `getAndAdd` does. No form gives the value after.
- An atomic is a marked variable in place. It is no object, and `size_of` counts the bytes of its type alone.

For a C programmer:

- `n++` and `n = 5` on an atomic are refused. Every access is written as a call.
- A static field without `atomic` does not parse, so a program has no plain global to race on.

For a C++ programmer:

- No operation takes a memory order. A counter that would take `memory_order_relaxed` pays for a full barrier on every `add`.
- `compare_swap` gives a `bool` and does not hand back the value it found. A retry loop calls `load` again.
- No atomic holds an `f64` or an `f32`. A float that threads share goes under a `Mutex`, or into an atomic integer as a scaled value.

## Locks

### What you know

Java locks an object with `synchronized` or holds a `ReentrantLock` and unlocks in `finally`. C++ has `std::mutex` with `std::lock_guard`, which unlocks in its destructor. C has `pthread_mutex_t` with a lock and an unlock call, and the programmer pairs them on every path.

### The feature

A `Mutex` is a lock, and `sync` holds one for a block.

| Form | What it does |
|---|---|
| `Mutex.new()` | gives an unlocked `Mutex` |
| `sync m { }` | takes `m`, runs the block and gives `m` back on every exit of the block |
| `sync p { }` | the same through a pointer `p` to a `Mutex` |
| `m.destroy()` | ends the use of the `Mutex` |

`Mutex` is a type of `anti.lang` that the compiler declares. It stands in a local, a field of a struct or a class, and behind a pointer.

A `sync` block gives its lock back on every way out: the end of the block, `return`, `break`, `continue` and `fail`. No form takes a lock without a block, and none gives one back by hand.

A `Mutex` cannot be copied. `let k = m;`, a `Mutex` argument by value and an assignment of an object that holds one are refused. A function shares one by its address, and a worker takes a `*Mutex`.

A `Mutex` is not taken twice by one thread. A `sync` inside a `sync` on the same place in one function is refused, and so is one in an anonymous function inside the block. The checker calls two operands the same place when they name the same variable or the same path of fields, with `&` and `*` taken off. It does not follow a pointer that was copied into another local. A dev build stops such a `sync` at run time, with the lines of both.

`sync a, b { }` holds two locks, and takes two synchronized objects of [chapter 14]({{% relref "14-concurrent-classes" %}}). Two `Mutex` values are refused there.

Memory that holds zero bytes is an unlocked `Mutex`, so a field of an object from `alloc T { }` with no value is ready to use.

The checker does not tie a field to its `Mutex` in a plain class. The `guarded by` of a concurrent class in [chapter 14]({{% relref "14-concurrent-classes" %}}) is that check.

```anti
//! name: locks
import anti.io;
import anti.text;

class Ledger
{
	pub lock: Mutex,
	pub total: int = 0,
	pub refused: int = 0,
}

class Teller
{
	pub ledger: *Ledger,
}

worker fn deposit(t: *Teller, amount: int, times: int) -> int
{
	let done = 0;
	for 0..times {
		sync t.ledger.lock {
			if t.ledger.total + amount > 30000 {
				t.ledger.refused += 1;
				continue;
			}
			t.ledger.total += amount;
		}
		done += 1;
	}
	return done;
}

worker fn count(t: *Teller, lock: *Mutex, times: int)
{
	for 0..times {
		sync lock {
			t.ledger.refused += 1;
		}
	}
}

export fn mutex_bytes() -> int
{
	return size_of(Mutex);
}

fn main() -> int
{
	let ledger = alloc Ledger { lock: Mutex.new() };
	let t0 = alloc Teller { ledger: ledger };
	let t1 = alloc Teller { ledger: ledger };
	let t2 = alloc Teller { ledger: ledger };
	let jobs = [dispatch t0 -> deposit(1, 20000), dispatch t1 -> deposit(1, 20000)];
	let extra = dispatch t2 -> count(&ledger.lock, 1000);
	let first = join(jobs[0]);
	let second = join(jobs[1]);
	join(extra);
	let attempts = first + second + ledger.refused - 1000;
	let line = f"total {ledger.total}, attempts {attempts}, refused {ledger.refused - 1000}";
	io.println(line);
	free(line.ptr);

	let local = Mutex.new();
	let n = 0;
	sync local {
		n += 1;
	}
	sync &local {
		n += 1;
	}
	line = f"n {n}, a Mutex is at most one word: {size_of(Mutex) <= size_of(*int)}";
	io.println(line);
	free(line.ptr);
	local.destroy();
	ledger.lock.destroy();
	free(t0);
	free(t1);
	free(t2);
	free(ledger);
	return 0;
}
```

```text output
total 30000, attempts 40000, refused 10000
n 2, a Mutex is at most one word: true
```

Three workers change one `Ledger` under its lock. The `continue` inside the `sync` of `deposit` leaves the block with the lock given back, or the second turn would wait for ever. The three tellers are three objects, so all three are in flight at once, and each holds a pointer to the one ledger. The checker does not see that pointer. The lock is what keeps the totals right.

The checker refuses each of the following.

```anti refused
//! error: `sync m` inside `sync m` deadlocks
fn run()
{
	let m = Mutex.new();
	sync m {
		sync m { }
	}
}
```

```anti refused
//! error: `sync &l.lock` inside `sync l.lock` deadlocks
class Ledger { pub lock: Mutex, pub n: int = 0, }

fn run(l: *Ledger)
{
	sync l.lock {
		sync &l.lock {
			l.n += 1;
		}
	}
}
```

```anti refused
//! error: a `Mutex` cannot be copied, pass a pointer to it
fn run()
{
	let m = Mutex.new();
	let k = m;
}
```

```anti refused
//! error: a `Mutex` cannot be copied, pass a pointer to it
class Cell { pub n: int = 0, }

worker fn add(c: *Cell, m: Mutex) { c.n += 1; }

fn run(c: *Cell)
{
	let m = Mutex.new();
	let job = dispatch c -> add(m);
	join(job);
}
```

```anti refused
//! error: `Ledger` holds a `Mutex` and cannot be assigned
class Ledger { pub lock: Mutex, }

fn run(a: *Ledger, b: *Ledger)
{
	*a = *b;
}
```

```anti refused
//! error: `sync` takes a `Mutex`, a synchronized object or a pointer to either, found `int`
fn run()
{
	let n = 0;
	sync n {
		n += 1;
	}
}
```

```anti refused
//! error: `sync a, b` takes two synchronized objects or pointers to them, found `Mutex`
fn run()
{
	let m = Mutex.new();
	let n = Mutex.new();
	sync m, n { }
}
```

```anti refused
//! error: `Mutex` has no function `lock`
fn run()
{
	let m = Mutex.new();
	m.lock();
}
```

```anti refused
//! error: `==` is not defined on `Mutex`
fn run() -> bool
{
	let m = Mutex.new();
	let n = Mutex.new();
	return m == n;
}
```

```anti refused
//! error: the field `lock` of export struct `Shared` has type `Mutex`, which C cannot represent
export struct Shared { lock: Mutex }
```

### How antic builds it

A `Mutex` is one word of the program's own memory, and the system keeps nothing for it until a thread has to wait.

| System | The word | Bytes | Unlocked |
|---|---|---|---|
| Linux | a futex word | 4 | 0 |
| macOS | an `os_unfair_lock` | 4 | 0 |
| Windows | an `SRWLOCK` | 8 | 0 |

The output of a program that prints `size_of(Mutex)` differs by system, so the example prints a comparison. The constant itself stands in `mutex_bytes` of each target.

```llvm excerpt=locks target=linux-x86_64 mode=release
define internal noundef i64 @mutex_bytes()
b0:
  ret i64 4
```

```llvm excerpt=locks target=macos-arm64 mode=release
define dso_local noundef i64 @mutex_bytes()
b0:
  ret i64 4
```

```llvm excerpt=locks target=windows-x86_64 mode=release
define dso_local noundef i64 @mutex_bytes()
b0:
  ret i64 8
```

antic's IR has a type of its own for the word, so the IR holds no size. The back end lays the word out per target. `Mutex.new()` is a store of zero into it and calls nothing.

`sync` is a call of `anti_rt_mutex_lock` before the block and one of `anti_rt_mutex_unlock` at each exit. The module below is the thunk of `count` after opt, with the worker inlined.

```llvm excerpt=locks target=linux-x86_64 mode=release
  tail call void @anti_rt_mutex_lock(ptr nonnull dereferenceable(4) %v3)
  %v9.i = load ptr, ptr %v7.i, align 8
  %v11.i = getelementptr inbounds nuw i8, ptr %v9.i, i64 24
  %v13.i = load i64, ptr %v11.i, align 8
  %v15.i = add i64 %v13.i, 1
  store i64 %v15.i, ptr %v11.i, align 8
  tail call void @anti_rt_mutex_unlock(ptr nonnull dereferenceable(4) %v3)
```

- `%v3` is the `*Mutex` the worker received, the address of the word. `dereferenceable(4)` is its 4 bytes on Linux.
- The four lines between the calls are `t.ledger.refused += 1`: the load of `t.ledger`, the address of `refused` at offset 24, and the load, the add and the store.
- The unlock is the exit action of the block.

`deposit` has two exits of its block, the `continue` and the end, and holds one unlock at each.

```llvm excerpt=locks target=linux-x86_64 mode=release
define internal void @locks.dispatch.0(
...
  tail call void @anti_rt_mutex_lock(
...
  tail call void @anti_rt_mutex_unlock(
...
  tail call void @anti_rt_mutex_unlock(
...
define internal void @locks.dispatch.1(
```

A dev build calls `anti_rt_mutex_lock_at` and `anti_rt_mutex_unlock_at`. The first takes the place of the `sync` as a second argument, a text of the form `file:line`.

```llvm excerpt=locks target=linux-x86_64 mode=dev
define hidden void @locks.count(ptr noundef nonnull dereferenceable(16) %p0, ptr noundef nonnull dereferenceable(4) %p1, i64 noundef %p2) #0 {
...
  call void (ptr, ptr) @anti_rt_mutex_lock_at(ptr %v5, ptr %v6)
...
  call void (ptr) @anti_rt_mutex_unlock_at(ptr %v23)
```

The runtime records there the order in which each thread takes its locks. [Chapter 14]({{% relref "14-concurrent-classes" %}}) describes that record and its report.

The lock itself is in the platform layer. On Linux a free lock is taken by one compare-and-swap of the word from 0 to 1 and given back by one exchange to 0. The word is 2 while a thread waits, and only then does either side call `futex`. On macOS the two calls are `os_unfair_lock_lock` and `os_unfair_lock_unlock`, and on Windows `AcquireSRWLockExclusive` and `ReleaseSRWLockExclusive`.

`m.destroy()` is one call of `anti_rt_mutex_destroy`. The word holds nothing of the system, so the call frees nothing. It makes a dev build forget the orders it recorded for the address.

Where it lives: `sema_check_mutex_new` and `sema_check_mutex_destroy` of `src/antic/sema_thread.c`, the check of `sync` in `src/antic/sema_stmt.c`, `lower_sync` and `lower_hold_lock` of `src/antic/lower_sync.c`, `anti_rt_mutex_lock` of `src/rt/lock.c`, and `anti_rt_word_lock` of `src/rt/platform_posix.c` and `src/rt/platform_windows.c`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of a `Mutex` | 4 on Linux and macOS, 8 on Windows | the same |
| `Mutex.new()` | one store of zero | the same |
| Allocations | 0 | 0 |
| Entering a `sync` | one call of `anti_rt_mutex_lock_at` | one call of `anti_rt_mutex_lock` |
| Leaving a `sync` | one call of `anti_rt_mutex_unlock_at` per exit taken | one call of `anti_rt_mutex_unlock` |
| A free lock on Linux | one compare-and-swap, and one exchange to give it back | the same |
| A lock another thread holds | a wait in the system | the same |
| The record of the order | a push and a pop on a list of the thread, and a table under a lock when the thread holds another lock | 0 |
| `m.destroy()` | one call and one atomic load | the same |
| Checks | a search of the stack of the thread for the `Mutex` it takes, which stops the program when the thread holds it already, and the order of the locks | 0 |

The copies of code are the unlock calls: one per exit of the block in the source.

The record of the order belongs to dev mode. No flag of antic turns it on in a release build or off in a dev build.

A `Mutex` field leaves the pairing of the lock with its fields to the program, and the checker holds no field to it. Two forms of [chapter 14]({{% relref "14-concurrent-classes" %}}) put the pairing under the checker.

| Form | Bytes of the lock | Taken | What the checker holds |
|---|---|---|---|
| A `Mutex` field and `sync` | 4, or 8 on Windows | by each `sync` the program writes | the `sync` of one place inside another, the copies, the order of `lock-order` |
| `synchronized class` | 24, hidden | at the entry of every function that is not private, again by its own thread | every field reached from the functions of the class alone |
| `concurrent class` with `guarded by` | 4, or 8 on Windows, per `Mutex` it names | by each `sync` the program writes | every read and write of a guarded field inside a `sync` on its lock |

The `Mutex` field is the smallest and the least checked. A class whose every function changes its state is a synchronized class. A class with more than one lock, atomic counters and fields fixed at `construct` is a concurrent class.

### Pitfalls

For a Java programmer:

- A `Mutex` is not taken again by the thread that holds it. A function that calls another function with a `sync` on the same `Mutex` waits for itself in a release build, and a dev build stops it.
- No lock belongs to every object. A plain class has one only as a field, and the hidden lock of [chapter 14]({{% relref "14-concurrent-classes" %}}) is the other form.

For a C programmer:

- No call locks or unlocks. A lock that must outlive a block is a design the language does not have.
- A zeroed word is a ready `Mutex`. No initializer and no `pthread_mutex_destroy` is owed, and `destroy` matters to a dev build alone.

For a C++ programmer:

- `sync m` through a copied pointer inside `sync m` compiles. The checker compares places as written and does not follow the pointer. A dev build stops the thread there, and in a release build it waits for itself.
- No `try_lock`, no timed lock and no shared lock exist. `Mutex` is exclusive and blocks.
- A `Mutex` field does not make its class thread-safe for the checker. A worker takes a pointer to the `Mutex` itself, or to a class declared `synchronized` or `concurrent`.

## Waits under a lock

### What you know

A Java thread that calls `take` of a `BlockingQueue` inside a `synchronized` block keeps the monitor while it waits. C and C++ have one wait built for a lock, `pthread_cond_wait` and `std::condition_variable`, which give the mutex back while they wait. A `read` of a pipe or a `join` of a thread under a mutex keeps it, and no compiler of the three says so.

### The feature

The warning `blocking-under-lock` names a wait where the code holds a lock. Five operations wait.

| Wait | Why it may block |
|---|---|
| `recv(c)` | the channel may be empty and open |
| `send(c, v)` | every channel has room for a bounded count of values, so any `send` may find it full |
| `select { }` | a `select` has no `else` and waits until one of its channels is ready |
| `join(job)` | the job may still run |
| `join_all(jobs)` | the same, for each job |

Code holds a lock in two places.

- Inside a `sync` block, on any operand: a `Mutex` field, a `Mutex` local or parameter, a synchronized object and `sync a, b`.
- In a function of a synchronized class that takes the hidden lock: a function with `self` that is not private, `construct` and `destruct` apart. [Chapter 14]({{% relref "14-concurrent-classes" %}}) describes the hidden lock.

A call of a function that reaches a wait gets the warning as well, where the call stands under the lock. The checker follows the calls a program names. It reads a function of the module, and the library file of a library records the first wait each of its functions reaches. It does not follow a call through a table. That is a call through an interface, a function value or a plugin. It is also a call through a pointer of a `pub` function that is not `final`, of a class that is not `final`. The function that runs there is not known. The body of an anonymous function and a worker hold no lock at their start, since they run later or on another thread.

The message names the innermost lock the code holds, in one of three forms.

| Where the code stands | Message |
|---|---|
| A wait inside a `sync` | `` `recv` may wait while the `sync` of line 10 holds its lock `` |
| A wait in a function of a synchronized class | `` `recv` may wait while `Queue.take` holds the lock of its object `` |
| A call of a function that waits | `` `pull` may wait in `recv` on line 23 while the `sync` of line 33 holds its lock `` |

For a call the message names the first wait the callee reaches in the order of its text, with the module of a library function. A warning prints in a dev build, which goes on, and refuses a release build, as every warning does. `--warnings-as-errors` refuses a dev build too. `allow(blocking-under-lock, "reason")` before the statement, or last in the header of the function, silences it.

The fix is the order of the code. The thread takes the value first, and holds the lock for the state it guards alone. Some waits cannot block for long, such as a `recv` on a channel that the function made, filled and closed. `allow` is the form there, and its reason says why.

```anti
//! name: waits
import anti.io;
import anti.text;

class Inbox
{
	pub lock: Mutex,
	pub total: int = 0,
	pub count: int = 0,
}

class Feeder
{
	pub first: int = 0,
}

worker fn feed(f: *Feeder, out: chan int) -> int
{
	for i in 0..4 {
		send(out, f.first + i);
	}
	close(out);
	return 4;
}

fn drain(box: *Inbox, c: chan int)
{
	while true do {
		let v = recv(c) else {
			break;
		};
		let n = *v;
		sync box.lock {
			box.total += n;
			box.count += 1;
		}
	}
}

fn replay(box: *Inbox, first: int) -> int
{
	let local = chan int(2);
	send(local, first);
	send(local, first + 1);
	close(local);
	sync box.lock {
		allow(blocking-under-lock, "the channel is local, closed and full")
		if let v = recv(local) {
			box.total += *v;
			box.count += 1;
		}
	}
	delete(local);
	return 1;
}

fn main() -> int
{
	let box = alloc Inbox { lock: Mutex.new() };
	let f = alloc Feeder { first: 10 };
	let c = chan int(2);
	let job = dispatch f -> feed(c);
	drain(box, c);
	let sent = join(job);
	let replayed = replay(box, 100);
	let line = f"sent {sent}, replayed {replayed}, count {box.count}, total {box.total}";
	io.println(line);
	free(line.ptr);
	delete(c);
	box.lock.destroy();
	free(box);
	free(f);
	return 0;
}
```

```text output
sent 4, replayed 1, count 5, total 146
```

`drain` waits in `recv` with no lock and takes the lock of the inbox for the two additions alone. `feed` sends on a channel of two values from the pool and waits while it is full, and `drain` empties it. `replay` waits under the lock on a channel that holds its two values and is closed, and its `allow` says so.

The checker gives the warning, and the release build refuses, in each of the following.

```anti refused
//! error: `recv` may wait while the `sync` of line 10 holds its lock [blocking-under-lock]
struct Inbox
{
	lock: Mutex,
	count: int,
}

fn take(box: *Inbox, c: chan int)
{
	sync box.lock {
		let v = recv(c) else {
			return;
		};
		box.count += *v;
	}
}

fn main() -> int
{
	let box = Inbox { count: 0 };
	let c = chan int(1);
	send(c, 4);
	take(&box, c);
	delete(c);
	return box.count - 4;
}
```

```anti refused
//! error: `recv` may wait while `Queue.take` holds the lock of its object [blocking-under-lock]
synchronized class Queue
{
	count: int = 0,

	pub fn take(self, c: chan int) -> int
	{
		let v = recv(c) else {
			return 0;
		};
		self.count += 1;
		return *v;
	}
}
```

```anti refused
//! error: `pull` may wait in `recv` on line
class Box { pub lock: Mutex, pub n: int = 0, }

fn pull(c: chan int) -> int
{
	let v = recv(c) else {
		return 0;
	};
	return *v;
}

fn under(b: *Box, c: chan int)
{
	sync b.lock {
		b.n += pull(c);
	}
}
```

```anti refused
//! error: `join` may wait while the `sync` of line
class Box { pub lock: Mutex, pub n: int = 0, }

class Cell { pub n: int = 0, }

worker fn work(c: *Cell) -> int { return c.n; }

fn under(b: *Box, cell: *Cell)
{
	sync b.lock {
		let job = dispatch cell -> work;
		b.n += join(job);
	}
}
```

A `parallel` waits for its chunks and is none of the five, so it gets no warning. Neither does a call through a table that reaches a wait.

### How antic builds it

The check runs in the compiler and adds nothing to the program. Once the checker has checked every function of the module, a pass reads each body once. It keeps the waits and the calls of each function in the order of the text, with the locks the code holds at each. It then marks every function that reaches a wait, itself or through a call. A walk of the calls of each in that order finds its first wait. Each wait or call that stands under a lock gets the warning.

The library file of a module carries the first wait of each of its functions that waits: the kind, the line and the module. A module that imports the library warns at a call of such a function without its body.

The code of `replay` shows that `allow` changes no code. The `recv` stands between the lock and the unlock, as written.

```llvm excerpt=waits target=linux-x86_64 mode=dev
define hidden noundef i64 @waits.replay(ptr noundef nonnull dereferenceable(32) %p0, i64 noundef %p1) #0 {
...
  call void (ptr) @anti_rt_chan_close(ptr %v25)
...
  call void (ptr, ptr) @anti_rt_mutex_lock_at(ptr %v28, ptr %v29)
...
  %v34 = call ptr (ptr, ptr) @anti_rt_chan_recv(ptr %v32, ptr %v33)
...
  call void (ptr) @anti_rt_mutex_unlock_at(ptr %v68)
...
  call void (ptr) @anti_rt_chan_delete(ptr %v71)
```

- `anti_rt_chan_close` ends the channel before the lock is taken.
- `anti_rt_mutex_lock_at` takes `box.lock`, and `anti_rt_chan_recv` runs while it is held. The channel holds a value, so the call returns at once.
- `anti_rt_mutex_unlock_at` gives the lock back, and the channel is deleted after it.

Where it lives: `read_function`, `find_waits`, `first_wait`, `warn_waits` and `warn_wait` of `src/antic/sema_lock.c`, and `keep_facts` there for the record of the library file.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Code, checks and calls at run time | 0 | 0 |
| A wait under a lock that the checker sees | a warning, and the build goes on | the build is refused |
| `allow(blocking-under-lock, "reason")` | no code, and the warning is gone | the same |
| A library file | one record per function that waits | the same |

`allow` gives up the warning for its statement or function and nothing else. A wait that `allow` covers holds the lock for as long as it waits, and every thread that needs the lock waits with it.

Three forms keep the wait out of the lock.

- The wait first: `recv` into a local, then `sync` for the state the value changes, as `drain` does.
- The state out of the lock: copy what the `send` needs inside the `sync`, and `send` after the block.
- No wait: `push` of an `SpscRing` of [chapter 14]({{% relref "14-concurrent-classes" %}}) gives `false` when the ring is full and never waits, for one producing thread and one consuming thread.

### Pitfalls

For a Java programmer:

- No `wait` and no `notify` give the lock back during a wait. A thread that waits for a state waits on a channel, outside the lock.
- The warning is a compile-time reading of the code. It names a wait that may block, and a wait that never blocks in a run gets it as well.

For a C programmer:

- A `send` on a channel with room is a wait for the checker. The capacity of a channel is a value of the run, and the checker proves no room.
- A call through a function value is not followed. A callback that waits, called under a lock, gets no warning.

For a C++ programmer:

- The hidden lock of a synchronized class is held in every function that is not private, so a `join` or a `recv` there gets the warning. A private function of the class called from one does as well.
- The warning stops a release build. A program that ships with a wait under a lock writes `allow` with its reason.
