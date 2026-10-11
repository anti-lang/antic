---
title: "Hooks"
description: "Hooks and tracing, injection, runtime configuration, plugins, and what --no-hooks and --no-trace remove."
summary: "The nine hooks of `anti.lang.Object` and the tracing built on them, injection and the standard interfaces, the runtime configuration, plugins, and what `--no-hooks` and `--no-trace` remove, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 150
tags: [compilers, programming-languages]
keywords: [hooks, tracing, dependency injection, runtime configuration, plugins, no-hooks, no-trace]
---

## Hooks

### What you know

The finalizer and the agents of Java, and nothing of the kind in C and C++.

### The feature

The nine hooks of `anti.lang.Object`, `created`, `destroyed`, `copied`, `dispatched`, `joined`, `enter`, `leave`, `failed` and `changed`, a class that overrides one, `--no-hooks`, and the refusals.

### How antic builds it

The table entries after the seven functions of `Object` and the call at each site, with excerpts.

### What it costs

The call per site in every build, which `--no-hooks` drops, and what dropping it gives up.

### Pitfalls

Where a hook of a plugin cannot be dropped, other than a Java programmer expects.

## Tracing

### What you know

The profilers and the JVM TI of Java, and `-finstrument-functions` of C and C++.

### The feature

`trace` before `class` and before `fn`, `--trace`, `--no-trace`, `--trace <pattern>`, `--trace writes`, `TraceHandler`, `Trace.install(h)`, the handlers of `anti.trace`, `trace.start`, and the refusals.

### How antic builds it

The handler call and the hook dispatch at a site, reversed on `leave`, with excerpts.

### What it costs

The calls per traced site, what `--no-trace` removes, and the time of a traced loop from `tests/bench`.

### Pitfalls

Where a release build traces nothing unless asked, other than a Java programmer expects.

## Injection

### What you know

The dependency injection frameworks of Java, and the function pointer or the link-time choice of C and C++.

### The feature

`inject log: *Logger`, `inject final`, the tables `[inject]` and `[inject.test]`, `--inject Interface=Provider`, the six standard interfaces with their default providers, and the refusals of the link.

### How antic builds it

One slot per interface, the provider that fills a field before `construct` runs, and `anti_rt_injectable`, with excerpts.

### What it costs

The call through the slot, the bytes of a slot, and the fill per object.

### Pitfalls

Where injection is fixed at start, and `inject final` refuses a replacement, other than a Java programmer expects.

## Runtime configuration

### What you know

The system properties of Java, and the environment variables and `getopt` of C and C++.

### The feature

`--anti.conf=<path>`, `ANTI_CONF`, `rt.configure(path)`, `include`, `[runtime]`, `[injections]`, `--anti.inspect`, `--anti.help`, `rt.get(key)`, the precedence per key, and the refusals.

### How antic builds it

The reader of `src/rt` that parses the TOML at start and the table the pool and `anti.log` read, with an excerpt.

### What it costs

The read at start, and nothing per use of a key after it.

### Pitfalls

Where a file of `rt.configure` may hold no `[injections]`, other than a Java programmer expects.

## Plugins

### What you know

`ServiceLoader` and the class loading of Java, and `dlopen` of C and C++.

### The feature

`provides Interface as Class;`, `antic --lib shared --no-runtime`, `plugin.load(path)`, `instance`, `supports`, `unload`, `anti-plugins.toml`, `--closed`, the version check, and the refusals.

### How antic builds it

The library bound against its host, the chain of structural hashes, the slots and the stub of a slot only reflection reaches, with excerpts.

### What it costs

The call through the table into a plugin, the load, and what the host keeps exported.

### Pitfalls

Where `unload` refuses while an object is alive, and a Linux host links against glibc, other than a Java, C or C++ programmer expects.
