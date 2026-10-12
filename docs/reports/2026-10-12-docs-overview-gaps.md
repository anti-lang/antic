# The syntax overview against the built language, second reading

Date: 2026-10-12. Branch `docs`, at `a8d6458f`. The first reading is
`docs/reports/2026-10-11-docs-overview-gaps.md`, and
`docs/reports/2026-10-11-docs-overview.md` reports the rewrite that followed
it. This reading compares the rewritten `docs/anti-syntax-overview.md` with
the code. It changes nothing but this report.

## Sources of the comparison

- `src/antic/lexer.c`: the table `kinds`, the list `reserved`, the table
  `string_prefixes` and the doc markers, word by word.
- `parser.c`, `parser_item.c`, `parser_stmt.c` and `parser_expr.c` in full,
  the words of `parser_decl.c`, `parser_type.c` and `parser_clause.c`, and
  the messages of the parser.
- The 563 message texts of the 18 `sema*.c` files, read as the list of the
  rules the checker holds.
- `docs/decisions.md`, with "Decisions of 2026-10-11" in full, and "Small
  things", "Messages" and "Keywords" of `docs/anti-language-additions.md`.
- Every `pub` item of the 35 modules under `src/std/`, by a script, and the
  public members of every class.
- The usage text and the option parser of `src/anti/main.c`, the options
  that `src/antic/main.c` and the driver parse, and the keys of
  `src/rt/conf.c`.
- The 95 rows of `docs/audit/tests-findings.md`, which show where the code
  leaves a rule.
- 124 probes through `antic --front-end`, with antic built from this tree.
  A line below that says "probe" was compiled.

`tests/run_document_examples.cmake` ran over the overview: 55 blocks
compiled, 1 program ran and 9 were skipped as not built.

## Reserved words against the lexer

| List of the overview | The code | Result |
|---|---|---|
| 68 keywords | the `CAT_KEYWORD` rows of `kinds` that name no type | equal, word by word |
| 18 type names | `bool` to `uint` | equal |
| 14 C types | `c_char` to `c_wchar` | equal |
| `thread`, reserved | `reserved` holds `thread` alone | equal |
| `null`, refused | `is_null`, with `` `null` is `none` in Anti `` | equal |
| contextual words | the 27 words of `parser_is_word` | equal |
| tokens beside those of C | the 64 symbols of `kinds` | every named token exists |
| string prefixes `r b br f rf x re` | `string_prefixes` | equal, see below |
| built-ins known by name | the names the checker tests | the list is partial |

The 27 contextual words are `align`, `allow`, `by`, `compatible`,
`concurrent`, `fatal`, `final`, `framework`, `guarded`, `in`, `inject`,
`keep`, `lent`, `link`, `linux`, `may`, `mutable`, `never`, `operator`,
`own`, `packed`, `simd`, `snapshot`, `synchronized`, `trace`, `transient`
and `unchecked`. The overview holds each, three of them as the pairs
`link framework`, `guarded by` and `may fail`.

What the section lacks:

- `string_prefixes` holds `fr` as well, refused with the message
  `` `fr"` is not a prefix, write `rf"` ``.
- The names known by name beyond the listed ones: `to_slice()` of an
  iterator, `x.hash()`, `.len`, `.ptr` and `.tag`, `Mutex.new()` and
  `m.destroy()`, the atomic operations, `Regex.compile` and
  `ByteRegex.compile`, and `Object.deserialize`.
- The methods of `str` and `[]byte` the checker knows: `matches`,
  `find_all`, `replace`, `split`, `patch`, `to_bytes` and `to_text`.
- `lib.instance(Interface)` and `lib.supports(Interface, "f")`, the two
  calls that take a type as an argument.
- The function names with a meaning: `main`, `construct`, `destruct`,
  `setup`, `teardown` and `get` of a singleton.

## Features the overview lacks

Each line names the section it belongs to and the source that holds it.

### Program structure to operators

- Program structure: the block forms of the doc comments, `/** */`,
  `/*! */`, `/*# */` and `/*#! */`. `doc_marker` of the lexer, probe.
- Modules: an import that forms a cycle is refused, and so is a module that
  imports itself and an import written twice. `sema.c`, probe.
- Modules: no variable stands at module level. `let` there is refused with
  `expected an item`, probe. State lives in a `static atomic` field or a
  singleton.
