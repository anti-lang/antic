# The summary of the test run

The last step of the test run, on 2026-10-13, on `main`. It reviewed the
tests the other steps wrote and wrote none of the suite itself. No file
under `src/` changed. The lists below are longer than two pages, since the
task asks for every finding and every unstated rule in one place.

## What the review checked

The matrix of `docs/notes/test-matrix.md`:

- The note holds 3797 marked cells, 2313 of contexts and 1484 of kinds.
  Every one names a source that `git ls-files` holds.
- The 66 rows of its other three tables and its prose name tests and
  finding ids. Each test exists, and each id has a row.
- 25 cells and 8 kinds are open, and each states why it cannot exist.
  Refusals of the matrix hold a channel of a slice, of a pointer and of a
  function. The review tried `keep` and `concurrent` in a field, a result
  and an alias, and antic refuses each.
- One open cell could exist: a `return` of a value in a `tests` block. An
  anonymous function of a test returns one. It is held now.
- The table of the areas said 59 held cells for classes, and its rows hold
  58.

Forty cells drawn at random, five to eight per area, each read in its
source:

- 39 hold the construct in the place the cell names. For 18 of them the
  review computed the expected lines by hand from the rules, and all agree.
- One was wrong: `here` as an argument in a library module named
  `modules/defaults/com/example/greeter`, which holds `here` as a default
  alone. Two cells beside it had the same mistake. All three now name a
  source that passes `here` to a function.
- One is weak: `for x in e` over a `str` names `programs/for_patterns`,
  where the `str` is a part of a tuple. `programs/pattern_walk` walks a
  slice of `str`.
- The cells that name a source older than the run were read from the trees
  by a script. The three wrong cells are of that kind.
- `matrix/values_compound_kinds` has 5211 lines and prints 1697. Every line
  names its operator and its type, as the rule of the matrix asks.

The known failures:

- Each of the 381 tests of `tests/known-failures.txt` ran alone, one after
  another, and each failed. `build/drive/logs/kf/` holds one log per test.
- The review read each output against the row of its finding. 380 fail for
  the defect of their row.
- One named another row. `matrix_classes_call_as_the_default_of_a_field_refused`
  expects a refusal and stood under T019, whose row holds a crash on a
  constant of a class. T028 holds the missing refusal, and the line names
  it now.
- The row of T106 said that a plain call as the default of a field is
  refused. `antic --front-end` takes it with status 0. The row says so now.
- The debugger gave the place of nine crashes of antic. The tests of T012
  stop at one line, and so do the two of T106. The test that moved to T028
  stops where T019 stops.

The grades:

- 119 rows follow the rule of the grades as written.
- Five rows were `major` and describe a wrong result or a memory error of
  a program antic built: T017, T020, T044, T053 and T093. They are `severe`
  now. Two small programs showed T020 and T044 again: an enum value of 0
  for `Low = five()`, and 40 where no `yield` ran.
- The header of `docs/audit/tests-findings.md` states the reading the review
  applied: a finding that two grades describe takes the higher one.
- T043 stays `minor`. The errors step graded it as the position of a
  message, although the origin is a value of the program.

## What the step changed

- `tests/anti-test/com/example/flow.anti` gains the test
  `return_value_in_tests` and the fixture `fix_apply`.
- `docs/notes/test-matrix.md`: four cells, the counts of the areas, and the
  counts of the two guards after the last step.
- `docs/audit/tests-findings.md`: five grades, the rows of T028 and T106,
  and the sentence on two grades.
- `tests/known-failures.txt`: one line names T028 in place of T019.
- `CLAUDE.md`: the suite times of this step.

## Counts of tests

| Count | Before the run, 2026-10-11 | After the run |
|---|---|---|
| Tests of the host suite | 1701 | 4650 |
| Expected failures among them | 0 | 381 |
| Tests of the ASan and of the UBSan suite | 1700 | 4648 |
| `.anti` sources under `tests/` | 710 | 2582 |
| Sources of `tests/matrix/` | 0 | 1638 |
| Findings | 0 | 124, all open |
| Messages a test expects, of 748 | 581 | 731 |
| Options and commands a test passes, of 134 | 126 | 134 |

The matrix holds 343 programs, three tests each, and 1295 refusals. The
counts of the messages and the options before are those of 2026-10-12,
when the two guards were built.

## Coverage

`./c coverage` ran in three parts, in 392 s, 180 s and 114 s, and every
part passed. `build/drive/logs/sum-cov-1.log` to `sum-cov-3.log` hold them.
The baseline is `docs/reports/2026-10-12-tests-coverage-baseline.md`, at
1704 tests.

| Directory | Lines | Not reached before | Reached before | Not reached after | Reached after |
|---|---|---|---|---|---|
| `src/antic/` | 58715 | 4206 | 92.84% | 1151 | 98.04% |
| `src/anti/` | 12727 | 1331 | 89.54% | 451 | 96.46% |
| `src/rt/` | 5300 | 1371 | 74.13% | 1367 | 74.21% |
| All three | 76742 | 6908 | 91.00% | 2969 | 96.13% |

