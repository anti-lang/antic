# anti bind: the answers to the three questions

The step carried out Eddie's answers to the questions of
`2026-09-23-anti-bind-built.md`.

## What changed

- `.gitignore` anchors `build-*/` at the root, as `/build-*/`, and
  `docs/site/build-tool/` is tracked with its edit for `--clang`. The line
  `keys/private` did not change, and every file under it stays ignored
  and outside the history.
- A C enum becomes an Anti enum. `docs/tooling-addendum.md` and the
  build-tool page say so, and the entry under "The bind command" is no
  longer provisional. A parameter that takes enum values combined as bit
  flags keeps the integer type of C: raylib's `SetConfigFlags` takes a
  `c_uint`, and the caller converts each value with `as`. "Open" holds
  the item that enums used as bit flags want a form of their own.
- `anti.raylib` is bound from `raylib.h` with `--clang`. The addendum,
  the decisions and the build-tool page name that command. The JSON input
  stays for a valid description, and the refusal of the pinned file with
  its line and column stays in `anti_bind_api`.
- The header gave none of raylib's colours, which it writes as
  `CLITERAL(Color){ 200, 200, 200, 255 }`. A macro that is a compound
  literal of a bound struct now becomes a constant of the struct. A
  leading call of a macro whose body is its one parameter is expanded
  first. With that, the binding of `raylib.h` equals the binding of a
  repaired copy of the JSON, line for line apart from the doc comments.
- The test `anti_bind_raylib` binds `raylib.h`, compiles it as
  `anti.raylib`, checks three lines of it and compares its two probes.
  The probes agree on 171 lines over all 35 structs.

## Commits

`9ab44d4` the ignore pattern and the page, `1154bb5` the enum rule,
`3f41e0e` raylib from its header, and this report.

## Provisional entries

One: the compound literal of a struct, under "The bind command".

## State

Host suite: 781 tests, all pass. ASan and UBSan: 780 each, all pass,
without `no_paths`.