- Types: the conversions `as` takes, from `can_convert` of `sema_expr.c`.
  The table is below.
- Types: an array length is at least 1, and a length from `size_of(T)`
  stands in a struct field or a local alone.
- Types: a struct that holds itself by value is refused, probe.
- Literals: the format specification of `f"..."`. An alignment `<`, `>` or
  `^`, a `0`, a width, `.` and a precision, and one kind of `x X b o e f`,
  in that order.
- Literals: an integer takes `x X b o`, a float takes a precision and
  `e f`, `{{` and `}}` write a brace, and an unknown format is an error.
  "Interpolation" in `docs/decisions.md`, probe.
- Literals: a decimal literal other than 0 does not start with 0, and `_`
  stands between two digits.
- Variables and constants: what a constant holds. A number, a `bool`, a
  `char`, a `str`, an array, a struct and a tuple compile, probe. A class
  is refused.
- Variables and constants: a `const` names its type, `const N = 2;` is
  refused, probe. A second `let` of one name in one block is refused.
- Variables and constants: an atomic local holds an integer, a `bool`, a
  `char` or a pointer. A parameter is assigned as a local is, probe.
- Operators: `is` is absent from the precedence table. It binds as `as`
  does, `cast_links` of `parser_expr.c`.
- Operators: the operand types. `%` and the shifts take integers, `+` takes
  no `str`, and `<` takes numbers, `char` and `str`. Probes.
- Operators: the flags form takes one `+`, `-`, `*`, `<<`, `>>` or unary
  `-` on an integer. `a / b` is refused there, probe.

| From | To | Note |
|---|---|---|
| a number or an enum | a number or an enum | every pair |
| `bool` | an integer | one way |
| `char` | `u32` | and back, no other integer |
| a pointer | a pointer | a view, or a class cast |
| `f32` | `f16` | and back, nothing else |
| a `simd struct` | an array or a struct of the same bytes | and back |

`65 as char` and `'a' as int` are refused, probe.

### Statements to tuples

- Statements: `switch` takes an enum, an integer, a `str` or a variant. A
  `char` and a `bool` are refused, probe.
- Statements: a `switch` on an integer needs no `else`, an arm value is a
  constant expression, and a value named twice is refused. Probes.
- Statements: what stands as a statement. A call, `free`, `delete`,
  `destroy`, `dup`, `join`, a channel operation or an assignment, by
  `expected a call or an assignment`.
- Statements: `if let` takes a variant or a value that may be `none`, a
  `?*T` included, probe. `yield` stands in a `catch` handler alone.
- Loops: a range counts over an integer. `for c in 'a'..'e'` and a `for`
  over a `str` are refused, probes.
- Loops: `for i, x` binds the index and the element of a slice or an array
  alone. The variable of a range is read-only too, probe.
- Functions: a parameter with a default stands after every one without,
  probe.
- Functions: no function and no type is declared inside a function body.
  Probes.
- Functions: a generic function is no value, and a variadic function has no
  function pointer type. Probe.
- Anonymous functions: one stands in the body of a function alone. As the
  default of a field it is refused, probe.
- Tuples: a tuple has two elements or more, and `t.0.1` reads an element
  of an element, probe. "Tuples" in `docs/decisions.md`.

### Structs to interfaces

- Structs: a literal names every field, since a struct field has no
  default. `the literal of P misses the field y`, probe.
- Enums: the body takes constants and functions without `self` after its
  values, `parser_members_of`. `K.label(k)` and `K.N` reach them, and
  `k.label()` is refused. Probe.
- Enums: a value is a constant expression and names no other value of its
  enum. `B = E.A + 1` is refused, probe.
- Variants: `v.tag` is the one field and is read-only. A variant without a
  case is refused.
- Classes: the order of a body. Fields stand first, then constants, static
  fields and functions. A field after one of those is refused with
  `` expected `fn` ``, probe.
- Classes: a class has one `construct`, and one without arguments cannot
  fail. Neither `construct` nor `destruct` is called directly.
- Classes: a base takes type arguments, `class IntBox inherits Box<int>`,
  probe.
- Classes: a static function may share its name with a static of the
  chain. "Small items, round three" of the additions, probe.
- Classes: a function that matches an abstract one needs `concrete`.
- Interfaces: a generic interface as a named sub-object,
  `implements src: Source<int>`, probe.