| Directory | Regions | Not reached before | Reached before | Not reached after | Reached after |
|---|---|---|---|---|---|
| `src/antic/` | 50887 | 3650 | 92.83% | 1068 | 97.90% |
| `src/anti/` | 10956 | 1109 | 89.88% | 368 | 96.64% |
| `src/rt/` | 5076 | 1239 | 75.59% | 1237 | 75.63% |
| All three | 66919 | 5998 | 91.04% | 2673 | 96.01% |

| Directory | Branches | Not reached before | Reached before | Not reached after | Reached after |
|---|---|---|---|---|---|
| `src/antic/` | 34849 | 5509 | 84.19% | 3119 | 91.05% |
| `src/anti/` | 6797 | 1555 | 77.12% | 906 | 86.67% |
| `src/rt/` | 3089 | 1004 | 67.50% | 1002 | 67.56% |
| All three | 44735 | 8068 | 81.96% | 5027 | 88.76% |

Of the 3575 functions, 74 were not reached before and 65 are not reached
now: one of `src/antic/`, one of `src/anti/` and 63 of `src/rt/`. The row
of `src/rt/` holds the files that antic, anti and the unit tests compile.
The runtime inside an Anti program is not instrumented, so the tests of
programs add nothing to it.

## The defects, ordered for a fix run

124 findings, all open: 35 severe, 60 major and 29 minor. Each line holds
the id and one sentence, and `docs/audit/tests-findings.md` holds the row.

### Severe, 35 findings

Flow:

- T005. A closure in a handler reads another word for the error the handler
  binds.

Values:

- T008. A constant `int` as `f32` is rounded twice and differs from the
  computed value.
- T011. A negative value of an enum converts as an unsigned number.
- T017. A dev build does not check the bounds of `xs[lo..hi]`.
- T122. A constant of `c_wchar` computed from `size_of` folds without a sign
  where `wchar_t` is signed.

Types:

- T020. A call compiles where a declaration takes a constant expression, and
  the value of the enum is 0.
- T021. A value of an enum computed from size_of compiles, and the value is
  0.
- T022. An atomic field or a Mutex that crosses a multiple of 16 in a packed
  class stops the program.

Ownership:

- T032. `alloc(T, n)` gives an empty block for a count whose size passes
  2^64.
- T033. A release build takes an `alloc(T, n)` that fails as memory.
- T034. `--memory-checks` stops the program at an `alloc(T, n)` that fails.
- T037. `return` of a field, of `*p` or of an element that owns something is
  not refused.
- T038. `yield` of an existing value that owns something is not refused.
- T039. A fresh owning argument of a `dispatch` is never torn down.
- T040. A local moved into an `own` parameter of a `dispatch` stops the
  program.
- T041. A call result read by `is`, by a test for `none` or by an index
  alone is never torn down.
- T042. A failing function that gives an owning variant traps in a release
  build.

Errors:

- T044. A handler of a value that can reach its end is not refused, and the
  value is what stood in memory.
- T045. `try` on a call that gives a `?T` and cannot fail is not refused.
- T047. The bare error a handler binds is stored by `=`, by `yield` and in a
  literal.
- T049. `??` in a generic over a `?*int` gives wrong values.
- T052. A derived `construct` that fails leaves what its base took.

Threads:

- T053. A second `join` of one job compiles and reads the freed job.
- T054. A second `join` that the checker cannot see hangs in a dev build.
- T055. Every job of a `dispatch` in a loop reads the arguments of the last
  turn.
- T060. The atomic `add` and `sub` on a `bool` and the four on a pointer
  compile.

Classes:

- T072. A constant and a static field of a generic class are one for every
  copy.
- T081. The default `equals`, `hash` and `==` compare an `own` pointer by
  its address.
- T082. `dup` hands a replaced `copy` an object without its table.
- T083. `==` on a pointer to an interface and a pointer to the object or its
  base compares the addresses.
- T112. A function of a copy of a generic class named as a value gives the
  address of another function.
- T114. The `changed` hook gets the record of the first field of the class
  for a write of any of its fields.

Modules, C and plugins:

- T092. A local object without `destruct` in a plugin stays counted as
  alive, and `unload` refuses.
- T093. A plugin cannot load when its host imports a module of the standard
  library and does not use it.

The `anti` tool:

- T117. An enum with a value above the signed 64-bit range is bound with the
  base `c_int`, and its struct lays out wrong.

### Major, 60 findings

Flow:

- T001. `return`, `break`, `continue`, `fail` and `try` inside a `defer` or
  an `undo` crash antic or compile.
