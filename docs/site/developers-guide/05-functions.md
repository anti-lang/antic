---
title: "Functions"
description: "Declaration, parameters, defaults and source locations, the calling convention, inlining, closures and their captures, snapshot fn, own fn and function values."
summary: "Declaration and parameters, default values and `here`, the calling convention, inlining, anonymous functions and closures with their captures, `snapshot fn` and `own fn`, and function values with their layout, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 50
tags: [compilers, programming-languages]
keywords: [functions, parameters, default arguments, calling convention, inlining, closures, captures, function pointers]
---

## Declaration and parameters

### What you know

Functions and methods of Java, C and C++, parameters by value and by reference, and overloading.

### The feature

`fn` with its parameters and its result, `pub` and the other visibility levels, `keep`, `own`, `lent` and `concurrent` on a parameter, `-> never`, and the refusals.

### How antic builds it

A function in the LLVM text with its linkage per mode and the attributes of each parameter kind, with excerpts.

### What it costs

The copies and the allocations of passing each kind of parameter.

### Pitfalls

Where overloading, the copy of a parameter or a missing `return` behaves other than a Java, C or C++ programmer expects.

## Defaults and source locations

### What you know

Default arguments of C++, the overloads Java writes in their place, and `__FILE__` and `__LINE__` of C.

### The feature

A default value of a parameter, `here` and `SourceLocation`, how a call fills each, and the refusals.

### How antic builds it

How a call fills a default and a `SourceLocation` in the LLVM text, with excerpts.

### What it costs

What a default and `here` cost at run time, and the bytes of a `SourceLocation` by `size_of`.

### Pitfalls

Where a default value is evaluated, and what `here` names, other than a Java, C or C++ programmer expects.

## The calling convention

### What you know

The C calling convention of each platform, which Java hides and C++ follows under `extern "C"`.

### The feature

How a value of each type is passed and returned, the out pointer of a `may fail` function, the two words of a function parameter, and `self`.

### How antic builds it

The signature of a function in the LLVM text and the registers it takes on x86-64 and on ARM64, with excerpts.

### What it costs

The registers and the stack slots of a call, and the extra argument of the error channel.

### Pitfalls

Where a struct, a slice or an error travels other than a Java, C or C++ programmer expects.

## Inlining

### What you know

`inline` of C and C++, the JIT of Java, and what each decides.

### The feature

What decides inlining in each mode. What opt inlines in a release build, the threshold antic passes to it, and what dev mode leaves as calls.

### How antic builds it

One small function in the module of a dev build and of a release build, and in the disassembly of each, with excerpts.

### What it costs

The inline threshold antic passes to opt, and the instructions of a call in dev mode.

### Pitfalls

Where a function a C++ programmer expects inlined stays a call, and the reverse.

## Anonymous functions and closures

### What you know

The lambdas of Java and C++, their captures, and the function pointers of C.

### The feature

`fn(m) { }` with its types from the parameter it fills, capture by reference, `concurrent` parameters, what a closure may change, and the refusals.

### How antic builds it

The context in the frame, the two words of a function parameter, and the code of a closure in the LLVM text, with excerpts.

### What it costs

The allocations of a closure, the words of a parameter of function type, and the words of a `keep` one.

### Pitfalls

Where a capture, a lifetime or a change through a closure behaves other than a Java, C or C++ programmer expects.

## snapshot fn and own fn

### What you know

Capture by copy in a lambda of C++ and the effectively final rule of Java.

### The feature

`snapshot fn` and what it copies, `own fn` at an `own` field or a `keep own` parameter, `dup` of one, who frees it, and the refusals.

### How antic builds it

The heap block of an `own fn` and its two words, and the copy a `snapshot fn` makes, with excerpts.

### What it costs

The heap allocations of an `own fn`, its bytes by `size_of`, and the runtime calls that free it.

### Pitfalls

Where a snapshot is taken, and when an `own fn` is freed, other than a Java, C or C++ programmer expects.

## Function values

### What you know

The function pointers of C, `std::function` of C++ and the functional interfaces of Java.

### The feature

`fn(A) -> R` as a type, `may fail` on it, `?fn(...)`, a plain field or a global of function type, and the refusals.

### How antic builds it

One C function pointer in a field, a global or a result, and two words in a parameter, with excerpts.

### What it costs

The bytes of each form by `size_of`, and the indirect call.

### Pitfalls

Where a function value, a closure in a field, or a `none` function behaves other than a Java, C or C++ programmer expects.
