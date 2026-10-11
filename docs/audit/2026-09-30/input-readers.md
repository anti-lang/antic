# Input readers

The second audit of every piece of code that reads bytes it did not write,
against rules 14 and 15 of `docs/c-guidelines.md`, with rules 3, 4, 5, 6 and
16 where they bear on reading, and the structure questions of the audit
where two readers of one format meet. It covers `src/rt/`, `src/antic/` and
`src/anti/` at `238a4f1`. `src/native/` holds no C file of its own: its
recipes compile `src/rt/regex.c` and `patterns.c`, which are read here.
Each reader was read by hand, and every length, offset, count and index it
takes from its input was followed to its first use. Four foreground
reviewers read the library reader, the runtime, the parser and the anti
tool in parallel, and each item they gave was read again in the code before
it went in. Nothing was built or run, so every consequence below is traced,
not run. The first audit's report is `docs/audit/2026-09-23/input-readers.md`
at `fddd434`, and its findings are named by the ids of
`docs/audit/2026-09-23/summary.md`. The findings of `docs/audit/tool-pass.md`
are not repeated: the unchecked `otherwise_at` and NULL references of the
generic trees, and the simd lane of no bytes. The first severe finding below
adds places to the first of those.

Collections read no bytes from outside: `deserialize` of a collection is not
built. Generics brought two readers, the generic syntax of the parser and the
tree section of the library file. Serialize of variants, tuples, optional
values and arrays brought the new cases of `Object.deserialize`. The new
sections of the library format are the generic declarations, the externs
and the trees. The type and IR tables gained the tuple, optional, variant,
injection, provides and relocation records.

## Counts

| Severity | Rule or question | Findings | New | Still open |
|---|---|---|---|---|
| Severe | 14 | 5 | 5 | 0 |
| Severe | 3 | 2 | 2 | 0 |
| Severe | 4 | 1 | 1 | 0 |
| Major | 14 | 4 | 4 | 0 |
| Major | 15 | 1, covering 12 readers | 0 | 1 |
| Major | Two copies that drifted | 4 | 4 | 0 |
| Minor | Two copies that drifted | 1 | 1 | 0 |
| Minor | 14 | 1 | 1 | 0 |
| Minor | 16 | 1 | 1 | 0 |
| Minor | 3 | 1 | 1 | 0 |
| Total | | 21 | 20 | 1 |

No finding returned. Of the 33 findings of the first report, 32 are fixed at
the lines they named. The one still open is the missing tests, M31, in part.

## The first audit's findings

