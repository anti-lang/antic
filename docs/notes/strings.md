# Strings, slices and bytes

Choices made for strings, slices and bytes. They describe the inside of the compiler.
`docs/decisions.md` holds what a reader of the language or a user of the
tools can observe.

- Chapter 19 lowers `str` and `[]T` as aggregates of 16 bytes, `ptr` at offset 0 and `len` at offset 8. `.ptr` and `.len` load a field. `s[i]` loads the pointer and indexes from it. `a[lo..hi]` stores the address of element `lo` and `hi - lo`, after evaluating the base, `lo` and `hi` in that order. A slice literal stores its fields in source order.
- A string literal and a byte string literal become a global of the module that holds the bytes and a NUL after them. The global is named by its decimal index among the module's globals, as `main.0` or `_A4main_0` on COFF. No identifier starts with a digit, so no function gets that name. Literals and `str` constants with equal bytes share one global.
- The LLVM text writes a global as a packed struct of its bytes, with the offsets `layout_data` computed. A global that the program never writes is a `constant`, which llc puts in the read-only section of each format: `.rodata` on ELF, `__TEXT,__const` on Mach-O and `.rdata` on COFF.
- The former Open item on System V array variables does not apply. antic's globals hold literal bytes under local symbols, and no C file can declare them.
- A program test reads the lines of an optional `NAME.args` beside its source as command-line arguments. The test `program_args` sets `ANTI_TEST_GREETING`.