- Interfaces: a conversion that two interfaces give is refused with
  `name one`.

### Generics to concurrent classes

- Generics: a `type` alias takes no type parameters, `type Same<T> = ...;`
  is refused. It names any type: a function type, a tuple, a slice. Probes.
- Ownership: a local does not move inside a loop, while a `defer` or an
  `undo` names it, or once a closure captured it. `sema_value.c`.
- Ownership: `[v; n]` refuses a value that owns something, probe.
- Static fields and singletons: a literal of a singleton is refused and
  names `get()`, probe. `delete` of one is refused.
- Static fields and singletons: the types an atomic takes. The checker
  states them for a local alone.
- Threads: `parallel` gives a `[]R` of heap memory that the program frees
  with `free(r.ptr)`. "Threading" in `docs/decisions.md`.
- Threads: `by N` sets the count of chunks and never the threads. The
  first chunks take the remainder.
- Threads: the worker takes arguments after its chunk,
  `parallel xs -> scale(3)`, and `dispatch p -> f` needs none. Probes.
- Threads: a `str` counts as pointer-free. `dispatch` gives a `Job`, and
  `join` gives the result of its worker.
- Threads: a `worker fn` cannot `delete` an object another one may hold.
- Concurrent classes: a class that inherits a synchronized or a concurrent
  class is written with the same word, probe. Its base is of its kind or
  an abstract class without fields.
- Concurrent classes: the fields of a synchronized class are reached by
  its own functions alone.

### Plugins to tools

- Plugins: the six keys of `[runtime]`, from `src/rt/conf.c`: `backtrace`,
  `hash_seed`, `logger`, `plugins`, `threads` and `trace`. The overview
  names `logger` and `plugins` nowhere.
- Tests: a function of `tests` takes no parameters and returns nothing.
  `anti test` takes the files to test, with `-I` and `--work`.
- C interop: `export` stands on `class`, `enum` and `variant` as well,
  probe. The sentence lists `fn`, `struct`, `union`, `const` and `type`.
- C interop: a variadic argument has type `i32`, `u32`, `int`, `u64`,
  `float` or a pointer. An `f32` is refused, probe.
- C interop: `main` is never exported, two `export fn` of one symbol are
  refused, and an `extern fn` takes no default, no `lent` and no closure.
- Collections: the operations of each family. The page names 21 of them.
  `List<T>` has 20 public members, `SortedMap<K, V>` 24, `SyncMap` 32 and
  `Tree<T>` 9, and `Collection<T>` gives 20 to all.
- Standard library: `reflect.Payload`, a public union, and
  `toml.Document.open`, which may fail.
- Standard library: the members of most classes. `args.Parser` has 10,
  `log.Logger` 11, `toml.Document` 10, `random.Source` 6, `time.Clock` 5,
  `config.Config` 5 and each handler of `anti.trace` 12 to 16.
- Tools, `anti build`: `--target all`, `--runtime <dir>`, `--llvm-ar` and
  `--llvm-mc`.
- Tools, `anti check` and `anti doc`: the files, `-I`, `--work` and
  `--runtime`, and `-o` of `anti doc`.
- Tools, `anti bind`: `-o`, `--module`, `--target`, `-I` and `-D`.
- Tools, `anti symbols`: `--conf`, `--from`, `--out` and `--symbols`. Tools,
  `anti sdk`: `--sdk`, `-o` and `--sysroot`.
- Tools, `anti`: `--help`, `-h` and `--version`.
- Tools, `antic`: `-I`, `--runtime`, `--keep-llvm`, `--sdk`,
  `--show-sdk-path`, `--show-sdk-version`, `--llc`, `--opt`, `--llvm-ar`,
  `--llvm-mc`, `--anti-internal`, `--help`, and the four options of a
  package header.

## Descriptions that are wrong or stale

- Operators on classes: "compound assignments derive". `a += b` on a class
  with `operator fn add` is refused with `` `+=` does not apply to `V` ``,
  probe. Decision 27 keeps it so. `!=`, `>`, `<=` and `>=` derive, probe.
- Optional values: "`?.`" among what a `?T` works with. `p?.v` of an `int`
  field is refused with `` `?.` on `p.v`, which is not a pointer ``, probe.
  `?.` gives a pointer or nothing.
