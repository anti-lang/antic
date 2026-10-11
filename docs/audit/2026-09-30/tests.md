# The tests and their scripts

This audit read `tests/` at `238a4f1`, whose `tests/`, `src/` and `tools/`
equal those of `70e8860`, the commit of the tool pass. It covers the 26
directories of `tests/`, `tests/CMakeLists.txt` with its 3917 lines, the 108
`run_*.cmake` scripts, `program_output.cmake`, the unit tests in C and the
tests that `src/native/` registers. It judged five things: one kind of test
per directory, names that say what a test checks, tests that pass whatever
the code does, leak checks that count teardowns as
`docs/notes/value-rules.md` asks, and runner code copied between scripts.
Scripts compared every pair of helper functions of the same name, every
leak check against the classes it counts, every registration against the
files on disk, and the logs of the last three suite runs. Each finding was
then read in the code. One foreground agent mapped the "Built" lines of
`docs/anti-syntax-overview.md` to tests. Each gap it reported was checked
by hand, and one of its claims, that no test runs `CallLogger`, was wrong
and is left out. The tool pass holds no data on `tests/`, so every item
here comes from this pass. The first audit in `docs/audit/2026-09-23/`
did not audit the structure of `tests/`, so every finding below is new. Its M31, the
readers without a malformed-input test, stays with the readers and is not
graded again here. No build ran, and no file other than this report
changed.

For C in `tests/`, "Scope and bar" of `docs/c-guidelines.md` asks that
the tests be correct and hide no failure. It exempts them from the
structure and naming rules. The questions below are the five of the task,
with "Uncovered features" for the Built lines.

## Counts

| Severity | Rule or question | Findings |
|---|---|---|
| Major | Tests that pass whatever the code does | 1 |
| Major | Leak checks count teardowns | 1 |
| Major | Uncovered features | 3 |
| Major | Duplicated runner code | 4 |
| Minor | Tests that pass whatever the code does | 1 |
| Minor | Duplicated runner code | 3 |
| Minor | Duplicated expectations | 1 |
| Minor | One kind per directory | 1 |
| Minor | Names that say what they check | 1 |
| Minor | Uncovered features | 1 |
| Minor | Dead runner code and a misplaced comment | 2 |
| Minor | Rule 4 | 1 |

No finding is severe. All 20 are new.

## Suite times

The last runs on the Mac, on 2026-09-29, at `-j8`: the `host` suite ran
1284 tests from 15:29 to 15:32, ASan 1283 from 15:32 to 15:36 and UBSan
1283 from 15:36 to 15:40. The times below are the elapsed time of each
test in `Testing/Temporary/LastTest.log` of `build/host`, `build/asan`
and `build/ubsan`, with seven other tests running beside it.

| Suite | Sum of test times | Longest tests, seconds |
|---|---|---|
| host | 1455 s | `package_keys` 81, `checks` 66, `deps_dir` 62, `release_dry_run` 29, `unit` 28, `plugin_versions` 23, `plugin_host` 17 |
| ASan | 1503 s | `emit_identity` 267, `package_keys` 102, `unit` 85, `deps_dir` 79, `checks` 41, `release_dry_run` 21, `overview_examples` 14 |
| UBSan | 1678 s | `unit` 136, `emit_identity` 120, `package_keys` 106, `deps_dir` 77, `checks` 40, `release_dry_run` 21, `std_sync_map_dev` 10 |

By family, the 425 `program_*` tests take 680 s of the host sum and the 115
`std_*` tests 179 s. 607 host tests finish in under a second.

What sets the wall time:

- Under ASan, `emit_identity` takes 267 s, about as long as the whole
  suite. `run_emit_identity.cmake:21` compiles 177 programs for six
  targets, 1062 runs of antic, one after another in one test. One test per
  target would let ctest run the six at once.
- `unit` is one process for 44 test files, with no way to run a part.
  `tests/unit/test_main.c` calls every group in turn. Under UBSan it is the
  longest test.
