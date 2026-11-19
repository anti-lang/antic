# Batch 3ab of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
Batches 3z and 3aa found their defects by reading readers, so this batch
read the readers of `src/rt/`, `src/anti/` and `src/antic/` that 3aa had
not read, for an entry that a missing key or an empty element ends, drops
or gives to another entry. It read `index_lists` of `src/rt/plugin.c`,
which 3aa named, the join of the array of `plugins` in `src/rt/conf.c`,
the reader of TOML in `src/rt/toml.c`, the readers of the runtime
configuration and of the plugin index in `src/anti/syms.c`, the
`[inject]` tables of `src/anti/manifest.c`, the lock and index walks of
`src/anti/deps.c`, `src/anti/bindapi.c`, the notice and source readers of
`src/anti/licensing.c`, the JSON scanner of `src/rt/json.c`, the
deserializer of `src/rt/registry.c`, the DWARF line reader of
`src/rt/symbols.c` and the sinks of `anti.log`. Eddie's direction is the
Mac alone, so the VMs were not touched. The batch found five defects,
T304 to T308. The next free id is T309.

## Commits

- `2c42adea` lists T304 to T308.
- `914b20a3` fixes T304, `3c478c02` T305, `64a4180a` T306, `4e57640a`
  T307 and `f4f706a4` T308.
- `3bc9ff9b` tests the sides of the reader of TOML, of a deployment index,
  of the manifest and of `anti bind`.
- One commit records the times in `CLAUDE.md`, the commits of the fixes in
  `docs/audit/tests-findings.md`, the guards of the batch in
  `docs/notes/unreached-guards.md` and this report.

## Findings

- T304, major, fixed. `index_lists` of `src/rt/plugin.c` took an empty
  interface of an entry of `anti-plugins.toml` for the end of the list,
  so discovery passed over the interfaces after it. antic keeps such an
  entry of another library as it stands, so only a hand-written index
  reaches it. The list now ends where the index holds no next element. A
  `[provisional]` entry records it.
- T305, major, fixed. `join_elements` of `src/rt/conf.c` wrote the
  separator only after a text that was not empty, so `plugins = ['', 'a']`
  gave `a`, and the working directory was not searched. An empty element
  after the first stayed. The value is now `:a`, as the entry of the
  joined array says.
- T306, major, fixed. `read_configuration` of `src/anti/syms.c` passed
  over an empty directory of a text of `plugins`, which the runtime
  searches as the working directory, so `inventory` and `check` read no
  index there. The case `inventory_plugins_as_a_text` of 2026-10-13 held
  the old reading, written before the `[provisional]` entry of 2026-10-16
  that makes an empty directory the working directory. It now expects the
  index of the directory of the configuration.
- T307, major, fixed. `inject_of` of `src/anti/manifest.c` passed over an
  empty provider of `[inject]` and `[inject.test]`. The link then asked
  for a provider the manifest wrote, and an empty entry of
  `[inject.test]` left the provider of `[inject]` in the tests. The entry
  is now passed on, and the link refuses it with the line it gives any
  provider that names no function. A `[provisional]` entry records it.
- T308, major, fixed. `repeats` of `src/rt/toml.c` numbered a `[[name]]`
  table from the keys of its pairs alone. A table that held no key, or
  empty arrays alone, gave the next table its number, the two entries
  became one, and `anti build` read an index whose first `[[version]]`
  held no `version`. The paths of the empty arrays and tables are now
  indexed with the keys, each once, as the DESIGN comment of `repeats`
  asks.

The fixes of T304, T305 and T308 change the runtime, so each writes the
digest of `link_identity_macos-arm64` again. The fix of T308 adds a
counter to the document, so the fault case of the manifest reader names
88 bytes.

## Read without a find

- `src/anti/bindapi.c` leaves out a struct whose fields are no array with
  a warning, and a function whose parameters are no array with one.
- The `needs` of a lock entry in `src/anti/deps.c` end at an element that
  is no string, such as `{}`. A package that only that edge reaches is
  then unreached, so the lock is resolved again and written anew.
- `licensing_notice`, `source_line`, the JSON scanner, the deserializer
  and the DWARF line reader turned away every damage the batch tried.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 43600.
`cmake -DFIRST=43601 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 43601 to
43800 at the default size in 169 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.42%, 3711 missed | 93.45%, 3693 missed |
| Regions | 98.52%, 1260 missed | 98.52%, 1260 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.73%, 1222 missed | 98.73%, 1223 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/rt/toml.c` | 29 | 25 |
| `src/anti/bindapi.c` | 26 | 23 |
| `src/anti/manifest.c` | 10 | 7 |
| `src/anti/syms.c` | 17 | 16 |
| `src/rt/plugin.c` | 26 | 27 |

`index_at` of the fix of T304 adds a guard to `plugin.c`, whose line is
the one more line missed. `src/rt/platform_posix.c`, `src/rt/sync.c`,
`src/rt/text.c` and `src/rt/mem.c` moved by five, one, one and one, since
the threads of the suite take other paths from run to run. The coverage
run had 7871 tests, all passed, and `./c coverage` took 1154 s.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. The first runs before the
  fixes of T304 and T308 failed one test each, for the digest and the
  size above. Every test passed once they stood written.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7881. Every test passed.
- The host runs 7883 tests and each sanitizer tree 7881.
  `tests/known-failures.txt` holds the two lines of `host=windows`, as
  before.

## Not reached

`read_configuration` of `src/anti/syms.c` splits a text of `plugins` at
`:` on every host, while the runtime splits it at `;` on Windows by
decision 6 of "Decisions of 2026-10-11". A Windows host would read
`C:\a;C:\b` as three directories. No test of the Mac shows it, and the
VMs were not used, so the batch lists no finding for it. The sides left
unsorted stand at the end of the section of the batch in
`docs/notes/unreached-guards.md`.

## Questions for Eddie

- `anti bind` leaves out an enum whose `values` is no array without a
  warning, where a struct whose `fields` is no array gets one. A value of
  an enum without its name gets `` a value of the enum `E` is no integer ``.
  Each needs a message of its own, so the feature freeze keeps them.
- Should the split of `plugins` in `anti symbols` follow the separator of
  the host, as the runtime does? The fix is one line, and only a Windows
  run would test it.

Twelve questions stay open from before, and nothing here acts on them.
