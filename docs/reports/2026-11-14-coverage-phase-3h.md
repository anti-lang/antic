# Batch 3h of the third phase of coverage

This step sorts the regions that `./c coverage` showed unreached in the
eight candidates that the report of batch 3g left:
`src/antic/sema_call.c`, `src/antic/antl_read_ir.c`,
`src/antic/sema_value.c`, `src/rt/registry.c`, `src/antic/header.c`,
`src/antic/parser_decl.c`, `src/antic/lexer.c` and
`src/antic/sema_uninit.c`. It follows the method of
`docs/reports/2026-11-12-coverage-phase-3d.md`. Each region first ran
through the antic of the coverage tree alone, with a profile of its own,
so a probe showed the count of each line it reached. Eddie's direction
is the Mac alone, so the VMs were not touched. The batch found two
defects, T257 and T258, and fixed both. T259 is the next free id.

## Commits

- `eb098bcf` tests `sema_call.c`.
- `b30ad715` tests `sema_value.c` and lists T257, and `feadc2d3` fixes it.
- `170d1464` tests `sema_uninit.c`.
- `1deb3a10` tests `parser_decl.c`.
- `ca8d929c` tests `lexer.c`.
- `1b4cb933` tests `header.c`.
- `95105307` names the guards in `docs/notes/unreached-guards.md`.
- `19ae46bb` records the times and the counts in `CLAUDE.md`.
- `2e6e4e3d` lists T258, and `cf33a4be` fixes it. `c49806e8` writes the
  commit of the fix into its row, and `4d88f25d` records the times.
  `c49806e8` has a subject line and no body. It is pushed, so it stays.
- This report.

## sema_call.c

A class nested in a generic class reads a private field of a copy. A
function of a class takes a literal as its receiver, and `carry` is read
through the element of a tuple. `.len` of an integer, `.ptr` of an array
and `construct` through a field are refused. A probe showed that the
client of a library checks no level in its copies of the generics of the
library. The arm of `from_module_operator` for such a copy is therefore
a guard. The `return` after the `switch` of `check_handled` belongs to
the measurement.

## sema_value.c and T257

`take(x.f)` with a handler and `take` of an unknown name are refused. `=`
that reads an `own` field of an object that a call gives into another
`own` field is refused as well. Its message put the words "the value"
between backticks, which hold code. T257 holds it, and the fix names
such a read in words.

## sema_uninit.c

`p[1] += 2` reads the element before it writes it, and the check refuses
it. A local of a buffer that `=` gives another pointer is followed no
longer. The walk of a block stops at the statement that ends its path,
so the checks of a path that has ended are guards.

## parser_decl.c, lexer.c and T258

Ten refusals take the parse errors of a static field, a constant and the
parameters of a function of a class body. A program names four
parameters `own`, `lent`, `keep` and `concurrent`, which are contextual
words. Seven refusals take the escapes `\x` and `\u{}`, a byte outside
ASCII in `x"..."` and three character literals. A program takes `\u{0}`
in a byte string. The formatter cannot write a source the lexer refuses,
so `fmt_canonical` leaves these refusals out.

A character literal whose escape lacks a digit, `'\x8'`, got the message
of the escape and then `a character literal holds one character`. A text
with the same escape gets the first message alone. T258 holds it, and
the fix gives a literal whose escape or NUL byte got its message no
second one. T096 and T232 give one message per mistake as well.

## header.c

The library of the forms exports a simd struct of `u8` lanes and one of
`f16` lanes. It also exports a text with a quote, a backslash and a
letter outside ASCII. The other regions are guards.

## registry.c and antl_read_ir.c

Every region that the suite misses is a guard, apart from one. The fixes
of T242 to T251 added checks of a later library file. Those checks meet
a class or a generic that the later file lacks only after the read of
the declarations has named it. A probe with a generic class that the
later file lacks got `needs struct` from that read. The damaged mark of
a generic copy in `read_generic_copies` needs a damaged file, which no
test writes yet.

## Coverage

| | Before | After |
|---|---|---|
| Regions | 98.32%, 1425 missed | 98.32%, 1419 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.54%, 1396 missed | 98.56%, 1379 missed |
| Branches | 91.72%, 4632 missed | 91.80%, 4592 missed |

| File | Lines before | Lines after | Branches before | Branches after |
|---|---|---|---|---|
| `sema_call.c` | 98.98%, 22 missed | 99.07%, 20 missed | 89.35% | 89.73% |
| `sema_value.c` | 96.86%, 20 missed | 98.28%, 11 missed | 88.30% | 88.87% |
| `sema_uninit.c` | 98.01%, 19 missed | 98.64%, 13 missed | 92.83% | 93.44% |
| `parser_decl.c` | 96.76%, 19 missed | 96.76%, 19 missed | 85.82% | 89.18% |
| `lexer.c` | 98.27%, 19 missed | 98.27%, 19 missed | 94.45% | 95.46% |
| `header.c` | 98.62%, 19 missed | 98.62%, 19 missed | 92.26% | 92.75% |

`antl_read_ir.c` and `registry.c` stay at 20 missed lines each. The
lines that stay unreached in the eight files are guards of the note. The
sides of their conditions that the batch did not sort stand under "Not
in this note" there: the quiet check that `sema_move_local` leaves, the
damaged mark of a generic copy, and the cap of 16 interfaces in
`ambiguous_member`. The coverage suite ran 7349 tests before and 7392
after, all passed. `./c coverage` took 1006 s before and 976 s after.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,<last>`. The times stand in `CLAUDE.md`.
  Every test passed apart from the two skips of `mimalloc_environment`.
- Both sanitizer suites ran before each of the two pushes, in the six
  parts of `asan`, up to tests 7401 and 7402. Every test passed. The
  times stand in `CLAUDE.md`.
- The host runs 7404 tests and each sanitizer tree 7402.
  `tests/known-failures.txt` holds the two lines of `host=windows` alone.

## Next candidates

The files with the most missed lines that no batch sorted yet are
`src/antic/pattern.c`, `src/antic/optimize.c` and
`src/antic/sema_free.c`, 18 lines each, `src/antic/sema_hash.c`, 17,
`src/rt/atomic.c`, 16, `src/antic/llvm_emit.c`, 15, `src/anti/bindwrite.c`,
14, and `src/antic/sema_safety.c`, 13 lines and 93 branches.

## Questions for Eddie

None.
