# Step 33, the minor findings of the library file

Step 33 of "Fix steps" in `docs/audit/summary.md` takes the minor
findings one area per session. `docs/reports/2026-10-19-solid-step33.md`
reports the front end, `docs/reports/2026-10-19-solid-step33-middle.md`
the middle and `docs/reports/2026-10-19-solid-step33-back-end.md` the
back end. This session took the library file and its leaf code: the
seven files of `antl`, the pointer set, `sha256.h`, and the TOML and
pattern code of the runtime that antic compiles. It also took
`antl_io.h:12`, which the back-end session left to it. The logs stand in
`build/drive/logs/step33-library-file/`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 26, `antl_tree.c:1377` | `a775d7ee` | The reader and the writer wrote the comparison of two names four times beside `types_same_name` and `antl_name_equals`. |
| One teardown rule, `ptrset.h:8` | `0edb4180` | The set had no function that frees it, so twenty places freed its field by name. |
| State without an owner, `antl_tree.c:845` | `6f1de1ce` | The walker set the room of the captures in all three passes, the writing pass among them. |
| A boundary, `antl_io.h:12` | `5ea90f45` | `antic.c` included the inside header to compare the magic. |
| Placement, `src/rt/regex.h:98` | `35687d33` | `regex.h` did not mark which file defines each group, and `sha256.h` handed on `digest.h`. |
| Comments, `antl_verify.c:13` | `02fe7839` | The verifier claimed every rule, `antl_header` named less than it reads, and two places gave `antl.c` every check of the enums. |
| A stale comment, `src/rt/toml.c:1` | `a705eedf` | The comment gave the logger as the reason and named basic strings alone. |
| Rule 14, `antl_tree.c:1459` | `3c3a6775` | Fixed before this session, and `runtime_names` of `test_modules` holds it. |
| Rule 14, `antl_read.c:515` | `89659227` | Fixed before this session, and `flags_refused` holds it. |
| Rule 14, `antl_read_ir.c:417` | `b4b0f9f0` | Fixed before this session, and `wide_addresses` holds it. |
| Rule 14, `antl_read.c:917` | `548f0370` | Fixed before this session, and `field_kinds` holds it. |

The summary already recorded rule 5 at `src/rt/toml.c:104`, rule 12 at
`src/rt/regex.c:491`, the NUL of a group name, the scan of `[[name]]`
and `versions.md:41` as fixed. `8f094a03` writes the commits of this
session into the summary.

The tests. `writer_keeps_trees` of `tests/unit/test_modules.c` failed
before `6f1de1ce`: the room of four captures came out as one. The unit
tests of `ptr_set_free` and `antl_starts` did not build before their
fixes. `header_sections` now refuses a file outside the family of a
shared header that includes it, and it failed on `antic.c` before
`5ea90f45`. `string_forms` of `tests/unit/test_toml.c` holds the strings
the new comment of `toml.h` names. `a775d7ee` and the include and
comment changes of `35687d33` change no output, and `emit_identity` and
`link_identity_macos-arm64` pass unchanged.

## Findings left

- Rule 18 of the library file belongs to step 34, which names it.
- The table "The sentence of each file" of the summary still says
  `antic.c` includes `antl_io.h`. It records what the audit read, and
  the row of `antl_io.h:12` now names its fix.

## New findings, provisional entries, known failures

No new finding and no `[provisional]` entry.
`tests/known-failures.txt`: 29 lines before and 29 after.

## Gates

- The three builds report no warnings, `build-host.log`,
  `build-asan.log` and `build-ubsan.log`.
- Host, 5132 tests, `-I 1,2000` and `-I 2001,5132`, 322 s and 271 s. All
  2000 and 3153 pass, since a part also counts the fixtures it pulls in.
  The 8 expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip, `host-1.log` and `host-2.log`.
- ASan, 5130 tests, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5130`, 275 s,
  364 s and 364 s. All 1150, 1113 and 2901 pass with the same skips,
  `asan-1.log` to `asan-3.log`.
- UBSan, the same parts, 258 s, 257 s and 291 s. All 1150, 1113 and 2901
  pass, `ubsan-1.log` to `ubsan-3.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged.
- The docs-style checker reports nothing on each `.md` file the session
  touched, `docs1.log` to `docs5.log`.

## Not run on the target

None. No change touches a path of a platform layer.

## State at the push

```text
$ git log --oneline -3
8f094a03 Record the library-file minor findings of step 33 in the summary
a705eedf Name every reader of the TOML subset and the strings it takes
02fe7839 Say where the rules of a library file stand, and what antl_header reads
$ git status --short
$ git rev-parse HEAD origin/main
8f094a033d439ddfd920497baae07542f247ac44
8f094a033d439ddfd920497baae07542f247ac44
```

## Left for the next session

The other areas of step 33, one per session, in the order of the
summary: the runtime, the standard library, `anti`, the build and the
tools, and the tests. Each takes the rows of the minor tables in
`docs/audit/summary.md` whose places lie in its area and have no commit
yet. The session for `anti` also takes the comment on `WRAP_COLUMNS` in
`src/anti/fmt.c`. The session of the tests takes the chapters of the
book in the comments of `tests/CMakeLists.txt`.

CONTINUE
