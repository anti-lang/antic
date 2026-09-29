# Front end

The second audit of the front end of antic at `238a4f1`. It covers every
line of the lexer, the parser, `ast.h` and `ast_dump.c`, the eleven files
of the checker, `sema.h` and `sema_checker.h`, `types.c` and `types.h`,
the library file code in `antl.c`, `antl.h`, `antl_io.h` and
`antl_tree.c`, and `modpath`, `diagnostic`, `warnings`, `pattern`, `text`
and `arena`. Structure comes first, then `docs/c-guidelines.md` at the bar
of `src/antic/`. Five foreground reviewers read the files in five groups.
Each finding below was then traced again in the code by the author of
this report, and the ones that did not hold were dropped. Nothing was
built or run, so every path is traced by reading. The evidence of
`docs/audit/tool-pass.md` and `docs/audit/data/` was checked the same way.
The status of each finding is against the first audit in
`docs/audit/2026-09-23/`, whose front end findings are named by their
ids in its `summary.md`.

## Counts

| Severity | Rule or question | Findings | New | Returned | Still open |
|---|---|---|---|---|---|
| Severe | One owner for shared state | 1 | 1 | 0 | 0 |
| Severe | 14 | 12 | 8 | 1 | 3 |
| Severe | 14, 17 | 1 | 1 | 0 | 0 |
| Severe | 3 | 1 | 1 | 0 | 0 |
| Major | One purpose per file | 1 | 1 | 0 | 0 |
| Major | Duplicated logic that diverged | 1 | 1 | 0 | 0 |
| Major | 14 | 4 | 4 | 0 | 0 |
| Major | 15 | 1 | 1 | 0 | 0 |
| Major | 3, 16 | 1 | 1 | 0 | 0 |
| Minor | One place per concern | 1 | 1 | 0 | 0 |
| Minor | 3, 5, 6, 9, 11, 14, 16, 20, 23, 24, 25, 27 | 12 | 7 | 0 | 5 |
| Minor | 26 and Warnings | 1 | 1 | 0 | 0 |
| Minor | 18, 19 | 1 | 0 | 0 | 1 |

Severe 15, major 8, minor 15. Six defects that no rule names follow at
the end without a severity.

## Severe

### The context of the checker has no owner, and its hand-written copies differ

`src/antic/sema_checker.h:64`, one owner for shared state, severe, new.
The `try` part is still open from the first audit.

`struct checker` holds 46 fields that all eleven files read and
write. A group of them is the context of the code being checked:
`within`, `signature`, `function`, `scope`, `quiet`, `target_sized` and
`error_type`. No function owns that context. Each place that switches it
saves some fields by hand and restores them by hand, and the places do
not agree:

- `c->within` is written 25 times in 4 files and `c->quiet` 14 times in 4.
  The passes of `sema.c` set `within` and reset it to NULL rather than
  restore it.
- `sema_const_symbol` at `sema_const.c:1096` saves `scope` and `within`
  and not `signature` or `function`. A constant first evaluated inside a
  signature resolves its names there. With `const K: int = 3;`,
  `const M: int = K + 1;` and `fn f<K: int>(x: [M]u8) { }`, `M` is cached
  for good as a value built on the parameter `K` of `f`.
- `sema_alias_type` at `sema_generic.c:418` saves `within`, `signature`
  and `function` and not `scope`. `const_deps_of_symbol` saves `scope`
  and `within` alone.
- A quiet probe caches what it evaluates. `sema_error_at` at `sema.c:58`
  returns under `quiet` without clearing `c->ok`. `refuse_nested_in_signatures`
  resolves a member type quietly at `sema.c:3503`, which may evaluate a
  class constant for the first time. The constant is then stored as done
  with no value, and a later use returns an error type with no message.
  `sema_check` can then return true for a program with an error in it.
- `try` with a handler other than a block returns at `sema_stmt.c:2283`
  before `c->error_type = outer_error;`, so an enclosing `try` loses the
  error type it gathered.

The fix is one type for the context, entered and left by one pair of
functions that are its only writers.

### Recursion that the depth limit of the parser does not reach

