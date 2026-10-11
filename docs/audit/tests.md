# The tests

This step audited `tests/` at `69fc7bee` against the three standards: the 56
files of `tests/unit/`, the 156 `run_*.cmake` scripts with
`program_output.cmake`, `tests/CMakeLists.txt` with its 4961 lines,
`tests/binary_stdio.h` and the 26 directories. It judged the five things the
task names: whether each unit test file tests one part and says so in its
header, whether the runners share code or copy it, which files of
`src/antic/`, `src/anti/` and `src/rt/` no unit test reaches, whether the
tests check behaviour or repeat the implementation, and whether an expected
file can be written again without reading what changed. It read what
`CLAUDE.md`, `docs/decisions.md`, `docs/notes/value-rules.md` and
`docs/notes/hosts-and-harness.md` say of the tests against the tree. No
`tests/README` exists.

How it was read. The first comment of all 56 unit files and of all 156
runners was read. Read in full: `program_output.cmake`, `binary_stdio.h`,
`check.h`, `pipeline.h`, the table and `main` of `test_main.c`, lines 1 to
480 of `tests/CMakeLists.txt`, and the runners `run_dump`, `run_expect`,
`run_program`, `run_emit_identity`, `run_link_identity`, `run_modules`,
`run_classlib`, `run_antl`, `run_shared_inputs`, `run_llvm_names`,
`run_probe` and `run_repo_layout`. The rest of `tests/CMakeLists.txt` and
of the unit files was read where a script pointed. Scripts under
`build/drive/logs/tests-audit/` measured the rest: the functions two
runners both define and whether their bodies agree, every build with antic
in a runner and what it checks afterwards, every path a comment names, and
the calls from `tests/unit/` into `src/`. The call graph is the one of
`docs/audit/data/function-metrics.txt`, each function with its body, an
edge where one names another before `(`. It matches by name, so a text
that holds a name counts as a call, and the lists below were corrected by
hand where that showed. The names of the 1701 registered tests come from
`CTestTestfile.cmake` of the host tree, configured on 2026-10-10. No build
ran, no suite ran and no reviewer agent was used.

For C in `tests/`, "Scope and bar" of `docs/c-guidelines.md` asks that the
tests be correct and hide no failure. It exempts them from the structure
and naming rules. A finding on a question of the task carries the standard
it belongs to and that question.

The second audit reported 20 findings on the tests in
`docs/audit/2026-09-30/tests.md`, and `closure.md` marks 19 fixed and one
left. The first audit did not audit `tests/`. Four items of
`docs/audit/tool-pass.md` lie in this area and are not counted again:
`tests/run_bench.cmake:14` names `--backend,native`, `--soname` and
`--strip-docs` have no test, `docs/decisions.md:500` names
`tests/run_pcre2_pin.cmake`, which is `run_native_pin.cmake` now, and rule
10 names a test of the banned calls that no file of `tests/` holds.

## Counts

| Severity | Standard | Findings | New | Returned | Still open |
|---|---|---|---|---|---|
| Major | Structure | 3 | 3 | 0 | 0 |
| Major | Safety, rule 15 | 1 | 1 | 0 | 0 |
| Major | Documentation | 2 | 2 | 0 | 0 |
| Minor | Structure | 8 | 6 | 0 | 2 |
| Minor | Safety, rule 6 | 1 | 1 | 0 | 0 |
| Minor | Documentation | 2 | 2 | 0 | 0 |

In all 17 findings: no severe one, 6 major and 11 minor. No finding of the
second audit returned.

What holds. `test_main.c` lists every one of the 51 entry functions under
the name of its file, and a name it does not know ends the run with status
2. Every allocation a search found in the C files of `tests/` is checked. No
registration sets `PASS_REGULAR_EXPRESSION`. Every file of `tests/errors/`
is read by a test, which `errors_registered` holds. The unit tests of the
tables check them against something outside the table: `runtime_functions`
of `test_lower.c:882` reads each row of `RT_FUNCTIONS` against the prototype
in `src/rt/`, `test_cpu.c` reads the defaults against the specification, and
`llvm_datalayout_pin` compares the strings of `llvm_target.c` with the
pinned clang. The 513 program tests and the 125 tests of the standard
library compare what a program prints, which is behaviour. The count of 1701
tests in `CLAUDE.md` is the count of the host tree, and every test
`CLAUDE.md` names is registered.

## Major

### The digest manifest is written again without a way to read what changed

`tests/run_emit_identity.cmake:52`, structure, whether a golden can be
re-pinned without reading what changed, major, new.

`tests/emit-identity/programs.sha256` holds 1332 digests, one per program of
`tests/programs/` and target. On the Mac the test fails after any change to
what antic writes, and `-DWRITE=yes` writes the manifest again. The failure
names the files that differ, `"antic wrote other LLVM text than the Mac for
${count} files"`, and line 24 removes the directory of the texts of the run
before, `file(REMOVE_RECURSE "${WORK}")`. So the old text is gone when the
new one is there, and the diff of the commit is a list of digests. "Pinned
outputs" of `docs/notes/value-rules.md:10` asks: "writes the file again from
this Mac, then reads the diff. Only what the change should alter may move."
For the manifest the diff says which programs moved and nothing of how. 189
commits have written the manifest, 38 of the 461 since 2026-09-30. The test
does what its header says, which is to compare hosts. On the Mac it also
stands as the one check that the LLVM text of 222 programs stayed as it was,
and there it accepts whatever the tree writes.
`tests/link-identity/return42.macos-arm64.sha256` is the same form with one
digest. Keeping the texts of the last written manifest beside the new ones,
and printing the difference of each file that moved, would let the reading
happen.

### No runner shows a difference, and each compares with its expected file itself

`tests/run_dump.cmake:32`, structure, whether a golden can be re-pinned
without reading what changed and one concern in one place, major, new.

