# Front end

The third audit of the front end of antic at `7746a9cc`, the tree of the
tool pass. It covers every line of 44 files of `src/antic/`, 38318 lines:
`lexer.c` and `lexer.h`, the seven parser files with `parser.h` and
`parser_parser.h`, `ast.h`, `ast_dump.c` and `ast_walk.c`, the eighteen
checker files with `sema.h` and `sema_checker.h`, `types.c` and `types.h`,
`pattern.c` and `pattern.h`, `diagnostic.c` and `diagnostic.h`,
`warnings.c` and `warnings.h`, and `modpath.c` and `modpath.h`. The three
standards of the audit apply: `docs/c-guidelines.md` at the bar of
`src/antic/`, structure and documentation. The sections "Files of the
parser" and "Files of the checker" of `docs/decisions.md` were read
against the files, and so were the ten notes `lexer.md`, `parser.md`,
`sema.md`, `check.md`, `generics.md`, `closures.md`, `patterns.md`,
`variants.md`, `value-rules.md` and `own-and-lent.md`. Five foreground
reviewers read the files in five groups. The author of this report then
traced every finding again in the code and dropped the ones that did not
hold. Among the dropped were a claimed drift between two type walks and a
claimed missing test, which `tests/CMakeLists.txt:3080` registers. Nothing
was built or run, so every path is traced by reading. The evidence of
`docs/audit/tool-pass.md` and `docs/audit/data/` was read the same way.
The ten notes and `docs/decisions.md` pass `check_docs.py`. The status of
each finding is against `docs/audit/2026-09-23/`, `docs/audit/2026-09-30/`
and its `closure.md`.

Two readings of the standards shape the counts. The documentation
standard grades a header whose documented behaviour differs from the code
severe. Seven stale header sentences therefore stand under one severe
finding, although each is a comment to rewrite. Rule 3 grades undefined
behaviour severe. A pointer comparison that no target misreads stands
there as well, as the earlier audits graded such lines.

## Counts

| Severity | Standard | Findings | New | Returned | Still open |
|---|---|---|---|---|---|
| Severe | Safety, rules 14, 3 and 3 with 16 | 3 | 2 | 0 | 1 |
| Severe | Documentation | 2 | 2 | 0 | 0 |
| Major | Safety, rule 14 | 3 | 2 | 1 | 0 |
| Major | Structure | 1 | 1 | 0 | 0 |
| Major | Documentation | 4 | 4 | 0 | 0 |
| Minor | Safety, rules 3, 5, 6, 9, 11, 12, 14, 15, 16, 20, 23, 24 | 12 | 12 | 0 | 0 |
| Minor | Safety, rule 26 | 2 | 2 | 0 | 0 |
| Minor | Safety, rules 18 and 19 | 2 | 0 | 1 | 1 |
| Minor | Structure | 5 | 5 | 0 | 0 |
| Minor | Documentation | 5 | 5 | 0 | 0 |

Severe 5, major 8, minor 26. The analyzer warnings on these files stand
under "False positives" at the end, each with its reason.

## Severe

### A class whose base was refused keeps a NULL base field, and two walks read it

`src/antic/types.c:1987` and `src/antic/sema_const.c:101`, rule 14,
severe, new.

`resolve_base` returns without setting the base at `sema.c:2200`, `2219`,
`2231` and `2238`: for an imported base in error, for a base that is no
class and for a `final` base. `declare_fields` then writes
`fields[0].type = it->symbol->type->base` at `sema.c:2516`, so field 0 of
the class has type NULL. The comment of `nest_value` at `types.c:1646`
records that, and that walk tolerates it. The passes of `sema_check` go on
after the error, `check_member_functions` among them, and two walks over
fields do not tolerate it:

- `pointer_free` at `types.c:1983` skips `FIELD_TABLE` and `own` fields
  alone, so it calls itself with NULL, and `switch (t->kind)` at line 1955
  reads through it. `chan D` in any body reaches it through
  `sema_chan_element` at `sema.c:793`. So do a `parallel` chunk of `[]D` at
  `sema_thread.c:381` and `moves_at_call` at `sema_value.c:601`.
- `class_value` at `sema_const.c:100` recurses into a `FIELD_BASE` with
  `s->fields[j].type` and writes `out->as.aggregate.count = s->field_count`
  through NULL at line 92. A field default `d: D = D { }` reaches it
  through `sema_eval_const` at `sema.c:2543`, since the struct literal path
  at `sema_const.c:739` hands a class to it.

`tests/errors/final_replace.anti:99` refuses `Spot inherits Dot` and
never names `Spot` again. No test uses a class after its base was refused
(rule 15). The fix is one of two. Skip a `FIELD_BASE` whose type is NULL
in both walks, as `nest_value` does, or give a refused base `types_object`
as the cycle case at `sema.c:2247` already does.

### A field is compared with the bounds of an array it may not lie in

`src/antic/sema_copies.c:197`, rule 3, severe by the definition of the
document, new.

`xf` walks the chain of bases and tests at each level whether `f` lies
between `up->fields` and `up->fields + up->field_count`. A relational
comparison of two pointers that do not point into one array is undefined
in C11 6.5.8. Every level but the one that declares `f` is such a
comparison. No target of the six misreads it, so the effect is none.
Every `struct struct_field` carries `home`, which `types_set_fields` at
`types.c:1542` sets. So `f->home == up` followed by `f - up->fields` is
the same test inside one array.

### The least match length counts in `long`, which overflows on Windows

`src/antic/pattern.c:1126`, rules 3 and 16, severe by the definition of
the document, still open as the remainder of M44 of the second audit.

The reader of `pattern.c` counts groups, repeats and lengths in `long`:
`capture`, `min`, `max` and `groups` at lines 112 to 125, `read_number`
at 321, `times` at 1100 and `least` at 1112. `times` caps a repeat at
`1L << 30`. `least` of a sequence adds the length of each child to
`total` at line 1126 and caps the sum at line 1127, after the addition.
Two children at the cap sum to 2^31, a signed overflow where `long` is
32 bits, which it is on both Windows targets. The pattern
`(a{65535}){65535}(b{65535}){65535}` reaches it, since 65535 is the
quantifier maximum of PCRE2, through `pattern_least_bytes` at
`sema_pattern.c:489`. There the wrapped total is negative and
`(uint64_t)least` at line 492 is a large number. A `with` that does not
fit the match then compiles on Windows and is refused on the Mac, a
result that differs between targets. M44 moved the group number of
`patch` and the header to `int64_t`. The reader's fields and arithmetic
stayed `long`. The fix is `int64_t` throughout and a cap of each addend
before the sum. That PCRE2 compiles the trigger was not run.

### Header comments that state what the code does not do

`src/antic/lexer.h:171`, documentation, a public header or a file whose
documented behaviour differs from what the code does, severe by the
definition of the standard, new. Seven places, each a comment to rewrite.

- `lexer.h:171-176`: "Errors inside one literal count toward the same cap
  and are dropped past it." `placeholder` at `lexer.c:1247` lexes an
  `{expr}` on a copy of the lexer. It copies `pos`, `line`, `column` and
  `ok` back at lines 1321 to 1324, and not `errors`. The errors of a
  placeholder count toward nothing. The major finding below has the
  effect.
- `lexer.h:178-182`: `lexer_lex` says "Report every error to diags and
  keep going, so that one run reports all of them". The cap of
  `LEX_ERRORS_MAX` seven lines above contradicts it.
