---
title: "The developer's guide"
description: "A guide to every feature of Anti for a programmer from Java, C or C++: what the feature does, how antic builds it and what it costs."
summary: "The developer's guide of Anti: each feature in depth, how antic builds it and what it costs, for a programmer who knows Java, C or C++, with examples that compile and excerpts of the code antic writes."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 1
tags: [compilers, programming-languages]
keywords: [developer guide, Anti language, compiler, performance, Java, C, C++]
---

The guide is for a working programmer from Java, C or C++. The reader knows pointers, the stack and the heap, a vtable and a little assembly. Each chapter teaches a feature in depth: what it does, how antic builds it and what it costs. The rules the checker applies and the cases it refuses stand with it. Every `anti` block of a page compiles, every program prints what its output block shows, and every excerpt stands in the code antic writes. The test `guide_examples` of the suite checks each page, so the guide fails the suite when the compiler changes under it.

## Section layout

Every feature section has five parts, in this order.

- What you know: how Java, C and C++ handle the subject, as far as it bears on the difference.
- The feature: every form, every rule the checker applies, the edge cases and the refusals, with examples that run and examples the checker refuses.
- How antic builds it: what the front end turns the feature into, the layout in memory and the runtime functions it calls by name. What dev mode adds and what a release build removes, with an excerpt of the LLVM IR or the machine code. The source files that hold it.
- What it costs: the bytes per value, the heap allocations, the calls into the runtime, the atomic operations and the locks. The checks inserted in each mode with the option that turns each off, and the copies of code a use makes. A time comes only from `tests/bench`.
- Pitfalls: where the feature behaves other than a programmer of each of the three languages expects.

## Chapters

1. Getting started: Install the package, antic and anti, a first program, `anti build` and `anti run`, dev and release, and where the files go.
2. How antic works: The passes from source to executable, the IR and the LLVM module, LTO, the runtime archive, and how to read the excerpts.
3. Values: Types, literals, variables and constants, integers that wrap or saturate, the overflow checks, floats and `f16`, text and bytes, tuples, type inference.
4. Flow: `if`, `while`, `for` over ranges and collections, `switch` and what it compiles to, `break` and `continue`, `defer` and `undo`.
5. Functions: Declaration, parameters, defaults, source locations, the calling convention, inlining, closures and their captures, `snapshot fn`, `own fn` and function values.
6. Errors: `may fail`, `fail`, `try`, handlers, how an error travels back, origins and stack traces, `never`, `assert` and the checks of dev mode.
7. Optionals: Nullable pointers, `?T` and its layout, `if let`, `??`, and the refusals of the checker.
8. Structs and enums: Structs and their layout, `packed` and `align`, unions and bitfields, simd structs, enums, variants with their tag, and pattern matching.
9. Classes: Classes and the object in memory, `construct` and `destruct`, fields, methods, interfaces and dispatch, static fields and singletons, operators, reflection.
10. Generics: Generic functions and classes, constraints, one copy per use, what can be generic, and the size of the code.
11. Ownership: `own`, `lent`, `dup`, moves, teardown, temporaries, the rules at a call, `Shared<T>` and its counts, and what the checker refuses.
12. Memory: Pointers, slices and views, aliasing, the bounds checks, `anti.mem`, the allocator, `alloc` and `free`, and `--memory-checks`.
13. Threads: Threads, `dispatch` and `join`, `parallel`, channels and `select`, atomics, locks, and what the checker refuses.
14. Concurrent classes: Synchronized and concurrent classes, the hidden lock, guarded fields, `sync` blocks, the lock order checks of dev mode, the thread-safe collections.
15. Hooks: Hooks and tracing, injection, runtime configuration, plugins, `--no-hooks` and `--no-trace`.
16. Collections: The collections of `anti.collection` with their layout, their growth and the cost of each operation, walking, hashing and its seed, order, equality.
17. C interop: `extern fn`, the C types, the ABI of a call, `anti bind`, `link`, libraries for C, and plugins across the boundary.
18. The tool: `anti.toml`, dependencies and the lock file, `anti build` and its cache, `anti check`, `anti fmt`, `anti doc`, `anti license`, `anti symbols`.
19. Tests: `tests` and `fixtures` blocks, `anti test`, expected output, and memory checks in tests.
20. Targets: The six targets, `--target` and `--cpu`, the processor levels, cross-building, script mode, `-g` and debugging, the build id, LLVM in a release build.
21. Performance: One table of the cost of every feature, measuring with `tests/bench`, profiles, tracing, and which checks a release keeps.