| First finding | Now |
|---|---|
| No depth limit in the parser, `bindexpr.c`, `bindtype.c`, `map_sym` and `map_agg` | Fixed. `PARSE_DEPTH_MAX` at `parser.h:15`, `EVAL_DEPTH`, `TYPE_DEPTH` and `NEST_LIMIT` at `antl.c:1587`. Three paths of the parser and lexer stand outside the limit, a new finding below. |
| A jump to an integer, a body of no blocks, a member not a function | Fixed in `7a48c10`: `targets_blocks` at `antl.c:3160`, `blocks == 0` at `3127`, `TYPE_FN` at `2330`. |
| `INT64_MIN / -1` in a size | Fixed at `layout.c:401`. |
| COFF data of an uninitialised section, the long section name | Fixed at `coff.c:459` and `351`. |
| ELF `NOBITS`, the DWARF line counter, the shift counters of LEB128, the entry table without formats | Fixed in `88b9365`: `elf_section_bytes` at `symbols.c:610`, `advance_line` at `344`, the shift at `154`, the checks at `307` and `444`. |
| The build id at an unchecked address, the Mach-O slide | Fixed: `anti_rt_elf_loaded_room` at `trace.c:493`, `macho_commands` at `symbols.c:864`. |
| The provides table, S38 | Fixed for the values it named, `plugin.c:466` to `517`. The lists of a class descriptor stay unchecked, a new finding below. |
| The `[[table]]` counter, the key length, the key leak | Fixed at `toml.c:148`, `245` and `117`. |
| `threads` by `atoi`, `ANTI_CONF` on Windows, a NUL in a configured path | Fixed: `anti_rt_conf_threads` at `conf.c:916`, `anti_rt_getenv`, `conf.c:905`. |
| The repeated member of `Object.deserialize`, the length converted to `int64_t` | Fixed in `fd0afd8`, `registry.c:831` and `475`. |
| The repeated raylib struct, the unnamed enum | Fixed at `bindapi.c:211` and `bindclang.c:447`. |
| Repository names and versions in cache paths, the SDK `Version` | Fixed: `repo_name_valid` and `deps_version_valid` at `repo.c:336`, `sdk_version_valid` at `sdk.c:170`. |
| Version numbers of `deps.c`, `sscanf` in `syms.c` and `applesdk.c` | Fixed: `version_part` at `deps.c:40`, `cursor_number` at `syms.c:930`, `version_number` at `applesdk.c:11`. |
| Counters narrowed to `int` in the lexer and the plugin index block | Fixed by the 64 MiB cap, `LEX_SOURCE_MAX`, at `lexer.c:1483` and `driver.c:127`, and `size_t` at `driver.c:2667`. |
| The library IR not verified, bitfield widths, array sizes and alignments | Fixed at `driver.c:2355`, `antl.c:2298` and `2872`, `layout.c:258`, `antl.c:2125` and `2857`. |
| Inflate without a bound, the name leak of a broken entry | Fixed per entry at `zip.c:301` and `246`. The total of an archive stays unbounded, a new finding below. |
| The doc marker past its token, the precision of a TOML key, `(int64_t)` of a length, the right shift in `bindexpr.c` | Fixed at `parser.c:247`, `toml.c:245` and `bindexpr.c:537`. |
| Fifteen readers without a malformed-input test, M31 | Still open in part: four of the fifteen, and two of the thin tests it named, below. |

## The readers

"Bounds" says whether each length and offset is checked before the read.
"Refusal" says how bad input ends. "Tests" names the malformed-input tests,
with "new" for those added since `fddd434`.

| Reader | Bounds | Refusal | Tests |
|---|---|---|---|
| Lexer, `lexer.c` | Yes, with a cap of 64 MiB. Nested f-strings recurse without a limit. | Diagnostic, or a crash on deep input. | `test_lexer.c`, with the cap new. `run_source_cap.cmake` and `errors/long_float.anti`, new. |
| Parser, `parser.c` | Lookahead yes. A depth limit, which three paths pass by. | Diagnostic, or a crash. | `depth_limit` at `test_parser.c:124`, new. Nothing nests types or `<`. |
| Source file, `read_source` of `driver.c` | Yes: the cap, and a NUL refused. | Diagnostic. | The cap by `run_source_cap.cmake`, new. No NUL. |
| Library tables, `antl.c` | Yes, for every section the first audit read and every new record of the tables. | Error. | `damaged_files` at `test_modules.c:1417`, every prefix, `damaged_records` at `1649`, `deep_tables` at `2019`, `run_antl_verify.cmake`, all new or grown. |
| Generic trees, `antl_tree.c`, new | Encoding only. No relation inside a tree, no count against the file, no depth. | Error, a crash, or exit 70. | None. `test_modules.c:1233` round-trips generics and damages none. |
| COFF, `coff.c` | Yes. | Error. | `test_coff.c:476`, `527`, `563`, `630`, new. |
| SDK names, `applesdk.c` | Yes. | Passed over. | `sdk_names` at `test_link.c:666`, new. |
| Pattern literals, `pattern.c` | Yes, after PCRE2 accepts the pattern. | Diagnostic. | `errors/pattern_malformed.anti`, `run_pattern_limit.cmake`, new. |
| TOML, `toml.c` | Yes. | NULL. | `table_repeat_limit` and `key_length` of `test_toml.c`, new. |
| JSON scanner, `json.c` | Yes. | Error. | Complete, as before. |
| Runtime configuration, `conf.c` | Yes. Includes fan out without a bound on the work. | Exit 70 by decision. | `tests/conf/`: `threads-range`, `threads-zero`, `seed-range`, `seed-text`, `nul-value`, `configure-nul`, new. No file that is not TOML. |
| `Object.deserialize`, `registry.c` | Yes, with the variant case taken from the descriptor and each array level checked. The descriptor of a plugin class is trusted. | NULL, with the undo. | `deserialize_undo.anti:158`, `serialize_tuples.anti:77`, new. No variant, no `?T`, no text cut off. |
| Regex glue, `regex.c`, `patterns.c`, new | Offsets from PCRE2 yes. The patch offset overflows. | Error. | `programs/regex_compile.anti`, `pattern_bytes.anti`, new. No open template, no large offset. |
| Float, UTF-8, command line | Yes. | Error, or no failure case. | As before. The splitter has Microsoft's examples alone. |
| ELF, Mach-O and DWARF, `symbols.c` | Yes. | No answer. | `test_symbols.c:337` to `574`, new. The Mach-O symbol table, debug map and object lines have none. |
| Build id and slide, `trace.c` | Yes. | No answer. | `loaded_room`, `macho_text`, new. |
| Plugin index and provides table, `plugin.c` | The table yes. The function and field lists of its classes no. | Error. | `test_plugin_table.c:195` to `270`, new. No damaged list. The index: a wrong digest and a missing library only. |
| JSON tree and `anti bind` readers | Yes. | Error. | `deep_input` at `test_bind.c:197`, `api_malformed`, `clang_malformed` and `ast_malformed` of `run_anti_bind.cmake`, new. |
| Manifest, lock and index | Yes. | Error. | `version_bounds`, `path_parts`, `urls` of `test_deps.c`, `run_anti_build_inputs.cmake`, new. |
| Zip, `zip.c` | Yes per entry. The total output has no bound. | Error, or exit 70. | `test_zip.c:152` to `517`, new. |
| `anti symbols`, `syms.c`, `symmap.c` | Yes, apart from a path the archive names. | Error, or exit 70. | `test_syms.c`, `run_anti_symbols_input.cmake`, new. |
| SDK settings, `sdk.c` | Yes. | Error. | The hostile `Version` of `run_anti_sdk.cmake`, new. |

