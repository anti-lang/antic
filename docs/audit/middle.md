# The middle of antic

This step audited the part of `src/antic/` between the checker and the LLVM
text: the IR in `ir.h`, `ir.c`, `ir_fold.c`, `ir_print.c` and `ir_verify.c`,
lowering in `lower.c`, `lower_lowerer.h`, `lower.h` and the sixteen
`lower_*.c` files, `optimize.c`, `reach.c`, `rt_abi.c`, `arith.c`, `layout.c`,
`memcheck.c`, `whole.c` and `whole_tables.c`, with their headers. Every line
of the 41 files was read at `7746a9cc`, against the three standards of the
brief. The two sections "Files of lowering" and "Files of the pass over the
whole program" of `docs/decisions.md` and the nine notes `ir.md`,
`lowering.md`, `optimizer.md`, `whole-program.md`, `aggregates.md`,
`hashing.md`, `iteration.md`, `dev-checks.md` and `simd.md` were read
against the code, and every sentence that no longer describes it stands
below. The machine evidence of `docs/audit/data/` was read for these files,
and each item of it was checked in the code. No build ran and no suite ran.
Each finding names the standard, the rule or the question, and the
severity, and says whether the first audit in `docs/audit/2026-09-23/` or
the second in `docs/audit/2026-09-30/` reported it. The findings of
`docs/audit/tool-pass.md` for these files are not repeated. Their state
stands under "The tool pass in this area".

## Counts

| Severity | Standard | Findings | New | Returned | Still open |
|---|---|---|---|---|---|
| Severe | | 0 | 0 | 0 | 0 |
| Major | Documentation | 5 | 5 | 0 | 0 |
| Minor | Safety, rules 5, 9, 18, 24 and 26 | 5 | 3 | 0 | 2 |
| Minor | Structure | 2 | 2 | 0 | 0 |
| Minor | Documentation | 7 | 7 | 0 | 0 |

In all: 19 findings, 17 new and 2 still open. None returned.

## Severe

None. No line of these files reads or writes outside an object, frees
twice or relies on behaviour C leaves open. Every index a library file
supplies to a class record is checked by the reader before the code here
reads it. The eleven analyzer warnings of `data/analyzer-unique.txt` in
these files are false positives on reading, as "The tool pass in this area"
records. The nine severe findings of the second audit in this area are
fixed, and the code at each place was read again.

## Major

### lowering.md describes a lowering that stopped at scalars

`docs/notes/lowering.md:7`, documentation, a notes file that describes a
design the code no longer has, major, new.

The first bullet says lowering "handles scalar values" and that "a struct
or array value reports that its lowering arrives in chapter 18, a `str` or
slice in chapter 19 and a function pointer in chapter 20. Field access waits
for chapter 18." Every one of them is lowered: `build_value_into` of
`lower_expr.c:280` builds a struct, an array, a tuple, a variant, a slice
and a bound function in place, and `lower_place.c:29` gives a field its
address. Line 15 names the test `dump_ir_main`, which no `CMakeLists.txt`
holds. The dump tests are `dump_ir_modules` and the `--dump-${stage}`
family of `tests/CMakeLists.txt:352`. The other eight bullets hold. The fix
is to replace the first bullet with what lowering handles now and to name a
test that exists.

### whole-program.md is stale in five places

`docs/notes/whole-program.md:26`, documentation, a notes file that
describes a design the code no longer has, major, new.

- Line 26 says a class record "carries four flags: abstract, final,
  singleton, and a `construct` with arguments". `ir.h:493` adds a fifth,
  `IR_CLASS_REQUIRED`, which `lower_desc.c:1618` sets and the registry and
  the provides table write as bit 2 of their flags.
- Line 61 says the registry is written "when `anti_rt_reflect_new` or
  `anti_lang_Object_deserialize` is among" the functions the entries reach.
  `whole_tables.c:18` lists a third reader, the loader `anti_rt_plugin_load`,
  and `whole.c:870` writes the registry in every dev build and for a bundled
  runtime whatever the reach says. Line 68 says "`src/rt/registry.c` holds
  both readers and nothing else", which the loader in `src/rt/plugin.c`
  contradicts.
