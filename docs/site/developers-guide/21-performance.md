---
title: "Performance"
description: "One table of the cost of every feature of the guide, measuring with tests/bench, --profile-generate and --profile-use, tracing, and the checks a release keeps."
summary: "One table with the cost of every feature of the guide and a link to its section, measuring with `tests/bench`, `--profile-generate` and `--profile-use`, tracing, and which checks a release build keeps and what turning them off gives up."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T20:23:44+02:00
draft: true
weight: 210
tags: [compilers, programming-languages]
keywords: [performance, benchmarks, profile-guided optimization, tracing, checks, release build]
---

The cost of a feature is a set of counts. They are the bytes of a value, the heap allocations and the calls into the runtime. The atomic operations, the locks and the checks a build inserts complete them. The first table of this page holds those counts for every feature section of the guide. Each row links to the section that shows them. A time stands on this page only where `tests/runners/run_bench.cmake` measured it beside a C twin. Its table names the machine, the commit and the command.

## The cost of every feature

A row gives the counts of a release build unless it names dev mode. "0" in a column means the feature adds none of its own, and the operations inside a body keep their own costs. The names with `anti_rt_` are functions of the runtime. A check "in both modes" is in every build and has no option.

| Feature | Bytes | Heap allocations | Calls into the runtime | Atomic operations and locks | Checks | Copies of code |
|---|---|---|---|---|---|---|
| [Types and literals]({{% relref "03-values#types-and-literals" %}}) | `bool` 1, `i16` 2, `i32` 4, `int` 8, `char` 4, a pointer 8 | 0 | 0 | 0 | dev: the range of a narrowing `as` | none |
| [Variables and constants]({{% relref "03-values#variables-and-constants" %}}) | a local: one stack slot in dev, a register in release. A scalar constant: 0 | 0 | 0 | 0 | none | none |
| [Integers]({{% relref "03-values#integers" %}}) | 1 to 8 | 0 | `anti_rt_check_failed`, on a failed check alone | 0 | dev: overflow of signed `+ - *`, a zero divisor, a shift count, a narrowing `as` | none |
| [Floats and f16]({{% relref "03-values#floats-and-f16" %}}) | `f16` 2, `f32` 4, `f64` 8 | 0 | one per read or write of an `f16` at `v1` and `v2`, 0 elsewhere | 0 | none | none |
| [Text and bytes]({{% relref "03-values#text-and-bytes" %}}) | a `str` 16, a `char` 4 | an `f"..."`: one block of at least 32 bytes | `anti_rt_same_bytes` per `==`, one call per part of an `f"..."` | 0 | dev: the bounds of `s[i]` | none |
| [Tuples]({{% relref "03-values#tuples" %}}) | the C struct of the elements, padding included | 0 | 0 | 0 | none | none |
| [Type inference]({{% relref "03-values#type-inference" %}}) | as with the type written | 0 | 0 | 0 | none | none |
| [if and while]({{% relref "04-flow#if-and-while" %}}) | 0 | 0 | 0 | 0 | none | none |
| [for over ranges and collections]({{% relref "04-flow#for-over-ranges-and-collections" %}}) | the iterator in the frame, 64 for a `List<int>` | 0 | 0 | 0 | dev: the change check of a collection loop | none |
| [switch]({{% relref "04-flow#switch" %}}) | 0 | 0 | 0. A `str` `switch` of n arms makes up to n calls of `anti.text.equal` | 0 | none | none |
| [break and continue]({{% relref "04-flow#break-and-continue" %}}) | 0 | 0 | 0 | 0 | none | one copy of each deferred statement per exit |
| [defer and undo]({{% relref "04-flow#defer-and-undo" %}}) | 0 in the frame | 0 | 0 | 0 | none | a `defer` once per exit of its block, an `undo` once per failing exit |
| [Declaration and parameters]({{% relref "05-functions#declaration-and-parameters" %}}) | a struct of up to 16 bytes in two registers, a larger one as a copy | 0 | 0 | 0 | none | none |
| [Defaults and source locations]({{% relref "05-functions#defaults-and-source-locations" %}}) | 64 of read-only data per `here` | 0 | 0 | 0 | none | none |
| [The calling convention]({{% relref "05-functions#the-calling-convention" %}}) | one more pointer argument for a function that may fail | 0 | 0 | 0 | none | none |
| [Inlining]({{% relref "05-functions#inlining" %}}) | 0 | 0 | a runtime call is inlined where lld chooses, under full LTO | 0 | none | one body per call site of an inlined function |
| [Anonymous functions and closures]({{% relref "05-functions#anonymous-functions-and-closures" %}}) | in the frame: 16 for the pair and 8 per captured variable | 0 | 0 | 0 | none | none |
| [snapshot fn and own fn]({{% relref "05-functions#snapshot-fn-and-own-fn" %}}) | an `own fn` value 16. On the heap 8 and the captures | one `malloc` per `own fn` made and per `dup` | `anti_rt_snapshot_new`, `anti_rt_snapshot_free`, `anti_rt_snapshot_text` per captured `str` | one atomic add when made, one atomic subtract when freed | none | none |
| [Function values]({{% relref "05-functions#function-values" %}}) | 8 in a field or a result, 16 at a parameter or in a local | 0 | 0 | 0 | none | none |
| [Failing functions]({{% relref "06-errors#failing-functions" %}}) | an error: 128 on the heap | 0 for a call that does not fail, one `malloc` per new error | per new error `anti_rt_hook` and `anti_rt_backtrace_on`, and `anti_rt_delete` when it is handled | per new error two atomic loads in `anti_rt_hook` and one in `anti_rt_backtrace_on`. No lock | none | none |
| [try and handlers]({{% relref "06-errors#try-and-handlers" %}}) | 0 | 0 | `anti_rt_delete` when a handler runs | 0 | none | the `defer` statements of the blocks left, once per `try` |
| [Error origins and stack traces]({{% relref "06-errors#error-origins-and-stack-traces" %}}) | in the error 64 for the origin and 8 for the trace. 64 of read-only data per `fail` | a capture: 2, of 40 bytes and of 48 per frame. Dev, and release with `--anti.backtrace` | `anti_rt_trace_walk`, and `anti_rt_trace_frame` per frame | one atomic load at the first `fail` of an error | none | none |
| [never]({{% relref "05-functions#never" %}}) | 0 | 0 | 0 | 0 | none | none |
| [assert and the dev checks]({{% relref "06-errors#assert-and-the-dev-checks" %}}) | one text in the read-only data per assertion and per check site, in a build that keeps them | 0 | one call that does not return, on a failure alone | 0 | dev: all. Release: none, and `--checks` and `--asserts` keep them | none |
| [Nullable pointers]({{% relref "07-optionals#nullable-pointers" %}}) | `*T` 8, `?*T` 8 | 0 | 0 | 0 | none. The tests are the ones the program writes | none |
| [Optional values]({{% relref "07-optionals#optional-values" %}}) | the size of `T` and one byte, rounded up to the alignment of `T`. A `?T` of a pointer: 8 | 0 | 0 | 0 | none | none |
| [Reading an optional]({{% relref "07-optionals#reading-an-optional" %}}) | one copy of the value for `if let` and `let ... else` | 0 when the value is there. One error of at least 128 bytes on the `none` path of a `catch` | `anti_rt_delete` of that error | 0 | none | none |
| [Structs]({{% relref "08-structs-enums#structs" %}}) | the fields and the padding of C, and 0 beside them. Read-only: a descriptor of 136 and 48 per field | 0 | 0 | 0 | none of its own | none |
| [packed and align]({{% relref "08-structs-enums#packed-and-align" %}}) | `Tight` 14 where `Loose` is 24. 64 for 8 bytes of fields at `align(64)` | 0 | 0 | 0 | none | none |
| [Unions and bitfields]({{% relref "08-structs-enums#unions-and-bitfields" %}}) | a union: its largest field. Bitfields: the bits, in units of the field types | 0 | 0 | 0. A write of a bitfield is not atomic | none. A write truncates | none |
| [Simd structs]({{% relref "08-structs-enums#simd-structs" %}}) | the lanes, no padding: `Vec4` 16, `F32x8` 32 | 0 | 0 | 0 | dev: the bounds of `load` and `store`, and the checks of `/` and the shifts | none |
| [Enums]({{% relref "08-structs-enums#enums" %}}) | its underlying type: 1, 2 or 4 | 0 | 0 on the passing path | 0 | dev: the test of `as` to an enum | none |
| [Variants]({{% relref "08-structs-enums#variants" %}}) | the tag, the padding and the largest case: `Shape` 12, `Token` 24 | 0 | 0 | 0 | none | none |
| [Pattern matching]({{% relref "08-structs-enums#pattern-matching" %}}) | dev: one frame slot per binding arm, as large as its case | 0 for a case that owns nothing | 0 for a case that owns nothing | 0 | none. Coverage is a rule of the checker | none |
| [Classes and the object in memory]({{% relref "09-classes#classes-and-the-object-in-memory" %}}) | 8 for the table pointer, then the fields. Per class a table of 8 per entry and a descriptor | 0 for a class value, one `malloc` per `alloc T { }` | `anti_rt_hook` per object made, `anti_rt_delete` per `delete` | two atomic loads in each `anti_rt_hook`. No lock | dev: the zero table before `is`, `as` and a dispatch. The check of `as` stays in release | none |
| [construct and destruct]({{% relref "09-classes#construct-and-destruct" %}}) | 0 | one `malloc` per `alloc T(args)`, one `free` per `delete` | `anti_rt_delete`, `anti_rt_hook`, and `anti_rt_delete_from` per `own` pointer field | two atomic loads in each `anti_rt_hook`. No lock | both modes: `anti_rt_delete` ends the program on a zero table | none |
| [Fields and methods]({{% relref "09-classes#fields-and-methods" %}}) | 8 per `pub` function in the table of every class of the chain below | 0 | 0 | 0 | dev: the zero table before a dispatch | none |
| [Interfaces and dispatch]({{% relref "09-classes#interfaces-and-dispatch" %}}) | per object 8 per implemented interface, and its fields. Per class one table and one descriptor per interface | 0 | 0 | 0 | dev: the zero table before the dispatch | one thunk per function entry that is not the interface's own body |
| [Static fields and singletons]({{% relref "09-classes#static-fields-and-singletons" %}}) | its type once per program, 0 per object | one `malloc` at the first `get()` of a singleton | `anti_rt_hook` at that first `get()` | every access is one atomic operation. No lock | none | none |
| [Operators]({{% relref "09-classes#operators" %}}) | 0 | 0 | `anti_rt_hook` for a class value as the result | two atomic loads in that `anti_rt_hook` | those of the arithmetic inside the function | none |
| [Reflection]({{% relref "09-classes#reflection" %}}) | read-only per class: a descriptor of 136, 48 per field, 40 per `pub` function. 0 per object | one `malloc` per `new` | one to five per `describe`, `field` and `get`, one per `set` and `call` | 0 | the tests of count and kinds in `call` | one trampoline per distinct signature |
| [Generic functions and classes]({{% relref "10-generics#generic-functions-and-classes" %}}) | the layout of its arguments: `Pair<u8, bool>` 2, `Pair<int, str>` 24 | 0 beyond the body | 0 beyond the body | 0 | none | one function per distinct set of arguments the program uses |
| [Constraints]({{% relref "10-generics#constraints" %}}) | 0 | 0 | 0 | 0 | none | none |
| [Constant parameters]({{% relref "10-generics#constant-parameters" %}}) | the layout with `N` in place: `Row<3>` 40, `Row<4>` 48 | 0 | 0 | 0 | dev: the bounds of an index into `[N]T` | one copy per distinct value of `N` |
| [Copies per use]({{% relref "10-generics#copies-per-use" %}}) | per copy of a class one table, one descriptor and its lists | 0 | 0 | 0 | none | one per set of arguments. A release build merges identical copies: 22 functions in dev, 9 in release, in the example |
| [Generic declaration forms]({{% relref "10-generics#generic-declaration-forms" %}}) | no table entry for a function with type parameters of its own | 0 | 0 | 0 | none | one copy per set of class arguments and function arguments used |
| [Generics of library files]({{% relref "10-generics#generics-of-library-files" %}}) | the library file holds the trees of its generics: `shelf.antl` 2871 bytes | 0 | 0 | 0 | none | dev: one copy per module that uses it, kept once by the link. Release: one per program |
| [own and lent]({{% relref "11-ownership#own-and-lent" %}}) | `own` and `lent` add 0. An `own` pointer field 8, an `own` slice field 16 | 0 | per teardown `anti_rt_delete_from` or `anti_rt_give`, with one `free` | two atomic loads in the `anti_rt_hook` of each object torn down | both modes: the zero table in `anti_rt_delete_from` | none |
| [dup and moves]({{% relref "11-ownership#dup-and-moves" %}}) | a move of a local copies the value once, 24 in the example | a move: 0. `dup` of a `Box`: 1. `dup` of a `*Box`: 2 | `anti_rt_copy_buffer` and `anti_rt_hook_copied` per `dup`, and `anti_rt_dup` for a pointer | one atomic load in each `anti_rt_hook_copied`. 0 for a move | both modes: the zero table tests of `dup`, and the test before the teardown of a local that moves | none |
| [Teardown and temporaries]({{% relref "11-ownership#teardown-and-temporaries" %}}) | 8 in the frame per statement temporary. No word for the teardown in a value | 0 | `anti_rt_hook` inside `<Class>.destroy`, `anti_rt_delete` per `delete` | two atomic loads in each `anti_rt_hook` | both modes: the zero table | one teardown sequence per exit of the block, per local that owns something |
| [The rules at a call]({{% relref "11-ownership#the-rules-at-a-call" %}}) | one copy of the value for a parameter by value and for `own` of a value. One register for a pointer | 0 | 0 at the call | 0 | none | none |
| [References to locals and uses after the end]({{% relref "11-ownership#references-to-locals-and-uses-after-the-end" %}}) | 0 | 0 | 0 | 0 | none at run time: `escaping-local` and `use-after-free` refuse at compile time, in every mode | none |
| [Shared objects]({{% relref "11-ownership#shared-objects" %}}) | a handle 16. The cell on the heap: 32 for a value of 16 | one `malloc` per `new` | two `anti_rt_hook` per `new`, `anti_rt_delete` at the last teardown | one atomic add per `share()` and `dup`, one atomic subtract per teardown, and the atomic loads inside the hook calls. No lock | none | one copy of `Shared<T>` and `Cell<T>` per type argument |
| [Pointers]({{% relref "12-memory#pointers" %}}) | 8 | one `malloc` or `calloc` per `alloc`, one `free` per `free` | 0. The allocator is that of the C library, mimalloc on Linux with musl | 0 | none. `p[i]` has no check in either mode | none |
| [Slices and views]({{% relref "12-memory#slices-and-views" %}}) | a `[]T` 16, a `[N]T` `N` times the size of `T` | 0 | 0 | 0 | none | none |
| [Bounds checks]({{% relref "12-memory#bounds-checks" %}}) | the text of each check a build keeps, 34 bytes in the example | 0 | on a failure alone | 0 | dev: one compare and one branch per `s[i]` and `a[i]`. Release: none, and `--checks` keeps them | none |
| [anti.mem and the allocator]({{% relref "12-memory#antimem-and-the-allocator" %}}) | a `*Allocator` 8, an `ArenaAllocator` 56, and 8 per block it holds | an `ArenaAllocator`: one block of `from` when an `alloc` does not fit | 0. `from.alloc` is two loads and one indirect call. An `alloc` of an `ArenaAllocator` that fits makes no call in release and two in dev | 0. An `ArenaAllocator` is not thread-safe | dev: the zero table before `from.alloc` | none |
| [--memory-checks]({{% relref "12-memory#--memory-checks" %}}) | 0 in the values | `alloc` and `free` of the AddressSanitizer runtime | `__asan_loadN` or `__asan_storeN` before every load and store through a pointer | 0 | with the option, in both modes. A release build with it links without LTO unless `--lto` is given | none |
| [Threads, dispatch and join]({{% relref "13-threads#threads-dispatch-and-join" %}}) | a job 8 | per `dispatch` one `calloc` of 56, one `malloc` of the context when the worker takes arguments beside the object, and one `malloc` when the worker returns a value | one per `dispatch` and one of `anti_rt_hook`, one per `join`, which calls the hook `joined` | the lock of the pool once per `dispatch` and per `join`. One atomic load in the hook call of each, and none with `--no-hooks` | both modes: the test for an object in flight | one thunk per `dispatch` in the source |
| [parallel]({{% relref "13-threads#parallel" %}}) | the results: chunks times the bytes of a result | one `malloc` of the results | one per `parallel` | the lock of the pool, once at the start and twice per chunk | both modes: the bound of the results. Dev: a count of chunks below zero | one thunk per `parallel` in the source |
| [Channels and select]({{% relref "13-threads#channels-and-select" %}}) | a channel value 8. On the heap 56 and `n` values, and a monitor | two per `chan T(n)` | one per `send`, `recv`, `close` and `select` | the lock of the channel once per `send` and `recv`, one atomic load per `send` and `close` | both modes: the capacity, the closed flag and a null handle | none |
| [Atomics]({{% relref "13-threads#atomics" %}}) | the bytes of its type | 0 | 0 | one instruction per operation. A loop on arm64 at `armv8.0`. No lock | none | none |
| [Locks]({{% relref "13-threads#locks" %}}) | a `Mutex`: 4 on Linux and macOS, 8 on Windows | 0 | one to enter a `sync`, one per exit taken | a free lock on Linux: one compare-and-swap, and one exchange to give it back | dev: a `Mutex` its thread holds already, and the order of the locks, with no option | one unlock call per exit of the block |
| [Waits under a lock]({{% relref "13-threads#waits-under-a-lock" %}}) | 0 | 0 | 0 | 0 | none at run time: `blocking-under-lock` warns in a dev build and refuses a release build | none |
| [Synchronized classes]({{% relref "14-concurrent-classes#synchronized-classes" %}}) | 24 per object for the hidden lock | 0 | two per call of a function that is not private | a free lock: one atomic load, the word as a `Mutex`, one atomic store | dev: the record of the order | one unlock call per exit of a function |
| [Concurrent classes]({{% relref "14-concurrent-classes#concurrent-classes" %}}) | the fields alone. A `Mutex` field is one word, a guard 0 | 0 | the `sync` around a guarded field | an atomic field: one atomic instruction per access | none at run time. `unguarded-field` runs in the compiler | none |
| [sync blocks]({{% relref "14-concurrent-classes#sync-blocks" %}}) | 0 | 0 | one lock call, and one unlock call per exit taken. One pair call each for `sync a, b` | a lock taken again: one atomic load and one add | dev: the record of the order | none |
| [Lock order at compile time]({{% relref "14-concurrent-classes#lock-order-at-compile-time" %}}) | 0 | 0 | 0 | 0 | none at run time: `lock-order` refuses a cycle in every build | none |
| [Lock order in dev mode]({{% relref "14-concurrent-classes#lock-order-in-dev-mode" %}}) | dev: 16 on the stack of the thread per lock held, 40 per new pair. Release: 0 | dev: one `calloc` of 64 records, and one per doubling. Release: 0 | dev: inside the lock calls. Release: 0 | dev: the lock of the table when the thread holds another lock | dev alone. No option turns it on in release or off in dev | none |
| [Thread-safe collections]({{% relref "14-concurrent-classes#thread-safe-collections" %}}) | `SyncList<int>` 184, `SyncMap<int, int>` 368, `ConcurrentMap<int, int>` 40, `SpscRing<int, 64>` 56 | those of the plain collection inside. `for x in c` copies every element into a snapshot | those of the hidden lock | one hidden lock per operation. `ConcurrentMap`: one `Mutex` of 16. `SpscRing`: one atomic load and one atomic store, no lock | dev: the record of the order | one copy of the class per set of type arguments |
| [Hooks]({{% relref "15-hooks#hooks" %}}) | 0 per object, 9 table entries of 8 per class | 0 | one per site: `created` and `destroyed` per object, `copied` per `dup`, `dispatched` and `joined` per job. With `--no-hooks`: 0 | inside each call one atomic load, and one more for `created` of an object on the heap. No lock without a plugin | none | none |
| [Tracing]({{% relref "15-hooks#tracing" %}}) | 0 per object. The name of each traced function, once | 0 | traced: two of `anti_rt_hook_call` per call of a function, and `anti_rt_hook_failed` on an exit with an error. Not traced: 0 | one atomic load inside each call. No lock | traced in dev and with `--trace`, not in release and with `--no-trace` | none |
| [Injection]({{% relref "15-hooks#injection" %}}) | 8 per `inject` field, 8 per interface for its slot | 0 by the injection | 0. Per object made, one indirect call of the provider per `inject` field | one atomic load in a provider that is `get` of a singleton | none | none |
| [Runtime configuration]({{% relref "15-hooks#runtime-configuration" %}}) | the table of six keys, the option texts and the TOML reader, in every program | the section counts none. A file costs one read and one parse | one per `rt.get(key)` | one lock per `rt.get(key)` | at start: every option and every key of a file | none |
| [Plugins]({{% relref "15-hooks#plugins" %}}) | the names of the exports of a host stay in the executable | one per `lib.instance` | one open and the checks per `plugin.load` | with a library open, `created` of an object on the heap and `delete` take one lock and one atomic add per object | at load: the checks per interface | none |
| [List and Deque]({{% relref "16-collections#list-and-deque" %}}) | 136 each, and `size_of(T)` per element of capacity | 0 when empty, one block per growth, by doubling from 8 | `anti_rt_mem_alloc` and `anti_rt_mem_free` per growth | 0 | dev: the index of `l[i]` and `remove_at(i)`, and the overflow checks inside | one copy per element type: 64 functions for `List<int>` in dev and 44 in release |
| [Ring and Grid]({{% relref "16-collections#ring-and-grid" %}}) | `Ring<int, 4>` 136, `Grid<i32>` 152 | one block when made, none after | 0 for `push`, `pop` and `g[x, y]` | 0 | dev: the two index compares of `g[x, y]` | one copy per pair of `T` and `N` of a ring, one per `T` of a grid |
| [Map and HashMap]({{% relref "16-collections#map-and-hashmap" %}}) | 160 each. Per entry of capacity 56 for `Map<int, int>` and 48 for `HashMap<int, int>` | 0 when empty, one block per growth, none per entry | `anti_rt_hash_seed` when a map is made, `anti_rt_hash_bytes` per hash of a `str` key | one atomic load when a map is made. No lock | dev: those of the arithmetic inside | one copy per pair of `K` and `V` |
| [Set, HashSet and BitSet]({{% relref "16-collections#set-hashset-and-bitset" %}}) | `Set<int>` and `HashSet<int>` 160, `BitSet` 112 | one block per growth | as the maps. 0 for a `BitSet` | one atomic load when a `Set` or a `HashSet` is made. 0 for a `BitSet` | none of its own | one copy per element type, and one `BitSet` |
| [SortedMap and SortedSet]({{% relref "16-collections#sortedmap-and-sortedset" %}}) | 160 each. A node: `11 * size_of(T)` and 112 | one block per growth of the nodes, none per element | 0. A sorted collection reads no seed and calls no hash | 0 | every build: the order check after a lent entry changed | one copy of the class and of `BTree<T>` per set of type arguments |
| [Pool and Handle]({{% relref "16-collections#pool-and-handle" %}}) | `Pool<Unit>` 520, a `Handle` 16, a slot `size_of(T)` and 24 | one per block of 8, 16, 32 slots and on. No block is copied | 0 | 0 | every build: the test of the handle | one copy per element type |
| [Tree and PriorityQueue]({{% relref "16-collections#tree-and-priorityqueue" %}}) | `Tree<str>` 528, `PriorityQueue<int>` 136. A slot of a tree: `size_of(T)` and 64 | a tree as a pool, a queue as a list. `children` and the walks make a new collection | 0 | 0 | every build: the test of each handle of a tree | one copy per element type |
| [Walking a collection]({{% relref "16-collections#walking-a-collection" %}}) | the iterator of a list 64 in the frame. 72 for the record of changes in every collection | 0 | `anti_rt_hash_draw` at the first walk of a `HashMap` or a `HashSet` | 0 | dev: the walk check, one compare per turn | none |
| [Hashing and its seed]({{% relref "16-collections#hashing-and-its-seed" %}}) | the seed: 8 per program | 0 | `anti_rt_hash_bytes` per hash of a `str`, `anti_rt_hash_seed` when a hashing collection is made | one atomic load in `anti_rt_hash_seed`. No lock | none | none |
| [Order and equality]({{% relref "16-collections#order-and-equality" %}}) | 0 | 0, also for `sort` | 0. `==` of a class value and `<` are calls of `equals` and `lt` | 0 | none | none |
| [The cost of each operation]({{% relref "16-collections#the-cost-of-each-operation" %}}) | one row per collection with its layout and its growth | per collection | per collection | 0 for the plain collections | per collection | per collection |
| [extern fn and C types]({{% relref "17-c-interop#extern-fn-and-c-types" %}}) | a C type: 1, 2, 4 or 8 | 0 | 0. `text.from_c` is one call of `anti_rt_text_from_c` | 0 | none | none. One `declare` per function and module |
| [The ABI of a call]({{% relref "17-c-interop#the-abi-of-a-call" %}}) | the sizes C computes | 0. One error object when a `may fail` function fails | 0 for the call | 0 | dev: the checks inside the function | one function per `export fn` |
| [anti bind]({{% relref "17-c-interop#anti-bind" %}}) | the C layout of a bound struct | 0 | 0 | 0 | none | none. A bound inline function is one more call, into the shim |
| [link]({{% relref "17-c-interop#link" %}}) | a `link` line adds no code and no data. A static Linux program holds musl and the allocator | 0 | 0 | 0 | none | none |
| [Libraries for C]({{% relref "17-c-interop#libraries-for-c" %}}) | for linux-x86_64 in the example: the static archive 48,480, the bundled one 362,292, the shared library 85,352 | 0 for a call | the runtime starts once per process, or once per shared library. Its pool starts at the first `parallel` or `dispatch` | 0 for a call | by the mode of the build | none |
| [Plugins across the boundary]({{% relref "17-c-interop#plugins-across-the-boundary" %}}) | the plugin of the example: 15,152. The host keeps its dynamic symbol table | one per instance | one call through the procedure linkage table per call into the runtime | those of `plugin.load` and of the count of live objects | once per load: the runtime version, the hash chains, the slots | no devirtualisation in a host or a plugin |
| [tests and fixtures blocks]({{% relref "19-tests#tests-and-fixtures-blocks" %}}) | 0 in a build without `--tests`. 16 in the runtime for the name of the running test | 0 by the runner | per test one `anti_rt_test_running` | 0 | the assertions of a dev run. A run with `--release` has none | one function per test and per fixture, in a test build alone |
| [The processor levels]({{% relref "20-targets#the-processor-levels" %}}) | a level changes no size and no layout | 0 | one check of the processor at start | an atomic operation is one instruction at `armv8.2` and above | at start, in every build: the level of the machine | one runtime per target and level in the archive |

