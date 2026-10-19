# Step 33, the runtime: the minor findings of src/rt

This session took the rows of the minor tables of `docs/audit/summary.md`
whose places lie in `src/rt/`, with the detail of `docs/audit/rt.md`. The
logs stand in `build/drive/logs/step33-runtime/`.

## Findings closed

- Rule 1, `assert.c:10`, `ea3d1998`. Six functions that format like
  printf had no format attribute. `ANTI_RT_PRINTF` of `platform.h` marks
  them, and the new test `rt_format_attribute` compiles `src/rt` with
  `-Wmissing-format-attribute`.
- Rule 1, `trace.c:50`, `137b5dcb`. A text sat in `char bytes[1]`, and it
  is now a flexible array member. The pinned clang took the old form as
  flexible, so no output changed and no test can fail first.
- Rules 3 and 16, `assert.c:66`, `6c88106a`. `(int)length` fed `%.*s`.
  `anti_rt_print_width` clamps the length, and the unit test
  `print_width` pins it. The same commit fixes `worker_count`, the NULL of
  `license_stub.c`, the sign extension of `anti_rt_load_integer` and the
  seed of `hash.c`.
- Rule 3, `object.c:757`, fixed by `ca450252` before this step.
- Rule 8, `registry.c:237`, `ca04365d`. `give_back` stored through a
  `void **` that a packed class leaves unaligned. The class `Packed` of
  `test_deserialize.c` stopped the ubsan tree before the fix.
- Rule 10, `call.c:37`, `0058792d`. A signature had no length, and
  `strcmp` compared it. Both records now carry `signature_length`, and the
  plugin loader checks it, which `test_plugin_table.c` holds.
- Rule 11, `registry.h:47`, `e221dc0c`. The comments name who frees.
- Rule 12, `platform_posix.c:92`, `87287098`. Three functions release at
  one place.
- Rule 13, `platform_windows.c:1363`, `3d675da9`. The start of a Windows
  program fails through the failure routine with status 70.
- Rule 14, `registry.c:734`, `71aead2e` and `831bef6e`. A class value
  field takes an object or `null`, and `--anti.inspect=x` and
  `--anti.help=x` are refused. `Holder` of the unit test and the cases
  `conf_inspect_value` and `conf_help_value` hold both.
- Rules 23 and 24, `assert.c:49`, `d76e1bda`. The new lock
  `ANTI_RT_LOCK_TEST` guards the name of the running test, and
  `anti_rt_give_nothing` has a DESIGN. No test can force the race.
- Rule 26, `object.c:1109`, `e62341b3`. Two unused functions, three
  includes and a dead length test are gone. `anti_rt_ready` stays, since
  `geo_ready` of `tests/clib` calls it.
- Rule 26, `patterns.c:35`: one literal compiler `71b26299`, the layout
  of a str `29877f5b`, the growth of the lists `f3d27e74`, the hash of a
  pattern `ff9afebf`, and the memory of the start `b250c0a0`, which the
  new case `start` of `rt_oom` reaches.
- Rule 25, `object.h:211`, `023bbf6a`. The guards, `ANTI_ENTRY_DESTRUCT`,
  `json.h` and the new `snapshot.h`. `rt_headers` checks all of them.
- DESIGN without a reason, `trace.c:142`, `2e3aeda7`. The two caches
  have no bound of 32 now.
- `symbols.h:65`, `7a5342f0`, and the comments that name what moved,
  `platform.h:4`, `e652adbd`. The place `conf.c:342` went with `66b3bd9f`.

## Findings left

- The field `destruct` of `struct anti_descriptor`, from the finding at
  `object.c:1109`. antic writes it and nothing reads it. A DESIGN of
  `src/antic/lower_desc.c` holds it, and its removal changes every
  descriptor. A design decision changes with Eddie's word alone, so it
  waits for him.
- Rule 18 of the runtime belongs to step 34.

## New findings, provisional entries, known failures

No new finding and no `[provisional]` entry. `tests/known-failures.txt`
has 29 lines before and 29 after.

## Identity

- `emit_identity` changes in `0058792d`. The function record grows by
  `signature_length`, and 840 of 1344 entries move in that record alone.
  Two programs were compared with the antic before the change.
- `link_identity_macos-arm64` changes from `6c88106a` on, since the
  start, the failure routines and the locks change. `b8c60e18` writes
  the new digest.

## Gates

- The three builds report no warnings: `build-host.log`,
  `build-asan.log` and `build-ubsan.log`.
- Host, 5135 tests, `-I 1,2000` 386 s and `-I 2001,5135` 271 s. All 2000
  and 3156 pass, a part counting the fixtures it pulls in. The 8 expected
  failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `host-1.log` and `host-2.log`.
- ASan, 5133 tests, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5133`,
  274 s, 362 s and 364 s. All 1150, 1113 and 2904 pass with the same
  skips: `asan-1.log` to `asan-3.log`.
- UBSan, the same parts, 256 s, 255 s and 291 s. All 1150, 1113 and 2904
  pass: `ubsan-1.log` to `ubsan-3.log`.
- The docs-style checker reports nothing on each `.md` file the session
  touched: `docs1.log` to `docs6.log`.

## Not run on the target

- `3d675da9`, the three failures of `anti_rt_windows_start`. A Windows
  program whose C runtime gives no arguments would show it, and no test
  can make it.
- `87287098`, `anti_rt_getenv` of Windows. `conf_environment` on
  anti-windows runs it.

## State at the push

```text
$ git log --oneline -3
a53e003a Capture the output of the format check without the console encoding
eea18785 Record the runtime minor findings of step 33 in the summary
b8c60e18 Write the digest of return42 for macos-arm64 again
$ git status --short
$ git rev-parse HEAD origin/main
a53e003aca9729c22bf4c3f3ef9bc689a9471fc9
a53e003aca9729c22bf4c3f3ef9bc689a9471fc9
```

## Left for the next session

The other areas of step 33, one per session, in the order of the
summary: the standard library, `anti`, the build and the tools, and the
tests. Each takes the rows of the minor tables in `docs/audit/summary.md`
whose places lie in its area and have no commit yet. The session for
`anti` also takes the comment on `WRAP_COLUMNS` in `src/anti/fmt.c`. The
session of the tests takes the chapters of the book in the comments of
`tests/CMakeLists.txt`.

CONTINUE