- `types.h:429-430`: `types_object` is "A new struct type without fields.
  Each call returns a distinct type." `types.c:686` returns the cached
  root `anti.lang.Object` with its table field on every call after the
  first.
- `types.h:889`: `types_has_fields` answers for "a struct, a union and a
  class". `types.c:1869` answers true for a tuple, a variant and a `?T` as
  well, which every caller of the walks relies on.
- `types.h:218-221`: "A struct that declares or inherits an abstract
  function is reached through a pointer of two words." An interface
  pointer is one pointer to an inline sub-object, as
  `docs/anti-object-model.md:323` says of `anti_Circle_as_Serializable`.
  The value of two words is the bound function of the DESIGN at
  `types.h:422`. Likely: read from the specification and the checker, not
  from lowering.
- `sema_generic.c:16-18`: "Compiling the copies is a later pass. The
  checker records the first use that needs one, and the driver refuses a
  build past the front end that has one." `sema_compile_copies` of
  `sema_copies.c` compiles every copy from `sema_check` at `sema.c:2943`.
  Nothing records a first use. The one refusal left, `copy_failed` at
  `sema_copies.c:90`, is the checker's.
- `sema_thread.c:1-3`: the header names "the Mutex, channels and `sync`".
  `check_sync` stands in `sema_stmt.c:1366`, and `STMT_SYNC` appears
  nowhere in `sema_thread.c`.

### Two function values that differ in nullability alone cannot be compared

`src/antic/sema_operator.c:470`, documentation, severe by the definition
of the standard, and a defect no rule names, new.

The DESIGN at lines 462 to 467 says two pointers of one element compare
"whether or not either of them may hold `none`". The one at 792 to 795
says "A function value follows the pointer rule". For two function types
`comparable_pointers` tests `a->params == b->params`, the address of the
parameter array. `types_fn` at `types.c:91` gives every interned function
type an array of its own, so two distinct types share one only when both
are NULL. `f == g` with `f: fn(int)` and `g: ?fn(int)` is therefore
refused at line 599 as "the operands of `==` have the types", while
`fn() == ?fn()` passes and `*T == ?*T` passes. No test compares two
function values. The fix compares the two types through
`types_without_none`, as `require_fn_form` at `sema_expr.c:660` compares
forms.

## Major

### The errors inside an `{expr}` escape the cap of the lexer

`src/antic/lexer.c:1321`, rules 14 and 15, major, returned. The second
audit's minor finding on a diagnostic per bad byte was closed by
`LEX_ERRORS_MAX`, and the fix does not reach this path.

`placeholder` runs `lex_token` on the copy `inner` and never copies
`inner.errors` back, and its loop at line 1260 tests no cap. So the stop
of `lexer_lex` at line 1511 never fires from errors inside placeholders,
and every placeholder starts its count from the outer value again. A
literal `f"{@}{@}{@}"` continued over 64 MiB gives one diagnostic and two
tokens per placeholder with no bound, the flood the second audit
reported. A placeholder with more than 100 bad bytes writes "more than
100 errors, the lexer stops here" and the lexer does not stop, once per
such placeholder. `tests/unit/test_lexer.c:258-260` says "Errors inside
one literal count toward the same cap". Its second case at line 280 is a
plain string, which shares the lexer. The fix copies `inner.errors` back
beside `ok`, breaks the loop past the cap, and adds a case with errors in
a run of placeholders.

### The parser loops forever at the depth limit before a statement keyword

`src/antic/parser_stmt.c:823`, rules 14 and 15, major, new.

`statement` at line 793 calls `parser_descend` before it reads a token.
Past `PARSE_DEPTH_MAX` it returns NULL without consuming one.
`parser_block` at lines 823 to 830 calls `statement` and, on NULL,
`parser_sync_statement`. That function stops at `parser.c:328-339` before
`let`, `const`, `if`, `while`, `do`, `return`, `break` and `continue`
without consuming, and clears `panic`. The loop calls `statement` again,
`too_deep` at `parser.c:100` sees `panic` false and adds the diagnostic
again, and nothing moves. The input is 256 nested `{` in a body followed
by `let x = 1`. The diagnostics grow until the allocator stops antic,
where rule 14 asks for one message. The item loop of `parser_parse`
guards its progress with `before` at lines 686 to 738. `parser_block` has
no such guard. `depth_limit` of `tests/unit/test_parser.c:83` nests
empty blocks and `defer` chains, whose tokens the sync consumes, so the
case is not covered. The fix consumes one token when `statement` returned
NULL and `p->pos` did not move.

### The tree built inside an `{expr}` is not counted by the chain around it

`src/antic/parser_expr.c:135`, rule 14, major, new.

The DESIGN of `struct chain` at `parser_parser.h:62-69` says the reach
keeps the tree itself within `PARSE_DEPTH_MAX`, beside the parser's own
recursion. `placeholder` copies the parser into `inner`, parses the
expression, and copies nothing back but `ok`. The reach the inner tree
built is lost, so the chain around the format literal counts it as a
leaf. With `LEX_FORMAT_DEPTH_MAX` levels of `f"..."`, each the last link
of a chain, the tree is far deeper than the limit. The checker,
`ast_dump.c` and `ast_walk.c` recurse along it. Whether the 64 MB work
stack of `platform.h:93-103` overflows in practice was not measured. The
fix carries `inner.reach` back into `p->reach` when it is deeper, after
`parser_expression`, with a case of `depth_limit` for nested format
literals in a chain.

### `e[i] = v` without a hook checks its base twice

`src/antic/sema_stmt.c:361`, structure, the rule of the `checked` flag
under "Files of the checker", major, new. It is the pattern of M35 of the
second audit on a path its fix did not reach.

`check_set_index` checks the base at line 361 with `sema_check_expr`, and
returns false at line 377 when the type has neither `index` nor
`set_index`. `check_assign` then calls `sema_check_storage` on the target
at line 633, which reaches `sema_check_expr` on the same base node at
`sema_expr.c:1983`. `sema_check_expr` at line 2473 skips a node only when
`prechecked` is set, and neither flag is set here. An error in the base
is reported twice. A call in the base is resolved and rewritten twice,
and an `own` argument of such a call moves twice, so the second pass
refuses it as moved. No test holds such an assignment. The fix lets
`check_set_index` hand the checked type on, or marks the base.

### Twelve files have no header comment that says their purpose

`src/antic/lexer.c:13`, documentation, a file without a header comment
that says its one purpose and its place among its neighbours, major,
new. The tool pass names eight of these. This reading adds `lexer.c`,
`diagnostic.h`, `ast_dump.c` and `sema.c`.

- `lexer.c`: the first comment, line 13, describes the `kinds` table.
- `lexer.h`: line 15, "Names and literals", inside the enum.
- `diagnostic.c`: no comment in the file.
- `diagnostic.h`: line 9, the DESIGN of the names.
- `warnings.c`: line 15, the `NAMES` table. `warnings.h`: line 11,
  `enum name_kind`.
- `modpath.c`: line 24, `separates`. `modpath.h` carries the sentence.
- `pattern.c`: line 77, the DESIGN of the check. `pattern.h` carries part
  of it.
