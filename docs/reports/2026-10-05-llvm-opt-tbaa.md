# Step tbaa of the optimization facts

The step `tbaa` of `docs/work-order-llvm-optimization.md`, choice D6 under
condition C2. Eddie decided on 2026-10-05 that the rules of "Aliasing of views"
need no review before this step.

## What the step built

- The first commit corrects choice D6 and the steps `tbaa-rules` and `tbaa` of
  the work order, which said that Eddie accepts the rules first. The additions,
  the entry of the rules in `docs/decisions.md` and the status line of the
  overview said the same and changed with them.
- The `of` of an `IR_LOAD` or an `IR_STORE` holds the type its access reads
  memory as: none, its scalar type, or a struct, a class or a tuple. The new
  `member` of `struct ir_inst` holds the index of the field. `ir_print` shows
  it as `!type`, `ir_verify` refuses it where it does not belong, the optimizer
  drops it where a load becomes a copy, and the library file carries it. The
  format is version 81.
- Lowering types a field by the type whose body declares it, `super` making
  the base for a field of a base. An element, a dereference, a simd lane and a
  variable whose address is taken take their scalar type. An access through a
  view, of bytes, of a lock word, of an aggregate or of a bitfield takes none.
  So does one along a path through a union or a variant. A read through a
  dereference now goes through the place.
- The LLVM text writes a root, bytes below it, a node per scalar type below
  bytes and a node per struct that an access names. Every typed load and store
  carries `!tbaa`. The DESIGN comment above `tbaa_scalar` in
  `src/antic/llvm_emit.c` holds the tree.
- `tests/bench/ablate/opt_no_tbaa.sh` removes `!tbaa` for the measurement.

## Tests

The unit tests failed before their code existed: `type_accesses` in
`unit_ir`, `fills_access_types` in `unit_lower` and `tbaa_text` in
`unit_llvm_emit`. `keeps_access_types` in `unit_modules` came after the reader,
which the build of the standard library drove. The golden
`tests/dump/tbaa.dev.macos-arm64.ll` shows each kind of type and its absence.

One program per rule runs in release mode: `alias_access`, `alias_memory`,
`alias_same`, `alias_integers`, `alias_bytes`, `alias_pointers`,
`alias_structs`, `alias_arrays`, `alias_simd`, `alias_unions`,
`alias_view_local`, `alias_view_passed`, `alias_class_pointers` and
`alias_c_functions` of `tests/programs`. Each writes through two pointers that
its rule lets refer to the same memory, one of which comes from `either`, a
choice on `getenv` that the optimiser cannot fold, and reads the result. They
pin behaviour, so the text without `!tbaa` prints the same.

A wrong fact breaks them. Four mutations of the compiler, each built and run
by hand and then reverted:

| Mutation | Program | Wrong lines |
|---|---|---|
| a type on a path through a union | `alias_unions` | `pun 1`, `deep 3` |
| a type through a view | `alias_view_local` | `bits 1.0000000`, `other 12` |
| bytes as `i16` | `alias_bytes` | all four |
| a struct path on a simd lane | `alias_simd` | `plain 1` |

## Re-pinned values

| Pin | Old | New |
|---|---|---|
| SHA-256 of `emit-identity/programs.sha256` | `684ae58a…c4d9537d`, 1242 lines | `5006244e…6e672d55`, 1326 lines |
| `link-identity/return42.macos-arm64.sha256` | `7b77362a…7378d8ec` | unchanged |

36 goldens of `tests/dump`, the IR texts of `unit_lower` and `unit_modules`, the
version byte of `scale_antl` and of the hex listings `scale.antl.hex` and
`generics/pick.antl.hex` changed. With `!tbaa`, `!type` and the metadata ids
taken out, every removed line of the goldens stands again among the new ones.

## Measurement

`tests/bench/ablate/run.py <program> opt_no_tbaa.sh`, 15 runs each, logs
`build/drive/logs/tbaa-bench-<program>.log`. Before is the build through the
wrapper, whose release compile includes the start of a shell and of sed. Its
objects for `map_work` and `mixed_work` equal those of the step `tbaa-rules`.

Before:

