# Tool pass

The machine evidence of the third audit, gathered at `f3af7693` on macOS
arm64 with the pinned clang 23.1.1 and no build file changed. Every
compile line is the one the host build runs, read from `build/host`. The
static analyzer and the extra warnings ran over `src/antic/`, `src/anti/`
and `src/rt/` for macos-arm64, linux-x86_64 and windows-x86_64, 252 units
in all. The glue of PCRE2 is covered: `src/native/` compiles `src/rt/regex.c`
and `patterns.c` and holds no C file of its own. Scripts measured every
function and file, drew the include graph and the calls between the files of
the checker, lowering, the driver and the LLVM back end, found the blocks two
files share, and read the first comment of every file, the comment before
every function a header declares, the DESIGN comments and the file names the
documents hold. The raw results are in `docs/audit/data/`, and `commands.txt`
there names each file and command. Each item below was read in the code.

## Counts

| Source | Now | Then | New | Kept | Gone |
|---|---|---|---|---|---|
| Analyzer warnings, unique | 34 | 43 | 8 | 26 | 17 |
| Compiler warnings, unique | 48 | 74 | 8 | 40 | 34 |
| Banned calls | 0 | 0 | 0 | 0 | 0 |
| Functions past a threshold | 142 | 163 | 32 | 110 | 54 |
| Files past 3000 lines | 2 | 9 | 1 | 1 | 8 |
| Host `#if` outside the platform layer | 0 | 110 | 0 | 0 | 110 |
| Duplicated blocks, exact and near | 2 and 43 | 28 and 40 | | | |
| Includes against the direction | 2 | 3 | 2 | 0 | 3 |

"Then" is the second audit at `70e8860`, in `docs/audit/2026-09-30/data/`.
The `*-compared.txt` files match each item across the move of lines and
the splits since: `parser.c`, `driver.c`, `antl.c` and `whole.c` are each
split into files now, and the native back end is gone. Of the 54 functions
that left the threshold list, 21 are not found by their name, and 13 of
those stood in the files of the native back end. The two compiler warnings
added this time, `-Wmissing-variable-declarations` and `-Wunused-macros`,
gave 7 of the 8 new lines. No analyzer warning points at a defect.

| Severity | Standard | Findings | New | Returned | Still open |
|---|---|---|---|---|---|
| Severe | | 0 | | | |
| Major | Documentation | 4 | 4 | 0 | 0 |
| Minor | Structure, rule 26 | 2 | 1 | 0 | 1 |
| Minor | Safety, rule 1 | 1 | 0 | 0 | 1 |
| Minor | Documentation | 3 | 3 | 0 | 0 |
| Minor | Structure, placement | 1 | 1 | 0 | 0 |
| Minor | Tests | 1 | 1 | 0 | 0 |
| Minor | Safety, rule 18 | 1, covering 142 functions | 0 | 0 | 1 |
| Minor | Safety, rule 19 | 1, covering 2 files | 0 | 1 | 0 |

## Severe

None. The eight new analyzer warnings are false positives on reading, and
the banned-call grep finds definitions alone. Both stand under "False
positives" below.

## Major

### Rule 10 names a test that does not exist

`docs/c-guidelines.md:46`, documentation, a documented behaviour the code
does not have, major, new.

Rule 10 ends: "A test fails the suite when a banned function appears in
`src/rt/`, `src/antic/` or `src/anti/`." No file of `tests/` or `tools/`
holds `strcpy`, `strcat`, `strncpy` or `vsprintf` apart from
`tests/abi/c_allocator.anti`, which binds `strcpy` as an `extern fn` of its
own, and `tests/run_rt_names.cmake`, which lists the names the Windows
runtime defines. `run_float_digits.cmake:38` refuses `scanf` in one
generated object alone. `data/banned-calls.txt` shows the grep the test
would run: twelve lines, all in `src/rt/platform_stdio.c`, which defines
the printf and scanf families of a Windows target and calls `vsprintf`,
`vscanf`, `vfscanf` and `vsscanf` inside those definitions at lines 104,
163, 180 and 197. A test would have to leave that file out, as
`run_platform_layer.cmake` does, so the rule should name the file or the
test should exist.