A comparison with an expected file is written out in `run_dump.cmake:32`,
`run_expect.cmake:52`, `program_output.cmake:130`, `run_modules.cmake:81`,
`run_antl.cmake:28` and `run_unwind.cmake:34`, and in `run_anti_doc`,
`run_anti_fmt`, `run_anti_bind`, `run_clib` and `run_dev`. None prints where
the two differ. `run_dump.cmake:33` prints `"output differs from
${EXPECTED}\ngot:\n${out}"`, the whole new text. `program_output.cmake:131`
prints both sides as hexadecimal digits. `run_modules.cmake:82` prints what
the program printed and not what was expected. `run_antl.cmake:29` prints
the whole library file as hex. Apart from the manifest above, no runner
writes an expected file, and a search of `tools/` found no script that does.
An expected dump cannot be written by redirecting antic either:
`run_dump.cmake:28` to 30 replace the version and drop the lines of `type
anti.rt.` first. So each change is answered with a loop written for the
occasion that copies the new text over the old, and the one reading step is
the diff of the commit, which nothing asks for. The size of that diff:
`024f6e61` rewrote 73 expected files of `tests/dump/` in one commit,
`68e38837` 46 and `64344540` 37, and the largest of them,
`tbaa.dev.macos-arm64.ll`, has 846 lines. One function of
`program_output.cmake` that compares two texts and prints the first line
that differs with its neighbours would hold the comparison once.

### The malformed input of `Object.deserialize` runs where no sanitizer looks

`tests/std/deserialize_input.anti:1`, safety, rule 15, major, new.

Rule 14 names `Object.deserialize` among the readers of untrusted input,
and rule 15 asks for a test with malformed input. The test is
`tests/std/deserialize_input.anti`, an Anti program. It links the runtime
of the archive, which carries no sanitizer in any of the three trees. The
compile line of the runtime, `CMakeLists.txt:375`, is `--target=${triple}
-std=c11 -O2` with flags of its own and without `CMAKE_C_FLAGS`, where a
preset puts `-fsanitize`. `tests/CMakeLists.txt:1712` says "Anti programs
link these objects without a sanitizer runtime, like the runtime library
itself", and line 392 says the same of `rt.configure`.
`CMakePresets.json:23` describes the preset as "Debug build of antic and
its runtime under AddressSanitizer", which holds for antic alone. The
reader is `src/rt/registry.c` with
`src/rt/object.c`, and neither is compiled into a C test program. So a
read past the text that stays inside mapped memory passes the test in
`host`, `asan` and `ubsan` alike. `tests/unit/test_json.c:1` shows the form
that sees it: "The input of each case is a heap copy without a NUL, so
AddressSanitizer reports a read past the end." The letter of rule 15 is
kept. The grade is major because the test cannot observe what rule 14
asks.

The same holds for every file of the runtime outside the C test programs.
`antic_rt_sources` of `tests/CMakeLists.txt:13` and the two core libraries
compile 19 of the 42 files of `src/rt/`: `assert`, `atomic`, `conf`,
`cpu`, `digest`, `f16`, `fs`, `hash`, `json`, `loaded`, `platform_posix`,
`platform_windows`, `plugin`, `regex`, `signal`, `symbols`, `text`, `toml`
and `utf`. The other 23, 5898 of 15542 lines, run inside Anti programs
alone: `object.c` with 1150 lines, `patterns.c` 960, `registry.c` 898,
`trace.c` 541, `threads.c` 421, `lock.c` 352, `platform_stdio.c` 334,
`sync.c` 251, `reflect.c` 248, `hooks.c` 180, `start.c` 146, and
`backtrace`, `call`, `cast`, `check`, `errno`, `init`, `io`, `license`,
`license_stub`, `mem`, `platform_entry` and `snapshot`. `test_conf_race.c`
and `test_plugin_race.c` give the reason they exist in the same words, and
show the way: a program of its own with stand-ins for the rest.

### 49 builds in 31 runners accept whatever antic and the linker print

`tests/run_modules.cmake:38`, structure, one way to run a tool, duplicated
logic that has diverged, major, new.

`run_program.cmake:56` gives the rule: "Warnings are errors: antic, llvm-mc
and the linker must print nothing but the warning that the program is
about." `antic_program` of `program_output.cmake:32` holds it for a
runner, and 8 runners call it. `run_modules.cmake` builds three times, at
lines 38, 48 and 65, and each time reads the status alone, `if(NOT status
EQUAL 0)`. It runs the 34 tests of `antic_modules_test`, half of them in
dev mode, where a warning of antic does not fail the build. A warning of
antic or of lld in a program of two modules or more therefore passes, and
the same warning fails a program of one module. A script found 49 such builds
in 31 runners. Read by hand: `run_modules.cmake:38`,
`run_classlib.cmake:15` and `22`, `run_build_id.cmake:25`,
`run_link_identity.cmake:26` and `run_probe.cmake:24`. The others, by the
line of the call: `run_address_tables` 29, `run_anti_doc` 56 and 64,
`run_anti_test` 105 and 117, `run_antic_stack` 31, `run_asserts` 33, 40
and 54, `run_checks` 25, 32 and 46, `run_codeview_empty` 16,
`run_conf_include_depth` 29, `run_cpu_check` 24, `run_dead_code` 50,
`run_debug` 58 and 69, `run_dev` 18, 30, 46 and 65, `run_direct_calls` 34,
`run_front_end` 14, `run_internal` 13 and 21, `run_licenses` 20,
`run_llvm_inline_lines` 37, `run_memory_checks` 32, 42, 58 and 116,
`run_memory_checks_list` 31, `run_mimalloc_environment` 35,
`run_musl_allocator` 41, `run_runtime_from_data_dir` 25,
`run_shared_inputs` 22, `run_symbols` 28 and 51, `run_trap_table` 39 and
`run_windows_addresses` 37. This is the pattern of M61 of the second
audit, which named five other runners and was fixed in `6c8da2de` by
`antic_program`. These places were not named then.

### 24 of the 56 unit files open without a header comment

`tests/unit/test_target.c:1`, documentation, a file without a header
comment that says its one purpose, major, new.