`src/antic/parser.h:11`, rule 14, severe, still open (S3) in forms its
fix did not reach.

The DESIGN comment says one limit keeps the checker, the dump and the
symbolic walks on the stack. `descend` stands in `type`, `unary`, `??`,
`if` and `statement` only, so these paths have no bound:

- Left-nested chains. `binary` at `parser.c:1742`, `postfix` and `cast`
  build `1+1+1`, `a.b.b`, `f()()` and `x as T as T` in a loop at one
  level of the parser. The tree is as deep as the chain, and
  `binary_operands`, `sema_check_field`, `dump_expr` and lowering recurse
  on the left side. A sum of 1000000 terms, about 2 MB, overflows the
  stack.
- Types nested in a class body. `nested_type` calls `inner = item(p);`
  at `parser.c:3272`, and `item` reaches `class_item` again. `class A {`
  written 200000 times overflows the stack. `push_item` at line 4072
  recurses over the same depth.
- The lookahead for type arguments. `scan_list`, `scan_type` and
  `scan_types` from line 488 call each other once per `<` or `(`, and
  each step calls `peek_at`, which walks from the parser's position.
  `let x = a<a<a<a;` with enough levels overflows the stack. Every
  identifier of the chain starts a new scan, so the time is cubic.
- Nested `f"..."` in the lexer. `interpolated` calls `placeholder` at
  `lexer.c:1244`, which calls `lex_token`, which calls `interpolated`.
  Each level needs one more `#` than the level inside it. The 64 MiB cap
  still allows about 11000 levels, past a Windows stack of 1 MiB.

The fix is a depth count in the lexer and in `item`. The parser also
needs a limit on the depth of the tree it builds, beside the one on its
own recursion.

### Chains of types the program writes recurse once per link

`src/antic/types.c:1412`, rule 14, severe, still open (S4) for types.

The fix of S4 bounded the chain of constants and made the worker walk a
queue. Chains of types still recurse once per link:

- `types_find_cycle` and `cycle_in` walk `struct S0 { a: S1 }` to
  `S300000` in one descent, about 6 MB of source.
  `type_pointer_free`, `sema_needs_teardown` and `literal_complete` at
  `sema_call.c:445` walk the same chain later.
- A chain of `type` lines, `type T0 = T1;` naming the next, recurses
  through `sema_alias_type` and `resolve_type_inner`. Chains of `constraint` sets recurse the same way.

The library reader bounds the same walks with `NEST_LIMIT`. Source has
no bound.

### Cycles through the copies of generics are never refused

`src/antic/sema.c:2749`, rule 14, severe, returned (S5, S6 and M2).

The pass that reports "contains itself" runs over the items of the
module, and `types_break_cycles` at `types.c:1437` breaks the copies that
exist then. A copy made later is never checked:

- `struct A<T> { b: B<T>, }`, `struct B<T> { a: A<T>, }` and a
  parameter `c: chan A<int>`. `A<int>` is filled from the fields of its
  generic after the pass, and it holds `B<int>`, which holds `A<int>`.
  `type_pointer_free` then recurses until the stack overflows.
- `class A<T> inherits A<int> { }`. When the base is resolved,
  `sema_descends_from` at `sema.c:2431` finds nothing, since `A<int>` has
  no base yet. `fill_copy` then sets the base of the copy at
  `sema_generic.c:1058` to the base of `A` with `int` in place. That is
  `A<int>` itself, and `check_implements` loops forever.

Generics came after the fixes of S5, S6 and M2. The fix is to run the
cycle and base checks on each copy when it is filled.

### A placeholder parses without the token map, and `for` or `assert` in it reads NULL

`src/antic/parser.c:981`, rule 14, severe, new.

`placeholder` sets `inner.origin = NULL;`. An anonymous function is an
expression, so its body parses statements, and `for` at line 2424 and
`assert` at line 2470 read `p->origin[p->pos - 1]`.
`fn main() { let s = f"{fn() { assert(true); }}"; }` reads through NULL.
The fix is an `origin` of the inner parser's own.

### A member reached through a type name keeps an untyped base

`src/antic/sema_call.c:3022`, rule 14, severe, new.

