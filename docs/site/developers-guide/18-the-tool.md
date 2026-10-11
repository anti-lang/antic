---
title: "The tool"
description: "anti.toml, dependencies and the lock file, anti build and its cache, anti check, anti fmt, anti doc, anti license and anti symbols."
summary: "The manifest `anti.toml`, dependencies and `anti.lock`, `anti build` and its cache, `anti check`, `anti fmt`, `anti doc`, `anti license` and `anti symbols`."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T08:10:24+02:00
draft: true
weight: 180
tags: [compilers, programming-languages]
keywords: [build tool, manifest, lock file, dependency resolution, build cache, formatter, documentation generator, licence notice, symbols]
---

## anti.toml

The manifest, its tables and keys, and what `anti new` writes.

## Dependencies and the lock file

The constraints, the resolution, `anti.lock`, the repositories, and the cache of the user.

## anti build and its cache

One object per module in dev mode, cached by the digest of its input, the compiler version, the target and the level. The one call of release mode.

## anti check

The four classes in order, the first failing class that ends the run, and `--targets all`.

## anti fmt

The canonical form of the formatter rules and what the formatter changes.

## anti doc

One page per module and the index, `--markdown`, `--dev` and `--private`.

## anti license

The five forms, and the `NOTICE.txt` that `anti build` writes.

## anti symbols

`inventory`, `check` and `resolve` over the symbols archives of a program and its libraries, matched by the build id.
