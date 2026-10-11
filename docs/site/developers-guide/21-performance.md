---
title: "Performance"
description: "One table of the cost of every feature of the guide, measuring with tests/bench, --profile-generate and --profile-use, tracing, and the checks a release keeps."
summary: "One table with the cost of every feature of the guide and a link to its section, measuring with `tests/bench`, `--profile-generate` and `--profile-use`, tracing, and which checks a release build keeps and what turning them off gives up."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 210
tags: [compilers, programming-languages]
keywords: [performance, benchmarks, profile-guided optimization, tracing, checks, release build]
---

## The cost of every feature

One table with a row per section of the guide, which holds its bytes, allocations, calls and checks and a link to the section.

## Measuring with tests/bench

The programs of `tests/bench`, their C twins, `tests/run_bench.cmake` and the table it prints.

## --profile-generate and --profile-use

The instrumented program, the raw profile it writes, `llvm-profdata merge`, and the build that reads the profile.

## Tracing

What the handlers of `anti.trace` cost a traced program, measured through `tests/bench`.

## Checks of a release build

Each check of dev mode, whether release mode keeps it, its option, and what turning it off gives up.
