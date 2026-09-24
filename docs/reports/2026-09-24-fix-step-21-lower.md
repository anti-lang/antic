# Fix step 21, the split of lower.c

The second half of fix step 21 of `docs/audit/summary.md`: `lower.c`
split along its parts, as the tool-pass report judges it. The findings
are minor and carry no IDs, so each is named by its rule and place. No
finding is severe or major, so no new test comes with the step. The
unchanged suite and the identity manifests are the check. Logs are under
`build/drive/logs/`, named `s21l-*`.

## Findings

- Rule 19, `src/antic/lower.c`, 8683 lines at the base. Fixed. Five
  files and a header:

  | File | Lines | Part |
  |---|---|---|
  | `lower.c` | 2220 | helpers, places, checks, hook sites, the items of a module, class functions |
  | `lower_desc.c` | 1524 | tables, descriptors, interface tables and thunks, `construct` runs |
  | `lower_expr.c` | 2397 | values built in place, expressions, casts, calls, workers, `sync` operations |
  | `lower_simd.c` | 560 | the lane loops and the operations of simd structs |
  | `lower_stmt.c` | 1993 | exits of a block, statements, loops, teardown, handlers, the slot reserve |
  | `lower_lowerer.h` | 419 | `struct lowerer`, the shared types and the 123 shared names |

  The tool-pass report names five parts. The helpers it does not name
  stay in `lower.c` with the module and tracing code, as the
  declarations stayed with the passes in `sema.c`. `hook_name` moved
  from the module code into `lower_desc.c`, since it takes the size of
  `root_names` there.
- Rule 18, `lower_stmt` and `lower_expr_value`. Left. The tool-pass
  report judges their size as the shape of the problem, a `switch` over
  the kinds, and this step moved them unchanged.
- Rule 18, the parameter lists of `check_call` and `check_branch`. Left.
  The tool-pass report judges them the shape of the problem.

## How the split was checked

A script (`build/s21l/split.py`, adapted from the one of the `sema.c`
step) cut the file at its top-level definitions along `build/s21l/plan.txt`
and listed every name one part uses from another. A shared function or
constant takes the prefix `lower_` under rule 25, and the rest stay
`static`. `build/s21l/verify.py` compares the tokens of all 278
functions with the base, ignoring the prefix, and finds no difference
(`s21l-verify.log`). The rename touched `` `none` `` in thirteen comments,
where it names the keyword, and those were set back. Lines the longer
names pushed past 80 columns are wrapped. No line is longer than it was
in the base apart from the 13 lines that were already over. The identity
manifests did not move, and `git status` lists no file after all three
suites.

## Gates

- Zero warnings in the host, ASan and UBSan builds
  (`s21l-host-build2.log`, `s21l-asan-build.log`, `s21l-ubsan-build.log`).
  The Mac linker prints `ignoring -lto_library` for the pinned clang, as
  before.
- Host: 841 of 841 passed, `s21l-ctest-host.log`.
- ASan: 840 of 840 passed, `s21l-ctest-asan.log`.
- UBSan: 840 of 840 passed, `s21l-ctest-ubsan.log`.
- Docs style: nothing on every touched file, `s21l-docs.log`.

## Decisions

Two `[provisional]` entries under "Files of lowering" in
`docs/decisions.md`: the five files with the header they share, and the
prefix `lower_` of the shared functions while the structs, enums and
macros keep their names. `docs/decisions.md`, `docs/notes/hooks.md`,
`docs/notes/versions.md`, `docs/notes/dev-checks.md` and
`src/rt/object.h` named a place in `lower.c` and now name its file.

## Proof

Before this report was committed:

```console
$ git log --oneline -3
2cbb34e Record the files of lowering
05c1685 Split lower.c along its parts (rule 19)
9956ede Report fix step 21 for sema.c
$ git status --short
$
```

`git rev-parse HEAD origin/main` after the push is in the reply of the
session, since the push follows this commit.
