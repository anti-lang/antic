# Summary of the second audit

This document merges the nine reports of the second audit under
`docs/audit/` and orders the work they found. The reports are
`tool-pass`, `structure`, `front-end`, `back-end`, `rt`, `anti-tool`,
`input-readers`, `std` and `tests`, each named here by the stem of its
file. They audited `238a4f1`, and the tool pass `70e8860`, whose `src/`,
`tests/` and `tools/` are the same. `git diff` shows no change under
`src/`, `tests/`, `tools/` or `CMakeLists.txt` from `238a4f1` to this
commit, so every path:line still points at the code the reports read.
The line of every severe and every major finding was read again for
this summary. One place moved from the report's line to the line given
here, `sema_checker.h:67`. A finding
that two or more reports list stands here once, with every report that
lists it. Where reports grade one finding differently, the summary
applies `docs/c-guidelines.md` and the severities of the structure
standard, and says which grade it took. The comparison with the first
audit uses `docs/audit/2026-09-23/summary.md` and its ids, written with
one digit, as S3 or M29. This summary numbers its own findings with two
digits, S01 to S34 and M01 to M62.

## Counts

The nine reports list 214 findings: 43 severe, 71 major and 100 minor.
Merged, they are 187.

| Report | Severe | Major | Minor |
|---|---|---|---|
| tool-pass | 3 | 4 | 11 |
| structure | 0 | 3 | 5 |
| front-end | 15 | 8 | 15 |
| back-end | 9 | 19 | 17 |
| rt | 5 | 4 | 12 |
| anti-tool | 3 | 9 | 10 |
| input-readers | 8 | 9 | 4 |
| std | 0 | 6 | 15 |
| tests | 0 | 9 | 11 |
| Listed | 43 | 71 | 100 |
| Merged | 34 | 62 | 91 |

| Status against the first audit | Severe | Major | Minor |
|---|---|---|---|
| New | 28 | 59 | 67 |
| Returned | 1 | 0 | 0 |
| Still open, in whole or in part | 5 | 3 | 24 |

The merged findings by rule or structure question. Q1 to Q5 are the
structure questions. Q1 is one purpose per module and file, Q2 one-way
dependencies and Q3 each concern in one place. Q4 is the splits of
`sema.c` and `lower.c`, and Q5 copies that drifted. The rows under "Tests" are the
questions of the `tests` report.

| Rule or question | Severe | Major | Minor |
|---|---|---|---|
| Q1 | 0 | 1 | 3 |
| Q2 | 1 | 3 | 2 |
| Q3 | 2 | 5 | 10 |
| Q4 | 0 | 2 | 2 |
| Q5 | 0 | 18 | 6 |
| The IR free of sizes | 0 | 0 | 1 |
| Names and signatures of the library | 0 | 0 | 5 |
| 1 | 0 | 0 | 3 |
| 3 | 3 | 2 | 4 |
| 4 | 1 | 0 | 1 |
| 5 | 2 | 0 | 3 |
| 6 | 1 | 0 | 2 |
| 9 | 0 | 0 | 2 |
| 11 | 0 | 0 | 2 |
| 12 | 0 | 1 | 1 |
| 13 | 0 | 0 | 1 |
| 14 | 17 | 10 | 4 |
| 15 | 0 | 1 | 0 |
| 16 | 1 | 0 | 3 |
| 17 | 1 | 0 | 0 |
| 18 | 0 | 0 | 1, covering 163 functions |
| 19 | 0 | 0 | 1, covering 9 files |
| 20 | 0 | 0 | 3 |
| 22 | 0 | 3 | 0 |
| 23 | 1 | 1 | 1 |
| 24 | 0 | 0 | 3 |
| 25 | 0 | 0 | 3 |
| 26 | 0 | 0 | 9 |
| 27 | 0 | 0 | 3 |
| Warnings | 0 | 0 | 1 |
| None, wrong code, output or a leak | 4 | 6 | 1 |
| Tests: pass whatever the code does | 0 | 1 | 1 |
| Tests: leak checks count teardowns | 0 | 1 | 0 |
| Tests: uncovered features | 0 | 3 | 1 |
| Tests: duplicated runner code | 0 | 4 | 3 |
| Tests: layout, names and dead code | 0 | 0 | 5 |
| Total | 34 | 62 | 91 |

### Merges and grades the reports disagree on

- The generic trees of a library file are one finding, S01. The tool
  pass found it, `front-end` and `input-readers` added places, and
  `back-end` added `l->loop` at `lower_stmt.c:2389` and
  `tuple->fields[i]` at `lower_stmt.c:545`. Its two severe entries in
  `input-readers` stand in it.
- The Mutex word of `fixed_layout` is a severe finding of its own in
  `back-end` and a second case of the unit break of the tool pass. One
  finding here, S02, with one fix.
- The recursion the depth limit does not reach is still open (S3) in
  `front-end` and new in `input-readers`. Still open here, since the fix
  of S3 promised one limit for the whole tree.
- The cubic scan of `<` is part of a severe finding in `front-end` and a
  major one in `input-readers`. A hang is not severe by the document, as
  `front-end` itself says, so the time is M37 and the stack overflow of
  the same scan stays in S05.
- The overflow of the patch offset is rule 5 in `rt` and rule 3 in
  `input-readers`. Rule 5 here, since the sum reaches an index.
- The place functions of the sequences are minor in the tool pass and
  major in `std`, which found two copies that drifted. Major here, M27.
- The out-of-memory exits of `antic` are minor in `structure` and
  `back-end`. The allocator they stand for is major in the tool pass, as
  a dependency that runs the wrong way. Both stay: M03 major, the copies
  minor.
- The count table of `rt` gives 13 minor findings, and the report holds
  12. The summary counts 12.
