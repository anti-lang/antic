# Tool pass

The machine evidence of the second audit, gathered at `70e8860` on macOS
arm64 with the pinned clang 23 and no build file changed. Every compile
line is the one the host build runs. The static analyzer and the extra
warnings ran over `src/antic/`, `src/anti/`, and `src/rt/` for macos-arm64,
linux-x86_64 and windows-x86_64. The glue of PCRE2 is covered too:
`src/native/` compiles `src/rt/regex.c` and `patterns.c`, and holds no C
file of its own. Scripts measured every function and file, drew the
include graph and the calls between the split files of the checker and
of lowering, and found blocks that two files share. The raw results are
in `docs/audit/data/`, and `commands.txt` there names each file and
command. Each item below was read in the code. Four foreground reviewers
read the 43 analyzer warnings in parallel, and each verdict they gave was
checked against the code before it went in. One finding was reproduced
with a program the built antic compiles and runs.

## Counts

| Source | Now | Then | New | Kept | Gone |
|---|---|---|---|---|---|
| Analyzer warnings, unique | 43 | 92 | 23 | 20 | 72 |
| Compiler warnings, unique | 74 | 34 | 52 | 22 | 12 |
| Banned calls | 0 | 0 | 0 | 0 | 0 |
| Functions past a threshold | 163 | 119 | 47 | 116 | 3 |
| Files past 3000 lines | 9 | 3 | 7 | 2 | 1 |
| Host `#if` outside the platform layer | 110 | | | | |

"Then" is the first audit at `ab03345`, in `docs/audit/2026-09-23/data/`.
The `*-compared.txt` files match each item across the move of lines and
the split of `sema.c` and `lower.c`. Of the 43 analyzer warnings, 6
point at real defects. Four lead to the finding on the generic trees and
one to the lane without bytes, although the analyzer's own path there is
impossible. One is a still-open minor finding. No compiler warning is a
defect, and 11 lead to a minor finding under rule 1.

| Severity | Rule or question | Findings | New | Still open |
|---|---|---|---|---|
| Severe | 14 | 2 | 1 | 1 |
| Severe | One layout rule | 1 | 1 | 0 |
| Major | One-way dependencies | 2 | 2 | 0 |
| Major | 22, one platform layer | 1 | 0 | 1 |
| Major | The splits of `sema.c` and `lower.c` | 1 | 1 | 0 |
| Minor | The splits of `sema.c` and `lower.c` | 2 | 2 | 0 |
| Minor | One-way dependencies | 1 | 1 | 0 |
| Minor | 26 and duplicated code | 5 | 3 | 2 |
| Minor | 1 | 1 | 0 | 1 |
| Minor | 18 | 1, covering 163 functions | 0 | 1 |
| Minor | 19 | 1, covering 9 files | 0 | 1 |

No finding returned. Since the first audit, the evidence shows these
closed: the `sscanf` calls of S41, the read loops without `ferror` of M1,
the twelve file readers of `src/anti/`, the copy of SHA-256 in
`src/antic/sha256.c`, the `main` of `anti` past 100 lines, and the
`_` lane of S1.

## Severe

### The generic trees of a library file reach lowering unchecked

`src/antic/antl_tree.c:1258`, rule 14, severe, new.

Commit `8935f19` put the checked tree of each generic into the library
file. The copy of a generic re-checks only the nodes that depend on a
type argument, as the `DESIGN` comment of `sema_copies.c:7` says. The
rest of the tree goes to lowering as the reader built it. The reader
checks no relation inside a tree:

- `io_size` takes `switch_stmt.otherwise_at` as any u32. Lowering writes
  `entry[k] = l->b;` at `src/antic/lower_stmt.c:2331` with that `k`,
  into an array of `arms + 1` slots, which is a write outside the block.
- `io_sym` at `antl_tree.c:657` and `io_ref` at `527` take reference 0 as
  NULL in any slot. A `for` with `pattern` set and no `element` reaches
  `s->as.for_loop.element->type` at `lower_stmt.c:536`. A `for` without
  names reaches `sym->address_taken` at `662` and `sym->type` at `821`.
  An `EXPR_NAME` without a symbol reaches `sym->ir` at
  `src/antic/lower_expr.c:1023`.

The analyzer reported the four NULL reads. For source input each is a
false positive, as the first audit judged `lower.c:3710`, because the
checker sets every symbol. The fix belongs in the tree reader: a check
of each tree after reading, not NULL tests in lowering.