`test_target.c` opens with `#include "../binary_stdio.h"` and holds no
comment before its first function. The same holds for `pipeline.c`,
`test_arith.c`, `test_header.c`, `test_ir.c`, `test_layout.c`,
`test_lexer.c`, `test_link.c`, `test_lower.c`, `test_main.c`,
`test_modules.c`, `test_optimize.c`, `test_parser.c`, `test_sema.c`,
`test_sha256.c`, `test_symbols.c`, `test_text.c`, `test_types.c`,
`test_userdirs.c`, `test_utf.c` and `test_whole.c`. Their first comment
describes one helper or one case, at line 128 in `test_lexer.c` and at
line 178 in `test_types.c`. `test_nullable.c`, `test_sync.c` and
`test_variant.c` name their subject in a comment after the includes, at
line 12. The files without a header are the largest: `test_modules.c` has
3261 lines, `test_lower.c` 2438 and `test_sema.c` 1362. Three of them test
more than one part and no line says so. `test_text.c` holds the text
buffer and `test_diagnostic_cut`, the cut of a line of a message.
`test_types.c` holds the type table and `test_ptr_tables` of `ptrset.c`.
`test_link.c` holds the command lines of the linker, the names of an SDK
of `applesdk.c` and the markers of the notice. Each of the 32 files with a
header says what it tests, and the header is what the file holds.

### No document lists the directories of `tests/` or its runners

`CLAUDE.md:200`, documentation, the way from `CLAUDE.md` to the directory
and from there to the file, major, new.

The row of `CLAUDE.md` reads "Test programs, expected outputs, the runner".
`tests/` holds 26 directories and 156 runners. `tests/CMakeLists.txt:1`
names two kinds: "Unit tests link the compiler's code directly. Program
tests in tests/programs compile .anti files with antic and run the
result." `CLAUDE.md:222` sends a new test to "the `tests/` subdirectory of
its kind" and names no kind. `tests/run_repo_layout.cmake:5` says of its
lists "the rule in CLAUDE.md names the same entries", and `CLAUDE.md`
names the directories of `src/` and `docs/` and none of `tests/`. The one
list of the 26 names is line 23 of that script, without a purpose beside
any. `docs/decisions.md:2158` gives the prefixes of four kinds of test
name. A reader who looks for the tests of `anti check` finds
`tests/check/`, and nothing says that `tests/traps/` holds the dev-mode
checks or that `tests/dump/` holds the expected C headers. The closing
section of this report gives a sentence per directory and per runner, as a
start.

## Minor

### Directories still hold more than one kind, and the runners lie flat

`tests/CMakeLists.txt:4653`, structure, each concern in one place, minor,
still open.

The second audit reported this at `tests/CMakeLists.txt:320`, and
`closure.md` left it for a decision on new directories. What stands now:

- The expected `.ir` and `.opt` dumps stand in two directories, 4 and 4
  in `tests/opt/` and 9 and 12 in `tests/dump/`.
- `tests/dump/` holds eleven kinds of file: sources, dumps of six stages,
  14 expected C headers, the unwind data of two Windows targets, a licence
  notice and `targets.txt`.
- The 156 runners lie in `tests/` beside the directories, 108 at the
  second audit.
- `tests/unit/` holds 41 files of `antic_unit_tests` with their driver and
  four shared files, five programs of the runtime, two programs a runner
  drives, two libraries that tests load and `symbols_probe.c`, a tool that
  `run_symbols.cmake` runs.

New in the same place: `plugin_table.c` is the library of
`test_plugin_race.c` and of `test_platform.c`, and the library of
`test_plugin_table.c` is `plugin_bad.c`.

### Helpers of the runners are copied, and the copies differ

`tests/run_anti_build.cmake:37`, structure, one way to run a tool, minor,
new.

- `run_anti` stands in seven runners with eight bodies:
  `run_anti_build.cmake:37`, `run_anti_build_deps.cmake:35`,
  `run_anti_build_inputs.cmake:46`, `run_anti_build_resolve.cmake:32` and
  `48`, `run_anti_internal.cmake:28`, `run_anti_memory_checks.cmake:45`
  and `run_license_forms.cmake:45`. They differ in whether `--runtime` and
  `--llvm-mc` are added, in whether a failure stops the run, and in
  whether the two outputs come back joined. Six set `XDG_CACHE_HOME` and
  `LOCALAPPDATA` to a directory of the run, "so the run names its own and
  leaves the one of this machine alone", and the copy of
  `run_anti_memory_checks.cmake` does not. 16 runners name
  `XDG_CACHE_HOME` on 25 lines.
- `run`, a command that must succeed, stands in 13 runners with seven
  bodies. `run_unwind.cmake:15`, `run_anti_symbols.cmake:30` and
  `run_llvm_coff_plugin.cmake:21` fail on any standard error, and
  `run_anti_bind.cmake:48` and `run_clib.cmake:114` accept it.
- `build` stands in four runners of `anti build` with four bodies,
  `get_sysroot` in three with three, `install_run` in three with three and
  `read_pin` in three with three: `run_native_pin.cmake:36`,
  `run_raylib_miniaudio_pin.cmake:10` and `run_sources.cmake:55` each read
  a pin file their own way.
- `file_url` in three runners and `expect` with `refuse` in
  `run_anti_check.cmake:34` and `run_anti_test.cmake:39` are the same
  bytes.

`program_output.cmake` is the one shared file, and 41 of the 156 runners
include it.

### Two unit files run the stages themselves beside `pipeline.c`

`tests/unit/test_whole.c:49`, structure, duplicated logic, minor, new.

`pipeline.h:2` says "One copy, so a change of a stage reaches every test
that runs it." `compile` of `test_whole.c:49` and `check_module` of
`test_modules.c:51` each call `lexer_lex`, `parser_parse` and `sema_check`
with the libraries of a program, over `struct program` and `struct
session`, which share seven of their eight fields. `test_modules.c` spells
the seven arguments of `lower_module` at ten sites, and `test_sync.c:26`
and `81` call the lexer and the parser for two cases.
`pipeline.c` takes no libraries, which is why the two files stand beside
it. A change of the signature of `sema_check` or `lower_module` is then
made in three files of `tests/unit/`. The second audit's finding on
copied helpers, fixed in `fbc477e4`, named other functions.

### An index of the two copies is not checked

`tests/unit/test_modules.c:54`, safety, rule 6, minor, new.

`struct token_list *tokens = &s->tokens[s->token_lists++];` writes into
`tokens[8]` of line 25 with no test of the count, and
`test_whole.c:52` does the same into `tokens[4]` of line 22. A test that
compiles one module more writes past the array. The sanitizer trees
report it, and the host tree does not.

