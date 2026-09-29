# Input readers

The audit of every piece of C that reads bytes it did not write, against
rules 14 and 15 of `docs/c-guidelines.md`, with rules 3, 5, 6, 12 and 16
where they bear on reading. It covers `src/antic/`, `src/anti/` and
`src/rt/` at `fddd434`. `src/native/` holds no C file. Each reader was read
by hand, the lengths, offsets, counts and indices it takes from its input
followed to their first use, and the tests of `tests/` searched for input
that is truncated, oversized or points outside the data. Every finding
below was checked against the code. Nothing was built or run. The
findings of `docs/audit/tool-pass.md` are not repeated: the simd `_`
division by zero that `read_tables` also reaches, and the `fread` loops
without `ferror`.

There is no PDB reader in C. On Windows the runtime asks DbgHelp for names
in `src/rt/trace.c:733`, and `tools/check-pdb.cmake` reads the PDB at build
time. The runtime writes to standard error and exits with 70 on a bad
runtime configuration. `docs/decisions.md` settles that under "Runtime
configuration", so it is not reported under rule 13.

## Counts

| Severity | Rule | Findings |
|---|---|---|
| Severe | 14 | 10 |
| Severe | 6 | 1 |
| Severe | 3 | 6 |
| Severe | 16 | 1 |
| Major | 14 | 6 |
| Major | 12 | 2 |
| Major | 5 | 1 |
| Major | 15 | 1, covering 15 readers |
| Minor | 14 | 3 |
| Minor | 6 | 1 |
| Minor | 3 | 1 |
| Total | | 33 |

## The readers

"Bounds" says whether each length and offset is checked before the read.
"Refusal" says how bad input ends. "Tests" names the malformed-input tests.

