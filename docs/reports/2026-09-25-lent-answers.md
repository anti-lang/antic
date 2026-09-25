# Eddie's answers on lent

Eddie answered the three questions of
`docs/reports/2026-09-25-collection-lanes.md`. Each answer is built with
tests for its rules and refusals, and recorded in `docs/decisions.md` and
`docs/anti-language-additions.md`.

## What was done

- The iterator decides what a walk with `&` binds. `operator fn value` may
  return a tuple with lent pointer parts, `-> (K, lent *V)`, and `lent`
  stands before a part there and nowhere else. `for x in &c` binds the tuple
  as it is, and `for x in c` and `to_slice` take a copy of every part. Such
  a tuple is bound by its loop alone.
- A pointer or a slice derived from a lent one is lent: `s.ptr`, `&p.field`,
  `&s[i]`, a part of a lent slice and a slice of an array that a lent pointer
  reaches. An argument of an `extern fn` is the one exit, and the
  specification states that the guide names C's contract.
- The parts of a pattern are what the value type says, in any position. The
  rule that every part after the first is lent is gone. An element lent
  whole, `lent *(A, B)`, as a slice gives one, gives a lent pointer to every
  part.

Commits: `34b7a10` the derived pointers, `13b3fe3` the iterator's value and
the parts of a pattern. Answers 1 and 3 are one change, since both follow
from the type of the value.

## Tests

- `programs/lent_derived.anti` in release and dev mode passes each derived
  pointer on to a `lent` parameter and to C. `errors/lent_derived.anti` holds
  eight refusals, one of them lent for one turn of a loop.
- `programs/lent_tuples.anti` in release and dev mode walks a map whose keys
  and values stand in two arrays in all four forms and with `to_slice`. It
  also walks a value of three parts with the lent one last, and a slice of
  tuples with `&`. `errors/lent_tuples.anti` holds eleven refusals: `lent`
  outside the result of `value`, `lent` before a part that is no pointer,
  the tuple at a `let`, an argument, a type argument, `return` and a typed
  `let`, a change of a copied part and of a lent part, and `&` over a value
  of copies.
- `generics_modules` walks a library class whose `value` gives
  `(int, lent *int)`, in both modes.

## What failed and how it was fixed

- `hold(e)`, a generic called with the tuple, passed at first, since the
  type argument kept the lent part. Inference now strips the lent parts of a
  tuple as it strips `lent` from a pointer, and the call is refused.
- Over a slice of tuples, `for (a, b) in &pairs` now gives `a` as a lent
  pointer, so the refusal of `errors/for_patterns.anti` line 33 is now the
  read-only rule. The assembly of `programs/for_patterns.anti` changed with
  the binding of that part, and its six entries of the manifest were written
  again. No other entry changed.
- `emit_identity` needed the entries of the two new programs.

## Proof

- Host suite: 1099 tests before `34b7a10` and 1103 before `13b3fe3`. Each run
  failed `emit_identity` alone, for the missing entries, which were written
  and rechecked before the commit.
- Sanitizers, once for the push at `13b3fe3`: ASan passed 1102 of 1102 and
  UBSan passed 1102 of 1102, the host suite without `no_paths`.

## Provisional decisions

All in `docs/decisions.md`:

- which paths a derived place takes, and what a refusal names;
- `lent` before a part at the top level of the tuple;
- the copy of every part, and no teardown of the parts;
- a lent pointer to every part of an element lent whole;
- the refusals of a tuple with lent parts and of a copied part.

## Questions for Eddie

- The loop tears down no part of a value with lent parts, as it tears down no
  copy of an element it is lent. A value that builds a part fresh, for
  example a key as a new text, would leak. Should the loop tear down the
  parts that are not lent pointers?