### The registrations still spell the arguments of antic at 103 sites

`tests/CMakeLists.txt:479`, structure, small duplication, minor, still
open.

The second audit counted `"-DANTIC=$<TARGET_FILE:antic>"` 159 times and
`"-DLLVM_MC=${ANTIC_LLVM_MC}"` 104 times. `ca3bf832` and `eaaa26f9` added
`antic_program_test`, `antic_modules_test` and `antic_dump_test`, and
`closure.md` marks the finding fixed. The first now stands 103 times and
the second 71 times, and the three lines of antic, llvm-mc and the runtime
stand in that order at 35 registrations. The file names 152 runners after
`-P`, three of them inside a function, and writes the block of
`run_asm.cmake` out at seven sites and that of `run_warnings.cmake` at
six.

### The tests still pass and document an assembler that nothing runs

`tests/run_program.cmake:30`, structure and documentation, what is left of
the native back end, minor, new.

`docs/decisions.md:19` says "antic accepts `--llvm-mc` and runs nothing
with it", and keeps the option for a later decision. The tests have not
followed:

- 57 runners take `LLVM_MC` and pass `--llvm-mc`, and
  `tests/CMakeLists.txt` names it on 89 lines. Two runners run the tool,
  `run_asm.cmake:48` and `run_unwind.cmake:30`.
- `run_program.cmake:30` ends every program test with "llvm-mc not found"
  when the file is missing, though no step of the test runs it.
- Comments say it assembles. `run_program.cmake:56` reads "antic, llvm-mc
  and the linker must print nothing". `tests/CMakeLists.txt:233` reads
  "builds source with antic, llvm-mc and the runtime archive". The headers
  of the runners that take it say the same, as "LLVM_MC  the assembler" of
  `run_anti_build.cmake:6`.
- `run_dev.cmake:4` says the second build "gives the same assembly". Line
  60 compares the `.ll` text of `--keep-llvm`.

`docs/audit/back-end.md` records the option itself in `driver.h:23`.

### Expected texts of the unit tests hold more than their subject

`tests/unit/test_lower.c:1277`, structure, whether tests check behaviour
or repeat the implementation, minor, new.

`lowers` of `test_lower.c:33` compares the whole IR of a source with a
text, at 31 sites, and 1248 of the 2438 lines of the file are lines of a
test source or of an expected text.
The case at line 1277 is about a conversion of `c_long`. Its expected text
also holds the declaration of `anti_rt_check_failed` and three lines that
open as `global main.0 size 22 align 1 bytes 6d 61 69 6e 3a 33 3a` does.
They are the bytes of the message of a check, with the line number of the
source in them. 59 lines of `test_lower.c`, `test_optimize.c`
and `test_modules.c` hold such bytes. A change to the words of a check
message, or one more line in a test source, rewrites texts whose subject
is something else. `c8845ef6` took the types of `anti.rt` out of every
expected text for the same reason, through `ir_print_own`. The globals of
the messages are the next of that kind. The rest of the unit tests state
single facts: `test_llvm_emit.c` holds 287 uses of `CHECK` against 308
lines of such text.

### 25 files of antic and anti are reached by no unit test

`src/antic/driver.c:1`, structure, which parts have no unit test, from the
call graph, minor, new.

Of the 96 files of `src/antic/`, the unit tests call into 39, reach 46
more through those, and reach 11 not at all: `driver.c`,
`driver_index.c`, `driver_library.c`, `driver_link.c`, `driver_search.c`,
`llvm_run.c`, `memcheck.c`, `notice.c`, `docpage.c`, `sha256.c` and
`main.c`, 5501 lines. `test_link.c:283` reads two constants of `notice.h`
and `test_llvm_target.c:105` an array of `llvm_run.h`, and neither calls a
function. Of the 28 files of `src/anti/`, the unit tests call into 12 and
reach `bindmodel.c` and `cursor.c` through them. The other 14 are
`bind.c`, `bindapi.c`, `bindclang.c`, `bindwrite.c`, `build.c`, `check.c`,
`doc.c`, `licensing.c`, `memreport.c`, `project.c`, `sdk.c`, `test.c`,
`units.c` and `main.c`. All 25 run in the tests of the commands, through
`run_anti_*.cmake`, `run_license_forms.cmake` and the runners that link a
program, and those run under both sanitizers, since antic and anti are
built with them. The 46 files reached through others are the split files
of the parser, the checker, lowering, the library file and the
whole-program pass, which `parser_parse`, `sema_check`, `lower_module`,
`antl_read` and `whole_program` enter. The files of the runtime are in the
major finding above.

### Test names of three groups do not start with their kind

`tests/CMakeLists.txt:309`, structure, a name that makes the tests harder
to find, minor, new.

`docs/decisions.md:2158` decides: "A test name starts with the kind of check
it makes", and "`unit_<file>` runs the unit tests of
`tests/unit/test_<file>.c`". 32 of the 34 tests of `antic_modules_test` are
named `<subject>_modules_<mode>`, as `closures_modules_dev`, with the kind
in the middle. Seven files named `test_*.c` in `tests/unit/` run under other
names: `rt_signal`, `rt_conf_race`, `rt_plugin_race`, `rt_platform`,
`rt_plugin_table`, `files_oom` and `alloc_oom`. The second audit's finding
on names, fixed in `eaaa26f9`, was about `listing_` and the targets.

### Comments cite a finding by a number that two audits both use

`tests/run_antl_verify.cmake:1`, documentation, a stale name in a comment,
minor, new.

"M7: the IR of a library file that antic --dev compiles passes ir_verify"
names M7 of the first audit. "M03." in `run_alloc_oom.cmake:1`,
`run_one_allocator.cmake:1` and `test_alloc_oom.c:1` names M03 of the
second, and the first audit has an M3 of its own, on the reader of a
bitfield width. `tests/CMakeLists.txt:155` says "M02 of the audit". 61
comment lines of the runners and the unit files cite such a number, 17 of
them in `test_modules.c`, and two say which audit.
`run_anti_build_inputs.cmake:12` names the document, "S45, S40, M13 of
docs/audit/summary.md", and that file is `docs/audit/2026-09-30/summary.md`
since the third audit took `docs/audit/`. `run_rt_wide_text.cmake:5` shows
the form that still reads right: "M32 of the second audit".