| Program | Anti | C twin | Anti / C | Object | Release compile |
|---|---:|---:|---:|---:|---:|
| `scalar_loop` | 300.4 ms | 295.8 ms | 1.02 | 1,896 B | 44.8 ms |
| `objects` | 142.4 ms | 106.8 ms | 1.33 | 13,448 B | 55.4 ms |
| `builder` | 263.9 ms | 45.6 ms | 5.79 | 15,328 B | 72.3 ms |
| `simd_loop` | 129.6 ms | 125.1 ms | 1.04 | 2,016 B | 44.7 ms |
| `map_work` | 182.9 ms | 138.0 ms | 1.33 | 108,608 B | 321.9 ms |
| `mixed_work` | 193.4 ms | none | | 668,960 B | 2069.1 ms |

After:

| Program | Anti | C twin | Anti / C | Object | Release compile |
|---|---:|---:|---:|---:|---:|
| `scalar_loop` | 300.3 ms | 295.8 ms | 1.02 | 1,896 B | 41.6 ms |
| `objects` | 141.3 ms | 106.8 ms | 1.32 | 13,448 B | 51.6 ms |
| `builder` | 266.0 ms | 45.6 ms | 5.83 | 15,328 B | 68.1 ms |
| `simd_loop` | 129.7 ms | 125.1 ms | 1.04 | 2,016 B | 41.3 ms |
| `map_work` | 182.7 ms | 138.0 ms | 1.32 | 108,592 B | 309.5 ms |
| `mixed_work` | 193.3 ms | none | | 668,352 B | 2019.5 ms |

Every build printed the line of its C twin, and `mixed_work` its baseline.
Every median lies within 2 percent of the one before it.

## Provisional entries

Under "Compiler behaviour" in `docs/decisions.md`, after the entry of the rules:

- The `!tbaa` metadata stands on the rules of "Aliasing of views". Where they
  leave the form open, the text gives fewer facts than they allow: one node
  for both signs of a width, an enum and its base, `char` and `u32`, `c_long`
  and its width, `f16` and `i16`, and every pointer. A field names the type
  that declares it at its offset there, and the struct node lists its scalar
  fields alone. A lane, an element, a `*p` and the parts of a `str` or a slice
  take their scalar type alone. `as` between classes of one chain is a view to
  the checker, so its accesses take no type.

## Questions

- `sema_value_view` counts `as` between two classes of one chain as a view, so
  an access through such a local takes no type. That gives fewer facts than the
  rule "Class pointers" allows. Should the checker stop counting it as a view?
  That would change `inbounds` as well.
- "Memory holds a type" lets a store give allocator memory a new type. LLVM
  keeps that only where it can relate the two stores, as clang does for the
  effective types of C. No program of the suite reaches the case. Memory that
  `free` hands back to an allocator of the program may take another type.
  Should a release build give no type to the accesses that reach it?

## Gates

- Build: zero warnings in `host`, `asan` and `ubsan`,
  `build/drive/logs/tbaa-{host,asan,ubsan}-build.log`.
- Host suite: 1598 of 1598, 222 s, `build/drive/logs/tbaa-host-suite.log`.
- ASan suite: 1597 of 1597, 441 s, `build/drive/logs/tbaa-asan-suite.log`.
- UBSan suite: 1597 of 1597, 303 s, `build/drive/logs/tbaa-ubsan-suite.log`.
- `emit_identity`, `link_identity_macos-arm64` and `llvm_verify_<target>` for
  all six targets passed in the host suite.
- The docs-style checker reports nothing on the documents of the step and on
  this report, `build/drive/logs/tbaa-docs{1,2,3,-report}.log`. The work order
  keeps the warning on its line 3 that it had before.

State after the push of the step, before this report:

```console
$ git log --oneline -3
f6e537e7 Add the wrapper that removes the !tbaa of the step tbaa
64344540 Write the aliasing rules of views as !tbaa
d43cbb9d Say that the aliasing rules need no review before the step tbaa
$ git status --short
$ git rev-parse HEAD origin/main
f6e537e76efead3ed2fa776f3146a9fe7319764c
f6e537e76efead3ed2fa776f3146a9fe7319764c
```