`sema_check_field` sends `E.A`, `K.N` and `m.T.x` to `check_type_member`
and never checks the base, so `e->as.field.base->type` stays NULL. Code
that reads the base of any field then dereferences it:

- `let p = &E.A;` reaches `base = e->as.field.base->type;` in
  `sema_is_place` at `sema_expr.c:31`.
- `1 + E.carry`, with an enum case `carry`, reaches
  `sema_struct_of(NULL)` at `sema_expr.c:2062`.
- `E.A = 1;` and `E.A.compare_swap(0, 1)` reach the same read in the
  assignment and atomic checks.

The fix is to give the base the type it names, or to mark the node so
that no reader takes it for a value.

### The checker takes any type below an enum, and none

`src/antic/sema.c:2356`, rule 14, severe, new.

`enum` resolves `it->base` with `sema_resolve_type` and passes it to
`types_enum` without a test. `enum A: A { X }`, or `enum A: B { X }`
before `enum B`, gives a NULL base, since the symbol has no type yet. A
struct, `str` or another enum is accepted as well. `let x = A.X;` then
reaches `t = t->base;` and `t->lock_word` through NULL at `lower.c:31`.
The reader refuses the same file since the fix of M6, at `antl.c:2252`.
The fix is the same test in the checker.

### A second generic class of one name crashes on its nested type

`src/antic/sema_generic.c:220`, rule 14, severe, new.

`t->nested_in = outer->symbol->type;` does not test `outer->symbol`.
`class Box<T> { }` followed by `class Box<T> { struct Node { } }` refuses
the second `Box`, which keeps a NULL symbol. `Box.Node` is a new name and
is declared, and `sema_check` does not stop between `declare_items` and
`sema_declare_generics`. The fix is to skip a nested type whose outer
item has no symbol.

### A bound function passes as a place

`src/antic/sema_expr.c:37`, rules 14 and 3, severe, new.

`sema_is_place` accepts a field of a place without asking what the field
is. `&v.area`, where `area` is a `pub fn` of `v`'s type, takes the bound
function path of `sema_check_field`, passes the place test and is marked
address-taken. Lowering then computes `lower_field_of(s, name) -
s->fields` at `lower.c:847` with a NULL result, which is undefined, and
the index reaches the layout. The fix is to refuse a field whose node
names a function.

### The generic trees of a library file reach lowering unchecked

`src/antic/antl_tree.c:1258`, rule 14, severe, new. It extends the
finding of the tool pass.

Every tree node is an index into a flat table read at line 2000, so any
node may name any other, itself included. Beyond `otherwise_at`, which
the tool pass names, these values reach the checker or lowering as the
file wrote them:

- `variant_case` of an arm, a cast and a struct literal, and
  `enum_value` of a field, at lines 756, 1045, 1076 and 1066. Lowering
  indexes with each one less 1, at `lower_stmt.c:2278` and
  `lower_expr.c:338`, `1970` and `2992`.
- `it->param_count = n;` at line 1703 is never compared with the
  parameter count of the function's type. Lowering adds the parameters
  of the type and then reads `l->f->params[at]` for each parameter of the
  item at `lower.c:1848`.
- `copy_count` of a call, at line 973. `callee_copy` at
  `sema_copies.c:742` sizes `args` by it, and `fn_copy` reads
  `type_param_count` entries.
- A node that names itself, a block that holds itself, or a chain of
  40000 unary records. `xe`, `xs` and `xb` of `sema_copies.c` and
  `lower_expr` recurse without a depth count or a map.
- Reference 0 is NULL in any slot, which the tool pass names, and
  `ANTL_NO_TYPE` is NULL the same way. `open_node` at
  `sema_copies.c:1029` reads through one before lowering.

The fix is one check of each tree after reading. It tests bounds and
counts against types, and walks with a depth limit that refuses a node
seen twice.

### The writer cuts the lane count of a shuffle, and valid source reaches it

`src/antic/antl_tree.c:1155`, rules 17 and 14, severe, new.