### A header documents a value the runner does not take

`tests/run_anti_build.cmake:4`, documentation, a comment that differs from
the code, minor, new.

The header lists "OBJDUMP  llvm-objdump of the pinned release". The
registration at `tests/CMakeLists.txt:1887` passes no `OBJDUMP`, and the
script reads none. It reads `READOBJ`, which the header lists as well.

## The sentence of each file

The sentence of each file as read from the code. A file whose own header
says the same carries no mark. "Header: none" means the file opens with
code. "Header: one item" means its first comment describes one function
or one case. "Header: after the includes" means the subject is named
there. A runner without a mark was compared with its header and its
registration. The opening of this report names the runners whose code was
read in full.

### Shared files

- `tests/CMakeLists.txt`: builds the C test programs and registers every
  test but those of `src/native/` and the root. Header: names two of the
  kinds of test, the unit tests and the program tests.
- `tests/program_output.cmake`: the functions a runner includes to build a
  program with antic, run it and compare its output as bytes.
- `tests/binary_stdio.h`: puts the standard output and error of a C test
  program into binary mode on Windows before `main`.

### The directories

No file gives these sentences. They are read from the registrations.

- `abi/`: C sources, Anti programs and expected outputs that compare the
  layout and the calls of C with those of Anti. It also holds the link
  probes of the native libraries.
- `anti-build/`: ten projects that `anti build`, `anti run` and `anti
  license` are run over.
- `anti-symbols/`: the project `tracer` of `anti symbols`.
- `anti-test/`: three modules that `anti test` runs.
- `bench/`: five benchmark programs with their C twins, outside the suite,
  and `ablate/`, the measuring script of
  `docs/work-order-llvm-optimization.md` with its wrappers.
- `bind/`: C headers, `raylib_api.json` and the expected binding modules
  of `anti bind`.
- `check/`: six projects of `anti check`.
- `clib/`: libraries for C, the C and C++ programs that link them and
  their expected outputs.
- `conf/`: runtime configuration files, five programs and the patterns
  their output must match.
- `doc/`: the fixture of `anti doc`, its expected pages and
  `antl_rename.c`.
- `dump/`: sources and the expected dumps of antic's stages, the expected
  C headers, the unwind data and the list of targets.
- `emit-identity/`: the manifest of digests of `emit_identity`.
- `errors/`: programs antic refuses, each with its expected messages.
- `fmt/`: one source and its canonical form for `anti fmt`.
- `framework/`: one program that links CoreFoundation.
- `inject/`: one program, two projects and the patterns of the injection
  tests.
- `link-identity/`: the digest of one linked program and the two programs
  of the address and dead-code tests.
- `modules/`: programs of more than one module, two library files as hex
  and `poke_byte.c`.
- `opt/`: four sources with their expected IR before and after the
  optimiser.
- `plugin/`: hosts and libraries of the plugin tests, with the versions of
  one interface.
- `programs/`: programs with their expected exit status and output.
- `raw/`: a C program and an Anti program that write a CR, an LF and a
  NUL.
- `std/`: programs that test the standard library, with expected output.
- `trace/`: programs of hooks, tracing and stack traces with the patterns
  their output must match.
- `traps/`: programs that must stop, the dev-mode checks and the programs
  of `--memory-checks`.
- `unit/`: the C tests that link the code of antic, anti and the runtime.

### `tests/unit/`

- `check.h`: the two assertion macros, which count a failure and go on.
- `path_rules.h`: the cases of the path rules that both platform layers
  are run over.
- `pipeline.h`: the stages the unit tests run a source through, and the
  IR print without the types of the runtime.
- `pipeline.c`: defines what `pipeline.h` declares. Header: none.
- `test_main.c`: the table of the 51 entry functions by file and the
  `main` that runs the files one argument names. Header: none.
- `plugin_table.c`: a shared library with an empty plugin table, which
  `test_plugin_race.c` and `test_platform.c` load.
- `plugin_bad.c`: a shared library with a writable plugin table, which
  `test_plugin_table.c` damages and loads.
- `symbols_probe.c`: a program that looks a function up in a binary of
  another target with the readers of `src/rt/symbols.c`.
- `test_abi.c`: `abi_classify`, one case per rule of the calling
  conventions.
- `test_alloc_oom.c`: a program that asks one allocation helper of antic
  for more memory than a host gives, for `run_alloc_oom.cmake`.
- `test_arith.c`: the upper half of a product and the saturating
  operations of `arith.c` at every width. Header: one item.
- `test_bind.c`: the parser of C type spellings and the evaluator of
  constant expressions of `anti bind`.
- `test_coff.c`: the join of COFF objects and the exports of an archive.
- `test_conf_race.c`: `rt.configure` replacing a value while another
  thread reads it.
- `test_cpu.c`: the level table of antic, the names of `--cpu` and the
  start-up check of the runtime.
- `test_deps.c`: the manifest, the version constraints and the repository
  URLs of `anti build`.
- `test_f16.c`: the conversions between `f16` and `f32`.
- `test_files.c`: the file helpers of anti and the walk over a directory
  tree.
- `test_files_oom.c`: a program that asks one allocation helper of anti
  for more memory than a host gives, for `run_files_oom.cmake`.
- `test_float_read.c`: the reader and the writer of the text of a float
  in `src/rt/text.c`.
- `test_fmt.c`: `anti fmt` over two sources the parser would refuse.
- `test_fs.c`: `anti_rt_fs_read`.
- `test_header.c`: the C header antic writes for a library, for owning
  structs and simd lanes. Header: one item.
- `test_interface.c`: the walks of `antic.h` that the commands of anti
  call.
- `test_ir.c`: the IR built by hand, its printed form and its verifier.
  Header: none.
- `test_json.c`: the JSON scanner on malformed input.
- `test_layout.c`: the sizes, alignments, bitfields and refusals of
  `layout.c`. Header: none.
- `test_lexer.c`: the tokens, the literals and the errors of the lexer.
  Header: none.
- `test_link.c`: the command lines of the linker per target and mode, the
  names of an SDK and the markers of the notice. Header: none.
