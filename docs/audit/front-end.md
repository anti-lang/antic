# Front end

The audit of the front end of antic at `fddd434`, whose sources equal those of
the tool pass at `ab03345`. It covers every line of `lexer.c`, `lexer.h`,
`parser.c`, `parser.h`, `ast.h`, `ast_dump.c`, `sema.c`, `sema.h`, `types.c`,
`types.h`, `antl.c`, `antl.h`, `modpath.c`, `modpath.h`, `diagnostic.c` and
`diagnostic.h` in `src/antic/`, with `arena.c` and `text.c`, which hold the
memory of the tree. The standard is `docs/c-guidelines.md`, at the bar of
`src/antic/`. Six reviewers read the files in parts, and every finding below
was checked against the code afterwards. Each input-reachable finding marked
reproduced was run through the existing `build/host/antic` of the main
checkout with `--front-end` or `-S`, without a build. The probes are in
`build/drive/logs/verify/`. A finding on the library file reader was traced by
reading, since no crafted `.antl` file was written. The items of
`docs/audit/data/` on these files were verified again, and the false positives
the tool pass named stay out.

## Counts

| Severity | Rule | Findings |
|---|---|---|
| Severe | 14 | 11 |
| Severe | 3 | 2 |
| Major | 14 | 5 |
| Major | 9 | 1 |
| Major | 5 | 1 |
| Minor | 3 | 2 |
| Minor | 5, 6, 9, 10, 11, 14, 15, 16 | 1 each |
| Minor | 18, 19, 21, 23, 24, 25 | 1 each |
| Minor | 26 | 2 |

Severe 13, major 7, minor 18.

## Severe

### A doc comment inside `{ }` of an interpolated text reads before the token list

`src/antic/lexer.c:1281`, rule 14, severe. Reproduced.

`placeholder` reads `kind = tokens.items[tokens.count - 1].kind;` after each
`lex_token`, which assumes one token was pushed. The error paths of
`block_doc` at lines 594, 609 and 617 push none. When the doc comment is the
first thing in the braces, `count` is 0 and `items` is NULL, so the read is
`NULL[SIZE_MAX]`. `let s = f"{/** a` followed by a newline and `}";` stops
antic with SIGSEGV. The fix is to read the kind only when `count` grew.

### The parser recurses once per level of nesting, without a bound

`src/antic/parser.c:1013`, rule 14, severe. Reproduced.

`unary`, `binary` on each `??`, `type` through `*`, `?*`, `[`, `chan` and
`fn`, `primary` on `(`, `block` through `statement`, `defer` and `undo`
before the check that refuses them, and `else if let` through `if_let` each
call themselves once per level of the source. A file with 200000 `(` around
an expression, or 200000 nested `{ }` in a body, overflows the stack. The
checker, `ast_dump.c`, `symbolic_print` and `cycle_in_symbolic` walk the same
trees recursively, so one depth limit in the parser, refused with a
diagnostic, protects all of them.

### A class that holds itself by value is never refused

`src/antic/sema.c:10119`, rule 14, severe. Reproduced.

The "contains itself" pass tests `ITEM_STRUCT`, `ITEM_UNION` and
`ITEM_VARIANT`, not `ITEM_CLASS`. Every recursive walk over fields then
recurses without end on such a class:

- `type_pointer_free` at `src/antic/types.c:1169`, through `chan_element`:
  `class A { b: B, } class B { a: A, }` and `chan A(1)`.
- `literal_complete` and `sema_field_takes_literal` at `sema.c:3036` and
  `3074`: `class A { a: A }` and `A { }`.
- `promoting_field` and `provides` at `sema.c:2864` and `2886`:
  `class A { use a: A }` and `p.zz`.
- The back end: `class A { x: int, a: A, }` passes `--front-end` and
  overflows the stack under `-S`.

The fix is to test classes in the same pass.

### The checker goes on after "contains itself" and recurses into the cycle

