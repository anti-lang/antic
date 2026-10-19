# The step answers-owned

The step builds R074, R083 and R081 of "Answers of 2026-10-18" in
`docs/decisions.md`, in that order. The test of `=` to an `own` field and the
five refusals of R083 failed before the code changed. The test of `recv`
in each place was written after the change of lowering, so no run shows
it failing. R081 needed no code. The logs stand in
`build/drive/logs/` under the prefix `answers-owned-`.

## What changed

| Rule | Tests | Commit |
|---|---|---|
| R074 | `matrix_ownership_assign_to_an_own_field`, `matrix_ownership_received_value_in_each_place`, `matrix_ownership_values_in_a_channel` | `bc982cdd` |
| R083 | five refusals `matrix_ownership_{delete,destroy}_of_*` | `41381f4d`, `508a58f7`, `3d4234f5` |
| R081 | `matrix_ownership_own_pointer_that_a_generic_does_not_hand_on` | `3323eb0a`, `6544e24e` |

- R074. `=` to an `own` pointer or slice field tears down what the field
  held before the new value goes in. The value `recv` gives, and the
  value an arm of `select` takes, belongs to the block that holds the
  `recv`, and the end of that block tears it down. Before the change the
  first test printed `alive 9` at its end, and `--memory-checks` reported
  10 leaks, in `answers-owned-t1m.log`.
- The deletes that R074 makes a second free left the code:
  `anti.text.Builder.take_in`, `grown` of `anti.args`, the tests
  `generic_copies`, `modules/generics` and `ownership_own_fields_and_parameters`,
  and every `destroy` of a received value in the tests of channels.
  `anti.toml.Document` and the test `wrap_c` give their handle back their
  own way, so the field lost `own`. `destruct` of `Shared<T>` gives up its
  cell through the address of the field.
- R083. `delete` and `destroy` of a parameter without `own`, and of a
  part of a singleton, are refused. `Collection.destroy_element` takes
  `own p: *T`. The tests `catch_none`, `owning_values`, `modules_ownership`
  and `unit_sync` and six examples of the guide take the parameter as
  `own`. `run_destroy` of `modules/ownership` frees what it took over.
- R081 needed no code. The test of T127 holds the rule now.
- `docs/anti-object-model.md`, `docs/anti-language-additions.md`,
  `docs/anti-syntax-overview.md` and chapters 9, 11, 12, 13 and 15 of the
  guide state the three rules. `7d3a387b` is the change of chapter 12.

## Findings

- T127 closed as no defect in `3323eb0a`, recorded in `6544e24e`. Its
  test had stopped compiling: the counter `live` was private to `Key`, so
  the expected failure failed for another reason than its finding. The
  counter is `pub` now.
- `docs/audit/tests-findings.md` and `tests/runners/run_known_failures.cmake`
  take `no defect` as a state that closes a row, as `fixed` does.
- The step recorded no new finding and left none of its own open.
- `tests/known-failures.txt`: 29 lines before, 26 after. It names 10
  tests before and 7 after, 5 of which run on the Mac.

## `[provisional]` entries

All three stand under "What the solid run decided":

- R074 on a channel. The value of a `recv` belongs to the block that holds
  it. A `recv` in a loop condition first tears down its value of the turn
  before, and `destroy` of the value ends it early.
- `=` to an `own` field gives the old memory back to the C library. A
  field given the pointer it holds keeps it. A store through the address
  of the field tears nothing down.
- What R083 refuses: the parameter as `p` and `&p`, `self` and a channel
  parameter. A part of a plain parameter is not refused. Any place whose
  chain passes through a singleton counts as a part of it.

## `emit_identity` and `link_identity`

- `bc982cdd` moves 51 programs, 306 texts: `anti.lang.StackTrace.capture`
  assigns the `own` slice `frames`, which now tears down the old slice.
- `508a58f7` moves `catch_none` and `owning_values`: an `own` pointer
  parameter carries `noalias`.
- No `link_identity_<target>` test changed.

## Gates

- Zero warnings in the three trees: `answers-owned-host-build.log`,
  `answers-owned-asan-build.log` and `answers-owned-ubsan-build.log`.
- Host, 5161 tests: `-I 1,2000` 2000 passed in 351 s, and
  `-I 2001,5161` 3187 passed in 313 s. The 5 expected failures of the
  Mac failed as listed, and the two `mimalloc_environment` tests skip.
  Logs `answers-owned-host-final-a2.log` and `-b.log`.
- ASan, 5159 tests: `-I 1,1150` 1150 passed in 231 s, `-I 1151,2250`
  1119 in 163 s and `-I 2251,5159` 2917 in 563 s. The third part ran
  past 9 minutes. Logs `answers-owned-asan-a.log` to `-c.log`.
- UBSan, 5159 tests: `-I 1,1150` 1150 in 244 s, `-I 1151,2250` 1119 in
  125 s, `-I 2251,3700` 1458 in 113 s and `-I 3701,5159` 1467 in 333 s.
  Logs `answers-owned-ubsan-a.log` to `-d.log`.
- The docs-style checker reports `0 error(s), 0 warning(s)` on every
  `.md` file the step touched, this report included.

## Not run on the target

The change is in the front end and in lowering, and the Mac builds the
new tests for every target through `emit_identity`. The programs run on
the Mac alone:

- `matrix_ownership_assign_to_an_own_field_*` on Linux and Windows.
- `matrix_ownership_received_value_in_each_place_*` on Linux and Windows.
- `matrix_ownership_values_in_a_channel_*` and
  `matrix_ownership_delete_of_a_channel_that_holds_values_*` on Linux
  and Windows.

## Questions

- The feature freeze and the state of ownership in `CLAUDE.md` still say
  that R074 and R083 wait for Eddie and that T127 holds R081. The task
  did not name `CLAUDE.md`, so it stands as it was.

## State

After the push of `3d4234f5`, before the commit of this report:

```text
$ git log --oneline -3
3d4234f5 Take the channel a worker deletes as own in the unit test of sync
7d3a387b Say in the memory chapter how = to an own field meets an allocator
6544e24e Record T127 as no defect with its commit
$ git status --short
$ git rev-parse HEAD origin/main
3d4234f562b18c995aca3ce0558ba8ec517c3a86
3d4234f562b18c995aca3ce0558ba8ec517c3a86
```