- The rematerialisation case, M53, is a case the fix of M35 missed.
  `back-end` grades it new, and so does this summary.

## Severe

Every severe finding, by area. "Reports" names each report that lists
it.

### antic library reader

| ID | Place | Rule | Finding | Status | Reports |
|---|---|---|---|---|---|
| S01 | `src/antic/antl_tree.c:1258` | 14 | The generic trees reach the copy pass and lowering unchecked. `otherwise_at` indexes `entry[k]` at `lower_stmt.c:2331`, `variant_case` and `enum_value` at lines 756, 1045, 1066 and 1076 index fields, reference 0 is NULL in any slot, a node may name itself so that `xs` recurses without end, and `param_count` and `copy_count` are not compared with the type. | new | tool-pass, front-end, input-readers, back-end |
| S03 | `src/antic/layout.c:96` | 14 | A library simd aggregate whose lane has no bytes divides by zero in `round_up`. `ir_lane` and the `simd` flag at `antl.c:2115` check no lane type. | still open (S1) | tool-pass, front-end, back-end |
| S13 | `src/antic/antl_tree.c:1155` | 17, 14 | The writer stores the lane count of a shuffle as `(uint8_t)`, so 256 lanes write 0, and lowering reads `field_count` lanes at `lower_simd.c:469`. | new | front-end |
| S23 | `src/antic/layout.c:882` | 14 | A library constant whose scalar is `IR_AGG` makes `layout_size` read `agg_state[UINT32_MAX]`. | new | back-end |
| S24 | `src/antic/arm64.c:1063` | 14 | `read_tables` does not apply the checker's simd rules, so `fold_registers[level + 1]` reads past its 7 entries and `movups` moves 16 bytes of a 12-byte value. | new | back-end |

### antic front end

| ID | Place | Rule | Finding | Status | Reports |
|---|---|---|---|---|---|
| S02 | `src/antic/sema_expr.c:2258` | Q3, one layout rule | `fixed_layout` counts the unit break `_` as 4 bytes, and a Mutex word as 4 at line 2316, where `layout.c` gives `_` none and a Windows lock 8. `as` to a simd struct then copies bytes from outside the value, reproduced in `data/probe-unit-break-cast.txt`. | new | tool-pass, back-end |
| S04 | `src/antic/sema_checker.h:67` | Q3, one owner for shared state | No function owns the context fields of `struct checker`. Hand-written saves differ: a constant first met in a signature caches a value built on a type parameter, a quiet probe caches an error type with no message, and `try` at `sema_stmt.c:2283` loses the outer error type. | new, the `try` part still open | front-end |
| S05 | `src/antic/parser.c:3272` | 14 | Recursion the depth limit does not reach: nested types in a class body, the scan of `<` at `parser.c:551`, nested `f"..."` at `lexer.c:1244`, and left-nested chains that later walks recurse on. | still open (S3) | front-end, input-readers |
| S06 | `src/antic/types.c:1412` | 14 | Chains of struct and alias types recurse once per link in `types_find_cycle`, `sema_alias_type` and the walks after them. | still open (S4) | front-end |
| S07 | `src/antic/sema.c:2749` | 14 | Copies of generics made after the cycle pass are never checked: a pair of structs overflows `type_pointer_free`, and `class A<T> inherits A<int>` loops in `check_implements`. | returned (S5, S6, M2) | front-end |
| S08 | `src/antic/parser.c:981` | 14 | `placeholder` parses with `inner.origin = NULL;`, and `for` or `assert` in a closure inside `f"..."` reads through it. | new | front-end, input-readers |
| S09 | `src/antic/sema_call.c:3022` | 14 | `E.A` reached through a type name keeps an untyped base, and `&E.A`, `1 + E.carry` and `E.A = 1` read the NULL type. | new | front-end |
| S10 | `src/antic/sema.c:2356` | 14 | The base of an enum is not tested. `enum A: A { X }` gives a NULL base, which `lower.c:31` reads. | new | front-end |
| S11 | `src/antic/sema_generic.c:220` | 14 | A refused second generic class of one name keeps a NULL symbol, and its nested type reads `outer->symbol->type`. | new | front-end |
| S12 | `src/antic/sema_expr.c:37` | 14, 3 | `&v.area` of a bound function passes as a place, and `lower.c:847` subtracts from a NULL field pointer. | new | front-end |
| S14 | `src/antic/sema_pattern.c:294` | 14 | A function of the `anti.regex` library file is indexed at the position the call needs, with no comparison against `param_count`. | new | front-end |
| S15 | `src/antic/ast_dump.c:430` | 14 | `--dump-ast` of `alloc P { x: 1 }` passes a NULL type and count to `dump_type` and `dump_expr`. | new | front-end |
| S16 | `src/antic/ast_dump.c:52` | 3 | `for 0..3 { }` binds no name, and its NULL pointer reaches `%.*s`. | new | front-end |

### antic back end

| ID | Place | Rule | Finding | Status | Reports |
|---|---|---|---|---|---|
| S17 | `src/antic/optimize.c:1072` | None, wrong code | Store forwarding keeps the held address after a load whose result is its own base, so `p = p.link; return p.link.v;` loses a load. Reproduced. | still open (S17) | back-end |
| S18 | `src/antic/optimize.c:1217` | None, wrong code | Scalar replacement splits a slot whose `ptradd` result has a second definition, and `*q` reads the wrong slot. Reproduced. | new | back-end |
| S19 | `src/antic/whole.c:240` | None, wrong code | Release devirtualisation calls a host class's function on a plugin's object. Reproduced in the IR. | new | back-end |
| S20 | `src/antic/lower_stmt.c:743` | None, wrong code | The counter of `for ... by k` wraps at the end of its type: `0 as u8..255 by 10` never stops, and a downward `i8` range runs zero times. Reproduced. | new | back-end |
| S21 | `src/antic/regalloc.c:993` | 5 | The frame size sums its slots without a check, wraps, and the locals overlap. ARM64 has no frame limit. Reproduced. | new | back-end |
| S22 | `src/antic/ir_print.c:484` | 3 | The printer passes the NULL module of a runtime global to `%s`, and 19 test files pin the `(null)`. `ir_verify.c:35` and `38` do the same. | new | back-end |