- Line 93 says the slot table is "a count and an array of `anti.rt.Slots`
  records". `rt_abi.h:371` gives `anti.rt.SlotTable` a third item, the
  `reflect` flag, which `whole_tables.c:642` writes and the loader reads.
- "Devirtualisation" is the heading of two sections, lines 46 and 98. The
  first says release mode rewrites a call when every table holds one
  function. The second adds that no plugin, no host of one and no program
  with injection is rewritten, which `whole.c:909` holds. One section with
  the whole rule is enough.
- The opening paragraph says `whole.c` holds the analysis and
  `whole_tables.c` the tables. No section covers the merge of identical
  copies (`whole.c:269`), the table facts (`whole.c:781`), the default of
  the backtraces (`whole_tables.c:171`), the injection pass
  (`whole_tables.c:690`) or the provides table (`whole_tables.c:1335`). The
  notes `injection.md`, `plugins.md` and `versions.md` may hold the last
  two, and a pointer from here would do.

### aggregates.md describes the emitter of the native back end

`docs/notes/aggregates.md:16`, documentation, a notes file that describes
a design the code no longer has, major, new.

Line 16 says "register parts of 3, 5, 6 or 7 bytes load and store in pieces
joined with shifts, so no copy reads or writes beyond the aggregate". That
was the emitter of the native back end. The LLVM text writes `memcopy` as
`llvm.memcpy` and llc chooses the moves, as line 10 of the same file says.
Line 18 says "the generated binding from `raylib_api.json` stays planned
for the library chapters". `anti bind raylib_api.json` is built, as
"The bind command" in `docs/decisions.md` records. Line 17 names the native
back end as history, which is fine. The ABI bullets describe `abi.c`,
which the back-end step reads.

### Nine more files open with a comment that is not their purpose

`src/antic/ir.c:10`, documentation, a file without a header comment that
says its one purpose, major, new.

`data/first-comments.txt` counts a comment after the includes as the
header of a file. In nine files of this area that comment describes one
function, one struct or one design and not the file:

- `ir.c:10` says where the memory of the module lives.
- `arith.c:9` describes `extend`.
- `rt_abi.c:7` describes the macro `RT_IR`.
- `lower_expr.c:11` describes `wraps`.
- `lower_stmt.c:11` describes `lower_has_defers`.
- `reach.c:9` describes `struct walk`.
- `lower_simd.c:9`, `optimize.c:13` and `whole.c:14` open with a DESIGN
  that gives the approach or an invariant, with no sentence that says what
  the file holds.

The tool pass already names `ir_fold.c`, `ir_print.c`, `layout.c`,
`lower.c`, `lower_desc.c`, `memcheck.c`, `lower.h` and `optimize.h` in this
area. The closing section gives the sentence of each file as read from the
code, so the fix is one comment at the top of each.

### memcheck.h gives a reason of a back end that is gone

`src/antic/memcheck.h:15`, documentation, a DESIGN comment of a public
header whose reason is no longer true, major, new.

The DESIGN says "a call keeps the check out of the two targets, since the
register allocator already treats one. An inline test of the shadow memory
would need both." The two targets and the register allocator were the
native back end. The LLVM text writes the call now, and llc lays out the
code. Line 47 says the checks "run before selection, which gives each
function of m its entry", and `ir.h:582` says "selection puts a check
before every load and store". No pass named selection exists, and
`driver.c` inserts the checks after `layout_resolve`. The same back end
left `loop_depth` in `struct ir_block` (`ir.h:312`). `lower_new_block` at
`lower.c:286` fills it, with the comment "the allocator weighs a spill by
it", and no file reads it: `grep` finds the field in no reader, writer or
emitter. The fix is one sentence each on why a call and not an inline test,
the name of the pass that inserts the checks, and the removal of the field
with its comment.

## Minor

### Three helpers and three list insertions are written more than once

`src/antic/optimize.c:24`, rule 26, minor, new.

- `bits`, `trim` and `signed_value` stand in `ir_fold.c:5`, `optimize.c:24`
  and `layout.c:372`, with one body each.
- `ir_entry_slot` at `ir.c:812` and `ir_entry_zero_slot` at `ir.c:852`
  each find the end of the slots of the entry block, grow it, move the rest
  and write one instruction. The second calls the first and then repeats
  the block for its store.
