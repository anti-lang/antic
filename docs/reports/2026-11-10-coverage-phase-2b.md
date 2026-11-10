# The second batch of coverage

This step sorts every region that the coverage report showed unreached in
`src/rt/object.c`, `src/antic/sema_stmt.c`, `src/antic/driver.c` and
`src/rt/plugin.c`. It follows the method of
`docs/reports/2026-11-09-coverage-phase-2a.md`. Eddie's direction is the
Mac alone, so the VMs were not touched. The batch adds tests, and it fixes
two defects that the tests showed. It deletes no code.

## Commits

- `acafa766` tests the walks of `object.c` and lists T218 and T219.
- `f932849b` fixes T218.
- `d15614a9` tests the loader of `plugin.c` and lists T220.
- `09b39830` fixes T220.
- `b4231586` tests `sema_stmt.c` and lists T221.
- `e9b1aabb` tests `driver.c`.
- `87e11f69` reaches a short input of `--dev` and an `own` pointer to a
  union. The coverage run after `e9b1aabb` showed both unreached.
- This report, with the times and the counts in `CLAUDE.md`.

## Findings

- T218, fixed. `serialize` of a `List<[2]Bits>`, a list of arrays of
  unions, wrote `[[]]`. A field of that type is `null`, so the list is
  `[null]`. `anti_rt_element_serialize` now writes `null` for an array
  whose elements it cannot count.
- T219, open. `to_text` of a `List<[2][2]int>` writes `[?]`, and
  `serialize` writes `[null]`. The record of a type argument carries the
  descriptor of the element of an array alone, so the runtime finds no
  lengths of the levels. See question 1.
- T220, fixed. The check of the floor compared the first eight parts of a
  version, so a library built for `1.0.0.0.0.0.0.0.1` loaded below the
  floor `1.0.0.0.0.0.0.0.2`. `version_below` now reads every part.
- T221, open. An `&&` chain narrows its first eight names alone, which is
  `PROVED_MAX` of `src/antic/sema_checker.h`, and the read of the ninth is
  refused. See question 2.

The executable of `return42` for macos-arm64 links the runtime. Both fixes
therefore wrote its digest under `tests/link-identity/` again.

## object.c

Tested:

- `to_text` of elements that hold unions, bitfields, channels, slices of
  floats, arrays of two levels and arrays of class values.
- `serialize` of arrays, `own` slices and `own` pointers of variants, `?T`
  values, tuples, unions and class values.
- Patterns of the empty text and of other lengths.
- `dup` of an `own` slice that holds memory and no element.
- `unit_object`, which runs the walks over records that antic does not
  write.

Unreachable, question 3:

- An array has at least one element. A length of 0 in
  `anti_rt_array_shape` and in `put_value` takes no input.
- Every caller gives `show_row` an element size above 0.
- No caller passes NULL as the descriptor of `checked_object`. `delete`,
  `destroy` and `dup` take a class pointer, and the C header passes the
  descriptor.

Twelve lines are a `}` after a `return`, which no run reaches.

## plugin.c

`unit_plugin_table` takes paths of no length and past their room, and a
handle given back. It compares versions of many parts, and loads a host
interface without a chain and a slot that no function names. It loads
damaged arrays, descriptors, records, chains and entries, and a chain of
17 types. It reaches a reason past the room of a message, an index and a
directory that discovery passes over, and reasons that name a symbol.
`unit_plugin_stub` calls the stub through a second object and through a
table of three entries. `plugin_host_fault_index_memory` refuses the
memory of the index.

Other host: the false side of the branch at line 855 runs on Linux alone.
The symbol stands at the end of a reason there.

Not tested: `fitted` after `vsnprintf` fails. That needs a message of more
than `INT_MAX` bytes, about 1.1 GB of memory per run.

Unreachable, question 4:

- `fitted` is called with the sizes 512 and 128 alone.
- `damage_in` refuses a NULL descriptor and an empty chain before
  `matches_host` and `checked` test for them.
- `missing_symbol` keeps a symbol below 256 bytes.
- `discover` turns an empty directory into `.`.
- The keys of the index fit their buffers of 64 and 32 bytes.
- `index_lists` meets an empty value before the end of its count.
- A slot closed between the load and `reserve` needs a second thread that
  unloads a handle more often than it loaded it.

## sema_stmt.c

44 programs and refusals of `tests/matrix/` reach the ends of a path
through loops, a `try` whose inner handlers yield and `switch` arms of
converted sizes. They reach the names and types that `sync`, `select`,
the flags form, `if let`, `for` and `let` may not know, and a `take`
through a loop copy or a singleton. They also reach an anonymous function
that returns `never`, a closure given to a local of another type and an
`own fn` given to a local of `?fn`.

`error_assert_call` holds every form of a call in an `assert`.
`error_tests_teardown` holds a `teardown` in a `tests` block. Three trees
under `tests/modules/` give `anti.text` an `equal` of another signature.

Unreachable, question 5:

- the checks of a NULL base, callee, type, symbol, condition or body that
  follow a successful check
- `sema_declare` returning NULL in a scope opened for that name
- an abstract class that fills, and a library class that fills a class of
  the module being compiled