### The checker and the back end lay out one struct differently

`src/antic/sema_expr.c:2258`, one layout rule, severe, new.

`fixed_layout` computes the size of a plain struct by the C rules, so
that `as` between a simd struct and a plain struct copies equal bytes.
It skips a bitfield but not the unit break `_`, whose `bits` is 0. It
counts `_: i32 : 0` as a field of 4 bytes. `layout.c:273` makes `_`
align the next field and take no bytes. For `struct S { a: i16, _: i32 :
0 }` the checker computes 8 bytes. Windows lays it out in 2, and every
other target in 4. The checker then
accepts `s as V` for a `simd struct V { x: i32, y: i32 }`, and
`lower_simd_cast` copies the 8 bytes of `V` out of the 4 of `S`.
`data/probe-unit-break-cast.txt` holds the program, its IR and its
output, `4 75628545 1`: the second lane is read from outside `S`.
`--memory-checks` reports nothing. The first audit named the two layouts
in the text of S1 without a finding of its own. The fix is one layout
rule: the checker refuses `_` in `fixed_layout`, or the check moves to
the back end, which has the real sizes.

### A library simd aggregate whose lane holds no bytes divides by zero

`src/antic/layout.c:96`, rule 14, severe, still open.

The fix of S1 refuses the lane `_` at `sema.c:592` and `antl.c:2796`, and
a simd aggregate without fields. `ir_lane` checks the name and the width
of a lane but not its type. A library file with a struct `Z { _: i64 :
0 }`, 0 bytes on every target, and a simd aggregate `V { a: Z }` passes
`read_tables`. `compute` then sets `align = bytes < 16 ? bytes : 16`,
which is 0, and `round_up` divides by 0 at line 96. An x86_64 host traps
and ARM64 gives 0. Source cannot reach it, since `check_simd_struct`
requires a scalar lane. The fix is to refuse a lane that is no scalar
in `ir_lane`.

## Major

### The anti tool uses the internals of antic, and no header is its interface

`src/anti/units.c:91`, one-way dependencies, major, new.

`antic_core` exports every header of `src/antic/`, and no document names
the one `anti` should use. `data/include-graph.txt` lists 9 files of
`src/anti/` that include headers of the front end, the IR or the library
reader. `units.c:91` and `check.c:348` call `lex` and `parse` and walk
the syntax tree for imports and doc blocks. `doc.c` walks `struct type`
and `struct item` of a checked module and includes `ir.h`. `fmt.c`,
`bindtype.c`, `check.c` and `units.c` include `lexer.h`. `build.c`, `deps.c`,
`repo.c`, `check.c`, `doc.c` and `units.c` include `antl.h`, and
`build.c`, `main.c` and `sdk.c` include `linker.h` of the back end. A
change of the tree or of a checker type then changes `anti`, and a
change of meaning, such as how an import is recorded, compiles without
error. The fix is a header beside `driver.h` that states what `anti` may
call, with the tree walks behind it.

### The checked allocator of antic lives in the IR

`src/antic/ir.h:477`, one-way dependencies, major, new. The duplication
part is still open.

`ir_alloc` and `ir_grow` are the checked allocation of the compiler, and
they stand in `ir.c`. The C header writer of the front end includes
`ir.h` for them at `src/antic/header.c:8`, the one include of the front
end into the IR. The other parts write their own growth and exit:
`fputs("antic: out of memory\n", stderr);` stands 49 times in 23 files
of `src/antic/`, `sema.c:275`, `lexer.c:281` and `parser.c:59` among
them. The
first audit counted 76 and reported the copies under rule 26 at
`ir.c:21`. `text.c` or a file of its own is the place for one allocator
that every part may include.

### Host conditionals stand in 23 files outside the platform layer

`src/rt/trace.c:11`, rule 22, major, still open.

Rule 22 now names the platform layer, and `platform_posix.c` and
`platform_windows.c` exist. `data/platform-conditionals.txt` still lists
110 `#if` lines on `_WIN32`, `__APPLE__`, `__linux__` or `_MSC_VER`
outside it. The runtime holds 81 of them, in `trace.c` and `fs.c` with
13 each, `threads.c` with 12, then `cpu.c`, `signal.c`, `lock.c`,
`mem.c`, `loaded.c`, `sync.c`, `start.c`, `errno.c`, `init.c` and
`atomic.c`. `src/antic/` holds 18, in `target.c`, `selfpath.c`,
`driver.c`, `applesdk.c`, `userdirs.c`, `process.c` and `process.h`. `src/anti/` has
no platform file, although rule 22 names `src/anti/platform.c`.
`files.c` includes `../antic/platform.h` and holds 6 conditionals, with
5 more in `sdk.c` and `syms.c`. This is M29 of the first audit, whose
document half is done.