## Measuring with tests/bench

`tests/bench` holds six programs, each with a C twin that does the same work and prints the same line.

| Program | Work |
|---|---|
| `scalar_loop` | sums one buffer of a million integers 1,000 times, each pass with its own factor |
| `objects` | 4,096 shapes of two classes take two calls through the table on each of 20,000 passes |
| `builder` | one `text.Builder` collects 50,000 entries of a word, a number and a comma, 200 times over |
| `simd_loop` | the dot product of two buffers of 4,096 `f32`, 200,000 times, in vectors of four lanes |
| `map_work` | 20 rounds of 100,000 insertions, 500,000 lookups and 50,000 removals on a `HashMap<int, int>` |
| `hooks` | 20 million objects of a traced class, each allocated, called once and deleted. [Chapter 15]({{% relref "15-hooks#hooks" %}}) holds its table |

The C twin of a program copies the data structures of the Anti program. `objects.c` has a table of function pointers per class. `builder.c` has a buffer that grows as the runtime's does. `map_work.c` has a hash table laid out as the one of `anti.collection.map`. The pinned clang compiles each twin at `-O2`.

```sh
cmake --build build/host --target bench
```

The target builds the twins and runs `tests/runners/run_bench.cmake`. The runner compiles each Anti program three times in release mode and three times in dev mode. It then runs the release program and its twin five times each. It prints the median of each step and writes the table to `build/host/tests/bench.md`. A program whose line differs from the line of its twin stops the run.