`(uint8_t)e->as.simd.simd->field_count` writes 256 lanes as 0 and 300 as
44. A `simd struct` of 256 `u8` lanes is legal, and above the cap of 256
bytes it is a warning only. A shuffle of one inside a generic body then
reads back with `lanes` NULL or short. `ir_vshuffle` at `ir.c:1006` and
the loop at `lower_simd.c:469` read `field_count` lanes from it. The
reader also takes each lane value unchecked at line 1165. The fix is a
32-bit count that the reader compares with `field_count`.

### A function of the regex library is indexed past its parameters

`src/antic/sema_pattern.c:294`, rule 14, severe, new.

`module_function` at line 67 checks only that the item is a function.
`struct type *param = f->type->params[at];` then indexes by the position
the call needs, and `patch_call` fills five or eight arguments, without
comparing either with `param_count`. A damaged or older `anti.regex`
library file makes the checker read past the parameter array.
`text_equal` at `sema_stmt.c:1417` checks the whole signature of its
function. The fix is the same check here.

### `--dump-ast` of `alloc T { }` dereferences NULL

`src/antic/ast_dump.c:430`, rule 14, severe, new.

`dump_type(d, depth + 1, e->as.alloc.type);` and `dump_expr` of
`alloc.count` run for every `EXPR_ALLOC`. The form `alloc P { x: 1 }`
fills `alloc.value` alone at `parser.c:1331`, so both are NULL, and the
value is never printed. `--dump-types` reaches the same line.

### `%.*s` receives a null pointer

`src/antic/ast_dump.c:52`, rule 3, severe by the definition of the
document, new.

`for 0..3 { }` binds no name, and `loop_name` at line 562 returns
`{NULL, 0}`. `label_name` passes it to `%.*s` from line 634. C11
7.21.6.1 requires a pointer to an array even with precision 0. No libc of
the six targets shows an effect. The tool pass dismissed a different
claim at this line, a NULL `name` pointer.

### A simd aggregate with a lane of no bytes has a second entry

`src/antic/antl.c:2115`, rule 14, severe, still open (S1).

The tool pass finds the lane of a library simd aggregate unchecked at
`ir_lane`. The `simd` flag of the type table at line 2115 checks no lane
at all, and `lower.c:209` passes its lanes to `ir_simd_add`. `round_up`
then divides by zero at `layout.c:96`. The fix is one lane check that
both paths call.

## Major

### The files of the checker have no one purpose, and the header shares everything

`src/antic/sema_checker.h:4`, one purpose per file, major, new. It
extends the finding of the tool pass on the splits.

`sema_checker.h` declares 181 functions and `struct checker`, and every
file includes all of it. The comment at line 4 names six of the eleven
files. The purpose of each file, as read:

- `sema.c` formats diagnostics, holds scopes, lookup and narrowing,
  resolves types, runs the declaration passes and checks the class model
  from line 1779 to 2102 and 2865 to 3417. That is four purposes.
- `sema_export.c` checks the C boundary, writes the doc warnings from
  line 431 to 930, which never use `struct checker`, and builds
  `sema_interface`.
- `sema_stmt.c` checks statements and also holds the worker walk, the
  pointer set that `sema_const.c` and `sema_export.c` use, the value
  rules, `sema_set_index`, `sema_thread_safe`, captures and the checks of
  `main` and `tests`.
- `sema_expr.c` also holds the class `==` and hash, the pattern literal
  and `fixed_layout`. `sema_call.c` also holds `sema_new_node`,
  `literal_complete` and the move rules.
- `sema_const.c`, `sema_hash.c`, `sema_pattern.c` and `sema_safety.c`
  each have one purpose.

The header then names the wrong file for 21 declarations, as
`data/header-sections.txt` lists. The eight under `/* sema_safety.c */`
at line 427 are all defined in `sema_stmt.c`. The fix is the split the
tool pass proposes, plus a file for the class model out of `sema.c` and
one for the doc warnings out of `sema_export.c`.

### Rules written twice have drifted apart

`src/antic/sema_export.c:531`, duplicated logic that diverged, major, new.

Each pair below does one thing in two places, and the copies no longer
agree:

- The names the compiler declares. `resolve_type_inner` at
  `sema.c:782` knows `ByteRegex`, `Match` and `ByteMatch`. The list at
  `sema_export.c:531` lacks them, so a doc comment that names them draws
  a false warning, as `regex.anti` does from line 18.
