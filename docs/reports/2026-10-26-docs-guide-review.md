# Review of the whole developer's guide

Date: 2026-10-26, from `caf2f0ab`. The step read the 21 chapters of `docs/site/developers-guide/` and `index.md` in order and mapped every section of `docs/anti-syntax-overview.md` to the guide. It changed `.md` files under `docs/` alone, so no suite ran. The probes stand under `build/grevf/probe/`, the scans under `build/grevf/scan/`, and the logs under `build/drive/logs/grevf-*`.

## Method

- Each page was read once in full, in order. At each section the reading asked the questions of a programmer from Java, from C and from C++ in turn. It looked for a term used before it is explained and a forward reference without a link. It looked for two sections that say the same thing and an example longer than its point. It looked for a claim about another language that is not common knowledge. The step made one pass with the three readers, not three passes.
- `build/grevf/scan/links.py` checked every `relref` link and its anchor, and every local link: none is broken.
- `build/grevf/scan/parts.py` checked that every feature section holds the five parts in order and nothing else.
- `build/grevf/scan/missing.txt` lists each backticked term of the overview that the guide never names. Its hits gave the gaps of the map below.
- Each claim added was built and run by a probe with the host antic of `caf2f0ab`. Its `src/` equals that of `ba2e424e`, the commit the numbers of chapters 2 and 3 name.

## Changes

| Chapter | Change |
|---|---|
| 1 | The block of the import forms holds an `if` outside a function. The page now says how the test compiles a block without `main` |
| 2 | "Source text" gains the five parts, with a tokens excerpt of a class whose doc comments stand inside it |
| 3 | `patch` over a byte sequence and a pattern, `s.to_bytes()` and `b.to_text()`: rules, a program, five refusals, the calls and their cost. A link to chapter 7 for `?Match` |
| 4 | The excerpt of `noreturn cold` is left to chapter 5. The walking hooks point to chapter 16, which shows them |
| 5 | `alloc` and `free` are explained where the first example uses them |
| 6 | `anti.error`, `SystemError.from_errno`, `from_win32`, `on_fatal`, `check`, and `debug.backtrace`, with a program and a cost row |
| 8 | `Signal` is named with the section that declares it |
| 9 | `index` and `set_index`: rules, a class and a struct with brackets, four refusals, IR and release excerpts. `concrete fn Base::f`. The atomic operations per type are left to chapter 13 |
| 11 | The example function `take` is `consume`, beside the built-in `take(x.f)` |
| 14 | The Java pitfall of `@GuardedBy` states what the checkers do |
| 15 | A pitfall now agrees that the tables of the `Profiler` stand under a `Mutex` |
| 16 | `[T; N]` and `[[T; W]; H]` are `[N]T` and `[H][W]T`. When `map` and `sort_by` need their type arguments, with the refusal |
| 18 | A blank line had cut a table of messages in two. Two references fixed, and a repeated definition of a library file removed |
| 19 | The internal id R147 gives way to a link to chapter 18 |
| 20 | "The processor levels" and "-g and debugging" gain the five parts. "--target and --cpu" keeps the names and their refusals |
| 21 | Rows for "Source text" and "-g and debugging", and `patch` in the pattern row |

Weights stay 10 to 210 and 1 for the index, and every page stays `draft: true`.

## Examples added

`source` of chapter 2 gains the block `notes` and a tokens excerpt. Chapter 3 gains `patches` and `patchcode` with an llvm excerpt, chapter 6 `syserr`, chapter 9 `cells` with an ir and an llvm excerpt and the qualified `concrete fn`. The refusals added are five of `patch`, four of the index hooks, one of `concrete fn` and one of `map`. The guide holds 180 blocks, 128 programs, 609 refusals and 289 excerpts, against 174, 125, 598 and 286 before.

## The overview against the guide

