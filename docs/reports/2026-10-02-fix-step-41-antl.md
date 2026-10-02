# Fix step 41, the split of antl.c

The part of fix step 41 of `docs/audit/summary.md` for `src/antic/antl.c`:
the file split along its parts, so each file has one purpose. Behaviour is
unchanged. Logs are under `build/drive/logs/`, named `s41a-*`.

## Findings

- Rule 19, `src/antic/antl.c`, 3788 lines at the audit and 3454 at the
  base. Fixed. The tool-pass report names its parts: the writer and the
  reader. The reader alone held 2300 lines, so the IR has a file of its
  own. Four files where `antl.c` stood, and the shared header:

  | File | Lines | Part |
  |---|---|---|
  | `antl.c` | 180 | the checks of the format, the magic, shared helpers, `antl_header` and `antl_read` |
  | `antl_write.c` | 1045 | the writer, with `antl_write` and `antl_write_header` |
  | `antl_read.c` | 1112 | the type table, the constants and the items |
  | `antl_read_ir.c` | 1117 | the IR, and the copies the program has already |
  | `antl_io.h` | 233 | `struct writer`, `struct reader`, the forms, the shared functions by file |

  `get_cstr`, a primitive of the format, moved to `antl_io.c` as
  `antl_get_cstr`. The DESIGN comment on the nesting limit covers both
  readers and moved to `antl_io.h`, with the forms of the type table.
- Q4, the 29 wrappers of `antl.c:3631`. Not part of this session: they
  were gone at the base. `antl_io.c` holds the primitives, which every
  file of the library file calls directly.
- Rule 18, the long functions of `antl.c`: `read_types` (490 lines,
  nest 6), `put_type` (203), `put_ir` (147), `read_ir` (130),
  `visit_type` (105), `read_tables` (105), `read_signature` (102) and
  the 9 parameters of `antl_read`. Left, moved unchanged, as the parser
  session left its own. This session was given the file split. The first
  audit judged `put_type`, `put_ir` and `read_ir` to follow the format.
  It judged that `read_types` would read better with a function per kind
  and `antl_read` with a struct of options, and both stay open.

## How the split was checked

`build/s41a/plan.py` cuts the file at its top-level definitions and lists
every name one part uses from another, and `build/s41a/assemble.py`
writes the files. Six names crossed a file and took the prefix `antl_`
under rule 25: `get_cstr`, `name_equals`, `read_types`, `read_items`,
`read_ir` and `magic`. The forms became `ANTL_FORM_*`, and the rest stay
`static`. `build/s41a/verify.py` compares the tokens of all 86 functions
with the base, with the old names mapped back, and finds no difference
(`s41a-verify.log`). The only token difference of the whole file is the
`static` of the six shared names, the forward declaration of
`name_equals` and the enum of the forms that moved to the header. The one
comment of the base that no longer stands is the section heading
`/* The IR */`, which became the comment at the top of `antl_read_ir.c`.

The test `header_sections` now checks `antl_io.h` as it checks the
headers of the parser, the checker and lowering. It fails with
`antl_read_ir` moved under `antl_write.c` (`s41a-header-neg.log`). No
note named a moved function by file, and `git status` lists no file after
all three suites.

## Gates

- Zero warnings in the host, ASan and UBSan builds (`s41a-host-build.log`,
  `s41a-asan-build.log`, `s41a-ubsan-build.log`).
- Host: 1459 of 1459 passed, `s41a-ctest-host.log`.
- ASan: 1458 of 1458 passed, `s41a-ctest-asan.log`.
- UBSan: 1458 of 1458 passed, `s41a-ctest-ubsan.log`.
- Docs style: nothing on every touched `.md` file, `s41a-docs.log`.

## Decisions

One `[provisional]` entry under "Files of the library file" in
`docs/decisions.md`. It names the seven files with the part each holds,
the shared header and the prefix of the names the files share.

## Proof

Before this report was committed:

```console
$ git log --oneline -3
8e4d5d45 Record the files of the library file
20623c5e Split antl.c along its parts (rule 19)
fb183bba Add the report of fix step 41 for parser.c
$ git status --short
$ git rev-parse HEAD origin/main
8e4d5d45db9eee5d0bd2cc2758cc5af85e16bb57
8e4d5d45db9eee5d0bd2cc2758cc5af85e16bb57
```
