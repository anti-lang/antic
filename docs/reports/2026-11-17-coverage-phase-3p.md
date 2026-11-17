# Batch 3p of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `dc7d710e` gave the ranking. The batch took the
three files of the checker that batch 3o named, `src/antic/sema_expr.c`
and `src/antic/sema_call.c` with 160 missed branches each and
`src/antic/sema_generic.c` with 153, and the eight sides of
`src/anti/doc.c` that 3o left. Each side ran first through the antic or
the anti of the coverage tree alone, with a profile of its own, as 3o did.
The sections of batches 3a and 3h in `docs/notes/unreached-guards.md`
were read first. Eddie's direction is the Mac alone, so the VMs were not
touched. The probes found ten defects, T276 to T285. The next free id is
T286. The phase goes on, since this batch found defects.

## Commits

- `b1155df6`, `6a007049`, `217a55bb` and `7283a6a9` test the sides of
  `sema_expr.c`, `sema_call.c`, `sema_generic.c` and `doc.c`.
- `641df503` lists T276 to T284, and `b29767dc` lists T285.
- `f863d2ad`, `9b3acc87`, `cc1d7e24`, `f50d8444`, `264cfc2b`,
  `5b22319d`, `9969644a`, `6eef165e`, `131b7c78` and `c5dba186` fix
  T276, T277, T285, T278, T279, T280, T281, T282, T283 and T284. T285
  was fixed before T278, whose fix showed it.
- `ec17c9ab` records the rules of the fixes and their commits, and
  `1ca7f03c` names the guards. One commit records the times and the
  counts in `CLAUDE.md` with this report.

## Findings

- T276, minor. `q.to_slice()` of a `?*T` of an iterator said the type has
  no function `to_slice`. It now gets the message of the nullable rule.
- T277, severe. `none` of a `?*T` of a class converted to a `?*I` of an
  interface became the offset of the sub-object, and the program ended
  with a segmentation fault. `none` now stays `none`.
- T278, major. `let b: ?*Base = make(1) catch e { yield none; }` of a
  call that gives `*Kid` was refused, and so was `catch none`. The handler
  now yields against the `?*Kid`, which converts to the `let`. A
  `[provisional]` entry records it.
- T279, severe. A static function with a parameter named on a value was
  bound with the object as that parameter. It now gets the message of
  T077.
- T280, minor. A private function named on a value said the class has no
  field of its name. A `[provisional]` entry records the new message.
- T281, major. A second `export type` of one copy was taken and left out
  of the header. It is refused, and a `[provisional]` entry records it.
- T282, minor. A constant argument of another type said it must be a
  constant, and one whose value was refused got a second message. A
  `[provisional]` entry records the new message.
- T283, minor. `none` as an argument of a generic function was checked
  with no type, so `pick(none, &x)` was refused. It gives nothing to the
  inference now, as in the field of a literal.
- T284, minor. `anti doc notes.txt` and `anti check notes.txt` read the
  file as a source. Both refuse it now, as antic does, and a
  `[provisional]` entry records the messages.
- T285, severe. `let b: *Base = alloc Kid(2);` took the memory of a
  `Base`, and the `construct` wrote past it. A `let` of an interface held
  the object and not its sub-object.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 41200.
`cmake -DFIRST=41201 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 41201 to
41400 at the default size in 153 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 92.41%, 4262 missed | 92.56%, 4181 missed |
| Regions | 98.50%, 1271 missed | 98.50%, 1271 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.71%, 1232 missed | 98.72%, 1227 missed |

| File | Branches missed before | after | Lines missed before | after |
|---|---|---|---|---|
| `sema_expr.c` | 160 | 98 | 28 | 28 |
| `sema_call.c` | 160 | 152 | 20 | 17 |
| `sema_generic.c` | 153 | 148 | 21 | 21 |
| `doc.c` | 17 | 10 | 5 | 5 |

The ten sides `doc.c` keeps are guards of the note. Most of the sides
left in the three files of the checker take a NULL operand or an error
type after a message. The note gives their count. The coverage suite
ran 7552 tests, all passed, and took 991 s.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,<last>`, up to test 7564 at the end. The
  times stand in `CLAUDE.md`. One run covered the four test commits and
  the list of T276 to T284. The program of T278 fails the front end.
  `emit_identity`, the six `llvm_verify` tests and the program under
  Rosetta fail through it, so they named T278 until its fix.
- The first run before the fix of T285 failed `header_sections`, since
  `lower_to_interface` was declared under `lower.c`. The declaration
  moved under `lower_expr.c`, and the suite ran again.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7562. Every test passed.
- The host runs 7564 tests and each sanitizer tree 7562.
  `tests/known-failures.txt` holds the two lines of `host=windows`, as
  before.

## Next candidates

By branches, `sema_call.c` misses 152, `sema_generic.c` 148 and
`sema_expr.c` 98, most of them sides of an error type that this batch
did not chase. Of the files no batch chose by branches, `sema_class.c`
misses 97, `antl_read_ir.c` 96, `src/anti/fmt.c` 90, `sema_safety.c` 88,
`lower_expr.c` 84, `sema_const.c` 81 and `sema_copies.c` 80.

## Questions for Eddie

1. T278. The handler of a call whose `*Kid` a `let` of a `?*Base` takes
   yields against `?*Kid`, so `yield none` works and a `yield` of a
   `?*Base` that holds another class below `Base` is refused. The slot
   then holds one class on both paths, which an interface needs. Should
   a handler yield any `?*Base` there instead?
2. T281 refuses a second `export type` of one copy within a module. Two
   modules of one program that each name the copy are not compared.
   Should that be refused as well?

The question whether `take` should also move an `own ?fn` field stays
open, and nothing here acts on it.