### Twenty-five files of src/ have no header comment

`src/antic/alloc.c:1`, documentation, a file without a header comment
that says its one purpose, major, new.

`data/first-comments.txt` reads the first comment of all 299 `.c`, `.h` and
`.anti` files under `src/`. 274 open with one, before or after the include
guard and the includes. Three hold no comment before their first function:
`alloc.c`, `diagnostic.c` and `sha256.c` of `src/antic/`. Twenty-two hold
their first comment after a declaration, so it describes one function or
one type and not the file:

- `src/antic/`: `ir_fold.c:20`, `ir_print.c:76`, `layout.c:27`,
  `linker.c:27`, `llvm_debug.c:35`, `lower.c:26`, `lower_desc.c:16`,
  `main.c:76`, `memcheck.c:14`, `modpath.c:24`, `notice.c:10`,
  `pattern.c:77`, `types.c:32`, `warnings.c:15`.
- The headers `lexer.h:15`, `lower.h:13`, `optimize.h:8`, `sema.h:40` and
  `warnings.h:11`, each a public header of a part.
- `src/anti/files.c:24`, `src/rt/text.c:29` and `src/rt/utf.c:27`.

Of these, `lower.c`, `layout.c`, `types.c`, `linker.c` and `sema.h` are the
hubs the call graph shows every file of their part calling. The matching
header of four, `alloc.h`, `layout.h`, `linker.h` and `types.h`, does
carry the sentence, so the fix is one comment at the top of each `.c`. The
closing section gives the sentence of each as read from the code.

### Public functions of headers without a comment of their own

`src/antic/optimize.h:6`, documentation, a public function of a header
without a comment that says what it takes, gives and refuses, major, new.

`data/header-function-comments.txt` lists the 1346 functions that 92
headers under `src/` declare. 755 have a comment directly above. 586
follow another declaration, so they share a comment with a group or have none, and 5 have
neither. `optimize_program` at `optimize.h:6` has no comment in the header
or at its definition, `optimize.c:1465`. `anti_rt_word_lock` and
`anti_rt_word_unlock` at `src/rt/platform.h:169` follow the struct of the
word, whose DESIGN comment describes the word and not the two calls.
`anti_rt_utf8_repair` at `src/rt/utf.h:14` has the file's comment alone,
and `anti_rt_atomic_load` at `src/rt/atomic.h:8` likewise. The fifth,
`anti_rt_conf_threads` at `src/rt/conf.h:76`, has its comment three lines
up with a `#define` between, and is no defect. Of the 586 grouped
declarations, 388 stand in the five shared headers of the splits,
`sema_checker.h` with 157, `lower_lowerer.h` with 150, `parser_parser.h`
with 54, `driver_parts.h` with 14 and `antl_io.h` with 13, where a comment
names the file that defines them and the test `header_sections` keeps that
true. The other 198 stand in public headers,
`ir.h` with 51, `src/rt/object.h` with 16, `types.h` with 15,
`src/rt/platform.h` with 12 and `src/rt/reflect.h` with 11 among them.

### Forty-one files of src/antic/ stand in no "Files of" section

`docs/decisions.md:2161`, documentation, a directory of src/ whose files no
document lists with their purposes, major, new.

