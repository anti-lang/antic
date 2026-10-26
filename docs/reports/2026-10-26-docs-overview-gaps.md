# Gaps between the syntax overview and the built language

Date: 2026-10-26, from `bceb991d`. A report and nothing else: no other
file changed, and no suite ran.

The step compared `docs/anti-syntax-overview.md` with the code. It read
`kinds[]` and `reserved[]` of `src/antic/lexer.c`, the forms of
`parser*.c`, the 737 message texts of `src/antic/sema*.c`, the built
entries of `docs/decisions.md`, `docs/anti-language-additions.md`, every
`pub` item of `src/std/anti/` and the usage of `src/anti/main.c`. Where
the text and the code disagree, the code is named as the authority. A
probe is a program under `build/overview-gaps/probe/`, built by the host
antic. `docs/reports/2026-10-11-docs-overview-gaps.md` served as a list
to check.

Contents:

- [Reserved words against the lexer](#reserved-words-against-the-lexer)
- [Features the overview lacks](#features-the-overview-lacks)
- [Descriptions that are wrong or stale](#descriptions-that-are-wrong-or-stale)
- [Examples in an old form](#examples-in-an-old-form)
- [Entries of Not built yet](#entries-of-not-built-yet)
- [The report of 2026-10-11](#the-report-of-2026-10-11)
- [Section plan of the updated overview](#section-plan-of-the-updated-overview)
- [Questions answered from the code](#questions-answered-from-the-code)
- [Questions for Eddie](#questions-for-eddie)
- [Gates](#gates)

## Reserved words against the lexer

The keyword list and the type names of "Reserved words" hold 100 words.
A script compared them with every `CAT_KEYWORD` entry of `kinds[]`, and
the two sets are equal. `reserved[]` holds `thread` alone, and
`is_null` refuses `null` with `` `null` is `none` in Anti ``, as the
section says. The contextual words match the words that `parser*.c`
reads with `parser_is_word`. Three differences remain:

| Overview | Code | Finding |
|---|---|---|
| `alloc`, `free` and `union` name a member after `.` | `field_link` of `parser_expr.c` also takes `super`, `destroy` and `select` after `.`, and `parser_is_field_name` takes `alloc` and `free` as the name of an `inject` field | Name the five words and their places. |
| String prefixes `r b br f rf x re` | `string_prefixes[]` holds `fr`, refused with `` `fr"` is not a prefix, write `rf"` `` | Add the refusal. |
| `//`, `/* */`, `///`, `//!`, `//#`, `//#!` | `doc_marker` reads `/** */`, `/*! */`, `/*# */` and `/*#! */` as well | Belongs to Program structure, see below. |

## Features the overview lacks

Each item is built, has its rule in `docs/decisions.md` or a message of
the checker, and no sentence of the overview states it.

| Section | Feature |
|---|---|
| Program structure | The block forms of the doc comments. `////`, `/**/` and `/***` stay ordinary comments. Identifiers are ASCII. A byte order mark is skipped, and a CRLF in a string becomes LF. |
| Modules | The import each form asks for. `may fail`, `fail`, `here` and a guard `catch` need `import anti.lang`. `f"..."` and a `switch` on a `str` need `anti.text`, and an allocator argument and `Object.deserialize` need `anti.mem`. The section states `anti.regex` alone. |
| Modules | An import cycle is refused with the cycle named, R125. A module is imported once, so `import anti.text;` beside `import anti.text.{equal};` gives `` `text` is already declared ``. A keyword is never a segment: a file `here.anti` is refused. The search roots are `-I`. |
| Variables and constants | Globals are constants. A `let` at module level is no item, and a static atomic field is the one global a program writes. |
| Literals | Without context an integer literal is `int` and a float literal is `float`. A decimal literal other than `0` has no leading zero. |
| Literals | The format specification `[align][0][width][.precision][kind]` with `<`, `>`, `^` and the kinds `x X b o e f`. `{{` and `}}` write a brace. An enum and a `?*T` are refused in `{expr}`. An `f"..."` is no constant. |
| Types | The length of `[N]T` is a constant `int` of at least 1. A length from `size_of` stands in a field or a local alone, and its `.len` is no constant. A C `void *` is `*byte`. |
| Operators | `/` truncates toward zero, `%` takes the sign of the dividend and `>>` is arithmetic on a signed type. `bool as` an integer gives 0 or 1, and no integer converts to `bool`. Operands run left to right, the place of an assignment first. |
| Statements | `(r, f) = e;` assigns the flags form, which takes one `+`, `-`, `*`, `<<`, `>>` or unary `-`. |
| Functions | A parameter with a default stands after every parameter without one, and an `extern fn` takes no default. A call through a function value gives every argument. |
| Anonymous functions and closures | An anonymous function stands in a function body alone. `break` and `continue` in it reach no loop around it. A captured `?*T` keeps its declared type inside the closure. |
| Errors | `e.fatal()` exits with `e.code`, or 1 for code 0. `catch fatal` in a worker ends the whole program, R021. `destruct` cannot fail. |
| Structs | A literal names every field exactly once. A field of function type wins over a method of its name. Method-call sugar does not reach a union. |
| Simd structs | `%`, the wrapping and saturating operators, `&&`, `\|\|` and `!` are refused. `+`, `-` and `*` on integer lanes wrap with no dev check. |
| Enums | An enum without values has the size of `c_int`, and two values may share a number, R052 and R053. |
| Enums | An enum body takes functions and constants after its values, see [Questions for Eddie](#questions-for-eddie). |
| Variants | A variant literal is no constant, so no constant, field default or parameter default has a variant type, R040. |
| Classes | A class has one `construct`, and every other maker is a static function. `construct` and `destruct` are never called by name. An `alloc` of a singleton is refused, and so is a literal of it outside the class. |
| Pointers | Narrowing reads a name alone. `if n.next != none` leaves `n.next` a `?*T`, which the probe `narrow.anti` shows. An assignment to the name ends it. |
| Static fields and singletons | An atomic field takes no `=` and no `+=`, and its value is read with `load()`. It holds an integer, a `bool`, a `char`, an `f16`, a pointer or a plain function pointer. |
| Threads | `parallel` gives `[]R`, freed with `free(r.ptr)`. The worker after the arrow takes arguments that every chunk receives, `parallel data by 2 -> scaled(10)`. `by 0` gives the chunk count of the machine, R087. |
| Threads | `join(job)` gives the result of the worker. A job is joined once, which the checker refuses and a dev build traps. A `send` on a closed channel stops the program, and a capacity below 1 does too. A worker cannot `delete`. |
| Concurrent classes | Either kind inherits the root, an abstract class without fields or a class of its own kind. A field of a synchronized class is reached by the functions of the class and of the classes below it alone. |
| Hooks and tracing | The names of the runtime key `trace`: `leaks`, `profile`, `calls`, `errors`, `threads` and `writes`. A list with commas installs a `Composite`. |
| Plugins | The keys of `[runtime]`: `threads`, `logger`, `plugins`, `trace`, `backtrace` and `hash_seed`. `logger` names the TOML file of `anti.log`, and `plugins` the directories of discovery. |
| Tests | A function of a `tests` block takes no parameters and returns nothing. |
| C interop | `export` takes a `class`, an `enum` and a `variant`, which the probe `exp.anti` writes into the header. An `extern fn` takes no `lent`, `concurrent` or `keep own` parameter. |
| Regular expressions | A pattern is a literal only where it stands in the call. `let r = re"a"; s.matches(r)` needs a handler. A constant of type `Regex` is refused. |
| Standard library | `Payload` of `anti.reflect`. `set_name`, `set_level` and `set_format` of `anti.log.Logger`, by Q4. `Mode.Append` of `anti.fs`. |
| Tools | `-I <dir>`, `--runtime <dir>` and `--keep-llvm` of antic. antic exits with 1 on a refused program, 2 on a refused command line and 70 when memory runs out. |

## Descriptions that are wrong or stale

1. Tuples: "Destructuring is `let (a, b) = e;` and `for i, x in items`,
   and nowhere else." `tuple_pattern` of `parser_stmt.c` reads
   `for (k, v) in e`, the probe `tup.anti` walks an array of tuples so,
   and the Collections section writes `for (k, v) in ages`.
2. C interop: "`export` on `fn`, `struct`, `union`, `const` and `type`
   implies `pub`." It omits `class`, `enum` and `variant`, which antic
   exports.
3. Collections: "Not built yet: the versioned `set` of `SpscRing`." The
   `[provisional]` entry at line 2584 of `docs/decisions.md` decides that
   the ring has none. That is a decision and no missing work, and the
   paragraph before already says so.
4. Standard library: the column of `anti.toml` names `Document.read`.
   `Document.open` may fail as well.
5. Standard library: the column of `anti.collection` names `find_one`,
   `update_one` and `remove_one`. `set_if_version` of `Pool`, of the
   synchronized collections and of `ConcurrentMap` may fail, and so does
   `find_one_versioned` of the synchronized ones.
6. Standard library: "`Library` with `instance`, `supports`". The class
   declares `instance_at` and `supports_at`, which the call forms
   `lib.instance(I)` and `lib.supports(I, "f")` reach. The row names call
   forms where it names items.
7. Warnings and safety checks and Modules: a clause of the whole file
   stands "before the imports". The loop of `parser_parse` and R122 take
   it among the imports as well.
8. Operators: the precedence table has no row for `is`. `cast_links` of
   `parser_expr.c` reads `is` beside `as`.
9. Checks and debugging repeats the paragraph on `trace` that Hooks and
   tracing holds. Nothing in it is wrong.

## Examples in an old form

No block shows a superseded form. `List<Person>.new(1000)` and the
positional `patch` stay the current forms while named arguments are not
built, and the text marks both.

The blocks depend on a line the reader never sees. The runner writes
`import anti.lang;` before every block that lacks it, by the header of
`tests/runners/run_document_examples.cmake`. The blocks at lines 612,
707, 1288, 1299 and 1470 name `lang.` without an import. Copied into a
file, the block of Source locations gives `` cannot find module `lang` ``,
and a `may fail` function gives `` `may fail` gives `?*anti.lang.Error`,
so the module imports `anti.lang` ``. The introduction says neither, and
it does not name the mark `anti refused` that Operators on classes uses.

## Entries of Not built yet

The overview holds 18 lines that open with "Not built yet". None names
a built feature. The probe `nb.anti` refuses `undefined`, `unreachable;`,
`show`, `embed`, a label, a named argument, `target.os` and `type_of`.
No code of `src/anti/` or `src/antic/` holds a cache of generic copies,
and `sema_strip_generics` still keeps a function with type parameters of
its own and a type nested in a generic class out of a library file. The
one misclassified line is item 3 above.

## The report of 2026-10-11

Every finding of that report now stands in the overview: the keyword
list, `thread`, `null`, the C types, `link`, `inject final`, the tokens,
the built-ins and the literal forms. So do the arm forms, the export
rules, the tables of 24 warnings and checks, the standard library and the
tools. Its stale sentences on `ANTI_THREADS`, the runs of Windows, the
pinned clang and the section Later are gone. Its first question, the
functions of an enum, has no answer yet.

## Section plan of the updated overview

In the order of the current contents. A section the table does not name
stays as it is.

| Section | Action | Change |
|---|---|---|
| Introduction | Change | Say that the runner adds `import anti.lang;`, and name the mark `anti refused`. |
| Program structure | Change | The block doc comments, ASCII identifiers. |
| Modules | Change | The import each form asks for, the cycle, one import per module, `-I` and no keyword as a segment. |
| Types | Change | The rule of an array length, `*byte` for `void *`. |
| Literals | Change | The default types, the format specification and its values, `fr`. |
| Variables and constants | Change | No module-level `let`. |
| Operators | Change | `is` in the table, the semantics of `/`, `%`, `>>` and `bool as`, the order of evaluation. |
| Statements | Change | The assignment form of the flags. |
| Functions | Change | Trailing defaults, the call through a function value. |
| Anonymous functions and closures | Change | Where one stands, `break` and narrowing inside it. |
| Tuples | Change | Fix item 1. |
| Errors | Change | The exit status of `fatal`, `destruct` cannot fail. |
| Structs | Change | One field per name in a literal, a field over a method. |
| Simd structs | Change | The refused operators, the wrap of integer lanes. |
| Enums | Change | An empty enum, shared values, and the enum body once Eddie answers. |
| Variants | Change | No constant of a variant type. |
| Classes | Change | One `construct`, the refusals of a singleton. |
| Pointers | Change | Narrowing by name. |
| Static fields and singletons | Change | The atomic field and its types. |
| Threads | Change | The result and the arguments of `parallel`, `by 0`, `join`, the closed channel. |
| Concurrent classes | Change | The rule of bases, the reach of fields. |
| Hooks and tracing | Change | The names of the key `trace`. |
| Plugins | Change | Move its third paragraph into the new section below. |
| Runtime configuration | Add | After Plugins: the file, the layers and the six keys. |
| Tests | Change | The signature of a test. |
| C interop | Change | Fix item 2, the refusals of `extern fn`. |
| Checks and debugging | Change | Drop the repeated `trace` paragraph and link Hooks and tracing. |
| Warnings and safety checks | Change | Fix item 7. |
| Regular expressions | Change | A literal at the call, no `Regex` constant. |
| Collections | Change | Fix item 3. |
| Standard library | Change | Fix items 4 to 6, add `Payload`, the setters and `Mode.Append`. |
| Tools | Change | `-I`, `--runtime` and the exit statuses of antic. |
| Reserved words | Change | The five words after `.`, the refusal of `fr`. |

No section is removed.

## Questions answered from the code

- Is the keyword list the lexer's? Yes, 100 words on both sides.
- Does an enum body take functions? Yes. In `enumfn.anti` `self` is
  `*Mode`, `Mode.rank(&m)` and `Mode.LIMIT` compile, and `m.rank()` gives
  `` `Mode` has no field `rank` ``.
- Does `e[i] += v` call `index` and `set_index`? No. `grid.anti` gives
  `` `+=` takes no `operator fn set_index` ``, which the overview's rule
  on compound assignments covers.
- Does `anti test --release` turn the assertions off? Yes, by the DESIGN
  comment of `src/anti/test.c`, as the overview says.

## Questions for Eddie

- An enum body takes functions and constants, and the matrix test
  `types_enum_internal_function` holds one. No specification names the
  form, and method-call sugar does not reach it. Is it a feature to
  describe, or a form to refuse?

## Gates

- `overview_examples` and `guide_examples` pass on the host build, 2 of
  2, in `build/drive/logs/overview-gaps-examples.log`. The runner reports
  56 blocks compiled, 1 program run, 1 refused as marked and 9 skipped as
  not built.
- The docs-style checker reports nothing on the report.
- A `git grep` for the three marker words the gates name finds none of
  them in the report.
- No suite ran, since the report is the one changed file, under `docs/`. The message
  texts of "Messages" in `docs/anti-language-additions.md` were not
  compared with the code, since they stand outside the overview.
