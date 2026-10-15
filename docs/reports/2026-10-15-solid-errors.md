# Solid: the findings of the area errors

The step fixed the open findings of `docs/audit/tests-findings.md` whose
test starts with `matrix_errors_`, in one session on 2026-10-15. Every
one is fixed, and `tests/known-failures.txt` holds no line of the area.
None of them was a case the freeze keeps. The logs lie in
`build/drive/logs/solid-errors/`.

## Findings closed

| Finding | Grade | Commit | Cause |
|---|---|---|---|
| T044 | severe | `8ce0a673` | Nothing checked that the handler of a value ends every path, so a binding held what stood in its memory. |
| T045 | severe | `97a0f88b` | `try` on a call that cannot fail and gives a `?T` or a `?*T` was taken as the guard of a pointer. |
| T047 | severe | `1aeebd84` | The refusal of a bare caught error ran before the value of `=` had a symbol, and `yield` and the parts of a literal had none. |
| T049 | severe | `126d5119` | `??` on a `??U` gave the `??U` where its right side may be `none`, and the cause of T048 refused the copy over a `?int`. |
| T048 | major | `126d5119` | The rule that a T passes where a `?T` is expected left out a T that is an optional itself. |
| T050 | major | `1a36a0bd` | The parser took `try` before a call alone, and the call of `alloc T(args)` is inside the `alloc`. |
| T046 | minor | `1a36a0bd` | The parser refused `try` before a name or a field before its type was known. The operand case had the cause of T045 and closed in `97a0f88b`. |
| T051 | minor | `e571e91e` | `alloc T(args)` added its own message to a call whose check had failed. |

- T046 and T050 share one commit, since both came from the one case of
  the parser that read `try`.
- T048 and T049 share one commit. The copy of a generic over a `?int`
  failed for the cause of T048, so T049 closes with both changes alone.
- The handler of T044 needed the position of its `catch`, which a named
  handler did not keep. The tree of a generic in a library file carries
  it, so `ANTL_VERSION` is 86. The version byte of `scale_antl`, the two
  hex listings of `tests/modules/` and the version `CLAUDE.md` names
  changed with it.
- `emit_identity` and every `link_identity_<target>` stayed as they
  were.

## Findings left open

None of the area. The step recorded no new finding.

## Documents

- `docs/site/developers-guide/06-errors.md` states the refusals of T044
  and T047 in its pitfalls, where it said the checker accepts them.
- `docs/notes/doc.md` says the format was 85 then, where it said it is
  85 since.

## Provisional entries

Under "What the solid run decided" in `docs/decisions.md`:

- Decision 20 refuses the handler of a value that a `let`, an
  assignment or a guard takes. The handler of a call that stands as a
  statement may reach its end, `break` and `continue` end a handler, and
  the message stands at `catch`.
- A `?U` passes where a `??U` is expected, and `o ?? b` on a `??U` gives
  the `?U` that `b` reaches, `none` included.

No feature, option, command or public item was added.

## Counts and gates

`tests/known-failures.txt` held 261 lines, 241 of them tests, before the
step. It holds 228 lines, 208 of them tests, after it.

- Host, `ctest -j14` in two parts: 2000 and 2978 tests pass, two
  `mimalloc_environment` tests skip, and the 208 listed tests fail as
  expected. Logs `h1.log` and `h2.log`, run at `e571e91e`, which the last
  commit changes in `docs/` alone.
- ASan: 1150, 1112 and 2724 pass, `final-a1.log` to `final-a3.log`.
- UBSan: 1150, 1112 and 2724 pass, `final-u1.log` to `final-u3.log`.
- The build has no warning. No change touched a platform layer, a link,
  a sysroot or a package, and no finding named a host, so no VM ran.

## State before this report

The report commits after these commands ran.

```text
$ git log --oneline -3
fe39eacd Record the commits of the findings the step errors fixed
e571e91e Give an alloc of a call that reported its mistake no second message
126d5119 Take a ?U where a ??U is expected, and give ?? on a ??U its ?U
$ git status --short
$ git rev-parse HEAD origin/main
fe39eacdabaf67dc532d746700fffc7ed7432747
fe39eacdabaf67dc532d746700fffc7ed7432747
```