### Runtime

| ID | Place | Rule | Finding | Status | Reports |
|---|---|---|---|---|---|
| S25 | `src/rt/patterns.c:753` | 5, 3 | `at + with_length` overflows for an offset near `INT64_MAX`, and `memcpy` then writes outside the text. `patterns.c:802` has the same sum. | new | rt, input-readers |
| S26 | `src/rt/patterns.c:547` | 6 | `anti_rt_regex_group` bounds `n` by `m->count`, a `pub` field the program may set, and reads the ovector past its groups. | new | rt |
| S27 | `src/rt/trace.c:367` | 14 | `symbolize` on macOS walks a Mach-O header at `frame->base`, which `deserialize` of a `StackTrace` takes from text. Linux opens the `module` the text names. | new | rt |
| S28 | `src/rt/plugin.c:597` | 23 | `instance` gives the lock back before `build` calls into the library, so `unload` on another thread may close it first. `plugin.c:616` and `880` read after the lock too. | still open in part (S34) | rt |
| S29 | `src/rt/hooks.c:79` | Q2, a boundary crossed | The guard of `unload` counts objects in the `created` and `destroyed` hooks alone, which `--no-hooks` removes at `lower.c:936`. `unload` then closes a library whose objects are alive. | new | rt |
| S33 | `src/rt/plugin.c:616` | 14 | `class_sound` checks no function or field list of a plugin class, and `supports` and `field_named` at `registry.c:330` walk them by the library's counts. | new | input-readers |

### anti tool

| ID | Place | Rule | Finding | Status | Reports |
|---|---|---|---|---|---|
| S30 | `src/anti/files.c:38` | 4 | `files_array` and `files_resize` return `calloc` and `realloc` unchecked, and `files_grow` writes through NULL at line 65. The 63 callers trust `files.h:13`. `d20c08a` introduced it. | new | anti-tool, input-readers |
| S31 | `src/anti/bindclang.c:1165` | 16 | The lines of `clang -E` are counted in `long`, which overflows on Windows after `#line 2147483647`. | new | anti-tool |
| S32 | `src/anti/bindclang.c:794` | 14 | An enumerator after `INT64_MAX` adds 1 in `int64_t`. | new | anti-tool |
| S34 | `src/anti/repo.c:248` | 3 | `now - then` overflows for a negative stamp beside a cached index. | new | input-readers |

## Major

Grouped by question or rule. Each entry gives its place, its status and
its reports.

### Q1, one purpose per module

- M01. `src/std/anti/collection.anti:708`. The module still holds the
  `List`, `Map` and `IntMap` over objects from before generics, which
  have drifted from the hash of the runtime and ignore the seed. New.
  std.

### Q2, one-way dependencies

- M02. `src/anti/doc.c:1440`. `anti` uses the internals of antic: 11
  files include the lexer, parser, checker, IR, library reader or
  linker, and `doc.c` owns IR modules. No header is its interface. New.
  tool-pass, anti-tool, structure.
- M03. `src/antic/ir.h:477`. The checked allocator of antic stands in
  the IR, and `header.c:8` includes `ir.h` for it. New. tool-pass,
  structure.
- M04. `src/antic/lower_desc.c:205`. The ABI between generated code and
  the runtime is written twice: 25 runtime functions that lowering calls
  have no prototype in `src/rt/`, and the descriptor and field records
  stand in `lower_desc.c` and `object.h` with nothing tying them. New.
  structure.

### Q3, each concern in one place

- M05. `src/antic/lower_stmt.c:1110`. Two teardown families with two
  gates. `lower_needs_teardown` is false for `[2]?C`, `?[2]C` and
  `[N]own fn`, so such a local is never torn down, and field and struct
  orders differ. New. structure, back-end.
- M06. `src/antic/whole.c:2390`. `lower_desc.c` writes and `whole.c`
  reads the descriptor by bare item numbers. New. back-end.
- M07. `src/antic/header.c:158`. Names in the C header collide: nested
  tuples, table wrappers against the helpers, and a generic variant
  written raw. New. back-end.
- M08. `src/std/anti/collection/map.anti:57`. `struct Record` copies
  `struct anti_field` by hand, where `sorted.anti` reads
  `FieldDescriptor`, and the writer of an entry stands three times. New.
  std.
- M09. `src/std/anti/log.anti:476`. `anti.log` opens, reads and writes
  files through its own `fopen`, which reads a sliced path as a C
  string. New. std.

### Q4, the splits of `sema.c` and `lower.c`

- M10. `src/antic/sema_checker.h:4`. The checker files reach into each
  other, and `sema.c`, `sema_stmt.c`, `sema_expr.c`, `sema_call.c` and
  `sema_export.c` each hold more than one purpose. New. tool-pass, front-end,
  structure.
- M11. `src/antic/lower.c:1`. `lower.c`, `lower_expr.c`, `lower_stmt.c`
  and `lower_desc.c` each hold more than one purpose, and `lower_expr.c`
  makes 538 calls into `lower.c`. New. tool-pass, back-end.

### Q5, copies that drifted

- M12. `src/antic/sema_export.c:531`. Checker rules written twice: the
  declared names, the lookup of a library function, `copy_in_chain` and
  `copy_name`, a field by name, and the walks of the tree. New.
  front-end.
