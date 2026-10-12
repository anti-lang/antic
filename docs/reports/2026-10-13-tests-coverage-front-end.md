# The regions of the front end that no test reached

A step of the test run, on 2026-10-12 and 2026-10-13, on `main`. It ran
`./c coverage`, read the regions that the suite did not reach in
`src/antic/lexer.c`, the seven parser files, `pattern.c`, `types.c` and the
eighteen `sema*.c` files, and wrote tests for them. No file under `src/`
changed. The step did not finish the checker: see "Regions this step did
not settle".

## What the step added

- `tests/matrix/` holds 304 new refusals and 8 new programs. Each refusal
  reaches one region and holds one message.
- `tests/errors/` holds 9 new programs: four of a `tests` or a `fixtures`
  block, which need `--tests`, the `if` at the depth limit of T103 and
  four of `--doc-warnings`.
- `tests/dump/` holds four new dumps: `dump_tokens_kinds` and
  `dump_types_symbolic_operators`, `_char_lanes` and `_lent_optional`.
- `tests/unit/test_lexer.c` gained the cases of the lexer that need bytes a
  checkout cannot hold, such as CR before LF.
- The programs: `flow_single_forms`, `values_pattern_escapes_and_classes`,
  `values_pattern_groups_and_repeats`, `values_constant_forms`,
  `types_generic_constraint_forms`, `values_hash_and_equal_forms`,
  `classes_construct_sets_through_statements` and, for T104,
  `classes_injected_logger_never_called`.

## The coverage of each file

Regions that no test reached, and the share of regions and of lines that
the suite reaches, before and after. `build/drive/logs/cv-before-summary.txt`
and `cv-after-summary.txt` hold the whole rows.

| File | Regions | Missed before | Missed after | Regions before | Regions after | Lines before | Lines after |
|---|---|---|---|---|---|---|---|
| `lexer.c` | 922 | 36 | 18 | 96.10% | 98.05% | 94.92% | 98.24% |
| `parser.c` | 424 | 26 | 4 | 93.87% | 99.06% | 95.13% | 98.56% |
| `parser_clause.c` | 138 | 14 | 0 | 89.86% | 100.00% | 92.59% | 100.00% |
| `parser_decl.c` | 541 | 52 | 3 | 90.39% | 99.45% | 88.28% | 99.29% |
| `parser_expr.c` | 752 | 78 | 2 | 89.63% | 99.73% | 91.39% | 99.76% |
| `parser_item.c` | 560 | 40 | 0 | 92.86% | 100.00% | 91.49% | 100.00% |
| `parser_stmt.c` | 699 | 86 | 11 | 87.70% | 98.43% | 88.66% | 98.82% |
| `parser_type.c` | 331 | 31 | 2 | 90.63% | 99.40% | 90.11% | 99.44% |
| `pattern.c` | 818 | 319 | 17 | 61.00% | 97.92% | 68.02% | 98.19% |
| `sema.c` | 1930 | 40 | 26 | 97.93% | 98.65% | 97.40% | 98.30% |
| `sema_call.c` | 2204 | 49 | 39 | 97.78% | 98.23% | 97.61% | 97.98% |
| `sema_chain.c` | 44 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `sema_class.c` | 954 | 23 | 19 | 97.59% | 98.01% | 98.57% | 99.05% |
| `sema_const.c` | 903 | 68 | 42 | 92.47% | 95.35% | 94.53% | 96.21% |
| `sema_copies.c` | 1002 | 46 | 46 | 95.41% | 95.41% | 94.86% | 94.86% |
| `sema_doc.c` | 472 | 36 | 5 | 92.37% | 98.94% | 92.48% | 99.00% |
| `sema_export.c` | 426 | 4 | 4 | 99.06% | 99.06% | 99.50% | 99.50% |
| `sema_expr.c` | 2056 | 59 | 53 | 97.13% | 97.42% | 96.75% | 97.36% |
| `sema_generic.c` | 1815 | 119 | 88 | 93.44% | 95.15% | 94.28% | 95.80% |
| `sema_hash.c` | 472 | 21 | 12 | 95.55% | 97.46% | 93.91% | 96.84% |
| `sema_operator.c` | 686 | 32 | 30 | 95.34% | 95.63% | 95.84% | 96.61% |
| `sema_pattern.c` | 674 | 55 | 33 | 91.84% | 95.10% | 93.68% | 96.37% |
| `sema_safety.c` | 542 | 29 | 26 | 94.65% | 95.20% | 96.17% | 96.17% |
| `sema_simd.c` | 407 | 48 | 42 | 88.21% | 89.68% | 88.81% | 90.71% |
| `sema_stmt.c` | 2307 | 130 | 76 | 94.36% | 96.71% | 94.38% | 96.29% |
| `sema_thread.c` | 465 | 30 | 23 | 93.55% | 95.05% | 91.78% | 94.67% |
| `sema_value.c` | 586 | 19 | 19 | 96.76% | 96.76% | 96.80% | 96.80% |
| `types.c` | 1313 | 67 | 24 | 94.90% | 98.17% | 95.72% | 98.57% |
| All 28 | 24443 | 1557 | 664 | 93.63% | 97.28% | 93.86% | 97.41% |

