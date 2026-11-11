# The plain form of `anti license` and the record of sources

Eddie answered the question of `docs/reports/2026-11-11-coverage-batch-6.md`
on 2026-11-11, with the first of three options. The plain form fails for
a component without a line in the record. The three components that have
no source the record names are the exception. This step records the
answer, lists the defect as T230 and fixes it. The VMs were not touched.

## Commits

- `c314e98f` records Q1 of the report of batch 6 under "Answers of
  2026-11-11" in `docs/decisions.md`, with a sentence in the entry of the
  freeze. It changes the plain form in `docs/distribution.md`.
- `8fbbd43c` lists T230, and `fa353273` fixes it.
- This report, with the times and the counts in `CLAUDE.md` and the
  commit of the fix in `docs/audit/tests-findings.md`.

## Findings

T230, major, fixed. The plain form gave the version `-` to every text of
`licenses/` whose line the record lacks, and ended with status 0. The case
`anti_message_license_sources_without_a_component` holds an archive whose
record lacks the line of mimalloc. Before the fix `ctest -V` showed
`package mimalloc -` and status 0, the reason the row names. The form now
ends with status 1 and `anti: archive/licenses/sources.txt holds no line
of mimalloc`.

`RUNTIME_UNSOURCED` of `src/antic/shared.h` names `apsl`, `compiler-rt`
and `macos-sdk` once. `component_package` of `src/anti/licensing.c` gives
them `-`. `antic_unsourced_components` of `tools/upstream-sources.cmake`
reads the same line of the header. The writer of the record then refuses
a line for one of them. The test `runtime_sources`, and `package_sources`
over a package, refuses a text of `licenses/` that has no line and is not
in the list, apart from the text of the runtime. A run of its runner over
a copy of the archive with an extra text failed for that reason. Entry
933 of `docs/decisions.md` names the list, and entry 324 is unchanged.

`--from` and `--from-archive` need no change. `--from-archive` reads no
record. `--from` adds the line of each component that the record names.
Its notice also names `antic`, `anti`, the program and its libraries,
which the record never holds, so a name without a line is no defect
there.

The real archive passes. `anti license --runtime build/host/runtime`
ends with 0 and prints the same bytes as before the fix. Every test whose
name holds `license` or `sources` passed, with `package_pack` building
the package that `package_sources` read.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,7178`, and every test passed apart from the
  two skips of `mimalloc_environment`: 376 s, 263 s and 298 s before the
  commit that lists T230, and 373 s, 264 s and 297 s before its fix.
- Both sanitizer suites ran after the fix, in the six parts of `asan` up
  to test 7176, and every test passed. `asan` took 247 s, 186 s, 263 s,
  185 s, 445 s and 428 s. `ubsan` took 251 s, 187 s, 263 s, 197 s,
  468 s and 454 s.

## Provisional lines

No new provisional line.

## Questions for Eddie

1. `licenses/macos-sdk.txt` of the tree and of the package says that the
   `.tbd` stubs are copied from `MacOSX26.5.sdk`. No script of `tools/`
   writes the file. The copy in `build/deps/sysroot/licenses/` is from
   2026-09-18, before the stubs of Zig, and the entry on the macOS
   sysroot says those stubs come from Zig. A fresh checkout would lack
   the text. Should the text go, or should a script write it?
