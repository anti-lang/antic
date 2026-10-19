# Step 33, the minor findings of the back end

Step 33 of "Fix steps" in `docs/audit/summary.md` takes the minor
findings one area per session. `docs/reports/2026-10-19-solid-step33.md`
reports the front end and `docs/reports/2026-10-19-solid-step33-middle.md`
the middle. This session took the back end and the drivers of antic: the
LLVM text, its runs, the ABI, the linker, COFF, the header, the targets,
the platform layer, the user directories, the doc page and `antic.h`.
The logs stand in `build/drive/logs/step33-back-end/`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 6, `llvm_run.c:224` | `c42376e1` | Three arrays of options were filled to a count kept by hand, each exactly full, and `coff_join` read `inputs[0]` for a count of zero. |
| Rule 9, `abi.c:12` | `ae482b8c` | The types of the ABI, the lines of the LLVM text, the offsets of the archive index and the positions of `--dump-tokens` were formatted into fixed buffers without a check. |
| Rule 21, `linker.c:9` | `d0e1f47c` | Three includes named nothing of their header. The other two are used, by `a5a84de2` and by a check older than the audit. |
| Rule 25, `platform.c:216` | `01512912` | Thirteen exported functions of four files stood outside the prefix of their file. |
| Rule 26, `reach.c:154` | `cb6152f3` | Three files each wrote out whether a function is the main of the program. |
| Rule 26, `driver_search.c:50` | `55ff2b54` | Two files of the driver opened a file to ask whether it exists, beside `driver_file_exists`. |
| Rule 26, `target.c:151` | `c0f1e8e1`, `3bcebed9` | `target_block_label` went with the assembler, and `rt_record_items` had the unit tests as its only caller. |
| Rule 26, `llvm_emit.c:242` | `0642823d` | The `_` of Mach-O was dropped in five places, the module flags were counted and written apart, two readers took the name of a COFF symbol, and two writers named a type their own way. |
| Dependencies, `antic.h:4` and `antic.h:111` | `91948f08` | The records every part shares stood in the interface of `anti`, so the compiler included the header of its client for them. |
| Dependencies, `docpage.c:13` | `461dba24` | `driver_interface` took the IR module from its caller, so the doc page built one. |
| A comment against the code, `antic.h:11` | `317837e5` | The list of helpers lacked `attributes.h`, which the test allows. |
| The steps of the work order, `llvm_emit.c:24` | `8f243735` | The comments listed the steps the text was built in, which git holds. |
| A reason that is gone, `driver_index.c:14` | `6b1b234a` | The DESIGN gave `anti build` not being built as the reason. |
| Rule 27, `driver.c:135` | `5d8f3283` | Four comments stood above the wrong item, one of them a leftover. |
| Bar of tests, `main.c:295` | `d378c144`, `17d39076`, `7bb1317c` | Gone before this step: the tests of `--soname` and `--strip-docs` of both tools came after the audit. |

`80e47e27` writes the commits into the summary. The sentence of the
front-end session on `sema.h`, which included `antic.h` for the package
record, is closed by `91948f08` as well: `sema.h` includes `shared.h`.

The tests. `test_no_input` of `tests/unit/test_coff.c` failed with a
segmentation fault before its fix. `main_of` of `tests/unit/test_ir.c`
did not build before `ir_is_main_of`. The bounds of rules 6 and 9 stop
antic with `abort` past the longest command and text, as the bound of the
link command does, and no input reaches them, so the suite holds them.
The other fixes move code and change no output, which `emit_identity`
and `link_identity_macos-arm64` hold, unchanged.

## Findings left

- Rules 18 and 19 at `llvm_emit.c` belong to step 34, which names them.
- `antl_io.h:12`, the inside header of the library file included by
  `antic.c`, goes to the session of the library file.

## New findings, provisional entries, known failures

No new finding. One `[provisional]` entry under "Files of the checker"
in `docs/decisions.md` records the new names of rule 25. The entry of
the interface of `anti` now names `shared.h` and `attributes.h`, and two
entries name `shared.h` where they named `antic.h`.
`tests/known-failures.txt`: 29 lines before and 29 after.

## Gates

- The three builds report no warnings, `build-host.log`,
  `build-asan.log` and `build-ubsan.log`.
- Host, 5132 tests, `-I 1,2000` and `-I 2001,5132`, 322 s and 273 s. All
  2000 and 3153 pass, since a part also counts the fixtures it pulls in.
  The 8 expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip, `host-1.log` and `host-2.log`.
- ASan, 5130 tests, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5130`, 274 s,
  362 s and 366 s. All 1150, 1113 and 2901 pass with the same skips,
  `asan-1.log` to `asan-3.log`.
- UBSan, the same parts, 252 s, 257 s and 290 s. All 1150, 1113 and 2901
  pass, `ubsan-1.log` to `ubsan-3.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged.
- The docs-style checker reports nothing on each `.md` file the step
  touched, `docs1.log` to `docs5.log`.

## Not run on the target

- The renames of `01512912` reach the Windows branches of
  `src/antic/platform.c`, `src/antic/userdirs.c` and
  `src/anti/platform.c`, which the Mac does not compile. The build of
  antic and anti on anti-windows and `unit_tool_platform` there would
  show a name the rename missed.

## State at the push

```text
$ git log --oneline -3
80e47e27 Record the back-end minor findings of step 33 in the summary
5d8f3283 Put four comments of the driver and the back end above their own item
8f243735 Say what the LLVM text does, not the steps it was built in
$ git status --short
$ git rev-parse HEAD origin/main
80e47e2752435e9b5c8dd45d1227e43963216c3e
80e47e2752435e9b5c8dd45d1227e43963216c3e
```

## Left for the next session

The other areas of step 33, one per session, in the order of the
summary: the library file, the runtime, the standard library, `anti`,
the build and the tools, and the tests. Each takes the rows of the minor
tables in `docs/audit/summary.md` whose places lie in its area and have
no commit yet. The session of the library file also takes `antl_io.h:12`.
The session for `anti` also takes the comment on `WRAP_COLUMNS` in
`src/anti/fmt.c`. The session of the tests takes the chapters of the
book in the comments of `tests/CMakeLists.txt`.
