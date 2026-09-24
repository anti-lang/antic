# Fix step 21, the split of sema.c

The first half of fix step 21 of `docs/audit/summary.md`: `sema.c` split
along its parts, with `sema_check` split into its passes, as the
tool-pass report judges both. `lower.c` is the next session. The minor
findings carry no IDs, so each is named by its rule and place. Logs are
under `build/drive/logs/`, named `s21-*`.

## Findings

- Rule 18, `sema_check` at `src/antic/sema.c:9778`, 838 lines and 9
  parameters. Fixed. `sema_check` is 45 lines that set up the checker
  and call twelve passes in order, each a static function taking the
  checker. The long passes split further: the fields of a type into
  their resolution and the checks of `own`, `transient` and `inject`,
  and the checks of a type with fields into its contracts, its
  redeclarations, its `construct`, its qualifiers and the functions it
  must fill. The 9 parameters stay, since they are the interface that
  `driver.c` calls.
- Rule 19, `src/antic/sema.c`, 12141 lines at the base. Fixed. Six files
  and a header:

  | File | Lines | Part |
  |---|---|---|
  | `sema.c` | 2888 | helpers, scopes, types, the declarations and the passes |
  | `sema_expr.c` | 2217 | places, literals, operators, casts, interpolation, `check_expr` |
  | `sema_call.c` | 2804 | members, visibility, calls, errors, atomics, simd calls, plugins, workers |
  | `sema_const.c` | 1146 | the constant evaluator and the order of the constants |
  | `sema_stmt.c` | 2309 | statements, narrowing, `sync`, the set analysis, bodies, the worker walk |
  | `sema_export.c` | 912 | what crosses to C, the doc warnings, `sema_interface` |
  | `sema_checker.h` | 253 | `struct checker` and the 90 shared names |

  The tool-pass report names expressions and calls as one part. Together
  they are about 5000 lines, so they are two files.
- Rule 18, `check_stmt`, `eval_const` and `check_expr_inner`. Left. The
  tool-pass report judges their size as the shape of the problem, a
  `switch` over the kinds, and this step moved them unchanged.

## How the split was checked

A script (`build/drive/s21-split.py`) cut the file at its top-level
definitions and listed every name one part uses from another. A shared
function or constant takes the prefix `sema_` under rule 25, and the rest
stay `static`. `build/drive/s21-verify.py` compares the tokens of all 339
functions with the base, ignoring the prefix, and finds no difference.
Lines the longer names pushed past 80 columns are wrapped, and a message
that moved to a new line keeps its text. The identity manifests did not
move, and `git status` lists no file after all three suites.

## Gates

- Zero warnings in the host, ASan and UBSan builds
  (`s21-host-build.log`, `s21-asan-build.log`, `s21-ubsan-build.log`).
  The Mac linker prints `ignoring -lto_library` for the pinned clang, as
  before.
- Host: 841 of 841 passed, `s21-ctest-host.log`.
- ASan: 840 of 840 passed, `s21-ctest-asan.log`.
- UBSan: 840 of 840 passed, `s21-ctest-ubsan.log`.
- Docs style: nothing on every touched file, `s21-docs-final.log`.

## Decisions

Two `[provisional]` entries under "Files of the checker" in
`docs/decisions.md`: the six files with the header they share, and the
prefix `sema_` of the shared functions while the structs and macros keep
their names. `docs/decisions.md`, `docs/decisions-mback.md`,
`docs/work-order-completion.md`, `docs/notes/check.md` and
`docs/notes/plugins.md` named a function of `sema.c` and now name its
file. The header takes `ATTRIBUTE_PRINTF` of `attributes.h`, so `sema.c`
no longer writes the attribute out.

## Proof

Before this report was committed:

```console
$ git log --oneline -3
5be89b8 Record the files of the checker
f9289c9 Split sema.c along its parts (rule 19)
4fd3213 Split sema_check into its passes (rule 18)
$ git status --short
$ git rev-parse HEAD origin/main
5be89b871a783be5bfda24d51f591443462f8c0f
3bf42212aee21ec9208c9606b4e02e34713ae712
```

`origin/main` stood at the base until the push that carries this report.
