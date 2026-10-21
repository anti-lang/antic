# Step rt-tests, fifteenth session

The session went on from the list of `2026-10-21-solid-rt-tests-14.md`,
which stays, as do the reports of the sessions before it. It wrote tests
for regions of `src/anti/`, `src/rt/` and `src/antic/` that a program, a
command or an injected fault reaches. Logs are under `build/drive/logs/`,
named `rt15-*`. The list of the regions not reached is
`rt14-end-unreached.txt` at the start and `rt15-end-unreached.txt` at the
end. The probes are in `build/rt15/`.

## Coverage per directory

Regions reached. The start is `rt14-end-summary.txt`, measured at
`03e6b831`, whose sources equal those of `3506ca7b`. The end is
`45a2ab21`, `rt15-end-summary.txt`, from `./c coverage part 1/5` to
`5/5`, `rt15-end-1.log` to `-5.log`, every part passed.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 95.70% of 9252 | 95.81% of 9252 | 398, 388 | 96.04% | 96.07% |
| `src/antic/` | 98.11% of 55587 | 98.13% of 55587 | 1051, 1038 | 98.16% | 98.19% |
| `src/anti/` | 98.42% of 11663 | 98.54% of 11663 | 184, 170 | 98.45% | 98.60% |

## The tests

- antic, `unit_modules`: a malformed pattern literal across a CRLF line
  break stands at the byte PCRE2 names on the second line,
  `pattern_position` of `sema_pattern.c`, `c03a2f01`.
- runtime, `faults_trace_copy` and its case `name_memory`: the readable
  name of a frame of `inside<int>` that runs out of memory leaves the
  escaped symbol, `function_name` of `trace.c`, `41c00ad8`.
- runtime, `anti_build_message_run_lines_of_a_trace_through_two_modules`:
  a dev build of `anti run` reads the line of each frame from the object
  of its module, the walk of `load` in `trace.c`, `76de7dd3`.
- runtime, the cases `name_memory_g`, `object_path_memory`,
  `object_memory` and `object_refused` of `faults_trace_copy`: a case of
  fault injection takes the word `g`, which builds with `-g`. An object
  whose path or bytes cannot be kept, or that cannot be opened, gives the
  line 0, `43f6b15e`.
- anti, five cases of `tests/check/messages.cmake`: the file of a user
  block and of a dev block not written, the source of a dev block not
  read, and the two directories not made, `compile_block` of `check.c`,
  `681814a9`. The two directories are T153.
- anti, four cases `anti_bind_message_fault_json_*_memory`: `list_add` of
  `jsontree.c` runs out of memory for the values or the names of an array
  or an object, `ebe9d2af`.
- anti, `unit_syms`: an entry whose data no longer matches its CRC
  refuses a deployment archive and the archive of one build,
  `load_deployment` and `load_archive` of `syms.c`, `cd69dd9e`.
- antic, `antic_option_bundle_static_linux`: `--bundle-runtime` of a
  static library for Linux joins the runtime with `ld.lld -r`,
  `join_bundle` of `driver_library.c`, `a3b5e9a6`.
- antic, `error_dump_opt_without_a_provider`: `--dump-opt` stops at the
  error of the whole-program pass, `dump_ir` of `driver.c`, `547180ab`.
- antic, `plugin_host`: an empty string of a kept entry of
  `anti-plugins.toml` comes back as `''`, `index_string` of
  `driver_index.c`, `42de8714`.
- antic, two refusals of the matrix: unary `-` and `+=` on a simd struct
  of `char` lanes add no message to the one of the declaration,
  `sema_simd.c` 34 and 139, `45a2ab21`.

## Findings

The step closed none and recorded none. T153 gains the two tests
`anti_check_message_fault_user_block_directory_not_made` and
`anti_check_message_fault_dev_block_directory_not_made`: anti check ends
with 1 and no message when the directory of a doc block cannot be made.

`tests/known-failures.txt` held 123 lines and 104 entries, 102 expected
to fail on the Mac. It holds 125 lines and 106 entries, 104 expected to
fail on the Mac. No feature, option or command, and no `[provisional]`
entry. The word `g` of a case of fault injection belongs to the tests alone.

