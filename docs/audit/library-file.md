# The library file and the shared leaf code

This step audited the library file of antic and the leaf code under it.
The files are `antl.c`, `antl_io.c`, `antl_read.c`, `antl_read_ir.c`,
`antl_tree.c`, `antl_verify.c` and `antl_write.c` with `antl.h` and
`antl_io.h`, and `alloc.c`, `arena.c`, `text.c`, `ptrset.c` and `sha256.c`
with their headers. The three readers of the runtime, `src/rt/json.c`,
`toml.c` and `regex.c`, and `src/anti/jsontree.c` belong to it as well.
Every line of the 27 files was read at `69fc7bee`, against the three
standards of the brief, with rule 14 of `docs/c-guidelines.md` first. Each
length, offset, count and index a reader takes from its input was followed
to its first use. Where a reader hands its result on, the use was read in
`types.c`, `ir.c`, `layout.c`, `sema_copies.c` and the lowering files. The
section "Files of the library file" of `docs/decisions.md` and the notes
`modules.md`, `versions.md` and `direct-imports.md` were read against the
code. The machine evidence of `docs/audit/data/` was read for these files,
and each item was checked in the code. No build ran and no suite ran, so
every consequence below is traced and not run. Each finding names the
standard, the rule or the question, and the severity. It says whether the
first audit in `docs/audit/2026-09-23/` or the second in
`docs/audit/2026-09-30/` reported it. The findings of
`docs/audit/tool-pass.md` and `docs/audit/structure.md` for these files are
not repeated. Their state stands under "The tool pass and the structure
report in this area".

## Counts

| Severity | Standard | Findings | New | Returned | Still open |
|---|---|---|---|---|---|
| Severe | Safety, rule 14 | 2 | 2 | 0 | 0 |
| Severe | Documentation | 1 | 1 | 0 | 0 |
| Major | Safety, rule 15 | 1 | 1 | 0 | 0 |
| Major | Structure | 2 | 2 | 0 | 0 |
| Major | Documentation | 2 | 2 | 0 | 0 |
| Minor | Safety, rules 5, 12, 14, 18 and 26 | 9 | 8 | 0 | 1 |
| Minor | Structure | 3 | 3 | 0 | 0 |
| Minor | Documentation | 3 | 3 | 0 | 0 |

In all: 23 findings, 22 new and 1 still open. None returned.

## The readers

"Bounds" says whether each length, offset, count and index is checked before
its use. "Refusal" says how bad input ends. "Tests" names the
malformed-input tests.

| Reader | Bounds | Refusal | Tests |
|---|---|---|---|
| Library tables, `antl_read.c` and `antl_read_ir.c` | Yes for every count and index. No limit on how deep a pointer or an array nests, the second severe finding. | `is damaged at byte N`, or a named need. | `damaged_files`, `damaged_records`, `damaged_simd`, `damaged_constants`, `damaged_relocations` and `deep_tables` of `test_modules.c`. |
| Generic trees, `antl_tree.c` and `antl_verify.c` | Yes for counts, references, depth, case numbers and lanes. No for the name of a field, the first severe finding. | The same message. | `tree_counts`, `truncated_trees`, `signed_numbers` and `damaged_trees` of `test_modules.c`. |
| JSON scanner, `src/rt/json.c` | Yes. | False. A number outside the grammar passes `anti_rt_json_skip`, the third severe finding. | The six groups of `test_json.c`. |
| TOML, `src/rt/toml.c` | Yes. The room of a path, 320 bytes, bounds the nesting of inline tables and arrays. | NULL. | The six cases of `test_toml.c`. |
| Regex glue, `src/rt/regex.c` | Yes. | An error number and an offset. | `programs/pattern_bytes.anti`, `programs/regex_compile.anti` and `errors/pattern_bytes.anti`. No pattern cut short. |
| JSON tree, `src/anti/jsontree.c` | Yes, with a depth of 1024. | A message with line and column. | `deep_input` of `test_bind.c` and the cases of `run_anti_bind.cmake`. |

## Severe