| Program | C run | Anti run | Anti / C | Release compile | Dev compile |
|---|---:|---:|---:|---:|---:|
| `scalar_loop` | 283.1 ms | 284.6 ms | 1.01 | 93.1 ms | 31.6 ms |
| `objects` | 101.2 ms | 104.0 ms | 1.03 | 202.5 ms | 39.5 ms |
| `builder` | 42.9 ms | 56.0 ms | 1.30 | 230.5 ms | 32.0 ms |
| `simd_loop` | 111.1 ms | 111.3 ms | 1.00 | 93.6 ms | 33.6 ms |
| `map_work` | 125.4 ms | 136.2 ms | 1.09 | 601.6 ms | 168.7 ms |

| Condition | Value |
|---|---|
| Machine | Apple M4 Pro, 14 cores, 64 GB, macOS 26.6.2 |
| Target | macos-arm64, at `armv8.5` |
| Commit | `a9e8ffd0` |
| Mode | release, linked with full LTO. The C twin is built by the pinned clang at `-O2` |
| Command | `cmake --build build/host --target bench` |
| Runs | the median of 5 runs of each program and of 3 builds |
| Load | another job ran on the machine during the run, at a load average of 2 on 14 cores |

The release compile is the whole build of a program: the passes of antic, `opt`, and the LTO link with the runtime. The dev compile covers the module of the program and the link. The dev objects of the standard library are built once before the timing starts, as `anti build` keeps them in its cache.