- M13. `src/antic/lower_eq.c:306`. The default `==` and hash disagree on
  a union with `eq`, on `f16` zero, on closures and on a Mutex. New.
  back-end.
- M14. `src/antic/lower.c:2197`. Class defaults are written four times,
  and the singleton's writes bitfields at the wrong place. New.
  back-end.
- M15. `src/antic/arm64.c:680`. `emit_imm12` drops the low bits of a
  large argument offset, unguarded here and at line 655. New. back-end.
- M16. `src/antic/whole.c:617`. The optimizer and `reach_program` decide
  what a program reaches by different rules. New. back-end.
- M17. `src/antic/driver.c:2780`. The link inputs of a shared library
  miss the frameworks, Linux libraries, glibc and memory checks. New.
  back-end.
- M18. `src/antic/layout.c:424`. `fold_op` folds a shift count modulo 64,
  and `optimize.c:201` does not fold it. The result differs between
  Linux and Windows. New. back-end.
- M19. `src/antic/header.c:937`. The header and lowering order a class
  table apart, so C reads later slots one off. New. back-end.
- M20. `src/antic/antl_tree.c:280`. The tree reader grew primitives of
  its own, each without a check the table reader has. New.
  input-readers.
- M21. `src/antic/driver.c:2664`. antic reads the plugin index with
  `strstr`, and the runtime with TOML. New. input-readers.
- M22. `src/rt/registry.c:386`. `Object.deserialize` converts a JSON
  integer with `strtoll` and accepts `+5` and `007`. New. input-readers.
- M23. `src/anti/build.c:177`. The package name reaches `anti check` and
  one compile of `anti build`, and no compile of `anti test` or
  `anti doc`. New. anti-tool.
- M24. `src/anti/symmap.c:146`. Two readers of the build id, and the
  marker spelled five times. New. anti-tool.
- M25. `src/anti/syms.c:312`. `anti symbols` copies the runtime's walk
  of configuration includes and stops one file short of it. New.
  anti-tool, input-readers.
- M26. `src/anti/bind.c:44`. `anti bind --header` splits a path at `/`
  alone. New. anti-tool.
- M27. `src/std/anti/collection.anti:600`. The sequences repeat their
  places, walks, copies and teardowns, and `Copies` has drifted from
  `List`. New. std, tool-pass.
- M28. `src/std/anti/collection/concurrent.anti:630`. `ConcurrentMap`
  hashes unlike `HashMap` with the same entries. New. std.
- M29. `src/std/anti/json.anti:146`. `json.unquote` reads escapes unlike
  the runtime and unlike `write_text`. New. std.

### Rule 22, one platform layer

- M30. `src/rt/platform.h:20`. 110 host `#if` lines stand outside the
  platform layer: 81 in the runtime, 18 in antic, 11 in `anti`, which
  has no `platform.c`. Six runtime files keep locks of their own. Still
  open (M29). tool-pass, rt, anti-tool.
- M31. `src/antic/selfpath.c:100`. The tools hold paths as UTF-8 and
  pass them to the ANSI entry points of Windows. New. structure.
- M32. `src/rt/errno.c:52`. `FormatMessageA` gives the text of a system
  error in the ANSI code page. New. rt.

### Rule 14, untrusted input

- M33. `src/antic/antl_tree.c:688`. The counts of a tree are not
  measured against the file. New. front-end, input-readers.
- M34. `src/antic/sema_generic.c:1092`. Copies of generics have no bound
  on their number. New. front-end.
- M35. `src/antic/sema_call.c:2605`. A checked subtree is checked again,
  doubling per link, and `-(-a)` is refused. New. front-end.
- M36. `src/antic/sema_call.c:445`. Walks over types and imports keep no
  record of what they answered. New. front-end.
- M37. `src/antic/parser.c:88`. The scan of `<` takes cubic time. New.
  input-readers, front-end.
- M38. `src/rt/registry.c:409`. `Object.deserialize` makes a `str` with
  invalid UTF-8 or a NUL. New. rt.
- M39. `src/anti/fmt.c:457`. The angle lists of `anti fmt` recurse
  without a bound. New. anti-tool.
- M40. `src/anti/doc.c:917`. `anti doc` writes any URL scheme of a doc
  link. New. anti-tool.
- M41. `src/anti/zip.c:232`. The unpacked total of an archive has no
  bound. New. input-readers.
- M42. `src/anti/syms.c:1113`. A path the archive names is read whole.
  New. input-readers.

### Rule 15, malformed-input tests

- M43. `tests/unit/test_modules.c:1417`. Readers without a malformed-
  input test: the generic trees, the new parser paths, the variant, `?T`
  and cut text cases of `Object.deserialize`, the lists of a plugin
  class, the plugin index, a configuration that is no TOML, the Mach-O
  symbol table, the regex template, `read_source` with a NUL, the
  command-line splitter, overlapping zip entries, and a truncated COFF
  object and archive index. Still open (M31). front-end, back-end,
  input-readers.

### Rule 3, behaviour the implementation defines

- M44. `src/antic/sema_pattern.c:335`. A group of `patch` is read as
  `long`, so Windows takes group 1 where the others refuse. New.
  front-end.
- M45. `src/antic/lower_expr.c:388`. `ir_store` reads `l->b` beside a
  call that ends the block, and the Mac fails verification. New.
  back-end.

### Rules 12 and 23

- M46. `src/anti/fmt.c:1491`, rule 12. `anti fmt` leaks the brackets of
  an anonymous function left open. New. anti-tool.
- M47. `src/rt/plugin.c:420`, rule 23. The loader asks the system for an
  image while it holds its own lock, a deadlock on Windows. New. rt.

### No rule, wrong code, output or a leak

- M48. `src/antic/lower.c:2832`. An `own` pointer to an owning struct
  frees its buffer alone, and a copy shares what it owns. New. back-end.
