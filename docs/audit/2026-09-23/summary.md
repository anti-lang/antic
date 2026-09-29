# Summary of the C audit

This document merges the seven reports of the C audit under `docs/audit/`
and orders the work they found. The reports are `tool-pass`, `front-end`,
`back-end`, `rt`, `anti-tool`, `input-readers` and `cross-cutting`, each
named here by the stem of its file. The reports audited `ab03345` and
`fddd434`, and `git diff` shows no change under `src/`, `tests/` or
`tools/` from `ab03345` to this commit, so every path:line still points at
the code the reports read. The line of each severe finding was read again
for this summary. A finding that two or more reports list stands here once,
with every report that lists it. Where reports grade one finding
differently, the summary applies the definitions of `docs/c-guidelines.md`
and says which grade it took. Each finding carries one primary rule, and a
second rule a report named is given in its entry. The judgement of each
threshold finding stays in the report that made it.

## Counts

The seven reports list 193 findings: 55 severe, 51 major and 87 minor.
Merged, they are 141.

| Severity | Merged findings |
|---|---|
| Severe | 46 |
| Major | 35 |
| Minor | 60 |
| Total | 141 |

| Rule | Severe | Major | Minor |
|---|---|---|---|
| 1 | 0 | 0 | 2 |
| 3 | 10 | 3 | 4 |
| 5 | 5 | 1 | 3 |
| 6 | 4 | 1 | 2 |
| 7 | 1 | 0 | 0 |
| 9 | 0 | 2 | 4 |
| 10 | 0 | 0 | 2 |
| 11 | 0 | 0 | 4 |
| 12 | 0 | 4 | 2 |
| 13 | 0 | 0 | 1 |
| 14 | 21 | 15 | 5 |
| 15 | 0 | 1 | 0 |
| 16 | 1 | 1 | 3 |
| 17 | 0 | 1 | 0 |
| 18 | 0 | 0 | 1, covering 119 functions |
| 19 | 0 | 0 | 1, covering 3 files |
| 20 | 0 | 0 | 1 |
| 21 | 0 | 0 | 2 |
| 22 | 0 | 1 | 0 |
| 23 | 3 | 1 | 1 |
| 24 | 0 | 0 | 3 |
| 25 | 0 | 0 | 2 |
| 26 | 0 | 0 | 14 |
| 27 | 0 | 0 | 3 |
| None, wrong code or output | 1 | 4 | 0 |

### Grades the reports disagree on

- The recursion of the readers of `anti bind` (S46) is major in
  `anti-tool` and severe in `input-readers`. It is severe here, as the
  parser's recursion is in `front-end`.
- The cache paths from a repository index (S45) are major in `anti-tool`
  and severe in `input-readers`. Severe here, as a write outside the
  directory the reader owns.
- The race on the signal table (S35) is major in `rt` and part of a severe
  finding in `cross-cutting`. A data race is undefined behaviour, so it is
  severe here.
- The long COFF section name (M8) is major in `back-end` and minor in
  `input-readers`. Major here, since a wrong offset is accepted without a
  refusal.
- The missing platform layer (M29) is minor in `rt` and major in
  `cross-cutting`. Major here, as a crossed boundary.
- The missing malformed-input tests (M31) are major in `input-readers` and
  minor in `front-end` and `anti-tool`. The guidelines now grade it
  major, as Eddie decided.
- The right shift of a negative constant in `bindexpr.c:438` is major in
  `anti-tool` and minor in `input-readers`. No target differs, as for the
  same shift in `sema.c:6995`, so it is minor here.

## Severe

Every severe finding, by area. The ID is the one the fix steps use.

### antic front end

