# Syntax overview after the section plan of the gaps report

Date: 2026-10-26, from `7bc100f4`. The step changed
`docs/anti-syntax-overview.md` alone, in 34 commits, one per section,
after "Section plan of the updated overview" in
`docs/reports/2026-10-26-docs-overview-gaps.md`. No file outside
`docs/` changed, and no suite ran.

Contents:

- [What the step wrote](#what-the-step-wrote)
- [Examples added](#examples-added)
- [What was wrong in the overview](#what-was-wrong-in-the-overview)
- [Findings](#findings)
- [Questions answered from the code](#questions-answered-from-the-code)
- [Gates](#gates)

## What the step wrote

Every row of the section plan is done, and every row of "Features the
overview lacks" of the gaps report stands in its section. Each claim was
checked against a probe under `build/ovu/r/`, built by the host antic,
or against the source it cites.

- Introduction: the mark `anti refused`, and the line
  `import anti.lang;` that the test writes before a block without it.
- Program structure: the block doc comments, the comments that stay
  ordinary, the byte order mark, ASCII identifiers, CR LF in a literal.
- Modules: the clause among the imports and no keyword as a segment.
  `-I` and the library file of an import, the cycle, one import per
  module, and the module each form needs.
- The rows of the plan for 22 more sections, from Types to Tools, each
  as the plan gives it.
- Runtime configuration: a new section after Plugins, which takes the
  third paragraph of Plugins. It adds a file, the six keys of
  `[runtime]`, the start errors with status 70 and `rt.get`, and the
  contents list names it.
- Checks and debugging: the paragraph on `trace` that repeated Hooks and
  tracing is now a link, and its one sentence the other section lacked
  moved there.
- Reserved words: `super`, `destroy` and `select` after `.`, `alloc` and
  `free` as an `inject` field, and the refusal of `fr"`. The keyword
  list equals the `CAT_KEYWORD` entries of `kinds[]` of
  `src/antic/lexer.c`, 100 words on both sides, by a script over both.

The "Not built yet" lines stay true. One line left the list, the
versioned `set` of `SpscRing`, which a `[provisional]` entry of
`docs/decisions.md` decides away.

## Examples added

Six programs whose output the test compares, beside the hello program that was there:

- the format specifications of Literals,
- the arithmetic and the order of evaluation of Operators,
- the empty enum, shared values and the enum body of Enums,
- `parallel` with an argument and `join` of Threads,
- `trace.install("leaks,calls")` of Hooks and tracing,
- `rt.get("threads")` of Runtime configuration.

Fifteen refused blocks: a second import of a module, `fr"`, a call
through a function value without its default, a captured `?*T`,
`destruct` that may fail, a struct literal that misses a field, `%` on a
simd struct, a constant of a variant type, a second `construct`, a
narrowed field, `+=` on an atomic field, a field of a synchronized class
read outside it, a test with a parameter, a compiled pattern without a
handler and a constant out of the range of `i8`. The `+=` of Operators on
classes was there before, so the test counts 16.

Blocks that compile without a run:

- the doc comments of Program structure,
- the array length of Types and the flags assignment of Statements,
- the tuple loop of Tuples,
- the exported class, enum and variant of C interop,
- `Mode.Append` and a setter of `anti.log.Logger` in Standard library.

Processor levels has two command lines, and Runtime configuration a TOML
file. Both were run by hand against the probe programs.

## What was wrong in the overview

The nine items of "Descriptions that are wrong or stale" of the gaps
report, each fixed in its section:

1. Tuples named `let` and `for i, x` as the only places that take a
   tuple apart. `for (a, b) in items` is a third, and `(a, b) = e;`
   is the flags form alone, which the probe `tup2.anti` shows with
   `` expected `Flags`, found `int` ``.
2. C interop left `class`, `enum` and `variant` out of what `export`
   takes.
3. Collections listed the versioned `set` of `SpscRing` as not built.
4. to 6. The table of the standard library lacked `Document.open`, the
   versioned functions that may fail, and named the call forms of
   `Library` where it names items.
7. Modules and Warnings put a clause of the whole file before the
   imports alone.
8. The precedence table had no row for `is`.
9. Checks and debugging repeated Hooks and tracing.

## Findings

None. Every probe did what `docs/decisions.md` and the specifications
say, so no row went into `docs/audit/tests-findings.md` and no line into
`tests/known-failures.txt`.

## Questions answered from the code

- Does an enum body take functions? Yes, and the overview now says so.
  `enumz.anti` prints `4 true 2 3`, and the matrix test
  `types_enum_internal_function` holds the form. The question of the
  gaps report to Eddie, whether the form stays, is still open. The
  overview describes it as built, since the code accepts it.
- Does `-I` find a source? The usage says that module `a.b` is
  `a/b.anti` or `a/b.antl` under a root. `search_roots` of
  `src/antic/driver_search.c` finds the library file alone, so an import
  needs the file of `antic -c`, and the overview says so.
- Which types does an atomic field hold? `sema_refuse_atomic_field` of
  `src/antic/sema_thread.c` takes an `f16` and a plain function pointer
  beside the four its message names.
- How does a width align? A number goes to the right and other text to
  the left, and a kind or a precision takes a number alone. So
  `format_spec` of `src/antic/parser_expr.c` and the formatter of
  `src/antic/sema_expr.c` have it.
- What separates the directories of `plugins`? `:`, and `;` on Windows,
  by `anti_rt_path_list_separator`.
- Is a field of a synchronized base reached below it? Not when it is
  private: the probe `syncok.anti` gives `` `count` is private to `Base` ``.
  The overview says that the class and the classes below it reach a field,
  and the visibility rules of Classes hold beside that.

## Gates

- `overview_examples` and `guide_examples` pass, 2 of 2, in
  `build/drive/logs/ovu-gate-examples.log`. The overview runs 63 blocks,
  7 programs, 16 refusals and 9 blocks skipped as not built, in
  `build/drive/logs/ovu-gate-overview-v.log`. The guide runs 133
  blocks, 96 programs, 458 refusals and 246 excerpts, in
  `build/drive/logs/ovu-gate-guide-v.log`.
- The docs-style checker reports nothing on the overview and on this
  report.
- A `git grep` for the three marker words the gates name finds none in
  the overview and in this report.
- No suite ran, since every changed file is a `.md` file under `docs/`.

## State after the push of the report

`git log --oneline -3`:

```text
7a2a4986 Report the update of the syntax overview
f8a118eb Give the processor levels of the overview an example
9dbebd94 Give the checks of the overview an example
```

`git status --short` printed nothing, and `git rev-parse HEAD
origin/main` printed one hash twice:

```text
7a2a49866a4a6354072e8905a16a26a823964116
7a2a49866a4a6354072e8905a16a26a823964116
```
