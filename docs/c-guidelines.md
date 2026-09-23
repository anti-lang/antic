# C guidelines

The rules the C code of this repository follows, and the standard the code audit checks against. A finding in an audit report names the rule it breaks by its number.

## Scope and bar

The bar differs by where the code runs.

- `src/rt/` runs inside every program a user builds. It is held to every rule without exception.
- `src/antic/` and `src/anti/` run on the developer's machine and read files the developer did not write. They follow every rule. A threshold in [Structure](#structure) is reported, not enforced.
- `tests/` must be correct and must not hide a failure. The structure and naming rules do not apply.
- `src/std/` is Anti code and is audited against the language's own rules, not these.

## Severity

- Severe: undefined behaviour, a read or write outside an object, a use after free, or a double free. Also any way for malformed input to cause one of these.
- Major: a leak, or an error path that skips cleanup. Also a boundary between parts of the code crossed, or a result that differs between targets.
- Minor: a threshold passed, a naming or `const` inconsistency, dead code, duplication.

## Rules

### Language

1. C11 as the pinned clang compiles it, with warnings as errors. No compiler extension outside a file that exists to hold one, such as the platform layer.
2. No variable-length arrays and no `alloca`.
3. No behaviour that depends on the implementation. That rules out relying on the signedness of `char`. It rules out shifting a negative value, or shifting by the width of the type or more. It also rules out relying on the order in which the arguments of a call are evaluated.

### Memory

4. Every allocation's result is checked before use.
5. A computed size, a product or a sum, is checked for overflow before it reaches an allocation or an index.
6. Every index into a buffer is checked against its length before the access. A loop bound that already proves it is enough.
7. A pointer is never cast to a pointer of an unrelated object type to read the bytes. Bytes of one type become another through `memcpy`.
8. A pointer cast never lowers the alignment the target needs.

### Strings and copies

9. No function that finds a length by scanning for a NUL writes into a buffer. `strcpy`, `strcat`, `sprintf`, `vsprintf`, `gets` and `strncpy` are banned, as is `scanf` with a bare `%s`. A copy is `memcpy` with a length the code knows, followed by an explicit NUL where one is needed. Text is formatted with `snprintf`, and its result is checked for truncation.
10. Text inside the code carries its length, as a `str` of Anti does. A NUL-terminated string stands only at the boundary with C, where a C function expects one. A test fails the suite when a banned function appears in `src/rt/`, `src/antic/` or `src/anti/`.

### Ownership and errors

11. Every function that returns allocated memory says in its comment who frees it and how.
12. A function that acquires more than one resource releases them on every path. It does so through one cleanup block at its end, reached by `goto`. It does not repeat the release before each return.
13. The compiler reports a problem in a program through its diagnostics. The runtime reports a failure through its failure routine. Neither prints to standard error on its own. Neither calls `abort` except where a state cannot be reached.

### Untrusted input

14. Code that reads bytes it did not write checks each length and offset before it reads. It refuses input it does not understand with an error, and never aborts on it. This covers the lexer and parser, the library file reader, and the TOML and JSON readers. It also covers `Object.deserialize`, the ELF, Mach-O and PDB readers, and the plugin index.
15. Each of those readers has a test that feeds it malformed input. The input is truncated, oversized, or has lengths and offsets that point outside the data.

### Integers

16. A size or count uses `size_t` or `int64_t`. It is not mixed with a narrower or differently signed type in a comparison or in arithmetic.
17. A value whose width is part of a format uses a fixed-width type. A field of a file or of an ABI is such a value.

### Structure

18. A function longer than 100 lines, nested deeper than 4 levels, or taking more than 6 parameters is reported. The report says whether a split would make it clearer, or whether the size follows the shape of the problem. A deep nest can read better than a chain of helpers. The threshold is a reason to look, not a rule to enforce.
19. A source file longer than 3000 lines is reported, with where it could split along the parts of its work. The same judgement applies as for a function.
20. A function used in one file only is `static`.
21. A header includes what it uses and nothing more, has an include guard, and no two headers include each other.
22. `src/rt/` includes no header of `src/antic/`, `src/anti/` or `tools/`. A platform `#if` stands only in the files of the platform layer.
23. Mutable global state exists only where a comment says why, and code of `src/rt/` that threads can reach guards it.

### Consistency

24. A pointer to data a function does not change is `const`.
25. The names of a module share its prefix. The runtime's exported names start with `anti_rt_` or the prefix its layer documents.
26. No function is unused, and no block of code is repeated where one helper would serve.

### Comments

27. Comments follow the docs-style rules of the repository.
