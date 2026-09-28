# Handler exits end temporaries, the refusal names the destruct

The session fixed the two open points of
`docs/reports/2026-09-28-temporaries-and-owning.md`.

## What changed

- `3cce1f3`. A `break` or a `continue` inside a handler left its statement
  before its end, and a temporary the statement made before the handler
  stayed alive. Each loop records the temporaries kept when it starts, and
  `break` and `continue` end those kept since. The temporaries of the loop
  itself stay for its end. `programs/handler_exit_temps.anti` counts
  `continue`, `break` and one loop inside another in both modes. Before the
  change 3, 4 and 14 boxes stayed alive.
- `3b0ee0e`. The refusal of a copy of a class that owns through a field
  alone follows that field down to its reason: `` `Holder` holds `Counted`
  in `counted`, which has a `destruct` ``, through a struct and through more
  than one class as well. `errors/owning_destruct.anti` failed on the old
  messages first. A message is no program, so no leak check applies to it.

## Gates

At `3b0ee0e` no build shows a warning. The host suite passes 1231 of 1231
(`build/scratch/suite12.log`). ASan passes 1230 of 1230
(`build/scratch/ctest-asan4.log`) and UBSan 1230 of 1230
(`build/scratch/ctest-ubsan4.log`), each without `no_paths`. The docs-style
checker reports nothing on each changed `.md` file.

## Provisional decisions

- A field whose class has `own` fields keeps `` has `own` fields `` in the
  message, as `errors/own_copy.anti` pins it.