The six sections "Files of the parser" to "Files of the pass over the whole
program" name 55 of the 96 `.c` files of `src/antic/`, and one they name,
`mach.c`, is gone. The 41 outside them are the shared helpers, `alloc.c`,
`arena.c`, `arith.c`, `cpu.c`, `platform.c`, `sha256.c`, `target.c`,
`text.c` and `userdirs.c`, the whole LLVM back end, `llvm_emit.c`,
`llvm_debug.c`, `llvm_run.c`, `llvm_target.c`, `layout.c`, `abi.c`,
`coff.c`, `linker.c` and `memcheck.c`, the IR, `ir.c`, `ir_fold.c`,
`ir_print.c`, `ir_verify.c`, `optimize.c`, `reach.c`, `ptrset.c` and
`rt_abi.c`, and `sema_chain.c`, `sema_copies.c` and `sema_value.c` of the
checker. `data/unnamed-files.txt` widens the view to every document: 102
tracked files under `src/` are named by none of CLAUDE.md,
`docs/decisions.md` and `docs/notes/`, among them 20 of `src/rt/` such as
`cast.c`, `errno.c`, `f16.c`, `reflect.c` and `license_stub.c`, and
`cursor.c`, `licensing.c`, `memreport.c`, `project.c` and `test.c` of
`src/anti/`. A reader cannot go from CLAUDE.md to these files through any
document.

## Minor

### One type walk and one operand comparison written twice

`src/antic/sema_value.c:310`, rule 26, minor, new.

- `class_in` at `sema_const.c:1104` and `holds_mutex` at
  `sema_value.c:310` walk a type the same way, a `ptr_set` of the types
  answered, down through arrays and fields, and differ in the test at the
  leaf and in `holds_mutex` following a `?T`. One walk with a predicate
  would hold both. `data/duplication.txt` finds the 20 lines they share.
- `same_operand` at `optimize.c:906` and at `whole.c:391` compare the
  scalar kinds `IR_NONE`, `IR_TEMP`, `IR_INT` and `IR_FLOAT` with the same
  twelve lines, and differ on purpose in the reference kinds, where the
  merge of `whole.c` follows representatives. The scalar part is one helper
  of `ir.h`.

### Eleven lines shared by the sequence collections

`src/std/anti/collection/list.anti:189`, duplicated code, minor, still
open.

The second audit reported the place functions of `list.anti`, `deque.anti`
and their kin, and `ebe86b3c` holds the matching loops once. `reserve`,
`shrink`, `iter` and `copy` are still the same 11 significant lines in
`list.anti:189` and `deque.anti:95`, as `data/duplication.txt` lists. The
other near blocks of the data file are tables and chains of `strcmp` or
`case`, which agree once names are replaced and are not duplicated logic.

### The format attribute is missing on six runtime functions

`src/rt/assert.c:10`, rule 1, minor, still open.

The second audit reported nine printf-like functions without the format
attribute. The closure fixed the three of antic and anti.
`-Wmissing-format-attribute` still names `write_line`,
`anti_rt_fail_exit`, `anti_rt_fail_abort` and `anti_rt_note` of
`assert.c`, `format_text` of `conf.c:121` and `fail` of `plugin.c:58`.
The runtime has no macro for the attribute, and `attributes.h` of antic is
no header of `src/rt/`, so the fix is one macro in the platform layer,
which holds the extensions of the runtime.

### Stale file names in docs/decisions.md and CLAUDE.md

`docs/decisions.md:1220`, documentation, a stale name in a document,
minor, new.

`data/named-files.txt` checks the 649 file names the three document sets
hold. 148 do not exist. Most are libraries of a sysroot, products of a
build or sources of a native library, which the tree does not carry. These
name a file of the repository that is gone or renamed:

- `docs/decisions.md:1220` says `src/antic/debug.c` records the byte range
  of everything `-g` appends. The file is `llvm_debug.c`, and the entry
  speaks in the present tense of a back end that is gone.
- `docs/decisions.md:500` names `tests/run_pcre2_pin.cmake`, which is
  `run_native_pin.cmake`. Line 539 names
  `src/native/get-media-sysroot.cmake`, which does not exist.
- `docs/decisions.md:2126` and `2292` cite `docs/audit/rt.md` and
  `docs/audit/summary.md`, which moved under `docs/audit/2026-09-23/` and
  `docs/audit/2026-09-30/`.
- The section "Files of the pass over the whole program" or its neighbours
  name `mach.c`, which is gone.
- `CLAUDE.md:351` names `docs/reports/2026-10-09-dist-mingw.md`. The report
  is `docs/reports/2026-10-10-dist-mingw.md`.