### The splits of `sema.c` and `lower.c` left files that reach into each other

`src/antic/sema_checker.h:4`, the splits of `sema.c` and `lower.c`,
major, new.

The checker grew from 11491 lines to 22531 in 11 files, and lowering
from 9026 to 12972 in 7. `sema.c`, `sema_expr.c`, `sema_call.c`,
`sema_stmt.c`, `lower.c` and `lower_expr.c` are each past 3000 lines.
`data/call-graph.txt` shows each domain file calling `sema.c` or
`lower.c`, which hold the shared helpers, as `sema_checker.h:4` and
`lower_lowerer.h:4` intend. The domain files also call each other both
ways: `sema_expr.c` and `sema_call.c` make 68 and 69 calls into each
other, and `sema_stmt.c` 65 into `sema_expr.c`. Part of this follows
from the grammar, since a call holds expressions. Part is placement:

- `sema_new_node` builds a tree node and stands in `sema_call.c:44`, and
  `sema_expr.c` calls it 30 times.
- The value rules are in three files. `sema_type_owns`,
  `sema_refuse_owned_copy` and `sema_bind_value` stand in
  `sema_stmt.c:740` to `810`. `sema_literal_moves`,
  `sema_refuse_caller_value` and `sema_move_into_literal` stand in
  `sema_expr.c:426` to `474`. `sema_move_local` stands in
  `sema_call.c:1292`.
- `sema_thread_safe` stands in `sema_stmt.c:3077`, although
  `sema_safety.c` holds the checks of thread-safe classes.

So `sema_stmt.c` cannot be described in one sentence, and the comment
of `sema_checker.h:4` no longer says what each file holds. In lowering,
`lower_stmt.c` makes 73 calls into `lower_expr.c`, and 15 go back. The teardown of an
array is split between `lower.c:2477` to `2687` and `lower_stmt.c:1082`
to `1208`. The fix is a file for the value rules of the checker and one
for the teardown of lowering, which also settles the minor finding on
the header sections below.

## Minor

### A tenth of the shared declarations stand under the wrong file

`src/antic/sema_checker.h:376`, the splits of `sema.c` and `lower.c`,
minor, new.

Both headers group their declarations under a comment that names a file,
as `/* sema_stmt.c */`. `data/header-sections.txt` lists 35 of 346 that
the named file does not define: 21 in `sema_checker.h` and 14 in
`lower_lowerer.h`. `sema_move_local` at line 377 stands under
`sema_stmt.c` and is defined in `sema_call.c:1292`.
`lower_destroy_owned` at `lower_lowerer.h:543` stands under
`lower_stmt.c` and is defined in `lower.c:2477`.

### The library reader exports its primitives through 29 wrappers

`src/antic/antl.c:3631`, the splits of `sema.c` and `lower.c`, minor,
new.

`antl_tree.c` reads and writes the generic trees with the primitives of
`antl.c`. Those stay `static`, and `antl.c:3631` to `3743` wraps each in
a one-line exported function, `antl_put_u8` calling `put_u8`. A file of
the primitives that both include would remove the wrappers, over 100
lines of `antl.c`, which is past 3000.

### Two small crossings of shared helpers

`src/antic/notice.h:6`, one-way dependencies, minor, new.

`notice.h` includes `sema.h` of the checker for `struct package`, and
`userdirs.c:6` includes `linker.h` of the back end for `RUNTIME_LIB_DIR`.
Both are helpers every part may call. The package record and the path
constant would sit better in a header of their own. No other include
runs against the direction: the runtime includes nothing of antic or
anti, and the back end includes nothing of the front end.

### One type rule and one place rule written twice in the checker

`src/antic/lower_eq.c:375`, rule 26, minor, new.

- `lower_unreadable` at `lower_eq.c:375` and `holds_union` at
  `sema_hash.c:357` are the same 29 lines: whether a type holds a union
  or a `Match` in place. The checker names the field that blocks the
  default `==` of a class with one. Lowering leaves the same field out of
  the comparison with the other. They agree today, and one copy in
  `types.c` would keep them so.
