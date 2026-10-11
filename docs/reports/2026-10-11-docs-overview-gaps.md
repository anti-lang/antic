# Gaps between the syntax overview and the built language

Date: 2026-10-11. Branch `docs`, from `f3af7693`. A report and nothing else: no
file outside `docs/reports/` changed.

The task compared `docs/anti-syntax-overview.md` with the language as built. The
overview has 45 sections, 98 `anti` blocks, of which 9 are marked `not-built`,
33 hidden context blocks and 37 status lines that open with `Built`. The
comparison read, in this order:

- `src/antic/lexer.c` and `lexer.h`, for every keyword, the reserved words, the
  string prefixes and the `null` refusal.
- `src/antic/parser.c`, `parser_item.c`, `parser_decl.c`, `parser_stmt.c`,
  `parser_expr.c`, `parser_type.c` and `parser_clause.c`, for every item,
  statement, expression and type form.
- `src/antic/sema*.c` for the rules named below, and `src/antic/warnings.c` for
  the names of the warnings and the safety checks.
- `docs/decisions.md`, `docs/anti-language-additions.md` and
  `docs/anti-object-model.md`.
- Every `pub` item of the 23 modules of `src/std/anti/` and of the 12 modules of
  `src/std/anti/collection/`.
- `src/anti/main.c` for the commands of `anti` and `src/antic/main.c` for the
  options of `antic`.

Where a document and the code disagree, the code is named as the authority.

Contents:

