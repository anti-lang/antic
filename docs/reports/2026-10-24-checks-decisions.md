# The decisions of the compile-time checks

The step wrote Eddie's compile-time checks of 2026-10-20 into the documents
and changed no code. Logs are under `build/drive/logs/checks-decisions/`.

## What changed

Commit `45eb07af`:

- `docs/decisions.md` holds the section "Compile-time checks of
  2026-10-20", after "Answers of 2026-10-18". It holds E1 `escaping-local`,
  E2 `use-after-free`, C1 `uninitialized-read` and C2 `index-out-of-range`.
  It holds the sweep C3, C4 `lock-order`, C5 `blocking-under-lock`, C6
  `unused-result` and what the run time keeps.
- "Ownership and copies" in `docs/anti-object-model.md` called ownership
  stated and not checked. That sentence now names the two ownership checks.
  It leaves two holders of one object to `--memory-checks` and the versions
  of a `Pool`.
- `docs/anti-syntax-overview.md` lists the two warnings and the five safety
  checks in its two tables, with a "Not built yet" line under them.
- The list of safety checks of `docs/anti-language-additions.md` names the
  new ones.
- `docs/notes/warnings.md` lists them, with meaning and fix, under a
  section "Decided and not built".

## Mistakes of the task

- The task counts four new safety checks. E1, E2, C1, C2 and C4 are five.
  The documents list all five.
- The test `warning_names` holds the two tables of `docs/notes/warnings.md`
  equal to the table of `src/antic/warnings.c`. A row in those tables
  without the name in the code fails the test, and the task changes no
  code. The new names stand in a section of their own, which the runner
  does not read, and the step that builds a name moves its row. The runner
  passed on the edited note, `warning_names.log`.

## Provisional entries added

- C2 proves an index out of range against a length that is one number on
  every target. A length from `size_of` keeps the bounds check of dev
  mode, by the entry on decision 24.
- C4 counts no edge from a lock to itself as a cycle, since the graph
  cannot tell one object of a class from two.

## Left as it stands

"Ownership and copies" says that a program that deleted the old value of
an `own` field before `=` would free it twice. E2 makes `=` to a dead `own`
field of `self` tear down nothing. The sentence stays until the step that
builds E2 changes it with the code, since the task named one sentence of
the object model.

## Findings and gates

- No finding closed, none left, none recorded.
- `tests/known-failures.txt` held 2 entries before and after.
- The docs-style checker reported nothing on the five files, `style.log`.
- The commit changes files under `docs/` alone, so by `CLAUDE.md` it runs
  the checker and no suite, and the task says the suites need not run.
  No suite ran, so this report gives no pass counts. `emit_identity` and
  `link_identity_<target>` cannot change without a change of code.

## State after the commit of the documents

```text
$ git log --oneline -3
45eb07af Record the compile-time checks of 2026-10-20
2c42ee9b Report the step of the open findings
2c445674 Bring CLAUDE.md up to date after the answers and the open findings
$ git status --short
$ git rev-parse HEAD origin/main
45eb07afe9639689fe2b4d2d79e21a7b44c19d26
45eb07afe9639689fe2b4d2d79e21a7b44c19d26
```