- Literals: "Literals take their type from context." An integer literal
  never becomes a float. `let f: f32 = 1;` is refused, probe.
- Program structure: "An item body opens `{` on its own line." `anti fmt`
  leaves `struct Pair { first: int, second: int, }` on one line, as it was
  run here. The page writes such items itself.
- Anonymous functions and closures: "a global" among the places a closure
  is never stored. The language has none.
- Tuples: the last sentence, on a tuple of more than three elements or one
  that crosses a module boundary. It reads as a rule and is advice. A tuple
  of four compiles, probe, and "Tuples" in `docs/decisions.md` says that
  nothing is refused.
- Checks and debugging: the paragraph on `trace` and its example repeat
  "Hooks and tracing".
- Standard library, `anti.plugin`: "`Library` with `instance`, `supports`".
  Its public functions are `instance_at` and `supports_at`. The two short
  names are forms of the checker.
- Standard library, `anti.toml`: the column of failing functions holds
  `Document.read` alone.
- Tools: "The options of `antic` that this page names". The page names
  `-I` under "Standard library", and the table lacks it.
- Reserved words: "Built-in functions known by name" reads as complete and
  is partial.

## Rules of 2026-10-11 against the overview

"Language rules" of "Decisions of 2026-10-11" holds decisions 19 to 27.
The overview states none of them but the first form of 27.

| Decision | Rule | The code today | Section |
|---|---|---|---|
| 17 | an arm of a `switch` may be `free(p)` | refused, probe | Statements |
| 19 | no `return`, `break`, `continue`, `fail` or `try` in a `defer` or an `undo` | accepted or a crash of antic, T001 | Statements |
| 20 | a handler of a value does not reach its end | accepted, T044 | Errors |
| 21 | `try` on an optional is refused | accepted, T045, probe | Errors |
| 22 | `return` of a field that owns something is refused | accepted, T037 | Ownership |
| 23 | a second `join` of one job is refused | accepted, T053, probe | Threads |
| 24 | a constant index past a fixed array is refused | accepted, T016, probe | Types |
| 25 | `count` of a collection is read-only outside its module | written, T090 | Collections |
| 26 | a `str` in an `extern fn` is refused | accepted, T089, probe | C interop |
| 27 | an `abstract fn` needs an `abstract class` | accepted, T064, probe | Classes |
| 27 | no direct call of `destruct` | `self.super.destruct()` compiles, T068 | Classes |
| 27 | `mutable` on a singleton alone | accepted on a plain class, T065 | Static fields |
| 27 | `static atomic` on an `f64` is refused | accepted, T059 | Static fields |
| 27 | no `==` on pointers of unrelated classes | accepted, T066 | Classes |
| 27 | `equals` and `hash` are replaced together | accepted alone, T067 | Operators on classes |
| 27 | `<` on an enum stays refused | refused | Enums |

## Claims of the overview the code does not hold

Each row is an open finding of `docs/audit/tests-findings.md` that touches a
sentence of the overview.

| Finding | Section | The sentence | The code |
|---|---|---|---|
| T018, T019 | Classes | a `const` of the body is reached as `Shape.MAX` | not in a constant or a length, and a crash as a default |
| T076 | Classes | the same | refused from another module |
| T075 | Classes | `fn` to the class only | a static member without a mark is reached from the module |
| T030 | Classes | a class of another module as `shapes.Circle { }` | `shapes.Circle(args)` is refused |
| T083 | Classes | `==` on class pointers is object identity | false between an interface pointer and the object |
| T073 | Classes | `p is *T` and `p as *T` | refused from an interface to the abstract base |
| T084 | Classes | a class inside a class body | a qualified `concrete fn` in it is refused |
| T091 | Variants | `geo.Shape.Circle { r: 2.0 }` | refused once type arguments follow the variant |
| T094 | Generics | `N: int` takes an integer constant | a constant of another module is refused |
| T072 | Generics | each copy is a type of its own | a constant and a static field are one for all copies |
| T031 | Ownership | `dup` copies a value that owns something | a fixed array of such elements has no copy |
| T081 | Operators on classes | the default `==` by the kind of each field | an `own` pointer compares by address |
| T074 | Operators on classes | only the left operand's type is looked up | a literal on the right takes the left type |
| T036 | Statements | `if let` binds the value of a `?T` | refused on `p as? *T` |
| T013 | Literals | `[v; n]` copies `v` into `n` elements | refused where the context widens the element |
| T048 | Types | `??T` is a `?T` whose value is a `?T` | a `?int` is refused there |
| T062 | Concurrent classes | no pointer into the fields leaves | one inside a tuple or a struct does |
| T079 | Static fields | a plain field is read-only after creation | written through its address |
| T058 | Threads | `chan T` carries pointer-free values | a generic copy sends a pointer |
| T092 | Plugins | `unload` refuses while an object is alive | a local without `destruct` counts for ever |
| T011 | Enums | `as` converts to any numeric type | a negative value is extended with zeros |