- the anonymous function in `sema_check_function`
- `EXPR_TAKE` in an `assert`, and a `take` of anything but a field
- `PROVED_MAX` past 8 in the walk of the leaves

`EXPR_FREE` in an `assert` is reached only through a tuple of `void`, see
question 6. The false sides of the branches at lines 1110 and 2843 were
not sorted.

Five regions belong to the measurement: a `}` after a `return`, a
`return;` after a block, and the false side of a `switch` without
`default`.

## driver.c

The tests reach a short input and a library file under `--dev`, a
profile of text, `--profile-generate` beside the platform linker and a
library file whose constant divides by zero. They reach the platform
linker of another system in a dev build and for a library, `--dump-ir` of
a program that the whole check refuses and `-S` without the `llc` of an
archive. Fault cases refuse an output in the top directory and the record
and the licence texts of a notice. The rest are a dev build without a
runtime, a Linux program of `--memory-checks` with and without `--lto`, a
dev build of a library file with `main`, the refused doc pages and the
refused level of the driver.

Not tested: the reader of `licenses/sources.txt` on a record without its
last line feed, or without the library. The output of the case holds the
licence text of the standard library. A case of `tests/anti_messages.cmake`
cannot hold its empty lines, see question 7.

Three inputs reach code alone and look like defects, see question 8:

- `--lib shared --dev` exits 0 and writes only `<base>.o`.
- `--dev -c lib.antl` writes an object.
- `anti doc .antl` reads a library file named `.antl` as a source.

Unreachable, question 9:

- a host that is none of the six targets
- a failure of the IR verifier after lowering or after the optimizer
- a message of the whole program without its line feed
- spans of the debug marks out of order, since they stay sorted and
  disjoint
- a notice that does not start with its marker
- a package without a name
- a clash of the externs of one module
- a library file of more than 2^32 entries
- `--lib shared --bundle-runtime`, which R132 refuses first
- a library file that holds no module of its own name
- a failed header of `anti bind --header`, since `unit_library_header`
  reads the header first
- a failure of `sema_check` on an empty module
- counts of warnings without `--warnings-as-errors`
- a library of a doc page without diagnostics

## Coverage

| | Before | After |
|---|---|---|
| Regions | 97.90%, 1755 missed | 98.02%, 1657 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.09%, 1806 missed | 98.23%, 1673 missed |
| Branches | 90.57%, 5215 missed | 91.00%, 4978 missed |

| File | Lines before | Lines after | Branches before | Branches after |
|---|---|---|---|---|
| `object.c` | 92.60%, 60 missed | 98.28%, 14 missed | 85.02% | 99.12% |
| `sema_stmt.c` | 97.63%, 54 missed | 98.95%, 24 missed | 90.46% | 95.43% |
| `driver.c` | 96.74%, 52 missed | 97.81%, 35 missed | 93.78% | 96.66% |
| `plugin.c` | 96.03%, 39 missed | 99.18%, 8 missed | 85.22% | 95.46% |

`rt_object_tests` joins the programs that the report measures.
`rt_deserialize_tests` compiles `object.c` and `registry.c` as well, and
the report does not measure it, see question 10. The coverage suite ran
7102 tests, all passed, and `./c coverage` took 961 s.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,<last>`. The times stand in `CLAUDE.md`.
  Every test passed apart from the two skips of `mimalloc_environment`.
- A run that failed was fixed, and the whole suite ran again. The failures
  were the manifest of `emit_identity` and the include of
  `binary_stdio.h` for the first commit, and the digest of
  `link_identity_macos-arm64` for each fix. They were also the form of
  `anti fmt` for the plugin sources, and a claim of
  `docs/notes/source-tree.md` that `note_claims` refused.
- Both sanitizer suites ran after the last code commit, in the six parts
  of `asan`, up to test 7112. Every test passed. `asan` took 247 s, 185 s,
  265 s, 186 s, 448 s and 439 s. `ubsan` took 249 s, 187 s, 264 s,
  199 s, 469 s and 468 s.

## Provisional lines

No new provisional line.

## Questions for Eddie

1. T219. Should the record of a type argument that is an array carry the
   descriptor of the array with its levels, as a field record does?
2. T221. Should an `&&` chain narrow any number of names, or a larger
   number with a message past it?
3. Keep the guards of `object.c` listed above, or remove them?
4. Keep the guards of `plugin.c` listed above, or remove them?
5. Keep the guards of `sema_stmt.c` listed above, or remove them?
6. `let t = (*sp, 1);`, where `sp: *Shape` points to an abstract class,
   compiles. So does `(nothing(), 1)` of a `void` call. `let s = *sp;`
   and a `let` of a `void` value are refused. Are both defects?
7. Should `run_anti_messages.cmake` take a file of expected output, so
   that a case can hold empty lines?
8. Should `--lib shared --dev`, `-c` beside a library file and a library
   file named `.antl` be refused? With which messages?
9. Keep the guards of `driver.c` listed above, or remove them?
10. Should `rt_deserialize_tests` join the measured programs?
11. `if let Circle c = p`, with `p: ?*Shape`, says that `if let` takes a
    value that may be `none`, found `?*Shape`. Should the message say that
    a case needs a variant?