- `test_llvm_emit.c`: `llvm_emit_module`, by the sections of the work
  order of the LLVM back end.
- `test_llvm_target.c`: the triple, the relocation model and the function
  attributes per target and level.
- `test_lower.c`: the IR that lowering writes for a source, and the table
  of the runtime functions against the prototypes of `src/rt/`. Header:
  none.
- `test_modules.c`: the library file written, read back and damaged, the
  imports across modules and the module paths. Header: none.
- `test_nullable.c`: the rules of nullable pointers in the checker.
  Header: after the includes.
- `test_optimize.c`: the passes of `optimize.c` over the IR of a source.
  Header: none.
- `test_parser.c`: the tree the parser builds, its errors and its depth
  limit. Header: none.
- `test_platform.c`: the platform layer of the runtime on the host.
- `test_plugin_race.c`: the table of open libraries under threads that
  load and unload at once.
- `test_plugin_table.c`: the loader of the runtime against a damaged
  plugin table and a damaged index.
- `test_rt_bounds.c`: the growth of a block, the size of a text builder
  and atomic subtraction at the minimum of each width. That is three parts
  of the runtime, and the header names all three.
- `test_sema.c`: what the checker accepts, refuses and warns of. Header:
  one item.
- `test_sha256.c`: the SHA-256 of `src/rt/digest.c` over inputs given in
  pieces. Header: one item.
- `test_signal.c`: the signal table of the runtime under threads that
  register at once.
- `test_symbols.c`: the readers of `src/rt/symbols.c`: the names of COFF,
  the sections and line tables of ELF and the symbols of Mach-O. Header:
  one item.
- `test_syms.c`: `anti symbols` over archives and programs that name
  nothing.
- `test_sync.c`: the rules of locking and channels in the checker. Header:
  after the includes.
- `test_target.c`: the mangled names, the C names and the block labels of
  `target.c`, and the target of the host. Header: none.
- `test_text.c`: the text buffer of antic and the cut of a long line of a
  message, two parts. Header: none.
- `test_toml.c`: the TOML subset of `src/rt/toml.c`.
- `test_tool_platform.c`: the platform layer of antic and anti on the
  host.
- `test_types.c`: the type table and the pointer sets of `ptrset.c`, two
  parts. Header: none.
- `test_userdirs.c`: the three user directories under an environment the
  test gives. Header: one item.
- `test_utf.c`: the repair, the conversion and the split of UTF-8 in
  `src/rt/utf.c`. Header: one item.
- `test_variant.c`: the rules of sum types in the checker. Header: after
  the includes.
- `test_whole.c`: the pass over the whole program. Header: one item.
- `test_zip.c`: the zip reader of `anti symbols` on damaged archives.

### Runners that many tests share

- `run_program.cmake`: compiles one program, runs it and compares its exit
  status and output with the expected file. Header: lists `LLVM_MC`, which
  no step runs.
- `run_llvm_program.cmake`: the same for a target the host runs, through
  opt and llc of the runtime archive.
- `run_modules.cmake`: builds library modules and a program in release or
  dev mode, runs it and compares its output.
- `run_dump.cmake`: compares what antic prints for a stage with an
  expected file.
- `run_expect.cmake`: runs a command and checks its exit status and its
  output against a file or a pattern.
- `run_trap.cmake`: builds a program that must stop and compares its
  standard error.
- `run_check.cmake`: one dev-mode check in a build that has it and one
  that does not.
- `run_trace.cmake`: runs a program and matches its output against a
  pattern file.
- `run_asm.cmake`: writes the assembly of a program with `-S` and
  assembles it with llvm-mc.
- `run_warnings.cmake`: checks every warning antic must report for a
  program it accepts.
- `run_ir_line.cmake`: requires one line of `--dump-ir`.
- `run_antl.cmake`: compares the bytes of a library file with a hex
  listing.
- `run_cross_link.cmake`: links a program for another target and reads
  the object format.
- `run_native_link.cmake`: links a program against a native library of
  the runtime tree.
- `run_native_pin.cmake`: holds the version, digest and URL of a
  third-party library in its pin alone.
- `run_cpu_level.cmake`: checks the instructions of one processor level in
  the assembly.
- `run_simd_width.cmake`: counts the lines of the assembly that hold an
  instruction.

### Runners of antic and the programs it builds

- `run_address_tables.cmake`: every object of a program and its runtime
  carries the address-significance table.
- `run_antic_stack.cmake`: compiles the deepest expression with the stack
  of the main thread cut to 256 KB.
- `run_antl_verify.cmake`: a damaged library file fails the IR verifier.
- `run_asserts.cmake`: the five behaviours of `assert`.
- `run_atomic_load.cmake`: `anti_rt_atomic_load` of an ARM64 runtime is a
  sequentially consistent load.
- `run_build_id.cmake`: two links of one program carry one build id and
  another program another.
- `run_checks.cmake`: the options that decide the dev-mode checks, and a
  library whose checks follow the build that links it.
- `run_classlib.cmake`: a library with a class and an enum, and a program
  that uses it.
- `run_clib.cmake`: libraries for C built with `--lib`, and C programs
  compiled and run against them.
- `run_codeview_empty.cmake`: a module without a function writes no empty
  CodeView subsection.
- `run_conf_include_depth.cmake`: the runtime and `anti symbols` stop at
  one depth and one count of included configuration files.
- `run_cpu_archive.cmake`: the runtime archive holds a runtime for every
  level of every target.
- `run_cpu_check.cmake`: the start-up check refuses a machine below the
  level of the program.
- `run_cpu_pin.cmake`: the level table of antic equals `tools/cpu-levels`.
- `run_cpu_runtime.cmake`: the atomics of a runtime library follow the
  level it was built for.
- `run_dead_code.cmake`: the link drops a runtime function the program
  never calls.
- `run_debug.cmake`: breakpoints by file and line and a backtrace of Anti
  names under `-g`.
- `run_dev.cmake`: a program of three modules in dev mode, from sources
  and from library files, with the same LLVM text. Header: says "the same
  assembly".
- `run_direct_calls.cmake`: a release build calls the function of an
  object of a known class directly.
