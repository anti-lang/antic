---
title: "C interop"
description: "extern fn, the C types, the ABI of a call, anti bind, link, libraries for C with --lib and --bundle-runtime, and plugins across the boundary."
summary: "`extern fn` and the C types, the ABI of a call, `anti bind`, `link`, libraries for C with `--lib` and `--bundle-runtime`, and plugins across the boundary, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 170
tags: [compilers, programming-languages]
keywords: [foreign function interface, extern, C ABI, bindings, link, static library, shared library, plugins]
---

## extern fn and C types

### What you know

JNI and the foreign function API of Java, the direct call of C, and `extern "C"` of C++.

### The feature

`extern fn`, the C types `c_int` and the rest, every pointer of an `extern fn` as `?*T`, a `str` and a slice at the boundary, and the refusals.

### How antic builds it

The declaration in the LLVM text and the conversion of each type at the call, with excerpts.

### What it costs

Nothing beyond the call, and the conversion of a `str` to a C string.

### Pitfalls

Where a pointer from C is nullable, and a `str` has no terminator, other than a Java, C or C++ programmer expects.

## The ABI of a call

### What you know

The System V and the Microsoft calling conventions, which Java hides and C and C++ follow.

### The feature

How each type of Anti matches a C type in the header, `struct anti_opt_<T>`, and the tuples of an exported signature. A simd struct of 16 bytes as a vector type, and `may fail` in C.

### How antic builds it

The header `anti bind --header` writes and the signature in the LLVM text, with excerpts.

### What it costs

Nothing beyond a C call, and the out pointer of a failing function.

### Pitfalls

Where a struct passes on Windows and on ARM64 other than a C programmer expects.

## anti bind

### What you know

The binding generators of Java, and the header a C or C++ program includes.

### The feature

`anti bind raylib_api.json`, `anti bind --clang <header>`, `--probe`, the shim of the inline functions, `anti bind --header <name>.antl`, and the refusals.

### How antic builds it

The JSON of `-Xclang -ast-dump=json` read by the scanner of `src/rt/json.c`, and the module it writes, with an excerpt.

### What it costs

Nothing at run time, and the shim the inline functions cost.

### Pitfalls

Where a macro, an inline function or a variadic function needs a shim, other than a C programmer expects.

## link

### What you know

The link line of C and C++, and `System.loadLibrary` of Java.

### The feature

`link framework "Name";`, `link linux "X11";`, the glibc link of a Linux program that reaches one, `--linux-lib`, `--framework`, and the refusals.

### How antic builds it

What lld receives for each form, and the dynamic link against glibc 2.35, with an excerpt.

### What it costs

The dynamic dependency a program takes on, and nothing per call.

### Pitfalls

Where a `link linux` line moves the Linux link from musl to glibc, other than a C programmer expects.

## Libraries for C

### What you know

A static and a shared library of C and C++, and the JAR of Java.

### The feature

`--lib static|shared`, `--bundle-runtime`, `--soname`, the header, `anti_<Class>_construct`, the licence notice of a library, and the refusals.

### How antic builds it

The archive with the library object, the runtime members and the marker member, and the duplicate symbol `anti_rt_bundle` of two bundles, with an excerpt.

### What it costs

The bytes a bundled runtime adds to a library, and nothing per call.

### Pitfalls

Where two bundled libraries in one program are refused, other than a C or C++ programmer expects.

## Plugins across the boundary

### What you know

`dlopen` of C and C++, and the class loader of Java.

### The feature

A plugin that C loads, a host written in C, `anti_rt_slots`, the `.lib` of a Windows host, and the refusals.

### How antic builds it

The exports of a host and the bindings of a plugin, with an excerpt.

### What it costs

What a host exports, and the call through the table.

### Pitfalls

Where a Windows plugin links against the import library of its host, other than a C programmer expects.