- `push_exit_action`, `lower_push_error_action` and
  `lower_push_unlock_action` of `lower_stmt.c:39`, `75` and `101` each
  write the fourteen fields of an exit action by hand, where
  `lower_push_leave_action` at line 63 clears one with `memset`.
- `hash_bytes` of `lower_desc.c:1013` and `ir_hash_name` of `ir.c:138` are
  two FNV-1a loops, the constants spelled in hex in one and in decimal in
  the other.
- The limit of 32 fields of a split slot stands at `optimize.c:1076` and as
  the array bound at `optimize.c:1303`.

`same_operand` of `optimize.c:906` and `whole.c:391` is the tool pass's.

### Three sizes reach an allocation without a check

`src/antic/lower_expr.c:1916`, rule 5, minor, new.

`alloc_zeroed(2 * n + 3, sizeof *args)` at `lower_expr.c:1916`,
`alloc_zeroed(2 * extra + 1, sizeof *args)` at `lower_worker.c:116` and
`alloc_product(block->count, 5) + 1` at `memcheck.c:236` compute a product
or a sum before the check inside `alloc_zeroed`. `lower_class.c:610` does
the same count with `alloc_sum(alloc_product(...), 1)`. The second audit
named the one in `lower_construct`, which is the fixed one.

### snprintf results are not checked, and two buffers are short

`src/antic/whole_tables.c:1481`, rule 9, minor, still open.

The first audit reported unchecked `snprintf` results in messages. The
names of globals and thunks are formatted the same way at `lower.c:969`,
`lower_place.c:397`, `lower_pattern.c:34`, `lower_worker.c:236` and `315`,
`lower_desc.c:701` and `1062`, and `whole_tables.c:67`, `516`, `622`,
`1239` to `1259`, `1418`, `1481` and `1512`, and none reads the result.
Each format is a fixed prefix and one `size_t`. Two buffers hold 32 bytes
for a prefix of 15, `"provides.built.%zu"` at line 1481 and
`"trampolines.%zu"` at line 516, so a count of 10^16 or 10^19 would be
cut. No count reaches that, so the cut is theory, and the rule asks for
the check all the same.

### A const is cast away to free a table

`src/antic/memcheck.c:248`, rule 24, minor, new.

`free((void *)c.frame)` casts the `const bool *` of `struct checker` away
so that `free` accepts it, with no comment at the line. The closure of the
second audit keeps the cast of `lower_place.c:494` with its reason at the
line. Here a non-const local that the struct reads from would need no cast.

### Functions past a threshold

`src/antic/lower_owning.c:56`, rule 18, minor, still open.

`data/thresholds.txt` lists 43 functions of this area, and the tool pass
judged the long ones. Of the parameter lists it did not judge: `work_on`,
`each_element`, `each_array_element` and `each_case` of `lower_owning.c`
take seven or eight parameters each, and five of them, `at`, `into`,
`from`, `made_only` and `what`, travel through three levels unchanged. A
struct of the work would shorten all four. `member_at` of `lower_eq.c:485`
and `ir_binary_wrap` of `ir.c:909` follow the shape of what they do.

### The channel design stands in the file of the workers

`src/antic/lower_worker.c:349`, structure, placement, minor, new.

The header of `lower_worker.c` says "`parallel` and `dispatch` in
lowering", and the "Files of lowering" entry gives locks and channels to
`lower_sync.c`. Lines 349 to 366 open a section "Locking and channels",
hold the DESIGN of how a channel and a Mutex are represented, and define
`lower_load_handle`, which `lower_sync.c` calls four times and
`lower_worker.c` never. `lower_lowerer.h:565` lists it under
`lower_worker.c`. The function and its DESIGN belong in `lower_sync.c`.

### The checks of a dev build stand in four files

`src/antic/lower_lowerer.h:9`, structure, one place per concern, minor,
new.

