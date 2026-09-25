# Readable names of the copies of generics

A symbol of a copy escapes each byte an assembler cannot read as `$` and
two hex digits, so a trace named `List$3cint$3e.push`. Every place a person
reads a name now gives `List<int>.push`.

## What was done

- `anti_rt_symbol_unescape` in `src/rt/symbols.c` reads the escapes back.
  `StackTrace.symbolize` calls it on every target. So do `anti symbols
  resolve` and the map of a symbols archive.
- The reverse cannot misread a name. antic escapes `$` itself as `$24`, so
  every `$` of an escaped name starts an escape, and one name gives one
  symbol. A name is read back only where it holds letters, digits, `_`,
  `.` and `$` alone, and every `$` stands before two lowercase hex digits.
  Those digits must give a byte the escape writes. The name must also hold
  one escape and a `.`. A C symbol holds no `.`, and a bare `$` breaks
  the rule, so either stays as it is. `docs/notes/symbols.md` holds it.
- A line of the map may hold a name with a blank, `Pair<int, str>.swap`.
  Its location ends the line as `<file>:<line>`, and the reader takes the
  rest as the name.
- The DWARF unit holds an entry per function with the readable name and
  its range. The CodeView record of COFF named the readable name already.
  The symbol table keeps the escaped name.

## What failed and how it was fixed

- lldb read no name from the new entries. It makes a function of an entry
  only through the type system of the unit's language. It has none for the
  code of an assembler, 0x8001. A DESIGN comment chose that code, so I
  asked. Eddie chose C99, 0x0c. The decision and the note say so.
- On Mach-O the unit covered its first function alone. Its length is a
  difference of two labels in two atoms, which llvm-mc writes as a pair of
  relocations that lldb does not apply. The unit's end is now an address on
  Mach-O. lldb now gives a line for every frame, where it gave one for the
  frame that stopped, and `debug_info` checks that for both debuggers.
- A new runtime changed the pinned digest of `return42` for macos-arm64,
  which was written again from this Mac.

## Tests

- `test_symbols` reads escaped names back and refuses every form that is
  no escape. It also takes names through `mangle` and back, one with `$`
  among them. `test_syms` reads a map line whose name holds a blank.
- `trace_copies`, `trace_copies_g` and `trace_copies_dev` fail in a copy
  and print `copies.Box<int>.check` and `copies.through<int>`.
- `anti_symbols` resolves a frame of `through<float, str>` and reads the
  name from the map.
- `debug_info` stops in `app.twice<int>` under lldb. `asm_debug_copies_`
  checks the readable name in the debug information and the escaped
  symbol on all six targets.

## Provisional decisions

- The rules for reading a name back, above.
- The form of a line of the map with a name that holds a blank.

## Gates

No warning in the host, ASan and UBSan builds. The host suite passes 1002
of 1002, ASan 1001 of 1001 and UBSan 1001 of 1001, without `no_paths`. The
logs are `build/logs/t8.log`, `build/logs/asan-test2.log` and
`build/logs/ubsan-test2.log`. The docs-style checker reports nothing on
every file touched but `tests/CMakeLists.txt`, which it reads as prose.

## Proof of the push

Taken after the push of the code and the report.

```text
$ git log --oneline -3
39a0711 Report the readable names of the copies of generics
a9fb975 Show copies of generics by their readable names
5b2db9a Add the proof of the push to the report of the compilation of generics
$ git status --short
$ git rev-parse HEAD origin/main
39a07115d5d8cb064be2c5d9a4431f0aeeaff0c0
39a07115d5d8cb064be2c5d9a4431f0aeeaff0c0
```

Suites: host 1002 of 1002, ASan 1001 of 1001, UBSan 1001 of 1001.
