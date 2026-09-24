# The syntax of generics

The step was the "Syntax" section of "Generics" in
`docs/anti-language-additions.md`. It holds type parameters, type arguments,
the rule of C#, `>>`, constraint declarations and type aliases. The type
arguments of a call are inferred. The declarations are parsed and checked. Compiling
the copies is the next step.

## Done

- The lexer reads `constraint` and `type` as keywords. The library format is
  61 for the longer token table.
- The parser reads type parameters in `<>` on functions, structs, classes,
  variants, abstract classes and functions of a class body, with constraints
  joined by `+` and `N: int`. A type reads type arguments after its name, an
  integer literal among them. `>>` closes two lists. In an expression `<`
  opens a list when a list of types closed by `>` follows and a `(`, `.` or
  `{` comes after it. `constraint Name = a + b;`, `type Name = T;` and
  `export type Name = T;` are items.
- The checker gives each parameter a type of its own. It checks a body
  against the constraints and names the missing hook, and it checks each
  use against them where it stands. A copy of a struct, a class or a
  variant is one type per set of arguments, with its fields, base and cases.
  A call infers the arguments of a generic function, and of the class of a
  static call, from its arguments in order.
- The six messages the specification lists for generics are built:
  `` `g` is not generic. Put the comparison in
  parentheses ``, `` `Circle` has no `lt`, which `max` needs for `T` ``,
  `` `max` uses `+` on `T`, which its constraints do not give. Add `add` to
  them ``, `` `map` takes type
  parameters, so it cannot be `abstract` or replaced ``, `` `size` must be a
  constant for `N` `` and the refusal of a generic `export fn`.
- A build past the front end refuses the first use that needs a compiled
  copy, and leaves out a generic that nothing uses. `anti fmt` writes type
  arguments without spaces.
- The overview, the decisions, `CLAUDE.md` and `docs/notes/generics.md` hold
  the state. The specifications needed no change.

Tests: `dump_ast_generics_syntax` pins the tree of each parsed form,
`listing_generics.types` the types the checker gives with the inferred
arguments. `listing_error_generics`, `listing_error_generic_syntax` and
`listing_error_generic_copies` pin 29 refusals, and `program_generic_unused`
runs a program whose generics nothing uses.

## Failures and fixes

- The first host run failed 14 tests, logs in
  `build/drive/logs/gen-ctest1.log`. `antl_scale` and the unit tests pinned
  version 60, and eight tests waited on it as a fixture. The hex listing and
  `test_modules.c` now read 61.
- `anti check` and `anti doc` failed because the driver returned under
  `--front-end` before it wrote a library file, which `anti check` asks for.
  The file is written before that return again, after the refusal of a use.
- `emit_identity` had no digests for the new program, and the manifest was
  written on the Mac with `-DWRITE=yes`. Only its six lines were added.
- The docs-style checker found long sentences in the new comments and notes,
  logs in `build/drive/logs/gen-docs.log`, and one banned word in
  `lexer.h`. Each is rewritten.

## Gates

Host 980 passed of 980 (`build/drive/logs/gen-ctest-host.log`), ASan 979 of
979 (`build/drive/logs/gen-ctest-asan.log`), UBSan 979 of 979
(`build/drive/logs/gen-ctest-ubsan.log`). The builds wrote no warning. The
docs-style checker reports nothing on the touched files.

## Provisional decisions

Each stands under "Generics and collections" in `docs/decisions.md`: the
refusal of a use past the front end, no generic, `type` or `constraint` in a
library file yet, constant arguments as literals or names, `N: int` alone,
literals that take their arguments from the context, inference for a static
call on a generic class, no value of a generic function, the hooks of the
built-in types, interface constraints and calls through them, the order of
constraint names, the forms of `type` and `export type`, filled generic
interfaces, the depth limit of nested copies, and `>>` with one list open.
`sema_generic.c` is recorded under "Files of the checker".

## Questions

- `anti check` writes a library file of each imported module, and a library
  file holds IR. A module that uses a copy and that another module imports
  therefore fails `anti check` until copies compile.
- `type Name = T;` takes any type. The specification shows it naming a copy
  alone.

## Proof of the push

The push of the step's last code commit, as `git` printed it:

```text
$ git log --oneline -3
4016098 Report the syntax of generics
7d419bb Split a long sentence of the comment on deep copies
ea85db8 Check the forms of generics a field and a bound function reach
$ git status --short
$ git rev-parse HEAD origin/main
4016098ea54a303dfe3ebfb905a0aa8482dad340
4016098ea54a303dfe3ebfb905a0aa8482dad340
```

Suites: host 980 of 980, ASan 979 of 979, UBSan 979 of 979.