`lower_lowerer.h:9` and the "Files of lowering" entry at
`docs/decisions.md:2190` say `lower_check.c` holds "the checks of a dev
build". It holds the text of a failed check, the checks of the integer
operators and the bounds check, which its own header says. The conversion
checks `narrow_check`, `scalar_check` and `enum_check` stand at
`lower_expr.c:1419` to `1529`, the zero-table check `lower_check_table` at
`lower_expr.c:1218`, the walk check at `lower_stmt.c:214` and the bounds
check of `load` and `store` at `lower_simd.c:262`. Either the entry names
the four files, or the checks that build no part of their expression move
to `lower_check.c`.

### Two comments of the IR claim more than the code checks or prints

`src/antic/ir_verify.c:679`, documentation, a comment that differs from
what the code does, minor, new.

`check_class` says "every global, function, aggregate and field a class
record names exists". It checks the aggregate only when the record has
`mutable` fields, and never the aggregate and field of a subtable, the
descriptor of an `inject` entry or the descriptor of a `provides` entry.
`whole_tables.c:883` reads a subtable's field into `ir_sym_offset_of`, and
`whole_tables.c:1282` writes an inject descriptor into the injectable
table. The library reader checks each at `antl_read_ir.c:900` to `955`, so
no file reaches them unchecked, and the gap is the claim. `ir_print.c:518`
says a class record prints "its flags", and `class_record` prints four of
the five and neither list.

### Comments and notes cite chapters of a book this repository does not hold

`src/antic/layout.h:12`, documentation, a reference a reader cannot
follow, minor, new.

`layout.h:12` says "Chapter 18 gives the rules", `layout.c:214` "the C
rules of chapter 18", `optimize.c:16` "chapter 2 leaves division by zero" undefined and `lower_expr.c:1531` "the conversions of chapter 2".
The notes open with "Chapter 7 IR" (`ir.md:7`), "Chapter 8 lowering"
(`lowering.md:7`), "Chapter 10 optimizer" (`optimizer.md:7`) and
"Chapter 18" (`aggregates.md:7` and `18`), and `lowering.md:15` names "the
chapter 1 listing". `CLAUDE.md` says the book lives in another repository
and is not a concern here. The rule the comment points at should stand in
the comment, or in a document of `docs/`.

### ir.md lists four scalar types short

`docs/notes/ir.md:7`, documentation, a stale description, minor, new.

The scalar types are "`i8`, `i16`, `i32`, `i64`, `f32`, `f64` and `ptr`".
`ir.h:34` adds `agg`, `clong`, `cwchar` and `lock`, the last three sized by
the target, which `layout.c:35` resolves. The verifier sentence names
"terminators, operand references and operand types" and leaves out
definite assignment, the facts of `ir_verify.c:710` and the class records.

### optimizer.md lists two passes short

`docs/notes/optimizer.md:7`, documentation, a stale description, minor,
new.

The round of `optimize_function` at `optimize.c:1344` runs folding, the
peephole rules, scalar replacement, store forwarding, copy propagation and
dead code elimination. The note names four of the six, and the file holds
no bullet on `split_slots` or `forward_stores`, whose DESIGN comments at
`optimize.c:882` and `1046` would do. The entry rule names `main` or every
function of the module. `reach.c:179` adds every export function, every
exported datum and the constructor of the patterns.

### hashing.md names a runtime function the code does not call

`docs/notes/hashing.md:11`, documentation, a stale description, minor,
new.

Line 11 says a class value inside a default hash calls "the `hash` of the
lowest class of its chain with `lower_level_fn`, or
`anti_lang_Object_hash`", and line 25 says the same of
`anti_lang_Object_equals`. `class_hash` at `lower_hash.c:197` and
`class_equals` at `lower_eq.c:57` fall back on `C.hash` and `C.equals`,
the defaults the compiler writes through `lower_class_function`, which the
section "The default of a class as code" of the same file says at line 48.
The two earlier bullets predate that section.

### dev-checks.md names two functions by their old names

`docs/notes/dev-checks.md:44`, documentation, a stale name, minor, new.

Lines 44, 51 and 92 name `first_element` and `binary_checks`. The rename of
the shared functions of lowering made them `lower_first_element`
(`lower_place.c:49`) and `lower_binary_checks` (`lower_check.c:107`).

### rt_abi.h says the contract stands in one place, and it stands in five

`src/antic/rt_abi.h:8`, documentation, a DESIGN whose claim the code does
not keep, minor, new.

