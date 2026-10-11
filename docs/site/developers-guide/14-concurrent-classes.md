---
title: "Concurrent classes"
description: "Synchronized and concurrent classes, the hidden lock, guarded fields, sync blocks, the lock order checks of dev mode, and the thread-safe collections."
summary: "`synchronized class` with its hidden lock, `concurrent class` with its guarded fields, `sync` blocks, the lock order checks of dev mode and the thread-safe collections of `anti.collection`, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 140
tags: [compilers, programming-languages]
keywords: [synchronized class, concurrent class, hidden lock, guarded fields, sync block, lock order, thread-safe collections]
---

## Synchronized classes

### What you know

The `synchronized` methods of Java, a class with a mutex member in C++, and a struct with a mutex in C.

### The feature

`synchronized class`, and the hidden lock every function with `self` that is not private takes, taken again by its thread without waiting. No pointer into the fields from a public function, and the refusals.

### How antic builds it

The hidden lock in the object, and the lock and unlock at the entry and at every exit of a function, with excerpts.

### What it costs

The bytes of the hidden lock by `size_of`, the runtime calls per function, and the default `==` and hash under the lock.

### Pitfalls

Where a private function takes no lock, and a pointer into the fields is refused, other than a Java or C++ programmer expects.

## Concurrent classes

### What you know

The fine-grained locking and `volatile` of Java, the `std::atomic` members of C++, and the data race of C.

### The feature

`concurrent class`, every field guarded by a Mutex, atomic or fixed, the safety check `unguarded-field`, `unchecked`, no default `==` and hash, and the refusals.

### How antic builds it

The lock per guarded field and the atomic instruction per atomic field in the LLVM text, with excerpts.

### What it costs

The bytes of a guarded field, the lock per access, and the atomic operation per atomic field.

### Pitfalls

Where a fixed field, and a pointer into the fields, behave other than a Java or C++ programmer expects.

## sync blocks

### What you know

The `synchronized` block of Java, `std::lock_guard` of C++, and `pthread_mutex_lock` of C.

### The feature

`sync obj { }` on a synchronized object, `sync a, b { }` in the order of the addresses, a nested `sync` on the same place, and the refusals.

### How antic builds it

The lock and unlock around the block at every exit, and the compare of two addresses, with excerpts.

### What it costs

The runtime calls per block, and the compare of two addresses.

### Pitfalls

Where two locks take the order of their addresses, other than a Java or C++ programmer expects.

## Lock order checks

### What you know

The deadlock detection of a Java thread dump, and the absence of a check in C and C++.

### The feature

The report of dev mode on two locks taken in opposite orders, what it prints, and the check in release mode.

### How antic builds it

The runtime functions that record the order of locks in dev mode, with an excerpt.

### What it costs

The calls per lock in dev mode, and what release mode keeps of them.

### Pitfalls

Where the check reports an order that has not deadlocked yet, other than a Java or C++ programmer expects.

## Thread-safe collections

### What you know

`ConcurrentHashMap` of Java, and the absence of one in C++ and C.

### The feature

`SyncList<T>`, `SyncMap<K, V>`, `SyncSet<T>` and `SyncPool<T>` of `anti.collection.synchronized`, `ConcurrentMap<K, V>` and `SpscRing<T, N>` of `anti.collection.concurrent`, and the refusals.

### How antic builds it

The hidden lock of each synchronized collection and the atomics of the concurrent ones, with excerpts.

### What it costs

The lock per operation of a synchronized collection, the atomic operations of a concurrent one, and the bytes of each by `size_of`.

### Pitfalls

Where a walk of a synchronized collection and its lock behave other than a Java programmer expects.
