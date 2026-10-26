---
title: "The developer's guide"
description: "A guide to every feature of Anti for a programmer from Java, C or C++: what the feature does, how antic builds it and what it costs."
summary: "The developer's guide of Anti: each feature in depth, how antic builds it and what it costs, for a programmer who knows Java, C or C++, with examples that compile and excerpts of the code antic writes."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-26T12:00:00+01:00
draft: true
weight: 1
tags: [compilers, programming-languages]
keywords: [developer guide, Anti language, compiler, performance, Java, C, C++]
---

The guide is for a working programmer from Java, C or C++. The reader knows pointers, the stack and the heap, a vtable and a little assembly. Each chapter teaches a feature in depth: what it does, how antic builds it and what it costs. The rules the checker applies and the cases it refuses stand with it. Every `anti` block of a page compiles, every program prints what its output block shows, and every excerpt stands in the code antic writes. The test `guide_examples` of the suite checks each page, so the guide fails the suite when the compiler changes under it.

## Section layout

Every feature section has five parts, in this order.

- What you know: how Java, C and C++ handle the subject, as far as it bears on the difference.
- The feature: every form, every rule the checker applies, the edge cases and the refusals, with examples that run and examples the checker refuses. A reader who has read it needs no other page to use the feature.
- How antic builds it: what happens when the program runs, first as a model in plain words. Where the value lives: on the stack, in a register, inline in its owner or on the heap. What is copied and what is passed by address. Which indirections a use goes through, and which calls into the runtime it makes. Then the layout in memory, what dev mode adds and what a release build removes. An excerpt of the LLVM IR or the machine code antic produces for an example proves the model. The prose explains the excerpt line by line. A line "Where it lives:" names the source files and functions.
- What it costs: in dev and in release, the bytes per value shown by `size_of`, the heap allocations and the calls into the runtime. The atomic operations and the locks. The checks inserted in each mode, with the option that turns each off and what that gives up. The copies of code a use makes. A time comes only from `tests/bench`, beside its C twin. Its table names the machine, the commit, the mode and the command. The part closes with the choice: the other forms of Anti that do the same job, and when each is the better one.
- Pitfalls: where the feature behaves other than a programmer of each of the three languages expects.

[Chapter 2]({{% relref "02-how-antic-works" %}}) explains the passes of antic, the model the third part builds on and how to read an excerpt.

## Chapters