- `checks` runs 26 programs in one test and compiles nine modules of the
  standard library in dev mode on the way, which the `dev_object_*`
  fixtures already compile. See the minor finding on dev objects.
- `deps_dir` copies the repository and builds it with `-j8` from inside a
  suite that already runs eight tests.

## Major

### Refusal tests pass on their output and ignore the exit status

`tests/CMakeLists.txt:1743`, tests that pass whatever the code does,
major, new.

`error_missing_semicolon` sets `PASS_REGULAR_EXPRESSION`, and ctest then
ignores the exit status, as `cmake --help-property
PASS_REGULAR_EXPRESSION` says. The comment at line 1738 reads "Each test
passes when antic's output matches the pattern." An antic that prints the
message and exits 0, having written the `.s`, passes. Three patterns
lack `error:`, so an error turned into a warning passes too:
`error_runtime_module` at 1754, `error_reserved_library` at 1761 and
`error_no_main` at 1931. `run_error.cmake:17` refuses exit status 0 and
compares the bytes, so the suite holds two ways to test a refusal, and
only one checks it.

Other places: 2091, 2099, 2105, 2130, 2182, 2767, 2775, 2789, 2796 and
3916 for refusals, and 1776, 1840, 1850 and 1924 for warnings, where a
crash after the warning passes. `inject_manifest_*` at 3162 and
`inject_standard_*` at 3181 pass on the summary line of `anti test`
whatever its status. The fix is to run each through `run_error.cmake` or a
script that checks the status.

### Leak checks count the leaves and not the teardowns of the keys

`tests/programs/owning_values.anti:24`, leak checks count teardowns,
major, new.

`class Key { pub n: int = 0, pub own leaf: ?*Leaf = none, }` has no
`destruct`, and the test counts `Leaf.live` alone. The subject of the
file is the teardown of structs and tuples that hold a `Key`. A second
teardown of a `Key` finds `leaf` cleared by the first and frees nothing,
so `Leaf.live` stays right, which is the trap "Leak checks" of
`docs/notes/value-rules.md` describes. `caller_params.anti:29` shows the
form the note asks for, `static atomic torn: int = 0;` raised in
`destruct`. So do `destroy_locals`, `generic_moves`, `literal_moves`,
`variant_owning` and `std/collection_variants`.

Other places: `walk_owned_parts.anti:25`, whose subject is the teardown of
each part on every exit of a loop, `alloc_owning.anti:20`, and `Shelf` at
`own_params.anti:30`, whose file comment says "a teardown missed or run
twice shows in the count", which holds for `Box` and not for `Shelf`.

### The refusals of `link linux` are never run

`tests/errors/link_linux.anti:1`, uncovered features, major, new.

`link_linux.anti` with its `.err`, and `link_linux_syntax.anti` with its
`.err`, stand in `tests/errors/`, and no registration names either. Their
twins `link_framework` and `link_framework_syntax` are registered at
`tests/CMakeLists.txt:3562`. The refusals of a repeated name, an empty
name, a path and a name that is no string for `link linux` therefore run
nowhere. They are the only files of `tests/errors/` that no test reads.

### A replacement of a `final fn` is neither refused nor tested

`src/antic/sema.c:3028`, uncovered features, major, new.

`docs/anti-object-model.md:235` says "A replacement of a `final fn` is an
error", and line 341 gives the message `` `Circle.area` replaces `final`
function `Shape.area` ``. `check_contracts` finds the base function with
`matched_above` and passes a `concrete fn` to `check_replacement`, and
neither reads `is_final`. No source file holds the message, and no test
replaces a `final fn`. `sema_call.c:1042` and `lower.c:1681` call a
`final` function directly, so a replacement that compiles would be passed
over by those calls. The refusal of a class that inherits a `final` class,
`sema.c:2422`, exists, and no test reaches its message: `grep -rn "cannot
inherit" tests` finds nothing. The overview's Built line 679 claims both.

