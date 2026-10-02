# Closure of the second audit

This session accounts for every finding of `docs/audit/summary.md` in
`docs/audit/closure.md`. It read the code at each place, fixed what the
reading found open, and brought `CLAUDE.md`, the specifications and the
overview in line with the code. Logs are under `build/drive/logs/`, named
`cl-*`.

## Findings

Every finding has a row in `docs/audit/closure.md`. All 34 severe and all
62 major findings are fixed, each at the commit its row names. Every minor
row and every defect no rule names is fixed or not a defect. Four rows
wait for a decision of Eddie's.

The reading found five minor items still open, and this session fixed them:

- Rule 24. `27450261` removes five casts that took `const` off an item, a
  constant value or an interface. Five casts stand, each with its reason
  at the line. Three are in the tree walk that reads and writes through
  one table. Two turn a const view into a type the substitution hands
  out.
- Rule 18. `e31b2bea` passes `check_run`, `doc_run`, `test_run` and
  `cache_key` one struct each, `12d4eea1` does the same for
  `fn_signature`, and `df183509` replaces the parameter lists of
  `read_level` and `put_level` with `struct anti_array_shape`. The last
  changes the runtime, so `return42` for macos-arm64 links to other bytes,
  and its pin holds the digest this Mac wrote. `antl_read`, `lower_module`
  and `sema_doc_warnings` were judged from their counts alone, so rule 18
  leaves them standing.
- Rules 18 and 19. `519dacc5` splits `whole.c` into the analysis and
  `whole_tables.c`, the writers of the runtime tables, as the back-end
  report judged. `header_sections` checks the new `whole_parts.h`.

These are changes of structure that keep behaviour, so no test failed
first. The three suites run them unchanged.

Not a defect after reading: `layout_data`, under the IR free of sizes. The
back end lays out a valued global for its one target, as `layout_resolve`
folds the instructions, by the DESIGN at `select.c`.

## Left for Eddie

- `Changed` and `Snapshot` of the thread-safe collections, which entries of
  `docs/decisions.md` keep in their modules.
- The names of size and presence: `len`, `count`, `has` and `given`.
- Whether `args.text_of` and `args.at` give `?str`.
- The directories of `tests/` for the projects of the `anti` commands and
  the scripts flat in `tests/`.
- Should the text builder stop the program when memory runs out?
- Is the reflection stub of a plugin slot for C alone?

## Documents

- `docs/anti-object-model.md` and the overview state that an enum value
  without `=` follows the one before it and fits the underlying type.
- `docs/anti-language-additions.md` and the overview name `rt.get`.
- `CLAUDE.md` "State" names `rt.get` and two refusals: `--no-hooks` where
  a library loads, and `[injections]` in `rt.configure`. It drops the old
  rule on `[injections]`.
- Two `[provisional]` entries no longer matched the code and were brought
  up to date: the one on `lower_module`, whose dead result and parameter
  `0356cefa` removed, and the one on the files of lowering, which named
  `hook_name` where `sema_root_hook` stands since M19.

## Provisional entries for review

The fix steps added or rewrote 110 `[provisional]` entries of
`docs/decisions.md`, this session's one on the files of the pass over the
whole program among them. Each was read against the code, and the names it
gives were checked to exist. The line is the line in `docs/decisions.md`.

