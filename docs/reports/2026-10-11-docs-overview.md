# The syntax overview rewritten after the gaps report

Date: 2026-10-11. Branch `docs`, from `acb12684`. The step follows the
section plan of `docs/reports/2026-10-11-docs-overview-gaps.md`.

## What the step wrote

`docs/anti-syntax-overview.md` now has 45 sections. Direct imports stands
under Modules, Later is gone, and Processor levels and Tools are new. Every
status line that named a test file is gone. A section whose features are all
built ends without a status line, and one with a gap ends with one sentence
that opens with "Not built yet". The Reserved words section is the keyword
table of `src/antic/lexer.c`: the keywords with the type names and the 14 C
types, `thread` as the one reserved word, the refusal of `null`, the
contextual words with `inject final` and the two `link` lines, the tokens and
the built-ins known by name. Standard library is a table of the 23 modules and
the 12 collection modules with their public items and the functions that may
fail, which takes the list of failing functions out of Errors. Warnings and
safety checks lists the 14 warnings and the 2 safety checks of
`src/antic/warnings.c`. Operators has a precedence table from
`parser_precedence` of `src/antic/parser_expr.c`. Tools lists the twelve
commands of `anti` and the options of `antic` the page names, which the gaps
report left to Eddie. The plan put the section in, so it is there, and taking
it out is one deletion.

`tests/run_overview_examples.cmake` builds and runs every block that declares
`main`, and compares what it prints with the `text` block after the example
through `program_expect` of `tests/program_output.cmake`, so the raw-bytes
rule holds. Its last line now reads `55 blocks compiled, 1 programs run, 9
skipped as not built`. A wrong expected text was tried once and failed the run.

## Examples added

- Program structure: the `text` block with the output of the hello program.
- Types: `.len`, `s[i]`, `s[lo..hi]`, `[]int { ptr: s.ptr, len: s.len }` and `??int`.
- Literals: `1.0e9`, `'q'`, `'\n'`, `br#"..."#`, `[0; 16]` and `[1, 2]`.
- Variables and constants: an atomic local, `let (lo, hi) = (1, 2);` and a `const` of a class body read as `Limits.MAX`.
- Operators: `-a`, `&a`, `*addr` and `shape is Shape.Circle`.
- Statements: `if let` with `else if let Circle c = shape`, `*slot = f() catch { yield 0; }` and `f() catch { yield; }`.
- Structs: a packed struct with bitfields and `_: u32 : 0`, a union and its literal, `size_of(Rect)` and `lang.SourceLocation { }`.
- Enums: `2 as Kind` and `!=`.
- Generics: `Pair<int, str> { }` and `Result<int, str>.Ok { }`.
- Reflection: `reflect.name(d)` and `reflect.parent(d)`.
- C interop: `export union`, `export const` and `link linux "X11";`.

The not-built blocks are the nine of before. No example was marked as not
built to pass.

## Wrong in an older document

- `docs/notes/warnings.md`, under "Safety checks": "Neither check is built
  yet." Both are built, `unguarded-field` with concurrent classes and
  `exponential-pattern` with pattern literals, as the tests of both run.
- `docs/reports/2026-10-11-docs-overview-gaps.md`, under "Features the
  overview lacks": "the six comparisons apply" to an enum. The checker refuses
  `k < m` on an enum with "`<` needs numeric, `char` or `str` operands", so
  `==` and `!=` alone apply, and the overview says so.
- `docs/decisions.md`, under "`anti.mem`": "The containers of
  `anti.collection` do not take an `Allocator` yet." Every collection takes
  an optional allocator when it is made, as the entries under "Generics and
  collections" say.
- `CLAUDE.md`, under "State": "x86-64-v3 on both x86_64 targets". There are
  three, and `antic --print-cpu-levels` gives `v3` to linux-x86_64,
  macos-x86_64 and windows-x86_64.

## Questions answered from the code alone

- Does `export fn` in a class body need `pub`? No. `parser_decl.c` sets the
  member's `pub` from `export`, so `export` implies it.
- Do `packed` and `align(N)` apply to a class, a union and a variant? Yes. A
  module with `packed class`, `class W align(16)`, `packed variant T align(8)`
  and `packed union B align(4)` passes the front end.
- Does the slice literal take the `?*T` of `.ptr`? Yes.
- Does `??T` take `none`? Yes.
- Is a `const` of a class body reached as `Name.NAME`? Yes.
- Does `--trace writes` differ from a pattern? `writes` is the one pattern word
  that sets the writes flag, `src/antic/main.c`.
- Which options does `antic` parse that its usage text omits? `--checks`,
  `--no-checks`, `--asserts`, `--no-asserts`, `--trace`, `--no-trace`,
  `--no-hooks`, `--no-reflect` and `--help`.
- What does `anti.simd` declare? Nothing. The compiler knows `select`, `any`
  and `all` by name.

Two questions of the gaps report stay open for Eddie. Whether Tools belongs in
the overview, and whether the functions and constants an enum body accepts
are a feature.

## Gates

`overview_examples` passes. The host suite: 1701 tests, 100 percent passed,
with `mimalloc_environment_linux-x86_64` and `mimalloc_environment_linux-arm64`
skipped as on every Mac. No file of C under `tests/` and no
`tests/CMakeLists.txt` changed, so the sanitizer suites did not run. The
docs-style checker reports nothing on the overview and on this report. A
`git grep` for the three marker words the gates name finds none in either.
