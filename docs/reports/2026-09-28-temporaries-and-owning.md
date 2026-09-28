# Temporaries, the one rule of owning, and a dropped failing call

The session built Eddie's answers to the two questions of
`docs/reports/2026-09-28-fresh-values-and-hash.md` and fixed the fault of a
dropped failing call that report found.

## What changed

- `5856652`. A statement that dropped the result of a `may fail` call with a
  handler lowered the call with the out address an earlier call had left,
  and the callee wrote through it. The statement now gives the call a slot
  with zeroed tables and tears down what the call or a `yield` left there.
  `programs/dropped_failing.anti` ended with a segmentation fault first.
- `32f59e6`. A type owns something when its teardown does anything.
  `sema_type_owns` reads `sema_needs_teardown`, so `=`, `let` and `if let`
  refuse to copy a value whose class has a `destruct`, and `dup` copies it.
  `errors/owning_destruct.anti` reported one of its seven refusals first.
  A collection gives its room back in its `destruct`, so `=` between two
  collections is refused as well, which `errors/collection_copy.anti` holds.
  The overview and `CLAUDE.md` no longer list that refusal as not built.
  `programs/owning_destruct.anti` counts `dup`, a move and a binding. It
  passes on the compiler before the change too.
- `6ee01e8`. A fresh value read through a field, passed as a receiver or
  whose address a call takes lives to the end of its statement. A slot per
  such value holds its address once it is made, and the end of the statement
  tears down what the slots hold. A condition ends its values before its
  branch, and every exit of the function ends those still kept. A call
  result is now a legal receiver. `programs/statement_temps.anti` did not
  compile first, and its field reads alone left eight boxes alive.

## Gates

At `6ee01e8` no build shows a warning. The host suite passes 1228 of 1228
(`build/scratch/suite10.log`). ASan passes 1227 of 1227
(`build/scratch/ctest-asan3.log`) and UBSan 1227 of 1227
(`build/scratch/ctest-ubsan3.log`), each without `no_paths`. The docs-style
checker reports nothing on each changed `.md` file.

## Provisional decisions

- The refusal names a class that owns through its `destruct` alone with
  `` has a `destruct` ``.
- A condition of `if` or of a loop ends its temporaries before its branch,
  and each exit of the function ends every temporary still kept.

## Open

- A `break` or a `continue` inside a handler of a statement leaves before the
  end of that statement. A temporary the statement made before the handler
  then stays alive. No test reaches it.
- The message `` `Holder` has `own` fields `` also names a class whose field
  only has a `destruct`.