The DESIGN says "the contract stands here once". The functions and the
records do. Four enumerations of the same contract stand elsewhere, each
mirroring an enum of `src/rt/`: `enum check_kind` at
`lower_lowerer.h:220`, `enum hook_kind` at line 228, `enum type_id` at
`lower_desc.c:232` and `enum value_kind` at `whole_tables.c:191`. The unit
tests `records_check_kinds`, `records_hook_entries` and `records_type_ids`
pin the first three, and the trampoline test of `tests/unit/test_whole.c`
the fourth. Three runtime names stand as literals as well:
`allocating_functions` at `lower.c:429` spells `anti_rt_copy_buffer` and
`anti_rt_dup`, which are rows of `RT_FUNCTIONS`, and `whole_tables.c:19`
and `409` spell `anti_rt_reflect_new` and `anti_rt_reflect_call`, which
`src/std/anti/reflect.anti:39` declares and no test pins against antic.
The sentence should name the four enums and the tests, and the two row
names should come from `rt_name`.

## The tool pass in this area

- The eight files without a header comment in this area are still
  without one, and the finding above adds nine.
- `optimize_program` at `optimize.h:6` is still without a comment.
- `same_operand` of `optimize.c:906` and `whole.c:391` is still the exact
  block `data/duplication.txt` names. The near blocks between
  `antl_read.c` and `lower_expr.c:2078` are the switches over the kinds
  of a node, which agree once names are replaced.
- The 15 files of this area that `data/unnamed-files.txt` lists are still
  named by no document.
- The analyzer warnings in these files are false positives on reading:
  `arith.c:18` and `117` shift by `n - 1` and `k` where both functions
  return before `n` or `k` is 64, `layout.c:98` and `313` need an
  alignment of zero that `antl_verify.c` and the checker refuse,
  `lower_desc.c:331` walks an array type whose element is never NULL,
  `lower_expr.c:630` and `2132` read a symbol and a type the checker
  binds, `lower_stmt.c:258` and `555` read the element and the name that
  the checker gives every pattern and every walk, `optimize.c:781` indexes
  a block the verifier bounded, and `optimize.c:83` frees the arguments of
  an instruction that `make_copy` set to NULL before.
- The closed findings of the second audit in this area were read again at
  their new places and stay closed: the IR free of sizes
  (`lower_pattern.c:36`, `whole_tables.c:1022`), the one reach
  (`reach.c`), the one folder (`ir_fold.c`), the one teardown
  (`lower_owning.c`), the one preparation of an object
  (`lower_owning.c:668`), the one rule of a part (`lower_eq.c:94`), the
  checked `malloc` (`lower.c:1124`), the range loop (`lower_stmt.c:449`),
  store forwarding (`optimize.c:1003`), scalar replacement
  (`optimize.c:1116`), devirtualisation of a host (`whole.c:909`) and
  the printer's NULL module (`ir_print.c:62`).

## The sentence of each file

For every file of this area, the one sentence of its purpose as read from
the code. "Header: agrees" means the file's own first comment says the
same. "Header: none" means the first comment describes something else,
named after the colon.

- `ir.h`: the typed three-address IR of antic, its types, instructions,
  functions, globals and class records, and the builders that append to
  it. Header: agrees.
- `ir.c`: the construction of a module of the IR, its tables of files,
  aggregates, symbolic values, functions, globals and class records, and
  the appenders of instructions. Header: none, the first comment says
  where the memory of the module lives.
- `ir_fold.h`: the one rule that folds an integer operation on constants,
  and the three cases that have no value. Header: agrees.
- `ir_fold.c`: the same. Header: none, the first comment describes
  `defined`.
- `ir_print.c`: the text form of a module of the IR, which `--dump-ir`,
  `--dump-opt` and the tests read. Header: none.
- `ir_verify.c`: the check of the structural and type rules of a module,
  definite assignment, the facts and the class records among them.
  Header: agrees.
- `lower.h`: the entry of lowering, a checked module into a module of the
  IR, and its options. Header: none, the first comment is the DESIGN of
  `--no-hooks`.
- `lower_lowerer.h`: the state of lowering and the functions its
  seventeen files share, each under the file that defines it. Header:
  agrees.
