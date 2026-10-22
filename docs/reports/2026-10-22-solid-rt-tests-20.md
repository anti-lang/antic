# Step rt-tests, twentieth session

The session went on from the list of `2026-10-22-solid-rt-tests-19.md`,
which stays, as do the reports of the sessions before it. It wrote tests
for regions of `src/rt/` and `src/anti/` that a program, a command or a
unit case reaches, and gave the reason for regions nothing reaches. Logs
are under `build/drive/logs/`, named `rt20-*`. The list of the regions
not reached is `rt19-end-unreached.txt` at the start and
`rt20-end-unreached.txt` at the end, which `build/rt20/unreached.py`
writes from the export of `build/rt20/export.sh`. The probes are in
`build/rt20/`.

## Coverage per directory

Regions reached. The start is `rt19-end-summary.txt`, measured at
`6308898e`, and no source changed between it and `1dd2d432`. The end is
`rt20-end-summary.txt`, measured at `614655c2` with `./c coverage part
1/5` to `5/5`, `rt20-end-1.log` to `-5.log`, in which every test passed.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 96.01% of 9252 | 96.08% of 9252 | 369, 363 | 96.22% | 96.25% |
| `src/antic/` | 98.20% of 55587 | 98.20% of 55587 | 1000, 1000 | 98.27% | 98.27% |
| `src/anti/` | 99.09% of 11663 | 99.13% of 11663 | 106, 101 | 99.08% | 99.14% |

The `0` of `copy_path` of `platform_posix.c` 483 counts among the
regions `src/rt/` gained. It needs a frame that `dladdr` gives no file
for, which one run meets and the next does not.

## The tests

- runtime, `std_regex_value_before_next`: `value` of a walk of
  `find_all` before its first `next` stops at the `catch fatal` with
  status 1, the none of `anti_rt_regex_current` of `patterns.c`,
  `9def62c7`.
- runtime, `clib_classes`: a C object that no init has set up hashes as
  0 and equals no other object, the `0` of `anti_lang_Object_hash` of
  `object.c`, `ff306851`.
- runtime, `unit_regex`: the message of a refusal of a byte pattern in
  no room and cut to a short room, `anti_rt_regex_message_into` of
  `regex.c` 539 and 551, `4e136a72`.
- anti, `anti_check_message_modules_import_each_other_after_one_in_order`:
  a cycle after a module the order took, `unit_order` of `units.c`,
  `d0ad77d7`.
- anti, `anti_doc_message_library_beside_a_source`: `anti doc` of a
  library file beside a source, `write_interfaces` of `doc.c`,
  `fe15ec1e`.
- anti, `unit_files_oom`: `files_array`, `files_resize` and `files_grow`
  of `files.c` on a count whose bytes a `size_t` cannot hold,
  `614655c2`.

## Findings

The step closed none and recorded none. `tests/known-failures.txt` held
133 lines and 114 entries, 112 expected to fail on the Mac, before and
after. No feature, option or command, and no `[provisional]` entry.

## Regions nothing reaches

- `src/rt/text.c` 424, the `-1`: the upper bound of `shortest` is a power
  of ten only where `2m + 1` is `5^23`, the neighbours of `1e23`. Their
  `m` is even, so one digit ends the search before `compare` meets two
  points.
- `src/rt/sync.c` 258: the parser refuses a `select` without a channel.
- `src/rt/regex.c` 103: a count of `INT64_MAX / 8` bytes. 594: PCRE2
  answers `PCRE2_ERROR_NOUNIQUESUBSTRING` for a name that stands in the
  table more than once, so the scan finds it.
- `src/rt/object.c` 243: every literal, `switch` arm and `deserialize`
  writes the tag of a case, so the tag of a variant names one.
- `src/rt/trace.c` 505: the walk reaches `j = i`, whose base is its own,
  and returns there.
- `src/rt/plugin.c` 1353: `anti_rt_conf_get` gives `""` for a key without
  a value, so `dirs` is never NULL.
