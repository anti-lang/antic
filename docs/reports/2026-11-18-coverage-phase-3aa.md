# Batch 3aa of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer. The
summary of `./c coverage` at the end of batch 3z gave the ranking. Batch
3z found T299 by reading `lock_read`, so this batch read the other readers
of `src/anti/` and `src/rt/` for the same flaw: an entry that a missing key
or a damaged element ends, drops or gives to another entry. It read the
readers of the manifest, of the cache of the digests, of the index of a
deployment archive and of the runtime configuration in `src/anti/syms.c`,
of the index of plugins in `src/rt/plugin.c` and in
`src/antic/driver_index.c`, and of the runtime configuration in
`src/rt/conf.c`. Each find ran first through the anti of the host with a
project of its own under `build/`. Eddie's direction is the Mac alone, so
the VMs were not touched. The batch found three defects, T301 to T303. The
next free id is T304.

## Commits

- `74609592` lists T301 to T303.
- `e5a52d1f` fixes T301, `22b8af22` T302 and `de6fda45` T303.
- `94f4143e` tests the file names that a program takes from the manifest,
  in `tests/unit/test_deps.c`.
- One commit records the times in `CLAUDE.md`, the commits of the fixes in
  `docs/audit/tests-findings.md`, the guards of the batch in
  `docs/notes/unreached-guards.md` and this report.

## Findings

- T301, major, fixed. `load_deployment` of `src/anti/syms.c` gave every
  key of `index.toml` to the unit of the last `module` line it read. A
  table without its `module` line added its `id` to the id of the table
  before it, so that unit named no entry and `anti symbols resolve`
  answered no frame of its program. A key now names the unit of its own
  table alone, as the `[provisional]` entry of R184 says of a key before
  the `module` line of its table. That entry keeps a table whose `id`
  stands before its `module` line passing over the `id`, so the batch
  wrote no test against it.
- T302, major, fixed. `array_of_key` of `src/anti/manifest.c` took an
  empty string or an inline table of a list for the end of the list. The
  build passed over it and every target after it, a list that started with
  it built the host, and the notice lost the attribution lines after it.
  The list now ends at the end of its array, and such an element is an
  empty text, which `[targets]` refuses with the line it gives any name
  that is no target. A `[provisional]` entry records it.
- T303, major, fixed. `known_read` of `src/anti/cache.c` read the numbers
  of a line of `archive-digests.txt` with `strtoull`, which passes over a
  line break. An empty line, or one cut short after three numbers, took a
  length below zero for its path, and every build ended with
  `anti: out of memory` and 70 until the file was removed. The numbers
  are read from a copy of the line now, and such a line is passed over.

## Read without a find

- `src/rt/conf.c` refuses an inline table in an array of `[runtime]`
  with the name of its key, and `src/anti/syms.c` reads `plugins` and
  `[injections]` as the runtime does.
- `index_lists` of `src/rt/plugin.c` ends the list of interfaces of a
  library at an empty one, so an interface after it is not discovered.
  antic writes every index and never an empty interface, so no input of
  the tools reaches it. A hand-written index does, and the next batch
  may list it as the T302 class.
- `sinks_of` of `anti.log` passes over a `[[sink]]` without its `kind`
  without a line. The decision names a missing or invalid file alone, so
  this stays as it is.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 43400.
`cmake -DFIRST=43401 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 43401 to
43600 at the default size in 153 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.39%, 3728 missed | 93.42%, 3711 missed |
| Regions | 98.52%, 1264 missed | 98.52%, 1260 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.73%, 1224 missed | 98.73%, 1222 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/anti/manifest.c` | 26 | 10 |
| `src/anti/syms.c` | 16 | 17 |

The fix of T301 added four branches to `syms.c`, and the tests take three
of them. `src/rt/sync.c` and `src/rt/text.c` moved by three and by one,
since the threads of the suite take other paths from run to run. The
coverage run had 7859 tests, all passed, and its suite took 1069 s.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7869. Every test passed.
- The host runs 7871 tests and each sanitizer tree 7869.
  `tests/known-failures.txt` holds the two lines of `host=windows`, as
  before.

## Not reached

The sides of `deps.c` that batch 3z named stay: a dependency of a library
file without a constraint, a lock entry whose names differ from those of
the graph at one place, and a lock that lacks a dependency the manifest
adds. The sides of `main.c` that remain are the tests `i + 1 < argc`,
which `lacks_value` leaves no input, and the runtime archive of the user
that is not found. The unsorted sides of the batch stand at the end of its
section of `docs/notes/unreached-guards.md`.

## Questions for Eddie

- A list of the manifest given as one value, `default = "macos-arm64"`,
  and a `[layout]` key given as an array, read as no value without a
  line. Should the reader refuse a value of the wrong shape? A refusal
  needs a message, so the feature freeze keeps them as they are.

Ten questions stay open from before, and nothing here acts on them.