- `run_distinct_addresses.cmake`: two functions and two descriptors that
  could be folded keep their own addresses.
- `run_emit_identity.cmake`: the digests of the LLVM text of every program
  and target equal those the Mac wrote.
- `run_float_digits.cmake`: the objects that write and read a float name
  no function of the C library that prints or reads a number.
- `run_framework.cmake`: a link with a framework, with and without an SDK.
- `run_front_end.cmake`: `antic --front-end` writes nothing and reports
  what the checker found.
- `run_function_alignment.cmake`: under `--lto none` every ARM64 function
  starts on a 16-byte boundary.
- `run_glibc_link.cmake`: the C probe of a native library links against
  the glibc sysroot.
- `run_hash_seed.cmake`: two runs without a fixed seed print two seeds.
- `run_internal.cmake`: a module of the same package sees an internal
  item, and another package does not.
- `run_licenses.cmake`: a linked program carries the licence notice
  between its markers.
- `run_link_identity.cmake`: one program links to the same bytes for
  macos-arm64 on every host.
- `run_linux_libc.cmake`: a Linux program names no glibc symbol above
  2.35.
- `run_linux_modes.cmake`: `anti build` links statically against musl or
  dynamically against glibc by what the modules name.
- `run_llvm_atomics.cmake`: each atomic operation is an LLVM atomic
  instruction on all six targets.
- `run_llvm_coff_plugin.cmake`: what the link of a Windows plugin host and
  its library reads and exports.
- `run_llvm_datalayout_pin.cmake`: the data layouts, relocation models and
  CPU attributes equal those of the pinned clang.
- `run_llvm_inline_lines.cmake`: an inlined callee keeps its line in the
  line table of the caller.
- `run_llvm_run.cmake`: what `--keep-llvm`, a dev build and `-S` leave
  beside the output.
- `run_llvm_verify.cmake`: the LLVM text of every program passes the
  verifier of opt.
- `run_lock_order.cmake`: the lock-order check of a dev build.
- `run_lto_default.cmake`: a release build links through full LTO, and
  `--lto none` links objects.
- `run_memory_checks.cmake`: `--memory-checks` reports each kind of memory
  error.
- `run_memory_checks_list.cmake`: `--memory-checks` finds a pointer into a
  `List` kept across a push that grows it.
- `run_mimalloc_environment.cmake`: a program of musl reads no
  `MIMALLOC_*` variable.
- `run_musl_allocator.cmake`: a program of musl takes its allocator from
  mimalloc in every link mode.
- `run_mutex_size.cmake`: the size of a Mutex on each target.
- `run_own_tools.cmake`: antic takes its tools, libraries and sysroots
  from the runtime archive alone.
- `run_overview_examples.cmake`: every `anti` block of the syntax overview
  passes the front end.
- `run_pdb_guid.cmake`: the PDB of a Windows program carries the GUID of
  its CodeView record.
- `run_pdb_names.cmake`: the PDB names every function of the program.
- `run_plugin.cmake`: builds a plugin and a host, runs the host and
  damages the plugin index.
- `run_plugin_versions.cmake`: six libraries of one interface at other
  versions load or are refused.
- `run_probe.cmake`: the ABI probe in C and in Anti print the same bytes.
- `run_profile.cmake`: a program built with a profile of its own run
  passes its test.
- `run_raw_output.cmake`: a program writes a CR, an LF and a NUL as given,
  and the runners read them as bytes.
- `run_rt_names.cmake`: every symbol a runtime library defines starts with
  `anti_rt_`.
- `run_runtime_frames.cmake`: `main` of the runtime keeps its frame
  record. Header: stands after `cmake_minimum_required`.
- `run_runtime_from_data_dir.cmake`: antic without `--runtime` takes the
  archive of the data directory of the user.
- `run_runtime_hosts.cmake`: the linux-arm64 runtime holds no outline
  atomics, whichever host built it.
- `run_shared_inputs.cmake`: a shared library takes the frameworks, the
  Linux libraries and the memory checks of its module.
- `run_source_cap.cmake`: antic reads a source of the largest size and
  refuses one a byte longer.
- `run_source_nul.cmake`: a NUL outside a literal is refused at five
  places of a source.
- `run_symbols.cmake`: the probe finds the functions of a `-g` program of
  another target.
- `run_symbols_layout.cmake`: the program in `dist/` and the link in its
  symbols archive hold the same sections.
- `run_trap_table.cmake`: an object whose table is zero traps with the
  name of its class.
- `run_unicode_paths.cmake`: antic and anti take paths and variables
  outside ASCII as UTF-8.
- `run_unwind.cmake`: the decoded Windows unwind data equals an expected
  file.
- `run_windows_addresses.cmake`: the object of a windows-x86_64 program
  holds no 32-bit absolute address.

### Runners of the `anti` commands

- `run_anti_bind.cmake`: the cases of `anti bind`, with its refusals and
  malformed inputs.
- `run_anti_build.cmake`: `anti build`, `anti run` and `anti new` over a
  project of two modules. Header: lists `OBJDUMP`, which it does not take.
- `run_anti_build_deps.cmake`: a dependency from a path and from a
  `file://` repository, and a library for C.
- `run_anti_build_framework.cmake`: a project whose module names a
  framework.
- `run_anti_build_inputs.cmake`: a malformed `index.toml` and a malformed
  `anti.lock`. Header: names `docs/audit/summary.md`, which moved.
- `run_anti_build_rerun.cmake`: run the program of `dist/` and build
  again, many times in a row.
- `run_anti_build_resolve.cmake`: the resolution over a repository whose
  picks change while the walk runs.
- `run_anti_build_running.cmake`: a build that replaces a program while it
  runs.
- `run_anti_check.cmake`: `anti check` over six projects.
- `run_anti_doc.cmake`: `anti doc` over a fixture, and a hostile library
  file.
- `run_anti_fmt.cmake`: `anti fmt` over one source.
- `run_anti_fmt_write.cmake`: a write of `anti fmt` that fails partway
  leaves the source whole.
- `run_anti_internal.cmake`: every command of anti that compiles, over a
  project with an internal item.
- `run_anti_memory_checks.cmake`: the reports of `anti run` and `anti
  test` under `--memory-checks`.