- `types.c`: line 32, `depth_of`. `types.h` carries the sentence.
- `sema.h`: line 40, `struct const_value`.
- `ast_dump.c`: line 8 says how node names are chosen, not what the file
  is or that it reads the checked types through `sema.h`.
- `sema.c`: lines 12 to 16 are the DESIGN of the one-pass checker. They
  say nothing of what `sema.c` holds beside its seventeen siblings.

### Public functions without a comment of their own

`src/antic/types.h:347`, documentation, a public function of a header
without a comment that says what it takes, gives and refuses, major, new.
The tool pass counts the fifteen of `types.h`.

- `types.h`: `types_init` 347, `types_builtin` 352, `types_is_lent` 364,
  `types_array` 386, `types_slice` 388, `types_fn` 399, `types_is_watch`
  706, `types_object_lock` 709, `types_is_object_lock` 710, `types_struct`
  789, `types_set_fields` 813, `types_is_integer` 865, `types_is_signed`
  876, `types_is_float` 877 and `types_is_numeric` 878. A reader cannot
  tell from the name that `types_set_fields` copies the fields and
  overwrites every `home` (`types.c:1541`). Nor that `types_struct` gives
  a distinct type per call where the constructors beside it intern. Nor
  that `types_is_integer` is false for `char` and `bool` and true for the
  three target-sized types, that `types_array` takes a length of 0 as
  symbolic, or what the object lock is, which `types.c:926` alone
  explains.
- `diagnostic.h`: `diagnostics_add` 59 and `diagnostics_warn` 62 share no
  comment, and `diagnostics_free` 74 does not say what it frees. Line 51
  calls the list "The errors of one compilation" where it holds warnings
  too.
- `warnings.h`: `warnings_name` 15 and `warnings_kind` 16.
- `modpath.h`: `modpath_segments` 38.

### The "Files of" sections describe files the checker no longer has

`docs/decisions.md:2176`, documentation, the sections "Files of the
parser" and "Files of the checker" do not describe the files that exist
with their current purposes, major, new. The tool pass reports the three
files no section names. These are the sentences that stand and no longer
hold.

- Line 2176: "The checker stands in six files along its parts", and
  `sema_export.c` holds "the checks of what crosses to C with the doc
  warnings". Eighteen files stand, which `sema_checker.h:4-24` lists. The
  doc warnings are in `sema_doc.c`, which line 2177 records, and line
  2176 still reads as current.
- Line 2177: "`sema_export.c` keeps the checks of what crosses to C."
  `provides`, `link framework` and `link linux` are checked in `sema.c`
  at lines 1925 to 2022, from `check_boundary` at 2854, whose own comment
  calls them "The checks of what crosses to C".
- Line 2183: "`sema_thread.c` holds the atomic operations, the Mutex,
  channels and `sync`." `sync` is `check_sync` of `sema_stmt.c:1366`.
- Line 2181: `sema_generic.c` also holds the `type` aliases,
  `sema_alias_type` at 515, and `sema_strip_generics` at 2220, a pass of
  the driver on the tree.
- Line 2182: `sema_hash.c` also holds the default `==`, `sema_default_eq`
  at 523, `sema_equals` at 548 and `sema_class_defaults` at 582, which its
  own header says.
- Line 2178: "every analysis of the checked tree visits through"
  `ast_walk.c`. The worker walk at `sema_stmt.c:98` and the `assert` search
  at 964 do. `stmt_returns` (176), `stmt_leaves` (216), `sets_stmt`
  (2374), `sema_proved_names` (301) and `sema_value_view` (1920) recurse
  on their own. The first two read the last statement of a block alone
  and cannot use a visitor that visits everything. The sentence should
  say so.
- Line 2163, and the header of `parser_decl.c:1-3`: `parser_decl.c` reads
  "the members, nested types and fields of a body". The field loops stand
  in `parser_item.c`, `class_item` at 62 to 172 and `item_level` at 435
  to 467. `parser_decl.c` holds a field's guard clause, its `unchecked`
  clauses and the struct-only refusal.

### A note describes a refusal the code replaced

`docs/notes/generics.md:31`, documentation, a notes file that describes
a design the code no longer has, major, new.

"A function with type parameters of its own keeps the member of the
generic and is refused where it is called." The first half holds:
`sema_copies.c:439-443` keeps the generic's member. The second does not.
`callee_copy` at `sema_copies.c:706` hands such a call to
`own_params_copy` at 610, which makes `Box<int>.map<float>`, as line 62
of the same note describes. The bullet of the first step was not removed
when "What can be generic" built the copies.

## Minor

### Tables and names defined in two places

`src/antic/sema_class.c:1020`, structure, one place per concern, minor,
new.

- The sixteen functions of the root are listed twice: `root[]` in
  `declare_root` at `sema.c:1519-1541` and `root_names` with
  `root_params` at `sema_class.c:1020-1029`. They agree today, and
  nothing pins one to the other. `sema_table_of` could read the members
  `declare_root` builds. The names `type_name`, `equals`, `hash`,
  `serialize`, `destruct` and `copy` are spelled in both outside the
  "defined here and nowhere else" set of `types.h`.
- The hook table is defined twice: `hook_names` at `sema_generic.c:22-26`
  and `names[]` of `sema_operator_named` at `sema_operator.c:134-139`,
  the same twenty strings in the same order. The bits of a `TYPE_PARAM`
  depend on the first copy alone.
- The name `anti.lang` stands as a static `struct name module` three
  times in `sema.c`, at 1177, 1193 and 1499. The name `anti.regex` stands
  three times in `sema_pattern.c`: `regex_module` at 31 and `module` at
  898 and 944.
- `same_text` at `types.c:528` and `sema_same_name` at `sema.c:88` are
  one function in two files. The in-file duplicate of the first audit is
  gone, and this one stands.
- `chain_entry` at `sema_class.c:45-61` is `types_member_level` of
  `types.c:585` with `sema_inherited` for `level_above`, which give the
  same step.
- "Does the qualifier name a level of the chain" is written at
  `types.c:548-552`, `types.c:634-638`, `sema_class.c:23-37` and twice in
  `one_table`. One helper would also take `types_interface_member` from a
  nest of 5 to 4.

### Repeated blocks

`src/antic/sema_generic.c:1838`, rule 26, minor, new. The walk of
`class_in` and `holds_mutex` is the tool pass's.

- The tail of `sema_generic_call` at `sema_generic.c:1838-1878` repeats
  `finish_copy` at 1890-1906 line for line: the "nothing in the arguments
  gives" loop, the `check_meets` loop and the recording of the copy
  arguments. The differences are the skipped class parameters and the
  names in the message.
- `align(N)` is read three times, `parser_item.c:28-35`, `195-202` and
  `421-428`. The recovery of `parser_parse`, one token on no progress and
  then `sync_item`, stands five times at `parser.c:686-690` to `734-738`.
  The `own` lookahead stands twice, `parser_decl.c:32-35` and `50-53`.
- The compound-assignment refusal of `check_set_index` stands twice,
  `sema_stmt.c:367-370` and `387-390`. `operator_receiver` at
  `sema_operator.c:282-294` builds the same `&a` node in both branches.
- `needs_teardown` at `sema_value.c:67-88` repeats the tests of
  `own_field_in_chain` and `destruct_in_chain` at 106-136, the `destruct`
  name once as `memcmp` and once as `sema_name_is`. `class_in` at
  `sema_const.c:1104` and `holds_mutex` at `sema_value.c:310` walk a type
  the same way with another leaf, which the tool pass names. The variant
  arm of `needs_teardown` at 49-58 walks by `params` where the other two
  walk by fields.