- M49. `src/antic/lower_stmt.c:1748`. `alloc` writes through the result
  of `malloc` unchecked. New. back-end.
- M50. `src/antic/arm64.c:1977`. An `f32` offset gets the scale of 8,
  and llvm-mc refuses the load. New. back-end.
- M51. `src/antic/header.c:716`. A class field's C type is not declared
  before the class. New. back-end.
- M52. `src/antic/header.c:1259`. An infinite float constant is written
  `inf.0`. Still open (M32). back-end.
- M53. `src/antic/regalloc.c:420`. Rematerialisation drops a constant
  that a memory operand reads. New. back-end.

### Tests

- M54. `tests/CMakeLists.txt:1743`. Refusal tests pass on their output
  and ignore the exit status, at 20 registrations. New. tests.
- M55. `tests/programs/owning_values.anti:24`. Leak checks count the
  leaves and not the teardowns of the keys, in four files. New. tests.
- M56. `tests/errors/link_linux.anti:1`. The refusals of `link linux`
  are never registered. New. tests.
- M57. `src/antic/sema.c:3028`. A replacement of a `final fn` is neither
  refused nor tested, against `docs/anti-object-model.md:235`. New.
  tests.
- M58. `src/rt/plugin.c:559`. Two refusals of the plugin loader are
  never triggered. New. tests.
- M59. `tests/run_modules.cmake:71`. Scripts spell the object suffix
  `.o`, and 16 cross-module dev tests skip Windows. New. tests.
- M60. `tests/run_pcre2_pin.cmake:46`. Three of the five pin checks
  match the version as a pattern. New. tests.
- M61. `tests/run_table.cmake:19`. Five runners of stopping programs
  disagree on the output of antic and on exit codes. New. tests.
- M62. `tests/run_plugin.cmake:25`. Scripts compare program output
  without the raw-bytes rule. New. tests.

## Minor

Counted by rule or question, with the place each report opens the
finding at. Paths without a directory are in `src/antic/`.

| Rule or question | Count | Findings and reports |
|---|---|---|
| Q1 | 3 | Second purposes of four `anti` files, `src/anti/syms.c:1251` (anti-tool). Modules doing the work of `anti.text`, `src/std/anti/json.anti:59`, and of `anti.runtime`, `src/std/anti/log.anti:28` (std). |
| Q2 | 2 | Two small crossings, `notice.h:6` (tool-pass). Placement and includes of the back end, `mach.c:9` (back-end). |
| Q3 | 10 | A generic copy found by `<`, `whole.c:281`, the NUL rule of a source, `driver.c:114`, names spelled twice, `src/anti/syms.c:32`, and the path rules, `selfpath.c:119` (structure). Shared helpers misplaced, `sema_stmt.c:27` (front-end). The glue's header list, `src/native/pcre2.cmake:71`, and the layout of `Match`, `src/rt/patterns.c:126` (rt). The alignment of a room five times, `src/std/anti/collection/map.anti:1595`, and shared parts in sibling modules, `synchronized.anti:29` and `map.anti:1398` (std). |
| Q4 | 2 | Declarations under the wrong file, `sema_checker.h:376`, and the 29 wrappers of `antl.c:3631` (tool-pass). |
| Q5 | 6 | A version compared two ways, `src/rt/plugin.c:129` (input-readers). Matching loops, `SpscRing`, the hash of `SyncList`, the text of the base and `int_of` twice, from `src/std/anti/collection.anti:453` (std). |
| The IR free of sizes | 1 | `lower.c:1534` (back-end). |
| Names and signatures of the library | 5 | Size and presence, "not present", `new`, set operations and the byte forms of `anti.regex`, from `src/std/anti/text.anti:141` (std). |
| 1 | 3 | The format attribute, `diagnostic.h:60` and `src/anti/bindmodel.h:143` (tool-pass, anti-tool). Extensions outside their files, `src/rt/lock.c:63` (rt). `-isystem` for the PCRE2 glue, `CMakeLists.txt:124` (rt). |
| 3 | 4 | Tree reader conversions, `antl_tree.c:247` (front-end, input-readers). Back end conversions, `x86_64.c:688` (back-end). `char` in the Windows atomics, `src/rt/atomic.c:60`, and the status of `main`, `src/rt/start.c:213` (rt). |
| 4 | 1 | Unit test allocations, `tests/unit/test_utf.c:11` (tests). |
| 5 | 3 | `arena.c:31` (front-end), `ir.c:644` (back-end), `src/anti/jsontree.c:82` (anti-tool). |
| 6 | 2 | `ast_dump.c:443` (front-end), `regalloc.c:664` (back-end). |
| 9 | 2 | `diagnostic.c:31` (front-end), `lower.c:450` (back-end). |
| 11 | 2 | `antl_io.h` (front-end), `src/rt/regex.h:29` (rt). |
| 12 | 1 | `src/anti/check.c:474` (anti-tool). |
| 13 | 1 | `src/rt/text.c:133` (rt). |
| 14 | 4 | Smaller reader gaps of the front end, from `antl.c:3518` (front-end). `mark_function` and COFF section numbers, `optimize.c:1361` (back-end). API names into the binding, `src/anti/bindwrite.c:263` (anti-tool). Configuration includes that fan out, `src/rt/conf.c:700` (input-readers). |
| 16 | 3 | `sema_call.c:3392` (front-end), `ir.c:398` (back-end), `src/rt/plugin.c:791` (input-readers). |
| 18 | 1 | 163 functions in `docs/audit/data/thresholds.txt`, judged in tool-pass, front-end, back-end, rt and anti-tool. |
| 19 | 1 | 9 files past 3000 lines, `parser.c` with 4340 the longest (tool-pass, front-end, back-end). |
| 20 | 3 | `sema.c:2223` (front-end), `memcheck.h:43` (back-end), `src/rt/object.c:218` (rt). |
| 23 | 1 | `sema_tn` of `sema.c:85` returns a static buffer kept across calls (front-end). |
| 24 | 3 | `sema.c:1927` (front-end), `memcheck.h:55` (back-end), `src/anti/test.h:21` (anti-tool). |
| 25 | 3 | Front end names, `sema.h:271` (front-end). `anti_lang_` of the root, `src/rt/object.c:65` (rt). `json_*` of `src/anti/jsontree.h:48` (anti-tool). |
| 26 | 9 | Type and place rules twice, `lower_eq.c:375`, loops over array elements, `lower.c:2687`, and the `neon` dead store, `header.c:619` (tool-pass). Out-of-memory exits, `lower.c:1542` (structure, back-end). Repeated blocks and dead code of the front end, `sema_expr.c:1683` (front-end). Repeated blocks of the back end, with the blocks the two targets share, `optimize.c:94` and `arm64.c:1779` (back-end, tool-pass). Dead code of the back end, `lower.c:3188` (back-end). Growing buffers five times, `src/rt/patterns.c:606` (rt). Small helpers of `anti`, `src/anti/deps.c:457` (anti-tool). |
| 27 | 3 | `sema_expr.c:1095` (front-end), `lower_lowerer.h:4` (back-end), `src/anti/doc.c:780` (anti-tool). |
| Warnings | 1 | `(void)` on 26 parameters without a reason, `arm64.c:406` (back-end). |
| None | 1 | The CodeView name of a function unescaped, `debug.c:440` (back-end). |
| Tests | 10 | Tests that drop out on some hosts, `tests/CMakeLists.txt:1313`. Dev objects compiled in four places, `tests/run_checks.cmake:96`. Registrations without a helper, `tests/CMakeLists.txt:479`. Copied unit test helpers, `tests/unit/test_sema.c:28`. The descriptor pinned in 28 texts, `tests/unit/test_lower.c:547`. Directories that hold more than one kind, `tests/CMakeLists.txt:320`. Test names, `tests/CMakeLists.txt:3678`. Smaller uncovered features, `main.c:265`. Dead runner code, `tests/run_start.cmake:1`. A comment above the wrong test, `tests/CMakeLists.txt:1305` (tests). |