### Two refusals of the plugin loader are never triggered

`src/rt/plugin.c:559`, uncovered features, major, new.

The Built line 1270 says "The load checks the runtime version exactly and
that every interface is the host's". No test holds the message "was built
for runtime" of line 559. None holds "is not the interface this program
carries" of line 231 either. `run_plugin_versions.cmake:110` to `119` loads
well-formed older and newer libraries, one with an added field and one
below the floor. None was built for another runtime or holds another
interface of the same name.

### The object suffix is spelled in five places, and the dev tests across modules skip Windows

`tests/run_modules.cmake:71`, duplicated runner code, major, new.

`list(APPEND program_inputs "${WORK}/std_${module}.o")` names the object
of a dev build with `.o`, as does line 53 and `run_dev.cmake:29` and
`61`. antic writes `.obj` on Windows, which `tests/CMakeLists.txt:95`
records in `anti_object`, `run_trace.cmake:34` from `CMAKE_HOST_WIN32` and
`run_anti_build.cmake:24` from `HOST`. `run_checks.cmake` takes it as
`OBJECT`. The 15 cross-module tests that run `run_modules.cmake` in dev
mode skip Windows with `if(mode STREQUAL "dev" AND WIN32)`, at 2275, 2298,
2350, 2371, 2392, 2413, 2434, 2455, 2474, 2493, 2513, 2534, 2554, 2576
and 2595, and `dev_modules` at 2632 does the same. No comment and no note
gives a reason. So no Windows host links a dev program across modules,
and the suffix the scripts spell is the likely cause. The name has one
definition in `src/antic/target.c`, and the scripts could take it as a
value.

### The five pin checks of the native libraries have drifted apart

`tests/run_pcre2_pin.cmake:46`, duplicated runner code, major, new.

`run_pcre2_pin`, `run_sqlite_pin`, `run_mbedtls_pin`, `run_miniaudio_pin`
and `run_raylib_pin` repeat one check with the names changed. The check
that no other file spells the version escapes the dots first in
`run_sqlite_pin.cmake:50` and `run_mbedtls_pin.cmake:44`. The other three
match the version as a pattern, `if(text MATCHES "${literal}")`, at
`run_pcre2_pin.cmake:46`, `run_miniaudio_pin.cmake:47` and
`run_raylib_pin.cmake:37`, where `.` matches any byte. The sixth form,
`run_pin.cmake`, is in the minor finding on dead runner code. One script with
the library as a value would hold the check once.

### Five runners run a program that must stop, and they disagree

`tests/run_table.cmake:19`, duplicated runner code, major, new.

`check` of `run_checks.cmake:30`, `run_table.cmake`, `run_lock_order.cmake`,
`run_parallel_size.cmake` and `run_pattern_limit.cmake` each build a
program, run it and match its standard error. The last two differ only in
the program name and the pattern. Their helper `build` has drifted: the
copy in `run_lock_order.cmake:18` fails on any output of antic, `if(NOT
status EQUAL 0 OR NOT err STREQUAL "")`, and the copies in
`run_table.cmake:19` and `run_asserts.cmake:18` accept it. `run_program.cmake`
says antic "must print nothing" beside the warning a program is about, so
a new warning passes in `table_unset` and `asserts` and fails in
`lock_order`. The exit codes each accepts differ too:
`run_pattern_limit.cmake:22` refuses 0 and 1, and `run_parallel_size.cmake:21`
refuses 0 alone.

### Scripts compare the output of a program without the raw-bytes rule

`tests/run_plugin.cmake:25`, duplicated runner code, major, new.

The `DESIGN` comment of `tests/program_output.cmake:1` says an
`OUTPUT_VARIABLE` "loses the CR of each CRLF and every NUL", so the
scripts read a program's output from a file. `run_plugin.cmake:18` reads
the output of the host and of the programs with a library provider into
an `OUTPUT_VARIABLE`, and `same` at line 25 compares it with an expected
file. A Windows program that writes CRLF passes there. Other places:
`run_lock_order.cmake:33` and `42` compare `"2\n"`, and
`run_linux_modes.cmake:79` and `84` compare fixed lines, on hosts where the
difference cannot show today.

