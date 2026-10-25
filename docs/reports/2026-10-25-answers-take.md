# Step take: Q17 of the answers of 2026-10-24

The step builds `take(x.f)`, the one new form the feature freeze lets in by
name. Logs stand in `build/drive/logs/take-*.log`.

## What changed

- `b26e8de9`. T201 recorded, a defect met on the way, with its test.
- `93822f72`. `take(x.f)` built. The checker writes the node `EXPR_TAKE`
  from the call in `sema_check_take` of `src/antic/sema_value.c`.
  `lower_take` of `src/antic/lower_owning.c` loads the field and stores
  `none`. An `=` to the field later tears down the `none`, as R074 says.
  The check `use-after-free` counts the field alive after a `take`. The
  library file carries the node, so the format version is 92, and the two
  pinned library files and `tests/unit/test_modules.c` follow. The message
  of T187 names `take` beside `dup`. The overview marks `take` built. The
  three specifications, the guide's chapter on ownership,
  `docs/notes/value-rules.md` and `docs/notes/test-matrix.md` describe it.
- `db62bb4c`. The verifier of a library tree refuses a `take` node without
  an operand, or with one that is no field. No test damages such a node:
  the pinned library files hold no generic with `take`, and a crafted file
  would be the only test. The suite reads the node back whole through
  `modules_generic_take`.
- `4232e020`. The row of T187 names `93822f72`, and `CLAUDE.md` counts the
  suite and records its times.

The tests, written first and seen failing, `take-red.log`:

- `matrix_ownership_take_leaves_none`: the field holds `none` afterwards,
  in a function, a method, through a pointer, a nested field, a generic
  class and an empty field.
- `matrix_ownership_take_moves_on`: the value moves into an `own` field by
  `=` and by a literal, into an `own` parameter and through a local. The
  list whose nodes own their successor pushes at its front through `take`.
- `matrix_ownership_take_then_assign`: `=` after a `take` tears down the
  `none`, then the node it stored.
- `matrix_ownership_take_in_contexts`, `modules_generic_take` and two tests
  of `anti_test_ownership` hold the other contexts of the matrix.
- Refusals: a field that cannot be `none`, a field that is not `own`, a
  local and two arguments. An `own ?fn` field, the field of a temporary
  and a statement that drops the value are refused as well.
- The test of T187 holds the list with `push_front` through `take`, beside
  the refused `self.spare = self.head;`.

## Findings

- T187 closed by `take` in `93822f72`. Its row stood fixed at `249fc070`,
  which refuses `=` between two `own` fields. That refusal left no form
  that moves the node, and `take` is that form.
- T201 recorded, open, major. A local declared in the block of a `defer`
  or an `undo` cannot move into an `own` parameter there.
  `sema_move_local` refuses every local whose `deferred` mark is set, and
  the mark is set for a local of the deferred block as well. Left for the
  step of the area ownership. `matrix_ownership_take_in_contexts` passes
  the value of `take` straight to the call there instead.

`tests/known-failures.txt` held 21 lines with two entries before, T136 and
T138. It holds 24 lines with five entries after, the three modes of T201
added.

## Where the task and the code differ

The task asked for a test that the value "moves on as an own value" into an
`own` parameter. An `own` parameter of a pointer gives the object back
itself, by "Ownership and copies": the function does not tear it down at
its exit. The test deletes the node in the callee, as
`ownership_own_fields_and_parameters` does.

## `[provisional]` entries

- A statement that drops the value of `take(x.f)` is refused, and the
  message says that `=` with `none` tears the object down.
- `take(x.f)` writes its field as `=` writes it. The field of a temporary
  is refused, and a place `=` refuses is refused with the words of `=`.
  An argument that is no field is refused.
- The tree of a generic in a library file carries `take`, and the format
  version is 92.

## Gates

- Zero warnings in the three builds, `take-build10.log`,
  `take-asan-build.log` and `take-ubsan-build.log`.
- Host at `db62bb4c`: 6712 tests, 6710 passed and 2 skipped, the two
  `mimalloc_environment` tests. The three of T201 are expected failures and
  failed as listed. `take-host4.log`, `take-host5.log` and
  `take-host6.log`.
- ASan: 6710 tests, 6708 passed and the same 2 skipped, the three of T201
  as expected, `take-asan-*.log` in six parts.
- UBSan: 6710 tests, 6708 passed and the same 2 skipped, the three of T201
  as expected, `take-ubsan-*.log` in five parts.
- `emit_identity` and `link_identity_<target>` passed unchanged.
- The docs-style checker reported nothing on every `.md` file touched.

## Not run on the target

- `take` lowers to a load and a store of the field on every target. The
  suite builds and links the programs for every target on the Mac. A run
  on Linux and Windows waits for the VMs: `matrix_ownership_take_*` and
  `modules_generic_take`.

## State before the commit of the report

```text
$ git log --oneline -3
4232e020 Close T187 at the commit of take and count the suite
db62bb4c Verify the field of a take node read from a library file
93822f72 Build take(x.f), which moves an object out of an own field
$ git status --short
$ git rev-parse HEAD origin/main
4232e020335cf7630a226cd28f08f703387273fd
4232e020335cf7630a226cd28f08f703387273fd
```

The commit of this report follows, then its push.