The provisional entries at lines 918, 941 to 945, 1012 and 1014 describe
the register allocator and the emitter of the native back end as current
decisions, `emit.c` among them. Whether they stand as history or go is
Eddie's decision, and the structure step should read them.

### The doc page of the front end builds an IR module

`src/antic/docpage.c:13`, structure, placement, minor, new.

`docpage.c` is the walk `antic.h` offers `anti doc`. It includes `ir.h`
and makes an `ir_module` at line 733 because `driver_interface` takes one.
That is the one include of a front-end file into the IR,
`data/include-graph.txt` shows, and it stands in a file that is a client
of the driver rather than part of the front end. The second audit's two
crossings, `notice.h` into `sema.h` and `userdirs.c` into `linker.h`, are
gone. The runtime includes nothing of antic or anti, and the IR nothing of
the back end.

### The comment of antic.h and the test anti_interface disagree

`src/antic/antic.h:11`, documentation, a comment that differs from what
the code does, minor, new.

The DESIGN comment of `antic.h` lists the shared helpers `anti` may
include: `text.h`, `arena.h`, `target.h`, `cpu.h`, `modpath.h`,
`platform.h`, `userdirs.h`, `sha256.h` and `applesdk.h`.
`tests/run_anti_interface.cmake:15` allows `attributes.h` as well, and
`src/anti/bindmodel.h:9` includes it. One of the two lists is wrong.

### A comment names an option that is gone

`tests/run_bench.cmake:14`, documentation, a comment that describes what is
gone, minor, new.

The comment gives, as the example of `OPTIONS`,
`--backend,native for an antic that still has that option`.
`data/back-end-mentions.txt` finds no other word of the native back end under `src/`, `tests/` and `tools/`:
`regalloc`, `x86_64.c`, `arm64.c`, `emit.c`, `select.c`, `--dump-select`
and `--dump-alloc` are gone, and the 146 lines that hold `select` are the
statement of channels, the `select` instruction of LLVM and `xcode-select`.

### Two options no test exercises

`src/antic/main.c:295`, tests, minor, new.

`data/options-unexercised.txt` holds the 60 options `src/antic/main.c`
reads and the 41 of `src/anti/main.c`, with the number of files under
`tests/` that hold each. Five of antic and two of anti have none.
`--dump-tokens`, `--dump-ast` and `--dump-types` are exercised after all:
`tests/CMakeLists.txt:352` builds `--dump-${stage}`. `--soname` and
`--strip-docs` of both tools have no test. `tests/unit/test_link.c:406`
checks the `-soname` of the link command, not the option that asks for it.

### Functions past a threshold

Rule 18, minor, still open. `data/thresholds.txt` lists 142 functions: 83
past 100 lines, 19 nested deeper than 4 and 48 taking more than 6
parameters, against 163 at the second audit. `data/thresholds-compared.txt`
marks 32 new. The groups of the new ones:

- Splits of a long function that stay long. `check_let` (149 lines),
  `check_for` (146) and `check_switch` (112) came out of `check_stmt` of
  `sema_stmt.c`, which fell from 645 lines to 235. `driver_build_c_library`
  (199) is `build_c_library` of the old `driver.c` (188) moved to
  `driver_library.c`, and `driver_load_libraries` (104, nest 5) is
  `load_libraries` renamed. `postfix_links` (104, nest 6) is `postfix` of
  the old parser.
- Dispatch over node kinds, which follows the shape of the problem:
  `walk_at` (151), `check_expr` (104) and `walk_stmt` (104) of
  `antl_verify.c`, `walk_children` (127) of `ast_walk.c`, `antl_read_ir`
  (140) and `read_body` (102) of `antl_read_ir.c`, `read_tree` (115) of
  `antl_tree.c` and `antl_visit_type` (105) of `antl_write.c`.
- Steps in sequence: `parser_parse` (135), `llvm_emit_module` (112),
  `bundle` (108) and `same_signature` (101), judged from their counts
  alone.
