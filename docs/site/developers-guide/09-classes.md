---
title: "Classes"
description: "Classes and the object in memory, construct and destruct, fields and methods, dispatch through interfaces, static fields and singletons, operators, reflection and its tables."
summary: "Classes and the object in memory, `construct` and `destruct`, fields and methods, interfaces as inline sub-objects with thunks and the dispatch through them, static fields and singletons, operators, and reflection over the descriptors, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 90
tags: [compilers, programming-languages]
keywords: [classes, objects, constructors, destructors, virtual dispatch, interfaces, vtable, singletons, operators, reflection]
---

## Classes and the object in memory

### What you know

A Java object with its header, a C++ object with its vtable pointer, and a C struct with function pointers.

### The feature

`class`, `inherits`, `abstract` and `final`, the four visibility levels, a class value and `*Class`, `alloc`, `delete`, and the refusals.

### How antic builds it

The object as a struct whose first word points at the descriptor and the table, and the layout of the fields, with excerpts.

### What it costs

The bytes of an object by `size_of`, the one word of the descriptor pointer, and the heap allocation of `alloc`.

### Pitfalls

Where a class value on the stack, a pointer to one, and the object header behave other than a Java, C or C++ programmer expects.

## construct and destruct

### What you know

The constructors and destructors of C++, the constructor and the finalizer of Java, and the init function of C.

### The feature

`construct`, a `may fail` one, `self.super.construct(args)` as the first statement, `destruct`, the order of teardown, `delete(p)` over what an object owns, and the refusals.

### How antic builds it

The construct chain, the destruct chain and the runtime functions that run them, with excerpts.

### What it costs

The calls per construction and per teardown, and the `created` and `destroyed` hooks.

### Pitfalls

Where the base constructor, a failing constructor and the order of destruction behave other than a Java, C or C++ programmer expects.

## Fields and methods

### What you know

The fields and methods of Java and C++, `this`, and a virtual call against a direct one.

### The feature

Fields with defaults, functions with `self`, `concrete`, the direct call of a `final` function, a nested type, and the refusals.

### How antic builds it

A field as an offset in the struct and a function as a table entry or a direct call, with excerpts.

### What it costs

The load of a field, the direct call and the call through the table, each as an excerpt.

### Pitfalls

Where a call is direct, a field is laid out, and a nested type is named other than a Java, C or C++ programmer expects.

## Interfaces and dispatch

### What you know

The interfaces of Java, the abstract classes and multiple inheritance of C++, and the adjustment of `this`.

### The feature

An interface as an inline sub-object with thunks, `implements`, a call through an interface pointer, release devirtualisation, and the refusals.

### How antic builds it

The table per class, the thunk per interface function, the used-slot bitmaps and the direct call of the whole-program pass, with excerpts in both modes.

### What it costs

The one indirect call of a dispatch, the thunk, the bytes of a table, and the calls a release build turns direct.

### Pitfalls

Where an interface pointer, a thunk and devirtualisation behave other than a Java or C++ programmer expects.

## Static fields and singletons

### What you know

The static fields of Java and C++, the singleton written by hand, and the order of static initialisation.

### The feature

A static field, which is atomic, `singleton` with its one instance and its read-only fields, `mutable`, and the refusals.

### How antic builds it

A static field as a global, the instance of a singleton and the check of the whole-program pass, with excerpts.

### What it costs

The atomic operation per access of a static field, and no allocation for a singleton.

### Pitfalls

Where a static field is atomic and a singleton is read-only, other than a Java or C++ programmer expects.

## Operators

### What you know

The operator overloading of C++, its absence in Java and C, and `equals` and `hashCode` of Java.

### The feature

`operator fn` in a class and at module level, the default `==` and hash, the naming `eq:Point`, `Ordered` and `lt`, and the refusals.

### How antic builds it

An operator as a function call in the LLVM text, and the default `==` and hash as the code `lower_hash.c` writes, with excerpts.

### What it costs

The call per operator, and what the default `==` and hash cost per field.

### Pitfalls

Where `==` on a class value, a missing `<` and an operator at module level behave other than a Java or C++ programmer expects.

## Reflection

### What you know

The reflection of Java, its absence in C and C++, and RTTI.

### The feature

`anti.reflect` with `call`, `Value`, `get`, `set`, `serialize` and `deserialize`, the field records with their type ids, and the refusals.

### How antic builds it

The descriptor and the field records per class, written once per program, and the runtime functions that read them, with excerpts.

### What it costs

The bytes of a descriptor and of a field record, the calls of `reflect.call`, and the slot a release build keeps for it.

### Pitfalls

Where `reflect.call` follows the error convention, and a field `serialize` cannot write keeps its default, other than a Java programmer expects.
