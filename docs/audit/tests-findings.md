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
| T001 | `matrix_flow_return_value_in_defer_refused` | `return`, `break`, `continue`, `fail` and `try` inside a `defer` and inside an `undo` | antic stops with a bus error on every one of them inside a `defer`, and on `fail` and `try` inside an `undo`. It compiles `return`, `break` and `continue` inside an `undo` without a message | The checker refuses all five in both. Decision 19 of "Decisions of 2026-10-11" in `docs/decisions.md` | major | open |
| T002 | `matrix_flow_yield_in_defer_refused` | `yield` inside a `defer` of a `catch` handler | antic stops with a bus error and prints nothing | A crash of antic is a defect whatever the documents say. No document names `yield` inside a `defer`. The test expects a refusal, as decision 19 gives the five exits | major | open |
| T003 | `matrix_flow_defer_failing_call_in_try_dev` | a failing call as the statement of a `defer` in a `try` block | antic stops with a bus error and prints nothing | The handler of the block takes the error. `try { ... } catch e { ... }` handles every unhandled failing call in the block, under "Errors" in `docs/anti-object-model.md` | major | open |
| T004 | `matrix_flow_if_let_binding_in_closure_dev` | the name an `if let` binds, read by a closure in the block of the `if let` | antic refuses the program with `` `h` may be `none`, test it before reading it `` | `if let a = e { }` binds the value without `none` in its block. "Generics and collections" in `docs/decisions.md` | major | open |
| T005 | `matrix_flow_closure_reads_caught_error_dev` | the error that `catch e` binds, read by a closure in the handler, after a call and after a `try` block | The closure reads another word of memory for `e.code`. The release program prints a number that is no code of the error, and the dev program stops at a range check | A closure captures the locals it names by reference. "Anonymous functions and closures" in `docs/decisions.md` | severe | open |
| T006 | `matrix_flow_defer_directly_in_defer_refused` | the refusal of a `defer` in a `defer` and of an `undo` in an `undo` | The message stands at the first token after the whole statement, on the next line | A message stands at the statement it refuses. The test expects the inner `defer` and the inner `undo` | minor | open |
| T007 | `matrix_flow_if_let_arms_all_return_dev` | an `if let` on a `?T` whose arm and whose `else` both return, as the last statement of a function | antic refuses the function with `` `run` can reach its end without `return` ``. The same function with an `if let` on a case of a variant compiles | The missing-return rule reads an `if let` as an `if` with an `else`. "Sum types" in `docs/decisions.md`, and "Generics and collections" gives `if let` on a `?T` the form of the other | major | open |