- Deep nests: `read_string` of `src/rt/json.c:87` (nest 6) reads escapes
  inside a string, `collect_tbaa` (5) and `frames_kept` (5) of
  `llvm_emit.c` and `optimize_drop_failures` (5), judged from their counts
  alone.
- Parameter lists: `llvm_debug_init` (8), and `base_options`,
  `class_uses`, `antic_library_header`, `work_on`, `unit_options`,
  `member_at`, `ir_binary_wrap` and `each_array_element` with 7 each.
  `anti_rt_regex_stop_group_bytes` (7) matches an `extern fn` of
  `src/std/anti/regex.anti` and stays.

The longest functions are now `check_expr_inner` (641), `statement_level`
(410), `primary` (356), `run` of `main.c` (329) and `dump_expr` (315), all
kept from the second audit, whose judgements hold.

### Files past 3000 lines

Rule 19, minor. `data/file-lengths-compared.txt` lists two files.
`llvm_emit.c`, 4102 lines, is new, since the LLVM back end came after the
second audit. `sema_stmt.c`, 3089 lines, returned:
`docs/audit/2026-09-30/closure.md` records every file of `src/` under 3000
lines at `b4736301`, where it had 2962, and it has grown by 127 since. `sema.c` stands at 2948.

## Closed since the second audit

The evidence shows these findings of the second audit gone:

- Rule 22. `data/platform-conditionals.txt` finds no `#if` on the host
  outside the platform layer, against 110. The one line it lists,
  `header.c:789`, is a string literal antic writes into a C header.
- The `uint64x2_t` of sixteen `bool` lanes. The `deadcode.DeadStores`
  warning at `header.c:619` is gone.
- The shared helpers that crossed, `notice.h` and `userdirs.c`.
- The 29 wrappers of `antl.c`, which is 180 lines, and the 35 declarations
  under the wrong file: `data/header-sections.txt` finds none in the six
  shared headers, 548 declarations.
- The two-way calls of the splits. `data/call-graph.txt` shows
  `sema_expr.c` and `sema_call.c` calling each other 24 and 20 times,
  against 68 and 69, and `lower_stmt.c` calling `lower_expr.c` 40 times,
  against 73. The value rules stand in `sema_value.c`, which six files
  call and which calls `sema.c` and `sema_generic.c` alone. The LLVM back
  end runs one way: `llvm_emit.c` calls `llvm_debug.c` and
  `llvm_target.c`, and nothing calls back.
- `-Wmissing-prototypes` in `lock.c`, `sync.c`, `threads.c` and
  `errno.c`, 28 lines, and the raw format attributes of `diagnostic.h`
  and `bindmodel.h`.
- Eight of the nine files past 3000 lines.

The other witnesses of the tool pass: `data/unused-static.txt` finds no
static function its file does not call, and `-Wunused-function` reports
none. `data/rt-functions-unused.txt` finds every one of the 69 rows of
`RT_FUNCTIONS` named by antic. `RT_FN_F16_TO_F32`, `RT_FN_F32_TO_F16` and
`RT_FN_INIT` are named by `llvm_emit.c` and `driver.c` rather than
lowering. `data/todo-marks.txt` finds no TODO, FIXME, XXX or HACK under
`src/` or `docs/`, and one under `tools/`, the allow list of the docs-style
checker itself.

## False positives

Each class once, with the reason.

- `core.NullDereference` in `antl_verify.c:339` and `467`. Each path has
  `need` set `failed` on a NULL and then assumes `failed` false at the loop
  condition on the next line, which `need` at line 110 rules out.
- `core.NullDereference` in `docpage.c:626`, the second audit's
  `doc.c:645` moved into antic. `sema_strip_generics` stores no NULL item.
- `core.CallAndMessage` in `llvm_emit.c:2456` and `2460`. The path needs a
  result of zero words in `return_aggregate`. `abi.c:102` refuses an
  aggregate of no bytes, so a coerced result has one word or two.
- `core.NullDereference` in `sema_call.c:565`. The path needs
  `sema_struct_of` to give NULL after `promote_base`, and `promoting_field`
  gives a field whose type holds the member.
