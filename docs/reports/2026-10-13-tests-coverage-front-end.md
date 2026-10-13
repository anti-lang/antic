# The regions of the front end that no test reached

A step of the test run, in two sessions on 2026-10-12 and 2026-10-13, on
`main`. Each ran `./c coverage`, read the regions that the suite did not
reach in `src/antic/lexer.c`, the seven parser files, `pattern.c`, `types.c`
and the eighteen `sema*.c` files, and wrote tests for them. No file under
`src/` changed. The step is not finished: see "Left for the next session".

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

## The coverage of each file

The table counts the regions that no test reached at three times: at the
start of the step, after the first session and now. It gives the share of
regions the suite reaches at the same three times. The last two columns
hold the share of lines before and after the second session.
`build/drive/logs/c2-before-summary.txt` and `c2-after-summary.txt` hold the
rows of the second session, `cv-before-summary.txt` those of the start.

| File | Regions | Missed at the start | Missed after the first | Missed now | Regions at the start | Regions after the first | Regions now | Lines after the first | Lines now |
|---|---|---|---|---|---|---|---|---|---|
| `lexer.c` | 922 | 36 | 18 | 18 | 96.10% | 98.05% | 98.05% | 98.24% | 98.24% |
| `parser.c` | 424 | 26 | 4 | 4 | 93.87% | 99.06% | 99.06% | 98.56% | 98.56% |
| `parser_clause.c` | 138 | 14 | 0 | 0 | 89.86% | 100.00% | 100.00% | 100.00% | 100.00% |
| `parser_decl.c` | 541 | 52 | 3 | 3 | 90.39% | 99.45% | 99.45% | 99.29% | 99.29% |
| `parser_expr.c` | 752 | 78 | 2 | 2 | 89.63% | 99.73% | 99.73% | 99.76% | 99.76% |
| `parser_item.c` | 560 | 40 | 0 | 0 | 92.86% | 100.00% | 100.00% | 100.00% | 100.00% |
| `parser_stmt.c` | 699 | 86 | 11 | 11 | 87.70% | 98.43% | 98.43% | 98.82% | 98.82% |
| `parser_type.c` | 331 | 31 | 2 | 2 | 90.63% | 99.40% | 99.40% | 99.44% | 99.44% |
| `pattern.c` | 818 | 319 | 17 | 17 | 61.00% | 97.92% | 97.92% | 98.19% | 98.19% |
| `sema.c` | 1930 | 40 | 26 | 14 | 97.93% | 98.65% | 99.27% | 98.30% | 99.33% |
| `sema_call.c` | 2204 | 49 | 39 | 20 | 97.78% | 98.23% | 99.09% | 97.98% | 99.15% |
| `sema_chain.c` | 44 | 0 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% | 100.00% |
| `sema_class.c` | 954 | 23 | 19 | 10 | 97.59% | 98.01% | 98.95% | 99.05% | 99.76% |
| `sema_const.c` | 903 | 68 | 42 | 35 | 92.47% | 95.35% | 96.12% | 96.21% | 96.84% |
| `sema_copies.c` | 1002 | 46 | 46 | 41 | 95.41% | 95.41% | 95.91% | 94.86% | 95.92% |
| `sema_doc.c` | 472 | 36 | 5 | 3 | 92.37% | 98.94% | 99.36% | 99.00% | 99.50% |
| `sema_export.c` | 426 | 4 | 4 | 3 | 99.06% | 99.06% | 99.30% | 99.50% | 99.50% |
| `sema_expr.c` | 2056 | 59 | 53 | 34 | 97.13% | 97.42% | 98.35% | 97.36% | 98.53% |
| `sema_generic.c` | 1815 | 119 | 88 | 33 | 93.44% | 95.15% | 98.18% | 95.80% | 98.54% |
| `sema_hash.c` | 472 | 21 | 12 | 3 | 95.55% | 97.46% | 99.36% | 96.84% | 99.10% |
| `sema_operator.c` | 686 | 32 | 30 | 13 | 95.34% | 95.63% | 98.10% | 96.61% | 98.15% |
| `sema_pattern.c` | 674 | 55 | 33 | 16 | 91.84% | 95.10% | 97.63% | 96.37% | 97.85% |
| `sema_safety.c` | 542 | 29 | 26 | 17 | 94.65% | 95.20% | 96.86% | 96.17% | 97.45% |
| `sema_simd.c` | 407 | 48 | 42 | 1 | 88.21% | 89.68% | 99.75% | 90.71% | 100.00% |
| `sema_stmt.c` | 2307 | 130 | 76 | 42 | 94.36% | 96.71% | 98.18% | 96.29% | 97.88% |
| `sema_thread.c` | 465 | 30 | 23 | 11 | 93.55% | 95.05% | 97.63% | 94.67% | 97.78% |
| `sema_value.c` | 586 | 19 | 19 | 11 | 96.76% | 96.76% | 98.12% | 96.80% | 98.29% |
| `types.c` | 1313 | 67 | 24 | 23 | 94.90% | 98.17% | 98.25% | 98.57% | 98.57% |
| All 28 | 24443 | 1557 | 664 | 387 | 93.63% | 97.28% | 98.42% | 97.41% | 98.52% |