- The message "is atomic, so it is read with `load()`" stands at
  `sema_expr.c:1908`, `sema_call.c:2272` and `2453`. The visibility
  message "is %s `%s`" stands at `sema_call.c:742`, `2442` and `2607`. A
  change touches three places and three `.err` files.
- The digit and letter tests are spelled three ways, `lexer.c:240-263`,
  `pattern.c:175-183` and `modpath.c:12-17`. `error_at` at
  `lexer.c:286-289` formats into a buffer and passes it to
  `diagnostics_add` through a second `"%s"`, because `diagnostic.h` has no
  `va_list` entry.

### Dead code, wrappers and unused includes

`src/antic/sema_copies.c:450`, rule 26, minor, new.

- The loop at `sema_copies.c:450-453` moves a field's `home` from the
  generic to the copy and is never true. `types_set_fields` sets
  `home = s` on every field at `types.c:1542` when `fill_copy` gives the
  copy its fields. The sentence of the comment holds through that line.
- The test of `==` and `!=` with `comparable_pointers` at
  `sema_operator.c:680` stands in the `default` arm, which the case for
  those two operators at 567 never reaches.
- `unify` tests `map->types != NULL` at `sema_generic.c:1642`, `1648` and
  `1665` and calls `types_symbolic` with it at 1686 without the test.
  Every caller sets it.
- `sema_operator_symbol` at `sema_operator.c:155` and `sema_set_index` at
  `sema_stmt.c:409` are one-line wrappers over a static of the same file.
- `sema_call.c:735` and `903` assign the result of `sema_check_expr` to
  the type of the base, which `sema_check_expr` stored at
  `sema_expr.c:2477` already.
- `<stdio.h>` is included and nothing of it used in `sema_chain.c`,
  `sema_copies.c`, `sema_safety.c` and `warnings.c`.

### Two functions used in one file are exported

`src/antic/sema_checker.h:392`, rule 20, minor, new. `sema_simd_numeric`
is called in `sema_simd.c` alone and `sema_holds_param` in `sema_value.c`
alone, over `src/antic`, `src/anti` and `tests/unit`.

### Casts that remove `const`, and parameters without it

`src/antic/sema_copies.c:152`, rule 24, minor, new. The cast of
`cl->map.params` to `struct type **` removes `const` with no comment at
the line, where the closure of the second audit says the casts of the
copy pass stand with their reason. `sema_generic.c:1143` has one. The
write is safe, since `map_add` reallocated the array before it.
`sema_bind_value` and `sema_refuse_owned_copy` at `sema_value.c:280` and
`292` take `struct type *t` and only read it, beside
`sema_refuse_lock_copy`, which takes `const`. `tuple_pattern` at
`parser_stmt.c:352`, `parser_never_result` at `parser_decl.c:168` and
`check_simd_struct` at `sema.c:638` only read their parser or item.

### A rotating buffer without its comment

`src/antic/sema_value.c:203`, rule 23, minor, new. `sema_owns_phrase`
keeps two static buffers and a static counter with no comment on why,
where `lexer_token_kind_name` at `lexer.c:1529` has one.

### Conversions and expressions whose result the implementation defines

`src/antic/types.c:373`, rule 3, minor, new, the kin of the first audit's
finding on `sema.c`.

- `(long long)` of a `uint64_t` above `INT64_MAX` at `types.c:373`,
  `sema_const.c:259` and `852`, which `sema_signed_bits` at `sema.c:20`
  exists to avoid by its own comment.
- `push` and `keep` run in one expression at `lexer.c:444`, `785` and
  `1169`: the text of the token `push` returns is assigned from `keep`,
  and both allocate. The order does not change the result, since the two
  touch different objects. It is the form CLAUDE.md forbids.

### Products before an allocation

`src/antic/sema.c:1292`, rule 5, minor, new. A product of a count and a
size reaches `arena_alloc` or `parser_node` unchecked at `sema.c:1292`,
`sema_const.c:687`, `sema_stmt.c:2822` and `parser_expr.c:173`, where the
same files use `types_alloc_array` and `alloc_product`. Each factor is
bounded by the source cap.

### A loop without its bound

`src/antic/pattern.c:1019`, rule 6, minor, new. The loop that finds
`child` among `at->children` has no test against `at->count`. It rests on
`child->parent == at` meaning membership, which `add_child` at 286 keeps
today.

### Formatted text not checked for truncation

`src/antic/sema_const.c:191`, rule 9, minor, new. The two `snprintf` of
`"%.*e"` at lines 191 and 197 into `buffer[40]` ignore their result. The
longest output at precision 16 is 24 bytes, so a comment giving the bound
satisfies the rule.

### Returned memory without its owner

`src/antic/pattern.c:9`, rule 11, minor, new. `compile` returns a handle
the caller gives to `anti_rt_regex_free`, with no comment. `new_node` at
275 and `opaque` at 539 return memory of the pool without saying so.

### Releases repeated before each return

`src/antic/parser_item.c:15`, rule 12, minor, new. `class_item` and
`variant_item` free their lists before each return instead of in one
cleanup block. Every path releases both lists. `block_doc` at
`lexer.c:551`, `577`, `586` and `598` releases `raw` before four returns.
`sema_check` at `sema.c:2946` frees `module_scope.entries` and not
`.narrowed`, which `sema_leave_scope` frees for every other scope. The
module scope never narrows.

### Counters of `int`

`src/antic/parser.c:349`, rule 16, minor, new. `sync_item` and
`parser_statement_clause` at `parser_clause.c:53` count braces and
parentheses in `int`, which the source cap keeps in range.

### Smaller gaps in the readers

`src/antic/modpath.c:93`, rule 14, minor, new.

- A source whose name is the suffix alone, `.anti` or `dir/.anti`, has
  `rest == end` before the loop at `modpath.c:93` and gives an empty
  module path with `true`.
- `coded_char` at `pattern.c:341` reads up to three octal digits after
  `\0`, where PCRE2 reads two (`\0dd`). So `[\0777]` is one character
  outside ASCII to the check and `\077` followed by `7` to PCRE2. Likely:
  PCRE2's rule is from its documentation, not run.
- The scan for `:]` of a POSIX class at `pattern.c:492-495` runs to the
  end of the pattern, where PCRE2 stops at a `]` and reads `[:` as two
  characters. For `[[:a]b(c+)+` the reader takes the rest of the pattern
  as the class, and the nested repeat after it is never seen, so the check
  `exponential-pattern` passes it. Likely, for the same reason.
- `lexer_token_category` at `lexer.c:1566` has no case for
  `TOKEN_PATTERN`, and the `kinds` table gives it `CAT_OTHER`. So
  `re"..."` prints as `symbol` in `--dump-tokens` where the other literals
  print `string_lit`. No dump source holds a pattern literal.

### Refusals and paths without a test

`src/antic/sema_copies.c:90`, rule 15, minor, new. No file of `tests/`
holds "compiles no copy", the six forms of `copy_failed`. None holds "is
atomic and needs no lock" of `sema_safety.c:205`. None holds "names no
group of a match" of `sema_pattern.c:695`, which the second audit's fix
added. The two parser inputs of the major findings above are missing
from `depth_limit`, and the lexer's `error_cap` claims a case it lacks.

