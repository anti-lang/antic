# Tool pass

The machine evidence of the C audit, gathered at `ab03345` on macOS arm64
with the pinned clang 23 and no build file changed. The clang static
analyzer ran over every C file of `src/antic/` and `src/anti/` for the host,
and over every file of `src/rt/` three times, for macos-arm64, linux-x86_64
and windows-x86_64, so that each platform branch was read. `src/native/`
holds no C file yet, only `CMakeLists.txt`. The same files were compiled
once more into `build/audit/` with the extra warnings the step names. A
grep found the banned string calls, and a script measured every function
and file. The raw results and the exact commands are in `docs/audit/data/`,
with `commands.txt` describing each file. Every item below was checked
against the code. The one severe finding was also reproduced with the
UBSan build of antic.

## Counts

| Source | Raw | Unique | Real | False positive |
|---|---|---|---|---|
| Analyzer warnings | 117 | 92 | 57 | 35 |
| Compiler warnings | 91 | 34 | 0 | 34 |
| Banned calls | 0 | 0 | 0 | 0 |
| Functions past a threshold | 119 | 119 | 119 reported | |
| Files past 3000 lines | 3 | 3 | 3 reported | |

A warning counts as real when it points at a finding below, even where
the finding is not the one the tool named. The 38 `unix.Stream` warnings
are counted so, since they lead to the major finding on read errors,
although their claim of undefined behaviour is wrong.

| Severity | Rule | Findings |
|---|---|---|
| Severe | 14 | 1 |
| Major | 14 | 1 |
| Major | 12 | 1 |
| Minor | 26 | 2 |
| Minor | 18 | 1, covering 119 functions |
| Minor | 19 | 1, covering 3 files |

## Severe

### A `simd struct` whose one field is `_` divides by zero in layout

`src/antic/layout.c:71`, rule 14, severe.
`src/antic/sema.c:539` is where the input should have been refused.

`check_simd_struct` rejects a lane with `bits != 0` but accepts the
zero-width field `_`, whose `bits` is 0. For
`simd struct V { _: i64 : 0 }`, `compute` in `layout.c` then sees no bits:
`bit` stays 0, `align = bytes < 16 ? bytes : 16` gives 0 at line 283, and
`round_up((bit + 7) / 8, align)` divides by zero at line 71. The UBSan antic
stops there on the two-line program in `data/probe-simd-unit-break.txt`.
The ordinary build passes over it, because `udiv` by zero gives 0 on
ARM64. An x86_64 host traps. The front end meanwhile computes
`size_of(V)` as 8 through `fixed_layout`, so the two layouts disagree. A
library file reaches the same line without the checker: `read_tables` of
`antl.c` accepts an IR aggregate with the simd flag and a field named `_`.
The fix is to refuse `_` in `check_simd_struct`, and to refuse a simd
aggregate without a lane in the library reader.

The analyzer's other division by zero, `sema.c:2391`, is a false
positive. It needs a simd struct without fields, and the parser and the
library reader both refuse one.

## Major

### Read loops take a read error for the end of the file

`src/anti/build.c:51`, rule 14, major.

Each `read_file` of `src/anti/` loops `while ((n = fread(...)) > 0)` and
then returns true without calling `ferror`. A read error ends the loop as
the end of the file does, and the caller parses a truncated `anti.toml`,
lock file, index or source as if it were whole. The analyzer reported each
loop as `unix.Stream`. Its claim of undefined behaviour is wrong, since a
further `fread` after an error is defined, but the missing check is real.
The fix is `ferror(f)` after the loop and a refusal when it is set, as
`read_source` and `read_bytes` of `driver.c` and `sha256_file` already do.

The same loop without the check stands in `src/anti/bind.c:87`,
`check.c:69`, `deps.c:47`, `fmt.c:1265`, `manifest.c:32`, `repo.c:42`,
`sdk.c:34`, `symmap.c:49`, `syms.c:56`, `test.c:66`, `units.c:37` and
`zip.c:79`, the lines of the `fread`. It also stands in
`src/antic/driver.c:805`, where `digest_file` then digests a partial file
into the build id, and in `src/rt/trace.c:140`, where `load` keeps a short
image and hands it to `anti_macho_relocate`.

