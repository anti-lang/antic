# Modules and library files

Choices made for modules and library files. They describe the inside of the compiler.
`docs/decisions.md` holds what a reader of the language or a user of the
tools can observe.

- Library format: magic `ANTL`, the version, little-endian integers of fixed width, strings as a u32 length and bytes, floats as IEEE 754 bits. Enum values are stored as bytes, and `_Static_assert` checks guard them. All function signatures precede all bodies. `put_inst` of `antl_write.c` writes an instruction as a record of 49 bytes: the operation, the type, the line, the result, three operands of 10 bytes each, the type of an access and the field. A load or a store of an aggregate adds the member, and the count of the arguments and the arguments of 10 bytes each follow.
- An IR function of another module is an `extern fn` with a module name, as in `extern fn scale.scale(i64) -> i64`. Loading maps it to the loaded definition.
- The reader checks every count against the bytes left and every index against its table. A damaged file reports `is damaged at byte N`. Other messages: `is not a library file`, `has format version N, and antic reads version M`, `needs module`, `needs struct`, `needs module of M.T`, `needs generic M.G` and `defines M.F, which another library defines`.
- Import errors: `` `main` cannot import itself ``, `` `b` depends on `main`, so the import forms a cycle ``, `` `geometry` has no public item `x` ``, `` `geometry` has no public struct `X` ``, `` `geometry` is a module, not a value ``.
- The type table, the externs and the trees of the generics are described in "Library files" of `docs/notes/generics.md`.
- `antl_verify.c` holds the verifier of library files. It holds each tree the reader built to the rules the checker makes true of a tree, and a tree that breaks one is damaged. The DESIGN at its start lists the rules. "Sanitizers" in `docs/notes/hosts-and-harness.md` gives the stack its walk takes.
