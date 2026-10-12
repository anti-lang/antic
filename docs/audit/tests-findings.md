# The findings of the test run

The test run builds the suite out until it holds every construct of the
language in every place it can stand. It holds every diagnostic and every
option as well, and it changes no code under `src/`. A defect that a test
shows gets the next free id and one row in the table below. Its test stays
in the suite, written as the rule says the program must behave. One line of
`tests/known-failures.txt` names the test and the id, so the suite expects
that test to fail. Decision 30 of "Decisions of 2026-10-11" in
`docs/decisions.md` states this.

A row holds seven cells:

- The id. The first is `T001`, and each new finding takes the next number.
- The test that shows the defect. A second test that shows the same defect
  names the same id in `tests/known-failures.txt` and gets no row.
- The construct and the context it stands in.
- What happens.
- What the rule says and where it says it.
- The grade.
- The state: `open`, or `fixed` followed by the commit of the fix.

The three grades:

- `severe`: a wrong result at run time, a memory error or a crash of a
  program antic built.
- `major`: a crash of antic or anti, or a rule the checker does not hold.
- `minor`: the text or the position of a message.

A `|` inside a cell is written `\|`. The fix of a finding removes its lines
from `tests/known-failures.txt` and writes `fixed` into the state of its
row. The row stays.

The test `known_failures` reads this table and the list. It fails when:

- a line of the list names no test
- the id of a line has no row, or a row marked `fixed`
- an open row has no line
- a row does not end with a grade and a state

| Id | Test | Construct and context | What happens | Rule | Grade | State |
|---|---|---|---|---|---|---|
