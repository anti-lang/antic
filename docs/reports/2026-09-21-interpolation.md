# Interpolation `f"..."` and `rf"..."`

The task: finish step 15 from the working tree an earlier session left
uncommitted, to the format specifications of "Small things" and the `rf`
item of "Small items, round three" in `docs/anti-language-additions.md`.
It covers `{expr}`, `{{` and `}}`, a specification after a colon, an unknown
one refused, `rf"..."` raw and `fr` refused naming `rf`. Built in `4732a1c`
and `4a77f77`.

## What this session did

- Read the uncommitted change against the specification. Every item was
  built and tested. No behaviour changed.
- Fixed six findings of the docs-style checker in the new comments of
  `rt/text.c`, `src/ast.h`, `src/lexer.c`, `src/lower.c`, `src/parser.c`
  and `tests/unit/test_lexer.c`.
- Split the work into two commits. `4732a1c` holds `anti.text` and
  `rt/text.c`, and `4a77f77` holds the compiler, its tests and the
  documents. The emit manifest of `4732a1c` leaves out the six digests of
  `format_strings`, which its tree does not hold yet.

## What the two commits build

- `anti.text.Builder` gains `append_int`, `append_uint`, `append_float`,
  `append_f32`, `append_bool`, `append_char`, `append_text` and `take`, with
  `Align { Left, Right, Center }`. A program calls them as well, with the
  defaults of the parameters of a specification.
- The lexer reads `f`, `rf` and `fr` from the one prefix table. It finds
  the closing quote first and lexes each `{expr}` where it stands.
- The parser reads `[align][0][width][.precision][kind]` with the kinds `x`,
  `X`, `b`, `o`, `e` and `f`, and refuses any other text with
  `` unknown format `{x:q}` ``.
- The checker writes the literal as calls on a local `Builder` and checks
  them as a program's calls. A specification that does not fit the type of
  the value is refused with `` unknown format `{n:.2}` for `int` ``.
- The library format moves to version 35 for `TOKEN_FORMAT`.

## Tests

- `program_format_strings` runs every form, `rf"..."` and `rf#"..."#`
  among them. `std_format` calls the functions of `Builder` directly, and
  `dump_ast_format` pins the node.
- The listings `format_literal`, `format_spec`, `format_types` and
  `format_no_import` hold the messages. The unit tests of the lexer read the
  pieces of 15 literals and check 13 errors, `fr#"..."#` among them.

## Gates

Each suite ran at `4a77f77` after `cmake --fresh` and a build with
`--clean-first`:

```text
build        100% tests passed out of 567
build-asan   100% tests passed out of 566
build-ubsan  100% tests passed out of 566
```

No AddressSanitizer or UBSan report. No compiler warning. ld prints its
five notes about the missing `libLTO.dylib`, as before. `4732a1c` alone
passed 560 of 560 on the host the same way. The docs-style checker reports
nothing on every touched file but `tests/CMakeLists.txt`, which it reads as
Markdown. Logs under `build/drive/logs/`, `gate-*`.

## Not done

- No Windows or Linux machine ran the program. `emit_identity` pins the
  assembly of all six targets. That a float reads the same on each rests
  on the C library of each target. The VMs of `docs/vm-setup.md` did not
  run in this session.
- The earlier session left no report, so the questions of
  `docs/reports/2026-09-21-string-prefixes.md` were answered by the gap
  procedure.

## Questions for Eddie

The provisional entries under "Interpolation" and "String prefixes" in
`docs/decisions.md` await review: where an expression ends, the grammar of
a specification, the kinds per type, the alignment, the float without a
precision, the types a literal writes, the `Builder` calls, the memory that
`free(s.ptr)` returns, the required import of `anti.text` and the
reading of `fr#"..."#`.