### Options of a build

`run_bench.cmake` passes what `OPTIONS` holds to antic, for the release and the dev compile of a program. The variable `RELEASE_OPTIONS` holds options for the release build alone. Each column below is one run of the runner with one set of options. The table therefore shows what an option costs or saves on the five programs.

| Program | Plain | `--lto none` | `--checks --asserts` | `--no-hooks` | `--trace objects` | `--profile-use` |
|---|---:|---:|---:|---:|---:|---:|
| `scalar_loop` | 284.6 ms | 286.3 ms | 286.4 ms | 287.8 ms | 288.9 ms | 288.6 ms |
| `objects` | 104.0 ms | 102.8 ms | 120.4 ms | 102.6 ms | 483.3 ms | 79.0 ms |
| `builder` | 56.0 ms | 57.6 ms | 56.4 ms | 59.2 ms | 58.1 ms | 52.5 ms |
| `simd_loop` | 111.3 ms | 111.5 ms | 111.5 ms | 112.3 ms | 113.7 ms | 114.5 ms |
| `map_work` | 136.2 ms | 137.0 ms | 165.4 ms | 132.2 ms | 139.3 ms | 144.7 ms |

| Condition | Value |
|---|---|
| Machine, target, commit, runs | those of the table above |
| Command of the first column | `cmake --build build/host --target bench` |
| Command of columns two to five | `cmake -DOPTIONS=<options> -P tests/runners/run_bench.cmake`, with the other values of the `bench` target and the options joined by commas |
| Command of the last column | `cmake -DRELEASE_OPTIONS=--profile-use,<file> -P tests/runners/run_bench.cmake`, with the other values of the `bench` target |
| C twins | the six runs measured the twins between 283.1 and 288.0 ms, 101.2 and 104.1 ms, 42.9 and 44.7 ms, 111.1 and 113.7 ms, and 125.4 and 128.3 ms |
| Load | another job ran on the machine during the runs |