- The integer base of an enum, checked by the reader at `antl.c:2252`
  and not by the checker. That is the severe finding above.
- The lookup of a library function, written at `sema_pattern.c:67`,
  `sema_stmt.c:1417`, `sema_expr.c:2667` and `sema_call.c:1818`. Only
  `text_equal` checks the signature, which leads to the severe finding on
  `sema_pattern.c:294`.
- `copy_in_chain` and `copy_name` of `sema_generic.c:1501` and `978`
  against those of `sema_copies.c:627` and `506`. The first
  `copy_in_chain` matches the generic itself and the second does not.
  The first `copy_name` is cut at `COPY_NAME_MAX` and the second is not.
- A field by name. `sema_find_field` skips the unit break `_`, and
  `lower_field_of` at `lower.c:762` does not.
- The walks of the tree. The worker walk at `sema_stmt.c:81` handles 15
  of the 38 expression kinds, and `STMT_LET` walks the value but not the
  `else` body. `delete` in a struct literal, in a `let ... else` or in a
  `catch` guard of a `worker fn` passes the rule that refuses it. The
  copy pass, the dump and the tree writer each have a walk of their own.

The fix is one definition of each, and a walk of the tree that each
analysis visits through.

### Copies of generics have no bound on their number

`src/antic/sema_generic.c:1092`, rule 14, major, new.

`COPY_DEPTH_MAX` bounds how deep a chain of type copies goes, and filling
goes on after the first refusal. `struct W<T> { a: ?*W<Box<T>>, b:
?*W<Opt<T>> }` names two copies at each of 64 levels, about 2^64 in all,
from the declaration alone. Function copies have no bound of any kind:
`fn deep<T>(n: int) -> int { return deep<Tag<T>>(n); }` is copied until
memory runs out, in the loop at `sema_copies.c:1735`. Their names are
never cut, which the DESIGN comment at `sema_generic.c:966` promises. A
hang is not severe by the document, as in the first audit.

### A checked subtree is checked again

`src/antic/sema_call.c:2605`, rule 14, major, new.

`sema_check_expr` skips a node only when `prechecked` is set. These
paths check a subtree and then check a tree that holds it:

- The call of a function-pointer field checks the base at line 2605,
  then checks the callee at line 2679, whose `sema_check_field` checks
  the base again. `s.f().f().f()` doubles the work at each link, so 40
  links do not finish.
- `check_unary` builds a call on an operand it has checked, at
  `sema_expr.c:997`, and checks the call at line 1003. After
  `method_call` the operand's callee is a name such as `Vec2.neg`, which
  the second lookup does not find. `-(-a)` on a class with `neg` is
  refused.
- A moved `own` argument is moved again by the second check, so
  `-take(k)` reports that `k` was moved.

The fix is to honour `checked` and `prechecked` on every such path.

### Walks over types and imports keep no record of what they answered

`src/antic/sema_call.c:445`, rule 14, major, new.

`literal_complete`, `provides` with `promoting_field`, and `fixed_layout`
recurse into every field without a record of the types they already
answered. Classes that share types, `C1 { a: C0, b: C0 }` up to `C40`,
cost 2^40 calls for one literal. `depends_on` at `sema.c:210` walks the
import lists of library files the same way. Its depth limit is the
library count, so ten crafted files that import each other take about
10^10 steps.

### The tree tables of a library file are not bounded by its size

`src/antic/antl_tree.c:688`, rules 14 and 5, major, new.

`io_count` takes a bare u32 through `io_size`, where `antl.c` uses
`get_count` against the bytes left. A count of `0xFFFFFFFF` asks the
memory pool for 32 GiB or more and zeroes it. The table counts at line
2000 take 1 byte per record, while each record allocates a whole node of
a hundred bytes or more. antic ends in `exit(70)` or the host kills it,
where rule 14 asks for a refusal.

### No malformed-input test reaches the generic trees or the new parser paths

`tests/unit/test_modules.c:1417`, rule 15, major, new.

