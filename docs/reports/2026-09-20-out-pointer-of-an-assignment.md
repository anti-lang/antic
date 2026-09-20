# The out pointer of an assignment

The defect of `docs/reports/2026-09-20-windows-pdb-and-install-layout.md`.
`*out = try inner();` reached `inner` with a null out pointer, and the
program faulted where the callee wrote through it.

## What it was

`lower_call` takes the out parameter of a failing call from
`l->out_address`, a field of the lowerer. Only `lower_let` ever wrote
that field. An assignment lowered its value through `lower_expr`, which
reached `lower_call` with whatever the last `let` had left there, and
then stored the result of the call, which is the error pointer, into the
place.

The reported case is the one that runs: an aggregate target takes the
`is_aggregate` path of `lower_assign`, which copies bytes and asks no
questions. A scalar target was louder. `x = try value();` failed IR
verification with `copy i64 has an operand of type ptr`, and
`*out += try value();` with `addov i64 has an operand of type ptr`. No
test held any of the three, and no program in the tree wrote them.

## What it is now

The assignment reads its place first, as it already did, and hands the
address to the call. Three targets have no address to hand over. A place
the back end keeps in a temporary has none, and a bitfield has none. A
compound assignment wants the operand in the place, not the result. Each
takes a slot of the frame, and the value moves from there into the place
after the handler. All three are scalars, so the slot needs no zero
table.

Nothing is cleared before the call, which is where an assignment parts
from a `let`. The storage of a `let` holds no value and its table is
zeroed, per the entry under "The object model" in `docs/decisions.md`.
The place of an assignment holds one, and the `=` of the callee destroys
it, as in every other assignment.

`docs/decisions.md` carries the rule as a `[provisional]` entry beside
the `let` it extends.

## The tests

Written first, and both were red on the compiler before the change.

`program_out_through_pointer` is the twenty-line case of the report, with
no standard library. `program_out_place_forms` covers a local, an array
element, a field, a bitfield, a compound assignment, an aggregate value
through `*out`, and the error path.

The emit-identity manifest gained the twelve lines of the two new
programs and changed no line it already held. The assembly of every
other program is byte for byte what it was.

## anti.os

The seven directory functions bound the result of a `try` to a local and
assigned it in the next statement. The binding and the `[AI AGENT]`
comment that explained it are gone, and each function writes
`*out = try f();`. `std_userdirs` and `std_userdirs_no_home` cover the
value path and the failure path.

## Results

492 ctest tests pass on the Mac, ASan 491, UBSan 491, none skipped. The
four new tests are the two programs, each run natively and cross-built
for macos-x86_64. `CLAUDE.md` carries the counts.

## The first question, answered

A failing call on the right of `+=` stays. A failing call is handled at
the point of use. That rule holds everywhere an expression stands,
rather than having a hole cut in it for one operator. A reader who
writes `total += try next();` means it. The entry in `docs/decisions.md`
is settled, not provisional.

## The question that remains

- The parser takes `try` in assignment position but not `catch`. So
  `x = f() catch e { }` is a syntax error while `let x = f() catch e { }`
  is not. The lowering handles the handler blocks either way. Whether the
  parser should take them is undecided.