The range of the C twins is the noise of the six runs, up to 4 percent for `builder` and up to 3 percent for the others. A difference inside that range shows nothing. The sections below read the columns that lie outside it.

The release compile under `--lto none` took 35.7, 45.2, 65.3, 35.5 and 294.1 ms for the five programs, against the 93.1 to 601.6 ms of the full LTO link. The run times of the two links lie within the noise.

### A new program

A measurement of a feature the other programs do not reach needs a program of its own. The program is `tests/bench/<name>.anti`, and its twin `tests/bench/<name>.c` does the same work with the same data layout. Both print one line, which the runner compares. The name goes into the list `programs` of `tests/runners/run_bench.cmake` and into `bench_programs` of `tests/CMakeLists.txt`, which builds the twin.

Where it lives: `tests/runners/run_bench.cmake`, the target `bench` in `tests/CMakeLists.txt`, and the programs in `tests/bench/`. `tests/bench/ablate/run.py` measures one change of the compiler against another with more runs.

## --profile-generate and --profile-use

A profile tells the optimizer of LLVM how often each branch and each call of a real run was taken. A build takes three steps.

```sh
antic --profile-generate -o objects objects.anti
./objects
llvm-profdata merge -o objects.profdata default_*.profraw
antic --profile-use objects.profdata -o objects objects.anti
```

