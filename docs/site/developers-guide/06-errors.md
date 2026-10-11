---
title: "Errors"
description: "may fail, fail, try and handlers, how an error travels back, error origins and stack traces and their cost, never, assert and the checks of dev mode."
summary: "`may fail` and `fail`, `try` and the handlers of `catch`, the out pointer an error travels back through, error origins and stack traces with their cost, `-> never`, `assert` and the checks of dev mode, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 60
tags: [compilers, programming-languages]
keywords: [error handling, may fail, try, catch, stack trace, never, assert, checks]
---

## may fail and fail

### What you know

The exceptions of Java and C++, and the return codes and `errno` of C.

### The feature

`may fail` on a function, `fail` with an error or a text, the `Error` class, `fail e` from a handler, the out pointer of the result, and the refusals.

### How antic builds it

The `?*Error` result and the out pointer in the LLVM text, and how an error travels back through each frame, with excerpts.

### What it costs

One pointer compare per call of a failing function, the heap allocation of an error, and nothing more on the success path.

### Pitfalls

Where a function fails by its marking alone, and an error is a value, other than a Java, C or C++ programmer expects.

## try and handlers

### What you know

`try`, `catch` and `finally` of Java and C++, and the `if (rc != 0)` of C.

### The feature

`try` that forwards, `catch e { }` with its guards and `catch none`, the exits of a handler that delete the error, and the refusals.

### How antic builds it

The branch after a call and the delete at every exit of a handler in the LLVM text, with excerpts.

### What it costs

The branch per `try`, the delete per handler, and no table and no unwinder.

### Pitfalls

Where `try` forwards, a handler ends, or an error is deleted other than a Java, C or C++ programmer expects.

## Error origins and stack traces

### What you know

The stack trace of a Java exception, and what C and C++ give in its place.

### The feature

`at` and `frames` of an error, `e.text()`, `StackTrace` with `capture`, `frames`, `text` and `symbolize`, backtraces in dev mode and `--anti.backtrace` in release mode.

### How antic builds it

The runtime functions that record an origin and walk the frames, and the symbols archive that names them, with an excerpt.

### What it costs

The runtime calls of the first `fail`, the frames captured in dev mode, and the option that turns them on in release mode.

### Pitfalls

Where a trace is captured, and what it costs when backtraces are on, other than a Java, C or C++ programmer expects.

## never

### What you know

`noreturn` of C and C++, and a `throw` at the end of a Java method.

### The feature

`-> never`, how a call of such a function ends its path, `Error.fatal`, and the refusals.

### How antic builds it

`IR_UNREACHABLE` after the call in the IR and `noreturn cold` in the LLVM text, with an excerpt.

### What it costs

Nothing beyond the call, and the code opt removes after it.

### Pitfalls

Where a `never` function in a condition or a `switch` behaves other than a C or C++ programmer expects.

## assert and the dev checks

### What you know

`assert` of Java and C, and the checks each turns off for production.

### The feature

`assert`, the checks of dev mode, `--checks`, `--no-checks`, `--asserts` and `--no-asserts`, and what a failed check prints.

### How antic builds it

The compare and the call of `anti_rt_check_failed` of a check in the LLVM text of dev mode, and its absence in release mode, with excerpts.

### What it costs

The branch per check in dev mode, nothing in release mode, and what turning each off gives up.

### Pitfalls

Where an assertion with a side effect, or a check off in release mode, behaves other than a Java, C or C++ programmer expects.