`src/antic/sema.c:2344`, rule 14, severe. Reproduced.

`struct S { a: S }` is reported, but function bodies are checked afterwards,
and `fixed_layout` has no guard against the cycle it reports. A simd struct
`V` and the cast `v as S` recurse in `fixed_layout(S)` until the stack
overflows. The fix is to mark a type that contains itself as an error type,
or to stop before the bodies.

### A constant reads a field of a struct literal that was never filled

`src/antic/sema.c:7102`, rule 14, severe. Reproduced.

`eval_const` of a struct literal fills every field it does not name with
`CONST_INT 0` (lines 7063 to 7068). It matches initialisers against
`s->fields` only, so a field left to its default, the base field and every
inherited field keep the filler. The line
`*out = a.as.aggregate.items[f - base->fields]` then reads `aggregate.items` of an integer 0. With an
inherited field, `f` points into the fields of another type, and the
subtraction is between two arrays. `const O: Out = Out { };` with
`const Z: i64 = O.inner.x;` stops antic with SIGSEGV. Without the crash the
value is wrong: `class P { pub x: i64 = 3, pub y: i64, }`,
`const Q: P = P { y: 1 };` and `Q.x` compile to `mov x0, #0`.

### The size of a repeated array constant overflows

`src/antic/sema.c:7045`, rules 5 and 14, severe. Reproduced.

`arena_alloc(c->arena, e->type->length * sizeof *out->as.aggregate.items)`
multiplies a length the program chose, and the loop at 7046 writes `length`
items. `const A: [576460752303423488]i64 = [0; 576460752303423488];` wraps the
product to 0 and writes past the block, and antic stops with SIGBUS. The fix
is a bound on the length before the product.

### The checker recurses once per link of a chain the program writes

`src/antic/sema.c:7199`, rule 14, severe. Reproduced.

A constant that names another is checked and evaluated through
`const_symbol`, `check_expr` and `eval_const` recursively. A chain of 3000
constants, each `const Cn: i64 = Cm + 1` naming the next, overflows the stack. The
worker walk at `sema.c:1603` does the same along a call chain,
`walk_function` into each callee: a `worker fn` that starts a chain of 60000
functions overflows the stack, and the same chain without `worker` passes.
The `seen` scan of the walk is also quadratic. The fix is an explicit
worklist or a depth limit with a diagnostic.

### `is` on a variant without cases reads case 0

`src/antic/sema.c:2309`, rules 6 and 14, severe. Reproduced.

After "variant `V` has no case", `v is X` reaches
`case_name(from, 0)`, which returns `&v->base->fields[0].name` of an array of
no elements. antic prints "as `V.`", having read the name outside the array.
The fix is to skip the example case when the variant has none.

### `doc_check_text` adds 0 to a null pointer

`src/antic/sema.c:11259`, rule 3, severe by the definition of the document.

`const char *line = doc->text + start;` runs once with `start` 0 for an item
without a doc comment, whose `doc_before` in `parser.c:197` leaves `text` NULL.
C11 6.5.6 leaves pointer arithmetic on a null pointer undefined, and UBSan
does not check `NULL + 0`, so the sanitizer suite passes over it. `anti
check` on `fn f() { }` reaches it. The fix is to return when `length` is 0.

### Counters of `int` overflow on a large input

`src/antic/lexer.c:219`, rules 3 and 16, severe by the definition of the
document.

`lx->line++`, `lx->column++` and `column += (int)n;` at line 392 count in
`int`, and `read_source` of `driver.c` has no size cap, so a file of more
than 2^31 lines, or one line over 2 GiB, overflows a signed integer. `tn` at
`sema.c:111` rotates with `static int next;` and `next++ % 4`, which
overflows after 2^31 type names and then gives a negative index. It needs an
input no one writes, so the fix is cheap and not urgent: a size cap on the
source and an unsigned counter in `tn`.