| ID | Place | Rule | Finding | Reports |
|---|---|---|---|---|
| S1 | `src/antic/layout.c:71` | 14 | A `simd struct` whose one field is `_` divides by zero in `round_up`. `check_simd_struct` at `sema.c:539` and `read_tables` of `antl.c` should refuse it. | tool-pass, input-readers |
| S2 | `src/antic/lexer.c:1281` | 14 | A doc comment first inside `{ }` of `f"..."` reads `tokens.items[tokens.count - 1]` with `count` 0. | front-end |
| S3 | `src/antic/parser.c:1013` | 14 | The parser recurses once per level of nesting, and 200000 `(`, `-` or `{` overflow the stack. | front-end, input-readers |
| S4 | `src/antic/sema.c:7199` | 14 | The checker recurses once per link of a chain of constants, and the worker walk at `sema.c:1603` once per call. | front-end |
| S5 | `src/antic/sema.c:10119` | 14 | A class that holds itself by value is never refused, and every walk over fields then recurses without end. | front-end |
| S6 | `src/antic/sema.c:2344` | 14 | After "contains itself" the checker goes on, and `fixed_layout` recurses into the cycle. | front-end |
| S7 | `src/antic/sema.c:7102` | 14 | A constant reads a field of a struct literal that `eval_const` filled with an integer 0. | front-end |
| S8 | `src/antic/sema.c:7045` | 5, 14 | The size of a repeated array constant overflows, and the loop writes past the block. | front-end |
| S9 | `src/antic/sema.c:2309` | 6, 14 | `is` on a variant without cases reads the name of case 0. | front-end |
| S10 | `src/antic/sema.c:11259` | 3 | `doc_check_text` adds 0 to a null pointer. | front-end |
| S11 | `src/antic/lexer.c:218` | 16, 3 | Counters of `int` overflow on a large input: the line and column of the lexer, `tn` at `sema.c:111`, the index length at `driver.c:2280`, the shift of `uleb` and `sleb` at `src/rt/symbols.c:98` and `117`, and the precision at `src/rt/toml.c:296` and `397`. | front-end, input-readers |

### antic library reader

| ID | Place | Rule | Finding | Reports |
|---|---|---|---|---|
| S12 | `src/antic/antl.c:1657` | 14 | A member of a library class need not have a function type, and the checker then reads `params[-1]` or a NULL result. | front-end, input-readers |
| S13 | `src/antic/antl.c:1878` | 14 | `map_agg`, `map_sym` at line 1944 and `types_find_cycle` at 1694 recurse once per forward reference. | front-end, input-readers |
| S14 | `src/antic/layout.c:352` | 3, 14 | A symbolic `sdiv` or `srem` of `INT64_MIN` by -1 from a library passes the reader at `antl.c:2058` and overflows in `fold_op`. | front-end, input-readers |
| S15 | `src/antic/antl.c:2266` | 14 | A `jump` or branch target may be an integer, and `mark_reachable` indexes the blocks with it. | input-readers |
| S16 | `src/antic/antl.c:2331` | 14 | A function body with no block reaches `f->blocks[0]` of a NULL array. | input-readers |

### antic back end

| ID | Place | Rule | Finding | Reports |
|---|---|---|---|---|
| S17 | `src/antic/optimize.c:1011` | none | Store forwarding reads a temporary that was assigned again, and the function returns the wrong value. | back-end |
| S18 | `src/antic/regalloc.c:713` | 6 | A third spilled read indexes `scratch[2]` past its end, and `msub` overwrites a live register. | back-end |
| S19 | `src/antic/coff.c:452` | 14 | The raw data of a section marked uninitialised is read without the range check of line 379. | back-end, input-readers |
| S20 | `src/antic/layout.c:166` | 5 | A packed bitfield whose unit is larger than the aggregate gets the offset `UINT64_MAX`, and the program reads and writes before the object. | back-end |
| S21 | `src/antic/layout.c:218` | 5 | The size of an array in bits wraps, from source or from a library, and line 251 wraps the same way. `input-readers` grades the library path major. | back-end, input-readers |
| S22 | `src/antic/linker.c:435` | 3 | A Linux shared library through lld passes NULL to `%s`. | back-end |
| S23 | `src/antic/lower.c:6547` | 3 | A `by` step of 2^63 negates `INT64_MIN`, here and at line 6602. | back-end |
| S24 | `src/antic/whole.c:2092` | 14 | `copy_chain` takes the source and the length of an interface chain from a library file unchecked. | back-end |
| S25 | `src/antic/ir_print.c:493` | 6 | The printer looks a function relocation up among the globals and reads past `m->globals`. | back-end |

### Runtime