- `src/antic/diagnostic.c` 70 to 75: `diagnostics_check` has no caller.
  The checker calls `diagnostics_vcheck` through `sema.c` 60.
- `src/antic/ir_print.c` 262: the parser takes `...` after a parameter
  alone, so a variadic function has one.
- `src/antic/header.c` 694: R062 refuses a `char` lane.
- `src/antic/llvm_emit.c` 657: `--bundle-runtime` is refused beside any
  library but a static one, which carries no notice.
- `src/anti/units.c` 233: a unit the order left out waits on another one
  left out, or the loop of `unit_order` would have placed it.
- `src/anti/build.c` 198: `manifest.c` 425 refuses a manifest without a
  name. 687: a project without a source stops before the build.
- `src/anti/licensing.c` 472: the widest number of an archive header has
  15 digits, and a `size_t` of 64 bits holds 19.
- `src/anti/platform.c` 401: `fdopen` of a readable descriptor fails
  only without memory for the `FILE`, which no fault of the seam reaches.
- `src/anti/zip.c` 149 to 152: a symbols archive past 4 GiB.
- `src/anti/units.c` 38 to 41, `check.c` 61, `doc.c` 851, `test.c` 58,
  `main.c` 421 and `sdk.c` 145: `target_host` fails on no host anti
  builds for.
- `src/anti/fmt.c` 430: every call enters `angle_list` with `half` clear,
  and the loop goes round only after 462 found it clear. 981: every
  caller of `space_before` passes a token, and `emit_comment` takes the
  comments. 1470: the `flush` of the first line of the comment cleared
  `blank`.

## Gates

- No warning of our code: `rt20-hb1.log` to `-hb3.log`,
  `rt20-asan-build.log` and `rt20-ubsan-build.log`.
- Host, 5789 tests, `-I 1,2000` and `-I 2001,5789`: 2000 and 3816 passed,
  in 367 s and 321 s. Part 2 runs 27 fixtures of part 1 again. 112
  expected failures fail as listed, and the two `mimalloc_environment`
  tests skip: `rt20-host-1.log` and `rt20-host-2.log`.
- ASan, 5787 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5787`: 1151, 1106, 1577, 906 and 1089 passed in 240 s, 185 s,
  190 s, 447 s and 356 s, `rt20-asan-1.log` to `-5.log`.
- UBSan, the same parts: 1151, 1106, 1577, 906 and 1089 passed in 238 s,
  164 s, 140 s, 172 s and 295 s, `rt20-ubsan-1.log` to `-5.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged,
  `rt20-ident.log`.
- The docs-style checker reports nothing on this report.

## State at the push

```text
$ git log --oneline -3
614655c2 Test the allocation helpers of anti on a count past size_t
fe15ec1e Test anti doc of a library file beside a source
d0ad77d7 Test an import cycle after a module the order took
$ git status --short
?? docs/reports/2026-10-22-solid-rt-tests-20.md
$ git rev-parse HEAD origin/main
614655c2fcbfe6bfefff785c0773fb8dd787aedc
614655c2fcbfe6bfefff785c0773fb8dd787aedc
```

The one untracked file is this report, whose commit follows the push.

## Left for the next session

- `src/anti/`, 101 places. `bindwrite.c` and `fmt.c` hold the most.
  `build/rt20/untriaged.py anti` lists the lines no report has given a
  reason for, once `build/rt20/triaged.txt` takes this report's list.
- `src/antic/`, 1000 places. `sema_generic.c`, `sema_copies.c`,
  `whole_tables.c`, `coff.c` and `llvm_emit_function.c` hold the most.
  `sema_const.c` 689 and `sema.c` 314 did not yield to the probes of
  `build/rt20/t`.
- `src/rt/`, 363 places: `trace.c` of ELF and PE, and `plugin.c` 218,
  229, 642, 719 and 864, which need a damaged library or a slot that
  names no function.

CONTINUE