### The two `main` functions free by hand before each return

`src/anti/main.c:296`, rule 12, major.

Each command block of `anti`'s `main` allocates two arrays and releases
them by hand before each `return`. When one of the two `malloc` calls
fails, the other is returned without a `free`:
`if (sources == NULL || roots == NULL) { ... return 70; }`. The same shape
stands at lines 348, 487 and 539, and in `src/antic/main.c:439`, where
eight arrays are allocated and a failure of any one leaks the other seven.
The process exits at once, so the leak costs nothing at run time, but the
functions break the rule of one cleanup block. The fix is a `goto` to one
cleanup block per function, or one function per command, as the threshold
finding below suggests for `anti`'s `main`.

## Minor

### Twelve copies of one file reader

`src/anti/build.c:51`, rule 26, minor.

`static bool read_file(const char *path, struct text *out)` is written out
in twelve files of `src/anti/`: `bind.c`, `build.c`, `check.c`, `deps.c`,
`fmt.c`, `manifest.c`, `repo.c`, `sdk.c`, `symmap.c`, `test.c`, `units.c`
and `zip.c`, with `read_all` of `syms.c` as a thirteenth. They come in four
variants, which differ only in the size of the buffer, 4096, 8192 or 65536
bytes, and in whether a failed `fopen` prints a message. `files.c` already
holds the shared file helpers of `src/anti/`, and one reader there would
also carry the `ferror` check of the major finding once.

### Stores that are never read

`src/antic/header.c:547`, rule 26, minor.

`const char *neon = "uint8x16_t";` is overwritten on every path of the
`switch` that follows, because its `default` sets `neon = "uint64x2_t"`.
The initial value looks like the intended case for byte lanes, and a
simd struct of sixteen `bool` lanes reaches the `default` and is written
to the C header as `uint64x2_t`. The register and the ABI are the same, so
this is a naming defect, not a crossing defect. The fix is a `TYPE_BOOL`
case and no initial value. `src/rt/start.c:159` holds a plain redundant
initialiser, `const wchar_t *p = block;`, which the `for` sets again.

### Functions past a threshold

Rule 18, minor. `data/thresholds.txt` lists 119 functions: 68 past 100
lines, 27 nested deeper than 4 levels and 40 taking more than 6
parameters. The nest counts brace blocks inside the body. The judgement
per group:

- Pass drivers, where a split would make the code clearer.
  `sema_check` at `src/antic/sema.c:9778` is 838 lines and takes 9
  parameters. It is about twenty passes in sequence over
  `module->items`, each under its own comment, and each would read better
  as a static function taking the checker. `main` at
  `src/anti/main.c:215`, 422 lines, is one block per command and splits
  into one function per command, which also settles the rule 12 finding.
  `run` at `src/antic/main.c:138`, 281 lines, is one flag loop and follows
  the shape of the problem.
- Dispatch over node kinds, where the size follows the shape of the
  problem. `check_stmt` (`sema.c:8253`, 518 lines, nest 7), `eval_const`
  (`sema.c:6715`, 482, nest 6), `check_expr_inner` (`sema.c:5952`, 447),
  `statement` and `primary` of `parser.c`, `lower_stmt` and
  `lower_expr_value` of `lower.c`, `dump_expr` and `dump_stmt` of
  `ast_dump.c`, `check_inst` of `ir_verify.c`, `instruction` of
  `ir_print.c` and `print` of `x86_64.c` are each built around a
  `switch` over the kind, with 20 to 53 cases. Where a single case
  carries the nest of 6 or 7, moving that case into a function is worth
  a look.
- Readers and writers of a format, which follow the format.
  `read_types` (`antl.c:1357`, 344 lines, nest 6), `put_type`, `put_ir`,
  `read_ir` and `read_tables` of `antl.c`, and `bind_read_clang` and
  `read_fields` of `bindclang.c`. `read_types` handles every kind in one
  `switch`, and a function per kind would read better.
