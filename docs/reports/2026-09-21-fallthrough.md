# Fallthrough

`fallthrough;` from "Small items, round three" of
`docs/anti-language-additions.md`. It is built, apart from its refusal into an
arm that binds a variant's fields. Variants are not built, so no such arm
exists yet.

## What the rule asks, and where it stands

| Rule | State |
|---|---|
| `fallthrough;` as the last statement of an arm enters the next arm's body without its test | Built, `program_switch_fallthrough` |
| Refused in the last arm, `` `fallthrough` in the last arm `` | Built, `listing_error_switch_fallthrough` |
| Refused anywhere else in an arm, or outside a `switch` | Built, the same listing |
| Refused into an arm that binds a variant's fields | Waits for variants. The overview says so under "Variants" |

## What changed

- `fallthrough` is a keyword. The token table grew, so `ANTL_VERSION` is 34,
  and `tests/unit/test_modules.c` and the entry in `docs/decisions.md` follow.
- The parser keeps the position the text gives `else` among the arms,
  `otherwise_at`, and reads `fallthrough;` as a statement of its own.
- The checker reads the arms in the order of the text, `else` in its place.
  It allows the one `fallthrough;` that ends each arm's block and refuses
  every other one: in the last arm, in a nested block, an `if`, a loop, a
  `defer`, and outside a `switch`. The check that a `construct` sets every
  field leaves an arm that falls through out of the paths that reach the end
  of the `switch`.
- Lowering keeps the last block of an arm that falls through open, and once
  every arm stands it jumps to the first block of the next arm in the text.
  The arm's block has closed by then, so its `defer` runs first. A `switch`
  without `fallthrough;` lowers as before, and no digest of
  `tests/emit-identity/programs.sha256` moved. The six lines of the new
  program were added, written on the Mac.
- `--dump-ast` prints `fallthrough`, `dump_ast_switch_fallthrough`.
- The syntax overview marks `fallthrough` built, and `docs/decisions.md`
  says the core `switch` has no fallthrough unless an arm ends in
  `fallthrough;`.

## Failures on the way

- The error listing was first named `fallthrough.anti`. antic refused it,
  since the module name comes from the file name and is now a keyword. The
  three test files are named `switch_fallthrough`.
- The build stopped at the static assertion on `TOKEN_KIND_COUNT` in
  `src/antl.c`, which asks for the version raise above.
- The first full run failed `antl_scale`, and eight tests that depend on it
  did not run. `tests/modules/scale.antl.hex` pins the bytes of a library
  file, and the version byte was the one that differed. It is 34 now, and the
  second run passed 556 of 556.
- Two probes proved the tests reach the code: with the construct change
  reverted, `program_switch_fallthrough` fails with `` `construct` of `Tally`
  returns `none` before it sets `v` ``. With `else` checked last, as before,
  the listing loses the `none` access in `narrowed`.
- The docs-style checker reads `tests/CMakeLists.txt` as Markdown and finds
  144 errors in it before this change, every comment line as a heading. The
  two comment lines added there were read by hand against the rules.

## Provisional entries

Under "Fallthrough" in `docs/decisions.md`:

- The arm `fallthrough;` enters is the next one in the text, `else` included
  wherever it stands, and the checker reads the arms in that order.
- `fallthrough;` stands in an arm written as a block. An arm of one call or
  one assignment has no room for it.

## Gates, at `157c97a`

- Build: `build/drive/logs/build.log`, no warning apart from the known
  `ld: warning: ignoring -lto_library` of the pinned clang.
- Host suite: `build/drive/logs/test.log`.
- Sanitizers: `build/drive/logs/asan-test.log` and
  `build/drive/logs/ubsan-test.log`.
- Docs style: `build/drive/logs/docs-style-src.log`.

## Seen on the way, not changed

`walk_stmt` in `src/sema.c`, the walk over every function a worker reaches,
visits the value arms of a `switch` and not its `else` arm. A `delete` in an
`else` arm of such a function is therefore not reported.