- T002. antic crashes on a `yield` inside a `defer` of a handler.
- T003. antic crashes on a failing call that a `defer` in a `try` block
  holds.
- T004. A closure reads the name of an `if let` as a value that may be
  `none`.
- T007. An `if let` on a `?T` whose arm and `else` return does not end the
  path of its function.
- T103. antic never ends on an `if` whose statement stands at the last level
  of the depth limit.

Values:

- T009. The least `int` as a float is refused in a constant conversion to
  `int`.
- T012. antic crashes on a constant of an optional type.
- T013. An array repeat does not take the element type of its context.
- T016. A constant index past the end of a fixed array is not refused.
- T018. A constant of a class is refused in a constant expression.
- T019. antic crashes on a constant of a class as the default of a field.
- T106. antic crashes on a field default that is a class literal with a call
  in a field.
- T111. `==` of a `fn` and a `?fn` of one type is refused, where a `*T` and
  a `?*T` compare.

Types:

- T023. antic crashes on a field whose type is a variant without fields.
- T107. A length `-(-N)` of a generic struct stays `[--3]int` in its copy
  and takes no `[3]int`.

Ownership:

- T028. `??` and a call compile in a constant expression, and a call as the
  default of a field crashes antic.
- T029. antic cannot read back a library file whose generic changes a
  `static atomic` field.
- T030. A class built with arguments under the name of its module is refused
  as a type.
- T031. `dup` of an array whose elements own something is refused.
- T036. `if let` and `let ... else` refuse the result of `as?` as a `*T`.

Errors:

- T048. A `?int` is refused where a `??int` is expected.
- T050. `try` before `alloc T(args)` is refused.

Threads:

- T056. antic crashes on a read of a constant of a struct that holds a
  Mutex.
- T057. The default of a static field that no code reads is not checked.
- T058. A channel of a type parameter takes a pointer.
- T059. An atomic field of a type no atomic takes compiles, crashes antic or
  stops opt.
- T061. The library file of a generic function that holds a `dispatch`
  cannot be read.
- T062. A public function of a thread-safe class gives out a pointer into
  its fields.

Classes:

- T064. A class with an `abstract fn` that is not `abstract` compiles.
- T065. `mutable` on a field of a plain class compiles.
- T066. `==` and `!=` on pointers of two unrelated classes compile.
- T067. A class that replaces one of `equals` and `hash` without the other
  compiles.
- T068. `self.super.destruct()` compiles and calls nothing.
- T069. A field after a constant, a static field or a function of a class
  body is refused.
- T073. `is` and `as` from a pointer to an interface to the abstract base of
  the object are refused.
- T074. A literal as the second operand of an `operator fn` that takes
  another type is refused.
- T075. A static function, a static field and a constant without a marker
  are reached from outside their class.
- T076. A public static field and a public constant of a class of another
  module are refused.
- T078. An abstract class compiles as the type of a value that is held in
  place.
- T079. The address of a field of a singleton that is fixed after creation
  lets a write through.
- T080. A literal of a type parameter ends in an error of the back end
  without a position.
- T084. A `concrete fn` in a nested class cannot be qualified by a type of
  the same class body.
- T104. A program with an injected `log.Logger` that it never calls fails at
  the link.
- T105. A class whose tuple field holds a union gets a default `==`.

Modules, C and plugins:

- T085. The library file of a class with a field `?int = none` cannot be
  read.
- T086. A protected static function of a class of a library is called from
  outside the class.
- T089. A `str` parameter of an `extern fn` is not refused.
- T090. A write of `count` of a `List` outside its module is not refused.
- T091. A literal of a case of a generic variant under the name of a module
  does not parse.
- T094. A constant of another module is refused as the argument of `N: int`.
- T115. A literal of a library class omits a field whose default names a
  function and is refused.
- T116. The header of an export function that never returns holds
  `_Noreturn`, which is no C++.
- T121. A plugin that converts an `f16` below x86-64-v3 does not link
  against a host that converts none.

The `anti` tool:

- T025. anti fmt moves the functions after a type alias of a function type.
- T099. `dist/` of a Linux library of `--soname` holds `libshapes.so` alone,
  without the file its soname names.
- T118. The enumerators of an enum without a name inside a struct are no
  constants of the binding.
- T119. `anti bind` writes a float with the exponent `e+20`, which does not
  lex, and ends with 1.
- T120. A macro that casts a negative number is skipped as no constant.
- T124. A link of a doc comment whose text `anti fmt` wraps over two lines
  is written as text.

### Minor, 29 findings

Flow:

- T006. The refusal of a `defer` in a `defer` stands at the next statement.
- T097. A function whose refused `switch` returns in every arm gets a second
  message.
- T100. The `}` inside a refused statement ends the function for the parser,
  which adds `expected an item`.

Values:

- T010. The refusal of a number as an enum in a constant names a pointer
  conversion.
