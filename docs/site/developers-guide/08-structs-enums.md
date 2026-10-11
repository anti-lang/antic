---
title: "Structs and enums"
description: "Structs and their layout, packed and align, unions and bitfields, simd structs, enums, variants with their tag and payload, and pattern matching."
summary: "Structs with C layout, `packed` and `align`, unions and bitfields, simd structs, enums, variants as a tag and a union of their cases, and the `switch` that takes a variant apart, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 80
tags: [compilers, programming-languages]
keywords: [structs, memory layout, packed, alignment, unions, bitfields, simd, enums, variants, pattern matching]
---

## Structs

### What you know

`struct` of C and C++, a class of Java with public fields, and the padding between fields.

### The feature

`struct` with its fields and defaults, a literal, `size_of`, the default `==` and hash, copying, nesting, and the refusals.

### How antic builds it

A struct as an LLVM struct type with C layout, laid out per target by `src/antic/layout.c`, with excerpts.

### What it costs

The bytes of a struct by `size_of` with its padding, and how it passes to a call.

### Pitfalls

Where layout, copying and equality behave other than a Java, C or C++ programmer expects.

## packed and align

### What you know

`#pragma pack` and `__attribute__((aligned))` of C and C++, and nothing of the kind in Java.

### The feature

`packed` before `struct`, `union`, `class` and `variant`, `align(N)` after the name, and the refusals.

### How antic builds it

The packed struct type and the alignment in the LLVM text, with excerpts.

### What it costs

The bytes `packed` saves and the unaligned loads it costs, shown by `size_of` and an excerpt.

### Pitfalls

Where a packed field, its address or its atomic use behaves other than a C or C++ programmer expects.

## Unions and bitfields

### What you know

`union` and the bitfields of C and C++, and their absence in Java.

### The feature

`union` with C layout, a literal that names one field, `flags: u32 : 4`, the zero-width bitfield, and the refusals.

### How antic builds it

A union as its largest member and a bitfield as masks and shifts in the LLVM text, with excerpts.

### What it costs

The bytes of a union and of a bitfield unit by `size_of`, and the shifts per access.

### Pitfalls

Where reading another member of a union, or the unit of a bitfield, behaves other than a C or C++ programmer expects.

## Simd structs

### What you know

The intrinsics of C and C++, the vector API of Java, and the width of a register.

### The feature

`simd struct`, the element-wise operators, the masks, `simd.select`, `simd.any` and `simd.all`, `splat`, `load`, `store`, `shuffle`, `sum`, `min`, `max` and `dot`, the cap, and the refusals.

### How antic builds it

A simd struct as a vector type of LLVM, one `vaddps` at x86-64-v3 and two adds below it, with excerpts per level.

### What it costs

The bytes of a simd struct by `size_of`, the vector register it passes in up to 16 bytes, and the loop above the cap.

### Pitfalls

Where the width, the processor level and the passing of a vector behave other than a C or C++ programmer expects.

## Enums

### What you know

`enum` of C and C++ and the enum class of Java.

### The feature

`enum` with its values, a nested enum, `switch` over one, the enum of the tags of a variant in the header, and the refusals.

### How antic builds it

An enum as an integer of its underlying type in the LLVM text, with an excerpt.

### What it costs

The bytes of an enum by `size_of`, and no call.

### Pitfalls

Where the value, the size and the conversion of an enum behave other than a Java, C or C++ programmer expects.

## Variants

### What you know

A tagged union written by hand in C, `std::variant` of C++, and the sealed classes of Java.

### The feature

`variant` with its cases, a literal, `v.tag`, `v is Shape.Circle`, a variant across a module, a case that owns something, and the refusals.

### How antic builds it

A variant as a struct of a tag and a union of its cases with C layout, and the enum of its tags, with excerpts.

### What it costs

The bytes of a variant by `size_of`, the tag and the largest case, and no allocation.

### Pitfalls

Where the tag, the layout and a case that owns something behave other than a Java, C or C++ programmer expects.

## Pattern matching

### What you know

`switch` on a sealed class in Java, `std::visit` of C++, and a `switch` on the tag in C.

### The feature

`switch` that binds a copy of the fields of a case with `Circle c =>`, the completeness rule without `else`, `if let` on a variant, and the refusals.

### How antic builds it

The tag test and the copies of the fields in the LLVM text, with an excerpt.

### What it costs

One compare per case and the copy of the fields, and no allocation.

### Pitfalls

Where a bound case is a copy, and a missing case is refused, other than a Java, C or C++ programmer expects.