- `core.NullPointerArithm` in `sema_safety.c:149`. `types_find_field` at
  `types.c:1892` searches `s->fields` alone, so a found `f` lies in the
  array it is subtracted from.
- `core.UndefinedBinaryOperatorResult` in `platform_windows.c:1014`.
  `units_of` copies `n + 1` units, and the caller at line 1069 computes `n`
  and the count from one walk of the same block.
- The 26 kept analyzer warnings match the second audit's classes at moved
  lines, and its reasons hold. `layout.c:98` and `313` and `sema_simd.c:158`
  need a lane or a field of no bytes, which `antl_verify.c` and
  `check_simd_struct` refuse before layout.
- `-Wmissing-prototypes`, 26 functions of `src/rt/patterns.c` and 2 of
  `text.c`. An `extern fn` of `src/std/anti/regex.anti` or `text.anti`
  declares each, and generated code calls them by name.
- `-Wcast-align` in `src/rt/object.c:1050`, `1132` and `1133`, and
  `-Wformat-nonliteral` in `sema.c:321`, the earlier audits' lines moved.
- `-Wmissing-variable-declarations`, five in `platform_windows.c`.
  `_tls_index`, `_tls_used`, `_fltused` and `__security_cookie` are the
  names the linker and the loader look for, which the test `rt_names`
  lists, and `anti_rt_no_licenses` is the default of an `/alternatename`.
  All stand in the platform layer.
- `-Wunused-macros` for `PCRE2_STATIC` in `regex.c:9` and `patterns.c:122`.
  `pcre2.h:57` reads it on Windows alone, to drop `dllimport`, and the
  POSIX builds report it unused.
- `banned-calls.txt`. The twelve lines are the definitions of `sprintf`,
  `vsprintf` and the scanf family in `platform_stdio.c`, which the headers
  of the UCRT declare and `ucrtbase.dll` does not export, and the calls
  inside them. Rule 9 bans calls, and no other file makes one. The gap is
  the missing test above.
- The exact block of `platform_posix.c:827` and `platform_windows.c:1107`,
  `cpuid` and `xgetbv` as inline assembly. The DESIGN comment at
  `platform_windows.c:1112` says why both platform files hold it.
- `data/design-comments.txt` marks 489 of the 1243 DESIGN comments as
  holding neither "because", a second colon nor a reason word. A sample of
  forty reads as giving the reason in another form, as `sema_hash.c:379`
  does with "therefore". The list is evidence for the documentation steps,
  not a count of defects.
- `redirect` of `sema_copies.c:863` repeats the child walk of
  `ast_walk.c:66`. The closure of the second audit records that the walks
  of the copy pass build every field of a node and are no analyses.

## The sentence of each file

The files of this step are the data files, and the source files whose own
header says nothing. For each data file, what it holds. For each of the 25
source files without a header comment, the sentence as read from the code,
marked "header: none".

- `data/analyzer.txt`: the output of the static analyzer per compile unit.
- `data/analyzer-unique.txt`: each analyzer warning once.
- `data/analyzer-compared.txt`: those warnings against the second audit.
- `data/warnings.txt`: the output of the compile with the extra warnings.
- `data/warnings-unique.txt`: each compiler warning once.
- `data/warnings-compared.txt`: those warnings against the second audit.
- `data/banned-calls.txt`: the grep for the functions rule 9 bans.
- `data/function-metrics.txt`: length, nest and parameters per function.
- `data/file-lengths.txt`: lines per file.
- `data/thresholds.txt`: the functions past a threshold of rule 18.
- `data/thresholds-compared.txt`: those functions against the second audit.
- `data/file-lengths-compared.txt`: the files past 3000 lines now and then.
- `data/include-graph.txt`: every include that crosses a part, and the
  crossings against the direction.
- `data/call-graph.txt`: the calls between the files of each split.
- `data/header-sections.txt`: declarations under the wrong file comment.
- `data/duplication.txt`: blocks two files share.
- `data/platform-conditionals.txt`: host conditionals outside the layer.
- `data/first-comments.txt`: the first comment of every source file.
- `data/header-function-comments.txt`: the comment before every declared
  function.
