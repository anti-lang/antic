# anti bind: built

The step resolved the block of `2026-09-23-anti-bind.md` with Eddie's
decision and built `anti bind` from `docs/tooling-addendum.md`.

## What changed

- `docs/decisions.md` and `docs/tooling-addendum.md` record the decision:
  `anti bind --clang` runs clang with `-Xclang -ast-dump=json
  -fsyntax-only` and reads the JSON. It takes the pinned clang in a
  development tree and the first clang on `PATH` elsewhere, and refuses a
  major version it was not tested against. The installers ship no clang,
  and antic computes every layout.
- The JSON scanner of `Object.deserialize` moved to `rt/json.c`, at the
  level of tokens and positions. `Object.deserialize` reads through it as
  before, and `anti` links it. The unit test `test_json` feeds it
  truncated input, unterminated strings, bad escapes, deep nesting and
  numbers out of range. `docs/c-guidelines.md` does not exist, so the
  scanner follows the principle Eddie named: every position is checked
  before a read, and malformed input is refused without an abort.
- `anti bind --header <name>.antl` writes the header of `antic --lib` from
  the library file alone. `anti_bind_header` compares eight of them with
  `tests/dump`.
- `link framework "Name";` is built. The library file records the names,
  its format version is 52, and `anti build` and `anti test` pass them to
  antic as `--framework`. `anti_build_framework` links CoreFoundation
  through a binding and a program that names no framework.
- `anti bind <api.json>` and `anti bind --clang <header>` are built:
  unions, bitfields, packed and aligned structs, enums, function pointers,
  macro constants, the table of frameworks, the shim and `--probe`.
  raymath of the pinned raylib binds, its two probes agree, and the calls
  of `abi_raymath.anti` through the generated binding and its shim print
  `abi_raymath.expected`. The refusal of clang 19 is tested with a script
  that stands in for clang on `PATH`.

## Commits

`b00458e` the scanner, `a52c0ed` the decision, `5922eea` `--header`,
`c8807dc` `link framework`, `61ab757` `anti bind`, `c15e899` a fix, and
this report. AddressSanitizer found a read of freed memory in
`anti_bind_api`: a constant of an enum kept the names of the enum inside
the tree of the reader. The constant keeps copies now.

## Provisional entries

The section "The bind command" of the decisions holds fourteen. They name
the accepted clang major 23 and where the pinned clang stands. They name the
target, the sysroot and the module and file names. They give the rules for
enums, pointers, typedefs and names that are words of Anti. They cover pack
and align, the two forms of the shim, and the macros and the defines of
rlparser. They fix the table of frameworks, `volatile` and the formatter
over the output. One stands under "SDK and frameworks": antic alone takes no
framework from a library file.

## Questions for Eddie

- The `raylib_api.json` of the pinned raylib 6.0 is not JSON. Line 4699,
  the description of `LoadDirectoryFilesEx`, holds unescaped quotes.
  `anti bind` refuses it, with the line and the column, so `anti.raylib`
  cannot come from that file as it stands. Bind it from `raylib.h` with
  `--clang`, repair the file after the download, or regenerate it with
  rlparser?
- `docs/tooling-addendum.md` says a C enum becomes `const` values of
  `c_int`. `docs/anti-object-model.md` says `anti bind` emits an Anti
  enum. The binding follows the object model, which stands higher in the
  order of authority. Is that the intended reading?
- `.gitignore` ignores `build-*/`, which also matches
  `docs/site/build-tool/`. The page there is untracked, so its edit for
  the decision stays on this machine. Should the pattern be narrowed?

## State

Host suite: 780 tests, all pass. ASan and UBSan: 779 each, all pass,
without `no_paths`, which needs a build without a sanitizer. The three
lines of proof stand in the final reply of the session.