### A member of a library class need not have a function type

`src/antic/antl.c:1657`, rule 14, severe. Traced by reading.

`m->symbol->type = r->table[s->member_types[j]];` checks only that the index
is below the table size, and line 1553 sets `has_self` on every member. A
member whose type is `int`, or a function without parameters, passes. Then:

- `written_params` at `sema.c:9425` computes `fn->param_count - 1` as
  `SIZE_MAX`, and `same_signature` reads `theirs->params[1]` of an array
  of none, for a class that inherits the library class and replaces the
  member.
- `written_result` at `sema.c:9433` reads `params[-1]` when the type is
  also marked `may_fail` and `has_out`.
- `lower.c:7200` and `header.c:928` read `type->result->kind` through a
  NULL `result` when the type is not a function.

The fix is to refuse a member whose type is not `TYPE_FN` with a `self`
parameter.

### The library reader recurses once per forward reference

`src/antic/antl.c:1878`, rule 14, severe. Traced by reading.

When no entry is ready, the loop at line 2110 maps on demand, and `map_agg`
and `map_sym` follow each forward index recursively. `MAP_BUSY` catches a
cycle but not a long chain: 300000 symbolic values, each an `IR_SYM_OP` whose
operand is the next, make 300000 frames from a file of about 8 MB, and a
Windows stack of 1 MB needs a twentieth of that. `types_find_cycle` at line
1694 does the same through `cycle_in` on a chain of structs whose field
types point forward. `read_symbolic`, `read_value` and `read_const` already
carry depth limits of 64, 64 and 32. The fix is a depth limit here as well,
or an order in which every index points back.

### A symbolic division of the minimum by -1 passes the reader

`src/antic/antl.c:2058`, rules 3 and 14, severe. Traced by reading.

The reader checks `s->op > IR_RET` and nothing about the operands. An
`IR_SYM_OP` of `IR_SDIV` or `IR_SREM` on I64 with `INT64_MIN` and -1, used as
the length of an array, reaches `fold_op` in `layout.c:351`, where
`signed_value(type, a) / signed_value(type, b)` overflows. It is undefined in
C and traps on an x86_64 host. `fold_op` refuses a zero divisor only. The fix
is to refuse the pair in `fold_op`, as `reports_undefined` does for source.

## Major

### A float literal past 127 characters is cut short

`src/antic/sema.c:6731`, rule 9, major. Reproduced.

`snprintf` into `char digits[128]` does not check the result, and the
exponent is lost with the tail. The constant `1.` with 130 zeros and `e-300`,
of type `f64`, takes the value 1.0. The range check at
`sema.c:1133` copies the same way, so `1.` with 130 zeros and `e39` as an `f32`
passes, although it overflows `f32`. `lower.c:631` holds the third copy.
The major grade is for a wrong value accepted without a message. The fix is
one helper that converts from the length-carrying text and refuses a literal
it cannot hold.

### `fixed_layout` overflows its size

`src/antic/sema.c:2368`, rule 5, major. Reproduced.

`*size *= t->length;` and the sum of offsets at line 2384 wrap. For a simd
struct `V` of 16 bytes, `v as [16][1152921504606846977]u8` is accepted as a
conversion between equal sizes, where `v as [3]u8` is refused.

### A cycle of base classes never ends

`src/antic/sema.c:9906`, rule 14, major. Reproduced.

`it->symbol->type->base = base_type;` is set without a test for a cycle, and
`class A inherits B { }` with `class B inherits A { }` makes antic loop
forever in the `for` over `inherited(base)` at line 10387. Every
chain walk of `sema.c` and `types.c`, `descends_from`, `reached_member` and the
callers of `level_above` at `types.c:313`, would loop the same way. A hang is
none of the kinds the document grades severe, and it is still input that
should be refused with a diagnostic.

### The reader takes a bitfield width from the file unchecked

`src/antic/antl.c:2089`, rule 14, major. Traced by reading.