## Severe

### The tree reader takes case numbers and lanes without their type

`src/antic/antl_tree.c:756`, rule 14, severe, new.

`io_u32(io, &a->variant_case);` takes any case number for a `switch` arm,
and lines 1045, 1066 and 1076 take the case of a cast and a struct literal
and the `enum_value` of a field the same way. A copy of a generic keeps them
unless its node depends on a type argument, and lowering indexes with them:
`&variant->base->fields[at->variant_case - 1]` at `lower_stmt.c:2278` and
`e->type->fields[e->as.field.enum_value - 1]` at `lower_expr.c:2992`. An arm
with case 65536 over a variant of two cases reads outside the fields. The
shuffle of a simd struct is the same: line 1157 reads a lane count of its
own, and `lower_simd.c:469` reads `e->as.simd.lanes[k]` for every field of
the struct, so a count of 1 on a struct of 8 lanes reads 7 values past the
array, and a lane of 1000 reaches `t->fields[1000]` in `lane_at`. This is the
gap of the tool pass's finding at `antl_tree.c:1258`, at four more places.
The fix is the one named there, a check of each tree after reading.

### A tree can name itself, and the copy recurses without end

`src/antic/antl_tree.c:526`, rule 14, severe, new.

`io_ref` takes any index into the tables of a tree, its own node included,
and no check follows. `xb` and `xs` of `sema_copies.c` keep no map of the
nodes they copied, so a block whose statement is a `STMT_BLOCK` naming that
block makes `n->stmts[i] = xs(cl, b->stmts[i]);` at `sema_copies.c:1476`
recurse until the stack overflows, for any use of the generic with concrete
arguments. A chain of 100000 nested blocks, a few MB of file, does the same
without a cycle. The tables of the file have `NEST_LIMIT` since the first
audit, and the trees have no limit. The fix is a depth limit and a check that
each reference points to a later record, in the reader.

### Three paths of the parser and lexer pass by the depth limit

`src/antic/parser.c:3272`, rule 14, severe, new.

