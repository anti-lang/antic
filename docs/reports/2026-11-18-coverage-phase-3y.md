# Batch 3y of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `be340cdd` gave the ranking. The batch took
`src/antic/header.c` with 59 missed branches, which batch 3x named, and
`src/antic/ir_verify.c` with 47, which no batch had chosen and which has
no section in `docs/notes/unreached-guards.md`. It then took the sides of
`src/anti/licensing.c` with 35 and of `src/rt/toml.c` with 34 that a
damaged archive and the blanks of a text reach. Each side ran first
through the antic, the anti or the unit tests of the coverage tree alone,
with a profile of its own. The sections of the four files in the note
were read first. Eddie's direction is the Mac alone, so the VMs were not
touched. The batch found no defect. The next free id stays T298. The
batch before found one, so this is the first batch in a row without a
defect, and the phase goes on.

## Commits

- `fd7cc2d9` tests the four files: two groups of `unit_ir`, three of
  `unit_header`, the case `clib_wide`, two malformed archives of
  `license_from_archive`, the message case
  `anti_message_license_from_archive_shorter_than_its_start` and the
  program `std_toml_blanks`.
- `7628ab6e` names the guards. One commit records the times and the
  counts in `CLAUDE.md` with this report.

## Findings

No defect. Every side that a probe reached gave the message or the
output that the code and the specifications describe. The tests hold it
as it stands.

- The IR verifier is a check of what lowering writes, so the unit test
  builds each refused form by hand. The sides left are the operands that
  `indices_ok` refuses before `check_inst` reads them. The others stand
  in the analysis of the definitions, which runs only for a function that
  passed the checks before it.
- An aligned class that is not packed takes `ANTI_ALIGNED(32)` on its
  struct, and the C program of `clib_wide` gives the size and the
  alignment of Anti. A field of a function type is one C function
  pointer, which C sets and Anti calls.
- A union in a class body is refused, so the `union` arm of the nested
  views of the header is a guard.

## ASan sweep of damaged library files

The library files of `classes`, `kinds`, `values`, `errors` and
`threads` of `tests/modules/` went to the antic of the ASan tree beside
their client programs, damaged in two ways: cut at an offset, and with
the byte at an offset inverted. Each of the first 399 offsets of each
file took both, and every 41st offset after them, 41462 files in all, in
35 minutes. Every run ended with the status 0 or 1, and none showed a
report of AddressSanitizer.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 43000.
`cmake -DFIRST=43001 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 43001 to
43200 at the default size in 169 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.27%, 3794 missed | 93.37%, 3740 missed |
| Regions | 98.52%, 1264 missed | 98.52%, 1265 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.72%, 1227 missed | 98.72%, 1227 missed |

| File | Branches missed before | after |
|---|---|---|
| `ir_verify.c` | 47 | 8 |
| `header.c` | 59 | 48 |
| `licensing.c` | 35 | 32 |
| `toml.c` | 34 | 29 |

The region more is one of `src/rt/trace.c`. The branches of four files
of the runtime moved by one to three. The threads and the traces of the
suite take other paths from run to run. The first coverage
run had 7830 tests, all passed, and its suite took 997 s. The run after
the tests had 7833, all passed, and took 1020 s.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,7845` once: 377 s, 274 s and 332 s. `note_claims` failed on
  the rows of the guards, which named `header.c` without its directory,
  and `tests/clib/header.c` has the same name. It passed alone once the
  rows named `src/antic/header.c`, and the note went into a commit of its
  own.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7843. Every test passed.
- The host runs 7845 tests and each sanitizer tree 7843.
  `tests/known-failures.txt` holds the two lines of `host=windows`, as
  before.

## Not reached

`src/anti/deps.c` with 38 missed branches has sides that a chain of path
projects reaches, a project of a path whose manifest names a dependency
of a repository or an absolute path, and the lock file of a path whose
directory holds a double quote. They need projects of their own in
`tests/anti-build/` and got no test. `lower_desc.c`, `sema_value.c`,
`main.c` of `anti`, `lower.c`, `docpage.c`, `symbols.c` of the runtime,
`bindclang.c`, `bindexpr.c`, `build.c`, the `doc` files, `test.c`, the
files of `llvm_emit` and `antl_tree.c` got no probe. No sweep fed cut
`anti.toml` or `anti.lock` files to the ASan anti.

## Questions for Eddie

No new question. Seven stay open, and nothing here acts on them: `take`
of an `own ?fn` field, the handler of T278, the reach of T281 across
modules, the body of a `do ... while` in a `construct`, whether the two
forms of a function type compare, from T290, whether the verifier of a
library file holds every name of a local to a binding before it, from
batch 3u, and whether a body of a struct, an enum or a variant that the
end of the file cuts after a refused member gets one message, from batch
3x.