`t->fields[j].bits = get_u8(r);` accepts a width of 40 on an I8 field, and
line 1503 does the same in the type table, where `bitfield_width` refuses
it for source. Layout then gives size 5 and an 8-byte unit, `unit_offset =
out->size - bytes` at `layout.c:166` wraps, and the built program loads 8
bytes 3 before the object.

### The reader takes the alignment of an IR aggregate unchecked

`src/antic/antl.c:2080`, rule 14, major. Traced by reading.

`t->align = get_u64(r);` is not tested for a power of two, as the type table
tests it at line 1490. An alignment of all ones makes `round_up` at
`layout.c:71` wrap and gives a 4-byte struct the size 0.

### The reader accepts a text constant for a slice of any element

`src/antic/antl.c:1740`, rule 14, major. Traced by reading.

`return !r->failed && (t->kind == TYPE_STR || t->kind == TYPE_SLICE);` lets a
constant of type `[]int` hold 3 bytes. `lower.c:1356` makes a slice of 3
elements over them, and the program reads 24 bytes of a 3-byte literal.
The fix is to require a slice of bytes.

### The reader accepts any type below an enum

`src/antic/antl.c:1592`, rule 14, major. Traced to the IR type only.

`base = type_ref(r, i);` is passed to `types_enum` without a test that it is
an integer. `ir_type_of` at `lower.c:129` unwraps one level, so an enum over
`f64` lowers as `IR_F64` and one over a struct as `IR_AGG`.

## Minor

### Conversions whose result the implementation defines

Rule 3, minor. `(uint64_t)((int64_t)a.as.integer >> b.as.integer)` at
`sema.c:6995` shifts a negative value right, and `(int64_t)` of a `uint64_t`
above `INT64_MAX` stands at `sema.c:453`, `523`, `613`, `6797`, `6905`, `6981`
and `7849`. `append_byte` at `lexer.c:298` converts a byte above 127 to
`char`. Clang, gcc and MSVC define each the same way, so no target differs
today. A shift written with unsigned operations, as `arith.c` has, and a
conversion through `memcpy`, remove the dependence.

### Two calls with side effects in one argument list

`src/antic/antl.c:2538`, rule 3, minor. The call of `ir_class_subtable`
passes `map_global(r, maps, interface, false)` and
`map_global(r, maps, at, false)` as two of its arguments. Each call may mark the file damaged. Both write the same message today.

### Unchecked products and sums before an allocation

Rule 5, minor. `realloc(list->items, capacity * sizeof *items)` at
`lexer.c:264`, `lexer.c:1229` and `parser.c:39`, the products of
`types.c:53`, `714`, `765`, `845`, `870` and `948`, of `sema.c:9068`, `9232`,
`9918` and `11435`, and `align_up` and `header + size` in `arena.c:31` and
`33`, and `t->length + extra + 1` in `text.c:12`. Every count is bounded by
an input that already fits in memory, so none wraps on a 64-bit host.

### Reads that rely on a terminator or on a table matching its enum

Rule 6, minor. `marker_length` at `parser.c:189` reads `s[3]` without
`t->length`, and a `//#` at the end of a file reads the NUL that
`read_source` appends, which `lex` and `parse` do not document as required.
The `names[]` tables of `ast_dump.c:374`, `427` and `445` are indexed by an
enum with no static assertion that their lengths match it.

### Formatted text is not checked for truncation

Rule 9, minor. `vsnprintf` into `message[160]` at `diagnostic.c:28`,
`sema.c:101` and `antl.c:936`, `snprintf` into the `96`-byte buffers of `tn` at
`sema.c:115`, and the messages at `sema.c:4910`, `9496`, `9500`, `10317`,
`10592`, `10770` to `10896`, `types.c:433` to `445`, `types.c:682` and
`lexer.c:1545`. A long name cuts the message without a mark.

### Text that carries a length is scanned for a NUL

