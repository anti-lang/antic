---
title: "Generics"
description: "Generic functions and classes, constraints, one copy per use, what can be generic and the size of the code."
summary: "Generic functions and classes with their type parameters, constraints checked where the generic is written and at each use, one compiled copy per use, what can be generic, and the size of the code, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 100
tags: [compilers, programming-languages]
keywords: [generics, type parameters, constraints, monomorphization, templates, code size]
---

## Generic functions and classes

### What you know

The generics of Java with erasure, the templates of C++, and the macro or `void *` of C.

### The feature

Type parameters in `<>` on functions, structs, classes, variants and interfaces, `N: int`, type arguments, the rule of C# in an expression, `>>`, and the refusals.

### How antic builds it

The checked tree of a generic in the library file, and the copy with the arguments in place, with an excerpt of `max<int>`.

### What it costs

The run-time code of a generic against the same code written by hand, and the size of a copy.

### Pitfalls

Where a generic is checked where it is written, and a copy is made per use, other than a Java or C++ programmer expects.

## Constraints

### What you know

The bounded types of Java, the concepts of C++20, and the unchecked template body before them.

### The feature

A constraint after `:`, hooks, interfaces, `+`, the items `constraint` and `type`, `Number` and `Ordered` of `anti.lang`, a parameter without constraints, and the refusals.

### How antic builds it

The check of a body against its constraints and of each use where it stands, and what reaches lowering, with an excerpt.

### What it costs

The run-time code a constraint adds, and the work of the checker.

### Pitfalls

Where an error of a generic lands, and what a parameter without constraints may do, other than a Java or C++ programmer expects.

## Copies per use

### What you know

The one erased class of Java, one instantiation per argument list of C++, and the code size each gives.

### The feature

One copy per use with concrete arguments, `max<int>` and `List<int>.push`, the copy each dev object carries, merged identical copies in release mode, and `export type`.

### How antic builds it

The names of the copies in the LLVM text, their linkage in a dev object and the merge of release mode, with excerpts.

### What it costs

The bytes of code per copy, shown by the symbols of a program with two uses, and the merge of identical copies.

### Pitfalls

Where the symbol of a copy escapes the brackets, and two uses share code or not, other than a Java or C++ programmer expects.

## Generic declaration forms

### What you know

The generic methods and nested generics of Java and C++, and their limits.

### The feature

A function of a class with type parameters of its own and a type nested in a generic class. Generic synchronized, concurrent and worker classes, and what a library file carries.

### How antic builds it

The copies these forms compile from the source of the module, with an excerpt.

### What it costs

The copies each form makes, counted.

### Pitfalls

Where a form compiles from the source alone and not from a library file, other than a Java or C++ programmer expects.