A probe of this reading found one more, which has no finding yet. A
qualified body cannot name a generic interface. `concrete fn Source::next`
is refused with `` `Source` is no base and no interface of `Ones` ``, and
`concrete fn Source<int>::next` does not parse. The unqualified body
compiles.

## Examples

No example shows a form the compiler refuses. The runner compiled all 55.
Four examples go against a rule the page states:

- Threads: `parts` and `parts2` are never freed, and `parallel` gives heap
  memory.
- Literals: `g` and `h` hold the text of an `f"..."` and an `rf"..."`, and
  neither is freed.
- Errors: `log.warn(f"bad number {s}")` drops the text, which the program
  owns.
- Regular expressions: `io.println(f"{m.year} / {m.month}")` does the same.

One block of 55 is a program with its output, the first. The built forms
without an example:

- `use`, `self.super` and `self.super.construct`
- `protected`, `transient`, `compatible` and `align(N)`
- `sync a, b` and `destroy`
- `p is *T` and `as?`
- a `switch` on a `str`
- a format kind such as `{n:x}`
- `concrete fn Base::f`

## Entries marked as not built

The rewrite of 2026-10-11 took "Later" out and gave each section one
sentence that opens with "Not built yet". All 18 were checked, and none is
built:

- `undefined`, `show`, `unreachable`, `embed`, `type_of` and `target` are
  unknown names, probes. A label and a named argument do not parse.
- `src/antic/warnings.c` holds no warning on a field written inside `sync`.
  `src/antic/llvm_debug.c` writes no variable.
- `sema_copies.c` still refuses a copy that a library file does not carry,
  which T095 shows.
- `src/anti/main.c` has no `format` command and runs no file as a script.
- `src/std/` holds none of `anti.net`, `anti.raylib`, `anti.miniaudio`,
  `anti.db` and `anti.binary`.

Not built and unnamed in the page: decision 17, the open rows of the
table of decisions above, and decision 12, by which `args.text_of` and
`args.at` give `?str`. Both give `str` today.

## Section plan

The table holds the sections in the order of the current contents. Ten
sections stay as they are, and the table leaves them out:

- Simd structs, Pointers, Reflection, Hooks and tracing and Injection
- Source locations, Processor levels and Warnings and safety checks
- Regular expressions and Bytes

| Section | Action | What changes |
|---|---|---|
| Program structure | change | the block doc comments, the brace rule as the form of `anti fmt`, a one-line item |
| Modules | change | import cycles, no variable at module level |
| Types | change | the array length, the struct that holds itself, decision 24 |
| Literals | change | the format specification as a table, the digit rules, the integer literal and floats, two freed examples |
| Variables and constants | change | what a constant holds, the atomic local, the parameter |
| Operators | change | `is` in the table, the conversion table, the operand types, the flags form |
| Statements | change | the types of a `switch`, what a statement is, decisions 17 and 19 |
| Loops | change | integer ranges alone, `for i, x` |
| Functions | change | trailing defaults, no nested function, the generic function as no value |
| Anonymous functions and closures | change | the body of a function alone, "a global" out |
| Tuples | change | two elements or more, `t.0.1`, the last sentence as advice |
| Errors | change | decisions 20 and 21, a freed example |
| Structs | change | the literal that names every field |
| Enums | change | the members of a body, the value as a constant, `export enum` |
| Variants | change | `tag` read-only, `export variant` |
| Classes | change | the order of a body, one `construct`, a base with type arguments, the rules of decision 27 |
| Interfaces | change | a generic interface, the ambiguous conversion |
| Generics | change | the alias without parameters |
| Ownership | change | where a local does not move, decision 22 |
| Optional values | change | what `?.` gives |
| Operators on classes | change | compound assignments out, decision 27 on `equals` and `hash` |
| Static fields and singletons | change | the literal, the types of an atomic, `mutable` |
| Threads | change | the result of `parallel`, `by N`, arguments, `Job`, decision 23, a freed example |
| Concurrent classes | change | the rule of inheritance |
| Plugins | change | the paragraph on the configuration moves out |
| Runtime configuration | add, after Plugins | the three ways to name the file, the six keys as a table, `--anti.inspect`, `--anti.help`, `rt.get` |
| Tests | change | the signature of a test, the arguments of `anti test` |
| C interop | change | the full list of `export`, variadic arguments, decision 26 |
| Compile-time targets | move | into "Features not built" |
| Checks and debugging | change | the paragraph on `trace` and its example go |
| Wire formats | move | into "Features not built" |
| Script mode | move | into "Features not built" |
| Collections | change | one table of operations per family, decision 25 |
| Standard library | change | a column of members, `Payload`, `Document.open`, `instance_at` |
| Tools | change | every option of every command, the missing options of `antic` |
| Features not built | add, before Reserved words | compile-time targets, wire formats and script mode, one `anti not-built` block each |
| Reserved words | change | `fr`, the full list of names known by name |

