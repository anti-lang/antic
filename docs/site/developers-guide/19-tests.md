---
title: "Tests"
description: "tests and fixtures blocks, anti test, expected output and memory checks in tests."
summary: "The `tests` and `fixtures` blocks of a module with how antic builds them and what they cost, `anti test`, how a test states the expected output, and `--memory-checks` in tests."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 190
tags: [compilers, programming-languages]
keywords: [unit tests, fixtures, test runner, expected output, memory checks]
---

## tests and fixtures blocks

### What you know

JUnit of Java, the test frameworks of C and C++, and where each keeps a test and what it may reach.

### The feature

`tests { }` and `fixtures { }`, at most one of each per module, `setup` and `teardown`, `assert`, what each sees, `antic --tests`, and the refusals.

### How antic builds it

What `antic --tests` makes of the two blocks, the drop after parsing in every other build, and the runner `anti test` writes, with an excerpt.

### What it costs

What a build without `--tests` holds of the two blocks, and the code of the runner.

### Pitfalls

Where what a test may reach in its module behaves other than a Java, C or C++ programmer expects.

## anti test

What `anti test` compiles, the runner it writes and links, and what it prints for a test that passes and one that fails.

## Expected output

How a test states what a program prints, and how the suite of Anti itself states it.

## Memory checks in tests

`anti test --memory-checks`, what it finds, and the report through the symbolizer.