- `same_object` at `sema_safety.c:234` and `same_mutex` at
  `sema_stmt.c:1863` compare two places the same way, each after its
  own copy of the loop that removes `&` and `*`.

### Loops over array elements written three times in lowering

`src/antic/lower.c:2687`, rule 26, minor, new.

The loop over the elements of an array from the last down stands in
`each_element` at `lower.c:2687`, `destroy_array` at `lower_stmt.c:1167`
and `lower_clear_tables` at `lower_stmt.c:1208`, 12 lines the same in
each. `element_count` at `lower_stmt.c:1118` and at `lower_hash.c:185`
count the elements the same way, and `lower_array_count` at
`lower_stmt.c:1134` exports the first.

### The two back ends still share blocks

`src/antic/arm64.c:1779`, rule 26, minor, still open.

The first audit listed the duplicates at `x86_64.c:364`. The tool now
finds exact blocks of 10 to 15 lines in `emit_call` at `arm64.c:1779`,
`1795` and `1887` against `x86_64.c:1299`, `1316` and `1422`, in
`address_of`, `memory` and `copy_memory`. The pattern tables at
`arm64.c:2017` and `x86_64.c:2355` also match, but a table per target
is the shape of the problem.

### The place functions of the sequence collections repeat

`src/std/anti/collection/list.anti:236`, duplicated code, minor, new.

`first_place`, `next_place`, `lend_place`, `copy_place` and `take_place`
are written out in `list.anti`, `deque.anti`, `ring.anti`, `grid.anti`,
`queue.anti` and `Copies` of `collection.anti:584`. The first two are
the same in each. `Copies.lend_place` returns quietly without room,
where `List` stops with `catch fatal`. A base class for the collections
indexed from 0 would hold them once. `data/duplication.txt` also lists a
block of 10 lines in `concurrent.anti:1242` and `synchronized.anti:323`.

### The format attribute is written raw and missing on nine functions

`src/antic/diagnostic.h:60`, rule 1, minor, still open.

`attributes.h` now holds `ATTRIBUTE_PRINTF` for antic. The raw
`__attribute__((format(printf, ...)))` still stands in `diagnostic.h:60`,
`66`, `73` and `81`, `antl.c:1120` and `src/anti/bindmodel.h:144`.
`__attribute__((weak))` stands in `src/rt/trace.c:140` and `init.c:31`,
outside a platform file. `-Wmissing-format-attribute` names nine
printf-like functions without the attribute. They are `add` of
`diagnostic.c:31`, `vformat_to` of `sema.c:34`, `refuse` of
`bindclang.c:290`, and four in `src/rt/assert.c:10` to `40`, the new
failure routines among them. `format_text` of `conf.c:120` and `fail`
of `plugin.c:57` complete the nine. The runtime has no macro of its own.

### A simd struct of sixteen `bool` lanes is written as `uint64x2_t`

`src/antic/header.c:619`, rule 26, minor, still open.

`const char *neon = "uint8x16_t";` is overwritten on every path, and
`TYPE_BOOL` still falls to `default` at line 634. The register and the
ABI agree, so the defect is the name in the C header.

### Functions past a threshold

Rule 18, minor, still open. `data/thresholds.txt` lists 163 functions:
93 past 100 lines, 31 nested deeper than 4 and 56 taking more than 6
parameters. `data/thresholds-compared.txt` marks 47 new. The judgements
of the first audit hold for the 116 kept. The groups of the new ones:

- Dispatch over node kinds, which follows the shape of the problem. They
  are `xe` (229 lines) and `xs` (185, nest 5) of `sema_copies.c`, and
  `io_expr_body` (195) and `io_stmt_body` of `antl_tree.c`. So are
  `put_value` of `src/rt/object.c:488` (141) and `print_type` of
  `types.c`.
- Rules in sequence, which follow the shape of the problem.
  `sema_require` at `sema_expr.c:818` (117) tries each implicit
  conversion in turn under its own comment. `check_unary` (124) is a
  `switch` over the operators. `check_assign` (120) and `patch_call` of
  `sema_pattern.c` (166) were judged from their counts alone.
- Of the kept ones, `sema_eval_const` at `sema_const.c:330` (520, nest 6)
  and `statement_level` at `parser.c:2346` (403) grew by 38 and 26 lines.
  The first audit's judgement of each stands.