- T014. The refusal of a function in a constant names a variable.
- T015. `is` on a value with an error adds a message about a module.
- T109. The default hash over a part whose `operator fn hash` is refused
  gets a second message at the call.

Types:

- T024. A switch on a pointer to a variant gives three messages for one
  mistake.
- T026. is with a case on a value that is no variant names a module that is
  not there.
- T027. An unknown value of an enum is refused as a function.
- T113. A receiver with two paths to the interface of a constraint gets a
  second message in the copy.

Ownership:

- T035. `dup` of a `?T` that may be `none` gives two messages.
- T108. The refusal of a copy names the element of a tuple `_0`, where the
  program writes `.0`.

Errors:

- T043. The origin of a `fail` in a generic of a library names the client.
- T046. The refusal of `try` on a `?T` name names neither `let ... else` nor
  `??`.
- T051. An unhandled `alloc T(args)` gets a second message.

Threads:

- T063. A pair of locks in opposite orders is reported once per order.

Classes:

- T070. A message of the class checker differs from the text the object
  model gives.
- T071. One mistake of a class declaration gets two messages.
- T077. The message for a member of a class reached the wrong way names
  another kind or denies the member.
- T096. A use of a declaration the checker refused gets a second message.
- T101. The parser reads the rest of a class body after a refused member as
  items of the module.
- T102. `pub` before three more words and `class` in a class body gets
  `expected identifier`.

Modules, C and plugins:

- T087. One mistake at the boundary of a module gets two messages.
- T088. The driver reports a cycle of imports without the position of the
  import.
- T095. A message on a copy of a library generic names a line of the library
  in the file of the client.
- T110. The refusal of an aligned `export union` whose first field is a
  bitfield calls the union a struct.

The `anti` tool:

- T098. A `static` function without a body gets a second warning, which
  names its result.
- T123. `anti doc` prints the message of a source that does not parse twice.

Findings that may share a fix:

- T012 and T056 stop antic at one line, `const_tree` of
  `src/antic/lower_place.c`: a constant whose type has fields and whose
  value is no aggregate.
- T019, T028 and T106 hold a default of a field that is no literal. The
  checker refuses no call there, and lowering stops at three places.
- T029, T061 and T085 are library files that antic writes and cannot read.
- T075 and T086 are static members that pass the check of visibility. The
  review did not read the checker to see whether one line holds both.
- Fourteen findings are a second message for one mistake: T015, T024,
  T035, T051, T071, T087, T096, T097, T098, T100, T101, T109, T113 and T123.

## Rules the documents do not state

The step reports of the run collected these. Each line is what antic does
today and what a test holds, unless it says that no test holds it.

Flow:

- A `break` or a `continue` whose loop stands inside the same `defer` or
  `undo` compiles and runs. Decision 19 does not name the case.
- `yield` inside a `defer` is in no document. The test of T002 expects a
  refusal.
- A `switch` whose arms all return and a `while true` do not end the path
  of a function. The `else` of a `let` that ends with such a `switch` is
  refused.
- A deferred statement that holds a closure runs the closure at each exit.
- An `undo` in a function that cannot fail, in the block of a `defer` and
  in the block of another `undo` compiles and never runs.
- `yield v` computes `v` before the deferred statements of the handler run.
- A `defer` whose block calls a function that returns `never` is taken,
  and the function ends inside the defer.
- A `for` whose low end is above its high end runs no turn, with `by -1`
  as well.
- `switch` on a `char`, a `bool` or a value of a type parameter is refused.
- A `switch` on an integer takes the arm `size_of(i64)` beside the arm `8`,
  and the first of the two runs for 8.
- `for x in &c` over a parameter with `Iterable<int>` is refused, since the
  value of such a walk is a copy.
- A source that ends inside a block gets `` expected `}` `` twice.

Errors:

- A `worker fn` that may fail is declared and called directly with a
  handler. `dispatch` and `parallel` refuse it, and the message counts the
  out pointer as an argument.
- `yield;` in the handler of a `try` block is refused with `` `yield`
  stands in a `catch` handler ``, although it stands in one.
- A `yield` in the handler of a `try` block inside the handler of a `let`
  gives the value of the `let`.
- `try f()` inside a `try` block forwards to the caller, not to the
  handler of the block.
- `fail "text"` takes a literal alone. A `str` in a variable and an
  `f"..."` are refused.
- A `fail` in an anonymous function does not answer the warning
  `never-fails` of the function around it.
- The function of an origin is `m.f.0` for an anonymous function,
  `m.f<int>` for a copy and `m.Outer.Inner.f` for a nested type.
- The `NoneDereference` of a guard has code 1 and no origin. Its message is
  `the pointer is none` for a `?T` of a value as well.
- `catch fatal` in a worker ends the whole program, and no `defer` runs.
- A `let` takes a lent pointer, and a `yield` into that `let` refuses it.
- A function that may fail and gives a function that cannot fail needs a
  `type` alias, since `may fail` after a function type belongs to it.
