# The marks of a generic in the library file

A generic `operator fn` read from a library file lost its operator mark. `==`
on a `List<T>` of another module then found no `eq` hook. The mark now
survives, and so do the two other marks the tree dropped.

## The defect

`put_declaration` of `src/antic/antl_tree.c` wrote `may fail` and `worker` for
a generic function and nothing else. The items section wrote the operator
mark on the symbol of a `pub` generic, but `symbol_is_operator` of
`sema_expr.c` reads the mark from the declaration when one stands behind the
symbol, and the declaration the reader built had none. So every module-level
generic `operator fn` of another module was invisible to `==`, `<`, `+`,
`[ ]` and `for`. That is the form `anti.collection` documents for giving a
collection class `==`. A private generic `operator fn` lost the mark on both.

The hooks declared in the body of a generic class were not affected. The
entry of the type carries their operator mark, and a test with those alone
passed before the change.

## The fix

- The declaration of a generic function carries `operator` as bit 2 of its
  marks. The reader sets it on the item and on its symbol, so both places the
  checker reads agree.
- The root of a tree carries `lent` of each parameter. The record of an
  anonymous function already carried it, and the root did not.
- Every expression node carries `prechecked`. The checker sets it on the
  receiver of a nested hash call, `hash(*hole)`, and a copy hands that node to
  the checker again as it stands.
- The library format is version 71. `scale.antl.hex`, `pick.antl.hex` and the
  bytes of `test_modules.c` follow.

## The audit of the other marks

Each record of the tree section was compared against the fields of its
struct.

- The declaration of a class, a struct or a variant carries every mark a copy
  reads. `simd` and `unchecked_fields` stand on the type, which the type
  table carries.
- The functions of a generic class carry their marks in the entry of the
  type, in `antl.c`. `trace` and `lent` of the result travel in the root of
  each tree. A function of a class body cannot be `worker`.
- `put_extern` leaves `worker`, `trace` and the `lent` result off the item it
  builds. The symbol carries `worker`, and nothing reads the other two from a
  function a tree only names.
- The `allow` and `unchecked` clauses stay out of the tree by design, since
  no pass after the checker reads them. The `here` default travels with the
  defaults in `antl.c`.

## Tests

- `generic_hooks_modules_release` and `generic_hooks_modules_dev`
  (`tests/modules/generic_hooks/`): a generic class `Row<T>` with
  `operator fn eq`, `lt`, `add`, `index` and `iter` in its body, and a generic
  class `List<T>` that takes the same five from `pub operator fn` generics of
  its module. The iterator of `List` lends. Another module uses both through
  `==`, `<`, `+`, `[ ]` and `for`, with `int` and `f64`. Before the fix the
  program failed to compile with seven errors, all on `List<int>`.
- `keeps_generic_marks` of `tests/unit/test_modules.c` writes a module to a
  library file, reads it back and checks the three marks on the generics
  read. It failed on `lent` and `prechecked` before their fix.
- `build_library` of `test_modules.c` now runs `sema_strip_generics` before
  lowering, as the driver does. Without it the unit test wrote no generics
  section, and a file with a generic function beside a second item read as
  damaged.

## Gates

- Host: 1116 of 1116 ctest tests pass, and none is skipped. The first full
  run failed `release_dry_run` after 45 seconds and `antl_generic`.
  `antl_generic` compares the bytes of `pick.antl` with a pin, which the new
  fields of the tree change, and the pin was written again. `release_dry_run`
  passed alone and in the second full run. Its failing output was not
  captured, so the cause stays open.
- ASan: 1115 of 1115 pass. UBSan: 1115 of 1115 pass. Both leave out
  `no_paths`, as before.
- The docs-style checker passes on every changed comment and `.md` file,
  apart from the comment in `tests/CMakeLists.txt` under Notes.

## Notes

- The docs-style checker reads `#` comments of `tests/CMakeLists.txt` as
  Markdown headings, so the comment of every test there fails it, the new one
  included. The finding is in the checker's handling of CMake, not in the
  text.
- Every link of a host tool prints `ld: warning: ignoring -lto_library`,
  since the pinned clang in `build/deps/clang` has no `libLTO.dylib`. It is a
  warning of the system linker, which `-Werror` does not reach. It names a
  file of the toolchain in `build/deps` and no source.