## The findings

`docs/audit/tests-findings.md` holds the rows.

- T100, minor: a refused statement with `{` and `}` after the mistake gets
  `expected an item` as well. Seventeen tests show it.
- T101, minor: after a refused member the parser reads the rest of the
  class body as items of the module.
- T102, minor: `pub singleton final trace class` in a class body gets
  `expected identifier`.
- T103, major: antic never ends on an `if` whose statement stands at level
  256, and takes about two gigabytes of memory per second. Its test stops
  antic after two seconds.
- T104, major: a program with `inject logger: *log.Logger` that calls no
  function of the logger fails at the link in release mode.
- T105, major: a class whose tuple field holds a union gets a default `==`.

One more test names T096, a class whose base is refused.

## Regions no input reaches

- `lexer.c`: the names of the literal kinds, of the end of the file and of
  a refused token in `lexer_token_kind_name`, since the parser asks for the
  name of a symbol or of an identifier alone. A spelling too long for the
  cache: the longest has 12 bytes. The groups `eof` and `error`: the
  listing stops before the last token and prints no refused source.
- `parser.c` 686 to 723: a block, a `provides` line and a `link` line read
  their first token before they can fail.
- `parser_decl.c` 475: a line is a `compatible` line only where a number
  follows.
- `parser_expr.c` 978: the operand before `??` is read one level deeper
  than the `??`, so the depth limit refuses the operand first.
- `parser_stmt.c` 313, 341, 422, 456, 500 and 503: every caller sees
  `catch`, an identifier or two names before it asks, and the text of a
  condition is never empty. 294 is T103, whose test stops antic before it
  writes a profile.
- `parser_type.c` 50: a scan with half of a `>>` left stands on that `>>`.
- `pattern.c` 51 and 68: the checker never asks for the groups of a
  pattern that does not compile.
- `pattern.c` 378, 409, 497, 506, 514, 556 and 785: the reader runs on a
  pattern that compiled. A class, an escape or a quote that the end of the
  pattern cuts off does not compile.
- `pattern.c` 1107 and 1127: PCRE2 refuses a pattern whose counts multiply
  past 2^30 as too large. 1002, 1136 and 1149 stand after a switch whose
  every case returns.
- `types.c` 41, 477 and 597 are guards no caller meets. 447 and 462 follow
  the refusal of a mark on a parameter that is no function. 1478 to 1491
  read the variant of a damaged library file.
- `sema_const.c` 218 and 229: a message writes a float only where it fits
  no integer, so its exponent is never below zero. 703 and 810: a constant
  and a default take no length computed from `size_of`.

## Regions this step did not settle