- The handler of a call statement whose result is dropped may reach its
  end.
- No rule says what frees the part a base took when the `construct` of
  the class below fails. T052 holds the leak.

Values and constants:

- A narrowing `as` of a constant keeps the low bits without a message:
  `300 as u8` is 44. The same conversion at run time stops a dev build.
- `0xD800 as u32 as char` in a constant gives a surrogate without a check.
- A float divided by zero in a constant gives an infinity or a NaN.
- A sum of two constants past the range of its type wraps in the constant.
- A constant index below zero on a fixed array, `xs[-1]`, compiles.
  Decision 24 names an index past the end.
- The least value divided by -1 and unary `-` of the least value give the
  least value in a dev build too, with no check.
- `<<%` by a negative count gives 0.
- `.len` of a constant `str` is no constant expression, and `chan int(1)`
  in a constant is refused as a call.
- `==` on two slices and on two unions is refused. `operator fn eq(a:
  []int, b: []int)` is taken where it is declared and changes nothing.
- `==` on a struct that holds a simd struct is refused.
- `>`, `<=` and `>=` on a struct come from its `operator fn lt`.
- A compound assignment on a simd struct and on a value of a type
  parameter is refused, as decision 27 refuses it for `operator fn add`.
- An array literal, an array repeat and a local take a lent pointer. A
  field, a tuple and a slice literal refuse one.
- A concurrent class with atomic fields alone is copied by `=`.
- A default of a parameter may be a literal that holds a class value. A
  case of a variant there is refused with `a variant is not a constant
  expression`.
- `(r, f) = e;` takes a name `r` whose address the program took.
- `as c_wchar` of an `i64` and of a `u16` is taken, and `as` back to each.
- A length computed from `size_of` is refused in the field of a class and
  in the default of one. The field of a struct that the class holds takes
  it.
- `struct Loop { a: [size_of(Loop) + 1]int }` is refused as a struct that
  contains itself.
- A default `[0; N]` over a constant parameter of the class is refused,
  and the message names `size_of`.
- `-(-size_of(H))` in a length prints as `--size_of(H)`.
- No rule says whether a constant may hold a Mutex. T056 expects the value
  of the literal.

Types:

- A value past the width of a bitfield keeps its low bits. A literal, a
  write and a compound assignment give no message and no check in dev
  mode.
- `+%=` and `+|=` on a bitfield compute in the type of the field and then
  keep the low bits, so a saturated value is cut again.
- `alloc(T, n)` gives the memory of `malloc` for a type with an `align(N)`
  above 16, so the address may not be aligned. No test holds it.
- `align(4294967296)` compiles, and the type has that size.
- An enum without values compiles, with the size of `c_int`.
- Two values of an enum may share a number, and a `switch` takes one arm
  for both.
- A value of an enum cannot name an earlier value of its own enum.
- `as` converts an enum to another enum, and a float to an enum.
- `Shape.Empty { }` and a case declared as `A { }` compile.
- No arm names a value of the `tag` of a variant, so a `switch` on it
  takes `else` alone. `v.tag as int` gives the number.
- `as ?T` reads as the nullable downcast `as? T`, and `anti fmt` writes it
  so. No `as` gives an optional value.
- `as` to the type a value has already is refused for a tuple, a slice
  and a channel.
- `packed` before `enum`, `simd struct` and `synchronized class` is
  refused with `expected an item`. A union and a `type` alias take no type
  parameters.
- A field default in a union is refused with the message of a struct.
- A `simd struct` of one `f64` lane compiles, and a simd struct takes
  `char` lanes.
- `lent` before a result is read before `*`, `?` and `?*` alone. The
  checker refuses `-> lent ?int` and the parser `-> lent int`.
- `operator fn value(self) -> lent ?*int` is taken.

Generics:

- A generic call whose argument has another shape than the parameter gets
  the message that nothing in the arguments gives `T`.
- `Pair { left: 1 }` of a generic struct inside a generic function is
  refused, since nothing gives its type arguments.
- `operator fn eq<T, U>` whose `U` no operand names is taken where it is
  declared. `==` is then refused, and the message asks for `U` after the
  name.
- A generic function that names a deeper copy of itself passes `antic
  --front-end`, and `-S` refuses it. A generic struct is refused by both.
- A receiver with two paths to the interface of a constraint is refused in
  the body of the generic. The call that names the copy gets no message.

Ownership:

- `dup` of a number, a `str`, a slice, a plain array, a union, an enum and
  a channel is refused outside a generic. `dup(*p)` in a generic copies
  each.
- `dup` of a `?T` before its test is refused, and after it gives a `T`.
- `dup` of a class value runs no `construct`.
- `delete` tears down what an object owns from its last field to its
  first.
- `=` to an `own` pointer or an `own` slice field leaves the old memory
  alone. The program deletes it first.