- Deep loops in the optimizer, where a split would help most.
  `split_slots` at `src/antic/optimize.c:1119` reaches a nest of 9. It has
  three phases, collecting the fields of a slot, rewriting the loads and
  stores and deleting the addresses, and each phase scans every block and
  instruction inside the slot loop. One function per phase would lower
  the nest. `remove_unused_functions`, `check_singletons` of
  `whole.c`, `rematerialise_constants` of `regalloc.c`, `write_slots`,
  `reach_program` and `reach_calls` nest 5 or 6 for the same reason.
- Long parameter lists that carry options, where a struct would be
  clearer. `check_run` (8), `doc_run` (10) and `test_run` (10) of
  `src/anti/` take the parsed flags one by one, while `build_run` and
  `bind_run` already take a request struct. In `src/antic/`, `emit` (12),
  `dump_ir` (12), `emit_program` and `emit_module` (11), `lower_checked`
  (10), `whole_checked` (9), `back_end` (7) and `antl_read` (9) pass
  fields of `struct options` one at a time.
- Long parameter lists that are the shape of the problem. The builders of
  `ir.c` (`ir_vshuffle`, `ir_vselect`, `ir_vbinary` and the rest, 7 to 8)
  take one operand each. `fold_chunks` of `arm64.c` and `x86_64.c`,
  `check_call` and `check_branch` of `lower.c` and
  `anti_rt_parallel` at `src/rt/threads.c:276`, whose 9 parameters are the
  ABI that generated code calls, stay as they are.
- The runtime's own long functions. `unit_line` of `src/rt/symbols.c:290`
  (163 lines) reads the header of a DWARF line table and then runs its
  program, and the two parts split cleanly. `anti_rt_read_float` of
  `text.c` (134), `read_value` of `registry.c` (120) and
  `anti_rt_plugin_load` of `plugin.c` (101) are one algorithm or one
  sequence of checks each, judged from their outline, and sit close to
  the line. `anti_rt_json_string` and
  `anti_split_command_line` nest 5 in escape and quote handling that
  follows the rules they implement.

### Files past 3000 lines

Rule 19, minor.

- `src/antic/sema.c`, 11491 lines. It splits along its parts: the item
  passes of `sema_check` and the declarations they need, expressions and
  calls (`check_expr_inner`, `check_call`, `method_call`, `check_binary`,
  `check_cast`), constant evaluation (`eval_const` and its helpers),
  statements with narrowing, `sync` and the set analysis, and the export
  and doc checks from `check_c_type` at line 10700 to the end.
- `src/antic/lower.c`, 9026 lines. The descriptors and tables
  (`class_descriptor`, `struct_descriptor`, `interface_thunk`), the simd
  lowering around `lower_simd`, expressions and calls, statements and
  loops, and the module and tracing code from `traced_function` on are
  each a file's worth.
- `src/antic/x86_64.c`, 3005 lines, 5 over the line. The calling
  convention (`locate`, `sysv_classify`, `windows_by_value`) and the text
  printer (`print_operand`, `print`) could part, but at this size the
  split is optional, and `arm64.c` at 2680 lines keeps the same shape.

## Banned calls

None. The grep over `src/rt/`, `src/antic/` and `src/anti/` found no
`strcpy`, `strcat`, `sprintf`, `vsprintf`, `gets` or `strncpy`, and no
`scanf` form with a bare `%s`. The four `sscanf` calls in
`src/antic/applesdk.c:25` and `src/anti/syms.c:961`, `1148` and `1152`
bound every `%s` by a width one below the buffer: `%7s` into `rest[8]`,
`%511s` and `%1023s` into `name[512]` and `where[1024]`, and `%79s` into
`id[80]`. They read their integers with `%d`, `%lld` and `%llx`, which C11
leaves undefined for a number out of range. The inputs are the names of
the SDK directory and the output of the symbolizer, so that is left to
the step that audits those readers.

## False positives

Each class once, with the reason.

- `unix.Malloc` in `src/rt/conf.c:412`, `738` and `751`. The path that
  `resolve` or `copy` allocates is kept on purpose as the source of a key,
  which `--anti.inspect` prints, and the comment above `read_file` at
  line 546 says the reader keeps it.