| ID | Place | Rule | Finding | Reports |
|---|---|---|---|---|
| S26 | `src/rt/text.c:34` | 5 | A large pad width overflows `reserve` of the builder, and the fill writes past the block. | rt |
| S27 | `src/rt/threads.c:300` | 5 | The results of `parallel` are allocated from an unchecked product. | rt |
| S28 | `src/rt/symbols.c:517` | 14 | An ELF section of type NOBITS passes with any size, and its readers read past the file. | rt, input-readers |
| S29 | `src/rt/symbols.c:420` | 3, 14 | The DWARF line counter overflows on `advance_line`, and at line 397 on special opcodes. | rt, input-readers |
| S30 | `src/rt/toml.c:137` | 3, 14 | `repeats` returns `LLONG_MAX + 1` for a `[[table]]` after a 20-digit key. | rt, input-readers |
| S31 | `src/rt/conf.c:140` | 3, 14 | `threads` is read with `atoi`, here and at `threads.c:120`, and the pool size differs between targets. | rt, input-readers |
| S32 | `src/rt/registry.c:626` | 14 | `Object.deserialize` takes a pointer, a function pointer and at line 448 a slice address from the text. The code follows `docs/decisions.md`, which the rule contradicts. | rt |
| S33 | `src/rt/conf.c:148` | 23 | `rt.configure` frees values that `anti_rt_conf_get` handed to other threads. | rt |
| S34 | `src/rt/loaded.c:26` | 23 | The table of open libraries and the message buffers at `plugin.c:29` and `88` have no guard. | rt, cross-cutting |
| S35 | `src/rt/signal.c:37` | 23 | `handlers`, `started` and `pipe_ends` are written at line 128 and read by the reader thread without a guard. | rt, cross-cutting |
| S36 | `src/rt/atomic.c:172` | 3 | Atomic subtraction negates `INT64_MIN`. | rt |
| S37 | `src/rt/trace.c:432` | 14 | The build id of a loaded module is read at an address the file on disk names. | input-readers |
| S38 | `src/rt/plugin.c:160` | 14 | The provides table of a plugin is trusted: the version overflows, line 383 adds an unchecked offset, line 441 passes a negative precision. | input-readers |

### anti tool

| ID | Place | Rule | Finding | Reports |
|---|---|---|---|---|
| S39 | `src/anti/bindapi.c:227` | 6 | Two structs of one name in `raylib_api.json` write past the field array. | anti-tool, input-readers |
| S40 | `src/anti/deps.c:60` | 3, 14 | A version part of 20 digits overflows a `long`, and `next_major` at line 90 adds 1. | anti-tool, input-readers |
| S41 | `src/anti/syms.c:961` | 3, 14 | `sscanf` and `atoi` convert numbers out of range: `syms.c:1148` and `1152`, `bindclang.c:70` and `1067`, and `src/antic/applesdk.c:25`. | anti-tool, input-readers |
| S42 | `src/anti/syms.c:643` | 14 | `memcmp(NULL, name, 0)`, and `qsort` of a NULL array at `symmap.c:219` and `225`. | anti-tool |
| S43 | `src/anti/bindclang.c:1252` | 7 | An array of `void *` is read as an array of `const char *`. | anti-tool |
| S44 | `src/anti/bindclang.c:399` | 14 | An unnamed enum passes NULL to `strcmp`. | input-readers |
| S45 | `src/anti/repo.c:300` | 14 | Names, versions and module paths of an index reach cache paths unchecked, and `../` writes outside the cache. Also `repo.c:246`, `deps.c:554` and `sdk.c:161`. | anti-tool, input-readers |
| S46 | `src/anti/bindexpr.c:319` | 14 | The readers of `anti bind` recurse without a bound: `bindexpr.c:277`, `bindtype.c:454`, `506` and `569`, and `bindwrite.c:594` on a self-referring struct. | anti-tool, input-readers |

## Major

Grouped by rule. Each entry names its place and its reports.

### Rule 14, untrusted input

- M1. `src/anti/build.c:51`. Read loops take a read error for the end of
  the file. Twelve more files of `src/anti/`, `src/antic/driver.c:805` and
  `src/rt/trace.c:140`. In `fmt_run` a short read rewrites the source
  without its tail. tool-pass, anti-tool, rt.