664 regions of the 28 files stay unreached, 583 of them in the checker.
The step read the lists of `sema.c`, `sema_call.c`, `sema_const.c`,
`sema_expr.c`, `sema_generic.c`, `sema_stmt.c`, `sema_pattern.c`,
`sema_simd.c`, `sema_operator.c`, `sema_thread.c`, `sema_class.c`,
`sema_hash.c`, `sema_doc.c` and `sema_safety.c` and wrote the tests it
could derive in one reading. It did not settle the rest, and it did not
read `sema_copies.c`, `sema_value.c` and `sema_export.c` region by region.
Most of what stays is a test of `sema_is_error` after a refusal, a guard
against `NULL`, or the making of a generic copy. None of these has a
reason in this report, so the next step starts from
`build/coverage/html/` of a fresh `./c coverage`. Of the other ten files,
`pattern.c` 36 and `types.c` 167, 1337, 1585, 1602, 1656 and 1880 are open
as well.

## Rules the documents do not state

- A source that ends inside a block gets `` expected `}` `` twice at the
  same position, for the block and for the function.
- The check of exponential time refuses `(a+\b)+` and `(a+(?=b))+`: a part
  that takes no text counts as nothing between the two repeats.
- A generic call whose argument has another shape than the parameter, an
  `int` for `[]T` or a tuple of three for `(T, T)`, gets the message that
  nothing in the arguments gives `T`.
- `operator fn eq` of a class that gives an `int` is taken where it is
  declared, and so is `operator fn eq()` without parameters at module
  level.
- `delete(p)` and `destroy(p)` of a parameter `p: *T` without `own` are
  taken.
- `operator fn value(self) -> lent ?*int` is taken, and a simd struct takes
  `char` lanes.
- The `else` of a `let` that ends with a `switch` whose every arm returns
  is refused, and the same `else` with `if let` and `else` is taken.
- `-(-size_of(H))` in a length prints as `--size_of(H)`.

## Where the task and the code differ

- `./c coverage` ran as `./c coverage part 1/2` and `part 2/2`, since one
  call ends after ten minutes.
- The task puts a test in the directory of its kind. The matrix keeps no
  `tests` block, so those four refusals stand in `tests/errors/`. T103
  stands there too, since a matrix test lets antic run for sixty seconds.
- A source that does not lex has no canonical form, so the refusals of the
  lexer are cases of `unit_lexer` and no programs.

## Counts and checks

- Tests on the host: 3794 before, 4135 after. Expected failures: 325
  before, 350 after.
- Host suite: 4135 of 4135 passed, 350 of them expected failures, two
  skipped, in 441 s, in `build/drive/logs/cv-suite-host.log`.
- ASan and UBSan: 4126 of 4126 each, in three parts,
  `cv-suite-asan-1.log` to `-3.log` and `cv-suite-ubsan-1.log` to `-3.log`.
  The eight matrix tests of the last test commit came after them. They ran
  in the host suite and, alone, under both sanitizers.
- Coverage: `cv-before-1.log` and `-2.log`, `cv-after-1.log` and `-2.log`.
- Every tree built with zero warnings. `emit_identity` and
  `link_identity_macos-arm64` pass, and no expected file of either changed.
- The docs-style checker reports nothing on `CLAUDE.md`,
  `docs/audit/tests-findings.md` and this report.

## Proof

`git log --oneline -3` after the push of the last test commit and of the
counts, before the commit of this report:

```text
a4eea0ba State the final counts of the coverage step in CLAUDE.md
38ce1129 Test the fields a construct sets through each statement
1b48c65a State the counts and the suite times of the coverage step in CLAUDE.md
```

`git rev-parse HEAD origin/main` after that push:

```text
a4eea0ba7253f560e55d8f78fccbf8a18740e621
a4eea0ba7253f560e55d8f78fccbf8a18740e621
```

`git status --short` printed nothing then. The reply of the session holds
the three outputs after the last push, since the hash of this commit is not
known inside it.
