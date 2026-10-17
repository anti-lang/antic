# The regions of the front end that no test reached

A step of the test run, in three sessions on 2026-10-12 and 2026-10-13, on
`main`. Each ran `./c coverage`, read the regions that the suite did not
reach in `src/antic/lexer.c`, the seven parser files, `pattern.c`, `types.c`
and the eighteen `sema*.c` files, and wrote tests for them. No file under
`src/` changed. The step is finished: every unreached region of the 28 files
has a test or a reason below.

## What the step added

- The first session: 304 refusals and 8 programs in `tests/matrix/`, 9
  programs in `tests/errors/`, four dumps and the cases of `unit_lexer`
  that need bytes a checkout cannot hold.
- The second session: 110 refusals and 45 programs in `tests/matrix/`. Each
  refusal holds one message.
- `tests/errors/` gained `plugin_instance_of_a_long_path` and the silent
  `doc_whole_path`, which needs `--doc-warnings`.
- `matrix_classes_call_through_a_function_field_no_reflect` runs a matrix
  program once more under `--no-reflect`.
- The larger programs of the second session are
  `types_simd_cast_to_plain_structs` with one function per kind of field,
  `classes_construct_sets_through_sync_and_select` and
  `types_generic_iterable_after_another_interface`.
- The third session: 15 programs and 17 refusals in `tests/matrix/`, and ten
  refusals in `tests/errors/` that read a library file.
- Seven of the ten call `anti.regex`, `anti.text` and the library `items`
  where the file lacks what the call names. The damaged `anti.regex` of
  `tests/modules/anti/` gained `split_literal` and `find_all_literal`, and
  `items` the constraint `Labelled`, which names an interface.
- `refuse` of `tests/modules/lang_code_alone` fails inside `anti.lang`
  itself, so `antl_lang_code_alone` reaches the lookup of `StackTrace` among
  the items of the module. `passed` of `generic_limits` hands a nested type
  on to a generic function.
- `variant_wide_tag` runs `tests/runners/run_variant_wide_tag.cmake`, which writes a
  variant of 65536 cases, compiles it and runs it. The tag is a `u32`.

## The coverage of each file

The table counts the regions that no test reached at four times: at the
start of the step, after each of the first two sessions and now. It gives
the share of regions and of lines the suite reaches before and after the
third session. `build/drive/logs/c3-before-summary.txt` and
`c3-after-summary.txt` hold the rows of the third session,
`cv-before-summary.txt` those of the start and `c2-before-summary.txt` those
after the first session.

