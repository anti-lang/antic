---
title: "Optionals"
description: "Nullable pointers, ?T and its layout, if let, ?? and ?., and the refusals of the checker."
summary: "`*T` that never holds `none` and `?*T` that may, `?T` of any type with its layout, `if let`, `let ... else`, `??` and `?.`, and what the checker refuses, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 70
tags: [compilers, programming-languages]
keywords: [nullable pointers, optional values, null safety, narrowing, if let]
---

## Nullable pointers

### What you know

`null` of Java, `NULL` of C, `nullptr` of C++, and the dereference that fails at run time.

### The feature

`*T` that never holds `none`, `?*T` that may, narrowing per block that follows `&&` and `||`, every pointer of an `extern fn` as `?*T`, and the refusals.

### How antic builds it

One pointer for both types in the LLVM text and the test of `none` as a compare against null, with excerpts.

### What it costs

Eight bytes for `*T` and `?*T` alike by `size_of`, and the compare per test.

### Pitfalls

Where a pointer that cannot be null, and a narrowing that ends with its block, behave other than a Java, C or C++ programmer expects.

## Optional values

### What you know

`Optional` of Java, `std::optional` of C++, and the sentinel value of C.

### The feature

`?T` of any type, `none`, `?*T` and `?fn(...)` as one pointer, a `?T` of a type parameter whose argument is a pointer, and the refusals.

### How antic builds it

A `?T` as the value and one flag byte with C layout, and `struct anti_opt_<T>` in the header, with excerpts.

### What it costs

The bytes of a `?T` by `size_of`, its padding, and no allocation.

### Pitfalls

Where a `?T` of a pointer is one pointer, and a `?T` of a struct exceeds the struct, other than a Java, C or C++ programmer expects.

## Reading an optional

### What you know

`Optional.get` and `orElse` of Java, `value_or` of C++, and the `if (p)` of C.

### The feature

`if let`, `let m = p else { }`, `p ?? q`, `p?.x`, `p?.f(args)`, the guards of `catch`, and the refusals.

### How antic builds it

The flag test and the branch of each form in the LLVM text, with excerpts.

### What it costs

One compare per form, and no call.

### Pitfalls

Where `??`, `?.` and `if let` behave other than a Java, C or C++ programmer expects.
