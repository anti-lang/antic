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
| T008 | `matrix_values_fold_int_to_f32_dev` | `as f32` of an `int` in a constant, where the `int` has more bits than an `f64` holds | The checker rounds the value to an `f64` and then to an `f32`. `1152921573326323713 as f32` folds to 2^60, and the same conversion at run time gives 2^60 + 2^37, the nearest `f32` | Integer to float rounds to nearest. "Numeric types" in `docs/decisions.md`. A constant and a computed value of one expression agree | severe | open |
| T009 | `matrix_values_fold_least_int_from_float_dev` | `as int` of the float that is the least `int`, in a constant | antic refuses `-9223372036854775808.0 as int` with `` does not fit `int` ``. The same conversion at run time gives the least `int` | A constant conversion from a float is refused outside the range of the integer type, and the least `int` is inside it. "Numeric types" in `docs/decisions.md` | major | open |
| T010 | `matrix_values_as_enum_in_constant_refused` | `as` from a number to an enum in a constant | antic refuses `2 as Kind` with `a pointer conversion is not a constant expression`. No pointer stands in the expression | A message names what it refuses. No document says whether the conversion folds, so the test expects the refusal with `a conversion to an enum` | minor | open |
| T011 | `matrix_values_as_negative_enum_dev` | `as` from an enum that holds a negative value to a wider integer and to a float | At run time `Sign.Minus as int` is 4294967295 and `Level.Low as int` is 65236: the value is extended with zeros. In a constant `Sign.Minus as f64` folds to 1.8e19. The fold to `int` gives -1 | An enum converts as its underlying type does, and `c_int` and `i16` are signed. "Enums" in `docs/anti-syntax-overview.md` and the entry on the dev-mode check of an enum under "Nullable pointers" in `docs/decisions.md` | severe | open |
| T012 | `matrix_values_const_optional_dev` | a `const` of a `?T` of a value, alone and as a part of a struct, a tuple or an array | The checker accepts the constant, and antic stops with a segmentation fault when it lowers a function that reads it | A crash of antic is a defect whatever the documents say. `?T` stands before any type, under "Generics and collections" in `docs/decisions.md` | major | open |
| T013 | `matrix_values_array_repeat_widens_dev` | `[v; n]` whose context gives the element another type than `v` has: a `?int`, a `?*int`, a pointer to an interface | antic refuses `let xs: [3]?int = [5; 3];` with `` expected `[3]?int`, found `[3]int` ``. `[5, 5, 5]` in the same place compiles | A T passes where a `?T` is expected, "Generics and collections" in `docs/decisions.md`, and `&c` converts to a pointer to its interface, "Interfaces" in `docs/anti-syntax-overview.md` | major | open |
| T014 | `matrix_values_const_fn_refused` | a `const` whose value is the name of a function | antic refuses it with `a variable is not a constant expression`. The name is a function | A message names what it refuses. The test expects `a function is not a constant expression` | minor | open |
| T015 | `matrix_values_is_after_error_refused` | `v is Shape.Circle` on a value that has an error already | After `` unknown name `missing` `` antic adds `` cannot find module `Shape` ``, and the program names no module | One mistake gives one message. `Shape.Circle` after `is` names a case of a variant, under "Sum types" in `docs/decisions.md` | minor | open |
| T016 | `matrix_values_index_past_end_refused` | a constant index past the end of a fixed array | `xs[5]` on a `[3]int` compiles. A dev build stops at the index, and a release build reads memory outside the array | A constant index past the end of a fixed array is refused. Decision 24 of "Decisions of 2026-10-11" in `docs/decisions.md` | major | open |
| T017 | `check_slice_range` | `xs[lo..hi]` past the length of a fixed array, in dev mode | A dev build makes the slice and runs on. No check reads the bounds | `xs[lo..hi]` is checked against the length in dev mode, as an index is. Decision 24 of "Decisions of 2026-10-11" in `docs/decisions.md` | major | open |
| T018 | `matrix_values_class_constant_dev` | a constant of a class, `Limits.MAX`, in another constant and as the length of an array | antic refuses both with `this field is not a constant expression`. A `let` and the default of a field take `Limits.MAX` | `const NAME: T = e;` declares a constant of the class, reached as `Name.NAME`. "Classes" in `docs/anti-object-model.md`, and "Variables and constants" in `docs/anti-syntax-overview.md` | major | open |
| T019 | `matrix_values_class_constant_as_default_dev` | a constant of a class as the default of a field | The checker accepts `pub used: int = Limits.MAX,`, and antic stops with a segmentation fault when it lowers a literal that omits the field | A crash of antic is a defect whatever the documents say. A default is a constant expression, under "Classes" in `docs/anti-object-model.md` | major | open |