`descend` stands in `type`, `unary`, `??`, `if_statement` and `statement`,
and three recursions reach none of them:

- A class body declares a type through `nested_type`, which calls
  `inner = item(p);`, and `item` reaches `class_item` and `nested_type`
  again. `class A {` 100000 times, 1 MB of source, overflows the stack.
- The scan that decides whether `<` opens type arguments recurses in
  `scan_type` at `parser.c:551` and `564`. `let x = a<` followed by 200000
  `(` recurses once per `(`, and `a<a<a<` once per name.
- `placeholder` of `lexer.c:1244` calls `lex_token(&inner)`, which reaches
  `interpolated` and `placeholder` again, once per nested f-string. Each
  level needs one more `#`, so the cap of 64 MiB allows about 11000 levels.
  That passes the stack of 1 MiB of Windows and of the sanitizer builds. An
  8 MiB stack may hold it.

The first audit's limit covers the grammar it read. Nested types and the
generic syntax came later. The fix is `descend` in `nested_type` and in the
scan, and a depth count in the lexer.

### A closure in an f-string reads a NULL token map

`src/antic/parser.c:981`, rule 14, severe, new.

`placeholder` parses the tokens of `{expr}` with `inner.origin = NULL;`, and
the expression may be an anonymous function whose body is parsed by
`statement_level`. Its `for` at line 2424 and its `assert` at line 2470 read
`p->all[p->origin[p->pos - 1]]`. `f"{fn() { for x in a { } }}"` then
dereferences NULL. The fix is a map for the inner parser, or a test for
NULL where the text of the source is taken.

### The allocation helpers of `anti` return NULL where the header says they exit

`src/anti/files.c:38`, rule 4, severe, new.

`items = calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);` is returned
unchecked, and `grown = realloc(...)` at line 49 as well, while `files.h:13`
says a failed calloc exits through `files_out_of_memory`. `files_grow`
then runs `memset(bytes + *room * size, 0, ...)` on NULL at line 65, a
write at an address from zero, and a failed realloc leaks the old block.
Commit `d20c08a` introduced the helpers without the check, replacing calls
that had one. Every reader of `anti` allocates through them, the zip
directory at `zip.c:216` and the lists of `syms.c`, `manifest.c` and
`deps.c` among them, so a file large enough to exhaust memory reaches it.
The fix is the NULL check the header describes.

### The patch offset overflows before its bound

`src/rt/patterns.c:753`, rule 3, severe, new.

`if (at + with_length > end - start) {` adds an offset the program passes to
the length of `with`, and only `at < 0` is refused before it. An offset of
`INT64_MAX`, as `text.parse_int` reads it from input, is signed overflow,
and on a host that wraps the sum is negative, the check passes and
`memcpy(data + start + at, with, ...)` at line 758 writes far outside the
text. `anti_rt_bytes_patch` has the same sum at line 802 before its
`memcpy` at 823. The fix is `at > span - with_length`, which cannot
overflow.

### The function and field lists of a plugin class are trusted

`src/rt/plugin.c:616`, rule 14, severe, new.

`class_sound` checks the name, the version and the size of a class
descriptor of the library. `anti_rt_plugin_supports` then walks
`e->class_of->functions[i]` for `function_count` entries, and
`Object.deserialize` of a plugin class walks `d->fields` and `d->parent` in
`field_named` at `registry.c:330`. A descriptor with a count of 1000 over one
entry reads past it. The file's `DESIGN` comment says the loader checks every
count and pointer of the table. The case is a damaged or mismatched library,
as for S38 of the first audit, which named other values of the same table.
The fix is to check the lists in `class_sound`.

### The stamp of a cached index overflows in a subtraction

`src/anti/repo.c:248`, rule 3, severe, new.

`then = strtoll(...)` takes the stamp beside a cached index, and
`now - then < REPO_INDEX_SECONDS` runs after `now >= then`. A stamp of
`-9223372036000000000` is in range, and the difference passes `LLONG_MAX`.
The stamp is a file of the user's cache that `anti` writes, so a damaged
cache is the case, not a repository. The fix is to refuse a stamp below 0.