## Regions nothing reaches

- `src/rt/trace.c` 471 to 475, 536 to 538, 545 to 547 and 562 to 564:
  `add_line` fails for a line of `LINE_ROOM` bytes or more, and every
  line is a fixed format of two numbers and a word. 317: the walk of
  `platform_posix.c` stops at a return address of 0, and a frame outside
  every module needs code outside every image. 431: the format of a
  Mach-O host is never PE.
- `src/rt/platform_posix.c` 895 and the lower levels of
  `anti_rt_arm64_level`: the M-series Mac answers every feature of
  `armv8.5`.
- `src/antic/driver.c` 639 and 641: every error line of the whole program
  ends with a newline.
- `src/antic/sema_pattern.c` 247: `matches` is the one method of a fixed
  count, and it takes one argument.
- `src/anti/doc.c` 480: the one caller of `doc_markdown` returns before
  an empty text.
- `src/anti/jsontree.c` 60: the second scan of a string reads what the
  first one counted. 90 and 91: a room whose double overflows needs an
  input larger than memory.

## Not run on the target

- `faults_trace_copy` and its five cases count the allocations and the
  opens of the Mac. ELF and PE read the line from the program itself,
  so on new VMs the counts of the `g` cases may name other calls.
- `anti_build_message_run_lines_of_a_trace_through_two_modules` reads the
  lines through `symbolize_elf` and `symbolize_pe` there.
- `antic_option_bundle_static_linux` runs on every host that has the
  sysroot of linux-x86_64.

## Questions

- `repo.c` 124: on the Mac `file:///C:/repo` drops its leading slash and
  reads `C:/repo` relative to the project. `docs/notes/hosts-and-harness.md`
  gives the form for Windows alone, so no test pins what a drive letter
  means on macOS and Linux.

## Gates

- No warning of our code: `rt15-hb.log`, `rt15-asan-build.log` and
  `rt15-ubsan-build.log`.
- Host, 5724 tests, `-I 1,2000` and `-I 2001,5724`: 2000 and 3751 passed,
  in 377 s and 330 s. Part 2 runs 27 fixtures of part 1 again. 104
  expected failures fail as listed, and the two `mimalloc_environment`
  tests skip: `rt15-host-1.log` and `rt15-host-2.log`.
- ASan, 5722 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5722`: 1151, 1106, 1577, 906 and 1024 passed in 239 s, 187 s,
  181 s, 456 s and 379 s, `rt15-asan-1.log` to `-5.log`.
- UBSan, the same parts: 1151, 1106, 1577, 906 and 1024 passed in 242 s,
  168 s, 143 s, 184 s and 323 s, `rt15-ubsan-1.log` to `-5.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged.
- The docs-style checker reports nothing on this report.

## State at the push

```text
$ git log --oneline -3
45a2ab21 Test unary minus and a compound assignment on char lanes
42de8714 Test an empty string of an entry that antic keeps in a plugin index
547180ab Test --dump-opt of a program whose whole-program pass fails
$ git status --short
$ git rev-parse HEAD origin/main
45a2ab2179ef9d6ae131ae2e6aeedcf40ab6e2c2
45a2ab2179ef9d6ae131ae2e6aeedcf40ab6e2c2
```

This report is written after that push, and its commit follows it.

## Left for the next session

- `src/anti/`, 170 places: `bindclang.c` and `bindwrite.c`, `fmt.c`
  430, 981 and 1470, `build.c` 1186 and 1473.
- `src/antic/`, 1038 places. `build/rt15/untriaged.txt` lists those no
  report has given a reason for yet, `sema_generic.c`, `sema_copies.c`,
  `driver_link.c`, `whole_tables.c` and `antl_tree.c` among the most.
  `lower_place.c` 138 to 141 is not reached by a generic of a library
  that reads a static field, `build/rt15/m/st`.
- `src/rt/`, 388 places: `trace.c` of ELF and PE, `trace.c` 117 and 118,
  `plugin.c`, `object.c` and `registry.c`.
