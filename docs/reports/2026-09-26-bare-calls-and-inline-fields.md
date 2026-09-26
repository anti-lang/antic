# Bare calls of shared operators, and inline fields in equals

The step builds Eddie's two answers to the questions of
`docs/reports/2026-09-26-equality-answers.md`. Each answer is recorded in
`docs/decisions.md` and `docs/anti-language-additions.md`. The test of a `Map`
with a tuple key waits for the hashed lane, as Eddie said.

## What was done

- `0860409`: a bare call `eq(a, b)` of a shared operator name picks the
  function by the type of its first argument. The checker reads the call as
  `a.eq(b)`, whose receiver the lookup of a function of a type already reads.
  A first argument no shared function takes is refused with `` no `operator
  fn eq` of the module takes `int` first ``. Tests:
  `programs/operator_overloads.anti` and `errors/overloads.anti`.
- `370c38e`: the runtime's `equals` and `hash` of `Object` walk an inline
  struct, class or array field. A class that holds a union in place gets no
  default `==` unless it replaces `equals`, and the refusal names the field.
  The default `==` of a struct compares an array part element by element, so
  a class and a struct with the same fields compare alike. Tests:
  `programs/eq_inline.anti`, `errors/equality.anti` and `records_type_ids`.

## What failed and how it was fixed

- The field record of an array held no length. Its type id now carries the
  innermost element in the third byte and the count from the fourth byte up.
  The record carries the descriptor of a struct or class element. Readers
  that mask the low two bytes are unaffected. Three pins changed with it:
  `dump/sizes.ir`, the manifest of `emit_identity` for fifteen programs with
  array fields, and the digest of `return42`.
- `clib_classes` crashed with a stack overflow in `hash_value`. The record of
  an `implements` field is a class value of the interface, and its table leads
  back to the object that holds it. The runtime now passes over a field of
  abstract class type, which only such a sub-object can be.
  `programs/eq_inline.anti` compares a class with an `implements` field, and
  fails without the fix.
- `errors/equality.anti` used an array field as the field without `==`. It
  now uses a union field in the struct and in a class, and an `f16` array in
  the tuple and the variant.

## Proof

- Host: 1140 of 1140 passed at `370c38e`, in `build/scratch/suite9.log`.
- ASan: 1139 of 1139 passed at `370c38e`, in `build/scratch/asan-suite.log`.
- UBSan: 1139 of 1139 passed at `370c38e`, in `build/scratch/ubsan-suite.log`.
- The docs-style checker reports nothing on every file the step touched.

## Questions for Eddie

- The runtime still passes over a field whose record has type id none. That
  covers a variant, a `?T`, an `own fn`, a Mutex, a channel, a Regex and a
  Match. A struct's default compares a variant and a `?T` part. Should the
  record carry enough for the runtime to compare them too?
- An array outside a struct, a class, a tuple or a variant still has no `==`.
  Should `a == b` on two arrays compare them element by element?
