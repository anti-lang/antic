# Nested types

The step builds "Nested types" of `docs/anti-language-additions.md`. A class
body declares a `struct`, an `enum` or a `class`, private to the class, named
`PeopleList.Node` in the symbols and `PeopleList_Node` in the C header, and
refused in a public signature of the class. The old rule that every type is
declared at module level was reversed on 2026-09-23. The documents already
said so, and this step builds it.

## What changed

- The parser reads a nested type anywhere among the fields and functions of a
  class body. It adds the type to the items of the module under its full
  name, before the class. A visibility word, a union and a variant are refused.
- The checker resolves the name as written in the body of the class and of
  every type nested in it, innermost first. The field `within` of the checker
  says whose body is checked, and `sema_module_find` replaces the module
  scope lookups that name a type.
- A `pub` or `protected` function or field, a `pub` constant and a
  `construct` with arguments that name a nested type of the class are refused.
- An export class carries the nested types its layout reaches into the
  header, with their layout alone, and their fields follow the export rule.
  The header writes an enum as the C enum of its values, where it wrote a
  struct of them before.
- Tests: `program_nested_types` on ARM64, through Rosetta and in the emit
  manifest, `nested_modules_release` and `nested_modules_dev`, `clib_nested`
  with the header in `tests/dump/nested.h` and a C++17 check,
  `anti_bind_header` on the same library file, and the error listings
  `nested_public`, `nested_outside`, `nested_forms` and `nested_export`.
- `docs/decisions.md` has the section "Nested types", `docs/notes/nested-types.md`
  the choices of the passes, and the syntax overview marks the feature built
  and compiles its example.

## What failed and how it was fixed

- A class default that named a nested struct found no fields, because the
  nested types stood after the class. They now stand before it.
- The first full suite failed `emit_identity` alone, since the new program had
  no entry in the manifest. The manifest was written with `-DWRITE=yes` and
  gained the six lines of `nested_types`.
- The style checker reported nine long sentences in new comments, and one
  comment of mine had split `check_types` from its own comment. Both fixed.

Logs: `build/drive/logs/nested-host-suite2.log`,
`build/drive/logs/nested-asan-suite.log`,
`build/drive/logs/nested-ubsan-suite.log`, `build/drive/logs/nested-docs.log`.

## Provisional entries

All under "Nested types" in `docs/decisions.md`:

- no visibility word on a nested type, and no nested union or variant;
- the name as written resolves in the class and every type nested in it,
  innermost first, and a nested class may nest further, `A.B.C`;
- the members of a nested type keep the ordinary levels;
- a public signature is a `pub` or `protected` function or field, a `pub`
  constant and a `construct` with arguments;
- the header name `PeopleList_Node`, and the nested types an export class's
  layout reaches cross with it, layout alone;
- the header writes an enum as the C enum of its values, and a field of one as
  its underlying integer.

## Questions

- A class literal as the default of a field, `count: Counter = Counter { }`,
  damages the library file of a module whose public class holds the field. It
  fails the same way without nested types, so this step left it alone, and
  `tests/modules/nested` takes the implicit `T { }` default instead. Should it
  be the next fix?
- The header wrote an export enum as a struct of its values, which no C
  compiler accepts. The fix came with the view of a nested enum. Is the form
  of the provisional entry the one wanted?
