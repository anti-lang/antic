# Compilation of generics

The step builds "Compilation" of "Generics" in round five of
`docs/anti-language-additions.md`, after Eddie's decision on how a library
file carries a generic. It covers the generics of a module's own source.
Eddie split the step on 2026-09-25: the tree section of the library file
and everything that needs it move to the step of the libraries.

## What was done

- The decision stands in `docs/decisions.md`, in "Libraries and C" of the
  specification and in the syntax overview. A library file stores the
  checked tree of a generic, and each use lowers it with its arguments.
- `src/antic/sema_copies.c` makes the copies once the checker has read the
  module. A copy clones the checked tree with the arguments in place of the
  parameters. An operator, a `for`, `e[i]` and a call of an interface on a
  value of a type parameter are checked again with the argument's type. A
  copy of a function is a function, `max<int>`. A copy of a type gets an
  item and a copy of each function, `List<int>.push`, so its table,
  descriptor, `construct` and `destruct` are its own.
- `size_of(T)` is the argument's size, and `type_name` gives `List<int>`.
- A symbol of a copy qualifies its arguments by their modules, and `mangle`
  writes each byte an assembler cannot read as `$` and two hex digits.
- A dev object marks a copy link-once: `.weak`, `.weak_definition`, or a
  COMDAT section with `discard`.
- A release build merges copies whose code is identical, in `whole.c`.
  `pick<*Order>` goes and `pick<*Person>` serves both.
- Tests: `programs/generic_copies.anti` in release and dev with one output,
  `listing_copies_merge.opt` and `.dev.opt`, `asm_copies_<target>` on all six
  targets, and `listing_error_generic_copies`.

## What failed and how it was fixed

- `--dump-types` made copies and the two type listings failed. The driver
  now asks for copies only past the front end and outside that dump.
- `c[0]` on `C: index` was typed before it was read again, so its type
  named `C.index`. An open node now takes its type from the second reading.
- A `construct` that a literal runs records no arguments of its copy. The
  copy of its class now comes from the object it builds or the receiver.
- Two arguments of one name in two modules would have given one symbol.
  Symbols now use `type_symbol_name`, which qualifies the arguments.
- A field of a copy named the generic as its `home`, which named the reach
  functions of every copy alike. The copy is now the `home`.
- A public generic gave public copies, which a library interface would have
  carried. Copies are private.
- `emit_identity` lacked the new program. Its manifest was written again
  with `-DWRITE=yes`, which added six lines and changed none.

## Notes for the step of the libraries

- A copy named in a public signature already crosses a library file. A
  module that calls `gen.make(4).get()` links against the copy of `gen`,
  in both modes.
- `lower_init_function` and the helpers of `lower_desc.c` define a function
  of a type only in the type's module and declare it elsewhere. A module
  that copies a generic of another module has to define the copy itself.
- No link has yet seen two definitions of one copy, so the link-once marks
  are checked in the assembly and not in a link.
- A backtrace names a copy by its symbol, with the escapes in it.

## Provisional decisions

- A build past the front end compiles every use of a generic of the
  module. It refuses a function of a class with type parameters of its own
  at its first call. This rewrites the entry that refused every use.
- The symbol of a copy qualifies its arguments and escapes each byte outside
  letters, digits, `_` and `.` as `$` and two hex digits.
- A copy is link-once in a build of one object per module.
- The merge runs in a release build, compares code, merges copies alone and
  gives merged copies one address.

## Gates

The build has no warning on the host, ASan and UBSan trees. The host suite
passes 993 of 993, ASan 992 of 992 and UBSan 992 of 992, without
`no_paths`. The logs are `build/logs/t6.log`, `build/logs/asan-test.log`
and `build/logs/ubsan-test.log`. The docs-style checker reports nothing on
every file touched, apart from `tests/CMakeLists.txt`, which it reads as
prose and has always failed on.

## Proof of the push

Taken after the push of the code and the report.

```text
$ git log --oneline -3
2357a26 Report the compilation of generics
b340f60 Compile the copies of generics
5bc0acb Store a generic in a library file as its checked tree
$ git status --short
$ git rev-parse HEAD origin/main
2357a26f35f5e83354700ad7dfd964bef7edc1f9
2357a26f35f5e83354700ad7dfd964bef7edc1f9
```

Suites: host 993 of 993, ASan 992 of 992, UBSan 992 of 992.
