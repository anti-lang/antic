# Direct imports

Choices made for "Direct imports" of round five in each pass. They
describe the inside of the compiler and of `anti fmt`. `docs/decisions.md`
holds what a reader of the language or a user of the tools can observe,
under "Generics and collections" and "The formatter".

- `struct import` of `ast.h` carries `names`, one `struct import_name` per listed name with its position, in the order written. `module_path` of `parser.c` stops before a `.` that `{` follows, and `import_names` reads the list after it. A list takes no `as`, so the parser asks for `;` after the `}`.
- `declare_direct_names` of `sema.c` runs after `declare_items`, so every item of the module is in the module scope when a listed name looks for a clash. `scope_put` puts the name with the symbol of the library, which `sema_library_item` gives for `module.Name` too. No symbol of its own is made. A clash is told apart by the symbol already held: a `SYMBOL_MODULE` is an import, `sema_direct_item` is a name of another list, and anything else is an item of the module, whose position names its line.
- A bare name then resolves as before. `EXPR_NAME` finds the symbol of the library, which is the node `check_qualified` makes of `module.Name`. `TYPEX_NAMED` finds a `SYMBOL_STRUCT` whose type is the library's, a generic among them, and makes its copy with `sema_copy_of` as the qualified form does. Lowering and the tree of a generic in a library file read the symbol and its `home`, so neither has a case of its own.
- `sema_method_symbol` of `sema_call.c` skips a symbol that `sema_direct_item` marks, so `v.f()` on a type of the module never reaches a listed function of another module.
- The backtick names of a doc comment resolve against the items of every loaded library already, so a listed name needs nothing there.
- `sort_import_lists` of `src/anti/fmt.c` runs after `pair_braces`. It marks the `{` and the `}` after `import` and a path, and sorts the names between them with an insertion sort that trades the token, the offset and the length of two pieces. The line fields stay with the place, which keeps the author's breaks. `emit_token` takes a marked brace as a bracket, so a comma inside ends no statement and a line after a break takes one tab. `space_before` writes no space inside the braces.
- The tests: `direct_modules_release` and `direct_modules_dev` over `tests/modules/direct/`, where `frame` names items of `shapes` in a generic as well, `std_direct_imports`, `listing_error_direct_imports`, the three parser cases of `test_parser.c` and the lists of `tests/fmt/loose.anti`.
