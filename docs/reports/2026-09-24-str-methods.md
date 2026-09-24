# Methods of str, the match and replacement

The step built "Methods of `str`", "The match" and "Replacement" of "Regular expressions"
in `docs/anti-language-additions.md`, with the failure rule by the origin of a pattern.

## What was done

- `s.matches(r)`, `s.find_all(r)`, `s.replace(r, with)` and `s.split(r)` are calls of
  `anti.regex` that `src/antic/sema_pattern.c` writes in their place. A literal at the
  call takes the `_literal` function, which cannot fail and stops at the match limit
  with the pattern and the line. Any other pattern may fail with `TooExpensive`, and a
  template with `MissingGroup`. `limit` works in both directions on `find_all`,
  `replace` and `split`. `find_all` gives `regex.Matches` and `split` gives
  `regex.Pieces`, two iterators that `to_slice()` collects.
- `Match` and `?Match` are structs of `anti.lang`. A match compares with `none`, narrows,
  stands as a condition in `if`, `while`, `&&`, `||` and `!`, and binds with `if let` and
  `let ... else`. A result that is only tested calls `matched`, which asks PCRE2 for no
  groups. The fields are `all`, `pre`, `post`, `count`, `group` and `took_part`, and for
  a literal `m.1` and `m.year`, checked at compile time.
- Templates take `$1`, `$10`, `${1}0`, `${name}` and `$$`. A template written as a
  literal beside a literal is checked at compile time. A function replacement may be
  named, anonymous or a closure.
- `src/rt/patterns.c` holds the walk, the groups found again, the templates and the
  stops. The library format is 59.
- Tests: `programs/pattern_match.anti`, `programs/pattern_walk.anti`,
  `programs/pattern_replace.anti` in release and dev mode, `traps/pattern_limit.anti`,
  and the listings `pattern_methods` and `pattern_import` of `tests/errors/`.

## What failed and how it was fixed

- A nested failing call, `let x = try f(try g(7));`, gave `f` the slot of `g` as its out
  pointer, so `x` kept garbage. The fault predates this step: `lower_call` read the out
  place after it lowered the arguments. It now reads it first, and
  `programs/failing_nested.anti` pins it. Commit `422bcd3`.
- `regex.antl` crashed antic by recursion, since `none` of a match had no lowering. It
  now writes zero into every field.
- A struct takes no `operator fn` in its body and no hook from another module, so the
  iterators became classes.
- The first full run (`build/drive/logs/str-ctest1.log`) failed six tests, and nine that
  depend on them did not run. The version
  pins of `tests/unit/test_modules.c` and `tests/modules/scale.antl.hex` took 59.
  `anti.regex` first imported `anti.text`, which broke the dev link of the pattern
  literals, so `replace` with a function writes through a buffer of the runtime and
  the stops format in C. `anti fmt` rewrapped `regex.anti`. `emit_identity` took the
  four new programs and `regex_compile` with `-DWRITE=yes` on the Mac. No other program
  changed.
- The docs-style checker found 16 long sentences on lines of the touched C files that
  predate this step. They are split.

Logs: `build/drive/logs/str-ctest-host.log`, `str-ctest-asan.log`,
`str-ctest-ubsan.log`, `str-emit-write.log`.

## Provisional entries added

All stand under "Regular expressions" in `docs/decisions.md`: the origin of a pattern at
the call alone, `Match` and `?Match`, the groups found again from the search, the fields
of a literal and the stop of `group` on a compiled pattern, two groups of one name, the
tests of a match and `if let`, `find_all` with a limit and `split` as an iterator, `pre`
and `post` of a walk, the counting of a negative limit, the match limit, UTF-8 and empty
matches, the reading of a template, the memory of `replace`, and the classes `Matches`
and `Pieces`.

## Questions for Eddie

- The list of methods in the additions wrote `split -> []str`, while the example of the
  overview calls `to_slice()` on it and the step asked for iterators. The additions now
  list `split` as an iterator and `find_all` with a limit. Is that the reading you meant?
- A `Regex` in a variable counts as compiled, even when a literal gave it. Should the
  checker follow a local that a literal initialised and nothing assigns?
- A function given to `replace` with a literal takes a plain `Match`, so it reads groups
  by `group`. Should it take the match of the literal?
- `tests/CMakeLists.txt` went from 248 to 250 findings of the checker, which reads each
  CMake comment as a Markdown heading. Every other touched file passes.

## Proof

The three suites at `8007ee0`, the last commit that changed code:

```text
host:  100% tests passed out of 967
asan:  100% tests passed out of 966
ubsan: 100% tests passed out of 966
```