The checker files held 583 unreached regions before the second session and
hold 307 now. The other ten files did not change in it.

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

## Where the task and the code differ

- `./c coverage` ran as `./c coverage part 1/2` and `part 2/2`, since one
  call ends after ten minutes.
- The matrix keeps no `tests` block and passes no option, so those
  refusals stand in `tests/errors/`.
- A source that does not lex has no canonical form, so the refusals of the
  lexer are cases of `unit_lexer`.

## Counts and checks of the second session

- Tests on the host: 4135 before, 4383 after. Expected failures: 350
  before, 361 after.
- Host suite: 4383 of 4383 passed, 361 of them expected failures, two
  skipped, in 438 s, in `build/drive/logs/c2-suite-host.log`.
- ASan and UBSan: 4382 tests each, in three parts, `ctest -I 1,1150`,
  `-I 1151,2200` and `-I 2201,4382`. Every part passed. `asan` took 267 s,
  359 s and 265 s, and `ubsan` 236 s, 344 s and 201 s. The second and the
  third part run the setups of their fixtures again, so the three counts
  are 1150, 1062 and 2201. The logs are `c2-suite-asan-1.log` to `-3.log` and
  `c2-suite-ubsan-1.log` to `-3.log`.
- Coverage: `c2-before-1.log` and `-2.log`, `c2-after-1.log` and `-2.log`.
- Every tree built with zero warnings. `emit_identity` and
  `link_identity_macos-arm64` pass, and no expected file of either changed.
- The docs-style checker reports nothing on `CLAUDE.md`,
  `docs/audit/tests-findings.md` and this report.

## Proof

`git log --oneline -3` after the push of the tests and of the counts, before
the commit of this report:

```text
1225e3a9 State the counts of the second coverage session in CLAUDE.md
13fcc304 Test the operators, the patterns, the simd checks and more statements
71c207fd Test more regions of the checker that no test reached
```

`git rev-parse HEAD origin/main` after that push:

```text
1225e3a9802c40945231df4a1879422808c70d5c
1225e3a9802c40945231df4a1879422808c70d5c
```

`git status --short` printed nothing then. The reply of the session holds
the three outputs after the last push, since the hash of this commit is not
known inside it.

## Left for the next session

By file and line of `build/coverage/html/` of the run of 2026-10-13. Every
other unreached region of the 28 files has its reason above.

- `sema.c` 828: a generic of a library with two parameters named without
  arguments. 1120: a module of the standard library that compiles itself.
  1164: a damaged library function with two parameters, a second function
  of `tests/modules/anti/regex.anti`. 1629: a variant of more than 65535
  cases. 2483: a union with two fields of one name.
- `sema_call.c` 372 and 652: an operator of a generic of a library, and a
  variadic `extern fn` of another module named without a call. Both need a
  tree under `tests/modules/`. 2320: `Object` named where `anti.lang` is
  not in reach.
- `sema_copies.c`, all 25 lines: 105, 196, 203, 206, 280, 400, 451, 600,
  627, 648, 671, 687, 700, 715, 725, 876, 935, 946, 1002, 1257, 1296, 1299,
  1419, 1442 and 1450. They read as guards of the copy maker after the
  check of the generic body, and none is proven.
- `sema_expr.c` 606, 1056, 1089 and 1116.
- `sema_generic.c` 403, 681, 685, 700, 788, 792, 1841 and 1892.
- `sema_operator.c` 222 and 681.
- `sema_pattern.c` 317, 461, 523, 560 and 610: a library file of
  `anti.regex` or `anti.text` that lacks a function. 730.
- `sema_stmt.c` 129, 152, 459, 826, 830, 875, 1136, 1145, 1191, 1289,
  1436, 1697, 1804, 1969, 2341 and 2838.
- `sema_thread.c` 39, 84, 88 and 414.
- `types.c` 167, 1585, 1602, 1656 and 1880, and `pattern.c` 36.