- `core.NullDereference`, `core.NullPointerArithm`,
  `unix.cstring.NullArg` and `unix.StdCLibraryFunctions` in
  `src/rt/conf.c:95`, `96`, `306`, `315`, `346`, `352`, `390` and `460`.
  Each path continues after `startup_error` or `unknown_option`, and
  both end in `exit(70)`. The analyzer does not know they do not return.
- `core.NullDereference` in `src/antic/lower.c:479`, `519`, `3710`, `6624`
  and `6625`. Lowering runs only on a tree that `sema_check` accepted, in
  which every expression has a type and every name its symbol.
- `core.NullDereference` in `src/antic/sema.c:10264`, `10310` and
  `10524`. Each loop skips an item unless `type_has_fields(t)`, which
  returns false for NULL in `types.c`, a file the analyzer does not see.
- `core.NullDereference` in `src/antic/sema.c:6411`. `bitfield_width` is
  called only when the field has a width expression or is `_`, and it
  returns before `check_expr` for `_` without one.
- `core.NullDereference` in `src/antic/sema.c:8787`. A top-level function
  always has a body, since the parser expects `{`, and the loop over the
  functions of a class body skips those without one.
- `core.NullDereference` in `src/antic/x86_64.c:1147` and `1249`. `agg`
  is NULL only for a variadic argument, and `sema.c:4703` allows only
  scalars there.
- `core.NullDereference` in `src/antic/optimize.c:842`. The address of the
  last element of a non-empty array is never NULL.
- `unix.Malloc` double free in `src/antic/optimize.c:93`. The slot
  instruction precedes every `ptradd` of its result in its block, so
  deleting those does not move it, and its op is not `IR_PTRADD`.
- `core.BitwiseShift` in `src/antic/arith.c:12` and `66`. `arith.h` fixes
  `n` at 8, 16, 32 or 64, and every caller passes the width of an IR
  integer type.
- `core.BitwiseShift` in `src/antic/layout.c:541`. `bit_unit` refuses a
  bitfield that spans more than 8 bytes, so `width` is at most 64, and the
  two loads call `low_bits` only for a width below the unit.
- `core.UndefinedBinaryOperatorResult` in `src/antic/parser.c:202`. The
  lexer always ends the list with `TOKEN_EOF`, which is not a doc token,
  so `origin[0]` is written.
- `core.UndefinedBinaryOperatorResult` in `src/rt/start.c:113`.
  `copy_units` copies the terminating 0 unit, which the analyzer cannot
  tie to `wcslen`.
- `core.CallAndMessage` in `src/anti/jsontree.c:140`. The analyzer
  follows a path on which `read_string` fails and the caller goes on,
  but `fail` returns false and the caller breaks.
- `core.CallAndMessage` in `src/anti/manifest.c:70`. `add_entry` reads
  only the first `count` entries, and `count` starts at 0 after the
  `memset` and grows with each entry written.
- `unix.cstring.NullArg` in `src/rt/fs.c:185`. With `capacity` 0 the
  growth branch always runs, so `bytes` is not NULL at the copy.
- `-Wmissing-prototypes`, 19 functions of `src/rt/errno.c`, `platform.c`,
  `sync.c` and `threads.c`. Generated code calls each by name from
  `lower.c`, or Anti code of `src/std/anti/` declares it `extern`, so
  none belongs under rule 20.
- `-Wmissing-format-attribute` on `startup_error` of `conf.c`, `fail` of
  `plugin.c`, `add` of `diagnostic.c` and `refuse` of `bindclang.c`. The
  attribute is an extension that rule 1 keeps out of these files.
- `-Wformat-nonliteral` in `src/antic/sema.c:250`. Every caller of
  `declare` passes one of two literal formats.
- `-Wenum-enum-conversion` in `src/rt/hooks.c:79`, `object.c:642` and
  `registry.c:161` and `226`. `ANTI_ENTRY_OF_HANDLER` adds a hook number
  to a table index, and both are small non-negative constants.
- `-Wcast-align` in `src/rt/object.c:624`, `706` and `707`. The `char *`
  comes from `malloc` or from an object pointer moved by the size of its
  own type, so the alignment is already that of the object.
- `-Wsign-conversion` in `src/rt/trace.c:228`. The bytes promote to
  non-negative `int` values below 2^24 before the conversion.