| File | Regions | Missed at the start | After the first | After the second | Now | Regions before | Regions now | Lines before | Lines now |
|---|---|---|---|---|---|---|---|---|---|
| `lexer.c` | 922 | 36 | 18 | 18 | 18 | 98.05% | 98.05% | 98.24% | 98.24% |
| `parser.c` | 424 | 26 | 4 | 4 | 4 | 99.06% | 99.06% | 98.56% | 98.56% |
| `parser_clause.c` | 138 | 14 | 0 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `parser_decl.c` | 541 | 52 | 3 | 3 | 3 | 99.45% | 99.45% | 99.29% | 99.29% |
| `parser_expr.c` | 752 | 78 | 2 | 2 | 2 | 99.73% | 99.73% | 99.76% | 99.76% |
| `parser_item.c` | 560 | 40 | 0 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `parser_stmt.c` | 699 | 86 | 11 | 11 | 11 | 98.43% | 98.43% | 98.82% | 98.82% |
| `parser_type.c` | 331 | 31 | 2 | 2 | 2 | 99.40% | 99.40% | 99.44% | 99.44% |
| `pattern.c` | 818 | 319 | 17 | 17 | 17 | 97.92% | 97.92% | 98.19% | 98.19% |
| `sema.c` | 1930 | 40 | 26 | 14 | 9 | 99.27% | 99.53% | 99.33% | 99.42% |
| `sema_call.c` | 2204 | 49 | 39 | 20 | 18 | 99.09% | 99.18% | 99.15% | 99.36% |
| `sema_chain.c` | 44 | 0 | 0 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `sema_class.c` | 954 | 23 | 19 | 10 | 10 | 98.95% | 98.95% | 99.76% | 99.76% |
| `sema_const.c` | 903 | 68 | 42 | 35 | 35 | 96.12% | 96.12% | 96.84% | 96.84% |
| `sema_copies.c` | 1002 | 46 | 46 | 41 | 33 | 95.91% | 96.71% | 95.92% | 96.45% |
| `sema_doc.c` | 472 | 36 | 5 | 3 | 3 | 99.36% | 99.36% | 99.50% | 99.50% |
| `sema_export.c` | 426 | 4 | 4 | 3 | 3 | 99.30% | 99.30% | 99.50% | 99.50% |
| `sema_expr.c` | 2056 | 59 | 53 | 34 | 30 | 98.35% | 98.54% | 98.53% | 98.83% |
| `sema_generic.c` | 1815 | 119 | 88 | 33 | 21 | 98.18% | 98.84% | 98.54% | 99.36% |
| `sema_hash.c` | 472 | 21 | 12 | 3 | 3 | 99.36% | 99.36% | 99.10% | 99.10% |
| `sema_operator.c` | 686 | 32 | 30 | 13 | 10 | 98.10% | 98.54% | 98.15% | 98.77% |
| `sema_pattern.c` | 674 | 55 | 33 | 16 | 6 | 97.63% | 99.11% | 97.85% | 99.19% |
| `sema_safety.c` | 542 | 29 | 26 | 17 | 17 | 96.86% | 96.86% | 97.45% | 97.45% |
| `sema_simd.c` | 407 | 48 | 42 | 1 | 1 | 99.75% | 99.75% | 100.00% | 100.00% |
| `sema_stmt.c` | 2307 | 130 | 76 | 42 | 17 | 98.18% | 99.26% | 97.88% | 98.96% |
| `sema_thread.c` | 465 | 30 | 23 | 11 | 7 | 97.63% | 98.49% | 97.78% | 98.67% |
| `sema_value.c` | 586 | 19 | 19 | 11 | 11 | 98.12% | 98.12% | 98.29% | 98.29% |
| `types.c` | 1313 | 67 | 24 | 23 | 17 | 98.25% | 98.71% | 98.57% | 98.78% |
| All 28 | 24443 | 1557 | 664 | 387 | 308 | 98.42% | 98.74% | 98.52% | 98.84% |

The checker files held 307 unreached regions before the third session and
hold 234 now, and `types.c` went from 23 to 17. The lexer, the parser files
and `pattern.c` did not change in it. Ten files changed: `sema.c`,
`sema_call.c`, `sema_copies.c`, `sema_expr.c`, `sema_generic.c`,
`sema_operator.c`, `sema_pattern.c`, `sema_stmt.c`, `sema_thread.c` and
`types.c`.

## The findings

`docs/audit/tests-findings.md` holds the rows. T100 to T105 are of the first
session.

- T100 to T102, minor: second messages of the parser after one mistake.
- T103, major: antic never ends on an `if` at level 256 of the nesting.
- T104, major: an injected `log.Logger` that is never called fails at the
  link in release mode.
- T105, major: a class whose tuple field holds a union gets a default `==`.
- T106, major: antic ends with a segmentation fault on a field default that
  is a class literal with a call in a field. `--front-end` takes the same
  program without a message. Two tests show it.
- T107, major: a length `-(-N)` of a generic struct stays `[--3]int` in its
  copy, and a literal with three elements is refused.
- T108, minor: the refusal of a copy names the element of a tuple `_0`.
- T109, minor: the default hash over a part whose `operator fn hash` is
  refused gets a second message at the call.
- T110, minor: the refusal of an aligned `export union` on a bitfield names
  the union a struct.
- T111, major: `==` of a `fn` and a `?fn` of one type is refused, where a
  `*T` and a `?*T` compare.
