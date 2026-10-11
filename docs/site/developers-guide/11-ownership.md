---
title: "Ownership"
description: "own, lent, dup, moves, teardown, temporaries, the rules at a call, Shared<T> and its counts, and what the checker refuses and why."
summary: "`own` and `lent`, `dup` and moves, teardown and temporaries, the rules at a call, `Shared<T>` of `anti.mem` with its atomic count, and what the checker refuses and why, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 110
tags: [compilers, programming-languages]
keywords: [ownership, borrowing, move semantics, dup, teardown, reference counting, Shared]
---

## own and lent

### What you know

The garbage collector of Java, `unique_ptr` and references of C++, and the conventions of C.

### The feature

`own` on a field and on a parameter, and `lent` on a pointer and on a slice. What a lent pointer may do, what derives from one, and the refusals.

### How antic builds it

What ownership changes in the LLVM text at a call and at the end of an owner, with excerpts.

### What it costs

The run-time cost of `own` and of `lent`, and the free an owner runs.

### Pitfalls

Where a lent pointer may not be stored, and an owner frees, other than a Java, C or C++ programmer expects.

## dup and moves

### What you know

The copy constructor and the move of C++, the reference copy of Java, and `memcpy` of C.

### The feature

A local that moves into an `own` parameter, `dup(x)` and `dup(p)`, and the copy of a value that owns nothing. The refusal of a moved local, and the other refusals.

### How antic builds it

A move and a `dup` in the LLVM text, with excerpts.

### What it costs

The bytes a move copies, and the allocations of a `dup`.

### Pitfalls

Where `=`, a move and `dup` behave other than a Java, C or C++ programmer expects.

## Teardown and temporaries

### What you know

The destructors at the end of a scope in C++, the finalizer of Java, and the manual free of C.

### The feature

Teardown part by part at the end of a block, and a struct or a tuple with an owning part. The temporary of a call, `destroy(p)`, `delete(p, from)`, and the refusals.

### How antic builds it

The teardown code at every exit of a block and the destruct chain in the LLVM text, with excerpts.

### What it costs

The calls per teardown, and the code at every exit.

### Pitfalls

Where a temporary is torn down, and a value with an owning part follows the class rules, other than a C or C++ programmer expects.

## The rules at a call

### What you know

By value, by reference and by move at a call in Java, C and C++.

### The feature

What `own`, `keep`, `lent` and a plain parameter accept and refuse, a parameter that belongs to the caller, `return` of one, and the refusals with their reasons.

### How antic builds it

What each parameter kind costs in the LLVM text at the call and in the function, with an excerpt.

### What it costs

The cost of a plain parameter, of an `own` one and of a `lent` one.

### Pitfalls

Where a plain parameter may not be stored, and `own` takes the local, other than a Java, C or C++ programmer expects.

## Shared objects

### What you know

`shared_ptr` of C++, the garbage collector of Java, and reference counting written by hand in C.

### The feature

`anti.mem.Shared<T>`, `share()`, `read(f)`, `modify(f)`, when the object is destroyed, the cycle that is never freed, and the refusals.

### How antic builds it

The atomic count of a `Shared<T>` and the runtime functions of `anti.mem`, with excerpts.

### What it costs

The bytes of a `Shared<T>` by `size_of`, the atomic operation per `share` and per free, and the heap allocation.

### Pitfalls

Where a cycle is never freed, and `=` stays refused, other than a C or C++ programmer expects.