## Major

### The counts of a tree are not measured against the file

`src/antic/antl_tree.c:688`, rules 14 and 5, major, new.

`io_count` reads each list count through `io_size`, a plain u32, where the
tables use `get_count`, which refuses a count larger than the rest of the
file. `antl_allocate(io->r, *count, size)` then asks the memory pool for up
to 2^32 elements, which `arena_alloc` zeroes, or ends antic with exit 70. Every
list of a tree takes this path: statements, arguments, arms, parameters,
captures and format parts. `read_tree` at line 2000 bounds its six table
counts at one byte per record, and then allocates a whole `struct expr` or
`struct symbol` per record, so a file of N bytes still asks for many times N.
The fix is `antl_get_count` with the least size of each element.

### The scan of `<` takes cubic time

`src/antic/parser.c:88`, rule 14, major, new.

`peek_at` walks `ahead` tokens from the parser's position on each call, and
the scan of `<` calls it once per token it passes, so one scan of L tokens
costs L^2 steps. `primary` starts a scan at every name followed by `<`, and a
failed scan leaves the parser one name further on, so `a<a<a<` of N names
costs N^3 steps. 20000 names, 40 KB of source, stall antic for hours before
the stack overflows. The fix is a scan that keeps its own index into the
tokens.

### The unpacked size of an archive has no bound

`src/anti/zip.c:232`, rule 14, major, new.

The first audit's fix stops each entry at its declared size, which the
archive itself gives, up to 4 GiB. `local + 30 > at` requires each local
header to stand before its central entry and not apart from the others, so
65535 central entries may name one local header. `syms.c:647` unpacks every
entry of a symbols archive and keeps them all. About 4 MB of deflate data
declared as 4 GiB and named 65535 times grows the unit until
`text_append_bytes` ends `anti symbols` with exit 70. The fix is to refuse
entries whose data overlap and to cap the total of an archive.

### A path from an archive is read without a bound

`src/anti/syms.c:1113`, rule 14, major, new.

`anti_rt_macho_debug_map` gives the object path of the debug map, an `N_OSO`
string of the `.debug` twin in the archive, and `files_read(object, &file)`
reads it whole. An archive whose twin names `/dev/zero` grows the buffer
until exit 70, and a FIFO or `/dev/stdin` stops the command. The object path
is meant to name a file where the program was built. The fix is to refuse
what is no regular file, or to read the object with a size cap.

### Readers without malformed-input tests

Rule 15, major, still open. M31 of the first audit listed fifteen readers.
Eleven have tests now, two in part, and two none. With the new readers,
these twelve lack one:

- The generic trees of the library file: no damaged declaration, extern,
  reference, count, case number or lane. New.
- The variant and `?T` cases of `Object.deserialize`:
  `std/serialize_variants.anti` round-trips only. New.
- A text of `Object.deserialize` cut off at the end. Still open.
- The function and field lists of a plugin class. New.
- The plugin index, beyond a wrong digest and a missing library. Still open.
- A runtime configuration that is not TOML. Still open.
- The Mach-O symbol table, the debug map and the object lines of
  `symbols.c`. `macho_text` tests the load commands alone. Still open in
  part.
- The patch offset and a template left open, `${` or a `$` at the end, in
  the regex glue. New.
- `read_source` with a NUL byte: no test matches "contains a NUL byte".
  Still open.
- The command-line splitter, with Microsoft's examples alone and no input
  that ends in a backslash. Still open.
- Nested types, the scan of `<` and nested f-strings in the parser and the
  lexer, which `depth_limit` does not reach. New.
- An archive whose entries overlap, for `zip.c`. New.

### The tree reader grew its own primitives, and they drifted from the tables

`src/antic/antl_tree.c:280`, two copies that drifted, major, new.

`antl.c` holds the primitives of the format and the checks the passes rely
on. `antl_tree.c` reads through them but adds its own layer, and each
piece of it has lost a check the original has. `io_size` and `io_count` skip
the bound of `get_count`, the major finding above. `io_type` at line 351 is
a second `type_ref` with a sentinel of its own. `read_extern` at line 1469
takes a symbol kind and a type without tying the two, where the member
reader at `antl.c:2330` requires `TYPE_FN` since S12 of the first audit. The
IR of the file has a verifier after reading, `ir_verify`, and the trees have
none.
The fix is one reader layer: the tree reader calls the checked primitives,
and a verifier of the trees follows it.

