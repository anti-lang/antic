# Language hooks and iteration

The step builds "Language hooks" and "Iteration" of
`docs/anti-language-additions.md`. The operator table takes `iter`, `next`,
`value`, `index` and `set_index` and checks each one. `for x in e` walks a
collection and an iterator, `while` calls the hooks by name, `e[i]` and
`e[i] = v` call `index` and `set_index`, and every iterator has `to_slice`.

## What changed

- The checker holds the nineteen names. A misspelled name lists them, and a
  hook with a wrong signature, one that may fail among them, is refused with
  the right signature. A free `operator fn` of a module is checked as well,
  which it was not before.
- `sema_iterate` binds the iterator to a hidden local and checks the calls
  `next()` and `value()` on it. `for` and `to_slice` share it. `e[i]` becomes
  `e.index(i)`, and `e[i] = v` becomes the statement `e.set_index(i, v);`.
- Lowering: `lower_for_hooks` is the loop of the `while` form, with the
  teardown of the iterator after the loop and of the value after each pass.
  `lower_collect` writes the values into memory from `realloc`.
- Tests: `program_iteration` on ARM64, through Rosetta and in the emit
  manifest. It covers a collection in `for` and `while`, nested loops over one
  collection, `break` and `continue`, an iterator walked in its place, a `str`
  element, a struct with free hooks, `index` and `set_index` on a value and
  through a pointer, and `to_slice`. `iteration_modules_release` and
  `iteration_modules_dev` take a collection from a library. The error listing
  `hooks` covers the misspelled and mis-signed hooks and the refused forms.
- `docs/decisions.md` has the section "Language hooks and iteration",
  `docs/notes/iteration.md` the choices of the passes, and the syntax overview
  marks both features built and compiles its example.

## What failed and how it was fixed

- A diagnostic holds 160 bytes, and the list of nineteen names in backticks
  was cut. The list now stands in one pair of backticks.
- Dev mode failed IR verification: a store took `l->b` and a call that ends
  its block in one argument list. The value is now bound first, as the rule in
  `CLAUDE.md` asks.
- I first put the module test into `tests/modules/hooks`, which the tracing
  test already uses, and overwrote its client and expected output. I restored
  both from git before any commit, and the test now lives in
  `tests/modules/iteration`.
- `fmt_canonical` failed. `anti fmt` reads `for x in (e)` as a call of `in`
  and indents what follows. The tests now walk iterators that calls give.
- Two long sentences in new comments and one in the decisions, split.

Logs: `build/drive/logs/ctest-host.log`, `build/drive/logs/ctest-asan.log`,
`build/drive/logs/ctest-ubsan.log`, `build/drive/logs/docs2.log`.

## Provisional entries

All under "Language hooks and iteration" in `docs/decisions.md`:

- the two message forms, and the name list in one pair of backticks;
- the five signatures, no hook may fail, and `set_index` takes the types of
  `index`;
- a hook belongs to `T` and to `*T`, so `p[i]` on a pointer to such a type
  calls `index`, and a free function is a struct's hook by its first
  parameter;
- an iterator in a place is walked there, and any other one by a hidden local;
- `for x in &e` and `for i, x in e` over a collection or an iterator are
  refused;
- `e[i] op= v` on a type with `set_index` is refused;
- `to_slice` on every iterator, from `realloc`, freed with `free(s.ptr)`;
- the value of a pass and the iterator of a loop are torn down as `let`s.

## Questions

- A free `operator fn` of a module does not reach the library file, so a
  struct's operators and hooks do not cross a module. That holds for `+` as
  well and predates this step. Should the library file carry them?
- `anti fmt` misformats `for x in (e)`. Should the next formatter fix treat
  `in` after the names of a `for` as a word?
- The host linker notes `ignoring -lto_library` for the pinned clang, which
  has no `libLTO.dylib`. It is no compiler warning and predates this step.
- The style checker reads every `#` comment of `tests/CMakeLists.txt` as a
  heading, 232 findings before this step. The new comment adds one more.

## Proof of the push

The push of `cbb8d5f`, before this section was added:

```text
$ git log --oneline -3
cbb8d5f Report the language hooks
51513b2 Record the language hooks in the documents
094d8b9 Put the iteration tests in the canonical form
$ git status --short
$ git rev-parse HEAD origin/main
cbb8d5fc40636525d104ffda601bea89cec8781e
cbb8d5fc40636525d104ffda601bea89cec8781e
```

Suites: host 869 passed of 869, ASan 868 of 868, UBSan 868 of 868.
