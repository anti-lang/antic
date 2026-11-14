# Batch 3g of the third phase of coverage

This step sorts the regions that `./c coverage` showed unreached in four
of the candidates that the report of batch 3f left:
`src/antic/parser_stmt.c`, `src/antic/types.c`, `src/rt/patterns.c` and
`src/anti/repo.c`. It follows the method of
`docs/reports/2026-11-12-coverage-phase-3d.md`. Each candidate first ran
through the antic or the anti of the host tree alone. Eddie's direction
is the Mac alone, so the VMs were not touched. The other three
candidates were sorted before: the guards of `src/rt/trace.c`,
`src/antic/sema_copies.c` and `src/antic/driver_link.c` stand in
`docs/notes/unreached-guards.md` already, and what else they miss runs on
another host. The batch found no defect, so no id was taken, and T257
stays the next free one.

## Commits

- `ddb3a155` tests `parser_stmt.c`.
- `b972bee2` tests `types.c`.
- `8177c4a8` tests `patterns.c`.
- `6102240b` tests `repo.c`.
- `7f2c37ba` names the guards in `docs/notes/unreached-guards.md`.
- `f730e78b` records the times and the counts in `CLAUDE.md`.
- This report.

## parser_stmt.c

Seventeen refusals of the matrix take a parse error of a statement that
no test gave. A `let` that takes a tuple apart lacks its `=`, or names
nothing in `let () = t;`. A `catch` that guards a value rather than a
call is no block, in both forms of `let`, and `alloc` of a struct
literal before a `catch` is refused by the checker. An `if let`, a
`while` and a `do` lack a block, a `while` its condition and a `do` its
`while`, and the block of a `try` does not parse. A `switch` lacks its
value, its `else` and an arm their arrow, and an arm its value, and an
arm of a `select` lacks its arrow. The expression parser reads the
`catch` after a call as part of the call, so the regions of a statement
that read one after a call are guards.

## types.c

The fix of T163 spells the wrapping and saturating operators of a length
that the target decides, and its tests named `<<%`, `+|` and `mul_high`.
`types_length_names_its_wrapping_and_saturating_operators` names `+%`,
`-%`, `*%`, `-|` and `*|` in one type. Every other missed line is a
guard: an operand that is no function type, a member that no level of
its chain declares, an empty item, and the `default` of the table of
operators.

## patterns.c

`matches_literal` of anti.regex is public, and its third argument is the
text of the pattern for the stop at the match limit.
`trap_pattern_limit_without_its_text` passes an empty text, and the
program stops with the line alone. The other regions are guards. A
probe showed that a slice of a `str` is a `[]byte`, which a pattern of
bytes searches, so no search of text meets UTF-8 that is not valid, and
`failure` stops on the limits alone.

## repo.c

The fake curl of the build cases serves `http://127.0.0.1` beside
`mirror.example`, so `anti_build_redirect_to_loopback` follows a
redirect to plain `http://` of the machine. It answers 410 where
`FAKE_ABSENT` says so, which `anti_build_index_gone` reads as an index
the server does not hold. `anti_build_deps` writes an empty stamp, one
that no 64 bits hold and one after the present time beside the cached
index, and each build fetches the index again. An offline build reads
the cached index whatever its stamp says. The messages of `anti build`
take an empty index, `index_empty`, and a lock file without a cache of
the user, `lock_cache_unknown`, which reaches the cache in
`repo_module`. `unit_deps` takes a port with a byte below `0`. The
checks of a name, a version, a module path and a digest in `index_dir`
and `repo_module` are guards, since `deps.c` makes each first.

## Coverage

| | Before | After |
|---|---|---|
| Regions | 98.30%, 1437 missed | 98.32%, 1424 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.52%, 1414 missed | 98.54%, 1396 missed |
| Branches | 91.67%, 4664 missed | 91.73%, 4630 missed |

| File | Lines before | Lines after | Branches before | Branches after |
|---|---|---|---|---|
| `parser_stmt.c` | 96.93%, 22 missed | 97.77%, 16 missed | 90.45% | 94.09% |
| `types.c` | 98.35%, 28 missed | 98.65%, 23 missed | 93.87% | 94.38% |
| `patterns.c` | 96.46%, 22 missed | 97.26%, 17 missed | 90.13% | 90.58% |
| `repo.c` | 93.82%, 22 missed | 94.38%, 20 missed | 87.66% | 93.51% |

The lines that stay unreached in the four files are guards of the note,
and the sides of their conditions that the batch did not chase stand
under "Not in this note" there. `rt/mem.c` and `rt/platform_posix.c`
lost one and two missed branches beside them. The coverage suite ran
7326 tests before and 7349 after, all passed. `./c coverage` took 993 s
before and 997 s after. `fmt_canonical` reads every matrix file, a
refusal included, so four new files took the line breaks of `anti fmt`
before their commit, which moved the positions of their messages.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,<last>`. The times stand in `CLAUDE.md`.
  Every test passed apart from the two skips of `mimalloc_environment`.
- Both sanitizer suites ran after the last code commit, in the six parts
  of `asan`, up to test 7359. Every test passed. `asan` took 273 s,
  186 s, 262 s, 173 s, 244 s and 524 s. `ubsan` took 297 s, 190 s,
  262 s, 178 s, 261 s and 550 s.

## Next candidates

The files with the most missed lines that no batch sorted yet are
`src/antic/sema_call.c`, 22 lines and 166 branches,
`src/antic/antl_read_ir.c`, `src/antic/sema_value.c`, `src/rt/registry.c`,
`src/antic/header.c`, `src/antic/parser_decl.c`, `src/antic/lexer.c` and
`src/antic/sema_uninit.c`, 19 to 20 lines each.

## Questions for Eddie

None.