Rule 10, minor. `strrchr(path.text, '.')` at `parser.c:2799`. `get_cstr` at
`antl.c:1025` drops the length of a name, so a name with a NUL inside is cut
and then compared with `strcmp` at `antl.c:2421`, and `foo\0x` matches `foo`.
`types_member_symbol` returns a NUL-terminated name that `antl.c:1660` and
`sema.c:10188` measure again with `strlen`. The same stands at
`sema.c:1856`, `10977`, `11062`, `types.c:763`, and in `append_byte` at
`lexer.c:296`, which appends one byte through `text_append` with a
placeholder for 0 where `text_append_bytes` serves.

### Returned memory without its owner

Rule 11, minor. `lex` at `lexer.h:136` does not say that `token_list_free`
releases the list. `types_member_symbol` at `types.h:405`, `keep_name` at
`sema.c:10617` and `antl_read` at `antl.h:31` return memory of the compilation's pool without
saying so, and `antl_read` does not say it leaves partial records in
`program` when it fails.

### Smaller gaps in the reader's checks

Rule 14, minor. `(enum ir_fail)get_u8(r)` at `antl.c:2337` stores values past
`IR_FAIL_CHECK`, which only an equality test reads. `get_count(r, 8)` at
`antl.c:1558` asks 8 bytes per parameter name where a record may hold 4, so
the bound could refuse a valid file.

### The malformed-input tests of the library file cover little of it

Rule 15, minor. `damaged_files` at `tests/unit/test_modules.c:1273` changes
the version and the magic, truncates one small file at every length and
changes four fields. Nothing feeds oversized counts, forward or out-of-range
indices, struct, class, enum or variant records, symbolic values, IR
aggregates, class records or long chains. No test feeds the parser deep
nesting. Each severe and major finding on the reader above is one such test.

### Integers of mixed width and sign

Rule 16, minor. `sema.c:3893` computes `required_params(...) - 1` for a
`construct` whose parameter type is unknown, which has no parameters, and
reports "`A.construct` takes 18446744073709551615 arguments". Also
`(int)e->as.call.arg_count != atomic_ops[i].args` at `sema.c:3250`,
`variant_case != i + 1` at `sema.c:7674`, `return (int)i;` at `types.c:917`,
and the writer's casts of `size_t` counts to 32 bits at `antl.c:70`, `155` and
`294` without a check, where rule 17 wants a refusal rather than a damaged
file.

### Functions past a threshold

Rule 18, minor. 33 functions of these files are listed in
`docs/audit/data/thresholds.txt`. The tool pass judges `sema_check`,
`check_stmt`, `eval_const`, `check_expr_inner`, `read_types` and the
dispatchers of `parser.c` and `ast_dump.c`. Of the rest, `check_call` (250
lines), `method_call`, `check_field` and `check_export` would read better with
their long cases in functions of their own. `put_type`, `put_ir` and
`read_ir` follow the format. `check_field_inits` (8 parameters),
`antl_read` (9), `sema_doc_warnings` (7), `types_fn_flagged` (7) and `add`
of `diagnostic.c` (7) pass options one at a time, and a struct would serve
`antl_read` and `sema_doc_warnings`. `eval_const`, which holds two of the
severe findings, would gain most from one function per expression kind.

### `sema.c` is 11491 lines

Rule 19, minor. The tool pass gives the split. `antl.c` at 2733 and
`parser.c` at 2975 are below the line.

### `sema.h` uses `size_t` without its header

Rule 21, minor. `sema.h` includes `<stdbool.h>` and `<stdint.h>` and uses
`size_t` at line 48 and after, through `types.h` and `ast.h`.

### A mutable cache without a comment

Rule 23, minor. `static char names[TOKEN_KIND_COUNT][16];` at `lexer.c:1545`
is written on each call, with no comment on why, in a program whose linker
starts threads.

### Casts that remove `const`, and parameters without it

