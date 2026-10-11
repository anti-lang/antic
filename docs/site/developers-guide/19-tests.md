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

JUnit of Java, the test frameworks of C and C++, and a test that sees private members.

### The feature

`tests { }` and `fixtures { }`, one of each per module, `setup` and `teardown`, `assert`, what each sees, `antic --tests`, and the refusals.

### How antic builds it

The functions of a block made public under `--tests` and dropped after parsing otherwise, and the runner `anti test` writes, with an excerpt.

### What it costs

Nothing in a build without `--tests`, and the code of the runner.

### Pitfalls

Where a test sees the private items of its module, other than a Java, C or C++ programmer expects.

## anti test

What `anti test` compiles, the runner it writes and links, and what it prints for a test that passes and one that fails.

## Expected output

How a test states what a program prints, and how the suite of Anti itself states it.

## Memory checks in tests

`anti test --memory-checks`, what it finds, and the report through the symbolizer.