`damaged_files` uses a file with no generics, and `damaged_records`
builds its file from `record_source` at line 1581, which holds no
generic. Nothing feeds the tree section an index out of range, a
cycle, a deep chain or a count against its type. `depth_limit` of
`test_parser.c` covers the forms its fix bounded and none of the paths
in the severe finding on the depth limit.

### A group number of `patch` is read as `long`

`src/antic/sema_pattern.c:335`, rules 3 and 16, major, new.

`group = into != NULL ? (long)into->as.integer : 0;` converts a `u64` to
a type of 32 bits on Windows. `data.patch(re"(a)", b"x", 4294967297)` is
refused on macOS and Linux and compiles as group 1 on Windows.
`pattern.h` counts groups and bytes in `long` throughout. The fix is
`int64_t` with a range check.

## Minor

### Shared helpers stand where they are not found

`src/antic/sema_stmt.c:27`, one place per concern, minor, new.
`sema_ptr_set_add` stands in the statements file and serves `sema_const.c`
and `sema_export.c`. Three pointer hash tables do one job:
`sema_stmt.c:27`, `ptr_map` at `sema_copies.c:44` and `index_map` at
`antl_tree.c:54`. `sema_new_node` stands in `sema_call.c`, as the tool
pass names.

### Formatted text is not checked for truncation

Rule 9, minor, still open for `diagnostic.c`. `vsnprintf` into the
message at `diagnostic.c:31` ignores its result. So do `snprintf` into
`message[96]` at `parser.c:3066` and into `message[192]` at `3198`, the
parameter text at `sema.c:3181`, `module[256]` at `antl_tree.c:1490`
and the reason at `sema_stmt.c:714`. `shared_operator` at
`sema_call.c:527` builds a name in `char text[256]` and finds nothing
when it does not fit, so the default `==` runs in place of the declared
operator.

### Sums before an allocation are unchecked

Rule 5, minor, still open. `align_up` and `header + size` at
`arena.c:31` and `33`, `t->length + extra + 1` at `text.c:12`, and
`capacity *= 2` at `text.c:20`. No input reaches them under the source
cap.

### Smaller gaps in the checks of input

Rule 14, minor, new.

- The lexer records a diagnostic and an error token for every bad byte,
  without a cap. 64 MiB of `@` asks for more than 11 GB and ends in
  `exit(70)`.
- A global's alignment from a library file is not checked, at
  `antl.c:3518`, before `emit.c:272` writes `.p2align`.
- `CONST_NULL` is accepted for a `*T` that is not nullable, at
  `antl.c:2429`. The `present` byte takes values above 1 at
  `antl_tree.c:393` and `419`.
- `m.<digits>` of a match accumulates without a test at
  `sema_pattern.c:681`, so `m.18446744073709551617` names group 1.

### Conversions whose result the implementation defines

Rule 3, minor, new. `io_int`, `io_i32`, `io_i64` and `io_char` at
`antl_tree.c:240` to `299` convert unsigned values from the file to
signed types. The first audit's conversions in `sema.c` are fixed.

### Tables indexed by an enum without an assertion

Rule 6, minor, still open. The `names[]` tables at `ast_dump.c:443`,
`496` and `514`. All three match their enums today.

### Returned memory without its owner

Rule 11, minor, new. The 29 helpers of `antl_io.h` carry no comments,
and those that return memory of the pool, `antl_allocate` and
`antl_get_name`, do not say so. `sema_new_node` and the other node
builders of the checker do not say it either.

### Counts printed as a wrapped `size_t`

Rule 16, minor, new. A worker without parameters reaches
`fn->param_count - 1` at `sema_call.c:3392` and prints 18446744073709551615.
The first audit's copy of this at `construct` is fixed. Messages in
`sema_call.c` and `sema_stmt.c` also cast a `size_t` to `int` for `%d`.

### A function used in one file is exported

Rule 20, minor, new. `sema_shared_name` at `sema.c:2223` is used in
`sema.c` alone, and `sema_checker.h:272` declares it under
`sema_expr.c`.

### A type name outlives its buffer

