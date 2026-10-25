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
| `undo-never-runs` | An `undo` stands in a function that cannot fail and in no `try` block, so no error exit runs it. | Remove the `undo`, or declare the function `may fail`. |
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
| `nothing-exported` | `--lib static` builds a module without an exported item, so C finds nothing in the library, by R131 of "Rules of 2026-10-13". | Export what C calls, or allow it where the library is linked for its module alone. |
| `blocking-under-lock` | A `recv`, a `send`, a `select`, a `join` or a `join_all`, or a call of a function that reaches one, stands in a `sync` block or in a function of a synchronized class that holds the hidden lock. | Take what the lock guards, leave the block, then wait, or allow it where the wait cannot block for long. |
| `unused-result` | The program drops the result of a call to a function without effects, so the call does nothing. | Use the result, or remove the call. |

The five doc warnings come with `--doc-warnings` alone, which `anti check`
passes. They belong to its doc class, which fails nothing. They stay warnings
under `--warnings-as-errors` and in a release build.

`sema_lock.c` holds `blocking-under-lock` beside `lock-order`. It reads the
same calls, so a call through a table, an interface or a function value is
not followed. Every channel has room for a bounded number of values, so any
`send` may find it full, and a `select` has no `else`, so every one may
wait. The message names the innermost lock, and for a call the first wait
the callee reaches:

```text
`next_two` may wait in `recv` on line 7 of `com.example.waits` while the `sync` of line 19 holds its lock [blocking-under-lock]
```

A library file carries the first wait of each function of its module that
waits. A call of a library function is then followed as well.
`tests/errors/blocking_under_lock.anti` and `tests/errors/allow_levels.anti`
silence it.

`warn_unused_result` of `sema_stmt.c` gives `unused-result` at a statement
that is a call. antic knows three calls without effects. The first is the
default hash of a value whose parts reach no function of the program, no
`hash` of a class and no Regex, which `sema_hash_without_effects` reads.
The second is `mul_high`. The third is an `extern fn` that names a
function of the runtime whose row has the class none or reads its
arguments, with willreturn, which `rt_without_effects` of `rt_abi.c`
reads. No pass reads the effects of a function body, so a function of the
program never warns. Neither does a simd built-in, which the optimizer
counts as an instruction with effects. A copy of a generic checks no
statement again and warns of nothing. The message names the function:

```text
`hash` has no effects and its result is dropped, so the call does nothing [unused-result]
```

`tests/errors/unused_result.anti` and `tests/errors/allow_levels.anti`
silence it.

## Safety checks

`unchecked(name, "reason")` overrules one of these. `sema_thread.c` holds
`unguarded-field`, which reads the fields of a concurrent class, and
`pattern.c` holds `exponential-pattern`, which `sema_check_pattern` runs on
every pattern literal. `tests/programs/lock_free_queue.anti` and
`tests/programs/pattern_literals.anti` overrule each. `sema_escape.c` holds
`escaping-local`, a walk of every function body after the checker, and
`tests/errors/escaping_local.anti` overrules it. `sema_free.c` holds
`use-after-free`, a walk of the paths of every function body after the
checker, and `tests/errors/use_after_free.anti` overrules it. `sema_uninit.c` holds
`uninitialized-read`, a walk of the paths of every function body after
the checker, and `tests/errors/uninitialized_read.anti` overrules it.
`sema_index.c` holds `index-out-of-range`, which reads the range of every
index of a function body after the checker, and
`tests/errors/index_out_of_range.anti` overrules it. `sema_lock.c` holds
`lock-order`, which builds the graph of lock orders of the module and its
libraries once every function is checked, and `tests/errors/lock_order.anti`
overrules it. A library file carries the locks of its functions and the
edges of its code, so a cycle across modules is refused in the module that
closes it.

| Name | Meaning | Fix |
|---|---|---|
| `unguarded-field` | A field of a concurrent class is not guarded, atomic or fixed. | Guard the field with `guarded by`, make it `atomic`, or fix it at construction. |
| `exponential-pattern` | A pattern literal nests repeats over text that overlaps, which can take exponential time. | Use the possessive quantifiers or an atomic group, `a++` or `(?>...)`. |
| `escaping-local` | A reference to a local or a parameter, or to a part of one, would outlive it: returned, yielded, stored outside its block, sent, passed to a `dispatch` its block does not join or captured by a closure that leaves. | Give the value its own memory with `alloc` or `dup`, or keep the reference inside the block of its local. |
| `use-after-free` | A value is used on a path after `delete`, `destroy` or a move of it. | Assign it a new value first, or use it before it dies. |
| `uninitialized-read` | An element of `alloc(T, n)` is read before any path wrote it. | Write the element, or fill the whole range, before the read. |
| `index-out-of-range` | An index is out of range on every turn or every path that reaches it. | Correct the bound, as `0..n` in place of `0..n + 1`. |
| `lock-order` | The graph of lock orders of the program holds a cycle, so two places take the same locks in opposite orders. | Take the locks in one order everywhere, or both with `sync a, b`. |

## Levels and cover

The parser reads each clause where it applies and records the source it
covers, both ends included. A clause before a statement covers the statement.
A clause last in a header covers the function, class or struct. The range
opens at the doc comment, where a doc warning stands, and closes at the brace. A clause after a field's type covers the field. A
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
program build, `single-segment-path` under `-c` and `nothing-exported` under
`--lib static`. A clause of another check
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