- `--profile-generate` builds an instrumented program in release mode. `opt` adds counters to the code of every function, and the program links the profile runtime of the archive, `libclang_rt.profile`.
- The instrumented program writes a raw profile when it exits: `default_<signature>_0.profraw` in its working directory. `<signature>` identifies the program. The runs of one program therefore merge into one file, and those of two programs stay apart. The environment variable `LLVM_PROFILE_FILE` names another file.
- `llvm-profdata merge` writes the indexed profile from one or more raw files. `llvm-profdata` stands in `bin/` of the package.
- `--profile-use <file>` builds the release program with the counts. `opt` reads them at the same point of the pipeline where the first build added the counters.

`anti build` and `anti run` take both options. A build with a profile keeps the kind of link it would have without one. Under full LTO the profile acts on `opt` before the link, since the LTO of lld reads no profile of this kind.

antic refuses the options where they cannot work.

| Command line | Message |
|---|---|
| `--dev --profile-generate` | `antic: --profile-generate builds in release mode, without --dev` |
| both options | `antic: --profile-generate and --profile-use exclude each other` |
| `--profile-generate -S` | `antic: --profile-generate links a program, without -S, -c, --lib or --linker platform` |
| `--profile-use` with a missing file | `antic: cannot read nothere.profdata` |
| `--profile-use` with a raw profile | `antic: <file> is a raw profile. llvm-profdata merge -o <file> <file> writes the profile that --profile-use reads` |
| `--profile-use` with another file | `antic: <file> is no profile of llvm-profdata merge` |

