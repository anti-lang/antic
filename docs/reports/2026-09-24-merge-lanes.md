# Merge of the fix lanes

This step brings the lanes of the audit fixes together on `main`. It fixes
no finding of its own.

## Decisions folded

The 74 entries of `docs/decisions-backend.md`, `-front.md`, `-library.md`,
`-runtime.md` and `-tool.md` now stand in `docs/decisions.md`, each with its
wording and its tag. A wrapped entry became one line, as every entry of
`docs/decisions.md` is. The five files are deleted, in commit `9cc8333`.
Each entry went to the end of its section:

| Section | Entries |
|---|---|
| Object model | contains itself, inherits itself, repeated member of `Object.deserialize` |
| Declarations and statements | class constants, repeated arrays, chains of constants |
| Libraries and runtime | the library reader, bitfield units, aggregate size, the `sdiv` fold, the platform layer of step 19 |
| Threading | the `worker fn` walk, `parallel` results, pool wakes, atomic loads |
| Standard library phase | text builder counts, TOML `[[name]]` and key length, the signal lock |
| Build tool and distribution | the COFF join, `src/anti/files.c` and the directory walk |
| Optimizer and register allocation | the borrowed register |
| Compiler behaviour | source size, parser depth, SDK names, `anti sdk export` versions |
| Build ids | the link reads every object, the id of a loaded module |
| Error origins and stack traces | NOBITS, DWARF lines and entry tables, LEB128, `trace.c` read errors |
| Sum types, Simd structs | `v is X`, the unit break and `fixed_layout` |
| Runtime configuration | `threads`, replaced values, a second `rt.configure` |
| Plugins, Versions | the loader lock and reason, the provides table, version parts |
| The build, doc, bind and symbols commands | steps 14, 15 and 16 |

The comment of `NEST_LIMIT` in `src/antic/antl.c` named
`docs/decisions-library.md` and now names `docs/decisions.md`. The reports
of earlier steps still name the lane files. They record history and stay as
written.

## State and format version

The "State" section of `CLAUDE.md` gives 831 host tests and 830 per
sanitizer build. The format version there was already 53. The entry on the
library format in `docs/decisions.md` said 41, and now says 53, which is
`ANTL_VERSION`.

## Identity manifests

`emit_identity` and `link_identity_macos-arm64` pass on the merged tree, so
nothing was regenerated. Every line the fix steps changed in
`tests/emit-identity/` and `tests/link-identity/` has a fix behind it:

- Added emit lines, six targets each: `deep_nesting` (S3, `aac19c8`),
  `spill_three` (S18, `8578728`), `by_unsigned` (S23, `2e8f1fa`) and
  `parallel_join` (S34, `a381a6b`). No existing emit line changed.
- The macOS link digest of `return42` moved with each runtime change:
  step 10 (`39b64d8`), step 11 (`eb74603`), step 12 (`a381a6b`), S32
  (`134512d`) and M29 (`9b7d7eb`).

## Gates

- Build: zero compiler warnings on host, asan and ubsan, in
  `build/drive/logs/merge-{host,asan,ubsan}-build.log`. Each link prints the
  known `ld: warning: ignoring -lto_library`, as earlier steps report.
- Host: 100% tests passed out of 831, `build/drive/logs/merge-host-ctest.log`.
- ASan: 100% tests passed out of 830, `build/drive/logs/merge-asan-ctest.log`.
- UBSan: 100% tests passed out of 830, `build/drive/logs/merge-ubsan-ctest.log`.
- Docs style: nothing on `CLAUDE.md`, `docs/decisions.md`,
  `src/antic/antl.c` and this report.

## Findings and provisional decisions

No finding was taken in this step, and no `[provisional]` decision was made.

## Proof

Before this report was committed:

```console
$ git log --oneline -3
caf7c9f Take the test counts of the merged tree and the current format version
23e21b5 Point the nesting limit of the library reader at docs/decisions.md
9cc8333 Fold the decisions of the fix lanes into docs/decisions.md
$ git status --short
$ git rev-parse HEAD origin/main
caf7c9fed5e5c79ed7ea85cb3efc27e4444d4cf4
caf7c9fed5e5c79ed7ea85cb3efc27e4444d4cf4
```