- Parameter lists that are an ABI. `anti_rt_regex_patch` (10),
  `anti_rt_bytes_patch` (8) and four more of `src/rt/patterns.c` match
  the `extern fn` lines of `src/std/anti/regex.anti` and stay.
- Parameter lists that carry one shape. `read_level` of `registry.c:548`
  (9) and `put_level` of `object.c:420` (8) pass the description of an
  array one field at a time, and a struct would be clearer. Lowering adds
  `each_element`, `each_case`, `compare_member`, `field_record` and
  three more with 7 each, judged from their counts alone.
- Deep nests. `walk_class` of `src/rt/regex.c:306` (nest 6) scans a
  character class with its escapes and ranges, and the nest follows that
  grammar. `postfix` of `parser.c:1486` (6), `angle_list` of
  `src/anti/fmt.c:379` and `reserve_borrow_slots` of `regalloc.c:781`
  (5) were judged from their counts alone.

### Files past 3000 lines

Rule 19, minor, still open. `data/file-lengths-compared.txt` lists 9,
and `x86_64.c` left the list at 2950 lines.

- `parser.c`, 4340 lines, up from 2975. Types to line 855, expressions
  to 1790, statements to 2808, generics and members to 3668, and items
  to the end are each a file's worth.
- `antl.c`, 3788. The writer to line 1123 and the reader from 1124, and
  the wrappers above.
- `sema.c`, `sema_expr.c`, `sema_call.c`, `sema_stmt.c`, `lower.c` and
  `lower_expr.c`, 3199 to 3752 each. The major finding above names the
  parts that would come out first.
- `driver.c`, 3017. The planning of the link, `xcrun` at line 245 to
  `link_program` at 806, belongs beside `linker.c`.

## False positives

Each class once, with the reason.

- `core.NullDereference` in `sema.c:110`, `2928`, `2974`, `3073` and
  `3392`, and `types.c:1453`. Each path needs `type_has_fields(NULL)`,
  which returns false at `types.c:1635`.
- `core.NullDereference` in `sema.c:559`, `2852`, `sema_call.c:272`,
  `965` and `1041`, `sema_stmt.c:397`, `sema_pattern.c:295` and `337`,
  and `ast_dump.c:52`. Each is the first audit's class, moved, or needs a
  NULL the parser or `arena_alloc` never gives.
- `core.NullDereference` in `lower_desc.c:497`, `lower_expr.c:2990` and
  `lower_stmt.c:2342`. Every array type has an element, `e->type` is read
  first at `lower.c:131`, and `falls[k]` is set with `tail[k]`.
- `core.NullDereference` in `antl_tree.c:1543`, `1548` and `1623`. The
  path needs `failed` to turn false again, and only a `memset` at the
  start clears it.
- `core.DivideZero` in `layout.c:311` and `sema_expr.c:2312`. A bitfield
  is an integer of at least 8 bits, and a struct has a field of at least
  1 byte.
- `core.BitwiseShift` in `arith.c:18` and `106`, and `layout.c:604`. The
  width is 8, 16, 32 or 64, and a bitfield is at most 64 bits.
- `unix.Malloc` in `optimize.c:78` and `core.NullDereference` in
  `optimize.c:858`, `x86_64.c:1113` and `1215`, and `parser.c:260`. The
  first audit's reasons hold at the moved lines.
- `src/rt/object.c:432` and `registry.c:830`. Both need a negative field
  count, and antic writes counts from a `size_t`.
- `src/rt/fs.c:235`, `start.c:128`, `src/anti/jsontree.c:148` and
  `doc.c:645`. The first three are the first audit's, moved, and
  `sema_strip_generics` stores no NULL item for the fourth.
- `-Wmissing-prototypes`, 58 functions of 6 runtime files. Generated
  code of `lower_expr.c` and `lower_stmt.c` calls each by name, or an
  `extern fn` of `src/std/anti/` declares it.
- `-Wcast-align` in `src/rt/object.c:1005`, `1087` and `1088`, and
  `-Wformat-nonliteral` in `sema.c:296`, the first audit's lines moved.
  Every caller of `sema_declare` passes one of two literal formats.
- `data/duplication.txt` also pairs `src/anti/bindtype.c:416` with
  `src/antic/main.c:175`, and `lexer.c:16` with `warnings.c:9`. They are
  chains of `strcmp` and tables of names that match once names are
  replaced.