The instrumented `objects` of macos-arm64 is 128,544 bytes, against 89,888 for the plain release program. It is a program for a training run and not one that ships.

The last column of the table of options is the build with a profile. Each of the five programs was built with `--profile-generate` and run once, and `llvm-profdata merge` wrote one profile from the five raw files. With it `objects` ran in 79.0 ms against 104.0 ms, and `builder` in 52.5 ms against 56.0 ms. `map_work` ran in 144.7 ms against 136.2 ms, which is slower by more than the noise. `scalar_loop` and `simd_loop` stayed within it. A profile is therefore measured per program before it is kept.

The profile fits the source it was made from. `opt` matches each function to its counts by a hash of its code, and a function that changed since the training run takes none.

Where it lives: `profile_options` of `src/antic/llvm_run.c` passes the options to `opt`, and `indexed_profile_file` of `src/antic/driver.c` checks the file. The tests `profile_guided` and `profile_lto_<lto>_<target>` of `tests/CMakeLists.txt` build and run a program with its own profile under each kind of link.

## Tracing

A traced function calls the runtime twice, once on entry and once on exit. A class marked `trace` is traced in a dev build and with `--trace`, and `--trace <pattern>` traces a class that did not ask. [Chapter 15]({{% relref "15-hooks" %}}) counts what stands inside each call.

The column `--trace objects` of the table of options traces every class of the module `objects`, in the release build, with no handler installed. On each of 20,000 passes, each of 4,096 shapes takes `grow`, `area` and one `add` of the counter. That is 245.76 million traced calls. The program ran in 483.3 ms against 104.0 ms. The other four programs hold no class of that module and stayed within the noise.

The time is the cost of the sites alone. An installed handler adds one indirect call per site and its own work. The stock handlers of `anti.trace` differ in that work: the `Profiler` reads the clock and searches its 64 names, and the `CallLogger` writes a line. `tests/bench` holds no program that installs a handler, so the page states no time for one.

Three options decide the sites of a build.

| Option | Sites of a traced class | Gives up |
|---|---|---|
| none, in a dev build | `enter`, `leave`, `failed` | nothing |
| none, in a release build | none | the three hooks, until `--trace` is given |
| `--trace`, `--trace <pattern>` | the three hooks, in either mode | the time above, per traced call |
| `--trace writes` | one more call per store to a field | the same, per store |
| `--no-trace` | none, in either mode | `enter`, `leave` and `failed` of every class |
| `--no-hooks` | none, and none of the five hooks of every object | every hook, every handler and the loading of plugins |

The column `--no-hooks` is a release build without the five hooks of every object. `objects`, `scalar_loop` and `simd_loop` stayed within the noise. `map_work` ran in 132.2 ms against 136.2 ms, at 1.05 times its twin against 1.09. `builder` ran in 59.2 ms against 56.0 ms, which lies outside the range of its twins.

The column was therefore measured two more times. `map_work` repeats its gain, and `builder` does not repeat its loss.

| Program | C twin | Anti with `--no-hooks` | Anti / C |
|---|---:|---:|---:|
| `builder`, first run | 44.7 ms | 57.8 ms | 1.29 |
| `builder`, second run | 44.7 ms | 55.5 ms | 1.24 |
| `map_work`, first run | 126.4 ms | 132.0 ms | 1.04 |
| `map_work`, second run | 126.8 ms | 132.6 ms | 1.05 |

| Condition | Value |
|---|---|
| Machine, target, mode, runs | those of the first table of this section |
| Commit | `2d850efa`, whose compiler, runtime and bench programs equal those of `a9e8ffd0` |
| Command | `cmake -DOPTIONS=--no-hooks -P tests/runners/run_bench.cmake`, with the other values of the `bench` target |
| Plain build of the same hour | `builder` 57.4 ms at 1.29 times its twin, `map_work` 137.3 ms at 1.09 |
| Load | another job ran on the machine, at a load average of 17 on 14 cores |

## Checks of a release build

Outside dev mode a program holds no check of that mode and no assertion. The options `--checks` and `--asserts` keep them in a release build, and `--no-checks` and `--no-asserts` drop them from a dev build. The build that compiles the program decides for every module in it. The standard library and the copies of its generics are among them.