### `anti symbols` reads the runtime configuration with a copy of the runtime's walk

`src/anti/syms.c:312`, two copies that drifted, major, new.

`read_configuration` follows the includes of a runtime configuration as the
runtime's `read_file` at `src/rt/conf.c:690` does, and the two disagree. The
runtime refuses a chain past 32 files that include each other,
`if (depth > 32)` at line 707, which lets 33 files stand. `anti` refuses at
`depth >= INCLUDE_DEPTH` with 32, one file fewer, and reports it as "includes
itself". A deployment of 33 files starts, and `anti symbols inventory`
refuses its configuration. The fix is one walk that both call, or one
constant and one message in both.

### Deserialize reads a JSON integer with its own conversion

`src/rt/registry.c:386`, two copies that drifted, major, new.

`read_integer` hands the raw span of `anti_rt_json_number` to `strtoll` and
`strtoull`, and the span accepts every digit, sign, `.` and `e` in any order.
`{"type":"P","x":+5}` and `"x":007` then read as 5 and 7, while
`anti_rt_json_integer` of `json.c:199`, which `anti bind` calls, refuses both
as no JSON number. The fix is `anti_rt_json_valid_number` before the
conversion, as `jsontree.c:231` does.

### antic reads the plugin index with `strstr`, and the runtime with TOML

`src/antic/driver.c:2664`, two copies that drifted, major, new.

`index_others` keeps the entries of other libraries when antic writes
`anti-plugins.toml`, and it finds them with `strstr` of `"[[library]]\n"` and
of `"\npath = '%s'\n"` at line 2641. The runtime reads the same file with
`toml.c`, which accepts `\r` at line 65 and a path in double quotes. An index
with CRLF line ends, as a checkout on Windows may give it, loads in the
runtime, and the next `antic --lib shared` into that directory finds no
block and drops every other library from it. A path in double quotes keeps
the stale entry of the library beside the new one. The fix is to read the
index with the TOML reader the runtime uses.

## Minor

### A version is compared two ways

`src/rt/plugin.c:129`, two copies that drifted, minor, new.

`version_below` compares up to 8 parts as digit strings, and a part that is
no number counts as 0. `deps_version_compare` at `src/anti/deps.c:84`
compares 3 parts as integers capped at 999999999. They agree on the versions
`deps_version_valid` accepts, but `package.version` of `anti.toml` is read at
`manifest.c:321` without it and goes to `antic --package-version` and into
every class descriptor. `1.0.0.1` is then above `1.0.0` for the floor of a
plugin and equal to it for the resolver. The fix is to check
`package.version` with `deps_version_valid`.

### Includes of a configuration fan out without a bound on the work

`src/rt/conf.c:700`, rule 14, minor, new.

Both walks refuse a cycle on the current chain alone. 32 files that each
include the next ten times ask for 10^32 reads, so a program at start, or
`anti symbols` at `syms.c:338`, never ends. The fix is a count of files read
per configuration.

### Lengths of input narrowed to `int` for a precision

`src/rt/plugin.c:791`, rule 16, minor, new.

`(int)name.len` and `(int)built.len` of the plugin index, and
`(int)(c.end - c.at + 1)` of a trace line at `src/anti/syms.c:1418`, narrow a
length taken from a file. The TOML reader bounds keys and not values. A
value past `INT_MAX` bytes gives a negative precision, and the NUL each reader
keeps stops the read.

### Implementation-defined conversions in the tree reader

`src/antic/antl_tree.c:247`, rule 3, minor, new.

`*v = (int)u;` of `io_int`, `(int32_t)u` of `io_i32` at line 251 and
`(int)antl_get_u32(r)` for a line at 1661 convert a u32 above `INT_MAX`, which
C11 leaves to the implementation. Every host gives the same value. The fix
is a range check before each.