## Minor

### Tests drop out or pass without running on some hosts

`tests/CMakeLists.txt:1313`, tests that pass whatever the code does,
minor, new.

`find_program(ANTIC_STRINGS strings)` takes the `strings` of the host,
which the pinned LLVM tools do not carry. Without it, `no_paths` at 1316,
`asserts` at 2801 and `checks` at 2815 are not registered, and the run
says nothing. `checks` is the one run-time test of the dev-mode checks. On
a windows-arm64 host, `run_anti_memory_checks.cmake:20`,
`run_memory_checks.cmake:84` and `run_memory_checks_list.cmake:17` return
before any check and report a pass. The rest of the suite prints `SKIP: `
and sets `SKIP_REGULAR_EXPRESSION`, which shows the test as skipped.

### The dev objects of the standard library are compiled in four places

`tests/run_checks.cmake:96`, duplicated runner code, minor, new.

The fixtures `dev_object_*` of `tests/CMakeLists.txt:605` to `1135` compile
the modules of the standard library in dev mode once per suite.
`run_checks.cmake:98` to `183` compiles nine of them again,
`run_trace.cmake:39` compiles those its program needs, and
`run_modules.cmake:61` those of its case. The comments at
`run_checks.cmake:96` and `run_modules.cmake:58` say no tool builds these
objects yet, while the fixtures do. The copy in `run_modules.cmake` is the
one with the wrong suffix above. Each program test then spells its
objects and its fixtures as two lists that must agree, as at lines 1059
and 1064.

### The registrations repeat their arguments instead of a helper

`tests/CMakeLists.txt:479`, duplicated runner code, minor, new.

The block that runs `run_program.cmake` is written out at 36 sites with
the same five arguments, `run_dump.cmake` at 32 and `run_modules.cmake` at
17. `"-DANTIC=$<TARGET_FILE:antic>"` stands 159 times and
`"-DLLVM_MC=${ANTIC_LLVM_MC}"` 104 times. A function per runner would
hold each list once.

### The helpers of the unit tests are copied, and one copy has drifted

`tests/unit/test_sema.c:28`, duplicated runner code, minor, new.

`run`, `accepts` and `rejects` stand in `test_sema.c`, `test_sync.c`,
`test_variant.c` and `test_nullable.c`, and `accepts` and `rejects` also
in `test_modules.c`. `run`, `emits` and the pipeline of 55 lines behind
them stand in `test_x86_64.c`, `test_arm64.c`, `test_float.c`,
`test_struct.c`, with forms in `test_emit.c`, `test_select.c` and
`test_regalloc.c`. `test_sync.c` and `test_variant.c` guard the message
of a failed parse with `c->diags.count > 0 ?`, and `test_sema.c:28` and
`test_nullable.c` read `c->diags.items[0].message` without it, which
rule 6 asks be checked. The six executables of `tests/CMakeLists.txt:8`,
`114`, `122`, `138`, `154` and `171` also list the files of `src/rt/` and
`src/anti/` they compile by hand, while the `DESIGN` comment of
`tools/sources.cmake:4` says "One list leaves nothing to fall out of
step."

### Allocations in the unit tests are not checked

`tests/unit/test_utf.c:11`, rule 4, minor, new.

`unsigned char *out = malloc(3 * n + 1);` is used without a test. 25 of
the 33 allocations of `tests/unit/` are unchecked, among them
`test_x86_64.c:44` and the same line in the copies of the finding above,
`test_float_read.c:73`, and `test_zip.c:519`. The grade is minor under
"Scope and bar": a NULL ends the process with a crash, `unit` then
fails, and no failure is hidden.

### Directories hold more than one kind of test

`tests/CMakeLists.txt:320`, one kind per directory, minor, new.