| Reader | Bounds | Refusal | Tests |
|---|---|---|---|
| Lexer, `src/antic/lexer.c` | Yes. Every lookahead goes through `at`, and `validate` checks the UTF-8 of the whole file first. | Diagnostic. | `test_lexer.c:426` to `501`: bad UTF-8, open comments, strings and literals at the end, an integer too large, a NUL. `errors/lexical.anti`. |
| Parser, `src/antic/parser.c` | Lookahead yes. No limit on depth. | Diagnostic, or a crash on deep input. | `test_parser.c:527` to `825`, `errors/syntax.anti`, `missing_semicolon.anti`. No deep or cut-off input. |
| Source file, `read_source` of `driver.c` | Yes, and it refuses a NUL. | Diagnostic. | None feeds it a NUL. |
| Library file, `src/antic/antl.c` | The primitives `take`, `get_uint` and `get_count` are bounded. Operand kinds, block counts, member types, bitfield widths, alignments and symbolic divisions are not checked. | Error, or a crash later in the passes. | `damaged_files` of `test_modules.c:1273`: every prefix of one small file, the version, the magic and four pokes. No out-of-range index, oversized count or recursion. |
| COFF objects, `src/antic/coff.c` | Header, section table, relocations and strings yes, apart from an uninitialised section. | Error. | `test_refusals` of `test_coff.c:469` feeds only well-formed objects. |
| SDK directory names, `applesdk.c` | `sscanf` with `%d`. | Passed over. | None. |
| TOML, `src/rt/toml.c` | Yes, through `peek` and `at_end`. | NULL. | `inline_table_unclosed` of `test_toml.c:112`, and two cases in `std/toml.anti`. No cut-off, oversized or deep input. |
| JSON scanner, `src/rt/json.c` | Yes. Depth is capped at 64 and integers are checked before they multiply. | Error. | `test_json.c`: `truncated`, `unterminated_strings`, `bad_escapes`, `deep_nesting`, `numbers`, `garbage`. Complete. |
| Runtime configuration, `src/rt/conf.c` | Through the TOML reader. `threads` is read with `atoi`. | Exit 70 by decision. | `tests/conf/`: `bad-value`, `unknown-key`, `unknown-option`, `cycle`, `missing`. No file that is not TOML and no count out of range. |
| `Object.deserialize`, `src/rt/registry.c` | Yes, with a depth limit and an overflow check before `lend`. | NULL, and the undo gives every block back. | `std/registry.anti:125`, `serialize.anti:113`, `deserialize_undo.anti`. No cut-off text, repeated member or oversized length. |
| Float reader, `anti_rt_read_float` of `text.c` | Yes. | Error. | `refusals` of `test_float_read.c`. |
| UTF-8 decoder, `src/rt/utf.c` | Yes. | Error. | `test_utf.c`, sequences cut off at the end and lone surrogates. |
| Command line, `anti_split_command_line` of `start.c` | Yes. | No failure case. | Well-formed examples only. |
| ELF, Mach-O and DWARF, `src/rt/symbols.c` | Mostly, through `get`, `take` and `inside`. An ELF `NOBITS` section escapes. | No answer. | None. `symbols_probe.c` reads well-formed binaries. |
| Build id and slide, `src/rt/trace.c` | No, for the two findings below. | Crash. | None. |
| Plugin index and provides table, `src/rt/plugin.c` | The index goes through the TOML reader. The provides table of the library is trusted. | Error through `fail`. | `run_plugin.cmake:92` refuses a wrong digest and a missing library. `run_plugin_versions.cmake` feeds well-formed mismatches. No malformed index or table. |
| JSON tree, `src/anti/jsontree.c` | Yes. Depth is capped at 1024 and each accessor checks the kind. | Error. | None. |
| raylib API, `src/anti/bindapi.c` | No, for a repeated struct. | Error, or a write out of bounds. | `run_anti_bind.cmake` refuses a file that is not JSON. |
| C expressions and types, `bindexpr.c` and `bindtype.c` | No limit on depth. | Error, or a crash on deep input. | `expressions` of `test_bind.c:111`: division by zero, a shift of 64, an open parenthesis. `spellings` holds no bad type. |
| clang AST, `src/anti/bindclang.c` | Yes, apart from an unnamed enum. | Error, or a crash on valid C. | `run_anti_bind.cmake` refuses `pack(2)`, C++ and another clang. No malformed tree. |
| Manifest, lock and index, `manifest.c`, `deps.c`, `repo.c` | Values reach paths and version arithmetic unchecked. | Error. | `manifest_refusals` and `manifest_missing` of `test_deps.c`. No malformed lock or index. |
| Zip, `src/anti/zip.c` | Yes for the central directory and the local headers. Inflate has no output bound. | Error, or exit 70 on out of memory. | None. |
| Symbols maps and traces, `syms.c`, `symmap.c` | `sscanf` with `%llx` and `%lld`. | Line passed over. | None. |
| SDK settings, `src/anti/sdk.c` | The version reaches a path unchecked. | Error. | `run_anti_sdk.cmake:114`, a file that is no bundle. |
| Child output, `src/antic/process.c` | Yes. | Not applicable. | Not applicable. |

## Severe

### Recursive readers with no depth limit

`src/antic/parser.c:1014`, rule 14, severe.

`e->as.unary.operand = unary(p);` recurses once per prefix operator, and
`primary`, `type` and `block` recurse once per `(`, `*` and `{`. The only
depth counter in the file is in `sync_item`. `fn main() { let x = `
followed by 100000 `-` and `1; }` overflows the stack of antic without a
diagnostic. The same shape stands in the readers of `anti bind`:
`src/anti/bindexpr.c:319` and `277` recurse per `-` and per `(` of a
define, and `src/anti/bindtype.c:506` and `569` per `[N]` and per group
of a C type. The library reader recurses per symbolic value in `map_sym`
at `src/antic/antl.c:1944` and per aggregate in `map_agg`. `MAP_BUSY`
stops a cycle, not a chain of 200000 entries. The fix is one depth limit
per reader, set low enough for the passes that walk the tree after it.

### A jump or branch target is not checked to be a block

`src/antic/antl.c:2266`, rule 14, severe.

`read_operand` accepts `case IR_INT: o.as.integer = payload;` for any
instruction, and `operand_ok` of `src/antic/ir_verify.c:91` returns true
for an integer. A `jump` whose operand is the integer 65536 passes both,
and `mark_reachable` at `src/antic/optimize.c:753` then reads and writes
`reached[b]` and `f->blocks[b]` out of bounds. `optimize.c:830` and
`1629` index the same way. The fix is to require `IR_BLOCK` for the
target operands of `jump`, `branch` and `branchov`, in the reader or in
`ir_verify`.

