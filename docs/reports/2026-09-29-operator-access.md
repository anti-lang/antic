# Operators reach the private members of their module's classes

The session built Eddie's answer to the question of
`docs/reports/2026-09-29-sync-pairs-and-equality.md`.

## What changed

- An `operator fn` declared at module level reaches the private and
  protected fields and functions of the classes declared in its own module.
  An ordinary function of the module and an operator of another module reach
  none. `from_module_operator` of `src/antic/sema_call.c` decides it beside
  the rule of `tests` blocks.
- A closure inside the operator reaches what the operator reaches. A copy of
  a generic operator of a library keeps the module of the generic through its
  home, so the copies another module makes reach the class as well.
- `equal_entries` and `hash_entries` of `ConcurrentMap` are private again.
- `docs/decisions.md`, the object model, the additions and the overview carry
  the rule in the same commit.

## Tests

- `access_modules` builds `tests/modules/access/com/example/ledger.anti`,
  whose operators read private fields and call a private function of a class
  and of a generic class. A program of another module compares and hashes
  both in release and dev mode. The library failed to compile before the
  change.
- `errors/operator_access.anti` accepts the operators and a closure inside
  one, and refuses an ordinary function and a function of another class.
  `errors/operator_access_modules.anti` refuses an operator and a function of
  the importer.

## Gates

At `801af52` no build shows a warning. The host suite passes 1265 of 1265
(`build/scratch/pair/full4.log`). ASan passes 1264 of 1264
(`build/scratch/pair/asan-test2.log`) and UBSan 1264 of 1264
(`build/scratch/pair/ubsan-test2.log`), each without `no_paths`. The
docs-style checker reports nothing on each changed `.md` file.

## Provisional decisions

- The operator reaches protected members as well as private ones, a closure
  inside it reaches the same, and a copy of a generic operator keeps the
  module of its generic.

## Questions

None.
