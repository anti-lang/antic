---
title: "Flow"
description: "if and while, for over ranges and collections, switch on values and what each compiles to, break and continue, defer and undo."
summary: "`if` and `while`, `for` over a range and over a collection, `switch` on an integer, a `str` and a variant with what each compiles to, `break` and `continue`, `defer` and `undo`, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 40
tags: [compilers, programming-languages]
keywords: [control flow, if, while, for loop, switch, break, continue, defer, undo]
---

## if and while

### What you know

`if` and `while` in Java, C and C++, their conditions, and what counts as true in each.

### The feature

`if`, `else`, `while`, `do`, a condition that is a `bool` or a checked pointer, narrowing in a branch, and the refusals.

### How antic builds it

The basic blocks and the `br` of an `if` and the loop of a `while` in the LLVM text, with excerpts.

### What it costs

No call and no allocation for a branch or a loop, and what a condition with narrowing adds.

### Pitfalls

Where a condition, an assignment in one, or narrowing behaves other than a Java, C or C++ programmer expects.

## for over ranges and collections

### What you know

The three-clause `for` of C, the enhanced `for` of Java and the range `for` of C++.

### The feature

`for i in 0..10`, `by`, a reversed range, `for x in e` over a slice, an array and a collection, `for x in &e`, `for i, x in items`, and the refusals.

### How antic builds it

A range loop as a counter and a compare, a collection loop as calls of `iter`, `next` and `value`, and the `Watch` of dev mode, with excerpts.

### What it costs

No call for a range, the hook calls per turn of a collection loop and the copy a plain `for x` makes. The check of a changed collection in dev mode.

### Pitfalls

Where a range bound, `by`, the copy per turn or a change inside the loop behaves other than a Java, C or C++ programmer expects.

## switch

### What you know

`switch` in Java, C and C++, fallthrough, and `switch` on a string in Java.

### The feature

`switch` on an integer, an enum, a `str` and a variant, `fallthrough`, `else`, the completeness rule, `x in lo..hi`, and the refusals.

### How antic builds it

The `switch` instruction of LLVM for an integer, the chain of `anti.text.equal` calls for a `str`, and the tag test of a variant, with excerpts.

### What it costs

The jump table or the compare chain llc chooses, the calls of a `str` switch, and no allocation.

### Pitfalls

Where fallthrough, completeness and a `str` case behave other than a Java, C or C++ programmer expects.

## break and continue

### What you know

`break`, `continue` and the labelled jumps of Java, C and C++.

### The feature

`break` and `continue` in every loop and in a `switch`, what each leaves, how each meets `defer`, and the refusals.

### How antic builds it

The branches the two become in the LLVM text and the `defer` bodies that run on the way out, with an excerpt.

### What it costs

One branch each, and the deferred code they run.

### Pitfalls

Where `break` inside a `switch` in a loop, or a `defer` on the way out, behaves other than a Java, C or C++ programmer expects.

## defer and undo

### What you know

`finally` of Java, the destructor of C++ and the `goto cleanup` of C.

### The feature

`defer` at every exit of a block, `undo` on the fail path alone, their order, what each may name, and the refusals.

### How antic builds it

The copies of a deferred body at each exit in the LLVM text and the fail path that runs an `undo`, with excerpts.

### What it costs

The code of a body copied per exit, no call and no allocation.

### Pitfalls

Where the order of deferred bodies, a variable they read, or `undo` on success behaves other than a Java, C or C++ programmer expects.
