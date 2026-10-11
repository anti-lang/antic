---
title: "Targets"
description: "The six targets, --target and --cpu, the processor levels, cross-building, script mode, -g and debugging, the build id and what a release build does with LLVM."
summary: "The six targets, `--target` and `--cpu` with the processor levels, cross-building from any host, script mode, `-g` and debugging, the build id, and what a release build does with LLVM."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 200
tags: [compilers, programming-languages]
keywords: [targets, cross compilation, processor levels, cpu, debugging, build id, LLVM optimization]
---

## The six targets

macos-arm64, macos-x86_64, linux-arm64, linux-x86_64, windows-arm64 and windows-x86_64, and what each links.

## --target and --cpu

The two options, the default level per target, and the runtime per level in `lib/<target>/<level>/`.

## The processor levels

`v1`, `v2` and `v3` on x86_64, `armv8.0`, `armv8.2` and `armv8.5` on ARM64, what each adds, and the check at start.

## Cross-building

A build for any of the six targets from any host, the sysroots of the package, and the frameworks of Apple that a package lacks.

## Script mode

A file with `#!/usr/bin/env anti` and `anti file.anti`, as far as they are built.

## -g and debugging

The line of every statement and the entry per function `-g` writes, lldb and gdb on them, and the variables that are not written yet.

## The build id

The digest of the LLVM text without the notice, where it stands in a program, and how `anti symbols` matches it.

## What a release build does with LLVM

`default<O3>` with the inline threshold, `lto-pre-link<O3>`, and the LTO of lld over the program and the runtime.
