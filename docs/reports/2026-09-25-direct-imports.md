# Direct imports

"Direct imports" of round five is built. `import anti.text.{Builder, equal};`
makes the listed items of a module visible without its name, and the module
stays reachable by its name. A listed name that clashes with a name of the file
is refused, naming both, and `anti fmt` keeps the list sorted.

## What was done

- The parser reads the list after the path of an import into `names` of
  `struct import`. An empty list and a list with `as` are refused.
- The checker puts each listed item into the module scope under its own name,
  after the items of the module. The entry holds the symbol of the library
  that `text.Builder` resolves to. A use of the bare name then reads the same
  item as the qualified one. That holds in values, types, generics, `inherits`,
  variants, enums and static functions. A generic of a library file whose body names
  items of a list is copied in the program as any other.
- A clash is refused at the listed name and names both. An item of the module
  and an imported module are named with their line. A name of another list is
  named with its module, and a name listed twice has a message of its own. A name the module does not offer is
  refused with `` `anti.mem` has no public item `Block` ``.
- `v.f(args)` on a type of the module does not reach a listed function.
- `anti fmt` sorts the names in the order of their bytes, writes no space
  inside the braces and keeps the author's line breaks in place.

## Tests

- `direct_modules_release` and `direct_modules_dev` over
  `tests/modules/direct/`. The program lists a struct, a constant, functions,
  an enum, a variant, a class and its static function, an interface it
  inherits, a generic struct, a generic class, a generic function, a
  constraint and a `type`, and reaches both modules by name too. The library
  `frame` lists items of `shapes` inside generics that the program copies.
- `std_direct_imports` lists items of `anti.io` and `anti.text`, with `try` on
  a listed `may fail` function and `f"..."` beside the list.
- `listing_error_direct_imports` holds every refusal above and shows that a
  name no list holds stays behind its module.
- Three parser cases in `test_parser.c`, and two lists in the fixture of
  `anti_fmt`, one of them over two lines.

## What failed and how it was fixed

- The first client wrote `return` in a `switch` arm, which the language does
  not allow. The arm now assigns.
- The first `.expected` of the standard library test lacked its `exit 0` line.
- `fmt_canonical` found the doc comment of the error test in another wrap. The
  file is in the canonical form now.
- The docs-style checker found three long sentences in the new comments and the
  decisions, which are split now.
- Logs: `build/drive/logs/di-host-full.log`, `di-asan-test.log`,
  `di-ubsan-test.log`, `di-t1.log` to `di-t5.log` and `di-docs2.log`.

## Provisional decisions

All in `docs/decisions.md`, under "Generics and collections" and "The
formatter": a list names what `module.Name` reaches, a listed name stands
behind no name the compiler declares, a module is imported once and a list
takes no `as`, a method call does not reach a listed function, and the sort
order of `anti fmt` with the author's breaks.

## Questions for Eddie

- "Direct imports" writes `import anti.regex.{Regex};` as an example. The
  provisional entry of "Regular expressions" declares `Regex` in `anti.lang`,
  so `anti.regex` offers no `Regex`, and the import is refused. Should
  `anti.regex` offer the name, or should the example change?
- Should `import anti.text;` beside `import anti.text.{equal};` be allowed?

## Gates

No compile warning on the host, ASan and UBSan builds. The linker prints
`ld: warning: ignoring -lto_library`, as before. The host suite passes 1075 of
1075, ASan 1074 of 1074 and UBSan 1074 of 1074. The docs-style checker reports
nothing on every touched file but `tests/CMakeLists.txt`, whose `#` comments it
reads as Markdown headings, as before.

## Proof of the push

After the push of `5827674`:

```text
$ git log --oneline -3
5827674 Report direct imports
b68714e Record direct imports in the decisions and the overview
f4f2040 Build direct imports of round five
$ git status --short
$ git rev-parse HEAD origin/main
5827674607af50c02d8cd7695a0edca6a45e5e33
5827674607af50c02d8cd7695a0edca6a45e5e33
```

Suite pass counts: host 1075 of 1075, ASan 1074 of 1074, UBSan 1074 of 1074.
