# Warnings and safety checks

The names of every warning and every safety check, each with its meaning and
its fix, and the choices of the passes that apply `allow` and `unchecked`. The
rules are in "Errors, warnings and checks" of
`docs/anti-language-additions.md`, and the settled points are in
`docs/decisions.md` under "Errors, warnings and checks". The table of
`src/antic/warnings.c` spells each name once, and the test `warning_names`
holds the lists below equal to it.

A name stands at the end of its message, `` `e` shadows the outer `e`
[shadowed-catch] ``. An error has no name, so no clause reaches one.

## Warnings

`allow(name, "reason")` silences one of these.

| Name | Meaning | Fix |
|---|---|---|
| `shadowed-catch` | The name a `catch` binds is a variable of an outer scope, which the handler hides. | Name the error another way, `catch err`. |
| `never-fails` | A `may fail` function holds no `fail` and no `try`. | Drop `may fail`, or allow it where an implementation of the contract may fail. |
| `assert-call` | An `assert` condition calls a function, which a release build does not run. | Call the function before the `assert` and test its result. |
| `unfilled-abstract` | No class of the program fills an abstract class, which a program build reports. | Fill it, or remove it, or allow it where a plugin fills it. |
| `above-vector-cap` | A `simd struct` is above the vector cap of 256 bytes, so it is an array and each operation on it a loop. | Split it into structs at or below the cap, or allow the loops. |
| `single-segment-path` | A library's module path has one segment, which is for a program's own files. | Give the module a path under the package, `com.example.geometry`. |
| `doc-markup` | A doc comment holds markup outside the doc subset: a heading, a table, emphasis, a numbered list, an image or HTML. | Write the text in the subset. |
| `doc-unresolved` | A name in backticks of a doc comment resolves to nothing. | Name an item that exists, or write the text without backticks. |
| `doc-note-only` | A `pub` item has a `//#` note and no `///` comment. | Add the `///` comment. |
| `undocumented` | A `pub` item has no `///` comment, under `--warn-undocumented`. | Add the `///` comment. |
| `doc-dropped` | A doc comment stands where no item or field follows it, or a module doc after the first import or item. | Move it before its item, or make it an ordinary comment. |
| `unused-allow` | An `allow` silences nothing. | Remove the clause. |
| `unused-unchecked` | An `unchecked` overrules nothing. | Remove the clause. |

The five doc warnings come with `--doc-warnings` alone, which `anti check`
passes, and belong to its doc class, which fails nothing. They stay warnings
under `--warnings-as-errors` and in a release build.

## Safety checks

`unchecked(name, "reason")` overrules one of these. Neither check is built
yet: each waits for the feature it guards, concurrent classes and regular
expressions. An `unchecked` of either overrules nothing until then.

| Name | Meaning | Fix |
|---|---|---|
| `unguarded-field` | A field of a concurrent class is not guarded, atomic or fixed. | Guard the field with `guarded by`, make it `atomic`, or fix it at construction. |
| `exponential-pattern` | A pattern literal nests repeats over text that overlaps, which can take exponential time. | Use the possessive quantifiers or an atomic group, `a++` or `(?>...)`. |

## Levels and cover

The parser reads each clause where it applies and records the source it
covers, both ends included. A clause before a statement covers the statement.
A clause last in the header of a function, a class or a struct covers the
declaration from its doc comment to its closing brace, since a doc warning
stands at the comment. A clause after a field's type covers the field. A
clause at the top of the file covers every line. A warning stands at a
position, so the pass that applies the clauses compares positions and needs
no tree. `src/antic/warnings.c` holds the pass, and the driver runs it after
the checker and the doc warnings.

A clause of a `tests` or `fixtures` block goes with the block, so a build that
drops the block does not report the clause as unused.

## Unused clauses

A clause is unused when nothing it covers carries its name. The pass reports
one only when the checker ran to its end, since a checker that stopped at an
error has not written every warning. It reports one only when the check of its
name ran in this build: the doc warnings run under `--doc-warnings`,
`undocumented` under `--warn-undocumented` as well, `unfilled-abstract` in a
program build and `single-segment-path` under `-c`. A clause of another check
waits for the build that runs it.

A clause may name `unused-allow` or `unused-unchecked`. The pass then applies
the clauses in two rounds, so the first round writes the warnings the second
round can silence.

## Release builds

A release build is one that is neither `--dev`, `-c` nor `--front-end`. It
turns every warning that stands into an error with the same message and name,
as `--warnings-as-errors` does in any build. `anti check` passes the option to
every call of the front end. `anti build --release` passes it to the call that
writes the library file of each module, since that is where each module is
checked, and its cache key keeps that file apart from the one a dev build
wrote.