### Functions that stand outside the file their sentence names

`src/antic/sema.c:1925`, structure, placement, minor, new. By the
sentences of "Files of the checker" and the headers of the files:

- `sema.c`: `check_link_names` 1925 and `check_provides`, the checks of
  what crosses to C. `check_simd_struct` 638, the declaration rules of a
  simd struct, while `sema_simd.c` holds no second copy.
- `sema_const.c`: `sema_holds_class` 1131, a predicate on types that
  lowering calls from three files, kept here because `sema_const_symbol`
  asks it once.
- `sema_stmt.c`: `sema_fills` and `sema_filled_somewhere` 116-158,
  class-model queries for `sema.c`. `sema_value_view`, `sema_place_view`
  and `mark_view` 1920-1982, expression analysis for lowering.
  `sema_check_main` and `sema_check_test_block` 3049-3089, signature
  checks of items.
- `sema_call.c`: `sema_arm_fallthrough` 499, a `switch` helper.
  `literal_complete` and `sema_field_takes_literal` 460-533. The field
  inits of struct, class and variant literals 2178-2229 and 2539-2668,
  which the sentence gives `sema_expr.c`. `sema_resolve_origin` 1021, the
  `fail` statement.
- `sema_expr.c`: `destroy_of_local` 485, a move rule. `sema_descends_from`
  1224, a class-chain query five files use.
- `sema_pattern.c`: `sema_if_let_none` 781 rewrites `if let` on any value
  that may be `none`, "a match among them" by its own comment.
  `sema_hash.c`: `sema_no_order` 564, a message fragment about `lt` for
  `sema_generic.c`.
- `sema_pattern.c:11` includes `../rt/regex.h` for `anti_rt_regex_piece`
  beside `pattern.h`, the front end's door to the pattern library.
- `sema.h:8` includes `antic.h`, the record of what the `anti` tool may
  use, for `struct package` at `antic.h:134`. The include graph does not
  count it against the direction. The front end's interface struct still
  depends on the tool-facing header for one record.

### Two context fields kept outside the context

`src/antic/sema.c:973`, structure, state saved by hand, minor, new.
`c->value_result` is saved and restored by hand at lines 973-996 and set
and reset at 1328-1333. `c->plain_fns` is counted up and down at 1307
and 1318. That is the pattern the DESIGN at `sema_checker.h:68` says
`struct context` replaced after S04. Both restore on every exit today.
The hazard is the next early return. The DESIGN's own claim holds:
`c->ctx` is written at `sema.c:370` and `375` alone.

### Two readings of one grammar that have drifted

`src/antic/parser_stmt.c:165`, structure, duplicated logic that has
diverged, minor, new.

- `arm_body` admits `EXPR_CALL` and `EXPR_SYNC_OP` as the statement of an
  arm at lines 165-168. `statement_level` at 764-777 also admits
  `EXPR_FREE`, `EXPR_OBJECT` and `EXPR_JOIN` and reads a `catch` after a
  call. `A => free(p)` and `A => f() catch e { }` are refused as "a call,
  an assignment or a block" where `free(p)` stands as a statement. No
  test writes either form, and the specification was not read for it.
- `scan_types` at `parser_type.c:71-74` skips `keep`, `concurrent` and
  `own` before a parameter type and not `lent`. `type_level` reads `lent`
  before a tuple element at 326 and a parameter at 377. So
  `f<fn(lent *T)>(x)` reads as a comparison.
- `stmt_returns` at `sema_stmt.c:176` and `stmt_leaves` at 216 enumerate
  the same statement shapes and differ on a bare block, which the second
  descends and the first does not. `docs/decisions.md:216` lists `return`
  and an `if` chain, and 1448 adds `sync`. So `stmt_returns` follows the
  rule and `stmt_leaves` is the looser one. `sets_stmt` at 2374 is a
  third enumeration, the one that treats `select` and `try`.

### Small things of structure

`src/antic/parser_stmt.c:12`, structure, minor, new.

- `parser_is_assign_op` reads two ranges of `enum token_kind` as
  contiguous. The builtin-type range carries a comment to that effect at
  `lexer.h:36-37`, and these two carry none.
- `TOKEN_ATOMIC`, `TOKEN_DISPATCH`, `TOKEN_JOIN` and `TOKEN_JOIN_ALL` stand
  under "Operators and punctuation" at `lexer.h:72-74` and are keywords.
- `parser_params` and `expressions` return NULL for an empty list and for
  a failure, so every caller tests `p->panic` instead of the result.
- `parser_doc_before` with `TOKEN_NOTE` at `parser_item.c:210` marks the
  `//#` of a variant case taken and drops the text. `struct variant_case`
  has no `note`, so the note is neither kept nor reported.
- `--dump-ast` prints `lent` for a parameter of a function type and not
  for a tuple element. It prints none of `?fn`, `may fail`, `own`, the
  `member` after `is`, `as?` against `as` or the `pattern` flag of a
  `for`, so a dump test cannot pin those decisions of the parser.

### Comments above the wrong item

`src/antic/parser.c:177`, rule 27, minor, new, the pattern the second
audit reported at other places. The DESIGN on `alloc` and `free` as
member names at `parser.c:177-181` stands above `parser_is_field_name`
and describes `parser_expect_member_name` at 191. The DESIGN on `worker`
at `parser_item.c:291-293` stands above the `abstract` one, and the code
is at 340. The DESIGN on the f16 read at `sema_expr.c:2432-2435` stands
above `value_expected` and describes `sema_check_expr` at 2478. The DESIGN
on `dup`, `delete` and `destroy` at `sema_expr.c:2301-2305` stands above
`case EXPR_ATOMIC`. The DESIGN on `Circle(2.0)` at `sema_call.c:1202-1209`
stands above `required_params`, and `check_construct` is at 1289. The
DESIGN on what a call carries for a generic at `sema_call.c:1542-1546`
stands above `refuse_field`. The DESIGN on `for` at `sema_stmt.c:2070-2072`
stands above `case STMT_YIELD`.

### Stale sentences in comments

`src/antic/parser_item.c:345`, documentation, a stale name or sentence in
a comment, minor, new.