- M2. `src/antic/sema.c:9906`. A cycle of base classes never ends, and
  every chain walk loops. front-end.
- M3. `src/antic/antl.c:2089`. The reader takes a bitfield width from the
  file unchecked, also at line 1503, which reaches S20. front-end,
  input-readers.
- M4. `src/antic/antl.c:2080`. The alignment of an IR aggregate is not
  checked to be a power of two. front-end, input-readers.
- M5. `src/antic/antl.c:1740`. A text constant is accepted for a slice of
  any element. front-end.
- M6. `src/antic/antl.c:1592`. Any type is accepted below an enum.
  front-end.
- M7. `src/antic/driver.c:1972`. The IR of a library file never passes
  `ir_verify`. input-readers.
- M8. `src/antic/coff.c:357`. A long section name is parsed with
  `strtoul` past its 7 bytes. back-end, input-readers.
- M9. `src/rt/symbols.c:369`. A DWARF 5 entry table without formats loops
  for its count, and `file_name` at line 248 too. input-readers.
- M10. `src/rt/registry.c:684`. A repeated member in `Object.deserialize`
  leaks what it built. rt, input-readers.
- M11. `src/anti/zip.c:398`. Inflate grows its output without a bound,
  compared with the size only at line 587. anti-tool, input-readers.
- M12. `src/anti/repo.c:86`. `http://localhost:@host/` passes the loopback
  check. anti-tool.
- M13. `src/anti/deps.c:762`. The lock file writes strings of the index
  into TOML unescaped. anti-tool.
- M14. `src/anti/doc.c:1270`. `anti doc` takes the module path of a
  library file as a file name, and writes names into HTML unescaped.
  anti-tool.
- M15. `src/anti/files.c:235`. The directory walk treats an unreadable
  directory as empty and follows links in a loop. anti-tool.

### Rule 12, cleanup on every path

- M16. `src/anti/main.c:296`. The command blocks of `main` free by hand
  before each return and leak on failure, at lines 348, 398, 460, 487 and
  539, and in `src/antic/main.c:439`. tool-pass, anti-tool.
- M17. `src/anti/manifest.c:340`. Readers leave allocations behind on
  failure: `manifest_read` at 340 and 349, `zip_read` at `zip.c:256`.
  anti-tool, input-readers.
- M18. `src/rt/toml.c:109`. A failed copy in `add` leaks the other half.
  rt, input-readers.
- M19. `src/rt/signal.c:84`. A failed start of the signal reader leaks the
  pipe. rt.

### Rule 3, results that differ between targets

- M20. `src/rt/conf.c:592`. Windows reads `ANTI_CONF` through the ANSI
  entry point into 1024 bytes, and `plugin.c:55` loads a library with
  `LoadLibraryA`. rt, input-readers.
- M21. `src/antic/optimize.c:1063`. `same_offset` and `same_address` at
  line 988 compare union bytes no store wrote. back-end.
- M22. `src/rt/atomic.c:17`. The `_MSC_VER` branch loads through
  `volatile`, which is no sequentially consistent load on windows-arm64.
  rt.

### Other rules

- M23. `src/antic/sema.c:2368`, rule 5. `fixed_layout` overflows its
  size. front-end.
- M24. `src/antic/sema.c:6731`, rule 9. A float literal past 127
  characters is cut short, also at `sema.c:1133` and `lower.c:632`, so
  the checker and the back end may fold different values. front-end,
  back-end.
- M25. `src/antic/lower.c:2712`, rule 9. A long name is cut and two
  symbols get one name, at eight more places of `lower.c` and in `c_name`
  at `header.c:93`. back-end.
- M26. `src/antic/ir_verify.c:80`, rule 6. The verifier passes an
  out-of-range function or global without `fail`. back-end.
- M27. `src/rt/time.c:61`, rule 16. A sleep of 2^32 milliseconds wraps on
  Windows. rt.
- M28. `src/antic/coff.c:910`, rule 17. The COFF join writes a 32-bit
  section count into 16 bits. back-end.
