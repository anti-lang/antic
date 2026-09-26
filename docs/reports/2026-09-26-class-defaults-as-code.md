# The default `==` and hash of a class as code

The step builds Eddie's option 1: the compiler writes each class's default
`==` and hash as code, and the struct descriptor that would carry `eq` and
`hash` for the runtime is recorded as covered by that and not built. Both
stand in `docs/decisions.md`, and the first in
`docs/anti-language-additions.md`.

## What was done

- `14ef440`: `C.equals` and `C.hash` are compiler-written functions for each
  class whose chain declares neither, and the class table names them. They
  read no field list. A field of a struct with its own `==` compares through
  it, since the checker gives each class a checked `==` and `x.hash()` with
  the calls of the operators its fields reach.
- The runtime keeps its field walk for `serialize`, `deserialize` and
  `reflect`. `anti_lang_Object_equals` and `anti_lang_Object_hash` stay, since
  C code calls them by name, and each calls the entry of the object's table.
- Under `--no-reflect` a call of the default `serialize`, of
  `Object.deserialize` and of `anti.reflect` is refused, naming the option.
- Tests: `programs/eq_compiled.anti` in a normal build, under `--no-reflect`
  and for macos-x86_64, and `errors/no_reflect.anti`.

## What failed and how it was fixed

- The table took the root's `equals` item, which the runtime implements, and
  not the compiled function. `main` then called the root, which called the
  table entry, which was the root again, and the program never ended. The
  table now treats an entry the runtime implements as the root's.
- The four devirt dumps, the `emit_identity` manifest and the digest of
  `return42` changed, since every class now carries two functions of its own.

## Proof

- Host: 1150 of 1150 passed at `14ef440`, in `build/scratch/suite17.log`.
- ASan: 1149 of 1149 passed, in `build/scratch/asan-suite.log`.
- UBSan: 1149 of 1149 passed, in `build/scratch/ubsan-suite.log`.
- The docs-style checker reports nothing over `docs/` and exits 0.

## Open

- A call `x.equals(y)` or `x.hash()` of a class that replaces neither still
  names the root, which forwards through the table. The call reaches the
  compiled function with one step more than a direct call.
