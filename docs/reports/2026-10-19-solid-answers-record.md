# The step answers-record

Eddie answered the ten cases of "Rules of 2026-10-13" that waited for
him, the question of R081 and decision 16 of 2026-10-11, on 2026-10-18.
This step writes the answers into `docs/decisions.md` and changes no
code. The logs stand in `build/drive/logs/` under the prefix
`answers-record-`.

## What changed

`610dfa87` adds the section "Answers of 2026-10-18" before "Open". It
holds R074, R083, R081, R017, R009, R037, R051, R054, R066, R100, R134
and D16, in the order of Eddie's answers. None is provisional.

The same commit marks the old entries:

- Each of the ten rules that waited, and R081, ends with "Answered by"
  and the id of its answer. The answer follows in a few words.
- Decision 16 of 2026-10-11 ends with a sentence naming D16.
- The description of the kind Eddie, the entry of the ownership step,
  the provisional entry on R081 and the feature freeze name the new
  section.
- The ten questions left "Open". Two items stay there: the enums used as
  bit flags and the release that holds the native libraries.
- Seven older entries that the answers replace end with a sentence that
  names the answer: `switch`, the N of `align(N)`, `fail "text"`, `recv`,
  `anti.plugin.Library`, the levels of the members of a nested type and
  the literal of a generic struct.

## Findings

The step closed no finding and recorded none. T127 stays in
`tests/known-failures.txt` and in `docs/audit/tests-findings.md` as it
was. R081 of the new section closes it as no defect, and the step that
builds R081 changes its test and its row. `tests/known-failures.txt`
holds 29 lines before and after. The step added no `[provisional]`
entry.

## Gates

The commit changes `docs/decisions.md` alone, so by the rule of
`CLAUDE.md` it runs the docs-style checker and nothing else, and the task
said the suites need not run. No suite ran, and no build. The checker
reports `0 error(s), 0 warning(s)` on `docs/decisions.md` and on this
report, with exit status 0, in `answers-record-docs.log`. The test
`note_claims`, which reads the claims of `docs/decisions.md`, passed
alone, in `answers-record-note-claims.log`. `emit_identity` and the
`link_identity_<target>` tests are unchanged, since no code changed.

## Questions

- The rule of `CLAUDE.md` puts a decision that changes the language into
  the specifications and the overview in the same commit. The task named
  `docs/decisions.md` alone, and the commit of the rules of 2026-10-13
  did the same. The specifications and the overview are left to the
  steps that build the answers.
- The feature freeze in `CLAUDE.md` still says the ten cases wait for
  Eddie and that no step changes them. The task did not name
  `CLAUDE.md`, so it stands as it was. The entry of the freeze in
  `docs/decisions.md` now names the answers and the two steps that build
  them.

## State

After the push of `610dfa87`, before the commit of this report:

```text
$ git log --oneline -3
610dfa87 Record the answers of 2026-10-18 in the decisions
503767bf Report the tenth session of step 34
9a703745 Record the split of xe and xs of step 34 in the summary
$ git status --short
$ git rev-parse HEAD origin/main
610dfa87258b1717587ae28d3fb3cb2cd0bb58f2
610dfa87258b1717587ae28d3fb3cb2cd0bb58f2
```
