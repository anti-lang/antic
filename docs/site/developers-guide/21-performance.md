---
title: "Performance"
description: "One table of the cost of every feature of the guide, measuring with tests/bench, --profile-generate and --profile-use, tracing, and which checks a release keeps."
summary: "One table with the cost of every feature of the guide and a link to its section, measuring with `tests/bench`, `--profile-generate` and `--profile-use`, tracing, and which checks a release build keeps and what turning them off gives up."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 210
tags: [compilers, programming-languages]
keywords: [performance, benchmarks, profile-guided optimization, tracing, checks, release build]
---

## The cost of every feature

One table with the cost of every feature of the guide and a link to its section.

## Measuring with tests/bench

The programs of `tests/bench`, their C twins, `tests/run_bench.cmake` and the table it prints.

## --profile-generate and --profile-use

The instrumented program, the raw profile it writes, `llvm-profdata merge`, and the build that reads the profile.

## Tracing

What the handlers of `anti.trace` cost a traced program, measured through `tests/bench`.

## Which checks a release keeps

The checks of dev mode, the ones a release build keeps, the option of each, and what turning it off gives up.
