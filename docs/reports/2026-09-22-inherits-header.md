# The base in the class header

`class Circle inherits Shape { ... }` replaces `inherits Shape,` as the first line of
the body. The Mac ran the suite and both sanitizer suites from `db062fd`.

## Counts

| Host | Run | Suite | ASan | UBSan |
|---|---|---|---|---|
| Mac | `db062fd` | 633 of 633 | 632 of 632 | 632 of 632 |

The logs are `build/drive/logs/test.log`, `asan-test.log` and `ubsan-test.log`. The
builds are in `build.log`, `asan-build.log` and `ubsan-build.log`.

## What changed

1. `34d8dc0`: the parser reads `inherits Base` or `inherits module.Base` after the
   class name and after `align(N)`. `inherits` in the body is refused with
   `inherits belongs in the class header: class Circle inherits Shape`, with the names
   the program wrote. A second base in the header gives `a class has one base`. The
   75 classes of `std/` and `tests/` and the sources in the unit tests moved their
   base to the header. `test_parser` covers the header after `abstract`, `final`,
   `singleton`, `pub`, `packed` and `align(16)`, a qualified base, and the refusal in
   the field list, among the functions and with a qualified base.
2. `b12b7fe`: `programs/inherits_header.anti` runs each modifier, `align(16)` and the
   base `lang.Error` of another module. `errors/inherits_body.anti` pins the refusal
   for a local and a qualified base. The refusal reads and drops the base, so the
   rest of the body parses and a later class reports its own error.
3. `9953361`: the object model, the additions, the syntax overview and the decisions
   write the header form with the reason. The additions carry a section, the message
   and a line under "Timing". The overview's "Built." line stands for the section.
4. `db062fd`: the handover names the header base and the new counts.

## What failed and how it was fixed

- The first suite after the conversion failed 13 tests, all pinned outputs holding
  line numbers: eight error listings, four `devirt` dumps and `emit_identity`. The
  conversion script recorded the old and new line of every line, and the listings
  were rewritten from that map. The `devirt` dumps differ in one byte, `48` to `47`
  in `devirt.anti:48: overflow in +`. Five programs changed their digests. Their old
  sources with the base moved and the line kept blank compile to the old digests on
  all six targets, 30 of 30, so line numbers are the whole difference.
- The first refusal returned from the class, and the item sync stopped inside the
  body at `pub`, which added `expected an item`. Reading past the base fixed it.

## Entries

- New `[provisional]` under "Object model": `align(N)` of a class stands directly
  after the name and `inherits` follows it. The specifications do not order the two.
- New entry under "Object model": `inherits` stands in the class header, with the
  reason.

## Findings

- `tools/scripts/format_anti.py --check` flags every file with a class, before this
  change as well: it does not know `class` and indents the body as a statement.
  `programs/super_call.anti` at `c2e6817` fails it the same way.
- The sanitizer builds print `ld: warning: ignoring -lto_library`, since the pinned
  clang has no `libLTO.dylib`. The same lines stand in the logs of earlier sessions.
  No compiler warning appears.

## State

```console
$ git log --oneline -3
db062fd Record the header base and the suite counts in the handover
9953361 Document the base in the class header
b12b7fe Test the header base with each modifier and the refused body form
$ git status --short
$ git rev-parse HEAD origin/main
db062fd324b5070aa40c99ece9cdf23f2e8dcefc
db062fd324b5070aa40c99ece9cdf23f2e8dcefc
```

The commit of this report follows `db062fd`.