- T112, severe, of the third session: `let f = Base<i16>.make;` gives `f`
  the address of the first function of the module, and `f()` calls it. The
  program prints bytes of its memory. Three tests show it.
- T113, minor: a receiver with two paths to the interface of a constraint
  gets two messages in the copy of the generic.
- Two refusals of the third session show T096 again, a `for` over a type
  whose `operator fn iter` or `next` is refused. One shows T095 again.

## Regions no input reaches

The first session:

- `lexer.c`: the names of the literal kinds, of the end of the file and of
  a refused token in `lexer_token_kind_name`, a spelling too long for the
  cache, and the groups `eof` and `error` of the listing.
- `parser.c` 686 to 723, `parser_decl.c` 475, `parser_expr.c` 978,
  `parser_stmt.c` 313, 341, 422, 456, 500 and 503, `parser_type.c` 50: the
  caller read the token already. `parser_stmt.c` 294 is T103.
- `pattern.c` 51, 68, 378, 409, 497, 506, 514, 556 and 785: the reader runs
  on a pattern that compiled. 1107 and 1127: PCRE2 refuses the pattern as
  too large first. 1002, 1136 and 1149 stand after a switch that returns.
- `types.c` 41, 477 and 597 are guards. 447 and 462 follow a refusal.
  1478 to 1491 read the variant of a damaged library file.
- `sema_const.c` 218 and 229: a message writes a float only where it fits
  no integer. 810: no constant takes a length computed from `size_of`.

The second session, by file. A guard is a test for `NULL`, for a refused
type or for a kind that no caller passes.

- `sema.c`: 353, 420, 523, 531, 1006 and 1300 are guards. 1002: the copy
  of a generic refuses a constant where a type stands before the type is
  resolved. 1771: a parameter whose type is refused is not checked for
  `own`.
- `sema_call.c`: 148, 556, 671, 1050, 1199, 2260 and 2296 are guards. 1028
  and 1030 need an `anti.lang` without `Error`.
- `sema_class.c`: 148, 159, 1041 and 1069 are guards.
- `sema_const.c`: 132: no character meets a value of `size_of`. 209: the
  shortest digits that give a float back end in no zero. 423 and 640: the
  check of the expression reports the operation first, and a refused
  expression is not evaluated. 492: a constant pointer is `none`, so the
  two kinds never differ. 367, 603, 802, 1001, 1024 and 1108 are guards.
  101 and 110 are reached by the two tests of T106, whose antic crashes
  before it writes its profile.
- `sema_doc.c` 221: every item is an item of the module or a member of
  one, and the loop over the module finds its names first.
- `sema_export.c` 147 and `sema_hash.c` 346 are guards. `sema_hash.c` 532:
  a type without `==` ends the question, so none is asked again after it.
- `sema_expr.c`: 334, 941, 1226, 1756 and 2429 are guards. 523: a name
  with fields is a local or a parameter. 1170, 1349, 1962, 2060, 2307 and
  2311: a node the checker made is checked once. 1187 and 1959: `to_slice`
  exists only where the walk does. 1874: the parser reads `same<int>`
  without `(` as a comparison.
- `sema_generic.c`: 637: every hook name is answered before it. 737, 826,
  945, 1721 and 1983 are guards. 99: a type in a signature names a generic
  class with its arguments. 2028 to 2036: a hook is used inside a function.
- `sema_operator.c`: 176 and 314 are guards. 471 to 473 are T111. 494 and
  497: a literal beside a value of `T` is refused before.
- `sema_pattern.c`: 247: `matches` is the one method with one count, and it
  is 1. 694: the lexer refuses a number past `u64`. 858: no source of the
  tree holds a CR.
- `sema_safety.c`: 171, 275, 307 and 313 to 316 are guards. 480: an array
  has no `ptr`. 488 and 489: an array converts to no slice without `[..]`.
- These are guards as well: `sema_simd.c` 541, `sema_thread.c` 363 and 378,
  `sema_value.c` 43, 67, 144, 153, 188, 314 and 554, and `sema_stmt.c` 307,
  1108, 1924, 1956, 2480, 2882 and 2934. `sema_stmt.c` 1057 and 2598: an
  anonymous function gets these two messages from its own check.