`front-end` also lists six defects no rule names, `rt` one, `anti-tool`
four and `back-end` none. They are not counted. Two need a decision,
under "Decisions a fix step needs" below.

## Compared with the first audit

The first audit merged 141 findings: 46 severe, 35 major and 60 minor.

### Fixed

- Severe: S2, S8 to S16, S18 to S33 and S35 to S46, 38 of 46. The
  commits that fixed S17 to S19, S22 to S25, M8, M21, M28 and M35 added
  tests, as `back-end` records. The readers gained the tests the table
  of `input-readers` names.
- Major: M1, M3 to M21, M23 to M28, M30, M33 and M35, 29 of 35.
- Minor: the reports name as closed the rule 1 finding on table entries,
  the conversions of `sema.c` and of input values, `marker_length`,
  the rule 9 findings outside `diagnostic.c`, both rule 10 findings,
  the rule 11, 12, 13, 21 and 23 findings at their places, the rule 20
  finding, the mixed widths of the front end and of `zip.c`, the
  twelve file readers and writers of `anti`, the copy of SHA-256, the
  module reader of `test.c`, `main` of `anti` and the dead code of the
  old `sema.c`.

### Returned

- S5, S6 and M2, as S07: types that contain themselves and base cycles
  come back through copies of generics, which the fixes did not reach.
- Four classes the first audit closed came back in code written since,
  and are counted new. Two calls with side effects stand in one argument
  list again, M45, in code of `d489d05`. New readers recurse without a
  bound, S05, S01 and M39. The tree reader skips a check the tables
  make, S01 and M20 against S12. A helper swallows a failed allocation,
  S30, introduced by the fix of a rule 5 finding.

### Still open

- S1 for a library simd aggregate, S03. S3 for nested types, the scan
  of `<`, nested f-strings and left-nested chains, S05. S4 for chains
  of types, S06. S17 for a load whose result is its own base, S17. S34
  for `instance` and `supports`, S28. S7 still gives a wrong value for a
  struct literal constant, among the defects no rule names.
- M29, the platform layer, M30. M31, the malformed-input tests, M43.
  M32 for infinity and NaN, M52. M22 for the other atomic operations of
  width 1, now minor under rule 3. M34 for the name of a function in
  CodeView, now minor.
- Minor: the format attribute, the unchecked sums of `arena.c` and
  `text.c`, the enum tables of `ast_dump.c`, `diagnostic.c` under rule
  9, rules 18 and 19, the prefixes of rules 20 and 25 in both halves of
  antic and in `jsontree.h`, the `const` casts, the out-of-memory exits,
  the blocks the two back ends share, the dead parameter of
  `lower_module`, the `neon` dead store, the small helpers of `anti`,
  the comment of `lower_address` and the path rules.

### New since

Most new findings come from features built since the first audit:

- Generics in library files: S01, S13, M20 and M33. Generics in the
  checker: S07, S11, M34 and the copies of M12.
- Plugins: S19, S28, S29, S33, M47 and M58. The regex glue and pattern
  calls: S14, S25, S26 and M44. Simd structs: S02, S03, S24 and S13.
- Nested types and closures in `f"..."`: S05 and S08.
- The structure of the whole repository, the standard library and the
  tests were audited for the first time. Of their 49 findings, 48 are
  new, and the path rules of `structure` are still open from
  `cross-cutting` of the first audit.

The tool pass counts 43 analyzer warnings where the first audit had
92, 163 functions past a threshold where it had 119, and 9 files past
3000 lines where it had 3.