- `delete(c)` tears down no value the channel still holds, and nothing
  tears down the slot that `recv` fills. The program calls `destroy` on
  what it received.
- A class value with an `own` pointer is an argument of a worker and an
  element of a channel, where a plain pointer is refused.
- A closure that a `dispatch` passes to its worker may `delete`. The walk
  of a worker follows the calls by name.
- `let xs = [p, p];` of a lent pointer makes an array of lent pointers.
- An array literal takes no owning local, where a tuple, a struct and a
  class literal move one.
- A local passed to a value parameter that is not `own` is a copy of its
  bytes, which the function does not tear down.
- An `own` parameter of a pointer type that a generic does not hand on is
  not deleted.
- An `own` parameter stored through `*slot(to) = b` moves, and the message
  on a later use names no place.
- `delete(p)` and `destroy(p)` of a parameter `p: *T` without `own` are
  taken, and so is `destroy(&Config.get().part)` on a part of a singleton.
- `none` into an `own` field of type `?fn(...)`, as a default and with
  `=`, is refused with the message that names `dup`.
- `f ?? g` of two function parameters is neither kept nor `concurrent`.

Threads:

- A worker of another module is named through a direct import. `m.f`
  after the arrow is refused.
- `by 0` and a count below zero give the chunk count of the machine.
- A `dispatch` writes every argument out. The default value of a
  parameter of the worker is not filled in.
- An atomic `add` wraps in a dev build as in release.
- A `snapshot fn` copies an atomic local, and its operations change the
  copy.
- An atomic field takes a `char`, and an atomic `bool` takes `and` and
  `or`.
- A pointer to a guarded field that was taken inside its `sync` is used
  after the block.
- `sync m` in an anonymous function inside `sync m` compiles, and so does
  `sync` on the same Mutex through a pointer. Both wait for themselves.
- A public function of a thread-safe class stores a pointer into its
  fields through an out parameter and into another object.
- A fresh value of a synchronized class, of a concurrent class and of a
  Mutex moves into a channel.
- A job stands in a tuple and in an array, and a closure joins one it
  captured.
- An `own fn` field passes to a `concurrent` parameter.
- A `chan f16` carries an `f16`, although no parameter takes one.
- The default `==` and the default hash of a struct that holds a Mutex
  pass over the lock and take the other fields.

Classes:

- A nested class and the class around it reach no private member of each
  other.
- A class below reaches a protected member on another object of its base.
- `T(args)` runs a `construct` without `pub` anywhere in its module.
- `==` on values of a class that replaces `equals` calls its `equals`.
- An object equals an object of its own class alone, never one of a class
  below with the same fields.
- `equals` passes over an `operator fn eq` of its module that gives an
  `int`, or that takes by value a class with a Mutex. It compares the
  fields then. Such an operator is taken where it is declared.
- `equals` of a copy of a generic class passes over an operator whose
  constraint the argument does not meet.
- A part of a concurrent class whose body declares `operator fn eq`
  compares through it in the default `==` of the class that holds it.
- `==` of a struct whose part has `operator fn eq(a: Key, b: int)` is
  refused at the `==` of the struct.
- A singleton whose `construct` takes arguments is built from its
  defaults.
- `dup(Config.get())` and `*Config.get()` copy a singleton.
- A singleton inherits a singleton, and a plain class inherits one.
- A singleton class that declares `get` itself is taken, and
  `Config.get()` calls it.
- A class with an `inject` field has no `T { }` default as an inline
  field.
- The `construct` of a base without arguments runs before a `construct`
  with arguments. When that one fails, no `destruct` of the base runs.
- A local of a class that replaces `destroyed`, has no `destruct` and owns
  nothing is not torn down, so the hook does not run.
- Two bodies of one name, each qualified by another class of the chain,
  are refused as declaring the name twice.
- An abstract function without `self` is taken, and a `concrete fn` fills
  it without `self`.
- A class may have a name in small letters, as `singleton class console`.
- A provider whose path is the `get` of a singleton and a function of a
  module below it is refused as naming more than one function.
- A write to a `transient` field of a traced class calls no `changed`
  hook under `--trace writes`.

Modules and C:

- A plain and a direct import of one module in one file are refused. An
  alias with a direct import compiles.
- A clause of the whole file stands among the imports as well.
- An expression reaches a module by its last segment or its alias, never
  by its whole path.
- A module that names a type of a module it does not import is refused,
  although it holds values of that type.
- Two modules of a project that import each other are refused with
  `` cannot find module `com.example.b` ``, which names a module that is
  there.
- An `extern fn` takes a slice, a `char`, a tuple, an optional value and
  an `own` parameter.
- Two modules declare one `extern fn` with two signatures.
- An `f32` and a `c_long` are no variadic arguments, and a `c_size_t` is.
- `export` stands before an `operator fn`, and an `export fn` takes a
  function parameter without `keep`. No test holds their header.