The first session listed `sema_const.c` 703 here. It is reached:
`matrix_classes_default_repeats_over_a_constant_parameter_refused` holds it.

The third session, by file. It settles the lines the second session left.

- `sema_call.c` 372: the nodes a copy reads again name operators and hooks,
  which have no level, and their operands are read once, in the generic.
  2320: `Object` is declared in every module, so its lookup finds it.
- `sema_copies.c` 105: `ty` alone passes no name, and it reports for a copy,
  which has its generic. 196, 203 and 206: a field the checker recorded
  stands in a class of the chain, and its copy has the same fields. 451:
  `types_set_fields` gives every field of a copy the copy as its home.
- `sema_copies.c` 400, 648 and 715: every generic type of the module and of
  its library files has an item. A type nested in a generic class of a
  library stops at `ty` first, which is T095.
- `sema_copies.c` 600, 627, 687 and 725: the checker records the arguments
  of the copy on every call of a generic. 671: a callee that is a name has
  a symbol. 1002: the checker refuses a generic named as a value before a
  copy is made.
- `sema_copies.c` 876, 1257 and 1296: a slice and an array meet no `index`,
  `set_index` or `iter` constraint, so the copy reads `e[i]` and the walk as
  calls of a hook. 1299: the declaration of `iter` checks its iterator.
  935, 1419, 1442 and 1450 are guards.
- `sema_expr.c` 606: `own` refuses a `??fn`, so no quiet reading passes a
  parameter to an `own fn` place. 1056 is a guard.
- `sema_generic.c` 681 and 700: a hook the argument lacks is reported before
  the walking interface is asked. 1841: a constant parameter of the class
  always has a value.
- `sema_operator.c` 681: `==` and `!=` have a case of their own before the
  default. `sema_pattern.c` 730: both callers pass a type that may be `none`.
- `sema_stmt.c` 129: `sema_implemented_in` on the line before finds every
  sub-object the loop would. 152: a class of a library names no class of the
  module that imports it, since a cycle of imports is refused.
- `sema_thread.c` 39 and 84: the caller passes the name of an atomic
  variable or a field that it checked.
- `types.c` 167: the one caller asks `types_holds_lent` first. 1880 is a
  guard. `pattern.c` 36: the one caller passes a buffer of the size the
  runtime writes.

## Rules the documents do not state

The first session:

- A source that ends inside a block gets `` expected `}` `` twice.
- The check of exponential time refuses `(a+\b)+` and `(a+(?=b))+`.
- A generic call whose argument has another shape than the parameter gets
  the message that nothing in the arguments gives `T`.
- `operator fn eq` of a class that gives an `int`, and `operator fn eq()`
  at module level, are taken where they are declared.
- `delete(p)` and `destroy(p)` of a parameter `p: *T` without `own` are
  taken.
- `operator fn value(self) -> lent ?*int` is taken, and a simd struct takes
  `char` lanes.
- The `else` of a `let` that ends with a `switch` whose every arm returns
  is refused.
- `-(-size_of(H))` in a length prints as `--size_of(H)`.

The second session:

- `==` of a struct whose part has `operator fn eq(a: Key, b: int)` is
  refused with ``expected `int`, found `Key` `` at the `==` of the struct.
- A singleton class that declares `get` itself is taken, and `Config.get()`
  calls it.
- `operator fn eq(a: []int, b: []int)` is taken where it is declared, and
  `==` of two slices stays refused.
- Two bodies of one name, each qualified by another class of the chain, are
  refused as declaring the name twice. So are one qualified by an interface
  and one by the interface it inherits.
- `equals` of a class passes over an `operator fn eq` of its module that
  gives an `int` and compares the fields. It passes over one that takes by
  value a class with a Mutex as well.
- A sum of two constants past the range of its type wraps in the constant.
- A default `[0; N]` over a constant parameter of the class is refused, and
  the message names `size_of`.