| Line | Section | Opening words |
|---|---|---|
| 180 | Object model | An unsigned integer field reads `-0` as 0. Reason |
| 182 | Object model | A field of a struct with a descriptor takes |
| 220 | Declarations and statements | A constant whose type holds a class is refused |
| 221 | Declarations and statements | The value of a class literal that defaults a |
| 224 | Declarations and statements | A chain of `type` lines and a chain of |
| 225 | Declarations and statements | A type nests at most 256 levels of pointers |
| 226 | Declarations and statements | A value of an enum without `=` follows the |
| 257 | Expressions and types | The header writes an infinite float constant as `INFINITY` |
| 289 | Program entry | A status of `main` past 32 bits keeps its |
| 291 | Program entry | The test scripts read the standard error of a |
| 338 | Libraries and runtime | `no_paths` reads every byte of each file with `file(STRINGS)` |
| 354 | Libraries and runtime | The values of a struct, a union, a class |
| 355 | Libraries and runtime | The checked tree of a generic in a library |
| 356 | Libraries and runtime | The rules a library file must meet beyond its |
| 357 | Libraries and runtime | A library file holds every count, the lane count |
| 358 | Libraries and runtime | The verifier of library files holds the type table |
| 364 | Libraries and runtime | A lock the runtime keeps at file scope is |
| 365 | Libraries and runtime | The word of a Mutex is `struct anti_rt_word` of |
| 366 | Libraries and runtime | The platform layer starts a thread with `anti_rt_thread_start`, which |
| 367 | Libraries and runtime | The route of a signal stands in the platform |
| 368 | Libraries and runtime | The files of the runtime stand on the platform |
| 369 | Libraries and runtime | The trace asks the platform layer for the walk |
| 370 | Libraries and runtime | The start of a program, the memory at an |
| 371 | Libraries and runtime | `src/rt/atomic.c` chooses its form by the compiler and never |
| 821 | Standard library phase | `json.unquote`, `json.member` and `json.write_text` are bindings of the runtime |
| 824 | Standard library phase | `fs.Mode` gains `Append`, which writes after the end of |
| 825 | Standard library phase | `anti.log` reads the file the `logger` key names with |
| 863 | Build tool and distribution | The size field of an archive member is decimal |
| 916 | Compiler behaviour | ARM64 frames of `2^31` bytes or more are a |
| 957 | Compiler behaviour | A source file holds at most 64 MiB, `LEX_SOURCE_MAX` |
| 958 | Compiler behaviour | The lexer alone holds the rule on the bytes |
| 959 | Compiler behaviour | The parser descends at most 256 levels, `PARSE_DEPTH_MAX` of |
| 960 | Compiler behaviour | The limit of 256 levels of the parser holds |
| 961 | Compiler behaviour | The lexer records at most 100 errors, `LEX_ERRORS_MAX` of |
| 964 | Compiler behaviour | The checked allocator of antic is `alloc_zeroed`, `alloc_resize`, `alloc_grow` |
| 966 | Compiler behaviour | A message that a fixed buffer cuts ends in |
| 968 | Compiler behaviour | `lexer_token_kind_name` gives the bare spelling of a symbol when |
| 1022 | Failing functions | The table type of the header has the slots |
| 1039 | Tuples | Inside the name of a tuple or a `?T` |
| 1062 | Error origins and stack traces | `t.symbolize()` takes the image of a frame from its |
| 1321 | Runtime configuration | `rt.get(key) -> ?str` of `anti.runtime` gives the effective value |
| 1326 | Runtime configuration | One configuration reads at most 256 files, the file |
| 1364 | Hooks and tracing | A plugin cannot be built with `--no-hooks` either. antic |
| 1391 | Standard interfaces | `toml.int_value(v, fallback)` and `toml.bool_value(v, fallback)` read a value text |
| 1407 | Plugins | antic reads `anti-plugins.toml` with `src/rt/toml.c`, the reader the runtime |
| 1419 | Plugins | A release build of a program that can load |
| 1422 | Plugins | An object that `lib.instance` or a provider of an |
| 1423 | Plugins | The loader never asks the platform for the image |
| 1424 | Plugins | The loader checks every class descriptor of a library |
| 1425 | Plugins | `plugin.load(path)` refuses a path that holds a NUL byte |
| 1463 | Errors, warnings and checks | antic exits with 1 when it refuses a program |
| 1472 | Nested types | The C header writes a nested type as `PeopleList_Node` |
| 1484 | Language hooks and iteration | Every iterator has `to_slice()`, which collects the values that |
| 1548 | Concurrent classes | `ConcurrentMap` has `equal_entries(other, same)` and `hash_entries(part_sum)`, which its module's |
| 1592 | The build command | `anti` refuses a manifest whose `[package] version` is not |
| 1599 | The build command | Every compile that `anti build`, `anti test` and |
| 1600 | The build command | A requirement of the walk belongs to the pick |
| 1601 | The build command | A repository answers for an index in one of |
| 1618 | The symbols command | `anti symbols` decides whether a path of a runtime |
| 1619 | The symbols command | `zip_read` refuses an archive two of whose entries share |
| 1620 | The symbols command | `resolve` reads the object file that the debug map |
| 1637 | The doc command | The HTML of `anti doc` writes a link `[text](url)` |
| 1653 | The formatter | The scan by which `anti fmt` tells a list |
| 1654 | The formatter | `anti fmt` writes the canonical form into `<file>.new` beside |
| 1664 | The bind command | `anti bind --header` writes `<last segment>.h`, the last segment |
| 1683 | The bind command | An API description is refused as a whole when |
| 1692 | The bind command | `anti bind --clang` counts every line in `int64_t` and |
| 1693 | The bind command | An enumerator without a value after `INT64_MAX` takes the |
| 1978 | Names and shared code of the runtime | The contract between generated code and the runtime stands |
| 1979 | Names and shared code of the runtime | The markers of the licence notice, the head of |
| 1980 | Names and shared code of the runtime | The name `anti-plugins.toml` of the index of plugins stands |
| 1991 | Repository layout | The platform layer of antic is `src/antic/platform.c` with `platform.h` |
| 1992 | Repository layout | The tools hold every path, argument and variable as |
| 1993 | Repository layout | The platform layer of anti is `src/anti/platform.c` with `platform.h` |
| 1994 | Repository layout | The path rules of the tools are those of |
| 1995 | Repository layout | `anti` reaches antic through `src/antic/driver.h`, `src/antic/antic.h` and the shared |
| 1998 | Repository layout | The hook is `tools/hooks/pre-commit`, and `tools/git-hooks.cmake` names the directory |
| 2000 | Repository layout | A test name starts with the kind of check |
| 2001 | Repository layout | Every file of `anti` but its `main` is the |
| 2005 | Files of the parser | The parser stands in seven files along its parts |
| 2009 | Files of the library file | The reader and the writer of the library file |
| 2013 | Files of the driver | The driver of antic stands in five files along |
| 2019 | Files of the checker | `sema_class.c` holds the class model that `sema.c` checked: the |
| 2020 | Files of the checker | `ast_walk.c` holds the one walk of a function body |
| 2021 | Files of the checker | Each of these rules has one definition. `types_find_field` in |
| 2022 | Files of the checker | The `checked` flag of a field means that its |
| 2025 | Files of the checker | `sema_operator.c` holds the binary operators with `in` and `??` |
| 2027 | Files of the checker | The exported functions of the lexer, the parser, the |
| 2028 | Files of the checker | The exported functions of the optimizer, the targets, the |
| 2032 | Files of lowering | Lowering stands in one file per part, and `src/antic/lower_lowerer.h` |
| 2035 | Files of lowering | `lower_module` returns nothing and takes no diagnostics. It reported |
| 2036 | Files of lowering | `lower_owning.c` holds the owning values: the one teardown, copy |
| 2037 | Files of lowering | The fields of a class are torn down last |
| 2038 | Files of lowering | An `own` pointer or slice whose element owns something |
| 2039 | Files of lowering | Every new object is prepared by `lower_prepare_object`: a literal |
| 2040 | Files of lowering | The default `==` and the default hash take each |
| 2041 | Files of lowering | `lower_new_memory` makes the memory of `alloc T { }` |
| 2045 | Files of the pass over the whole program | The pass over the whole program stands in two |
| 2075 | Generics and collections | A copy of a generic is checked when it |
| 2077 | Generics and collections | A cut name inside a chain of copies is |
| 2078 | Generics and collections | A module names at most 16384 copies of generic |
| 2134 | Generics and collections | A pointer, a `?*T` and a function value hash |
| 2135 | Generics and collections | An array and a slice take their elements in |
| 2155 | Generics and collections | A collection is made with |
| 2257 | Generics and collections | A shared library, a library for C and a |
| 2272 | Generics and collections | `l[i] = x` tears down the element it replaces |
| 2274 | Generics and collections | `IndexedWalk<T>` of `anti.collection` is the public iterator of a |
| 2275 | Generics and collections | `anti.collection.Indexed<T>` is the public abstract base of `List`, `Deque` |
| 2295 | Generics and collections | `serialize` writes a map with `str` keys as a |
| 2353 | Generics and collections | The versions of a part are kept as a |

## Gates

- Build: zero warnings in the host, ASan and UBSan trees
  (`cl-final-host-build.log`, `cl-asan-build.log`, `cl-ubsan-build.log`).
- Host: 1459 of 1459 passed, `cl-final-host.log`.
- ASan: 1458 of 1458 passed, `cl-final-asan.log`.
- UBSan: 1458 of 1458 passed, `cl-final-ubsan.log`.
- Docs style: nothing on every `.md` file touched.

## Proof

Before this report was committed:

```console
$ git log --oneline -3
b4736301 Account for every finding of the second audit in docs/audit/closure.md
dc52bcaa Bring the specifications, the overview and the state in line with the fix steps
519dacc5 Split whole.c into its analysis and the writers of the runtime tables (minor, rules 18 and 19)
$ git status --short
?? docs/reports/2026-10-02-audit-closure.md
$ git rev-parse HEAD origin/main
b4736301b621040684a7a9fff6547d42f8d04dd4
b4736301b621040684a7a9fff6547d42f8d04dd4
```

The reply of the session gives the same after this report is committed and pushed.