### The verifier does not hold a field name to its struct

`src/antic/antl_verify.c:282`, safety, rule 14, severe, new.

The tree of a generic carries the name of each field expression as free
text, read by `io_name(io, &e->as.field.name);` at `antl_tree.c:1011`. The
verifier asks of an `EXPR_FIELD` that its base is set and its `enum_value`
is in range, and nothing of the name. The copy pass copies the node as it
stands at `sema_copies.c:1092`. Lowering then runs
`index = (uint32_t)(types_find_field(s, name) - s->fields);` at
`lower.c:902`, where `types_find_field` gives NULL for a name the struct
lacks. The subtraction from NULL is undefined, and the index it gives goes
into `ir_sym_offset_of`. `layout.c:451` reads
`layout_agg(l, s->of.agg)->offsets[s->field]` with it, outside the array.
The names of a struct literal are the same case at line 288, where
`inits_set` checks the values alone. `lower_owning.c:689` reads
`field->bits` and `lower_expr.c:188` reads `field->type` of a NULL field.
A library file whose generic names a field its struct lacks so makes antic
read through NULL or outside an array, in any program that uses the
generic. The second audit's S01 named
the case numbers, the lanes and `otherwise_at`, and its fix covers those.
The fix is one more rule of `check_expr`: the name of a field expression
and of each init is a field of the type or of its chain.

### Pointers and arrays of the type table nest without a limit

`src/antic/antl_read.c:271`, safety, rule 14, severe, new.

`check_nesting` measures the entries that have fields, by
`types_has_fields(r->table[i])`, and no other. `read_pointer`, `read_slice`,
`read_optional`, `read_array`, `read_fn` and `read_tuple` at lines 454 to
593 each build a type over an earlier entry and compare no depth. The
DESIGN comment at `types.h:317` says each constructor sets `depth`, the
checker refuses a type deeper than `TYPES_NEST_MAX`, "and no walk over the
parts of a type recurses past the limit". `sema.c:1101` holds that for a
type the program writes. The reader holds it for no type. A pointer entry
takes 6 bytes, so a file of 2 MB gives `*T` 300000 levels deep. `print_type`
of `types.c:1696` then recurses once per level for any diagnostic that
names the type and for `anti doc` of the library. `antl_visit_type` of
`antl_write.c:178` does the same when an importing module writes its own
file. Each overflows the stack of antic without a message. `deep_tables` of
`test_modules.c:2722` chains symbolic values, aggregates and structs, and
no pointer or array. The fix is to refuse an entry whose `depth` passes
`TYPES_NEST_MAX` in `antl_read_types`.

### `anti_rt_json_value` takes a number the grammar refuses

`src/rt/json.h:66`, documentation, a public header whose documented
behaviour differs from the code, severe, new.

The header says `anti_rt_json_value` tells "whether the text between at
and end is one value". At line 73 it says `anti_rt_json_member` gives false
"when the text is no single object of the grammar". Both go through
`anti_rt_json_skip`, whose last case at `json.c:335` takes
`anti_rt_json_number(s, &number, &digits_length)`. That function takes any
run of digits, signs, `.` and `e`, as its own comment at line 45 says. So
`{"a":+-.e}` is an object, and `member` of `anti.json`, at `json.anti:73`,
gives `+-.e` as the value of `a`. The two readers that build values check
the span themselves, at `registry.c:278` and `jsontree.c:238`. The
consequence is a wrong answer and no memory error. The second audit
reported the conversion of `registry.c`, and not this path. The fix is
`anti_rt_json_valid_number` in the last case of `anti_rt_json_skip`.

## Major

### The types the compiler declares are listed three times, and the reader lacks `Job`

`src/antic/antl_write.c:36`, structure, two copies that drifted, major,
new.

