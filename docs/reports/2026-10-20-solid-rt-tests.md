# Step rt-tests: tests for the regions the suite does not reach

The step ran `./c coverage` with the runtime measured and the fault
tests. It wrote tests for the regions of `src/rt/` that a program, an
option or an injected fault reaches. The measurement itself lost the
counts of the fault tests, which the step corrected first. `src/antic/` and `src/anti/`
are left for the next session. Logs are under `build/drive/logs/`, named
`rtt-*`. The lists of the regions not reached are
`rtt-before-unreached.txt`, `rtt-base-unreached.txt` and
`rtt-final-unreached.txt`, one line per function with the places of its
regions.

## The measurement

`5c8e61a2` and `0da17f65` correct three gaps of the coverage tree.

- The runtime of the fault tests, `runtime-faults/`, was not instrumented,
  so no fault case of a program counted.
- A function that holds `ANTI_FAULT` had one shape with `ANTI_FAULTS` and
  another without it. llvm-cov reads one record of a function, so the
  counts of `antic_faults`, `anti_faults` and every fault case were lost
  for such a function. In a coverage tree the seam now keeps its choice
  without `ANTI_FAULTS`, a mask of zero, and both builds have one hash.
- A program that the runtime aborts wrote no profile. In a coverage tree
  `anti_rt_fail_abort` and `anti_rt_fail_memory` write the counts first.
  Every other tree builds the runtime to the bytes it had, which
  `link_identity_macos-arm64` shows.

A coverage tree also disables two cases of anti build,
`anti_build_message_fault_map_not_written` and
`anti_build_message_fault_symbols_not_written`. anti copies the program
in writes of 64 KiB. The counters make it 243 KB where the host tree's
is 69 KB, so the write each case names moves.

## Coverage per directory

Regions, the share of them reached. "Start" is the tree at `c74094fe`
measured as it stood, `rtt-before-*.log`. "Start, corrected" is the same
tests with the measurement above, `rtt-base-*.log`. "End" is `0da17f65`,
`rtt-final-*.log`. Every part of every run passed, apart from the two
cases of the first run that the coverage tree now disables.

| Directory | Start | Start, corrected | End | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 89.23% of 8998 | 90.90% of 9251 | 93.26% of 9252 | 88.02% | 93.44% |
| `src/antic/` | 97.66% of 55491 | 97.68% of 55587 | 97.68% of 55587 | 97.74% | 97.79% |
| `src/anti/` | 97.05% of 11603 | 97.00% of 11663 | 97.00% of 11663 | 97.00% | 97.00% |

The seam adds the region of its failed value to every call it stands
in, so the corrected count holds more regions. The failed values that
no fault case takes lower `src/anti/` by 0.05 points. The places of
`src/rt/` not reached went from 814 to 672 with the measurement and to
495 with the tests.

## The tests

- `std/collection_text`: `to_text` and `serialize` of a pointer, a
  function, a slice and an array element, and of a struct that holds
  them.
- `std/serialize_c_types`, `std/deserialize_parts`, `std/reflect_range`:
  fields of `c_long`, `c_ulong`, `c_wchar` and `char`, the parts of an
  object a text can get wrong, a chain past the depth of the reader and
  `anti.reflect` out of range.
- `programs/hooks_super`, with the case `writes`: each hook of the root
  called through `self.super`.
- `std/toml_subset` and the case `names_memory` of `std/faults_toml`: the
  edges of the TOML subset and the table of the names of the paths.
- `std/runtime_edges`: a path with a NUL byte, a signal outside the
  table and a sleep of no time. `std/regex_classes`: the classes of byte
  patterns, a newline of CRLF and two groups of one name.
- Seven cases of `conf_`. Options of the runtime without their value or
  past their room, and an empty hash seed. An element key, an array
  before a key and `rt.configure` twice. `inject_layers`: two layers of
  injections.
- Traps `cast_failed`, `pattern_group_name`, `pattern_template` and
  `pattern_group_bytes`, and six cases `faults_variable_` of a malformed
  `ANTI_FAULT`.
- `matrix/threads_parallel_in_parallel`, `matrix/threads_synchronized_reentry`,
  the masks and the compare-and-swap of `unit_rt_bounds`, and
  `plugin_host` refused with two objects alive.

## Findings

New: T158, severe, open. `to_text` of a collection writes an array
field of a struct element as `?`. `show_value` of `src/rt/object.c` takes
the size of an array from its caller, and a field passes 0 although the
type id holds the length. `std_collection_text_array_field` holds it,
for the step of the runtime's text. The new case `names_memory` of
`faults_toml` shows T139 again and is listed under it.

The step closed no finding. `tests/known-failures.txt` held 82 lines and
63 entries, 61 expected to fail on the Mac, and holds 84 lines and 65
entries, 63 on the Mac. No `[provisional]` entry, and no feature, option
or command.

## Regions of `src/rt/` nothing reaches