1. [Getting started]({{% relref "01-getting-started" %}}): Install the package, antic and anti, a first program, `anti build` and `anti run`, dev and release, and where the files go.
2. [How antic works]({{% relref "02-how-antic-works" %}}): The passes from source to executable with the files that run them. The source text, the IR of antic and the LLVM module. Dev and release builds, whole-program compilation and the LTO of the runtime. The runtime archive, the allocator and mimalloc on musl, and what an executable holds beside the program. Reading and making the excerpts with the `--dump-*` options and `--keep-llvm`.
3. [Values]({{% relref "03-values" %}}): Types, literals, variables and constants, and the constants the checker refuses for their range. The operators and the order of evaluation. Integers that wrap by `+%` and saturate by `+|`, the flags form, `mul_high` and the overflow checks. Floats and `f16`, text and bytes and their layout, `f"..."` and its format specifications, pattern literals, tuples and type inference.
4. [Flow]({{% relref "04-flow" %}}): `if`, `while`, `for` over ranges and collections, `switch` on values and on `str`, `x in lo..hi` and what they compile to. `break` and `continue`, `defer` and `undo`, the paths that end a function and the checks of the paths.
5. [Functions]({{% relref "05-functions" %}}): Declaration, parameters, defaults, `here` and source locations, `never`, the calling convention and inlining. Anonymous functions and closures with their captures, `snapshot fn` and `own fn`, function values and their layout, calls without effects.
6. [Errors]({{% relref "06-errors" %}}): `may fail`, `fail`, `try` and `try` blocks with `yield`, `catch`, `catch none` and `catch fatal`, and how an error travels back. Error origins and stack traces and their cost, `assert` and the dev checks. Warnings with their names, `allow` and `unchecked`, `--warnings-as-errors`.
7. [Optionals]({{% relref "07-optionals" %}}): Nullable pointers, `?T` of any type and its layout, narrowing, `if let`, `let ... else`, `??`, `?.` and the refusals of the checker.
8. [Structs and enums]({{% relref "08-structs-enums" %}}): Structs and their layout, `packed` and `align`, unions and bitfields, simd structs and their constraints. Enums and the constant expressions of their values, variants with their tag and payload, pattern matching, the default `==` and hash.
9. [Classes]({{% relref "09-classes" %}}): Classes and the object in memory, `construct` and `destruct`, fields, methods, dispatch through interfaces and nested types. Static fields, constants and their bare names, singletons, operators and the language hooks, reflection and its tables.
10. [Generics]({{% relref "10-generics" %}}): Generic functions and classes, constraints and `Ordered`, inference through the base classes, one copy per use and constant parameters. Generics of library files, what can be generic, and the size of the code.
11. [Ownership]({{% relref "11-ownership" %}}): `own`, `lent`, `for x in &e`, `dup`, moves, `take(x.f)`, teardown, temporaries and the rules at a call. `Shared<T>` and its counts, the safety checks `escaping-local` and `use-after-free`, and what the checker refuses.
12. [Memory]({{% relref "12-memory" %}}): Pointers, slices and views, the aliasing of views, the bounds checks of each mode and those the checker proves, `anti.mem`, `Allocator` and `ArenaAllocator`, `alloc` and `free`, and `--memory-checks`.
13. [Threads]({{% relref "13-threads" %}}): `worker fn`, `dispatch` and `join`, `parallel`, channels and `select`, atomics, `Mutex` and `sync`, waits under a lock, and what the checker refuses.
14. [Concurrent classes]({{% relref "14-concurrent-classes" %}}): Synchronized and concurrent classes, the hidden lock, guarded fields and `sync` blocks. The lock order check of the checker and that of dev mode, the thread-safe collections.
15. [Hooks]({{% relref "15-hooks" %}}): Hooks and tracing, injection, runtime configuration, plugins and interface versions, `--no-hooks` and `--no-trace`.
16. [Collections]({{% relref "16-collections" %}}): `List`, `Map`, `HashMap`, `Set`, `Deque`, `Ring`, `Grid`, `Pool`, `Tree`, `PriorityQueue`, `BitSet` and the sorted ones. Their layout, their growth and the cost of each operation in one table. Walking, the change check of dev mode, hashing and its seed, order and equality.
17. [C interop]({{% relref "17-c-interop" %}}): `extern fn`, the C types, the ABI of a call, `anti bind`, `link`, libraries for C with `--lib` and `--bundle-runtime`, and plugins across the boundary.
18. [The tool]({{% relref "18-the-tool" %}}): Modules, imports and direct imports, visibility across modules, library files, `anti.toml`, dependencies and the lock file, `anti build` and its cache, `anti check`, `anti fmt`, `anti doc`, `anti license`, `anti symbols`.
19. [Tests]({{% relref "19-tests" %}}): `tests` and `fixtures` blocks, `anti test`, expected output, and memory checks in tests.
20. [Targets]({{% relref "20-targets" %}}): The six targets, `--target` and `--cpu` and the processor levels, cross-building and script mode. `-g` and debugging, the build id, and what a release build does with LLVM.
21. [Performance]({{% relref "21-performance" %}}): One table of the cost of every feature of the guide, with a link to its section. Measuring with `tests/bench`, `--profile-generate` and `--profile-use`, and tracing. Which checks a release keeps, and what turning them off gives up.

## Subjects outside the guide

No chapter covers the subjects below. The overview is `docs/anti-syntax-overview.md` of the repository. It lists every form of the language on one page and names the forms that are not built.

| Subject | Where it stands |
|---|---|
| The modules of the standard library that no chapter uses, `anti.json`, `anti.args`, `anti.fs`, `anti.log` and `anti.os` among them | "Standard library" of the overview |
| `target.os`, wire formats and named arguments, which are not built | the overview marks each as not built |