- M29. `src/rt/conf.c:359`, rule 22. No document names the platform layer,
  and `conf.c`, `plugin.c`, `loaded.c`, `src/antic/driver.c:504` and
  `524`, `src/antic/userdirs.c:134` and `src/anti/bind.c:97` branch on the
  platform. rt, cross-cutting, anti-tool.
- M30. `src/rt/threads.c:94`, rule 23. `wake_caller` signals one waiter
  where two kinds wait, and a `parallel` can sleep forever. rt.
- M31. Rule 15. Fifteen readers have no malformed-input test. The tests
  of the parser, the library reader, TOML, the configuration,
  `Object.deserialize` and `bindexpr.c` feed no deep, oversized or
  out-of-range input. The list is in `input-readers`. front-end,
  anti-tool, input-readers.

### No rule, wrong output

- M32. `src/antic/header.c:966`. A float constant is written to the C
  header as `2f` or `2`. back-end.
- M33. `src/antic/header.c:765`. The C header drops table functions past
  64. back-end.
- M34. `src/antic/debug.c:145`. The debug directives do not escape `\` or
  `"` in the source path, and `-g` fails on Windows paths. back-end.
- M35. `src/antic/regalloc.c:388`. Rematerialisation clears the constant
  of register 0 after an instruction that defines nothing. back-end.

## Minor

Counted by rule, with the place each report opens the finding at.

| Rule | Count | Findings and reports |
|---|---|---|
| 1 | 2 | The format attribute in ten places, `src/antic/text.h:17` (back-end, cross-cutting). Table entries read as functions, `src/rt/object.c:612` (rt). |
| 3 | 4 | Implementation-defined conversions in the front end, `sema.c:6995` (front-end), and in the back end, `x86_64.c:367` (back-end). Conversions of input values, `src/rt/registry.c:451` and `src/anti/bindexpr.c:438` (input-readers, anti-tool). Two calls with side effects in one argument list, `antl.c:2538` and `lower.c:1710` (front-end, back-end). |
| 5 | 3 | Unchecked products before an allocation, `lexer.c:264` (front-end), `ir.c:19` (back-end), `src/anti/test.c:157` (anti-tool). |
| 6 | 2 | `marker_length` reads past a 3-byte token, `parser.c:189` (front-end, input-readers). Fixed arrays filled up to an unchecked count, `regalloc.c:852` (back-end). |
| 9 | 4 | Unchecked `snprintf` results in messages, `diagnostic.c:28` (front-end), `select.c:231` (back-end), `src/rt/conf.c:489` (rt), `src/anti/bindclang.c:571` (anti-tool). |
| 10 | 2 | Text with a length scanned for a NUL, `parser.c:2799` (front-end), `src/rt/conf.c:416` (rt). |
| 11 | 4 | Returned memory without its owner, `lexer.h:136` (front-end), `lower.c:336` (back-end), `src/rt/object.h:213` (rt), `src/anti/zip.h:48` (anti-tool). |
| 12 | 2 | Releases repeated before each return, `lower.c:1852` (back-end), `src/rt/plugin.c:433` (rt). |
| 13 | 1 | The runtime has no one failure routine, `src/rt/sync.c:78` (rt, cross-cutting). |
| 14 | 5 | Gaps in the reader's checks, `antl.c:2337` (front-end). Stack depth of `mark_reachable`, `optimize.c:747` (back-end). Saturating numbers, `src/anti/bindexpr.c:115` (anti-tool). The Mach-O slide walk, `src/rt/trace.c:228`, and a NUL in a configuration path, `src/rt/conf.c:750` (input-readers). |
| 16 | 3 | Mixed width and sign, `sema.c:3893` (front-end), `regalloc.c:469` (back-end), `src/anti/zip.c:121` (anti-tool). |
| 18 | 1 | 119 functions past a threshold, `docs/audit/data/thresholds.txt`. The judgements are in tool-pass, with more in front-end, back-end, rt and anti-tool. |
| 19 | 1 | `sema.c`, `lower.c` and `x86_64.c` past 3000 lines (tool-pass, front-end). |
| 20 | 1 | Functions used in one file are not `static`, `src/rt/digest.c:79` (cross-cutting, back-end, anti-tool). |
| 21 | 2 | `src/rt/std.h:107` declares half its functions outside its guard (rt). Headers that include too much or lean on another, `src/antic/cpu.h:9` (cross-cutting, front-end, anti-tool, rt). |
| 23 | 1 | Mutable state without its comment, `src/rt/conf.c:69`, `src/rt/init.c:3` and `lexer.c:1545` (cross-cutting, rt, front-end). |
| 24 | 3 | `const` cast away or missing, `sema.c:2315` (front-end), `lower.c:7126` (back-end), `src/anti/doc.h:18` (anti-tool). |
| 25 | 2 | Runtime names outside `anti_rt_`, `src/rt/utf.h:11` (cross-cutting, rt). Host names outside their module's prefix, `src/anti/files.h:10` (cross-cutting, front-end, back-end, anti-tool). |
| 26 | 14 | The out-of-memory block and `allocate` copies, `ir.c:21` (front-end, back-end, anti-tool, cross-cutting). File reader and writer copies, `src/anti/build.c:51` and `doc.c:81` (tool-pass, anti-tool, cross-cutting). The module reader of `src/anti/test.c:29` (anti-tool). Small helpers of `anti`, `src/anti/syms.c:109` (anti-tool). Code the runtime and the host both write, `src/antic/sha256.c:1`, with `file_bytes` and `same_bytes` of `rt` (cross-cutting, rt). Path helpers, `src/anti/syms.c:135` (cross-cutting). Repeated blocks of the front end, `types.c:305` (front-end). Duplicates between the back ends, `x86_64.c:364` (back-end). Duplicates elsewhere in the back end, `lower.c:498` (back-end). Dead code of the front end, `sema.c:3212` (front-end). Dead code of the back end, `lower.c:113` and `select.c:236` (back-end, cross-cutting). Dead code of the runtime, `src/rt/cpu.c:307` and `start.c:159` (rt, cross-cutting, tool-pass). The dead store at `header.c:547` (tool-pass). Dead stores and parameters of `anti`, `src/anti/build.c:636` (anti-tool). |
| 27 | 3 | Comments above the wrong item, `lower.c:1674` (back-end) and `src/anti/test.c:371` (anti-tool). Two long sentences, `src/rt/assert.c:19` (rt). |

