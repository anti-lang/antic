# The answers to the report on the answers to items 20 to 24

The four answers to `docs/reports/2026-09-22-answers-items-20-to-24.md` are in, one commit
each where a change was due. The development Mac passes 611 of 611 tests, and the ASan and
UBSan builds pass 610 each, without `no_paths`. The commit that changed code ran the full
suite first, and its push ran both sanitizer suites. The VMs did not run.

## What was done

1. Answer 1, `a22e201`. Three entries lost their tags: `own` before the name of a parameter,
   a moved error that is not named again, and `may fail` after a function type.
2. Answer 2, `cba4fa3`. A `concrete fn` takes the signature of the entry it fills exactly:
   `self`, the type and the `own` of each parameter, the result and `may fail`. The checker
   names the first difference in the order of the text. The message stands at the parameter,
   the result, the words `may fail` or the name. An unqualified body is compared with the
   entry of its base chain and with that of every interface of the chain that no qualified
   body fills. A qualified body is compared with the entry of its table.
   `errors/concrete_signature.anti` refuses a parameter type, a missing and an extra `own`,
   a missing and an extra `may fail`, a result, a parameter count and a missing `self`. It
   reaches the entry through a base, an interface, a qualifier, the root and `log.Sink` of
   another module, and the bodies that match give no message. `test_sema.c` accepts a
   matching set and refuses one parameter type. Every `concrete fn` of `std/` and `tests/`
   already matched, so none changed. `docs/decisions.md` carries the rule under "Object
   model" as a settled entry.
3. Answer 3, `7a03f5d`. The entry on `by k` now starts a negative step at the largest value
   that `by k` reaches, uses `i` and then decrements it. `docs/work-order-completion.md`
   keeps the old procedure, since it is the work order as given.
4. Answer 4. Nothing changed. `docs/notes/linker.md` already records the `-lto_library`
   warning, and reports no longer raise it.

## Found and fixed with answer 2

A qualified body whose table has no entry of its name filled nothing, in silence. Take
`concrete fn Named::radius` in a class whose base declares `radius` and whose interface
`Named` does not. It passed the check and counted as filling the base's `radius`. Lowering
left that entry zero. The specification refuses a `concrete fn` that matches nothing, and
the checker now says `fills no abstract function` for it.

The root's `serialize` holds `*Object` for its parameter in the checker, because the root is
built before any module and cannot name `anti.text`. The specification gives
`serialize(self, out: *text.Builder)`, and `tests/std/json.anti` replaces it so. The
comparison asks a replacement for a `*anti.text.Builder` by the module and the name of the
class. `docs/notes/sema.md` says so.

## Found and not done

The checker refuses two forms that the specification allows with ``declares `f` twice``. One
is two qualified bodies of one name for two interfaces whose signatures differ. The other is
an unqualified body beside a qualified one, where "A qualified body wins in its table over
an unqualified one". The refusal predates this session. It is work for a later session.

## Provisional decisions for review

None new.

## Questions

- The specification gives the root `copy(self) -> *Object`. The runtime allocates the copy
  and calls the entry as `copy(self, to: *Object)`, the checker declares that form, and the
  comparison now holds a replacement to it. A `concrete fn copy(self) -> *Object` written as
  the specification says is refused. It was accepted before, and the runtime then called it
  with an argument it did not read and ignored its result. No test replaces `copy`. Should
  the specification take the form of the runtime, or the runtime the form of the
  specification?
