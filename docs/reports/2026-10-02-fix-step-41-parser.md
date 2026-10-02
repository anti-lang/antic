# Fix step 41, the split of parser.c

The part of fix step 41 of `docs/audit/summary.md` for `src/antic/parser.c`:
the file split along its parts, so each file has one purpose. Behaviour is
unchanged. Logs are under `build/drive/logs/`, named `s41-*`.

## Findings

- Rule 19, `src/antic/parser.c`, 4340 lines at the audit and 4465 at the
  base. Fixed. The tool-pass report names its parts: types, expressions,
  statements, generics and members, and items. Seven files and a header:

  | File | Lines | Part |
  |---|---|---|
  | `parser.c` | 754 | shared helpers, `parser_parse` and the lines at module level |
  | `parser_type.c` | 432 | types and the scan of `<` |
  | `parser_expr.c` | 1021 | expressions |
  | `parser_stmt.c` | 834 | statements and blocks |
  | `parser_clause.c` | 186 | the clauses `allow` and `unchecked` |
  | `parser_decl.c` | 721 | signatures, type parameters, members, nested types, fields |
  | `parser_item.c` | 573 | classes, variants and the other items |
  | `parser_parser.h` | 163 | `struct parser`, the lists, the chains, 60 shared functions |

  The clauses were part of the statements in the audit's count. Items,
  fields, members and the whole file read them as well, so they are a
  file of their own.
- Rule 18, the long functions of the parser: `statement_level` (403
  lines), `primary` (368), `item_level` (283), `type_level` (174),
  `class_item` (161), `parser_parse` (158), `member_level` (130) and the
  nest of 6 in `postfix`. Left, moved unchanged. The tool-pass report
  keeps the first audit's judgement of `statement_level` and `primary`, a
  `switch` over the kinds that follows the shape of the problem, and
  judged the others from their counts alone. This session was given the
  file split.

## How the split was checked

`build/s41/chunks.py` and `build/s41/plan.py` cut the file at its
top-level definitions and list every name one part calls from another.
`build/s41/assemble.py` writes the files. A shared function takes the
prefix `parser_` under rule 25, and the rest stay `static`. Continuation
lines keep the paren or the token they were aligned to.
`build/s41/verify.py` compares the tokens of all 118 functions with the
base, ignoring the prefix, and finds no difference (`s41-verify.log`).
It also lists every comment of the base that no longer stands word for
word. Those are the five section headings, as `/* Types */`, which became
the comment at the top of each file, and five comments that name
`descend`, `ascend`, `type` or `close_clauses`, which now give the
prefixed name. The lines the prefix pushed past 80 columns are
wrapped, and a message that moved keeps its text.

The test `header_sections` now checks `parser_parser.h` as it checks the
headers of the checker and of lowering. It fails with a declaration moved
under the wrong file (`s41-header-neg.log`). The notes that named a
renamed or moved function name the new name or file. The identity
manifests did not move, and `git status` lists no file after all three
suites.

## Gates

- Zero warnings in the host, ASan and UBSan builds (`s41-host-build.log`,
  `s41-asan-build.log`, `s41-ubsan-build.log`).
- Host: 1459 of 1459 passed, `s41-ctest-host.log`.
- ASan: 1458 of 1458 passed, `s41-ctest-asan.log`.
- UBSan: 1458 of 1458 passed, `s41-ctest-ubsan.log`.
- Docs style: nothing on every touched `.md` file, `s41-docs.log`.

## Decisions

One `[provisional]` entry under "Files of the parser" in
`docs/decisions.md`. It names the seven files with the part each reads,
the shared header and the prefix `parser_` of the shared functions.

## Proof

Before this report was committed:

```console
$ git log --oneline -3
85b6118f Record the files of the parser
c05d3054 Split parser.c along its parts (rule 19)
48f95a43 Add the report of fix step 40 for the tests
$ git status --short
$ git rev-parse HEAD origin/main
85b6118fd92d6a8f1c2c526aa7c5bf138be24435
85b6118fd92d6a8f1c2c526aa7c5bf138be24435
```
