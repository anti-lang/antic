# Step 34, rules 18 and 19, second session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The first session,
`docs/reports/2026-10-19-solid-step34.md`, split `sema_stmt.c`. This
session took the first item of its list, `llvm_emit.c`. The logs stand
in `build/drive/logs/` under the prefix `s34b-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 19, `llvm_emit.c` past 3000 lines (tool-pass, back-end) | `8b814dea` | One file held the text of a function, the C convention, the facts for opt and the module, 4192 lines. |

The defect was confirmed by its count: 4192 lines at `e75d7020`, past
the 4102 of the audit. The file split along the four seams of
`docs/audit/back-end.md`:

| File | Lines | Holds |
|---|---|---|
| `llvm_emit.c` | 944 | The module, its names and globals, the COFF imports, the declarations and the metadata. The roots, the constructors, the four small texts and `llvm_emit_module` |
| `llvm_emit_function.c` | 1924 | The operands, the operations, each instruction and the definition |
| `llvm_emit_call.c` | 833 | The classification, parameters, prologue, calls, atomics, signature and linkage |
| `llvm_emit_facts.c` | 457 | The ranges and the `!tbaa` tree. The table facts, the cold branches and the frames a trace keeps |
| `llvm_emit_parts.h` | 159 | `struct emitter`, the structs it names and the 33 shared functions under their files |

The move changes no line of code. A script moved each top-level item
whole. It renamed the 33 functions that more than one file calls to the
prefix `llvm_emit_`. It realigned the continuation lines of a call
whose parenthesis moved. The three forward declarations went into the
header. The renames undone, the old file and the new files differ in
the header comments, the includes and the declarations of the header
alone. 18 lines the renames pushed past 80
columns were rewrapped by hand. The original included `stdarg.h` and
`stdio.h` and used neither, so no new file includes them.

The test of the finding is the suite, as in the first session.
`header_sections` now checks `llvm_emit_parts.h` against the family
`llvm_emit`, and `file_headers` counts it among the shared headers.
`file_headers` failed until it did. The developers' guide, `docs/notes/`
and a DESIGN comment of `lower_expr.c` that named a moved function now
name its file.

`48876f86` records the commit in the row of rule 19 of the summary.

## Findings left

- Rule 19, `sema.c`, 3134 lines. The front-end report names its
  declaration passes as `sema_declare.c`.
- Rule 18, the functions the reports judged clearer split:
  `docs/audit/front-end.md` under "Functions past a threshold",
  `docs/audit/back-end.md` under "The functions past a threshold",
  `docs/audit/middle.md` for the four parameter lists of
  `lower_owning.c`, and `docs/audit/library-file.md` for
  `read_signature`, `antl_read_ir`, `read_declaration` and
  `read_extern`.

## New defects and entries

No new defect. One `[provisional]` entry, under the new heading "Files
of the LLVM text" of `docs/decisions.md`, records the four files, the
header and the prefix `llvm_emit_`.

`tests/known-failures.txt` holds 29 lines before and after, ten of them
tests. No new feature, option or command. `emit_identity` and every
`link_identity_<target>` pass unchanged.

## Gates

- The three builds report no warnings: `s34b-build5.log`,
  `s34b-asan-build.log` and `s34b-ubsan-build.log`.
- Host, 5153 tests, `-I 1,2000` and `-I 2001,5153` (280 s). All 2000
  and 3179 pass, a part counting the fixtures it pulls in. The 8
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34b-host1.log`, `s34b-host2.log`.
- ASan, 5151 tests, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5151`,
  246 s, 163 s and 544 s. All 1150, 1119 and 2909 pass:
  `s34b-asan1.log` to `s34b-asan3.log`.
- UBSan, the same parts, 247 s, 141 s and 371 s. All 1150, 1119 and
  2909 pass: `s34b-ubsan1.log` to `s34b-ubsan3.log`.
- The docs-style checker reports no error on each `.md` file the
  session touched: `s34b-docs1.log` and `s34b-docs2.log`. Its one
  warning stands on line 510 of `docs/notes/source-tree.md`, the line
  the first session reported at 506, which four new rows moved.

The first part of the host suite ran in the background. The command
was started without a time limit, and the tool moved it there after two
minutes. Every later run was in the foreground.

## Not run on the target

Nothing. The move is in the back end, which writes the same text on
every host, and `emit_identity` compares it with the pinned output.

## State at the push

```text
$ git log --oneline -3
48876f86 Record the split of llvm_emit.c of step 34 in the summary
8b814dea Split llvm_emit.c along the four seams of the back-end report
e75d7020 Report the first session of step 34
$ git status --short
$ git rev-parse HEAD origin/main
48876f86140e08e68d5487789f2a93a5562ce4ef
48876f86140e08e68d5487789f2a93a5562ce4ef
```

## Left for the next session

1. `sema.c`, its declaration passes into `sema_declare.c`.
2. The functions of rule 18 that the reports judged clearer split, one
   file per session, from the lists under "Findings left".