| Check of dev mode | Section | In a release build | Without it |
|---|---|---|---|
| Overflow of signed `+ - *` | [Integers]({{% relref "03-values#integers" %}}) | dropped, kept by `--checks` | the result wraps |
| A zero divisor of `/` and `%` | [Integers]({{% relref "03-values#integers" %}}) | dropped, kept by `--checks` | the division gives 0 |
| The count of `<<` and `>>` | [Integers]({{% relref "03-values#integers" %}}) | dropped, kept by `--checks` | the count is masked to the width |
| The range of a narrowing `as` | [Integers]({{% relref "03-values#integers" %}}) | dropped, kept by `--checks` | the value is truncated |
| A number converted to an enum | [Enums]({{% relref "08-structs-enums#enums" %}}) | dropped, as the section states it | a value outside the declared ones is undefined behaviour |
| The bounds of `s[i]` and `a[i]` | [Bounds checks]({{% relref "12-memory#bounds-checks" %}}) | dropped, kept by `--checks` | an index out of range reads or writes other memory |
| The bounds of `load` and `store` of a simd struct | [Simd structs]({{% relref "08-structs-enums#simd-structs" %}}) | dropped, as the section states it | the same |
| The index of `l[i]`, `remove_at(i)` and `g[x, y]` | [List and Deque]({{% relref "16-collections#list-and-deque" %}}) | dropped, kept by `--checks` | the same |
| A collection changed during a loop over it | [Walking a collection]({{% relref "16-collections#walking-a-collection" %}}) | dropped, kept by `--checks` | the loop reads a block that a growth freed |
| The zero table before `is`, `as` and a dispatch | [Classes and the object in memory]({{% relref "09-classes#classes-and-the-object-in-memory" %}}) | dropped, and `--checks` does not bring it back. `--no-checks` leaves it in a dev build. The check of `p as *T` stays | a zero table pointer is not reported before the dispatch reads through it |
| `assert` | [assert and the dev checks]({{% relref "06-errors#assert-and-the-dev-checks" %}}) | dropped with its condition, kept by `--asserts` | the condition is not evaluated |
| The order of the locks | [Lock order in dev mode]({{% relref "14-concurrent-classes#lock-order-in-dev-mode" %}}) | not built in, with no option | an inversion that no dev run took is found by no build |

These checks stand in every build, and no option of antic removes them.

| Check of every build | Section |
|---|---|
| The check of `p as *T` to a class | [Classes and the object in memory]({{% relref "09-classes#classes-and-the-object-in-memory" %}}) |
| The zero table in `delete`, in the teardown of an `own` pointer and in `dup` | [own and lent]({{% relref "11-ownership#own-and-lent" %}}) |
| The test for an object in flight at `dispatch` | [Threads, dispatch and join]({{% relref "13-threads#threads-dispatch-and-join" %}}) |
| The bound of the results of `parallel` | [parallel]({{% relref "13-threads#parallel" %}}) |
| The capacity of `chan T(n)`, the closed flag at `send` and a null handle at every operation | [Channels and select]({{% relref "13-threads#channels-and-select" %}}) |
| The test of a handle of a `Pool` and of a `Tree` | [Pool and Handle]({{% relref "16-collections#pool-and-handle" %}}) |
| The order check after a lent entry of a sorted collection changed | [SortedMap and SortedSet]({{% relref "16-collections#sortedmap-and-sortedset" %}}) |
| The checks of a plugin at its load | [Plugins across the boundary]({{% relref "17-c-interop#plugins-across-the-boundary" %}}) |
| The level of the processor at start | [The processor levels]({{% relref "20-targets#the-processor-levels" %}}) |

### A check in the code

The program below reads one element of a slice in `pick` and sums a slice in `total`.

```anti
//! name: kept
import anti.io;
import anti.text;

export fn pick(p: *int, n: int, i: int) -> int
{
	let items = []int { ptr: p, len: n };
	return items[i];
}

export fn total(p: *int, n: int) -> int
{
	let items = []int { ptr: p, len: n };
	let t = 0;
	for i in 0..items.len {
		t += items[i];
	}
	return t;
}

fn main() -> int
{
	let a: [4]int = [3, 5, 8, 13];
	let line = f"pick {pick(&a[0], 4, 2)}, total {total(&a[0], 4)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
pick 8, total 29
```

A release build writes `pick` as one load, and the loop of `total` as vector adds.

```asm excerpt=kept target=linux-x86_64 mode=release
<pick>:
movq (%rdi,%rdx,8), %rax
retq
...
<total>:
...
vpaddq (%rdi,%rax,8), %ymm0, %ymm0
vpaddq 0x20(%rdi,%rax,8), %ymm1, %ymm1
vpaddq 0x40(%rdi,%rax,8), %ymm2, %ymm2
vpaddq 0x60(%rdi,%rax,8), %ymm3, %ymm3
addq $0x10, %rax
cmpq %rax, %rcx
jne
```

- `pick` loads `p[i]` and returns. No instruction reads `n`.
- One turn of the loop of `total` adds 16 elements, four in each of four registers of 256 bits. A signed `+` without its check wraps, so the optimizer may add the elements in any order.

The release build with `--checks` keeps the bounds check of `pick` and the overflow check of `total`.

```asm excerpt=kept target=linux-x86_64 mode=release option=checks
<pick>:
movq %rdx, %rcx
cmpq %rsi, %rdx
jae
movq (%rdi,%rcx,8), %rax
retq
...
<total>:
...
movq (%rdi,%rcx,8), %r8
incq %rcx
movq %rax, %rdx
addq %r8, %rdx
jno
```

- `cmpq` and `jae` of `pick` compare `i` with `n` as unsigned numbers, so one branch covers an index below zero and one past the end. The passing path is three instructions longer than the plain one.
- The loop of `total` has no compare of the index with the length. The optimizer proved that `i` stays below `items.len` and removed the bounds check.
- The overflow check stays: `addq` and `jno` once per element. The check names the first sum that overflows, so the elements are added one by one and the loop is not vectorized.

The build for linux-arm64 shows the same two forms: `cmp` and `b.hs` for the bounds, and `adds` and `b.vc` for the overflow.

The column `--checks --asserts` of the table of options is a release build that keeps the checks of dev mode. `objects` ran in 120.4 ms against 104.0 ms, and `map_work` in 165.4 ms against 136.2 ms. `scalar_loop`, `builder` and `simd_loop` stayed within the noise. The five programs hold no assertion, so the column measures the checks.

A release build with `--checks --asserts` runs the optimized code and still stops at the first violation. It is the build for a test run of the program that ships, and the table gives what it costs on these five programs.