## The structure

The direction of the build holds. The runtime includes nothing of antic
or `anti`, and the back end includes nothing of the front end. The front
end reaches the IR at one include, for the allocator. Each runtime file
has one purpose, and equality and hashing each stand in one file of
lowering. The checker holds one rule of ownership in
`sema_needs_teardown`, and the thread-safe collections wrap the plain
ones. What fails is the one place of a concern. 18 major findings are
copies that drifted, and five of them already give wrong code or output
(M13, M14, M15, M18, M19).

The three places where separation of concerns most needs work:

1. The checker. `struct checker` has 46 fields that eleven files read
   and write. The context of the code being checked has no owner (S04).
   The split of `sema.c` produced files that call each other both ways
   and whose purpose the header no longer states (M10). The context
   needs one type with one pair of writers. Files for the value rules
   and the class model would then give each part one sentence.
2. Owning values in lowering. Teardown, copy, the defaults of an object,
   `==` and hash are each written as two or more families across
   `lower.c`, `lower_stmt.c`, `lower_expr.c`, `lower_eq.c` and
   `lower_hash.c`. Four of them have drifted into a leak or a wrong
   result (M05, M13, M14, M48). One file of owning values, gated by
   `sema_needs_teardown`, and one walk of the parts would hold them.
3. The library file as a second way into the checker's rules. The
   checker enforces the rules of simd structs, enums, lanes and trees
   for source. The table reader repeats part of them, and the tree
   section of generics repeats none. A library file then reaches
   lowering and the back ends with what source cannot (S01, S03, S13,
   S23, S24, M20, M33). A verifier after reading that applies the checker's rules
   would put the rule in one place.

After these three come the contracts at antic's edges, the runtime ABI
(M04, M06) and the interface `anti` uses (M02), and the platform layer
(M30, M31).

## Decisions a fix step needs

- S29 has two fixes. `rt` names a count of live objects that the
  compiler writes for every class of a library. The other is a refusal
  of `--no-hooks` in a program that loads one. The refusal stays inside
  antic, and step 10 takes it. The count crosses into the runtime.
- M01 removes `List`, `Map` and `IntMap` of `anti.collection`, which
  `docs/decisions.md` keeps until Eddie confirms.
- `rt.configure` records `[injections]` and applies none of it. `rt`
  leaves refusing or applying the table to Eddie.
- `anti_lang_` of the root functions stands against rule 25. `rt` leaves
  an exception in the rule to Eddie.
- M25 in one walk shared by the runtime and `anti` crosses the boundary.
  Step 29 takes one constant and one message in both instead.

## Fix steps

Each step is one session's work, stays within the files of one area,
and names the findings it closes. Each severe or major fix starts with
the test that shows the defect, which also closes the part of M43 for
that reader. Steps 1 to 13 close every severe finding.

1. antic library reader, tree relations. S01: one check of each tree
   after `read_tree`, for `otherwise_at`, case numbers, `enum_value`,
   parameter and copy counts against their types, reference 0 and
   `ANTL_NO_TYPE` where a node needs one, and a depth limit that refuses
   a node seen twice. Damaged trees as tests. Files: `antl_tree.c`,
   `tests/unit/test_modules.c`.
2. antic library reader, tree counts and primitives. S13 with a 32-bit
   lane count, M33 through `get_count`, M20 with the tree reader on the
   checked primitives, and the minor rule 3 conversions and wrappers of
   the tree reader. Files: `antl_tree.c`, `antl.c`, `antl_io.h`, tests.
3. antic library reader, tables. S03 with one lane check for both
   paths, S24 with the checker's simd rules in `read_tables`, and S23.
   The smaller reader gaps of the minor rule 14 row go with them. Files: `antl.c`,
   `tests/unit/test_modules.c`.
4. antic parser and lexer. S05 with `descend` in `nested_type` and the
   scan, a depth count in the lexer and a limit on the depth of the tree
   built, S08 with an `origin` for the inner parser, and M37 with a scan
   that keeps its own index. Files: `parser.c`, `parser.h`, `lexer.c`,
   `tests/unit/test_parser.c`, `tests/unit/test_lexer.c`.
5. antic checker context. S04: one type for the context, entered and
   left by one pair of functions, a quiet probe that caches nothing, and
   the error type of `try`. Files: `sema_checker.h`, `sema.c`,
   `sema_const.c`, `sema_generic.c`, `sema_stmt.c`.
6. antic checker, types and copies. S06 by a worklist, S07 with the
   cycle and base checks on each copy when filled, S11, M34 with a bound
   on copies, and M36 with a record of answered types. Files: `types.c`,
   `sema.c`, `sema_generic.c`, `sema_copies.c`, `sema_call.c`.
7. antic checker, expressions and the dump. S02 with `_` and the lock
   word refused in `fixed_layout`, S09, S10, S12, S14 with M44, S15 and
   S16. Files: `sema_expr.c`, `sema_call.c`, `sema.c`, `sema_pattern.c`,
   `ast_dump.c`.
8. antic optimizer and register allocator. S17, S18, S21 with a frame
   limit on both targets, and M53. Files: `optimize.c`, `regalloc.c`,
   `arm64.c`, `x86_64.c`.
9. antic lowering of loops, and the IR printer. S20 and S22, with the 19
   pinned outputs rewritten. Files: `lower_stmt.c`, `ir_print.c`,
   `ir_verify.c`, `tests/dump/`, `tests/opt/`.
10. antic whole-program pass and hooks. S19 with calls through an
    abstract class kept indirect where plugins load, and S29 with a
    refusal of `--no-hooks` for such a program. A host class of
    `Greeter` in the plugin test. Files: `whole.c`, `driver.c`,
    `main.c`, `tests/plugin/`.