Paths without a directory are in `src/antic/`.

`front-end` also lists six defects no rule names, and `anti-tool` a
question of resolution. Neither is counted here.

## Answered by Eddie

- S32. `Object.deserialize` never takes an address from the text.
  `serialize` writes a pointer, a slice or a function pointer that the
  object does not own as `null`, since it refers to something outside the
  object. `deserialize` accepts only `null` there, which gives `none` or
  an empty slice, and fails on anything else. Owned data is written by
  content, as now. The entry under "Object model" in `docs/decisions.md`
  says so, and fix step 13 changes the code.
- M29. The platform layer is the files named platform:
  `src/rt/platform.h` with `src/rt/platform_posix.c` and
  `src/rt/platform_windows.c`, and `src/antic/platform.c` and
  `src/anti/platform.c` for the tools. A `#if` on the host system stands
  only there. Rule 22 concerns the host, and code that chooses by target
  through run-time values is not affected. Rule 22 of
  `docs/c-guidelines.md` and "Repository layout" in `docs/decisions.md`
  say so.
- Rule 25. Every exported symbol of the runtime starts with `anti_rt_`,
  with no other prefix. Rule 25 of `docs/c-guidelines.md` says so. The
  renaming of `anti_cpu_`, `anti_utf8_`, `anti_elf_` and the rest is a
  minor step later.
- Rule 15. A reader of untrusted input without a malformed-input test is
  a major finding, which confirms the grade of M31 here. "Severity" in
  `docs/c-guidelines.md` says so.

## Fix steps

Each step is one session's work, stays within the files of one area, and
names the findings it closes. Each severe or major fix starts with the
test that shows the defect, which also closes the part of M31 for that
reader. Severe steps come first.