`types.c:714` holds the types of `anti.lang` that the compiler declares,
under a DESIGN comment that says "This is the one list of them".
`is_local_struct` of the writer lists them again, and the reader a third
time in `foreign_struct` at `antl_read.c:89`, `lang_struct` at line 597 and
the test for `chan` at line 787. The writer's list holds `types_is_job`.
The reader's holds no `Job`, and `foreign_struct` then looks for a struct
`Job` among the items of `anti.lang`, which declares none. The writer also
writes no result type for a `Job`, where it writes the element of a `chan`
at line 478. `sema_thread.c:576` gives a `dispatch` the type
`types_job(c->types, fn->result)`. A generic whose body holds a `dispatch`
so puts a `Job` in the type table through `io_type` of its tree. The reader
then refuses the file with `` needs struct `anti.lang.Job` ``, in every
module that imports it. No test under `tests/modules/` holds a `dispatch` in a generic
of a library. The fix is one list in `types.c` that both sides ask, and the
result of a `Job` written as the element of a `chan` is.

### Four limits bound the nesting where the header names one

`src/antic/antl_read.c:1060`, structure, one concern in one place, major,
new.

The DESIGN comment at `antl_io.h:59` says "one limit bounds how deep the
tables of a library file nest, TYPES_NEST_MAX of types.h". That limit is
256 at `types.h:824`. `antl_read_value` refuses a constant at
`depth > 64`, `antl_read_symbolic` a symbolic value of the type table at
`depth > 64` on line 148, and `read_const` a constant of the IR at
`depth > 32` on `antl_read_ir.c:387`. The three numbers stand as literals
and carry no name. The writer has none of them: `antl_put_value` and
`put_const` write a value as deep as its type nests. A constant of a struct
that nests 40 levels is inside the checker's limit, and
`lower_const_address` of `lower_place.c:380` keeps it as a global. antic
writes that library file, and antic then calls it damaged. The fix is the
one limit in all four places, or a refusal in the writer at the depth the
reader takes.

### `modules.md` describes an instruction record the file no longer has

`docs/notes/modules.md:7`, documentation, a notes file that describes a
design the code no longer has, major, new.

- Line 7 says "Each instruction is a 49-byte record". `put_inst` at
  `antl_write.c:755` writes 49 bytes, then the member of a load or a store
  of an aggregate, then a count and the arguments. The reader counts 50
  bytes as the least at `antl_read_ir.c:600`.
- Line 9 lists five messages of the reader. `antl_tree.c:1435` and `1493`
  add "needs module of" and "needs generic", and `antl_read_ir.c:746` adds
  "defines, which another library defines".
- The note has four bullets and none on the forms of the type table, the
  externs or the verifier. `generics.md` holds the section of
  the generics. No note names `antl_verify.c` but
  `hosts-and-harness.md:135`, for its stack.

The fix is to rewrite the first bullet from `put_inst` and to say where the
tree and its verifier are described.

### Two files open with the comment of one function

`src/antic/text.c:10`, documentation, a file without a header comment that
says its one purpose, major, new.

The first comment of `text.c` is "Grow to hold extra more bytes plus the
terminating NUL", the comment of `reserve`. The first of `src/rt/regex.c`,
at line 7, is "The 8-bit library, linked statically", the comment of a
define. Neither says what the file is for. `data/first-comments.txt` counts
both among the 274 files that open with a comment, since each stands before
the first declaration. The tool pass so lists neither with its 25 files.
`text.h` and `regex.h` carry the sentence, so the fix is one comment at the
top of each `.c`.

### The byte-pattern rewriter has no test of a pattern cut short

`src/rt/regex.c:436`, safety, rule 15, major, new.