11. runtime regex glue and traces. S25, S26 and S27, with a large patch
    offset and an open template as tests. Files: `patterns.c`,
    `trace.c`.
12. runtime plugin loader. S28 with the object counted under the lock,
    S33 in `class_sound`, M47, and M58 with a library for another
    runtime and one with a foreign interface. Files: `plugin.c`,
    `loaded.c`, `hooks.c`, `tests/run_plugin_versions.cmake`.
13. anti allocation and integers. S30, S31, S32 and S34. Files:
    `files.c`, `bindclang.c`, `repo.c`, tests.
14. antic checker split, value rules. M10 for the value rules,
    `sema_new_node`, `sema_thread_safe` and the pointer sets, with the
    minor header sections and misplaced helpers. Files: the `sema_*.c`
    files and `sema_checker.h`.
15. antic checker split, class model and doc warnings. M10 for the class
    model out of `sema.c` and the doc warnings out of `sema_export.c`,
    and M57 with its tests. Files: `sema.c`, `sema_export.c`, new files
    in `src/antic/`.
16. antic checker, rules written twice. M12 and M35. Files:
    `sema_export.c`, `sema_pattern.c`, `sema_stmt.c`, `sema_expr.c`,
    `sema_call.c`, `sema_generic.c`, `sema_copies.c`.
17. antic lowering, owning values. M05, M14 and M48 in one file with
    one predicate, and the minor loops over array elements. Files:
    `lower.c`, `lower_stmt.c`, `lower_expr.c`, a new file of lowering.
18. antic lowering, `==` and hash. M13 with one walk of the parts, and
    the minor copy of `holds_union` into `types.c`. Files: `lower_eq.c`,
    `lower_hash.c`, `sema_hash.c`, `types.c`.
19. antic lowering, the rest of the split. M11, M45 and M49. Files:
    `lower.c`, `lower_expr.c`, `lower_stmt.c`, `lower_desc.c`.
20. The ABI between antic and the runtime. M04 and M06 with one table of
    runtime functions and records, a unit test against `src/rt/`, and
    named descriptor items, with the minor on the place of `Match`. This
    step crosses the boundary by its nature. Files: `lower.c`,
    `lower_desc.c`, `whole.c`, a header of `src/antic/`, headers of
    `src/rt/`, `tests/unit/test_lower.c`.
21. antic reach and folding. M16 and M18. Files: `whole.c`,
    `optimize.c`, `layout.c`, `lower_expr.c`.
22. antic ARM64. M15 and M50. File: `arm64.c`.
23. antic C header. M07, M19, M51, M52 and the minor `neon` name. Files:
    `header.c`, `lower_desc.c`.
24. antic driver. M17 and M21. Files: `driver.c`, `linker.c`.
25. antic allocator. M03 and the minor out-of-memory exits. Files:
    `ir.c`, `ir.h`, one new file of `src/antic/`, its callers.
26. antic platform layer. M30 and M31 for antic, and the minor path
    rules. Files: `platform.c`, `selfpath.c`, `userdirs.c`, `process.c`,
    `target.c`, `applesdk.c`, `driver.c`.
27. anti interface to antic. M02 and the minor crossings of `notice.h`
    and `userdirs.c`. Files: `src/anti/`, `src/antic/driver.h`, one new
    header.
28. anti platform layer. M30 and M31 for `anti`. Files:
    `src/anti/platform.c`, `files.c`, `sdk.c`, `syms.c`.
29. anti copies that drifted. M23, M24, M25, M26 and the minor names
    spelled twice. Files: `build.c`, `test.c`, `doc.c`, `check.c`,
    `main.c`, `symmap.c`, `syms.c`, `bind.c`, `bindclang.c`.
30. anti inputs. M39, M40, M41, M42 and M46. Files: `fmt.c`, `doc.c`,
    `zip.c`, `syms.c`.
31. runtime text and `Object.deserialize`. M32, M38 and M22, with the
    variant, `?T` and cut text tests of M43. Files: `errno.c`,
    `platform_windows.c`, `registry.c`.
32. runtime platform layer, locks and threads. M30 for the lock, the
    condition variable, the thread and the loader queries. Files:
    `lock.c`, `sync.c`, `threads.c`, `loaded.c`, `signal.c`, the
    platform files.
33. runtime platform layer, files and traces. M30 for `fs.c` and
    `trace.c`.
34. runtime platform layer, the rest. M30 for `cpu.c`, `mem.c`,
    `start.c`, `errno.c`, `init.c` and `atomic.c`, with the minor rule 1
    extensions.
35. The remaining malformed-input tests of M43. They cover the plugin
    index, a configuration that is no TOML, and the Mach-O symbol table
    and debug map. They also cover the command-line splitter,
    `read_source` with a NUL, and a truncated COFF object and archive
    index. Files: `tests/`.
36. Tests that pass whatever the code does. M54, M55, M56 and the minor
    tests that drop out. Files: `tests/CMakeLists.txt`,
    `tests/programs/`.
37. Test runners. M59, M60, M61 and M62. Files: `tests/*.cmake`,
    `tests/CMakeLists.txt`.
38. Standard collections. M27, M08 and M28, then M01 once Eddie
    confirms. Files: `src/std/anti/collection.anti`,
    `src/std/anti/collection/`.
39. Standard `json` and `log`. M29 and M09. Files: `json.anti`,
    `log.anti`.
40. The minor findings, one session per area: antic front end, antic
    back end, runtime, `anti`, standard library, tests. Each takes the
    rows of the minor table whose places lie in its area.
41. The splits of rules 18 and 19, last and one file per session. First
    `parser.c`, `antl.c` and `driver.c`. Then each checker or lowering
    file that still passes 3000 lines after steps 14 to 19.
