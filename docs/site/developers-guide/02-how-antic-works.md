---
title: "How antic works"
description: "The passes of antic from source to executable, its IR and the LLVM module, dev and release builds, LTO, the runtime archive and how to read the excerpts of this guide."
summary: "The passes from source to executable, the IR of antic and the LLVM module, dev and release builds, whole-program compilation and the LTO of the runtime, the runtime archive and the allocator, what an executable holds beside the program, and how to read and make the excerpts of this guide."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 20
tags: [compilers, programming-languages]
keywords: [compiler passes, intermediate representation, LLVM, link-time optimization, runtime archive, dump options, keep-llvm]
---

## Passes from source to executable

The seven passes in order: the lexer, the parser, the checker, the lowering, the whole-program pass, the LLVM back end and the link. What each reads and what each writes.

## The IR of antic

The form after lowering, which holds no sizes, and the layout per target that follows it in `src/antic/layout.c`.

## The LLVM module

The LLVM text antic writes, how it names a function, and what opt and llc of the pinned tools do with it.

## Dev and release builds

One object per module in dev mode, one module for the whole program in release mode, and the checks each keeps.

## Whole-program compilation and LTO

The pass over the IR of every module and release devirtualisation. The link of the program with the runtime as bitcode through the LTO of lld.

## The runtime archive and the allocator

What `lib/<target>/<level>/` holds, the runtime a program links, and musl with mimalloc on Linux.

## What an executable holds

The code of the program, the runtime, the licence notice, the build id and the tables of the classes, with the bytes of each.

## Reading the excerpts

The marks of an example of this guide, each shown once below, and what the test `guide_examples` checks for each.

A program with a name on its first line is built in release mode and run, and the `text output` block after it holds what it prints.

```anti
//! name: hello
import anti.io;

fn twice(x: int) -> int
{
	return x * 2;
}

fn main() -> int
{
	io.println("hello");
	return twice(21) - 42;
}
```

```text output
hello
```

A block marked `refused` does not compile, and its `//! error:` line names the first error.

```anti refused
//! error: expected `int`, found `str`
let x: int = "a";
```

An `llvm` excerpt holds lines of the LLVM text antic writes for the named block. Here it is the dev build of `twice` for linux-x86_64 with its overflow check. A line of `...` stands for lines left out.

```llvm excerpt=hello target=linux-x86_64 mode=dev
define hidden noundef i64 @hello.twice(i64 noundef %p0) #0 {
...
  %v1 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %v0, i64 2)
  %v2 = mul nsw i64 %v0, 2
  %v3 = extractvalue { i64, i1 } %v1, 1
  store i64 %v2, ptr %t1, align 8
  br i1 %v3, label %b1, label %b2, !prof !3
...
  unreachable
...
  ret i64 %v6
}
```

An `asm` excerpt holds lines of the disassembly of the object, here the release build of `main` for linux-x86_64, where `twice(21) - 42` is the constant 0.

```asm excerpt=hello target=linux-x86_64 mode=release
<hello.main>:
...
xorl %eax, %eax
popq %rbp
retq
```

## Making your own

`--dump-tokens`, `--dump-ast`, `--dump-types`, `--dump-ir`, `--dump-opt`, `--dump-llvm` and `--keep-llvm`, with what each shows and the files the last keeps in each mode.