A guard tests for `NULL`, a size past `SIZE_MAX` or a kind that no
caller passes.

- `atomic.c`: the end of each function after `WIDTHS`, whose every case
  returns.
- `reflect.c` 19 to 83 and `object.c` 66, 79, 234, 266, 280, 446, 600,
  683, 731, 785 and 929: guards of a descriptor or a record. `anti.reflect`
  takes `*byte`, which holds no `none`.
- `object.c` 939: the root's `destruct`, which the compiler's teardown
  never calls and Anti refuses to call.
- `toml.c` 131, 197 and 333, `conf.c` 129, 234 and 534, `fs.c` 114,
  `registry.c` 206 and `json.c` 358 to 384: sizes past the memory, a
  failed `vsnprintf` and a member that `anti_rt_json_value` validated.
- `toml.c` 613 to 662, `sync.c` 73, 190 and 206, `signal.c` 31: guards of
  a handle and of a signal the platform layer never routes.
- `lock.c` 139 and 182: a thread that takes a lock it holds records no
  order, and a release with nothing held.
- `fault.c` 293: `ANTI_FFLUSH` has no caller. 321 to 330 are Windows,
  185 needs nine entries, which a case of `add_expect_test` cannot write,
  and 209 and 238 a race and a full table of streams.
- `platform_posix.c` 907 to 912 and `cpu.c` 172: a Mac below `armv8.5`
  and the level of a record of another copy of `cpu.c`.

## Left for the next session

- `src/rt/`: `trace.c` 85, `plugin.c` 70, `symbols.c` 55, `registry.c`
  46, `object.c` 43, `patterns.c` 32 and `regex.c` 26 places, read but
  not all written. Most of `trace.c` and `symbols.c` are ELF and PE, which
  only a Linux or a Windows host runs. `plugin.c` needs damaged
  libraries, and `inject.c` 227 to 237 a library that loads under
  `--anti.inspect`.
- Open in the smaller files of `src/rt/`: `conf.c` 346, an option
  without its value, 365 and 495, an include beside a file without a
  directory and an array before another table, and 667, the
  fault kind `env`. `text.c` 424, 602, 700 and 866, `mem.c` 99, `call.c` 44,
  `loaded.c`, `license.c`, `snapshot.c` 39, `check.c` 50 and
  `platform_posix.c` 281 and 774 were not probed. `start.c` 43 and
  `platform_posix.c` 790 need a program started without arguments, and
  267 and 458 a system that gives no count of processors and a frame
  pointer out of line.
- `src/antic/`, 1178 places, and `src/anti/`, 323. `sema_const.c` holds
  110 of them, the folding of symbolic constants. The earlier reports
  `2026-10-13-tests-coverage-front-end.md` and
  `2026-10-13-tests-coverage-rest.md` give reasons for many, and the
  regions behind a failed call of the system now count the fault tests.
- The scripts are in `build/rtt/`: `list.py` and `unreached.sh` write a
  list, `ctx.py` shows the line of each region and `probe.sh` runs one
  program against the instrumented runtime of the fault tests and shows
  the counts of a range of lines.

## Not run on the target

- The shape of `ANTI_FAULT_SYSTEM` in a coverage tree, and the
  instrumented runtime of the fault tests, on a Linux and a Windows host.
  `./c coverage` there would show both.

## Gates

- No warning of our code: `rtt-host-b6.log`, `rtt-asan-build.log`,
  `rtt-ubsan-build.log` and `rtt-cov-build4.log`.
- Host, 5447 tests, `-I 1,2000` and `-I 2001,5447`: 2000 and 3473
  passed, the second with the fixtures ctest adds. 63 expected failures
  fail as listed, and the two `mimalloc_environment` tests skip:
  `rtt-host-3.log` and `rtt-host-4.log`.
- ASan, 5445 tests, `-I 1,1150`, `1151,2250`, `2251,3800` and
  `3801,5445`: 1150, 1114, 1576 and 1650 passed in 255 s, 176 s, 189 s
  and 529 s, `rtt-asan-1.log` to `-4.log`.
- UBSan, the same parts: all passed in 256 s, 155 s, 146 s and 392 s,
  `rtt-ubsan-1.log` to `-4.log`.
- `emit_identity` gained the six lines of `hooks_super`, and no digest
  changed. `link_identity_macos-arm64` passes unchanged.
- The docs-style checker reports nothing on the `.md` files the step
  touched.

## State at the push

```text
$ git log --oneline -3
0da17f65 Keep the bytes of the runtime outside a coverage tree
01a23e7f Reach the readers of TOML and of the configuration where the tests missed
35c8367f Keep the new programs in canonical form and pin hooks_super
$ git status --short
$ git rev-parse HEAD origin/main
0da17f65eeedce7108321fd7d9d8eebe8470fb61
0da17f65eeedce7108321fd7d9d8eebe8470fb61
```

This report is written after that push, and its commit follows it.