The move leaves 44 sections: 45 less the three that hold nothing built,
plus the two new ones. Every other section then describes a built feature
and ends with its own "Not built yet" sentence where it has one.

A rule of a decision that the code does not hold yet goes into its section
as the rule. The "Not built yet" sentence of that section names it. The
rows of the findings table that are defects and no rule stay in
`docs/audit/tests-findings.md` alone.

## Wrong in an older document

- `docs/notes/test-matrix.md`, "Statements": `while c { }`, `assert c;`
  and `const N = e;`. The parser asks for `do`, for the parentheses and for
  a type. Probes.
- The same note, "Expressions": `alloc(S { })` and `alloc(C(args))`. The
  forms are `alloc S { }` and `alloc C(args)`, and `alloc(` opens
  `alloc(T, n)`. Probe.
- The same note: "`dup(e)`, `delete(p)`, `destroy(p)`, each with `from`".
  `dup` takes one argument, probe.
- The same note, "Items": `static fn` and `type Name<T> = ...;`. Neither
  parses, probes.
- `docs/anti-language-additions.md`, "Small things": `p?.x` is "refused
  otherwise, since Anti has no optional values". The language has `?T`
  now. The refusal stands in the code.
- The same document, "Keywords": `unreachable`, `undefined`, `show` and
  `embed` as keywords added. The lexer holds none of the four.
- The same document, "Messages": the runtime keys as `threads`, `logger`,
  `plugins`, `trace`. `src/rt/conf.c` holds six.
- `docs/decisions.md`, "Tuples": "A tuple carries no descriptor". The entry
  under "Generics and collections" gives a tuple the type id `Tuple` and a
  descriptor, and the overview follows that one.

## Questions answered from the code alone

- Does `anti fmt` keep a one-line item? Yes, run on a struct, an enum and
  a function.
- Does an enum body take members? Yes, constants and static functions. The
  first reading left it open.
- Is `dup(p, from)` a form? No. The parser reads a second argument and the
  checker refuses it.
- Does `by N` of `parallel` set the chunk size? No, the count of chunks.
- Does a `switch` on an integer need an `else`? No.
- Which integer does a `char` convert to? `u32` alone.
- Does `import a.b.{x as y}` exist? No, probe.
- Do block comments nest? No, probe.

## Questions for Eddie

- Should `p?.x` of a value field give a `?T`, now that the language has
  optional values?
- Do compile-time targets, wire formats and script mode move into one
  closing section, as the plan proposes?
- Does the overview list the members of every class of the standard
  library, or point at `docs/site/standard-library/` for them?

## Gates

- Only this report changed, so no example test and no suite ran as a gate.
  The host suite did not run, as the task allows, and has no pass count
  here.
- The build of this tree was `antic`, `anti`, `anti_std`, `llvm_tools` and
  `anti_rt_macos-arm64_armv8.5`, for the probes and the example runner.
- `python3 tools/docs-style/check_docs.py` reports nothing on this report.
- `git grep` finds none of the three marker words of the gates in it.
