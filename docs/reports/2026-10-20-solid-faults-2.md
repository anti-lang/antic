# Step faults, second session

The session goes on from "Left for the next session" of
`docs/reports/2026-10-20-solid-faults.md`, which stays as it was. Every
item of that list now has its tests or its seam. The suite grew from 5368
to 5411 tests. Logs are under `build/drive/logs/`, named `faults2-*`.

## What changed

- `anti sdk export`: the open of a stub and of the frameworks, the write
  of a copy and of `sdk-version`, and the kind of a stub.
- `anti symbols`: `inventory` and `check` whose directory does not open,
  an archive `inventory` cannot write, and `resolve` that cannot read
  the archive or the trace.
- The fetch of `deps.c` and `repo.c` from a `file://` repository: the
  index and the module of the repository, the cached index and the
  stamp. The read for the digest, the copies into the cache and the
  directories of the cache follow. Over HTTP, the pipe of curl and the
  start of curl.
- The loader of plugins: `tests/plugin/<host>.faults` takes cases as a
  program of `tests/programs/` does, through `run_plugin_kinds.cmake`.
  `faults_host` loads the library `clock`, and `named_host` makes the
  loader read the library for a module it lacks.
- The registry: `reflect.new` and `Object.deserialize` with one
  allocation refused, `tests/std/faults_registry.anti`.
- A line of `<name>.faults` takes words after its value: `dev` builds
  the program in dev mode with the dev objects of its imports, a word
  `NAME=value` sets a variable and a word with `--` passes an argument.
  `faults_locks` grows the table of the orders of the locks of a dev
  build. `faults_start` copies `ANTI_CONF` and `--anti.inject` at start.
- The Windows halves of the runtime, antic and anti take the seam where
  their POSIX halves take it. A call of Windows that reports through
  `GetLastError` goes through `ANTI_FAULT_SYSTEM`, which sets that error
  from a table of `src/rt/fault.c`. The table holds numbers, since rule
  22 of `docs/c-guidelines.md` keeps `#if` on the host out of `fault.c`,
  which the test `platform_layer` showed.
- `emit_identity` gained the six lines of `faults_locks`, and no digest
  changed.

## Findings

Closed: T157, major, in `fbf4a798`. After a wait for the thread of the
work failed, `platform_run_on_stack` returned while the thread still ran
on the memory of `main`, which `main` then freed. AddressSanitizer
reported it in `antic_message_fault_work_not_waited` of the first
session, one run in two. The layer now ends the process with `_Exit(1)`
after the message.

New, left for the step of their area:

- T155, minor. `anti sdk export` leaves a stub whose kind the system does
  not give out of the bundle and ends with 0. `copy_stubs` of
  `src/anti/sdk.c` passes over `PLATFORM_UNREADABLE`.
- T156, minor. The fetch names the file of the repository where the
  write of `<file>.new` failed, and nothing where the copy to the place
  of the file failed. `files_copy` says nothing by its contract.

New tests of open findings: T139 holds the loader of plugins in three
cases and the registry in four. T153 holds the two directories of the
cache of a fetch.

`tests/known-failures.txt` held 68 lines and 49 entries before, and
holds 82 lines and 63 entries after. 61 of them are expected to fail on
the Mac.

## `[provisional]` entries

- New, under "Object model": `Object.deserialize` gives `none` when its
  allocator gives `none`, and gives back what it took.
- Changed, under "Repository layout": the entry of the seam names the
  Windows halves and `ANTI_FAULT_SYSTEM`. The entry of where a fault test
  stands names the hosts of `tests/plugin/` and the words of a case.

## Observations that are no finding

- `inventory` whose directory does not open writes its archive and ends
  with 0, as "The symbols command" says of what is missing.
- The fetch that cannot remove `<file>.new` leaves it in the cache, and
  the next fetch writes over it.
- A library the loader cannot read for the name of a module gives the
  reason of the system, as the DESIGN of `missing_module` says.
- `anti sdk export` that cannot remove its stage leaves
  `.apple-sdk-<version>` beside the bundle and ends with 0.

## Gates

- Builds without a warning of our code: `faults2-build13.log`,
  `faults2-asan-build3.log` and `faults2-ubsan-build2.log`.
- Host, 5411 tests, `-I 1,2000` 354 s and `-I 2001,5411` 281 s: 2000 and
  3437 passed, the count of the second with the fixtures ctest adds. 61
  expected failures fail as listed, and the two `mimalloc_environment`
  tests skip: `faults2-host-5.log` and `faults2-host-6.log`.
- ASan, 5409 tests, `-I 1,1150`, `1151,2250`, `2251,3800` and
  `3801,5409`: 1150, 1114, 1576 and 1614 passed in 244 s, 170 s, 190 s
  and 511 s, `faults2-asan-9.log` to `-12.log`.
- UBSan, the same parts: all passed in 243 s, 146 s, 142 s and 349 s,
  `faults2-ubsan-5.log` to `-8.log`.
- `emit_identity`, `emit_identity_texts` and `link_identity_macos-arm64`
  pass.
- The docs-style checker reports nothing on the `.md` files the session
  touched.

## Not run on the target

- The Windows halves of `src/rt/platform_windows.c`,
  `src/antic/platform.c` and `src/anti/platform.c` behind the seam, and
  `_Exit` after a failed `WaitForSingleObject`. The pinned clang checked
  each for both Windows processors with the seam on and off. A fault tree
  on a Windows host would run them.
- The counts of every new fault case on Linux: the plugin cases, the
  fetch, `anti symbols` and the programs. `tests/known-failures.txt`
  lists the plugin cases with `host=macos,linux`. A suite of a fault tree
  on Linux would show them.

## State at the push

```text
$ git log --oneline -3
fbf4a798 End the process when the thread of the work cannot be waited for
de2f8139 Test the directories of the cache of a fetch when mkdir fails
4d665978 Put the Windows halves of the platform layers behind the seam
$ git status --short
$ git rev-parse HEAD origin/main
fbf4a7983b650fdc254a86021ea9ebe039fa6468
fbf4a7983b650fdc254a86021ea9ebe039fa6468
```

This report is written after that push, and its commit follows it.