- [Reserved words against the lexer](#reserved-words-against-the-lexer)
- [Features the overview lacks](#features-the-overview-lacks)
- [Descriptions that are wrong or stale](#descriptions-that-are-wrong-or-stale)
- [Examples in an old form](#examples-in-an-old-form)
- [Entries of Later that are built](#entries-of-later-that-are-built)
- [Section plan of the updated overview](#section-plan-of-the-updated-overview)
- [Questions answered from the code](#questions-answered-from-the-code)
- [Questions for Eddie](#questions-for-eddie)
- [Gates](#gates)

## Reserved words against the lexer

The keyword table is `kinds[]` in `src/antic/lexer.c`, and the reserved words
are `reserved[]` beside it. Every word of the overview's keyword list stands in
the table, with four exceptions. Every word of the table stands in the overview,
either in the keyword list or in the line on types.

| Overview | Lexer | Finding |
|---|---|---|
| `show`, `unreachable`, `undefined`, `embed` listed as keywords | `TOKEN_IDENT`, ordinary names | The closing sentence says the lexer reads them as names. The list contradicts it. Move the four to a not-built line. |
| No reserved word named | `thread` is the one `TOKEN_RESERVED` word, which no grammar rule accepts | Add it. |
| `null` not mentioned | Refused with `` `null` is `none` in Anti `` | Add one sentence under Pointers or Reserved words. |
| "the `c_` types" | 14 keywords: `c_char c_uchar c_short c_ushort c_int c_uint c_long c_ulong c_longlong c_ulonglong c_size_t c_float c_double c_wchar` | List them once. |
| Contextual words | `link`, `framework` and `linux` at module level are contextual there alone | Add the three. |
| `inject` | `inject` and the two-word `inject final` | Name the two-word form. |
| Built-in functions: `mul_high` | `close(c)` is a built-in by name unless a function `close` is in scope. `splat`, `load`, `store`, `shuffle`, `sum`, `min`, `max` and `dot` of a simd struct, and `select`, `any` and `all` of `anti.simd`, are known by name | Add them to the built-ins line. |
| No token list | `::`, `=>`, `->`, `..`, `...`, `?*`, `??`, `?.`, `as?`, `+% -% *% <<%`, `+| -| *|` and the compound forms, `.{` of a direct import | Add the list of "Keywords" in `docs/anti-language-additions.md`. |

The positions of the contextual words are right where the overview gives them.
`atomic` is a keyword, as the list says.

## Features the overview lacks

Each item is built, has a test or a decision, and no sentence of the overview
describes it. The section named is where it belongs.

Literals and types:

- Character literals, `'q'`, and the escape set `\n \r \t \\ \" \' \0 \xHH
  \u{H}` with 1 to 6 hex digits. "Lexical rules" in `docs/decisions.md`.
- Float literals with an exponent, `1.0e9`, and the rule that `1e9` is none.
  No `0b` or `0o` prefix.
- Hash delimiters on `b`, `br` and `rf` as well as on `r` and the plain form.
- The array repeat `[v; n]`, which the hidden blocks use and no sentence names.
- The slice literal `[]T { ptr: p, len: n }`, the one way to build a slice from
  a pointer.
- `.len` and `.ptr` of a slice and a `str`, `s[i]` as a `byte`, `s[lo..hi]` as
  `[]byte`, and `a[lo..hi]` with both bounds required.
- `??T`, an optional of an optional, which `parser_type.c` reads as two `?`.
- The names the compiler declares in `anti.lang`, from `src/antic/types.h`:
  `Object`, `Job`, `Flags`, `Mutex`, `chan`, `Regex`, `ByteRegex`, `Match`,
  `ByteMatch`, `FieldDescriptor`, `Number`, and `Watch` beside the source's
  `Changes`. The Types section names `Flags`, `Mutex` and `chan T` alone.
- Enums: the underlying type is `c_int` without one, and a value without `=`
  follows the one before it and must fit. `as` converts both ways unchecked,
  and the six comparisons apply. The section has two lines.

Operators:

- The unary operators `&`, `*` and `-`. The section lists `!` and `~` alone.
- Precedence where it differs from C: `??` binds between the comparisons and
  the shifts and groups from the right, `in` binds as `<` does, `as` binds
  tighter than every binary operator. `parser_precedence` in `parser_expr.c`.
- `v is geo.Shape.Circle`, the test of a variant case of another module.

Statements:

- `if let` on a `?T` and on a variant, and `else if let` closing a chain. The
  form stands in two examples and no sentence of Statements.
- The forms of an arm of `switch` and `select`: a call, an assignment or a
  block, one value per arm, one `else`. A `select` arm may leave the binding
  out.
- `let (a, b) = f() catch e { }`, `*p = f() catch e { }`, `catch { }` without
  a name, and `yield;` without a value.
- `defer` and `undo` hold one statement and refuse a nested `defer` or `undo`.
- The message of `assert` is a string literal, and a call in its condition is
  the warning `assert-call`.
- A struct or variant literal of another module, `geo.Rect { }` and
  `geo.Shape.Circle { }`, and of a generic, `Pair<int, str> { }` and
  `Result<int, str>.Ok { }`.

Items:

- `const` in a class body, reached as `Name.NAME`, and `const` in a block.
- `packed` before `class`, `union` and `variant`, and `align(N)` on a class.
  The Structs section names `packed struct` alone.
- The zero-width bitfield `_: T : 0`, a unit break as in C, refused in a union.
- Trailing commas in every comma-separated list.
- `export` on `const`, `union` and `type`, the export signature rule, and
  `export fn` in a class requiring `pub`. The C interop section names `fn`,
  `struct`, `class` and `type`.
- `extern fn` takes `...`, refuses `may fail` and refuses `export`.
- `internal` marks a module item alone, and `protected` a class member alone.
- One `tests` block and one `fixtures` block per module.
- An `allow` or `unchecked` clause at the top of the file, before the imports.

Warnings and safety checks: the overview names `shadowed-catch`,
`never-fails`, `unused-allow`, `unused-unchecked`, `unguarded-field` and
`exponential-pattern`. `src/antic/warnings.c` holds 14 warnings and 2 safety
checks. The nine it lacks are `assert-call`, `unfilled-abstract`,
`above-vector-cap`, `single-segment-path`, `doc-markup`, `doc-unresolved`,
`doc-note-only`, `undocumented` and `doc-dropped`.

Standard library. The section is one example and a list of module names. The
public items it does not name, by module:

| Module | Public items the overview does not name |
|---|---|
| `anti.lang` | `Changes`, `Watch`, `RawFrame`, `Frame`, `Ordered`, `hash_seed`, `seeded` |
| `anti.io` | `print`, `eprint`, `eprintln`, `exit` |
| `anti.text` | `from_c`, `from_bytes`, `byte_count`, `char_count`, `slice`, `find_byte`, `Align`, and the `append_*`, `take`, `len` and `clear` of `Builder` |
| `anti.fs` | `read_file`, `write_file`, `Mode`, `File`, the default `mode` of `open` |
| `anti.os` | `INTERRUPT`, `BREAK`, `TERMINATE`, `on_signal`, `signal_pending` |
| `anti.time` | `Duration` and its nine functions, `now`, `wall`, `sleep` |
| `anti.log` | `Level`, `Format`, `Sink`, `StderrSink`, `FileSink`, `CallbackSink`, `Log`, `logger`, `set`, the six level functions, `level_name`, `level_of`, `register` |
| `anti.json` | `write`, `write_text`, `write_int`, `write_bool`, `member`, `unquote` |
| `anti.toml` | `Document`, `int_value`, `bool_value` |
| `anti.config` | `current`, `read` |
| `anti.args` | `Parser` |
| `anti.random` | `Random` beside `Source` and `SharedRandom` |
| `anti.reflect` | `parent`, `name`, `TypeId`, `Field`, `Function`, `ValueKind`, `nothing`, the seven `of_*` constructors, the four error codes |
| `anti.plugin` | `NOT_LOADED`, `IN_USE`, `Library` |
| `anti.runtime` | `configure`, `get` named under Plugins alone |
| `anti.debug` | `backtrace(skip)` |
| `anti.license` | `text` |
| `anti.error` | named in full |
| `anti.trace` | the four slot constants, `install(what)` |
| `anti.regex` | `compile_bytes`, `Cursor`, `Matches`, `Pieces`, `ByteMatches`, `BytePieces`, the functions behind the methods of `str` and `[]byte` |
| `anti.collection` | `Indexed<T>`, `IndexedWalk<T>`, `Copies<T>`, `NotOne`, `allocator`, `equal`, the six hashing helpers, the entry writers |
| `anti.collection.*` | the `*Walk`, `*Keys`, `*Values` and `*Entries` iterator classes, `EntryTable`, `SlotTable`, `BTree`, `Slots`, `Handle<T>`, `Changed`, `Snapshot<T>` |
| `anti.simd` | declares nothing, the compiler knows `select`, `any` and `all` by name |

Tools. `src/anti/main.c` has twelve commands: `new`, `build`, `run`, `sdk
export`, `sdk import`, `test`, `check`, `doc`, `fmt`, `bind`, `symbols` and
`license`. The overview names `bind`, `build`, `test`, `check`, `doc`, `fmt`
and `symbols`. It lacks `new`, `run`, `sdk export`, `sdk import` and the five
forms of `license`. Of the options of `antic` it names `-g`, `--checks`,
`--no-checks`, `--asserts`, `--trace`, `--no-trace`, `--no-hooks`,
`--no-reflect`, `--cpu`, `--memory-checks`, `--closed`, `--inject`, `--lib`,
`--no-runtime`, `--warnings-as-errors`, `--tests` and `--package-version`. It
lacks `--lto full|thin|none`, `--profile-generate` and `--profile-use`,
`--bundle-runtime`, `--soname`, `--linker lld|platform`, `--framework`,
`--linux-lib`, `--front-end`, `--dev`, `-c`, `-S`, `--strip-docs` and the
`--dump-*` and `--print-*` forms.

## Descriptions that are wrong or stale

1. Reserved words. The keyword list names `show`, `unreachable`, `undefined`
   and `embed`, which the lexer reads as names. See the table above.
2. Checks and debugging: "Linux and Windows have not run a program of the
   option." Stale. The run on x86_64 hardware of 2026-10-08 passed
   `memory_checks` on linux-x86_64 and windows-x86_64, as "Scope and
   toolchain" in `docs/decisions.md` records and
   `docs/reports/2026-10-08-x86_64-hardware.md` reports.
3. Checks and debugging: "Windows has not run a trace." Stale. The suite of
   anti-windows passed on 2026-10-10 with 1619 tests, the tests of
   `tests/trace/` and `program_hook_trace_*` among them, as
   `docs/reports/2026-10-10-dist-release-check.md` reports. That report
   names 14 skipped tests and does not list them, so the sentence stands
   only if the trace tests are among the 14.
4. Checks and debugging: "the build links its runtime from the pinned
   clang" under `--memory-checks`. The runtime of AddressSanitizer stands in
   `lib/<target>/` of the runtime archive, and a package carries no clang.
   `CLAUDE.md` says "from the runtime archive".
5. Threads, twice: "`ANTI_THREADS` is gone." History, not a description of
   the language. The rule is that `--anti.threads` and the key `threads` set
   the pool size.
6. Injection: the service interfaces "wait for the native libraries of the
   runtime archive." The libraries are built, step 16 of `CLAUDE.md`. The
   modules `anti.db`, `anti.http`, `anti.serialize` and `anti.crypto` are not
   built, and the reason given is gone.
7. Standard library: `anti.net`, `anti.raylib` and `anti.miniaudio` "over the
   native libraries" are not built. The sentence is right, and the matching
   entry of `docs/decisions.md` under "Compiler behaviour" still says they
   wait for the libraries.
8. Direct imports: "The same over modules that are built" before the second
   example. Both examples import built modules, `anti.collection.map` and
   `anti.mem` among them.
9. Generics: the sentence that says `anti.lang` ships the constraints
   `Number` and `Ordered`. The compiler declares `Number` with the hooks `add
   sub mul div neg lt`, `sema_generic.c`. The source of `anti.lang` declares
   `Ordered` alone.
10. Statements: "Labels on blocks." stands in the list of statements, and the
    status line of Statements does not name labels as not built. Loops does.
    `parser_stmt.c` has no label form.
11. Errors: "every function of `anti.fs`: `open`, `read`, `write`, `flush`,
    `size`, `close`, `list`, `remove` and `rename`" lacks `read_file` and
    `write_file`, both `may fail`.
12. Types: "The built-in types `Flags`, `Mutex` and `chan T`." The compiler
    declares `Job`, `Regex`, `ByteRegex`, `Match` and `ByteMatch` as well.
13. Standard library: the list of what `anti.lang` holds lacks `Changes`,
    `Watch`, `RawFrame`, `Frame`, `Ordered`, `hash_seed` and `seeded`, and
    the list of what the compiler declares lacks `Regex`, `ByteRegex`,
    `Match`, `ByteMatch`, `Number` and `chan`.
14. Every section: the 37 status lines name 54 test files and directories,
    `tests/programs/own_params.anti` and the like. That is the log of the
    sessions that built each feature, which `docs/reports/` holds. A working
    programmer reads none of it.
15. Later: the whole section says that everything it names is built. See
    below.
16. Checks and debugging and Hooks and tracing: the lines "Not built yet: the
    variables of `-g`", "the symbols archive of a shared library and of a
    plugin", "the names of a Windows frame in `resolve`", "the leak report on
    windows-x86_64" and "the cost of one load and one compare at a site
    without a handler" are right. `build.c` writes the archive for
    `BUILD_PROGRAM` alone, `resolve` leaves a Windows frame raw by decision,
    and a hook site is one call of the runtime.
17. Reflection: "Not built yet: `type_of(T)`" is right. No source of
    `src/antic/` spells the name.
18. Generics: "Not built yet: the cache of copies in a dev build" is right. A
    dev build marks a copy link-once, and the entry under "Generics and
    collections" gives the cache to the step of the library file.

## Examples in an old form

Every `anti` block compiles through `overview_examples`, so an old form here
means one the specification has replaced. Two blocks show arguments by position
where the specification writes them by name, and the text marks both:
`List<Person>.new(1000)` with `Map<str, int>.new(0, &arena)` under Collections,
and `data.patch(x"80 10 20 30", x"FF", 0, 2)` in the text of Bytes. Named
arguments are not built, so the positional form is the current one. No other
block shows a superseded form. The `not-built` blocks show the forms of the
specification and stay as they are.

## Entries of Later that are built

The section names the syntax of generics, the constraints, the compiled copies,
the generics of library files and of C, what can be generic, the other features
with generics, optional values, ownership at a call, lending, walking a
collection, hashing and order, direct imports, the parts every collection
shares, the collections, the thread-safe collections, shared ownership, memory
checks, `Iterable<T>` and `Iterator<T>`, and closures. Every one is built, and
the section says so of each. Nothing in it is later. The not-built items of the
language stand in their own sections and need no list of their own: named
arguments, labels, `show`, `unreachable`, `undefined`, `embed`, `target.os`
with its `switch`, wire formats, script mode, `type_of(T)`, `deserialize` of a
collection, the warning on a field written inside `sync` and read outside it,
the named arguments of `patch`, the cache of generic copies, the variables of
`-g`, the symbols archive of a library, the Windows frames of `resolve`, the
hook-site cost, the service interfaces, `anti.net`, `anti.raylib`,
`anti.miniaudio`, `anti.db` and `anti.binary`, and a library file carrying a
function of a class with type parameters of its own or a type nested in a
generic class.

## Section plan of the updated overview

One change applies to every section. The status line becomes one sentence
that opens with "Not built yet" where something of the section is missing,
and nothing where everything is built. No status line names a test file. The order below is
the order of the current contents.

| Section | Action | Change |
|---|---|---|
| Introduction | Change | Keep the three sentences on the rules and the test. |
| Program structure | Keep | |
| Modules | Change | Take Direct imports in as a paragraph, and add `link framework` and `link linux` as the two other module-level lines. |
| Direct imports | Remove | Merged into Modules. |
| Types | Change | Add the compiler-declared types, `.len`, `.ptr`, `s[i]`, `s[lo..hi]`, `a[lo..hi]`, `??T`, the `c_` types by name. |
| Literals | Change | Add character literals, the escapes, exponents, the hash forms, `[a, b]`, `[v; n]` and `[]T { ptr: p, len: n }`. |
| Variables and constants | Change | Add `const` in a block and a class body, `let x: atomic T`, `let (a, b) = e`, and a pointer to `let x = p else { }`. |
| Operators | Change | Add `& * -`, a precedence table of the eleven levels with `in` and `??`, and `is` on a variant case. |
| Statements | Change | Add `if let` and `else if let`, the arm forms, the nesting rule of `defer` and `undo`, the message rule of `assert`, `catch { }`, `yield;` and `*p = f() catch`. One not-built line for labels, `show` and `unreachable`. |
| Loops | Change | Cut the two status paragraphs to the one not-built line, labels. |
| Functions | Change | Add the three rules of `extern fn`, or point at C interop. |
| Anonymous functions and closures | Change | Cut the status paragraph. |
| Tuples | Keep | |
| Errors | Change | Add `read_file` and `write_file`. Move the list of failing functions of the standard library into the table of Standard library. |
| Structs | Change | Add `packed` and `align(N)` on class, union and variant, `_: T : 0`, trailing commas, nominal typing, and the one-field rule of a union literal. |
| Simd structs | Keep | |
| Enums | Change | Add the underlying type, the fit rule, `as` both ways and the comparisons. |
| Variants | Change | Cut the status paragraph. |
| Classes | Change | Add the literal of a class of another module. Cut the status paragraph. |
| Interfaces | Keep | |
| Generics | Change | Fix the sentence on `Number` and `Ordered`. Cut the status paragraph to the two not-built lines. |
| Ownership | Change | Cut the status paragraph. |
| Pointers | Change | Add the `null` refusal. |
| Optional values | Keep | |
| Reflection | Change | Add `parent`, `name`, `TypeId`, `Field`, `Function` and the `Value` constructors. |
| Operators on classes | Change | Cut the status paragraph. |
| Static fields and singletons | Keep | |
| Threads | Change | Drop the two sentences on `ANTI_THREADS`. |
| Concurrent classes | Change | Cut the status paragraph. |
| Hooks and tracing | Change | Cut the status paragraph to the one not-built line. |
| Injection | Change | Replace the sentence on the service interfaces with "Not built yet: the interfaces for services." |
| Plugins | Change | Cut the status paragraph. |
| Tests | Change | Add one `tests` and one `fixtures` block per module. |
| C interop | Change | Add `export` on `const`, `union` and `type`, the signature rule, the rules of `extern fn`, and `link linux` beside `link framework`. |
| Compile-time targets | Keep | |
| Source locations | Keep | |
| Checks and debugging | Change | Fix items 2, 3 and 4 above. Split the processor levels into a section of their own. |
| Processor levels | Add | The levels, the defaults per target, `--cpu`, the start-up check, one runtime per level. Today the third paragraph of Checks and debugging. |
| Warnings and safety checks | Change | Add a table of the 14 warnings and the 2 safety checks with one line each. |
| Wire formats | Keep | Not built. |
| Script mode | Keep | Not built. |
| Regular expressions | Change | Cut the status paragraph. |
| Bytes | Keep | |
| Collections | Change | Cut the status paragraph to the three not-built items. |
| Standard library | Change | A table of the 23 modules and the 12 collection modules, one line per module with its public items and which of them fail. |
| Tools | Add | The twelve commands of `anti` and the options of `antic` the overview names, with `--lto`, the two profile options, `--bundle-runtime`, `--soname` and `--linker`. One table. Whether build options belong in a language overview is Eddie's call, see the questions. |
| Later | Remove | Everything it names is built. |
| Reserved words | Change | Drop the four names, add `thread`, `null`, the 14 `c_` types, `link framework` and `link linux`, `inject final`, the token list and the built-ins known by name. |

## Questions answered from the code

- Is `ByteRegex.compile(text)` built? Yes. `sema_check_regex_compile` of
  `sema_pattern.c` rewrites it to `compile_bytes` of `anti.regex`.
- Are labels parsed? No. `statement_level` of `parser_stmt.c` has no form of
  an identifier and `:` before `for`, `while` or a block.
- Is a shebang line read? No. `doc_marker` of `lexer.c` reads `#` after `//`
  alone, so script mode has no part in the lexer.
- Is `thread` the only reserved word? Yes, `reserved[]` of `lexer.c` holds it
  alone.
- Does `dup` take an allocator? No. `sema_expr.c` refuses a second argument of
  `dup` and of `delete` on a channel, and `delete` and `destroy` take one.
- Does `anti build --release` write a symbols archive for a library? No.
  `build.c` calls `build_symbols` for `BUILD_PROGRAM` alone.
- Where is `Number` declared? In the compiler, `LANG_NUMBER` of `types.h` and
  `number_hooks` of `sema_generic.c`. `Ordered` is in `src/std/anti/lang.anti`.
- Does `--memory-checks` run on Linux and Windows? Yes on x86_64 hardware,
  2026-10-08. The arm64 VMs skip the x86_64 tests under emulation.

## Questions for Eddie

- An enum body accepts functions and constants after its values.
  `parser_members_of` of `parser_decl.c` reads them with no nested types, and
  `sema.c` stores them as the members of the enum type. No specification
  names the form, no test uses it, and the overview does not describe it. Is it
  a feature, to be described, or an acceptance to refuse?
- Do the options of `antic` and the commands of `anti` belong in the overview?
  `CLAUDE.md` says the overview summarises the two specifications and adds
  nothing, and the specifications describe `anti` under tooling. The overview
  names 17 options today. The plan above proposes one Tools section and leaves
  the choice open.
- The two sentences on runs that have not happened, items 2 and 3 above, are
  the kind of claim that goes stale with every release. Should the overview
  carry such sentences at all?

## Gates

The task names the report as the one change, and says the host suite need not
run. `CLAUDE.md` gives the same rule for a commit that changes files under
`docs/` alone: the docs-style checker over its `.md` files and nothing else. So
the host suite was not run, and its pass count is not reported here. The last
recorded count is 1701 of 1701 on the Mac, 2026-10-10, in
`docs/reports/2026-10-10-dist-release-check.md`.

The docs-style checker, `python3 tools/docs-style/check_docs.py` over this
file, reported nothing. A `git grep` for the three marker words the gates name
finds none of them in it.
