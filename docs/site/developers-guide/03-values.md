---
title: "Values"
description: "The types and literals of Anti, variables and constants, integers that wrap or saturate, overflow checks, floats and f16, text and bytes, tuples and inference."
summary: "Types, literals, variables and constants, integers with `+%` and `+|` and the overflow checks of dev mode, floats and `f16`, `str` and `[]byte` with their layout, tuples and type inference, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 30
tags: [compilers, programming-languages]
keywords: [types, literals, integer overflow, wrapping arithmetic, saturating arithmetic, f16, strings, tuples, type inference]
---

## Types and literals

### What you know

The primitive types of Java, C and C++, their literals and the suffixes that fix the type of a literal.

### The feature

Every type of Anti and every literal form with its prefixes and escapes. How a literal takes its type from its context, and what the checker refuses.

### How antic builds it

The LLVM type of each type of Anti per target, with an excerpt of a function that returns a literal of each kind.

### What it costs

The bytes and the alignment of each type by `size_of` on the six targets, and the instructions a literal takes at run time.

### Pitfalls

Where a literal, a type name or a conversion behaves other than a Java, C or C++ programmer expects.

## Variables and constants

### What you know

Declaration and initialisation in Java, C and C++, `final` and `const`, and memory that is never initialised.

### The feature

`let` with its mandatory initialisation, `const` at module level, in a block and in a class body, shadowing in an inner block, and the refusals.

### How antic builds it

A local and a constant in the LLVM text of each mode, with an excerpt of each.

### What it costs

What a local and a `const` cost in memory in dev mode and in release mode.

### Pitfalls

Where `let`, `const` and shadowing behave other than a Java, C or C++ programmer expects.

## Integers

### What you know

Integer overflow in Java, C and C++: wrapping, undefined behaviour and the checks each language offers.

### The feature

The integer types, `+%` and the other wrapping operators, `+|` and the saturating ones, `mul_high`, `let (result, flags) = e;`, the overflow checks of dev mode, and the refusals.

### How antic builds it

The `llvm.sadd.with.overflow` family of a checked operation, the plain instruction of a wrapping one, and `adc` for a carry in, with excerpts.

### What it costs

The branch per checked operation, the mode that inserts it, `--checks` and `--no-checks`, and what turning the checks off gives up.

### Pitfalls

Where overflow, division, a shift and a conversion behave other than a Java, C or C++ programmer expects.

## Floats and f16

### What you know

`float` and `double` of Java, C and C++, their arithmetic, and how each stores a half-precision number.

### The feature

`f32`, `f64` and `float`, their literals and operators, `f16` as storage that reads as an `f32` and writes with `as f16`, and the refusals.

### How antic builds it

The float instructions of LLVM, the conversion of an `f16` in one instruction on ARM64 and x86-64-v3 and through the runtime below, with excerpts.

### What it costs

The bytes of each float type by `size_of`, the runtime calls of an `f16` conversion at `v1` and `v2`, and the checks of each mode.

### Pitfalls

Where a float literal, a comparison, an `f16` and the processor level behave other than a Java, C or C++ programmer expects.

## Text and bytes

### What you know

`String` of Java, `char *` of C and `std::string` of C++, their encoding, their length and who frees them.

### The feature

`str`, `[]byte` and `char`, the literal forms with their prefixes, the functions of `anti.text`, `f"..."` and `rf"..."`, and the refusals.

### How antic builds it

A `str` as a pointer and a length, where a literal stands in the object, and an `f"..."` as calls on `anti.text.Builder`, with excerpts.

### What it costs

The bytes of a `str` by `size_of`, the heap allocation of an `f"..."` and who frees it, and the runtime calls of each function of `anti.text`.

### Pitfalls

Where the encoding, the length, a comparison and the memory of a text behave other than a Java, C or C++ programmer expects.

## Tuples

### What you know

Pairs and tuples in Java, C and C++: `std::tuple`, `std::pair` and a struct written by hand.

### The feature

`(int, str)` as a type, `(a, b)` as a value, `t.0`, `let (a, b) = e;`, `for i, x in items`, and the refusals.

### How antic builds it

A tuple as an anonymous struct with C layout, one per distinct tuple in the header, with excerpts. A `may fail` function that returns one through an out pointer.

### What it costs

The bytes of a tuple by `size_of`, its padding, and the calls and the allocations of building one and of taking it apart.

### Pitfalls

Where a tuple, its layout and its passing behave other than a Java, C or C++ programmer expects.

## Type inference

### What you know

`var` of Java, `auto` of C++, and the absence of inference in C.

### The feature

Where Anti infers a type: `let` without a type, a literal from its context and the type arguments of a generic call. Where the checker refuses to infer.

### How antic builds it

The pass of the checker that fixes every type before lowering, so the IR and the LLVM text carry no inference, with an excerpt.

### What it costs

The run-time code of an inferred type against a written one, and the work of the checker.

### Pitfalls

Where inference picks a type other than a Java, C or C++ programmer expects.