- `run_anti_memory_dll.cmake`: `anti build --memory-checks` puts the DLL
  of AddressSanitizer beside a Windows program. Header: stands after
  `cmake_minimum_required`.
- `run_anti_sdk.cmake`: `anti sdk export` and `import` over a stand-in
  SDK.
- `run_anti_symbols.cmake`: `anti symbols inventory`, `check` and
  `resolve` over a deployment.
- `run_anti_symbols_input.cmake`: `anti symbols resolve` with numbers that
  do not fit 64 bits.
- `run_anti_test.cmake`: `anti test` over three modules.
- `run_std_tests.cmake`: `anti test` over one module of the standard
  library in both modes.
- `run_fmt_canonical.cmake`: every Anti source of the checkout stands in
  the canonical form.
- `run_license_forms.cmake`: the four forms of `anti license` beside
  `--from`.
- `run_license_notice.cmake`: the notice of a program names musl and
  mimalloc where it links them.

### Runners that read the sources

- `run_anti_interface.cmake`: anti includes of antic the interface
  headers and the shared helpers alone.
- `run_errors_registered.cmake`: every program of `tests/errors/` is read
  by a test.
- `run_expect_self.cmake`: `run_expect.cmake` refuses what a pattern of
  ctest lets pass.
- `run_find_order.cmake`: no use of a `find_program` variable stands above
  its call.
- `run_header_names.cmake`: no header of ours shares its name with a
  system header one of our sources includes.
- `run_header_sections.cmake`: every function declared under the comment
  of a file in a shared header is defined in that file.
- `run_line_endings.cmake`: `.gitattributes` keeps a carriage return out
  of every text file.
- `run_may_fail_only.cmake`: no function of the standard library or the
  tests writes the out-pointer convention by hand.
- `run_names_once.cmake`: a name the tools and the runtime both spell
  stands in one header.
- `run_native_pin_self.cmake`: `run_native_pin.cmake` refuses and passes
  what it must, over a made-up library.
- `run_no_sdk_fetcher.cmake`: the name of the removed tool stands in no
  file outside the decisions and the reports.
- `run_no_workflows.cmake`: no file stands under `.github/workflows/`.
- `run_one_allocator.cmake`: antic and the unit tests that link it
  allocate through `src/antic/alloc.c` alone.
- `run_platform_layer.cmake`: a `#if` on the host stands in the platform
  layer alone.
- `run_repo_layout.cmake`: every tracked path follows the layout rule.
  Header: says `CLAUDE.md` names the same entries, which holds for `src/`
  and `docs/` and not for `tests/`.
- `run_rt_extensions.cmake`: a compiler extension of the runtime stands in
  the platform layer or `atomic.c`.
- `run_rt_wide_text.cmake`: the runtime calls no ANSI entry point of
  Windows.
- `run_script_policies.cmake`: every script run with `cmake -P` names the
  minimum version. Header: stands after `cmake_minimum_required`.
- `run_std_collection_modules.cmake`: the build compiles every file under
  `src/std/anti/collection/` by itself.
- `run_warning_names.cmake`: the names of the warning table equal those of
  `docs/notes/warnings.md`.
- `run_warning_set.cmake`: every warning flag stands in one of two files.

### Runners of the build, the pins and the release

- `run_alloc_oom.cmake`: each allocation helper of antic ends with status
  70 when memory is refused.
- `run_files_oom.cmake`: the same for the helpers of anti.
- `run_bench.cmake`: builds and times the benchmark programs and prints a
  table, outside the suite. Header: names `--backend,native`, as the tool
  pass reports.
- `run_deps_dir.cmake`: a copy of the sources configures and builds
  against the downloads of this tree.
- `run_glibc_sysroot.cmake`: `tools/get-sysroot.cmake` lays out the glibc
  sysroot from stand-in packages.
- `run_host_sources.cmake`: every source of antic and anti compiles for
  another host under the warning set.
- `run_installer_alone.cmake`: the installers download the package alone
  and run no CMake.
- `run_installer_github.cmake`: the installers take the package from the
  release and the signature from the site.
- `run_installer_options.cmake`: the installers and uninstallers of both
  shells take the same options.
- `run_llvm_names.cmake`: `tools/get-llvm.cmake` leaves a name of lld
  alone that already holds lld.
- `run_macos_sdk.cmake`: a release links macOS against the pinned Apple
  SDK and refuses one that changed.
- `run_macos_sysroot.cmake`: `tools/get-sysroot.cmake` lays out the macOS
  sysroot from a stand-in archive.
- `run_manifest_signature.cmake`: the installer checks the signature of
  the manifest before it trusts a line.
- `run_native_licenses.cmake`: every native library has its licence text
  in `LICENSES/`, equal to that of the pinned release.
- `run_no_paths.cmake`: no file a user receives holds a path of the
  machine that built it.
- `run_package.cmake`: packs the package of the host and builds with it
  alone.
- `run_pinned_compiler.cmake`: the build compiles with the pinned clang,
  and no runner names a compiler of the machine.
- `run_pinned_tools.cmake`: the configure step refuses a tree that would
  run a tool of the host.
- `run_pre_commit.cmake`: the pre-commit hook refuses staged `.md` files
  that fail the docs-style checker.
- `run_publish_manifest.cmake`: `tools/publish.cmake` takes what the
  manifest names and refuses the rest.
- `run_raylib_miniaudio_pin.cmake`: the two pins name one miniaudio.
- `run_release_dry_run.cmake`: `./r --dry-run` on a copy of the tree, with
  stand-ins for the commands that build and reach the VMs.
- `run_release_key.cmake`: each installer holds the public key of
  `tools/keys/release.pem`, and no package holds one.
- `run_release_pins.cmake`: `tools/llvm-pin` and `tools/clang-pin` name
  one release.
- `run_release_site.cmake`: the permissions step 9 leaves on the site.
- `run_sources.cmake`: `licenses/sources.txt` holds one line per pinned
  component.
- `run_sysroot_links.cmake`: `tools/get-sysroot.cmake` turns absolute
  links of a sysroot into relative ones.
- `run_windows_sysroot.cmake`: `tools/get-sysroot.cmake` lays out the
  Windows sysroot from a stand-in release of mingw-w64.