### A function body with no blocks

`src/antic/antl.c:2331`, rule 14, severe.

`blocks = get_count(r, 4);` accepts 0 for a function that has a body.
`ir_verify` checks nothing in a function without blocks, and
`remove_unreachable_blocks` calls `mark_reachable(f, 0, reached)`, which
reads `f->blocks[0]` from a NULL array. The fix is to refuse a body with
no block.

### A member of a class body is not checked to be a function

`src/antic/antl.c:1657`, rule 14, severe.

`m->symbol->type = r->table[s->member_types[j]];` checks the index and not
the kind, while the top-level function at line 1806 requires `TYPE_FN`. A
tuple type carries two parameters and a NULL result, so an operator member
typed `(int, int)` passes the parameter check of `check_operator` and
`sema.c:1969` sets `call->type = sig->result`, which is NULL. The unchecked
kind is certain. The dereference that follows was traced through
`check_operator` and not run. The fix is the `TYPE_FN` check of line 1806.

### Signed division of the least value by minus one in a size

`src/antic/layout.c:352`, rule 3, severe.

`*out = (uint64_t)(signed_value(type, a) / signed_value(type, b));` is
guarded against a zero divisor only, and the reader checks only
`s->op > IR_RET` at `antl.c:2059`. A library whose array length is the
symbolic `sdiv` of `INT64_MIN` by -1 folds to undefined behaviour, a trap
on an x86_64 host. `%` at line 356 is the same. The fix is to refuse that
pair in `fold_op`.

### COFF data of an uninitialised section is read without a range check

`src/antic/coff.c:379`, rule 14, severe.

The range check of the raw data is skipped when the section is flagged
`SCN_UNINITIALIZED`, but `read_sections` at line 452 still takes
`data = o->in->data + get(s->header + 20, 4)` and hands it to
`add_directives` for `.drectve` and to `holds_lines` for `.debug$S`. A
`.drectve` with flags `0x280` and a raw pointer past the file reads outside
it. The inputs are the objects of llvm-mc and of the runtime archive. The
fix is to pass over an uninitialised section in `read_sections`, as the
writer at line 733 does.

### An ELF `NOBITS` section escapes the size check

`src/rt/symbols.c:517`, rule 14, severe.

`return inside(b, out->offset, out->type == 8 ? 0 : out->size);` accepts
any size for a section of type 8, and the callers read that size as bytes
of the file: the section names at line 539, `.strtab` at 576 and the three
debug sections of `anti_elf_line` at 715, since `elf_find` never checks the
type. A `.shstrtab` of type 8 with a size of 1 GiB reads past the file.
The trace code reaches it on a damaged `.so`, and `anti symbols` on any
file it is given. The fix is to refuse `NOBITS` where the bytes are read.

### The DWARF line counter overflows

`src/rt/symbols.c:420`, rule 3, severe.

`r.line += sleb(&unit);` adds a value from the file to an `int64_t`. An
`advance_line` of `INT64_MAX` while the line is 1 is signed overflow.
Line 397 overflows the same way over many special opcodes. The fix is to
check the sum before it is taken, or to keep the line unsigned and
refuse one past `INT64_MAX`.

### The build id of a loaded module is read at an address from the file

`src/rt/trace.c:432`, rule 14, severe.

`return notice_id((const char *)(m->bias + (uintptr_t)vaddr));` reads
memory at the address that `anti_licenses` has in the file now at the
module's path. Nothing checks that the address lies in a `PT_LOAD` of the
mapped module, and `notice_id` then runs `strchr` from it. A plugin `.so`
replaced on disk while the host runs makes the next trace read an
unmapped address. The fix is to check the address against the program
headers that `dl_iterate_phdr` gives.

### The provides table of a plugin is trusted

`src/rt/plugin.c:160`, rule 14, severe.