`program_simd_big` runs `tests/dump/simd_big.anti` as a program test.
`tests/dump/` also holds the sources of the 147 `asm_*` tests, the
expected output of `llvm-readobj --unwind`, the sources of `cpu_level` and
a licence notice. `tests/traps/` and `tests/checks/` both hold programs
that must stop with a message, under five different scripts.
`tests/check/`, projects for `anti check`, and `tests/checks/`, dev-mode
checks, differ by one letter. The projects of `anti build`, `anti test`
and `anti symbols` stand in `anti-build/`, `anti-test/` and
`anti-symbols/`, and those of `anti check`, `anti doc`, `anti fmt` and
`anti bind` in `check/`, `doc/`, `fmt/` and `bind/`. The 108 scripts lie
flat in `tests/`, and their names do not follow the directories they
read: `run_table.cmake` reads `traps/`, `run_dump.cmake` reads `dump/`,
`opt/`, `errors/` and `modules/`.

### Test names do not say which kind of check they are

`tests/CMakeLists.txt:3678`, names that say what they check, minor, new.

A dump compared with an expected file is named `dump_*`, `listing_*` or
`emit_*`, as `dump_ast_scale`, `listing_devirt.ir` and
`emit_main.linux-x86_64`. A refusal is `error_*` or `listing_error_*`, and
120 of them carry `listing`, the name of the table in the file.
`listing_header/main.ir` holds a slash. The target part of `dump_alloc_*`
is a suffix, an arch or a middle word: `dump_alloc_args.linux-arm64`,
`dump_alloc_x86.arm64` and `dump_alloc_arm64`. `unit` names 44 files and
`checks` 26 programs, so a failure names neither.

### Smaller features without a test

`src/antic/main.c:265`, uncovered features, minor, new.

- `--no-trace`, parsed at `main.c:265`, which no test passes.
- `trace.start`, which reads the runtime key `trace`, at
  `src/std/anti/trace.anti:831`, which no test calls. Tests call `trace.install` alone.
- A `protected fn` called from a derived class is checked by the front end
  in `errors/visibility.anti:18` and never compiled and run.
- The stub the loader writes for a slot only `reflect.call` reaches,
  `src/rt/plugin.c:275`. `plugin/versions/reader.anti` loads an older
  library and never calls through it.
- The refusal of `import anti.regex.{Regex};`, which the Built line 120
  names. `unit/test_parser.c:508` parses it, and no test checks it.
- The Built line 1231 names `f"..."(from)`, which
  `errors/format_call.anti:10` checks is refused. The overview is stale
  there.

### Dead runner code

`tests/run_start.cmake:1`, dead runner code, minor, new.

No registration runs `run_start.cmake`, which links with the `CC` of the
host and names a `start.c` of chapter 1.
`docs/reports/2026-09-26-warnings.md:51` recorded that, and the file
stayed. `run_pin.cmake:41` returns when `tools/get-cmake.cmake` is
missing, and it is, so the last check of `cmake_pin` never runs. Line 4 of
the same script points at `tests/run_llvm_pin.cmake`, which does not
exist. `tests/programs/.gitkeep` is tracked in a directory of 363 files.

### A comment stands above the wrong test

`tests/CMakeLists.txt:1305`, placement, minor, new.

"A small raylib binding: the raymath functions of the pinned raylib
release" and the two lines after it stand above `no_paths`. The raymath
test is at line 1704.

### The layout of the descriptor is pinned in 28 expected texts

`tests/unit/test_lower.c:547`, duplicated expectations, minor, new.

`type anti.rt.Descriptor = struct { name: ptr, ... }` with all 17 fields
stands in 12 expected texts of `test_lower.c`, `test_optimize.c` and
`test_modules.c` and in 16 files of `tests/dump/`. None of those tests is
about the descriptor, and a change to its fields rewrites all 28. A dump
that leaves out the types of `anti.rt` would keep the layout in the tests
that check it.