| Overview section | Guide section with the five parts |
|---|---|
| Program structure | 2 Source text, 5 Declaration and parameters for `main`. 1 Hello is a tool section |
| Modules | 18 Modules and imports, 18 Visibility across modules, 18 Library files |
| Types, Literals | 3 Types and literals, 3 Text and bytes, 12 Slices and views |
| Variables and constants | 3 Variables and constants |
| Operators | 3 Operators, 3 Integers, 3 Floats and f16 |
| Statements, Loops | 4 if and while, 4 for over ranges and collections, 4 switch, 4 break and continue, 4 defer and undo, 7 Reading an optional |
| Functions | 5 Declaration and parameters, 5 never, 5 The calling convention, 5 Inlining, 5 Calls without effects, 4 Paths that end a function |
| Anonymous functions and closures | 5 Anonymous functions and closures, 5 snapshot fn and own fn, 5 Function values |
| Tuples | 3 Tuples |
| Errors | 6 Failing functions, 6 try and handlers, 6 Error origins and stack traces |
| Structs | 8 Structs, 8 packed and align, 8 Unions and bitfields |
| Simd structs, Enums | 8 Simd structs, 8 Enums |
| Variants | 8 Variants, 8 Pattern matching |
| Classes, Interfaces | 9 Classes and the object in memory, 9 construct and destruct, 9 Fields and methods, 9 Interfaces and dispatch |
| Generics, Ownership | the six sections of 10 and the six of 11 |
| Pointers, Optional values | 7 Nullable pointers, 7 Optional values, 7 Reading an optional, 12 Pointers |
| Reflection, Operators on classes, Static fields and singletons | 9 Reflection, 9 Operators, 9 Static fields and singletons |
| Threads, Concurrent classes | the six sections of 13 and the six of 14 |
| Hooks and tracing, Injection, Runtime configuration | 15 Hooks, 15 Tracing, 15 Injection, 15 Runtime configuration |
| Plugins | 15 Plugins, 17 Plugins across the boundary |
| Tests | 19 tests and fixtures blocks |
| C interop | the six sections of 17 |
| Source locations | 5 Defaults and source locations |
| Checks and debugging | 6 assert and the dev checks, 12 Bounds checks, 12 --memory-checks, 20 -g and debugging |
| Processor levels | 20 The processor levels |
| Warnings and safety checks | 6 Warnings, allow and unchecked, and the section of each check |
| Regular expressions, Bytes | 3 Pattern literals, 3 Text and bytes |
| Collections | the ten feature sections of 16, 14 Thread-safe collections |
| Standard library | 15 Injection for the six interfaces, 12 anti.mem, 6 Failing functions for `anti.error` |
| Tools, Reserved words | 18 and 19 anti test as tool sections, 2 Source text |
| Compile-time targets, Wire formats, Script mode | none: not built |

## What the guide does not cover

- `target.os` and the other compile-time targets, the wire formats, named arguments, `embed`, `show` and `type_of(T)`. The script mode of `#!/usr/bin/env anti` belongs here too. None is built, and the guide states no feature that is not. Chapter 20 says that script mode is not built.
- The modules of the standard library outside the six interfaces, `anti.mem`, `anti.error` and the collections, as `anti.json`, `anti.args`, `anti.fs`, `anti.log`, `anti.os`, `anti.time` and `anti.toml`. No chapter of the task holds them, and `index.md` sends a reader to "Standard library" of the overview.
- The commands of `anti` stand as tool sections, without the five parts. They are commands, not forms of the language, and the overview teaches them as tools.

## Older documents against the code

- "Bytes" of the overview says that `patch` allocates nothing. With a pattern, `anti_rt_regex_patch` of `src/rt/patterns.c:711-780` takes the match data of PCRE2 and a block of the places, and frees both before it returns. The guide follows the code.
- "Checks and debugging" of the overview says that `show` is active in dev mode, and "Reserved words" says that `show` is not built.
- The same section of the overview says that a program under `--memory-checks` runs two to three times slower. The measured table of chapter 12 gives 8.1 to 53.5 times for the five programs of `tests/bench`.
- The example test moves a comment that stands outside an item into the body of the function it writes. The header of `tests/runners/run_document_examples.cmake` says so of every such line. A module-level doc comment of a block therefore reaches no item there, which is why the tokens excerpt of chapter 2 uses a class.

## Findings

No defect was recorded. Every probe did what `docs/decisions.md` and the specifications say.

## Questions answered from the code

- Does a `//#` note reach the library file? No. `put_doc` of `src/antic/antl_write.c` writes the doc text alone, and the note stays in the syntax tree.
- Does `concrete fn Base::f` compile for a base class? Yes, and a name that is no base and no interface is refused.
- Does `l.map(f)` infer its types from an anonymous function? Only when the function names its parameter types.
- Does `SystemError.from_errno` own its message? No. `anti_rt_text_kept` of `src/rt/text.c` keeps one copy of each text to the end of the program.

## Gates

- `guide_examples` and `overview_examples` passed, 2 of 2, in `build/drive/logs/grevf-examples.log` and `grevf-examples-counts.log`. The overview ran 63 blocks, 7 programs and 16 refusals, with 9 skipped as not built.
- The docs-style checker reports nothing on the 22 pages, in `build/drive/logs/grevf-style-all.log`, and on this report.
- `git grep` finds no TODO, FIXME or placeholder text in the pages. Its one hit for `tbd` is the file `libSystem.tbd`.
- No suite ran, since the step changed `.md` files under `docs/` alone.