The loader reads the table `anti_rt_provides` of the library without
checks. `value = value * 10 + (**at - '0');` overflows an `int64_t` on a
package version of 20 digits. Line 383 adds `e->offset` to an object of
`e->class_of->size` bytes without comparing the two, and lines 389 to 393
read and write through the result. Line 441 passes
`(int)table->version_length` as a precision, and a negative one reads to a
NUL the text may lack. The library's own code has already run when
`dlopen` returns, so the practical case is a damaged or mismatched
library, not an attack. The fix is to check each value of the table as
the index values are checked.

### A `[[table]]` counter overflows

`src/rt/toml.c:136`, rule 3, severe.

`repeats` takes the largest number after `name.` with `strtoll`, which
gives `LLONG_MAX` for 20 digits, and returns `seen + 1`. The file
`[a]\n99999999999999999999 = 1\n[[a]]\n` is signed overflow. It reaches
every TOML file the runtime reads: the configuration, the plugin index,
`toml.Document.read` and the log file of `anti.log`. The fix is to refuse
a number at the limit.

### The thread count is read with `atoi`

`src/rt/conf.c:140`, rule 3, severe.

`if (*digit != '\0' || atoi(value) <= 0)` checks only that each byte is a
digit, and `worker_count` at `src/rt/threads.c:120` reads the value with
`atoi` again. C11 leaves `atoi` undefined for a value outside `int`, so
`--anti.threads=99999999999` is undefined. On Windows it gives
2147483647, where the LP64 hosts give another value, so the pool size also
differs between targets. The fix is `strtoll` with a bound.

### A repeated struct of the raylib API writes past its fields

`src/anti/bindapi.c:227`, rule 6, severe.

The second pass of `read_structs` finds the first record of a name for
every entry of that name. For the second entry it allocates
`r->fields` again for that entry's count, but `field_count` keeps the
count of the first, so `&r->fields[r->field_count]` writes past the new
array. Two entries `A` with three fields and then one field do it. The fix
is to refuse a repeated name in the first pass.

### An unnamed enum crashes `anti bind --clang`

`src/anti/bindclang.c:399`, rule 14, severe.

`declare_enum` sets `e->name = entry->tag;`, which is NULL for an enum no
typedef names, and `enum_named` calls `strcmp(e->e->name, name)` on every
entry without that check. The valid header
`enum { F = 1 }; enum Mode { M0 }; void f(enum Mode m);` passes NULL to
`strcmp`. Lines 976 and 1270 already test the name for NULL, and the fix
is the same test here.

### Names and versions from a repository reach cache paths unchecked

`src/anti/repo.c:300`, rule 14, severe.

`text_appendf(&directory, "/pkg/%s/%s", name, version);` is followed by
`make_dirs`, and line 304 appends `module` to form the file that `fetch`
writes. `name`, `version` and `module` come from an index fetched over
https at `deps.c:512` to `532`, or from a cloned `anti.lock`, and nothing
refuses `..`, `/` or an absolute part. An index with
`version = "../../../../tmp/x"` makes directories and writes a `.antl` of
its choosing outside the cache, and the digest does not stop it, since the
same index carries it. The guidelines name no severity for a write outside
the directory a reader owns. It is counted as severe because it is the
file-system form of a write outside an object. `index_dir` at line 246
does the same with `name`, and `src/anti/sdk.c:161` puts the `Version` of
`SDKSettings.json` into the path that `remove_tree` deletes. The fix is to
refuse a name, version or module path that is not a plain part.

### Version numbers overflow

`src/anti/deps.c:60`, rule 3, severe.

`value = value * 10 + (**p - '0');` has no bound, and `next_major` at
line 90 adds 1 to the result. A version of 20 digits in `anti.toml`, a lock
or an index is signed overflow. `long` is 32 bits on Windows, so the
comparison of two large versions also differs between targets. The fix is
`int64_t` and a refusal at the limit.

### `sscanf` converts numbers of foreign text

`src/anti/syms.c:961`, rule 3, severe.

