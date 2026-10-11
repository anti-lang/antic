---
title: "Collections"
description: "List, Deque, Ring, Grid, Map, HashMap, Set, HashSet, BitSet, the sorted ones, Pool, Tree and PriorityQueue with their layout, their growth and the cost of each operation."
summary: "The collections of `anti.collection` with their layout and their growth, walking, hashing and its seed, order and equality, and one table with the cost of each operation, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 160
tags: [compilers, programming-languages]
keywords: [collections, List, Deque, Map, HashMap, Set, SortedMap, Pool, Tree, PriorityQueue, iteration, hashing]
---

## List and Deque

### What you know

`ArrayList` and `ArrayDeque` of Java, `std::vector` and `std::deque` of C++, and a growing array in C.

### The feature

`List<T>` and `Deque<T>` of `anti.collection`, how each is made with an optional allocator, their operations, `dup`, the refusal of `=`, and the other refusals.

### How antic builds it

The pointer, the length and the capacity of a `List<T>`, the ring of a `Deque<T>`, and the growth of each, with excerpts.

### What it costs

The bytes of each by `size_of`, the allocation per growth, and the cost of each operation.

### Pitfalls

Where a pointer into a `List` kept across a push is wrong, other than a Java or C++ programmer expects.

## Ring and Grid

### What you know

A circular buffer and a two-dimensional array written by hand in Java, C and C++.

### The feature

`Ring<T, N>` with its fixed size and `Grid<T>` with its two dimensions, their operations, and the refusals.

### How antic builds it

The array of `N` elements of a `Ring<T, N>` and the one block of a `Grid<T>`, with excerpts.

### What it costs

The bytes of each by `size_of`, no allocation for a `Ring`, one for a `Grid`, and the cost of each operation.

### Pitfalls

Where a full `Ring` drops or refuses, other than a Java or C++ programmer expects.

## Map and HashMap

### What you know

`HashMap` of Java, `std::unordered_map` of C++, and a hash table written by hand in C.

### The feature

`Map<K, V>` and `HashMap<K, V>`, what each needs of `K`, their operations, `dup`, and the refusals.

### How antic builds it

The table of each, its probing or its buckets, the growth, and the `hash` hook it calls, with excerpts.

### What it costs

The bytes of each by `size_of`, the allocation per growth, the hash call per operation, and the cost of each operation.

### Pitfalls

Where a key needs `hash` and `eq`, and a walk follows no insertion order, other than a Java or C++ programmer expects.

## Set, HashSet and BitSet

### What you know

`HashSet` and `BitSet` of Java, and `std::unordered_set` and `std::bitset` of C++.

### The feature

`Set<T>`, `HashSet<T>` and `BitSet`, their operations, and the refusals.

### How antic builds it

The table of a `Set<T>` and the words of a `BitSet`, with excerpts.

### What it costs

The bytes of each by `size_of`, the allocations, and the cost of each operation.

### Pitfalls

Where a `BitSet` holds integers alone, other than a Java or C++ programmer expects.

## SortedMap and SortedSet

### What you know

`TreeMap` and `TreeSet` of Java, and `std::map` and `std::set` of C++.

### The feature

`SortedMap<K, V>` and `SortedSet<T>`, the `lt` they need, their operations in key order, and the refusals.

### How antic builds it

The tree of each and the `lt` hook it calls, with excerpts.

### What it costs

The bytes of a node, the allocation per insert, and the cost of each operation.

### Pitfalls

Where a key type needs `operator fn lt`, other than a Java or C++ programmer expects.

## Pool and Handle

### What you know

An object pool and a generational index written by hand in Java, C and C++.

### The feature

`Pool<T>` with `Handle<T>`, what a handle of a freed slot gives, the operations, and the refusals.

### How antic builds it

The slots of a `Pool<T>` and the index and the generation of a `Handle<T>`, with excerpts.

### What it costs

The bytes of a handle and of a slot by `size_of`, and the cost of each operation.

### Pitfalls

Where a stale handle is refused at run time, other than a C or C++ programmer expects.

## Tree and PriorityQueue

### What you know

`PriorityQueue` of Java, `std::priority_queue` of C++, and a tree of nodes written by hand.

### The feature

`Tree<T>` with its nodes and `PriorityQueue<T>` with its order, the operations, and the refusals.

### How antic builds it

The nodes of a `Tree<T>` and the heap of a `PriorityQueue<T>`, with excerpts.

### What it costs

The bytes of a node, the allocations, and the cost of each operation.

### Pitfalls

Where the top of a `PriorityQueue` is the least by `lt`, other than a Java or C++ programmer expects.

## Walking a collection

### What you know

`Iterator` of Java, the iterators of C++, and the index loop of C.

### The feature

`for x in e` over a collection, `for x in &e`, `iter`, `next` and `value`, `to_slice`, `Changes` and `Watch` in dev mode, and the refusals.

### How antic builds it

The hook calls per turn and the `Watch` check of dev mode in the LLVM text, with excerpts.

### What it costs

The calls per turn, the copy of a plain `for x`, and the check of dev mode.

### Pitfalls

Where a change during a walk traps in dev mode, other than a Java or C++ programmer expects.

## Hashing and its seed

### What you know

`hashCode` of Java, `std::hash` of C++, and a hash function chosen by hand in C.

### The feature

`x.hash()` on every type, the default over the parts, `operator fn hash`, the seed the runtime chooses at start, `lang.hash_seed()`, `--anti.hash_seed`, and the refusals.

### How antic builds it

The default hash `lower_hash.c` writes, the constants of `src/rt/hash.h`, and the seed read at start, with excerpts.

### What it costs

The calls per hash, and what hashing a struct of each field kind costs.

### Pitfalls

Where the seed changes per run, other than a Java or C++ programmer expects.

## Order and equality

### What you know

`compareTo` and `equals` of Java, `operator<` and `operator==` of C++, and `memcmp` of C.

### The feature

The default `==` part by part, no default `<`, `Ordered` with `operator fn lt`, `eq:Point` at module level, and the refusals.

### How antic builds it

The compare per part of the default `==` in the LLVM text, with an excerpt.

### What it costs

The compares per part, and the call per `lt`.

### Pitfalls

Where `<` has no default, and `==` on a channel compares identity, other than a Java or C++ programmer expects.

## The cost of each operation

One table with the cost of every operation of every collection of `anti.collection`, measured through `tests/bench`.