- An export constant named as a macro of `<stdint.h>`, as `SIZE_MAX`, is
  written as a `#define` of that name.
- `--lib static` builds a library of a module without an exported item.
- `--lib shared --bundle-runtime`, `--lib static --no-runtime` and
  `--lib static --dev` are taken without a message.
- A plugin that uses a module its host does not import fails to load with
  the message of the loader.
- `lib.instance(Interface)` gives `none` for a class the loader cannot
  build, and no function of `anti.plugin` gives the reason.
- The plugin index refuses a library name with both kinds of quote.

Messages:

- A message names `u64` where the source says `uint`, and `int`, `byte`
  and `float` where it says `i64`, `u8` and `f64`.
- `Color.Red()` of an enum is refused with `` `Color` is not a function ``,
  which names the type of the value.
- `destroy(node)` on a value says `` `destroy` needs a class pointer ``,
  though it takes a pointer to a value of any type.
- A private function of a class of a library is reported as missing, and
  a private field as private.
- The check of exponential time refuses `(a+\b)+` and `(a+(?=b))+`.

The driver of antic:

- The usage names no `--help`, `--no-reflect`, `--asserts`, `--no-asserts`,
  `--checks`, `--no-checks`, `--no-hooks`, `--trace` and `--no-trace`.
  antic takes all nine.
- `antic --help` among other arguments prints the usage and ends with 0.
- An option that takes a value as the last word gives `needs a value`. A
  wrong value of `--lib` ends antic with 2.
- `--checks --no-checks` drops the checks, and the last of the two decides.
  No test holds it.
- `--dev --no-asserts` drops the call and keeps the text of the assertion
  as a constant. No test holds it.
- A library of the language under `-I` is read before the one of the
  runtime archive.
- `antic -I . -c com/example/lib.anti` gives the module the path `lib`.
  The root and the source agree when both are absolute.
- A search root that holds the library file of the module antic compiles
  is refused.
- `-o` of a program in a directory that is not there names `<output>.ll`
  in its message.
- `--soname` beside `--lib static` changes no byte of the archive.
- A library of macOS without `--soname` has the path of its output as its
  install name and 0.0.0 as both versions.
- The link of a Windows library writes its time into the file, so two
  links of one input differ in two bytes.
- `antic --tests --dump-ast` prints no function of a `tests` or a
  `fixtures` block.
- Under `--linker platform`, a Linux program that hosts plugins links with
  `--export-dynamic`. The report of 2026-10-13 on the rest of antic holds
  the Windows libraries of that mode.

`anti build`, `run` and `check`:

- `anti --help` with a second word is refused with the usage and 2. An
  option that lacks its value gives the whole usage and no line of its
  own.
- `anti build` exits with 1 and writes nothing when `dist` is a file.
  `anti sdk import` does the same when `--sysroot` names a file.
- `anti build` takes `--soname` and `--bundle-runtime` without `--lib` and
  ends with 0.
- `anti build --target all --cpu v1` builds linux-x86_64, refuses
  linux-arm64 and ends with 1. What it built before stays.
- `anti run` takes every option of `build`. It ignores `--target`, and
  `--lib static` fails for the `main` of the program. No test holds
  either.
- `anti run` of a library project builds it and then says there is nothing
  to run. `anti doc` without a module exits with 2, and `anti check` with
  1.
- `anti check -I <dir>` finds an imported module as a library file under
  the root, or as a source the command names, and no other source.
- anti writes one line per layer of a failure, and the lines of `curl`,
  `tar` and `clang` pass through before its own.
- A program anti cannot start is reported as `antic: cannot run clang`,
  with the name antic.
- A dependency whose name is no package name passes the manifest reader,
  and the refusal comes when its index is read.
- A cycle of path dependencies is refused eight projects deep.
- A path dependency whose path holds a control byte is built before the
  lock file refuses the path.
- A lock file that is no TOML, or whose version or repository no longer
  answers the manifest, is passed over without a message.
- A library project without the module whose path is the name of the
  package builds, and its header holds the functions of every module.
- `anti run --memory-checks` writes a frame line as it went in when its
  module is no program of Anti.

`anti bind`:

- A type of an API description that does not parse gets two warnings.
- A part of an API description that is `null` is a part that is not
  there. A function without its name or its result, a struct without its
  name and a field without its type are passed over without a warning.
- A struct that lost a field binds with a layout other than that of C,
  and no warning says so.
- `int (*)()` binds as `?fn() -> c_int`, where C leaves the parameters
  open.
- A macro with `!` is skipped, and so is a text with an escape other than
  the six of Anti. `docs/notes/bind.md` gives no list of either.
- A compound literal takes a comma after its last value. Two macros that
  name each other are skipped, and `#undef` drops a macro.