`sscanf(text, "%llx-%llx %511s %1023s", ...)` reads a map line of a
symbols archive, and lines 1148 and 1152 read `%lld` and `%llx` from the
trace a user hands to `anti symbols resolve`. C11 7.21.6.2 leaves a
conversion undefined when the value does not fit, so a map line
`fffffffffffffffffffff-0 f` is undefined. `src/antic/applesdk.c:25` reads
`MacOSX%d.%d%7s` from the names of the SDK directory the same way. The C
libraries of the hosts clamp in practice. The fix is `strtoull` and
`strtoll` with an end pointer and a check of `errno`.

### Counters and lengths narrowed to `int`

`src/antic/lexer.c:217`, rule 16, severe.

`lx->line++` and `lx->column++` count in `int`, so a line of 2^31 bytes is
signed overflow, and `read_source` bounds no size. `uleb` and `sleb` of
`src/rt/symbols.c:98` and `117` count the shift in `int`, which overflows
after about 300 million continuation bytes. `src/rt/toml.c:296` and `397`
pass `(int)name_length` as a precision, and a key of 2^31 bytes gives a
negative precision, which reads the key past its end. `src/antic/driver.c:2280`
narrows the length of a plugin index block the same way. Each needs an
input of hundreds of megabytes. The fix is `size_t` or `int64_t` for the
counters and a bound on the length before the narrowing.

## Major

### A DWARF 5 entry table without formats loops for its count

`src/rt/symbols.c:369`, rule 14, major.

`for (j = 0; j < count && !unit.bad; j++)` reads nothing per pass when
`formats` is 0, so a unit with no directory formats and a count of 2^64-1
never ends. `file_name` at line 248 has the same loop. A program asking
for its trace, or `anti symbols`, hangs. The fix is to refuse a non-zero
count with no formats.

### The library reader leaves its checks to a verifier that does not run

`src/antic/driver.c:1972`, rule 14, major.

`compile_library_file` calls `back_end(o, NULL, ...)`, and with no tree
`lower_checked`, the only caller of `ir_verify` before the passes, is
skipped. The IR of a library file then reaches the passes over the whole
program and the optimizer with no check of its terminators or of the
argument count of each call against its callee. `anti build` takes that
path for every dependency in dev mode, including files fetched over https.
The fix is to run `ir_verify` after `load_libraries` on this path.

### Bitfield widths are not checked against their type

`src/antic/antl.c:1503`, rule 14, major.

`s->fields[j].bits = get_u8(r);` and the IR field at line 2089 take any
width. An `i8` field of 40 bits reaches `bit_unit` at
`src/antic/layout.c:166`, where `b->unit_offset = out->size - bytes`
wraps when the aggregate is smaller than the unit. The compiler's memory
stays safe, since `write_int` bounds each write by the global's size, but
the emitted program then reads and writes outside the object. The fix is
the width check of `sema.c` in the reader.

### Array sizes and alignments from a library overflow

`src/antic/layout.c:218`, rule 5, major.

`bit = length * layout_size(l, element) * 8;` multiplies a length from
the file without a check, so 2^61 gives an aggregate of a few bytes. The
IR alignment at `antl.c:2080`, `t->align = get_u64(r);`, is not checked to
be a power of two as the type alignment is at line 1489, and a large one
wraps `round_up`. The fix is an overflow check on the product and the
power-of-two check on the alignment.

### A repeated member of `Object.deserialize` leaks the first

`src/rt/registry.c:686`, rule 14, major.

`fill` reads each member into its field without asking whether the member
came before. `{"type":"Holder","child":{"type":"C"},"child":{"type":"C"}}`
builds the first `C`, runs its constructor, stores it, and then overwrites
the field with the second. The first is never destroyed and its block
never goes back to the allocator. A repeated `str` or owned slice leaks
the same way. The fix is to refuse a member seen before.

### Inflate has no bound on its output

`src/anti/zip.c:587`, rule 14, major.

`inflate(data, item->packed, out) && out->length - from == item->size`
compares the declared size after the whole stream is decoded. A deflate
stream expands about 1000 to 1, so a 10 MB entry declared as 10 bytes
grows `out` until `text_append_bytes` exits with 70 on out of memory. The
fix is to stop once the output passes `item->size`.

### `ANTI_CONF` is ignored on Windows past 1023 bytes

`src/rt/conf.c:592`, rule 14, major.