- `data/todo-marks.txt`: every TODO, FIXME, XXX and HACK.
- `data/design-comments.txt`: every DESIGN comment and its reason marks.
- `data/named-files.txt`: the file names the documents hold.
- `data/unnamed-files.txt`: the files of src/ no document names.
- `data/unused-static.txt`: static functions nothing calls.
- `data/options-unexercised.txt`: each option and the tests that hold it.
- `data/rt-functions-unused.txt`: each row of RT_FUNCTIONS and who names
  it.
- `data/back-end-mentions.txt`: the words of the native back end.
- `data/commands.txt`: the command behind each file.

The source files, by the code:

- `src/antic/alloc.c`: the checked allocation of antic, which exits on a
  product or sum that overflows or a block the heap refuses. Header: none.
- `src/antic/diagnostic.c`: the list of errors, warnings and doc warnings
  of one compile, and the printing of each. Header: none.
- `src/antic/sha256.c`: the digest of a file by SHA-256. Header: none.
- `src/antic/ir_fold.c`: the folding of an IR operation on constant
  operands. Header: none, the first comment describes `bits`.
- `src/antic/ir_print.c`: the text form of a module of the IR. Header:
  none.
- `src/antic/layout.c`: the sizes, alignments and offsets of the
  aggregates of the IR for one target, and the folding of the symbolic
  values that depend on them. Header: none, `layout.h` has it.
- `src/antic/linker.c`: the command line of lld for a program or a library
  of one target, and the inputs of the link. Header: none, `linker.h` has
  it.
- `src/antic/llvm_debug.c`: the debug metadata the LLVM text carries for
  `-g`, files, functions and the line of each statement. Header: none.
- `src/antic/lower.c`: the helpers every file of lowering uses, the
  declarations of the runtime functions and the items of a module. Header:
  none, `lower_lowerer.h` has it.
- `src/antic/lower_desc.c`: the descriptors, tables and field records of
  classes, structs and interfaces. Header: none, `lower_lowerer.h` has it.
- `src/antic/main.c`: the command line of antic and the run it asks the
  driver for. Header: none.
- `src/antic/memcheck.c`: the declarations and calls of AddressSanitizer
  that `--memory-checks` puts before every load and store. Header: none,
  `memcheck.h` has it.
- `src/antic/modpath.c`: module paths, from a source path and back, and
  their segments. Header: none.
- `src/antic/notice.c`: the licence notice of a program, built from its
  components. Header: none.
- `src/antic/pattern.c`: the check of a pattern literal at compile time,
  through PCRE2 and the check `exponential-pattern`. Header: none, the
  first comment is the DESIGN of the check.
- `src/antic/types.c`: the pool of types, their construction, comparison
  and printing. Header: none, `types.h` has it.
- `src/antic/warnings.c`: the table of warning names and the clauses
  `allow` and `unchecked` that act on them. Header: none.
- `src/antic/lexer.h`: the tokens of Anti and the lexer that makes them.
  Header: none.
- `src/antic/lower.h`: the entry of lowering, a checked module into a
  module of the IR, and its options. Header: none, the first comment is the
  DESIGN of `--no-hooks`.
- `src/antic/optimize.h`: the optimizer of the IR over a program or one
  module. Header: none.
- `src/antic/sema.h`: the symbols, scopes and constant values of the
  checker, and its entry. Header: none.
- `src/antic/warnings.h`: the names of warnings and checks and the clauses
  that silence them. Header: none.
- `src/anti/files.c`: the file helpers of anti, reading, writing, listing
  and removing, which end the program when memory runs out. Header: none.
- `src/rt/text.c`: the `str` and `[]byte` operations of the runtime,
  copies, comparison, the builder and the conversions of numbers. Header:
  none, the first comment is the DESIGN of `to_bytes`.
- `src/rt/utf.c`: the UTF-8 encoder, decoder and repair, UTF-16 to UTF-8,
  and the split of a Windows command line. Header: none, `utf.h` has it.
