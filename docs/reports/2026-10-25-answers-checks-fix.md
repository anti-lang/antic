# The corrections of E1 and C3 and the open findings of the checks

The step `checks-fix` of the answers of 2026-10-24 built the part before
the answers: the corrections of E1 and C3 of 2026-10-25, the E2 entry on a
store into an `own` field, and every open finding of
`docs/audit/tests-findings.md` but T136 and T138, which Windows alone
shows. Q17 to Q20 and the sixteen decisions of 2026-10-24 belong to the
steps of the run after this one. Logs are under
`build/drive/logs/checks-fix/`.

## The corrections

- E1, `d5fa66f5`. `parallel` takes a local reference, as its array and as
  an argument of its worker. `dispatch` takes one, as its object and as
  an argument of its worker, when the job is the value of a `let` and a
  later statement of the same block joins it on every path. No
  `return`, `fail`, `try` or `break` that leaves the block stands between
  them. The 117 `unchecked(escaping-local, ...)` clauses are gone, 94
  before a `parallel` and 23 before a `dispatch`, from the tests, the
  guide and the fuzzer. Five new refusals keep the cases that stay: a
  job joined on one path, and a `return`, a `break`, a `try` and a `fail`
  before the join. `..._passed_to_parallel` and the `parallel` test of
  T193 are programs now, since the correction allows what they refused.
- C3, `7021c4c6` and `9b7d6e52`. A `do ... while` without a `break` or
  `continue` of its own, a `select` and a `try` block end the path when
  every path through them does. The second commit came from the review
  of the first: a `yield;` earlier in a handler that ends in `return`
  goes on after the block, and lowering would have marked that block
  unreachable. Four refusals and one program test the forms.
- E2, `58aec540`. The entry on a store into an `own` field cites the rule
  E2 of 2026-10-20 and is no longer provisional. `param_facts` reaches the
  node through `h.node`, and `728b2a45` writes its six new digests of
  `emit_identity`.

Both corrections stand in `docs/decisions.md` under "Compile-time checks
of 2026-10-20". The E1 rule goes into `11-ownership.md` of the guide and
`docs/notes/warnings.md`, and C3 into the overview, the additions and
`04-flow.md`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| T193 | `d5fa66f5` | E1 walked the arguments of the worker of `dispatch` as an ordinary call, which gets them lent. |
| T197 | `d5fa66f5` | `root_of` followed no `??`. |
| T198 | `d5fa66f5` | `anchor_of` saw a parameter only where it named the place directly, and a local that copied its pointer reached nothing. |
| T199 | `d5fa66f5` | `walk_arms` declared the name an arm binds with no reference. |
| T200 | `7021c4c6` | R003 named no `do ... while`, and the correction of C3 adds it. |
| T191 | `28e2d2f3` | The field `super` of a class whose base was refused had no type, and `method_callee` read it. |
| T194 | `222e4ee3` | C1 read nothing in a `defer` or an `undo`. |
| T195 | `222e4ee3` | E2 read a `defer` or an `undo` once, with the state where it stands. Both walks now read each pending statement at every exit that runs it, with the state of that exit, in the order lowering runs them. |
| T196 | `56a8dea3` | The walk of C2 did not enter the body of an anonymous function. |
| T190 | `3b9ee629` | `anti fmt` counted a block inside brackets as part of the expression around it. The same path fixed the line after an `if let` that holds a struct literal, and three sources moved to the canonical form. |

Findings left: none open on the Mac. T136 and T138 wait for Windows. No
new finding was recorded.

## Counts

- `tests/known-failures.txt` held 22 lines before and 2 after, both with
  `host=windows`. The Mac expects no failure.
- `[provisional]` entries added: two. E2 and C1 read a `defer` and an
  `undo` at each exit, and C2 checks the indices of a closure where it
  stands. Changed: the entries on C1 and on E2 in a `defer`, which name
  T194 and T195. Turned plain: the entry of E2 on a store into an `own`
  field.
- Not run on the target: nothing. Every change is in the front end, in
  `anti fmt` or in lowering that the Mac builds for all six targets.

## Gates

- No warning: `final-build-host.log`, `final-build-asan.log`,
  `final-build-ubsan.log`.
- Host, `final-host-1.log`, `final-host-2.log` and `final-host-3b.log`:
  6688 tests, 6686 passed and 2 skipped, the `mimalloc_environment`
  tests, with no expected failure. `final-host-3.log` failed at
  `emit_identity` alone, before `728b2a45`.
- ASan, `final-asan-1.log` to `-6.log`, and UBSan, `final-ubsan-1.log` to
  `-5.log`: 6686 tests each, 6684 passed and 2 skipped.
- `emit_identity` changed for the six entries of `param_facts`, by the
  change of E2 above. `link_identity_macos-arm64` passed unchanged.
- The docs-style checker reported nothing on every `.md` file touched.

## State before the commit of the report

```text
$ git log --oneline -3
728b2a45 Write the digests of param_facts that reaches its node through the owner
3b9ee629 Lay out a block inside brackets as the body of an anonymous function
56a8dea3 Check the indices in the body of an anonymous function
$ git status --short
 M CLAUDE.md
 M docs/audit/tests-findings.md
$ git rev-parse HEAD origin/main
728b2a457001f330cb3522da86ae751cdeb2efcb
d41545085118683a3db52310797780ceadffb553
```

The two differ because the nine commits are not pushed yet. The push
follows the commit of the report, and the reply of the session holds the
state after it.