`widen` and `walk_class` read a pattern that `ByteRegex.compile` takes at
run time, byte by byte, and test `i + 1 < n` or the like before each look
ahead. `programs/pattern_bytes.anti:120` to `153` compile whole patterns,
and `errors/pattern_bytes.anti` takes the two refusals. No test gives a
class without its `]` that holds a character outside ASCII, a `\Q` without
`\E`, a `\` at the end or a UTF-8 sequence cut off. The reading found no
defect in them. The two severe findings above lack a test as well: no test
names a field its struct lacks, and none chains pointers. The fix is a unit
test of `anti_rt_regex_compile_bytes` over every prefix of one pattern that
holds each construct.

## Minor

### The table of pairs grows by an unchecked product

`src/rt/toml.c:104`, safety, rule 5, minor, new.

`size_t capacity = doc->capacity == 0 ? 16 : doc->capacity * 2;` and
`realloc(doc->pairs, capacity * sizeof *grown)` on the next line multiply
without a check. The count of pairs is below the length of the input, so
no target of 64 bits reaches the overflow. `jsontree.c:87` checks the same
product since the second audit. The fix is the same check here.

### `anti_rt_regex_compile_bytes` frees its two blocks at three returns

`src/rt/regex.c:491`, safety, rule 12, minor, new.

`free(w.out); free(w.from);` stands at lines 491, 496 and 509, once before
each return. Rule 12 asks for one cleanup block reached by `goto`. No path
leaks.

### Two names become C strings with no test for a NUL

`src/antic/antl_tree.c:1459`, safety, rule 14, minor, new.

`read_extern` reads the runtime name of a function with `antl_get_name` at
line 1423 and copies it into a C string. `antl_get_cstr` of `antl_io.c:193`
exists for that, and refuses a NUL inside. `anti_rt_regex_group_number` at
`src/rt/regex.c:578` copies a group name the same way. The name `a`, a NUL
and `b`, which `patch` passes on at `patterns.c:716`, finds the group `a`.
Each cuts the name at the NUL and reads
nothing outside it. The fix is `antl_get_cstr` in the first and a `memchr`
in the second.

### Eight flag bytes take any value above 0

`src/antic/antl_read.c:515`, safety, rule 14, minor, new.

`if (antl_get_u8(r) != 0)` reads the flag of a symbolic array length, and
the same form reads the default of a field at line 986, a `bool` constant
at 1075 and `variadic` at 1191. `antl_read_ir.c:1101` reads the kind of a
relocation that way, and `antl_tree.c:1440` and `1467` the two flags of an
extern. The marks of a global at `antl_read_ir.c:1112` keep bits 0 to 3
and refuse no other. `io_bool` at `antl_tree.c:139` and the `owned` byte at
`antl_read.c:258` refuse a value above 1. Rule 14 asks a reader to refuse
what it does not understand. A file with a flag of 7 reads as one with 1,
so the DESIGN at `antl_read_ir.c:344`, "A library read and written again
then comes out byte for byte", does not hold for it. The fix is one helper
that reads a flag and refuses a value above 1.

### The address of an IR constant is narrowed before its check

`src/antic/antl_read_ir.c:417`, safety, rule 14, minor, new.

`c->global = (uint32_t)payload;` cuts a 64-bit payload to 32 bits, and
`remap_const` at lines 462 and 468 then checks the cut value against the
count. A payload of 2^32 + 3 passes as global 3. `read_operand` checks the
whole payload first, at lines 526 and 533. The fix is the same order here.

### A field of the type table may have the type of `none`

`src/antic/antl_read.c:917`, safety, rule 14, minor, new.

`read_type` admits the kinds up to `TYPE_ERROR` at line 900, since a tree
carries the type of `none` and the error type. `bind_struct` refuses a
field of `TYPE_VOID` and no other of them. `lower_ir_type_of` at
`lower.c:28` gives `IR_VOID` for such a field, which the reader of the IR
tables itself refuses at `antl_read_ir.c:327`. The two tables of one file
so disagree on what a field may be. The fix is to refuse both kinds where
`TYPE_VOID` is refused.

### A `[[table]]` header scans every key read so far

`src/rt/toml.c:139`, safety, rule 14, minor, new.

`repeats` walks all pairs of the document for each `[[name]]` header, so N
headers cost N^2 / 2 comparisons. A file of 100000 `[[a]]` tables of one
key, about 1 MB, asks for 5 * 10^9 of them. `anti` reads an index fetched
over https with this reader, at `deps.c`, and a program reads its
configuration with it at start. The fix is to keep the last count of each
repeated name.

### The comparison of two names is written four times

`src/antic/antl_tree.c:1377`, safety, rule 26, minor, new.

`name_is` is `antl_name_equals` of `antl.c:35`, which `antl_io.h:128`
declares for all seven files. `same_name` at line 1372 is
`name_equals_name` of `antl_read.c:60`. `antl_read.c:106` and
`antl_write.c:51` write the comparison out once more each. The four agree
today. The fix is the two functions of `antl.c` and `antl_io.h` for all.

### Functions past a threshold

Safety, rule 18, minor, still open. Both tool passes report the class.
`data/thresholds.txt` lists 16 functions of the library file. The judgement
for the ones the tool pass of this audit leaves open:

- One case per kind of record, which follows the format: `put_type` (206
  lines), `io_expr_body` (200), `put_ir` (162), `io_stmt_body` (112) and
  `read_tables` (109). A split would scatter one record over two functions.
- `read_signature` of `antl_read_ir.c:680` (156 lines, nest 4) reads a
  signature, checks its flags and merges it into a function the program
  has. The merge from line 754 to 770 and the two parameter loops would
  read better as functions of their own.
- `antl_read_ir` at line 1056 (140 lines) reads the globals, their
  relocations, the functions and the classes. The loop of the globals from
  line 1072 to 1129 is one part.
- `read_declaration` (120) and `read_extern` (110) of `antl_tree.c` each
  hold a function half and a type half, which could stand apart.
- `antl_read` of `antl.c:136` takes 9 parameters. The bytes with their
  size, the libraries with their count and the message with its size are
  three pairs.

### The pointer set has no function that frees it

`src/antic/ptrset.h:8`, structure, one teardown rule, minor, new.

The comment says "the owner frees slots with free()", while the map and the
index have `ptr_map_free` and `ptr_index_free`. Twenty-one places in eleven
files free the field by its name, `free(walked.slots);` at
`sema_hash.c:210` and `free(answered.slots);` at `types.c:1197` among them.
A change of the set's memory then changes all of them. The fix is
`ptr_set_free`.

### The writer changes the tree it writes

`src/antic/antl_tree.c:845`, structure, state without an owner, minor, new.

`it->capture_capacity = it->capture_count;` runs in all three passes of
`io_item_body`, the collecting and the writing pass among them. `antl_write`
takes its interface as `const` at `antl.h:30`, and the walk reaches the
item through it. The checker grows the captures from the memory pool at
`sema_stmt.c:2653`, so nothing breaks today. The fix is to set the capacity
in the reading pass alone.

### Two headers carry more than their sentence

`src/rt/regex.h:98`, structure, placement, minor, new.

The header opens with "The compile of a pattern with PCRE2, which antic and
the runtime share". Lines 98 to 128 declare `anti_rt_regex_literal`,
`anti_rt_regex_literal_bytes` and the layout of `anti.lang.Match`.
`patterns.c:35` and `55` define the two, which antic never links, and
nothing marks which file a declaration belongs to. `src/antic/sha256.h:7`
includes `../rt/digest.h`, which its one declaration does not use, against
rule 21. `driver.c:689` reaches `anti_rt_sha256_update` through it. The fix
is a line per group in `regex.h` that names its file, and an include of
`digest.h` where `driver.c` uses it.

### Three comments say more or less than the code

`src/antic/antl_verify.c:13`, documentation, a comment that differs from
the code, minor, new.

- The DESIGN comment says "The rules of a library file stand here and
  nowhere else". The rules of the tables stand in the readers:
  `bitfield_fits` at `antl_read.c:23`, the flags of a function type at
  line 551, `ir_bitfield_fits` at `antl_read_ir.c:240`, `targets_blocks` at
  482 and the rule of a load at 633. `antl_verify.c` holds the rules of the
  trees and three of the tables.
- `antl.h:38` says `antl_header` reads "the package header, the module
  name, the imports and the module's doc text". `antl.c:106` to `117` read
  the frameworks and the Linux libraries as well.
- `docs/decisions.md:2167` says `antl.c` holds "the checks that tie the
  format to the enums it stores". `antl_tree.c:31` to `42` hold twelve more
  of them, for the enums of the tree.

### The TOML reader's two comments name fewer callers and fewer forms than it has

`src/rt/toml.c:1`, documentation, a stale comment, minor, new.

The file comment names `anti.toml` and the logger and says "One parser then
serves both". Seven files call `anti_rt_toml_read`: `conf.c`, `plugin.c`,
`driver_index.c` of antic, `manifest.c`, `deps.c` and `syms.c` of anti,
and `toml.anti`. `toml.h:8` lists "basic strings" among what the subset
holds. `read_quoted` at `toml.c:207` takes a string in single quotes as
well, which the plugin index writes, and decodes no escape of either.

### `versions.md` names two functions by their old names

`docs/notes/versions.md:41`, documentation, a stale name, minor, new.

Line 41 names `compatible_line` of `parser_decl.c`, which is
`parser_compatible_line` at `parser_decl.c:463` since the split of the
parser. Line 62 names `write_slots`, which is `whole_write_slots` at
`whole_tables.c:665`. The other ten functions the note names exist where
it says.

## Earlier findings in this area

No finding returned. Each fix the closure of the second audit records for
these files was read again at its place.

| Finding | Now |
|---|---|
| S01, a tree is not verified | Holds. `antl_verify_tree` runs at `antl_tree.c:2029` and checks the case numbers, `enum_value`, `otherwise_at`, the parameter and copy counts, the depth and a record seen twice. The first severe finding above is a relation it does not name. |
| S13, the lanes of a shuffle | Holds at `antl_tree.c:1105` to `1119` and `antl_verify.c:338`. |
| S03 and S23, a lane of no bytes and a scalar of `IR_AGG` | Hold in `simd_shape` and `antl_verify_const`. |
| M20 and M33, the tree's own primitives and counts | Hold. `io_count` calls `antl_get_count` at `antl_tree.c:633`, and `read_tree` bounds each table by its least record. |
| Rule 3, `antl_tree.c:247` | Holds. `io_int` reads through `antl_get_int`, and `antl_get_i32` and `antl_get_i64` rebuild the sign by arithmetic. |
| Rule 5, `arena.c:31` and `jsontree.c:82` | Hold at `arena.c:23` and `35` and at `jsontree.c:87`. |
| Rule 11, `antl_io.h` and `regex.h:29` | Hold. Each comment names the pool or the caller as the owner. |
| The first audit's `toml.c` findings | Hold at `toml.c:148`, `245` and `119`. |
| The first audit's depth limit of `map_sym` and `map_agg` | Holds at `antl_read_ir.c:61` and `115`, with the heights at 102 and 168. |

## The tool pass and the structure report in this area

- `alloc.c` and `sha256.c` have no header comment, as the tool pass
  reports. Both stand as it found them.
- The class of grouped declarations of the tool pass holds 13 of these
  headers: `text.h:17` to `22`, `ptrset.h:36` to `45`, `arena.h:17`,
  `toml.h:44` and `jsontree.h:51`. `text.h` says nowhere that `text_free`
  frees a `struct text`.
- `data/analyzer-unique.txt` holds four warnings here, and all four are
  false positives on reading. `antl_verify.c:339` and `467` are the tool
  pass's own class. At `antl_tree.c:1575` every caller refuses a parameter
  without `param` before `own_params`, at `antl_read.c:378` and
  `antl_tree.c:1681`. At `jsontree.c:155` `read_string` sets `key` on every
  path that gives true.
- `-Wunused-macros` for `PCRE2_STATIC` at `regex.c:9` is the false positive
  the tool pass records.
- `data/duplication.txt` pairs the `_Static_assert` tables of `antl.c:17`
  and `antl_tree.c:31` and the switches of `antl_read.c:876` and
  `antl_tree.c:1254` with others of their shape. Each is one case per kind
  and no copy.
- `data/banned-calls.txt`, `data/unused-static.txt`,
  `data/platform-conditionals.txt` and `data/todo-marks.txt` hold no line of
  these files.
- `antic.c:12` includes `antl_io.h` against the header's last sentence, and
  `antl.h:8` includes `antic.h` without need. The structure report holds
  both.

## The notes and the decisions against the files

- "Files of the library file" in `docs/decisions.md` names the seven files
  and the two headers that exist, each with the purpose its code has. Its
  one gap is the checks of the enums, the third minor finding of
  documentation.
- `antl_io.h` lists each shared function under the file that defines it.
  The 47 functions were compared with their definitions, and none stands
  under the wrong file.
- `modules.md` is the second major finding of documentation.
- `versions.md` describes the descriptor, the chain, `compatible`, the
  record of a plugin, the bitmaps and the checks as the code has them. The
  library file carries `compatible` at `antl_write.c:496` and
  `antl_read.c:748`. Its two stale names are the last minor finding.
- `direct-imports.md` holds. Its fourteen functions exist in the files it
  names, and `antl_tree.c:1292` reads the `home` of a symbol as it says.
- Every DESIGN comment of the 27 files gives a reason. Two no longer
  describe the code: `antl_io.h:59` on the one limit and `antl_verify.c:13`
  on the one place of the rules.
- A search of the comments of these files for the banned terms of the
  docs-style rules found none.

## The sentence of each file

Each sentence is read from the code. "Header: none" marks a file with no
comment that says it.

- `src/antic/antl.h`: the public face of the library file, with its
  version, the depth limit of a tree and the four entry points.
- `src/antic/antl.c`: the magic, the checks of the enums the tables store,
  two shared helpers and the top of the reader, `antl_header` and
  `antl_read`.
- `src/antic/antl_io.h`: the inside of the library file, with the writer,
  the reader and every function the seven files share.
- `src/antic/antl_io.c`: the primitives that write and read an integer, a
  count, a name and a C string, each with its check.
- `src/antic/antl_write.c`: the writer of the header, the type table, the
  constants, the items and the IR.
- `src/antic/antl_read.c`: the reader of the type table, the constants and
  the items, with the checks of each record.
- `src/antic/antl_read_ir.c`: the reader of the IR, which moves each record
  to the indices of the program and merges the copies it has.
- `src/antic/antl_tree.c`: the section of the generics, written, collected
  and read by one walker per kind of record.
- `src/antic/antl_verify.c`: the rules of a tree the reader built, with the
  shape of a simd struct and the type of an IR constant. Its header claims
  every rule of a library file.
- `src/antic/alloc.h` and `alloc.c`: the checked allocation of antic, which
  ends the run on a failure or an overflow. Header of `alloc.c`: none.
- `src/antic/arena.h` and `arena.c`: memory handed out from blocks and freed
  all at once.
- `src/antic/text.h` and `text.c`: a byte buffer that grows and stays
  NUL-terminated, and the formatting of a message into a fixed buffer.
  Header of `text.c`: none, its first comment is that of `reserve`.
- `src/antic/ptrset.h` and `ptrset.c`: a set of pointers, a map from a
  pointer to a pointer and one to an index, over one table.
- `src/antic/sha256.h` and `sha256.c`: the SHA-256 digest of a file, by the
  hash of the runtime. Header of `sha256.c`: none. `sha256.h` also hands on
  `digest.h`.
- `src/rt/json.h` and `json.c`: a scanner of JSON text at the level of
  tokens, which builds nothing.
- `src/rt/toml.h` and `toml.c`: one pass over a subset of TOML that gives a
  flat list of dotted keys. The header of `toml.c` names two of its seven
  callers.
- `src/rt/regex.h`: the handle of a compiled pattern, its compile, the
  pieces of a template, the literals of `patterns.c` and the layout of a
  match. Its header names the compile alone.
- `src/rt/regex.c`: the compile of a text and a byte pattern with PCRE2,
  the rewrite of a byte pattern, and the groups and template pieces.
  Header: none, its first comment is that of a define.
- `src/anti/jsontree.h` and `jsontree.c`: a tree of a JSON document in a
  memory pool, built over the scanner of the runtime.
