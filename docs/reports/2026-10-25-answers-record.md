# Step record: the answers of 2026-10-24

The step writes the twenty answers to the questions of
`docs/reports/2026-10-22-solid-summary.md` into `docs/decisions.md` as
"Answers of 2026-10-24", Q1 to Q20. It changes no code. Logs stand in
`build/drive/logs/answers-*.log`.

## What changed

- `270cdc4e`. The new section of `docs/decisions.md` holds the four rules
  of the language, Q17 to Q20, and the sixteen other answers. The feature
  freeze names Q17, `take(x.f)`, as its one new form. The entries that the
  answers replace name them: the entry on `anti.log` under Q3, the
  `[provisional]` entry on the fields of `Logger` under Q4, the answers of
  a repository under Q5, the flags of the native libraries under Q6, the
  work directory of `anti doc` under Q7, `./c coverage` under Q12 and the
  version of clang of `anti bind` under Q14. The solid summary marks its
  twenty questions as answered.
- Same commit, by Q2. `docs/anti-syntax-overview.md`,
  `docs/anti-object-model.md` and `docs/anti-language-additions.md` hold
  Q17 to Q20 where each holds the subject. The overview marks all four as
  not built, and adds a `not-built` example of `take`. Of the answers of
  2026-10-18 the object model gains R017 and R074 on channels. The
  additions gain R083 and R009. The steps `answers-owned` and
  `answers-language` had written every other one into the three documents.
- Same commit, by Q16. Neither the overview nor the object model derives a
  compound assignment from an `operator fn` any more. Both name the simd
  struct of R037, and the overview holds a `refused` example of `+=` on a
  class with `operator fn add`.
- `7b9bcbe2`, by Q1. `CLAUDE.md` names the answers of 2026-10-24 under the
  feature freeze, and item 14 reads the thresholds of `anti.log` at the
  first line written.

## Where an answer does not stand

The object model has no section on `switch` beyond the cover of an enum,
and none on simd structs, generics or plugins. R009, R066, R134 and Q18
therefore stand in the overview and the additions alone. Q19 stands in its
section "Inheritance". The additions hold no rule of `align(N)` and no
section on enums, so R051 and R054 stand in the other two documents.

## Findings

None closed and none recorded. The task says that T187 closes with Q17.
Its row in `docs/audit/tests-findings.md` stands fixed at `249fc070`,
which refuses `=` from one `own` field into another. The record keeps the
row and says that `take` is the move the refusal left without a form.

`tests/known-failures.txt` holds 21 lines before and after, two entries,
T136 and T138, both `host=windows`.

## `[provisional]` entries

- `take` is a built-in name, as `close` is, so a function `take` in scope
  keeps its name. The overview and `Builder.take` of `anti.text` name one.
- A simd struct meets the five constraints of Q18 where its operator
  applies to its lanes, and no other hook.
- The walk of Q19 follows `inherits` alone, and an `implements` line gives
  no type argument.
- The bare name of Q20 reaches a constant in its class and in the types
  nested in it, and not in a derived class.

## Left for the steps that build the answers

Q3 to Q15 and Q17 to Q20 in the code, the tests and the guide. That
includes the setters of Q4, the DESIGN comment of Q12, the messages of
Q14 and Q15 and the split of Q9. The message that T187 refuses with may
then name `take`.

## Gates

- The docs-style checker reported nothing on the six `.md` files of the
  commits and on this report, `answers-check6.log`.
- `overview_examples` passed, `answers-overview.log`. It passed again
  among twelve tests that read the edited documents, `note_claims`,
  `no_sdk_fetcher`, `repo_layout` and `source_tree` among them. All twelve
  passed, `answers-doc-tests.log`.
- No suite ran. The task names these two checks, and `CLAUDE.md` asks for
  the checker alone on a commit of documents. `emit_identity` and
  `link_identity_<target>` stay as they were, since no code changed.

## Not run on the target

Nothing. The step changes no code.

## State before the commit of the report

```text
$ git log --oneline -3
7b9bcbe2 Bring CLAUDE.md up to the answers of 2026-10-24
270cdc4e Record the answers of 2026-10-24 and write them into the specifications
69c0e40c Report the corrections of E1 and C3 and the findings of the checks
$ git status --short
$ git rev-parse HEAD origin/main
7b9bcbe284d1b50eb1523ac98d7dbd3fec90867a
7b9bcbe284d1b50eb1523ac98d7dbd3fec90867a
```

The commit of this report follows, then its push. The reply of the
session holds the state after it.