Rule 23, minor, new. `sema_tn` at `sema.c:85` returns one of four static
buffers. `sema_check_field_inits` keeps the result as `type_name` from
`sema_expr.c:3393` while it checks nested literals. Those call `sema_tn`
again, so a message or `moved_to` can name another type.

### Casts that remove `const`

Rule 24, minor, still open. `sema.c:1927`, `sema_export.c:1038`,
`sema_call.c:1434`, `1435`, `2642` and `2831`, and `antl_tree.c:478`.
`sema_safety.c:160` and `212` cast the result of `sema_find_field` and
write through it.

### Exported names without their module's prefix

Rule 25, minor, still open. `lex`, `token_*`, `parse`, `module_path_*`
and `module_file_of_source`, and 21 `type_*` functions and
`symbolic_print` beside `types_*`. `ast_dump_typed` is declared at
`sema.h:271`.

### Comments above the wrong item

Rule 27, minor, new. The comment on both operands at `sema_expr.c:1095`
stands above `names_carry`. The DESIGN comment on `v.f(args)` at
`sema_call.c:518` stands above `shared_operator`. The DESIGN comment on
failing functions at `sema_call.c:1086` has no item. "The passes of
sema_check" at `sema.c:2219` stands above `sema_shared_name`. Also
`ast_dump.c:88` and `831` and `parser.c:2920`.

### Repeated blocks and dead code

Rule 26, minor, new except the allocation block.

- `held_value` at `sema_expr.c:1683` and `stand_in` at `sema_hash.c:222`
  are the same 13 lines.
- `string` and `interpolated` of the lexer repeat the prefix and `#`
  reading and the escapes, at `lexer.c:1056` and `1322`. `primary`
  builds a generic struct literal in three blocks at `parser.c:1175`,
  `1203` and `1229`.
- `antl_float_bits` at `antl.c:3691` is never called.
- A `memset` follows a zeroing allocation at `sema.c:2467`, `2671` and
  `sema_pattern.c:755`.
- The out-of-memory block stands in the lexer, the parser, the checker
  and both reader files. The tool pass reports it as major under the
  checked allocator.

`(void)negative;` at `sema_expr.c:140` hides an unused parameter, which
the Warnings section refuses.

### Functions and files past a threshold

Rules 18 and 19, minor, still open. The tool pass lists them.
`sema_eval_const` is 520 lines with a nest of 6, `read_types` about 490,
and `antl_read` takes 9 parameters. `check_call` split along its callee
forms would also give the finding on checking twice one place to fix.

## Defects no rule names

These are correct as C and wrong for the program.

- `sema_const.c:527`: two text constants compare by the bits of their
  pointers. `const SAME: bool = "a" == "a";` folds to `false`.
- `sema_const.c:694`: a struct literal constant fills every field it does
  not name with 0, so `const Q: P = P { y: 1 };` loses the default of
  `x`. This is the wrong-value half of S7, still open. A class literal
  used as a field default stores the same filler in its base. The
  writer puts it in the library file, and `read_value` at `antl.c:2413`
  refuses it, so the next module cannot read the file.
- `sema.c:2475`: an enum value without `=` may leave its base.
  `enum E: u8 { A = 255, B }` gives `B` the value 256.
- `sema_stmt.c:2515`: each `switch` arm evaluates every earlier arm
  again, still open.
- `sema_export.c:613`: the doc names of a library skip classes, still
  open, so a method of a library class in backticks draws a warning.
- A doc comment inside `{ }` of `f"..."` still gives "expected an
  expression", and a raw NUL byte between single quotes still gives
  character 0 at `lexer.c:918`.

## Fixed since the first audit

S2, S7 for the crash, S8, S9, S10, S11 in the lexer and `tn`, S12, S13,
S14, S15 and S16. M3, M4, M5, M6, M23 and M24. The minor findings on
`marker_length`, `strrchr`, the float literal copies, the rule 11 notes
on `lex`, `keep_name` and `antl_read`, and `sema.h` without
`<stddef.h>`. The mixed widths at `construct`, `atomic_ops` and
`variant_case`, the dead code of the old `sema.c`, and the repeated
lookups of `anti.lang` through `sema_std_item`. The `realloc` of the
worker walk now stops antic like every other failed allocation.