`static char text[1024]` and `GetEnvironmentVariableA` return NULL for a
longer value, so a long path is passed over in silence, where POSIX reads
it. The ANSI code page also differs from the UTF-8 that `anti_rt_fs_open`
decodes, so a path outside ASCII fails on Windows alone. The result
differs between targets. The fix is `GetEnvironmentVariableW` with a
buffer of the length it reports.

### The TOML reader leaks a key on out of memory

`src/rt/toml.c:109`, rule 12, major.

`pair->key = keep(key, key_length);` and `pair->value = keep(...)` run
before the NULL check, and a failure of one returns without `doc->count++`,
so `anti_rt_toml_free` never frees the other. The fix is to free both on
that path.

### The zip reader leaks the name of a broken entry

`src/anti/zip.c:256`, rule 12, major.

`text_append_bytes(&item->name, p + at + 46, name_length);` runs before
the check of the data range, and the failure returns without
`out->count++`, so `zip_archive_free` never frees that name. The fix is
to take the name after the check.

### Fifteen readers have no malformed-input test

Rule 15, major. The guidelines name no severity for a missing test. This
report counts the gap as one major finding. Most readers in the list carry
a severe finding above that such a test would have found.

- The COFF reader of `coff.c`.
- The SDK directory names of `applesdk.c`.
- The command-line splitter of `start.c`.
- The ELF, Mach-O and DWARF readers of `src/rt/symbols.c`.
- The build id and slide readers of `src/rt/trace.c`.
- The plugin index and the provides table of `src/rt/plugin.c`.
- The JSON tree of `src/anti/jsontree.c`.
- The raylib API reader of `bindapi.c`.
- The clang AST reader of `bindclang.c`.
- The C type parser of `bindtype.c`.
- The lock and index readers of `deps.c` and `repo.c`.
- The zip reader of `zip.c`.
- The map and trace parsers of `syms.c` and `symmap.c`.
- The source reader `read_source`, for a NUL byte.
- The SDK settings reader of `sdk.c`, beyond a file that is no bundle.

The parser, the library reader, the TOML reader, the runtime
configuration, `Object.deserialize` and the expression evaluator of
`bindexpr.c` have tests. None of them feeds deep, oversized or
out-of-range input. The library test cuts one file of no aggregates,
symbols or classes, so its prefixes never reach those tables.

## Minor

### The doc marker reads one byte past its token

`src/antic/parser.c:189`, rule 6, minor.

`return s[2] == '#' && s[3] == '!' ? 4 : 3;` reads the fourth byte of a
token that may be three bytes long, when a file ends in `//#`. It stays in
bounds only because `read_source` ends the text with a NUL, and `parse`
takes no length. The fix is to pass the token length.

### The long section name of COFF is read with `strtoul`

`src/antic/coff.c:357`, rule 14, minor.

`strtoul((const char *)h + 1, NULL, 10)` scans past the 8 bytes of the
name field while digits follow. The NUL that `struct text` keeps stops it.
`unsigned long` is 32 bits on Windows, and every large value is refused
either way. The fix is to parse the seven digits with a bound.

### The Mach-O slide walks load commands unchecked

`src/rt/trace.c:228`, rule 14, minor.

`image_slide` checks neither `cmdsize >= 8` nor the bound of
`sizeofcmds`. The image is the one in memory, which dyld has checked, and
`symbols.c` checks the same walk. The fix is the check of `symbols.c:178`.

### A NUL in a configuration path cuts it short

`src/rt/conf.c:750`, rule 14, minor.

`anti_rt_conf_configure` copies `length` bytes, and `file_bytes` at line
327 opens the copy by `strlen`, so `rt.configure("a.toml\0x")` opens
`a.toml`. The refusal of a NUL in `native_path` of `fs.c` is passed by.
The fix is to pass the length on.

### Implementation-defined conversions of input values

`src/rt/registry.c:451`, rule 3, minor.

`out->len = (int64_t)value;` converts an `unsigned long long` above
`INT64_MAX` to a signed type, which C11 leaves to the implementation.
`src/anti/bindexpr.c:438` right-shifts a negative `a.i`, also left to the
implementation. Every host gives the same result, so neither is major. The
fix is a range check before each.