- `lower.c`: the helpers every file of lowering uses, the declarations of
  C and runtime functions and of signatures, and `lower_module`, which
  lowers the items of a module. Header: none, the first comment describes
  `lower_ir_type_of`.
- `lower_check.c`: the text of a failed check, the checks of the integer
  operators and the check of an index against its length. Header: agrees.
- `lower_class.c`: the functions written for a class, its init, the
  `construct` for C, the `get` of a singleton, the thunks and the tables
  of a new object. Header: agrees.
- `lower_desc.c`: the descriptors, field and function records, tables and
  class records of classes, structs, variants, tuples, arrays and `?T`.
  Header: none, the first comment describes `table_of`.
- `lower_eq.c`: the default `==` of a value and the `equals` of a class.
  Header: agrees.
- `lower_error.c`: the error channel, a handled call, its handler, the
  guards of a `let` and `fail`. Header: agrees.
- `lower_expr.c`: expressions, a literal built in place, the operators,
  the conversions with their checks, and calls with their targets.
  Header: none, the first comment describes `wraps`.
- `lower_function.c`: a function body with its parameters, hooks and
  lock, an anonymous function, and a closure with its context or
  snapshot. Header: agrees.
- `lower_hash.c`: the default hash of a value and the `hash` of a class.
  Header: agrees.
- `lower_hook.c`: the sites of the hooks and which classes are traced.
  Header: agrees.
- `lower_owning.c`: the one teardown, copy and clear of a value, the
  destroy and copy of a class, and the preparation of a new object.
  Header: agrees.
- `lower_pattern.c`: pattern literals and the constructor that compiles
  them. Header: agrees.
- `lower_place.c`: places and addresses, the type an access reads memory
  as, constant data and `here`. Header: agrees.
- `lower_simd.c`: the operations of simd structs, as one instruction, as
  one operation per lane or as a loop over the lanes. Header: none, the
  first comment is the DESIGN of that choice.
- `lower_stmt.c`: statements, loops, `switch`, the exit actions of a
  block and the slots of a function. Header: none, the first comment
  describes `lower_has_defers`.
- `lower_sync.c`: locks, `sync`, `select` and the operations of a channel
  and a Mutex. Header: agrees.
- `lower_worker.c`: `parallel` and `dispatch` with their thunks and
  contexts, and `join`. Header: agrees, apart from the channel section
  named above.
- `optimize.h`: the optimizer of the IR over a program, a module or one
  function, and the drop of the failure arms. Header: none.
- `optimize.c`: the passes over one function and the removal of what the
  entry cannot reach. Header: none, the first comment is the DESIGN of
  the three invariants.
- `layout.h`: the layout of the aggregates of the IR for one target and
  the folding of its symbolic values. Header: agrees.
- `layout.c`: the same, with the units of bitfields, the resolution of a
  function and the bytes of a constant global. Header: none.
- `memcheck.h`: the checks of AddressSanitizer that `--memory-checks`
  puts before every load and store. Header: agrees in what, not in why.
- `memcheck.c`: the declarations and the insertion of those checks.
  Header: none.
- `whole.h`: the passes over the IR of the whole program and the class
  model they share. Header: agrees.
- `whole_parts.h`: what the two files of the pass share. Header: agrees.
- `whole.c`: the class model, devirtualisation, the merge of copies, the
  singleton check, the table facts and `whole_program`, which runs the
  passes. Header: none, the first comment is the DESIGN of class
  identity.
- `whole_tables.c`: the tables the runtime reads, the registry, the
  default of the backtraces, the trampolines, the slots, the injections
  and the provides table. Header: agrees.
- `reach.h`: what the entries of a program reach, by one rule. Header:
  agrees.
- `reach.c`: the walk that computes it. Header: none, the first comment
  describes `struct walk`.
- `rt_abi.h`: the contract with the runtime, the functions generated code
  calls and the records it writes. Header: agrees, with the claim named
  above.
- `rt_abi.c`: the tables behind that header. Header: none, the first
  comment describes a macro.
- `arith.h`: the arithmetic on constants of n bits that the checker, the
  optimizer and the layout share. Header: agrees.
- `arith.c`: the same. Header: none, the first comment describes
  `extend`.