- `parser_item.c:345-347` says `packed` stands "only directly before
  struct, union or variant", and `ast.h:925` lists `ITEM_STRUCT,
  ITEM_UNION, ITEM_VARIANT`. Line 351 accepts `class`, the object model
  names `packed class`, and `test_parser.c:920` parses one.
- `sema_expr.c:171-173`: the conversion to a base is "the one implicit
  conversion of the language". `sema_require` holds four: to a base, to
  an interface at 190, T to `?T` at 742 and the forms of a function type
  at 647.
- `sema_expr.c:2301-2305`: "All three need a class pointer". The code
  takes `dup` of a type parameter, a class value, a struct, a tuple and a
  variant, and `destroy` of a pointer to any type. Each has a DESIGN of
  its own at 2352 to 2393.
- `sema_operator.c:130-131`: "the nineteen the operator table holds: the
  fourteen operators and the five language hooks", over a list of twenty.
- `types.h:432-440`: the compiler "declares two more of the module
  itself", `Object` and `Job`. `types.c:720-734` lists nine.
- `lexer.c:1529-1531`: the names cache is safe because "antic runs on one
  thread and starts none". `main.c:574` runs the compiler on a work
  thread through `platform_run_on_stack` while the main thread waits. The
  cache is still safe, and the reason is not the one written.
- `lexer.h:199-201` lists the groups of a token dump without `eof` and
  `error`, which the function returns.
- `parser_parser.h:23-26`: "one mistake produces one message". `sync_item`
  counts braces from the error. So an error in the header of a class
  member stops at the next `fn` of the body and reads it as an item. One
  mistake gives two or three messages.
- `parser_decl.c:100-102`, `307-308` and `356-357`, `ast.h:884` and
  `ast_dump.c:899` say "struct" where a class or an enum body is meant,
  since a struct body holds fields alone.
- `ast.h:207-217`: `pos` of a handler carries no comment. It means the
  `catch` word for `catch { }`, `catch fatal` and `catch none`, and the
  bound name for `catch e { }`, which `read_handler` at
  `parser_stmt.c:307` and `340` writes.
- `sema_checker.h:280-282`: `sema_declare` takes a `duplicate_message`
  that must hold exactly one `%.*s`, which `sema.c:321` fills and
  `-Wformat-nonliteral` reports. All nineteen callers do, and the
  contract is written nowhere.
- `sema_stmt.c:2902`: `snapshot_copies` keys on the enum order from
  `TYPE_BOOL` to `TYPE_F64` without a comment that the order is relied on.

### DESIGN comments without a reason

`src/antic/sema_expr.c:1984`, documentation, a DESIGN comment that gives
no reason, minor, new. `data/design-comments.txt` marks 158 of the 586
DESIGN comments of these files as holding no reason word. A sample reads
as description. "`e[i]` on a type with the `index` hook is the call
`e.index(i)`" at `sema_expr.c:1984`. "The check of the changes of a
collection" at `ast.h:233`, which names the four fields and not why they
are on the node. "`defer stmt` records a statement that runs at every
exit" at `parser_stmt.c:644`. The five one-line DESIGNs of
`parser_item.c:291-297`. Another part of the sample gives its reason in a
form the marks miss. The list is evidence for a pass over them, not a
count of defects.

### Comments that cite the book as the authority

`src/antic/sema.h:196`, documentation, minor, new. Thirteen comments send
the reader to a chapter of the book for the rule they apply: `sema.h:196`
"the rules of chapter 2", `sema.c:547` and `2503` ("the C rules of
chapter 18"), `sema_const.c:328` and `888`, `sema_expr.c:14` and `1191`,
`sema_stmt.c:175` and `3049`, `types.h:14`, `parser_parser.h:23`,
`lexer.c:197-201` (a history of chapters 2 and 22) and `ast.h:1143` with
`ast_dump.c:8`. CLAUDE.md names the two specifications as the authority
and says the book is not a concern here. A reader cannot follow the
reference in this repository.

### Stale names and sentences in the notes

`docs/notes/check.md:76`, documentation, a stale name or sentence in a
note, minor, new. Each gives the note's wording and what the code does.

- `check.md:76`: "`sema_doc_warnings` of `src/antic/sema_export.c` holds
  them". It stands in `sema_doc.c:474`.
- `own-and-lent.md:43`: "`note_move` reads the `own` list of the
  function". The function is `sema_note_move`, `sema_value.c:620`.
- `variants.md:17`: "The format is version 44." `own-and-lent.md:72-73`:
  "version 64" and "from version 67". `generics.md:7`: "the format version
  is 61". `antl.h:19` holds 81. Each sentence states a past version as
  current.
- `sema.md:10`: "Until `modules.md`, `import` and qualified names report
  `cannot find module`." Modules are built. The message stands for a
  module not among the libraries, `sema.c:733` and `1827`.
- `parser.md:9`: `parser_expect_member_name` is said to read the name
  after `fn` and after `.`, taking an identifier, `alloc` or `free`.
  `parser_decl.c:407` and `422` and `parser_expr.c:742` call
  `parser_expect_function_name`, which takes `union` as well
  (`parser.c:208`). `parser_expect_member_name` reads the name of an
  `inject` field alone, `parser_item.c:151`.
- `closures.md:87-89`: "`parser_fn_param_marks` reads `own` after `keep`".
  `parser_decl.c:43-57` reads it after either mark.
- `lexer.md:7`: "the lexer records a diagnostic, emits `TOKEN_ERROR` and
  continues" holds for the first hundred errors. The next one stops the
  run, and invalid UTF-8 or a source over 64 MiB stops it before any
  token. `lexer.md:7` also says `re` is read "as a raw form". The entry
  is `FORM_PATTERN`, a form of its own at `lexer.c:1001`.
- `patterns.md:37-38`: the methods `sema_pattern_call` takes are listed
  as `matches`, `find_all`, `replace`, `split`, `group` and `took_part`.
  `is_method` at `sema_pattern.c:621-627` also takes `patch`, `to_bytes`
  and `to_text`, which lines 82-87 describe.
- `generics.md:35`: "An operator, a `for` and `e[i]` on a value of a type
  parameter are open nodes." `open_node` at `sema_copies.c:955-974` also
  opens `x.hash()` and the default `==` of one.
- `own-and-lent.md:26-29`: the parser section names `lent` before a
  parameter and before the type of a parameter of a function type. It
  does not name `-> lent *T` of a result (`parser_decl.c:74-81`) or
  `lent` before a tuple element (`parser_type.c:326`).
- `lexer.md:9`, `parser.md:8` and `sema.md:11` cite "chapter 1" for the
  listing their tests pin. The tests exist: `dump_tokens_scale`,
  `dump_ast_scale` and `dump_types_scale` from `antic_dump_test`.

### Functions past a threshold

Rule 18, minor, still open. `data/thresholds.txt` lists 42 functions of
these files. The judgements, from the reading. A split would read better
for these:

- `statement_level` (410 lines, nest 5): the `for`, `switch`, `select`
  and trailing expression statement each read as a helper, as
  `let_or_const` and `if_let` already do.
- `primary` (356, nest 5): the `TOKEN_IDENT` case at 333-419.
- `postfix_links` (104, nest 6): the `[` branch at 679-718 takes the nest
  to 4.
- `item_level` (298): the struct and enum cases, as `class_item` and
  `variant_item` are. `class_item` (166): the field loop that `item_level`
  half repeats.
- `check_expr_inner` (641): the `EXPR_NAME`, `EXPR_STRUCT_LIT` and
  `EXPR_OBJECT` arms are each a function's worth, which leaves about 330
  lines of dispatch.
- `resolve_type_inner` (206): `TYPEX_NAMED` and `TYPEX_FN`.
- `sema_generic_call` (151, 6 parameters): the map builder, the inference
  loop and `finish_copy`.
- `xe` (229): its three recheck blocks are one helper. `xs` (185, nest 5):
  the `STMT_ASSIGN` and `STMT_FOR` cases.
- `patch_call` (166): the `[]byte` form and the pattern form.
- `check_let` (149): the marks on the declared symbol. `check_for` (146):
  the element type of the `over` forms.
- `parse_group` (129, nest 4): the `(*VERB)` block and the flag tail.
- `sema_check_binary` (185): a comparison check and an arithmetic check
  with one shared prefix.

These follow the shape of their problem: `dump_expr`, `dump_stmt`,
`dump_module`, `walk_children`, `type_level`, `parser_parse`,
`member_level`, `check_stmt`, `check_switch`, `sets_stmt`, `check_unary`,
`sema_require`, `check_assign`, `function_type_of`, `print_type`,
`sema_eval_const`, `same_signature`, `subst`, `doc_name_known`,
`sema_interface`, `sema_check_export` and `types_interface_member`.

The parameter lists: `sema_check_field_inits` (8) passes two array-count
pairs and a literal whose fields every caller passes again. `sema_check`
(9) and `sema_doc_warnings` (7) are the inputs of one compilation and
would read as a struct. `add` of `diagnostic.c` (8) is the one sink of
four wrappers and follows its record. `types_fn_flagged` (7) is a
constructor whose flags are its key.

### A file past 3000 lines

Rule 19, minor, returned. `sema_stmt.c` is 3089 lines, from 2962 at the
closure of the second audit, as the tool pass records. The parts that do
not fit "statements and function bodies" are the splits:

- the anonymous functions, lines 2623-3047 with `check_closure_lifetime`
  at 414-465, about 480 lines, as a `sema_closure.c`,
- the definite assignment of `construct`, 2241-2542, about 300 lines,
  beside `sema_class.c`,
- the views by `as`, 1920-1982, into `sema_expr.c`,
- `sema_fills` and the checks of `main` and `tests` into `sema.c`.

The first alone brings the file to about 2600 lines.

## False positives

The analyzer warnings on these files, each with its reason.

- `ast_dump.c:54`, `name` NULL: `label_name` at 52-57 guards it since S16.
- `sema.c:623`, `e->pos`: `e` is NULL for `_` without `: 0` alone, which
  returns at 600, and the caller at 2470 passes `bits` or a unit break.
- `sema.c:2707`, `type->item_exported`: `declare_items` gives every type
  item with a symbol a type, and 2703-2706 filters those kinds.
- `sema_call.c:565`, `s->generic`: every caller passes a non-NULL `s`.
  Moving the kind test of 569 above it would make that plain.
- `sema_call.c:920`, `s->kind`: `descends_or_copies` with NULL at 882 is
  false and `method_call` returns at 883.
- `sema_class.c:370`, `416`, `515` and `835`, `t->field_count`:
  `types_has_fields` at `types.c:1869` returns false for NULL and gates
  each call.
- `sema_pattern.c:302`, `with->kind`: reached when `arg == with` alone,
  and the parser writes no NULL argument.
- `sema_safety.c:149`, pointer subtraction: `types_find_field` searches
  `s->fields` alone, so `f - t->fields` lies in one array.
- `sema_simd.c:158`, division by zero: `most` starts at 1 and each lane
  is at least 1 byte, since `check_simd_struct` at `sema.c:649` refuses a
  lane of no bytes before layout.
- `sema_stmt.c:206`, `b->count`: every block that reaches `block_returns`
  is one the parser made, and the `else` of an `if let` is always written
  at `parser_stmt.c:216-225`. Likely: no NULL test stands at 2551 or 2591.
- `sema.c:321`, `-Wformat-nonliteral`: the format is the
  `duplicate_message` of `sema_declare`, which every caller spells with
  one `%.*s`. The stale-sentence finding names the undocumented contract.

## Fixes of the earlier audits that hold

Each was read again.

- The placeholder reads the token kind only when a token was pushed.
  `LEX_SOURCE_MAX`, `LEX_FORMAT_DEPTH_MAX` and `LEX_ERRORS_MAX` stand,
  with the gap of the major finding. `alloc.c` owns the heap memory and
  no allocation is checked twice. `string` and `interpolated` share
  `open_quote` and `content_char`. `text_vformat` marks a cut. A raw NUL
  in `'...'` is refused.
- `parser_descend`, the chain reach, the depth of the angle scan and
  `parser_nested_type` bound the parser, with the two gaps above. The
  inner parser of a placeholder has its `origin`. The dump of
  `alloc T { }` and the `%.*s` guard stand. The three `names[]` tables
  carry `_Static_assert`. Every switch of `ast_dump.c` and `ast_walk.c`
  covers the 38 expression kinds, the 22 statement kinds and the 10 type
  kinds.
- `sema_enter` and `sema_leave` are the only writers of `c->ctx`.
  `types_nest` and `sema_check_nesting` refuse a type that holds itself.
  An enum base that is no integer is refused. `sema_chain_prepare` takes
  chains apart. `CONST_ELEMENTS_MAX` and `CONST_DEPTH_MAX` bound the
  constants. `CONST_DEFAULT` keeps a field default. Text constants compare
  by their bytes.
- `sema_is_place` refuses a bound function. The base of a type member is
  typed. `checked` is set at its five sites and honoured at
  `sema_check_field`, `method_callee` and `check_collect`.
  `literal_complete`, `provides` and the owning walks keep a `ptr_set`.
  `check_try` enters a `try_record` through `sema_enter`.
- The copies are checked for cycles and bases when filled, bounded by
  `COPY_COUNT_MAX` and cut at `COPY_NAME_MAX`. A nested type of a refused
  generic class is skipped. The calls of `anti.regex` are held against
  the signatures of the library by `sema_std_function`. The group number
  is `int64_t` in `pattern.h` and `sema_pattern.c`. The doc names are
  `types_declares`. `sema_copy_in_chain` and `sema_copy_name` have one
  definition each. No cast of `sema_safety.c` writes through a found
  field.
- Nothing of the native back end remains in these files.

## Not verified

- The two NULL reads of the first severe finding, the parser hang and
  the reach of a placeholder were traced and not run. So was the overflow
  of `pattern.c`.
- That PCRE2 compiles `(a{65535}){65535}(b{65535}){65535}`, reads two
  digits after `\0` and ends a POSIX class scan at `]`: from its
  documentation and sources as remembered, not run.
- Whether the tree depth the reach finding allows overflows the 64 MB
  work stack under each build.
- Whether `A => free(p)` is meant to be refused as an arm. The
  specifications were not read for it.
- Whether the "two words" of `types.h:221` is stale was read from the
  specification and the checker, not from lowering.
- Whether the walks of `sema_value.c` and `sema_hash.c` over the fields
  of a class tolerate the NULL base field of the first severe finding.

## The sentence of each file

One sentence per file as read from the code, and what the file's own
header says. "Header: the same" means the first comment of the file says
it. "Header: none" means no comment describes the file.

- `lexer.c`: splits a validated UTF-8 source into tokens under caps on
  size, nesting and errors. It decodes literals and doc comments and
  lexes each `{expr}` of an interpolated literal with a nested lexer.
  Header: none, the first comment describes the `kinds` table.
- `lexer.h`: the token kinds, the token and format-piece records, and
  the five entry points of the lexer. It also holds the three caps of the
  lexer and the depth limit of the parser. Header: none.
- `parser.c`: the helpers every parser file shares, and `parser_parse`,
  which reads the lines at module level and hands items to `parser_item`.
  The helpers are tokens, errors, depth and chain reach, lists, doc
  comments and recovery. Header: the same.
- `parser_clause.c`: reads `allow` and `unchecked` and records the source
  each covers. Header: the same.
- `parser_decl.c`: the parts of a signature and of a body. Those of a
  signature are marks, parameters, result, `may fail` and type parameters
  with constraints. Those of a body are members, nested types, a field's
  guard and clauses. Header: the same but "fields of a body", which
  `parser_item.c` reads.
- `parser_expr.c`: expressions, from the primary forms through postfix,
  unary, casts and precedence climbing, with `f"..."` and anonymous
  functions. Header: the same.
- `parser_item.c`: the items of a module, a class and a variant with their
  bodies among them, each with its header clauses. Header: the same.
- `parser_stmt.c`: statements and blocks, the handlers of `catch`
  included. Header: the same.
- `parser_type.c`: types, and the scan that tells type arguments after `<`
  from a comparison. Header: the same.
- `parser.h`: the one entry point of the parser. Header: the comment of
  `parser_parse`, enough for a two-line header.
- `parser_parser.h`: the state of the parser and the functions its seven
  files share, grouped by file. Header: the same.
- `ast.h`: the syntax tree of one module, the fields the checker adds to
  it, and the entry points of the dump and the walk. Header: the first.
  The dump and the walk carry their own comments.
- `ast_dump.c`: writes the tree, untyped or with the checked types, one
  node per line for `--dump-ast` and `--dump-types`. Header: none, the
  first comment says how node names are chosen.
- `ast_walk.c`: the one walk over a body that every analysis of the
  checked tree uses. Header: the same.
- `types.c`: builds and interns the checked types of a compilation and
  answers the predicates on them. It declares the types of `anti.lang`
  the compiler knows, names types and measures how deep their values
  nest. Header: none, `types.h` has it.
- `types.h`: the same, with the names of `anti.lang`, `anti.text`,
  `anti.simd`, `anti.mem` and `anti.collection` the compiler knows.
  Header: "The checked types of a compilation" and nothing of the names.
- `sema.c`: the shared helpers of the checker, every declaration pass over
  the items of a module, and `sema_check`, which runs the passes in order.
  The helpers are scopes, lookup, narrowing and the resolution of types
  and signatures. The passes run from the root to the defaults. Header:
  none, the first comment is the DESIGN of the one-pass checker. The
  declaration passes, lines 1497-2880, are the natural `sema_declare.c`
  before rule 19 bites at 2948 lines.
- `sema.h`: the interface of the checker to the driver, lowering and the
  header. It holds symbols, constants, the interface of a module,
  `sema_check` and the queries the later passes ask. Header: none.
- `sema_checker.h`: the state of the checker and the functions its files
  share, grouped by defining file. Header: the same. The `sema.c` line
  leaves out `provides`, `link` and the simd declaration.
- `sema_chain.c`: takes a chain of declarations that name each other
  apart and resolves them leaves first, so no resolution recurses more
  than a link. Header: the same, as a reason.
- `sema_class.c`: checks the class model of every type with fields once
  the module is declared, and gives lowering and the C header the primary
  table. The model is interfaces, abstract fields, contracts and the
  bodies that fill them, redeclaration, hooks, operators, `construct`,
  qualifiers and nested types in signatures. Header: the same without the
  table.
- `sema_const.c`: evaluates constant expressions and gives each constant
  symbol its value once, in dependency order. Header: the same. It also
  holds `sema_holds_class`.
- `sema_expr.c`: checks expressions and the conversions a value makes
  where it stands. The kinds are the forms of function values, places,
  literals, the unary operators, iteration and `to_slice`, casts,
  interpolation, `?.`, `dup`, `delete`, `destroy`, `alloc` and `free`,
  and the dispatch over them. Header: the same by omission. The
  conversions, a quarter of the file, and the forms of function values
  are not named.
- `sema_call.c`: checks calls and members. Those are fields and their
  visibility, functions of a class and the table they go through,
  `construct` and the handlers of failing calls. The field inits of
  struct, class and variant literals, `mul_high` and the calls of a plugin
  library stand here too. Header: the same, less the literals.
- `sema_operator.c`: checks the binary operators, `in` and `??` through
  the operator table, and finds the `operator fn` functions and language
  hooks of a type. It also builds the checked calls of a module's
  `operator fn eq` and `hash` for a class. Header: the same.
- `sema_stmt.c`: checks statements and function bodies, and walks the
  functions a worker reaches. The statements are narrowing, assignment
  and `set_index`, `switch` and `select`, `sync` and `try`. The bodies
  bring the definite assignment of `construct`, anonymous functions with
  their captures and snapshots, the views by `as`, `main` and the test
  blocks. Header: the same, less the views and `sema_fills`.
- `sema_value.c`: the value rules: what owns something, what moves and
  the copies that `=`, `let`, a literal and a call refuse. Header: the
  same.
- `sema_safety.c`: the checks of the thread-safe classes. Those are the
  kind of a class, its base, the hidden lock, the guards and the reach of
  a field. The writes that make a field none of the three and the
  pointers into the fields follow. Header: the same.
- `sema_simd.c`: the checks of simd structs: operators lane by lane and
  `as` with the C layout it compares. The built-ins of `anti.simd`, of a
  simd struct and of its values follow. Header: the same.
- `sema_thread.c`: the atomic operations, `Mutex.new` and `destroy`, the
  channel operations, and the workers of `parallel` and `dispatch` with
  `join`. Header: the same plus `sync`, which it does not hold.
- `sema_generic.c`: the checker's generics, and the strip of generics
  before the later passes. The generics are type parameters and their
  constraints, `constraint` sets and `type` aliases, and the copies of
  generic types with their bounds. Substitution, the inference of a call
  and the hooks of a type parameter follow. Header: a DESIGN that ends in
  two stale sentences.
- `sema_copies.c`: the compiled copies. It clones the checked tree of a
  generic per use with the arguments in place and rechecks the open
  nodes. It rewrites the calls of code that is not generic to name the
  copies. Header: the same.
- `sema_hash.c`: the checker's part of hashing and of the default `==`.
  It decides whether a type takes the default or its own function, builds
  the calls of the parts a default reaches, refuses a concurrent class,
  and writes the default `equals` and `hash` of each class. Header: the
  same.
- `sema_pattern.c`: the methods of `str`, `[]byte` and a match that become
  calls of `anti.regex` and `anti.text`, the groups of a literal as fields,
  the pattern literal with its checks, and `Regex.compile`. Header: the
  same. It also holds `sema_if_let_none`.
- `sema_export.c`: the checks of what crosses to C, export items and the
  pointers of an `extern fn`, and the interface of a checked module.
  Header: the same.
- `sema_doc.c`: the doc warnings of `anti check`, reading the module alone
  without `struct checker`. Header: the same.
- `pattern.c`: checks a pattern literal. It compiles the literal through
  the PCRE2 wrapper of the runtime and counts and numbers its groups. It
  reads the literal into a tree of its own for the check
  `exponential-pattern` and the least match length. Header: none.
  `pattern.h` names the compile and the check and not the groups or the
  length.
- `pattern.h`: the same. Header: partial, as above.
- `diagnostic.c`: appends a formatted error, warning, safety check or doc
  warning to a growable list. Header: none.
- `diagnostic.h`: the record of one diagnostic with its stable name, and
  the list of one compilation. Header: none, the first comment is the
  DESIGN of the names.
- `warnings.c`: the one table of warning and safety-check names, the
  application of `allow` and `unchecked`, and the promotion of warnings to
  errors. Header: none.
- `warnings.h`: the interface of the same. Header: none.
- `modpath.c`: derives a module path and a root-relative file name from a
  source path under the search roots, and reads the segments of a module
  path. Header: none. `modpath.h` has it.
- `modpath.h`: the same. Header: the same.