Rule 24, minor. `tn((struct type *)iface)` and `is_error((struct type *)t)`
cast away a `const` that both take, at `sema.c:2315`, `2692`, `3681`, `3832`,
`4477`, `7586`, `7685`, `8826`, `9736`, `9745`, `9770`, `10705` to `10913`.
`(struct checker *)c` at `sema.c:2902` casts it away to call `error_at`.
`starts_member(struct parser *p)` at `parser.c:1977` only reads, and the
`struct type **params` of `types_fn`, `types_fn_failing`, `types_fn_flagged`,
`types_tuple` and `types_set_cases` in `types.h:227` to `483` are only read.

### Exported names without their module's prefix

Rule 25, minor. `lexer.h` exports `lex` and `token_*` beside
`lexer_is_keyword`, `parser.h` exports `parse`, `modpath.h` exports
`module_path_*` and `module_file_of_source`, and `types.c` exports `type_*`
and `symbolic_print` beside `types_*`. `ast_dump_typed`, defined in
`ast_dump.c`, is declared in `sema.h:197` rather than in `ast.h`.

### Repeated blocks

Rule 26, minor.

- The out-of-memory block, `fputs("antic: out of memory\n", stderr);` and
  `exit(70)`, stands at `lexer.c:268` and `1234`, `parser.c:43` and `2870`,
  `sema.c:258` and `349`, `antl.c:174` and `1366`, `diagnostic.c:17`,
  `arena.c:36` and `text.c:24`. Running out of memory is no problem in the
  program, so this is duplication and not a rule 13 finding.
- `same_text` and `same_name_text` of `types.c:305` and `829` are the same
  function.
- The lookup of an item of `anti.lang` or `anti.mem` is written out in
  `error_class`, `location_type`, `allocator_pointer`, `lang_error_or_null`,
  `null_pointer_maker` and `trace_capture` of `sema.c`, and `lang_struct`
  of `types.c:631` repeats the test of `types_is_flags`, `types_is_job`
  and `types_is_lang_error`. `names_root`, `names_flags` and
  `names_field_descriptor` of `antl.c:1142` repeat `names_lang`.
- `check_parallel` and `check_dispatch` of `sema.c:5296` and `5369` repeat
  the worker lookup, the arity message and the argument loop. The block that
  declares a caught name stands at `sema.c:3740`, `7882` and `8470`.

### Dead code

Rule 26, minor. `(void)c;` after the `return` at `sema.c:3212`, `(void)0;` at
`3228`, `atomic_place(c, place)` called twice at `3242` and `3245`, and
`instance ? 11 : 11` at `4484`. A `memset` follows an `arena_alloc`, which
already zeroes, at `sema.c:1844`, `2713`, `2799`, `3829`, `8038`, `8127`,
`9087`, `9233`, `9278`, `9921` and `9976`.

## Defects no rule names

These are correct as C and wrong for the program. They carry no severity.

- `sema.c:8637`: each `switch` arm evaluates every earlier arm again, so a
  non-constant arm reports "a variable is not a constant expression" once
  per later arm. Reproduced with three arms.
- `sema.c:8466`: `try` with a handler other than a block returns before
  `c->error_type = outer_error;`, so an enclosing `try` loses the error type
  it gathered.
- `sema.c:1764`: a failed `realloc` in `walk_function` returns and drops the
  worker diagnostics, where every other allocation failure stops antic.
- `sema.c:11115`: the comment names "a class or a struct of a library", and
  the test skips `TYPE_CLASS`. A method name of a library class in
  backticks therefore draws a false doc warning.
- `parser.c:552`: an ordinary comment inside `{ }` of `f"..."` is skipped,
  and a doc comment gives "expected an expression".
- `lexer.c:963`: `'\0'` is refused while a raw NUL byte between quotes gives
  a character 0. antic's `read_source` refuses the byte, and the readers of
  `src/anti/` do not.
