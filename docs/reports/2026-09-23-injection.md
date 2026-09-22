# Injection

"Injection" of `docs/anti-language-additions.md` is built: the `inject` and
`inject final` field markers, the `[inject]` and `[inject.test]` tables of the
manifest passed as `--inject Interface=Provider`, one slot per interface that
holds the provider and that every site calls through, and the link-time errors,
a missing provider and a cycle among them. The development Mac passes 751 of 751
tests, and the ASan and UBSan builds pass 750 each, without `no_paths`. The build
has no warnings. The VMs did not run.

## What was done

1. `8ef669c`. The parser reads `inject` and `inject final` as contextual words
   before the name of a field of a class body. The checker refuses a type that is
   no `*Interface` of an abstract class, a default beside the marker, `own` on
   the field and a literal that names it, and a literal that leaves it out is
   complete. Lowering writes one slot per interface and fills the field by
   calling through it, before `construct` runs. The class record carries the
   `inject` fields its class declares, and a subtable carries the aggregate and
   the field of its sub-object, so the pass over the whole program converts a
   pointer to a class into a pointer to an interface without the types. The
   library file carries both, and the format version rose to 48. The pass
   resolves `--inject`, writes the slot, and refuses a missing provider, a
   provider that is no function of the program, one that takes arguments or
   gives no pointer, one whose class is no such interface, and a cycle.
2. `d69c08e`. `anti_rt_injectable` names every injectable interface of a
   program, with the class and the field that need it and whether it is final.
   `rt/conf.c` reads it, so a `[injections]` line and a `--anti.inject` report
   what the program does have, refuse an `inject final` field and say that this
   build loads no plugin. `--anti.inspect` lists the interfaces.
3. `4cffc8f`. `anti test` reads `anti.toml` of the directory it runs in with the
   runtime's TOML reader, lays `[inject.test]` over `[inject]` per interface and
   passes each provider to antic. `tests/inject` holds a program that injects
   three interfaces, and `tests/inject/project` drives `anti test` over a
   manifest in both modes. Six error listings cover the checker and the link.
4. `17203d4`. `docs/decisions.md` gained an "Injection" section,
   `docs/notes/injection.md` the choices inside the passes, and the "Built" and
   "Not built yet" lines of the syntax overview what the step finished.
   `docs/tooling.md` documents the two manifest tables. `alloc` and `free` name
   a field after `inject`, which is the position the example of the overview
   writes.
5. `9db7491`. A provider path that names no function of the program names a
   class, and `get` is appended to it. That is the second form the specification
   gives a provider, a class name whose `get` is a singleton's.
6. `dfb1924`. The comment of the manifest reader moved into the DESIGN comment
   of the `anti` target. The docs-style checker reads the line after a comment
   as a part of it.

## What failed and how it was fixed

- The slot began in a module of its own, `anti.inject`. A dev build drops the
  data of every module but the one it compiles and the runtime module. The
  object that links then held no definition, and `anti test` failed at the
  link. The slot moved to the runtime module under the name `inject.<path>`.
- A library for C with `--bundle-runtime` carries `rt/conf.c`, which reads
  `anti_rt_injectable`. The pass skipped the table for a library, as it skips
  the used-slot bitmaps, and `clib_bundle` failed to link. The table is written
  for a bundled library too, and the providers of a library for C are resolved
  as a program's are, because its host is C and cannot fill a slot.
- Every program now carries the table, empty where nothing injects, so the four
  optimizer listings, the nine IR listings, the twelve assembly listings, the
  manifest of `emit_identity` and the digest of `link_identity_macos-arm64`
  changed. Each was written again from a run on the Mac. The library file
  version rose to 48, which moved one byte of `tests/modules/scale.antl.hex` and
  one of the blob in `tests/unit/test_modules.c`.
- The example of the overview writes `inject final alloc: *Allocator`, and
  `alloc` is a keyword. The entry of `docs/decisions.md` under "`anti.mem`" left
  that field to the session that built `inject`, and the gap procedure gave it
  the position after `inject`.

## Provisional entries added

`docs/decisions.md` gained the section "Injection" with eleven `[provisional]`
entries: the contextual words, the type of the field, the slot and its module,
one slot per interface with `inject final` on any field, the two forms of a
provider and how a path resolves, the provider graph and the cycle,
`anti_rt_injectable` in every program, the providers of a library for C, the
manifest that `anti test` reads, `alloc` and `free` as the name of an `inject`
field, and `--closed`, which is not built. The entry of `[injections]` under
"Runtime configuration" was rewritten: a line now reports what the program does
have, refuses an `inject final` field, and says that this build loads no plugin.

## Questions

- `anti build` does not exist, so `anti test` is the one command that reads the
  manifest. The `[inject]` table reaches a build through `anti test` alone until
  `anti build` is written.
- The run-time replacement of a slot waits for plugins. `[injections]` and
  `--anti.inject` name the path of a library, and nothing loads one.
- A provider that is a module function is checked as a function of no arguments
  that gives a pointer. The IR carries no Anti types, so the interface of its
  result is not checked. A provider that names a class is checked in full.

## Logs

`build/drive/logs/build.log`, `build/drive/logs/test.log`,
`build/drive/logs/asan-test.log` and `build/drive/logs/ubsan-test.log`.