- A struct without a name that is the type of a field `plain` of `Outer`
  is bound as `Outer_plain`. A signed bitfield of one bit is `c_int : 1`.
- A define of the kind `FLOAT` whose value is a whole number is written
  with `.0`.

`anti doc`, `fmt`, `symbols` and `license`:

- `anti doc` links a name with its whole module path and the name of an
  import, and leaves `forms.Level` of the module's own last segment as
  text. It writes `u8` as `byte`, `f64` as `float` and `c_int` as `i32`.
- A fence whose tag is `anti` and a space is an `anti` block. A fence that
  nothing closes holds no block, and `anti check` says nothing of it.
- In the Markdown page of `anti doc --dev`, a code block that never closes
  takes the next heading into the block.
- `anti fmt` writes a space after `]` before `fn`, `chan` and `?`, as
  `[2] fn(int) -> int`.
- `anti fmt` keeps a comma before the first name of a direct import and
  sorts the names after it.
- `anti symbols resolve` answers no frame from an archive that holds a
  map and no debug link.
- An index of a symbols archive may hold a key outside its `module`
  tables, which names nothing.
- A program whose build id holds a letter outside the hex digits is no
  program of Anti for `anti symbols`.
- `anti symbols inventory` finds a program of Linux and one of Windows on
  a Mac, each by the build id in its file.
- `anti license --from-archive` ends its walk at a member whose size holds
  no digit, and reports an archive without a package header.
- `anti license --runtime <archive>` of an archive without
  `licenses/anti_rt.txt` names antic in its message and ends with 0.
- The last line of a notice may lack its newline. `anti license --from`
  then writes the first `source` line onto the end of that line.

Messages that are this run's wording, since no document gives the text:

- The refusals of decision 19 and of `yield` in a `defer`, T001 and T002.
- The refusal and the check of decision 24, T016 and T017.
- The refusals of decisions 20 and 21, T044 to T046, and of decision 23,
  T053.
- T010, T014, T021, T026, T027 and T028, each a message for a constant, an
  enum or a case.
- T038, which expects the refusal of `=` for a `yield`. A move would be as
  safe.
- T059, T060, T065, T066, T080, the reverse of T067 and two tests of T077.
- T058 and T062, a message of antic at a position the step chose.
- T086, T088, T089, T090 and T095, messages and positions at the boundary
  of a module.

## Questions

- The sentence on two grades in `docs/audit/tests-findings.md` is the
  reading of this review. Does it stand, and do the five grades stand?
- T019, T028 and T106 hold one missing refusal and three crashes. One row
  or three for the fix run?
- T043 is `minor` as the position of a message. The origin is a value of
  the program, so the sentence on two grades would make it `severe`.
- The task asks for the sanitizer suites after a change of a C or a CMake
  file. `CLAUDE.md` asks for them before a push with any change outside
  the documents. The step followed `CLAUDE.md`.

## Counts and checks

- Tests: 4650 before and 4650 after, by `ctest --preset host -N`. The new
  test of the `tests` block runs inside `anti_test_flow`.
- Host suite: 4650 of 4650 passed, 381 of them expected failures, in two
  parts of 396 s and 216 s. Two skip, the `mimalloc_environment` tests.
  `build/drive/logs/sum-host-1.log` and `sum-host-2.log` hold the run.
- ASan: 4648 tests in three parts, `-I 1,1150`, `-I 1151,2250` and
  `-I 2251,4648`, in 288 s, 406 s and 313 s. Every part passed, in
  `sum-asan-1.log` to `sum-asan-3.log`.
- UBSan: the same three parts in 245 s, 364 s and 237 s, all passed, in
  `sum-ubsan-1.log` to `sum-ubsan-3.log`.
- The three trees built with zero warnings, in `sum-host-build2.log`,
  `sum-asan-build.log` and `sum-ubsan-build.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no expected
  file of either changed.
- The docs-style checker reports `0 error(s), 0 warning(s)` on the note,
  the findings, `CLAUDE.md` and this report.
- Defects found by this step: none. No id after T124 is taken.

## Not done

- Neither VM ran the suite.
- The review read forty cells of 3797. The other cells that name a source
  older than the run may hold the mistake of the three it found.
- A first run of the known failures ran ten tests at a time. One test
  failed there at the link of a dev object that another run wrote. The
  logs of that run are gone, and the review rests on the second run.

## Proof

`git log --oneline -3` after the push of the commits of the step, before
the commit of this report:

```text
7817d2c0 State the suite times of the summary step in CLAUDE.md
2b3e8969 Grade five findings again and name T028 for a call in a field default
3d975d58 Correct three cells and the counts of the test matrix
```

`git rev-parse HEAD origin/main` after that push:

```text
7817d2c0e9471680ff5abee7568bd35328e40dd8
7817d2c0e9471680ff5abee7568bd35328e40dd8
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds the three outputs after the last push, since
the hash of this commit is not known inside it.
