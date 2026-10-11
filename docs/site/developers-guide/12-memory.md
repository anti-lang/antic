---
title: "Memory"
description: "Pointers, slices and views, the aliasing of views, the bounds checks, anti.mem, the allocator, alloc and free, and --memory-checks."
summary: "Pointers, slices and views with their aliasing, the bounds checks of dev mode, `anti.mem` and the allocator, `alloc` and `free`, and `--memory-checks`, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 120
tags: [compilers, programming-languages]
keywords: [pointers, slices, views, aliasing, bounds checks, allocator, AddressSanitizer, memory checks]
---

## Pointers

### What you know

The pointers of C and C++, the references of Java, and pointer arithmetic.

### The feature

`*T`, `&x`, `*p`, `alloc T { }`, `free`, `size_of`, a pointer to a field or to an element, no arithmetic, and the refusals.

### How antic builds it

A pointer as `ptr` in the LLVM text, and `alloc` and `free` as calls of the allocator, with excerpts.

### What it costs

The bytes of a pointer by `size_of`, the runtime calls of `alloc` and `free`, and the checks of a dereference in each mode.

### Pitfalls

Where pointer arithmetic and `free` of a class pointer behave other than a C or C++ programmer expects.

## Slices and views

### What you know

The arrays of Java, the arrays and pointers of C, and `std::span` and `std::vector` of C++.

### The feature

`[]T`, `[N]T`, a slice from a pointer and a length, `s[a..b]`, `.ptr` and `.len`, a view that aliases, `for x in &s`, and the refusals.

### How antic builds it

A slice as a pointer and a length, an array in place, and a view as the same pointer, with excerpts.

### What it costs

The bytes of a slice by `size_of`, the allocations of a view, and the copy of an array.

### Pitfalls

Where a view aliases the memory it comes from, and an array is a value, other than a Java, C or C++ programmer expects.

## Bounds checks

### What you know

The `ArrayIndexOutOfBoundsException` of Java, the undefined behaviour of C and C++, and `at()`.

### The feature

The check of an index, the mode that inserts it, what a failed check prints, `--checks` and `--no-checks`.

### How antic builds it

The compare and the call of `anti_rt_check_failed` of an index in the LLVM text, with excerpts in both modes.

### What it costs

The compare per index, the mode that inserts it, and what turning it off gives up.

### Pitfalls

Where the index check of each mode behaves other than a Java, C or C++ programmer expects.

## anti.mem and the allocator

### What you know

`malloc` and `free` of C, `new` and `delete` of C++, and the heap of Java.

### The feature

`anti.mem.Allocator` with `alloc(size, align)` and `free(p)`, `LibcAllocator`, `ArenaAllocator`, `take_in(from)`, `deserialize(input, from)`, `delete(p, from)`, and the refusals.

### How antic builds it

The table entries 8 and 9 the runtime calls, `src/rt/mem.c`, and the blocks of an `ArenaAllocator`, with excerpts.

### What it costs

The call per `alloc` and `free`, the bytes of a block of `ArenaAllocator`, and mimalloc as the allocator of every program of musl.

### Pitfalls

Where memory of one allocator goes back to another, other than a C or C++ programmer expects.

## --memory-checks

### What you know

AddressSanitizer of C and C++, Valgrind, and the garbage collector that spares Java both.

### The feature

`--memory-checks` for antic, `anti build`, `anti run` and `anti test`, what it finds, the report through the symbolizer, and the targets that refuse it.

### How antic builds it

The checks of AddressSanitizer at every load and store, the runtime from the runtime archive and the blocks it marks, with an excerpt.

### What it costs

The time and the memory of a checked program, from `tests/bench`, and the cost without the option.

### Pitfalls

Where a leak at exit, a kept block and an x86_64 target under emulation behave other than a C or C++ programmer expects.