- `Pair { left: 1 }` of a generic struct inside a generic function is
  refused, since nothing gives its type arguments.
- `destroy(&Config.get().part)` on a part of a singleton is taken.
- An abstract function without `self` is taken, and a `concrete fn` fills
  it without `self`.

The third session:

- A `switch` on an integer takes the arm `size_of(i64)` beside the arm `8`,
  and the first of the two runs for 8.
- `none` into an `own` field of type `?fn(...)`, as a default and with `=`,
  is refused with ``an `own fn` value is not copied by `=`, use `dup` ``.
- `operator fn eq<T, U>` whose `U` no operand names is taken where it is
  declared. `==` is then refused with the message that the call writes `U`
  out after the name.
- The module of a generic class gives it an `operator fn eq` with a
  constraint, a hook or an interface. `equals` of a copy whose argument
  does not meet it passes over the operator and compares the fields.
- A receiver with two paths to the interface of a constraint is refused in
  the body of the generic. The call that names the copy gets no message.
- A part of a concurrent class whose body declares `operator fn eq` compares
  through it in the default `==` of the class that holds it.
- `struct Loop { a: [size_of(Loop) + 1]int }` is refused as a struct that
  contains itself.
- `for x in &c` over a parameter with `Iterable<int>` is refused, since the
  value of such a walk is a copy.

## Where the task and the code differ

- `./c coverage` ran as `./c coverage part 1/2` and `part 2/2`, since one
  call ends after ten minutes.
- The matrix keeps no `tests` block and passes no option, so those
  refusals stand in `tests/errors/`.
- A source that does not lex has no canonical form, so the refusals of the
  lexer are cases of `unit_lexer`.
- The matrix reads no library file, so the refusals of the third session
  that need one stand in `tests/errors/` with a case in
  `tests/CMakeLists.txt`.
- A probe showed `Box<T>.plain()` refused inside the body of `Box<T>`. The
  last list of "Generics and collections" in `docs/decisions.md` records
  that for `SyncList<T>.new`, so it is no finding and got no test.

## Counts and checks

- The second session: 4135 tests on the host before and 4383 after, 350
  expected failures before and 361 after. Its logs are `c2-*.log`.
- The third session: 4383 tests before and 4456 after, 361 expected failures
  before and 368 after.
- Host suite: 4456 of 4456 passed, 368 of them expected failures, two
  skipped, in 451 s, in `build/drive/logs/c3-suite-host.log`.
- ASan and UBSan: 4455 tests each, in three parts, `ctest -I 1,1150`,
  `-I 1151,2200` and `-I 2201,4455`. Every part passed. `asan` took 276 s,
  360 s and 283 s, and `ubsan` 248 s, 345 s and 209 s. The second and the
  third part run the setups of their fixtures again, so the three counts are
  1150, 1062 and 2274. The logs are `c3-suite-asan-1.log` to `-3.log` and
  `c3-suite-ubsan-1.log` to `-3.log`.
- Coverage: `c3-before-1.log` and `-2.log`, `c3-after-1.log` and `-2.log`.
- Every tree built with zero warnings. `emit_identity` and
  `link_identity_macos-arm64` pass, and no expected file of either changed.
- The docs-style checker reports nothing on `CLAUDE.md`,
  `docs/audit/tests-findings.md` and this report.

## Proof

`git log --oneline -3` after the push of the tests of the third session,
before the commit of this report:

```text
8f2f170e Test a nested type of a library generic handed on to a generic function
a95b1361 Test two refusals of a generic body that the copy pass and the walk give
97f73a34 Test the regions of the checker that the second session left open
```

`git rev-parse HEAD origin/main` after that push:

```text
8f2f170ebb3b042e079418a3e6d6a62fcbe75c25
8f2f170ebb3b042e079418a3e6d6a62fcbe75c25
```

`git status --short` printed nothing then. The reply of the session holds
the three outputs after the last push, since the hash of this commit is not
known inside it.