1. antic input and driver. S2, S3 with one depth limit in the parser and
   a diagnostic, S11 for `lexer.c` and a size cap in `read_source`, the
   narrowing at `driver.c:2280`, S41 for `applesdk.c`, M1 for
   `driver.c:805` and M16 for `src/antic/main.c`. Files: `lexer.c`,
   `parser.c`, `driver.c`, `main.c`, `applesdk.c`.
2. antic checker, types that contain themselves. S1 in
   `check_simd_struct`, S5, S6, M2, M23 and the counter of `tn` from S11.
   Files: `sema.c`, `types.c`.
3. antic checker, constants and walks. S4, S7, S8, S9, S10. File:
   `sema.c`.
4. antic library reader, record checks. S12, S15, S16, S1 in
   `read_tables`, M3, M4, M5, M6, with malformed library files as tests.
   File: `antl.c`, tests in `tests/unit/test_modules.c`.
5. antic library reader, depth and verification. S13, M7 and M26, so
   that every library runs through a verifier that fails on a bad index.
   Files: `antl.c`, `driver.c`, `ir_verify.c`.
6. antic layout. S14 in `fold_op`, S20 and S21. File: `layout.c`.
7. antic optimizer and register allocator. S17, S18, M21 and M35. Files:
   `optimize.c`, `regalloc.c`.
8. antic lowering, linking and printing. S22, S23, S24, S25. Files:
   `linker.c`, `lower.c`, `whole.c`, `ir_print.c`.
9. antic COFF. S19, M8, M28, with malformed objects in
   `tests/unit/test_coff.c`. File: `coff.c`.
10. runtime symbol and trace readers. S28, S29, S37, M9, the `uleb` and
    `sleb` shift of S11 and M1 for `trace.c`, with malformed ELF, Mach-O
    and DWARF inputs as tests. Files: `symbols.c`, `trace.c`.
11. runtime integer bounds. S26, S27, S30, S31, S36, the precision of
    S11 in `toml.c`, and M18. Files: `text.c`, `threads.c`, `toml.c`,
    `conf.c`, `atomic.c`.
12. runtime shared state. S33, S34, S35, M19, M22 and M30. Files:
    `conf.c`, `loaded.c`, `plugin.c`, `signal.c`, `threads.c`,
    `atomic.c`.
13. runtime plugin table and `Object.deserialize`. S38 and M10, and S32 as
    Eddie answered it. Files: `plugin.c`, `registry.c`.
14. anti repository inputs. S45, S40, M12, M13, M14, with a malformed
    `index.toml` and `anti.lock` as tests. Files: `repo.c`, `deps.c`,
    `sdk.c`, `doc.c`.
15. anti bind readers. S39, S43, S44, S46 and S41 for `bindclang.c`,
    with malformed API files, headers and ASTs as tests. Files:
    `bindapi.c`, `bindclang.c`, `bindexpr.c`, `bindtype.c`,
    `bindwrite.c`.
16. anti symbols and zip. S41 for `syms.c`, S42, M11 and M17 for
    `zip.c`, with truncated archives, broken deflate streams and
    malformed maps and traces as tests. Files: `syms.c`, `symmap.c`,
    `zip.c`.
17. anti files and `main`. One reader and one writer in `files.c` with
    the `ferror` and `fclose` checks, which closes M1 and the rule 26
    reader and writer copies of `src/anti/`, then M15, M16 for
    `src/anti/main.c` with one function per command, and M17 for
    `manifest.c`. Files: `src/anti/`.
18. antic output text. M24 with one helper for float literals, M25 with
    names built in a `struct text`, M32, M33 and M34. Files: `lower.c`,
    `header.c`, `debug.c`, `sema.c`.
19. runtime on Windows. M20 and M27, and M29 in the platform files Eddie
    named. Files: `conf.c`, `plugin.c`, `time.c`.
20. The minor findings, one session per area. The order is antic front
    end, antic back end, runtime and anti tool. Each session takes the
    rows of the minor table whose places lie in its area. The code both sides
    write twice under rule 26, and rule 25 as Eddie settled it,
    are one further session, since they cross the boundary.
21. The splits of rules 18 and 19, last and one file per session:
    `sema.c`, with `sema_check` split into its passes, then `lower.c`,
    following the judgements in `tool-pass`. Step 17 already splits
    `main` of `src/anti/`.
