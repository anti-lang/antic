# Step rt-tests, sixth session

The session went on from the list of `2026-10-21-solid-rt-tests-5.md`,
which stays, as do the reports of the sessions before it. It wrote tests
for regions of `src/rt/`, `src/antic/` and `src/anti/` that a program, an
option or an injected fault reaches. Logs are under `build/drive/logs/`,
named `rt6-*`. The list of the regions not reached is
`rt5-end-unreached.txt` at the start and `rt6-end-unreached.txt` at the
end. The probes are in `build/rt6/`: `show.py` and `brief.py` print the
source of each region of the list.

## Coverage per directory

Regions reached. The start is `08ae0a4a`, `rt5-end-summary.txt`. The end
is `318bc115`, `rt6-end-summary.txt`. Every part of the run passed,
`rt6-end-1.log` to `-5.log`.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 94.51% of 9252 | 94.65% of 9252 | 508, 495 | 94.63% | 94.92% |
| `src/antic/` | 97.97% of 55587 | 97.98% of 55587 | 1127, 1122 | 98.02% | 98.02% |
| `src/anti/` | 98.02% of 11663 | 98.11% of 11663 | 231, 221 | 98.10% | 98.16% |

## The tests

- runtime: `std/plugin_long_path` loads a path one byte too long, and
  `plugin_no_plugin` loads a shared library built with its own runtime,
  which carries no table of what it provides. The runner of the plugin
  kinds takes `PLAIN` for it. `plugin_host` runs `--anti.inspect` with a
  library that `--anti.inject` loads. `std/toml_inline_tables` nests
  inline tables and overruns the room of a path in one.
  `std/text_float_precision_bound` gives `append_float` a precision above
  its bound. `conf_include_relative` names a configuration file without
  its directory. `programs/sync_deep` holds 40 locks in dev mode, past the
  32 whose order the check records. `faults_variable_nine_entries` gives
  `ANTI_FAULT` nine entries.
- antic: the matrix gives out the `ptr` of a `str` field and a slice field
  of another object from a concurrent class.
  `error_modules_plugin_without_a_class` builds a plugin of a module
  without a class.
- anti: `anti run` takes the `[inject]` table of the manifest in a dev
  build, and `anti bind --clang` binds parameters without names.

`emit_identity` gains the six texts of the new program `sync_deep`. The
text of every other program stays as it was.

## Findings

The step closed none. One is new, with its test.

- T169, major: a program whose path dependency is a project that names
  another project by its path does not build. The library file of the
  first names the second without its path, and the walk looks for it in
  the repositories. `anti_build_message_path_chain_of_two_projects` holds
  it. Naming the second project in the program's manifest as well builds.

`tests/known-failures.txt` held 109 lines and 90 entries, 88 expected to
fail on the Mac, and holds 110 lines and 91 entries, 89 on the Mac. No
feature, option or command, and no `[provisional]` entry.

## Regions nothing reaches

- `src/rt/atomic.c`, 12 regions: the code after the `switch` of `WIDTHS`
  in each function. Every case returns, and every function runs.
- `src/rt/plugin.c` 286 to 301, `stub`: `reflect.call` names a function
  through the descriptor of the class. That lists only the functions the
  library carries, so no call reaches a slot of the stub table.
- `src/rt/fault.c` 293 to 301: no source calls `ANTI_FFLUSH`, so the seam
  of `fflush` has no caller. 321 to 331 is the seam of Windows alone.
- `src/rt/toml.c` 333: the header reader keeps 24 bytes of room, and the
  `.N` of an array of tables takes at most 20.
- `src/rt/lock.c` 139: a synchronized object taken again by its thread
  never reaches `order_take`, and `held_already` stops a Mutex taken
  twice.
- `src/rt/trace.c`, `elf_id`, `symbolize_elf`, `symbolize_pe` and the
  cases of `module_id` for ELF and PE: the Mac runs Mach-O alone.
- `src/antic/driver.c` 1887: the option reader refuses a level of the
  other architecture first. Only a caller that builds its own options
  reaches the check.
- `src/antic/platform.c` 855 to 863: the child of the fork ends with
  `_exit`, which writes no profile.
- `src/antic/sema_call.c` 2594 to 2597: `Object` is a struct symbol in
  every module, so the lookup never misses it.
- `src/antic/llvm_emit_function.c` 27 to 41, 48 to 60, 69 to 75 and 175
  to 181: `layout_resolve` replaces the aggregate, `c_long`, `c_wchar`
  and the lock word before the text is written, and the lowering writes
  no other operand.

## Left for the next session

- `src/antic/`, 1122 places: `llvm_emit_function.c` 55, `sema_generic.c`
  47, `driver_link.c` 43, `sema_const.c` 39 and `antl_tree.c` 36 have the
  most. `driver_link.c` holds the link for Linux and the SDK of Apple,
  which the Mac reaches in part.
- `src/anti/`, 221 places: `bindclang.c` 34, `bindwrite.c` 27, `build.c`
  18 and `deps.c` 14. T169's test reaches `deps.c` 371 to 374.
- `src/rt/`, 495 places: `trace.c` 85 and `symbols.c` 55, mostly ELF and
  PE, `plugin.c` 66 and `object.c` 32.

## Not run on the target

- `plugin_no_plugin` on Windows: the runner links the plain library
  without the import library of its host. The test would show whether
  the load refuses it there as well.

## Gates

- No warning of our code: `rt6-host-b1.log`, `rt6-asan-build.log` and
  `rt6-ubsan-build.log`.
- Host, 5618 tests, `-I 1,2000` and `-I 2001,5618`: 2000 and 3644
  passed, in 351 s and 303 s. 89 expected failures fail as listed, and
  the two `mimalloc_environment` tests skip: `rt6-host-1.log` and
  `rt6-host-2.log`. The first run of the second part failed
  `emit_identity` and `fmt_canonical`, which `148e4cb0` and `318bc115`
  answer.
- ASan, 5616 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5616`: 1151, 1114, 1576, 905 and 918 passed in 243 s, 182 s,
  186 s, 446 s and 338 s, `rt6-asan-1.log` to `-5.log`. Parts 3 and 4
  ran in one call, which the tool moved to the background when it passed
  ten minutes. The session waited for it before it ran anything
  else.
- UBSan, the same parts: all passed in 243 s, 160 s, 140 s, 177 s and
  303 s, `rt6-ubsan-1.log` to `-5.log`.
- `emit_identity` passes with the manifest of `148e4cb0`, which adds the
  texts of `sync_deep` alone. `link_identity_macos-arm64` passes, and no
  file of `tests/link-identity/` changed.
- The docs-style checker reports nothing on the `.md` files the session
  touched.

## State at the push

```text
$ git log --oneline -3
318bc115 Write toml_inline_tables in the form of anti fmt
148e4cb0 Pin the LLVM text of sync_deep for emit_identity
ecc77222 Test ANTI_FAULT with nine entries
$ git status --short
$ git rev-parse HEAD origin/main
318bc115c1591b4874debaa5929c488bdd4d35f6
318bc115c1591b4874debaa5929c488bdd4d35f6
```

This report is written after that push, and its commit follows it.
