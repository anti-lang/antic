# Generics in library files and in C

The step builds "Libraries and C" of "Generics" in round five of
`docs/anti-language-additions.md`, the second step of the compilation of
copies that `docs/decisions.md` names. The work order says "as IR with its
parameters open". Eddie's decision of 2026-09-25 replaced that with the
checked tree, and the step follows the decision.

## What was done

- A library file carries every generic of its module with the checked tree
  of each body. `src/antic/antl_tree.c` writes and reads that section. One walker per record serves the collecting, writing and
  reading passes. The type table writes type parameters, generics with their
  parameters and copies as a generic with its arguments. The items carry
  `type` names and constraints. The format version is 62.
- A module that uses a generic of a library makes the copy from that tree,
  under the path of the generic's module. The copies of one generic with the
  same arguments are one type in every module. A dev object holds the copies
  it made as link-once symbols. A module whose program holds a copy already
  uses that one, and reading two files that define a copy keeps the first.
- A type of a library takes its arguments where the module qualifies it,
  `gen.Box<int>`, and a constraint of a library is named by its module,
  `T: gen.Ordered`.
- `export type Name = G<Args>;` writes the copy into the C header as the
  export class or struct `Name`, with its table, init and functions. A
  generic `export fn` was refused already, with the fix of a named copy.
- `anti doc` shows type parameters with their constraints, a `constraint`,
  and a `type` of a copy with a link to its generic. The page from a library
  file equals the page from the source.
- Tests: `generics_modules_release` and `generics_modules_dev` run a program
  and two libraries that share copies, `clib_generics` calls a named copy
  from C, `antl_generic` pins the bytes of a library file with a generic, and
  the fixture of `anti_doc` gained generics.

## What failed and how it was fixed

- `unit` and `antl_scale` pinned version 61 and the old struct entry. The
  expected bytes were written again for version 62.
- A tree carried the type of `none` and the error type on a module name
  before `.`, which the type table refused. The reader admits both.
- `gen.Box<int>` in a type was refused as not generic. The resolver of a
  qualified type now makes the copy.
- The export check read the copy of `export type` before it was marked. The
  mark moved to `sema_resolve_generics`.
- `fmt_canonical` refused the new sources, which `anti fmt` then wrote.
- ASan found `anti_doc` reading the module path of a type after the driver
  freed it. The path now lives in the pool. Log:
  `build/drive/logs/t-asan1.log`.

## Provisional decisions

Six entries under "Generics and collections" in `docs/decisions.md` cover
these. They are what a library file carries and how a body names items
outside its tree. Then the symbols and the merging of copies, the C symbols
of `export type`, the page of `anti doc` and the qualified type. The entry that said a
library file carries no generic now says `Ordered` waits for "Hashing and
order".

## Not built, and questions

- The cache of copies in a dev build. `anti build` keys the library file of a
  module by its own source alone. A changed generic body in a dependency
  then leaves the copies of a cached module stale. That already holds for any
  interface a module reads. Does "cached by the generic's identity and its
  arguments" ask for a cache per copy in `anti build`, and where do those
  objects go?
- A program that holds a copy made under the symbols of its generic and the
  same copy named by `export type` holds two copies. Should `export type` of
  a copy another module compiled be refused instead?
- `gen.Maybe<int>.Some { }`, a literal of a case of a generic variant of
  another module, does not parse. The test builds one through a function.
- The reader checks the counts, indices and enums of a tree, not its shape.
  300 mutated files and every truncation of one failed cleanly under ASan.

## Gates

No compiler warning on the host, ASan and UBSan trees. The linker note about
`-lto_library` stood before. Host 1006 of 1006, ASan 1005 of 1005, UBSan
1005 of 1005, without `no_paths`: `build/drive/logs/t-host-final2.log`,
`t-asan-final2.log` and `t-ubsan-final2.log`. The docs-style checker reports
nothing on every file touched but `tests/CMakeLists.txt`, which it reads as
prose and has always failed on.

## Proof of the push

Taken after the push of the code and the report.

```text
$ git log --oneline -3
d2192ef Report the generics of library files and of C
0429dc6 Record the generics of library files and of C
da5e5b6 Use a generic of a library, offer a copy to C, and document generics
$ git status --short
$ git rev-parse HEAD origin/main
d2192ef901074f44224db7f11b1891ca1b840061
d2192ef901074f44224db7f11b1891ca1b840061
```

Suites: host 1006 of 1006, ASan 1005 of 1005, UBSan 1005 of 1005.
