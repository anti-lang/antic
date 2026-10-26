# Plan of the developer's guide against the code

Date: 2026-10-26, from `e87b42f7`. The step read the 21 chapters of `docs/site/developers-guide/` and `index.md` against the code. It changed no file but this report, and no suite ran. The later steps of this run follow one section per chapter below.

Every claim here was read in the code or shown by a probe. A probe is a program or a project under `build/guide-plan/`, built by the host antic of `e87b42f7`. The scans that found the form findings are the scripts of `build/guide-plan/scan/`.

Contents:

- [The marks of the example test](#the-marks-of-the-example-test)
- [Findings across the guide](#findings-across-the-guide)
- [The index](#the-index)
- [Chapter 1, getting started](#chapter-1-getting-started)
- [Chapter 2, how antic works](#chapter-2-how-antic-works)
- [Chapter 3, values](#chapter-3-values)
- [Chapter 4, flow](#chapter-4-flow)
- [Chapter 5, functions](#chapter-5-functions)
- [Chapter 6, errors](#chapter-6-errors)
- [Chapter 7, optionals](#chapter-7-optionals)
- [Chapter 8, structs and enums](#chapter-8-structs-and-enums)
- [Chapter 9, classes](#chapter-9-classes)
- [Chapter 10, generics](#chapter-10-generics)
- [Chapter 11, ownership](#chapter-11-ownership)
- [Chapter 12, memory](#chapter-12-memory)
- [Chapter 13, threads](#chapter-13-threads)
- [Chapter 14, concurrent classes](#chapter-14-concurrent-classes)
- [Chapter 15, hooks](#chapter-15-hooks)
- [Chapter 16, collections](#chapter-16-collections)
- [Chapter 17, C interop](#chapter-17-c-interop)
- [Chapter 18, the tool](#chapter-18-the-tool)
- [Chapter 19, tests](#chapter-19-tests)
- [Chapter 20, targets](#chapter-20-targets)
- [Chapter 21, performance](#chapter-21-performance)
- [The overview against the guide](#the-overview-against-the-guide)
- [Older documents](#older-documents)
- [Questions answered from the code](#questions-answered-from-the-code)
- [Open points for Eddie](#open-points-for-eddie)
- [Gates](#gates)

## The marks of the example test

The header of `tests/runners/run_document_examples.cmake` describes every mark of the task, and the code of the runner does what the header says. `build/guide-plan/proof/base.md` holds each mark once. `build/guide-plan/proof/pages/` holds 30 copies of it with one change each, beside the intact one. Each log is `build/drive/logs/guide-plan-proof-<copy>.log`.

| Copy | Result |
|---|---|
| `intact` | passes: 1 block, 1 program, 1 refusal, 5 excerpts |
| `output`, `output_extra`, `output_missing_line`, `no_output_block` | fail: the standard output differs |
| `error_text`, `refused_compiles`, `refused_no_error`, `error_unrefused` | fail with the message of each |
| `llvm_dev_line`, `llvm_dev_order`, `llvm_dev_adjacent`, `llvm_release_line`, `llvm_release_before_opt` | fail: the line does not stand where the excerpt puts it |
| `asm_line`, `asm_adjacent`, `option_line`, `dump_line`, `dump_option` | fail with the message of each |
| `too_long`, `unknown_block`, `name_twice`, `name_not_first`, `bad_header`, `bad_target`, `excerpt_before_block` | fail with the message of each |
| `no_compile`, `warning` | fail: an example does not compile, or prints a warning |
| `asm_blanks`, `dump_line_number` | pass, since runs of blanks and the line of a token are not compared |

Three behaviours of the runner differ from the words of the task. None makes a mark fail its description in the header.

- Two lines of an excerpt without `...` between them must stand on two adjacent lines of the output. Each line matches as a substring of its output line. Chapter 2 does not say either.
- The normalisation makes `%12`, `.LBB0_3` and a decimal line label equal. antic names its values `%v12`, `%t3`, `%s0` and its blocks `b4`, and these stay literal. 506 lines of the llvm, ir and opt excerpts of the guide pin such names. The copy `llvm_dev_numbered` fails for `%v9` in place of `%v2`. A change of lowering that renumbers values therefore breaks excerpts whose code did not change.
- A block without `main` goes through `--front-end --tests` alone. A block that the front end accepts and the lowering or the whole-program pass would refuse passes as compiled, unless an excerpt names it.

The address of an objdump line is hex, as `1c:`, and the normalisation reads decimal digits alone. No excerpt of the guide copies an address, so nothing fails by it.

## Findings across the guide

### Form of the sections

All 87 feature sections hold the five part headings in order. Two sections add headings between the parts.

- `11-ownership.md:508` and `:562`, "References to locals" and "Uses after the end", stand between "What it costs" and "Pitfalls" of "own and lent".
- `14-concurrent-classes.md:1204` and `:1249`, "The check at compile time" and "A wait under a lock", stand between "The feature" and "How antic builds it" of "Lock order checks".

### Where it lives

A script compared every file and every name of the 108 "Where it lives" lines with `src/`, and every path the guide names with the tree.

- `15-hooks.md:253` names `root_names` of `src/antic/lower_desc.c`. It went in `63c503a5` of 2026-10-18. The functions of the root and the hooks are `sema_root_fns` of `src/antic/sema_class.c`.
- `20-targets.md:120` names `link_runtime_library` of `src/antic/driver_link.c`. The function is defined in `src/antic/linker.c`, and the driver files call it.
- Every other file exists, and every other name stands in a file its line names. `size_of` and `offset_of` of `08-structs-enums.md:283` name symbols of the IR and no function.

### How antic builds it

These sections open with a pass of the compiler where the task asks for the model of what runs. The pass may stay where it changes the program.

- `03-values.md:688`, Integers: opens with `addov` and `branchov` of lowering.
- `04-flow.md:1291`, defer and undo: opens with the list of exit actions that lowering keeps.
- `06-errors.md:994` and `:1020`, assert and the dev checks: lowering and the driver.
- `09-classes.md:1781`, Operators: the checker rewrites the operator.
- `10-generics.md:240`, Generic functions and classes: the checker clones the tree.
- `12-memory.md:787` and `:812`, Bounds checks: lowering and a pass of a release build.
- `15-hooks.md:1287`, Plugins: what the build with `--no-runtime` emits.
- `19-tests.md:225`, tests and fixtures blocks: the parser and the driver.

### What it costs without the choice

These sections do not close with the other forms of Anti that do the same job. A later step closes each with the choice its counts support.

- Chapter 3: Types and literals, Variables and constants, Integers, Floats and f16, Text and bytes, Tuples, Type inference.
- Chapter 4: if and while, switch, break and continue.
- Chapter 5: Declaration and parameters, where the choice stands mid-section, The calling convention, Inlining, Anonymous functions and closures, snapshot fn and own fn, Function values.
- Chapter 6: try and handlers, Error origins and stack traces, never, assert and the dev checks.
- Chapter 8: Structs, Unions and bitfields, Simd structs, Enums, Pattern matching.
- Chapter 9: Classes and the object in memory, construct and destruct, Fields and methods, Interfaces and dispatch, Static fields and singletons, Reflection.
- Chapter 10: Generic functions and classes, Constraints, Generic declaration forms.
- Chapter 11: own and lent, dup and moves, Teardown and temporaries.
- Chapter 12: Pointers, Bounds checks.
- Chapter 13: all five sections.
- Chapter 14: Synchronized classes, Concurrent classes, Lock order checks, Thread-safe collections.
- Chapter 15: Hooks, Tracing, Runtime configuration.
- Chapter 16: all ten feature sections.
- Chapter 17: extern fn and C types, The ABI of a call, anti bind, Libraries for C.
- Chapter 19: tests and fixtures blocks.

The task names three choices that belong here. They are a struct beside a class, a generic copy beside a call through an interface, and a List beside a Ring or a Pool.

### Numbers of an older build

These numbers come from a build older than the code, and most name no commit. A later step measures them again and names the commit, or keeps the equality and drops the number.

- `02-how-antic-works.md:367-386`: the sizes of `walk` at `ca620e14`. At `e87b42f7` they are 228,800, 261,256 and 262,440 bytes against 227,768, 257,368 and 258,488.
- `18-the-tool.md:329-332`, `19-tests.md:298`, `20-targets.md:243-250`, `:329` and `:351-355`, and `21-performance.md:211`.

## The index

`index.md` lists the 21 chapters in the order and under the slugs of the task. Its text differs from the task in four places.

- The line of each chapter omits subjects of the task. Chapter 3 lacks `f"..."`, pattern literals, the flags form and `mul_high`. Chapter 4 lacks `x in lo..hi` and the paths of C3. Chapter 5 lacks `never`, `here` and calls without effects. Chapter 6 lacks the warnings, `allow`, `unchecked` and `--warnings-as-errors`, and lists `never`. Chapter 18 lacks modules, imports, visibility and library files.
- The lines of chapters 8 to 14 omit the default `==` and hash, nested types, constant parameters and generics of library files. They omit `take(x.f)`, the safety checks and the checks C2, C4 and C5 as well.
- "Subjects outside the guide" sends modules, regular expressions and warnings to the overview. The task puts them into chapters 18, 3 and 6.
- "Section layout" describes "How antic builds it" as what the front end turns the feature into. The task asks for the model of what runs, the excerpt and the line "Where it lives:".

## Chapter 1, getting started

Stale or wrong:

- `:123`: `-I` "module `a.b` is `a/b.anti` or `a/b.antl`". `search_roots` of `src/antic/driver_search.c` finds the library file alone.
- `:178`: "The guide has no chapter on modules". Chapter 18 gets that section by the task.
- `:227`: "The key is one line". The key holds that line, the lines of `options_key` and one `import <path> <digest>` line per import, by `src/anti/build.c:121-470`.
- `:272`: the dev message names `grow.anti:6`. The file as shown gives line 5. The probe of `build/guide-plan/c1/grow` prints `com/example/grow.anti:5: overflow in *: left 2, right 4611686018427387904`.

To add: the exit statuses 1, 2 and 70 of antic beside the table of options.

## Chapter 2, how antic works

To add:

- A row or a paragraph on the safety checks in the table of passes. `escaping-local` is `src/antic/sema_escape.c`, `use-after-free` `sema_free.c` and `uninitialized-read` `sema_uninit.c`. `index-out-of-range` is `sema_index.c`, `lock-order` and `blocking-under-lock` `sema_lock.c`, and `unused-result` stands in `sema_stmt.c`. The lock graph spans the modules through the library files.
- A tool section "Source text": comments and the block forms of doc comments, ASCII identifiers, the byte order mark and CR LF in a literal. It holds the reserved and the contextual words as well. The overview holds each, and no chapter does.

Stale or wrong:

- `:164`: "The front end builds the `str` there". Lowering does.
- `:367-386`: the sizes, see "Numbers of an older build".
- `:435`: "Reading the excerpts" omits the adjacency of excerpt lines and the substring match. It does not say that antic's numbered names are compared as written.

Verified: the start order of `src/rt/start.c:124-134`, the inline threshold 225 of `src/antic/llvm_run.c:166`, `-O1` and `-O2` of llc, the four functions of `--dump-opt`, and the tools of `bin/`.

## Chapter 3, values

To add:

- A section "Pattern literals", which `:1082` sends to the overview. `re"..."` has type `Regex`, and `ByteRegex` follows from where the literal stands. Each module compiles its literals once before `main`. A malformed literal is refused at the byte PCRE2 names. The safety check `exponential-pattern` narrows by R140. A literal at the call cannot fail and stops at the match limit, and any other pattern may fail. A constant of type `Regex` is refused. Groups are fields of a `Match`. The cost covers `size_of(Regex)`, `size_of(Match)`, the link of PCRE2 and a call per match. Sources: `src/antic/pattern.c`, `src/antic/sema_pattern.c`, `src/antic/lower_pattern.c`, `src/std/anti/regex.anti` and `src/rt/regex.c`.
- A section or a subsection "Operators": the precedence table with `is`, `/` toward zero, `%` by the dividend, the arithmetic `>>`, `bool as` an integer, and the order of evaluation. No chapter holds the precedence.
- Variables: no module-level `let`, globals are constants, and a static atomic field is the one written global.
- Integers: the assignment `(r, f) = e;` and R041. The flags operation is no constant expression, by the entry that closed T180.
- Text: `fr"` is refused with "`fr"` is not a prefix, write `rf"`". An enum and a `?*T` are refused in `{expr}`, and an `f"..."` is no constant. Memory that runs out in a builder stops the program, by decision 4.
- Tuples, `:1337`: `for (k, v) in e` is a third place that takes a tuple apart.
- Type inference, `:1502`: a literal of a generic struct infers from its fields, by R066. Q19 belongs to chapter 10.

Stale or wrong:

- `:314`: a constant of a class body is `Name.NAME`. By Q20 the bare name reaches it inside the class, as `09-classes.md:693` says.

Verified by `build/guide-plan/c3/chk.anti`: the dev messages of `/` by -1, unary `-`, `<<%` by -1 and `as char`.

## Chapter 4, flow

To add:

- A section "Paths that end a function", which the task names as C3. Now `:105` holds one paragraph and `:167` one refusal. The section takes the five kinds of C3 and the forms of R003. It adds the correction of 2026-10-25 for `do ... while`, `select` and a `try` block, and `yield;`. A `break` or a `continue` in the handler of a call in a loop condition belongs to that loop. The handler of a call statement may reach its end, by R024. Lowering ends such a block in `IR_UNREACHABLE`.
- defer and undo: the warning `undo-never-runs` of R005, and the refusal of an `undo` inside a `defer`.
- switch: an arm is any call statement, `A => free(p)`, by decision 17. `size_of(i64)` and `8` are one value, by R010. Two enum values of one number cannot both have arms, by R053.

Stale or wrong:

- `:1362`: "A `return`, a `break`, a `continue`, a `fail` or a `try` inside a deferred statement is not refused, and antic 0.2.0 crashes on it." `:1276` and the refused block after it show the refusal of decision 19.

## Chapter 5, functions

To add:

- A section "Calls without effects", which the task names as C6. The warning is `unused-result`. It knows the default hash without an operator, `mul_high` and an `extern fn` of a runtime row without effects. It warns of no simd built-in. It warns once in a generic. `:29`, "The result of a call may be dropped", needs it.
- A section "never", which `06-errors.md:772` holds today. The task puts it here.
- Defaults: an `extern fn` takes no default, and a call through a function value gives every argument.
- Closures: an anonymous function stands in a function body alone, and its `break` reaches no loop around it. A captured `?*T` keeps its declared type. A closure in the block of an `if let` reads the narrowed value when nothing in the block assigns the name, by the entry that closed T004.
- snapshot fn: R090 refuses a snapshot that names an atomic local. R084 lets an `own` field of type `?fn(...)` take `none`, and `=` frees what it held. R097 holds at a `concurrent` parameter.
- Function values: a failing function that gives a function needs a `type` alias, by R023.

## Chapter 6, errors

To add:

- A section "Warnings, allow and unchecked". It names the warnings and the safety checks of `src/antic/warnings.c`. It places `allow` and `unchecked` before a statement, last in a header, after the type of a field, at the top of the file and among the imports, by R122. It holds `unused-allow` and `unused-unchecked`, the refusal of every warning by a release build and `--warnings-as-errors` for dev and `anti check`.
- try and handlers: `catch fatal` in a worker ends the program and runs no `defer`, by R021. `yield v` moves a fresh value and refuses an owning one with the message of `=`, by the entry that closed T038. A failing worker is called with a handler, by R013.
- Error origins: the function of an origin is `m.f.0`, `m.f<int>` or `m.Outer.Inner.f`, by R019.
- assert and the dev checks, `:926`: the table lacks the checks of R027, R031, R032, R048, R087 and R093. It lacks the slice part of T017 and the second `join` of T054. A link to the complete list of chapter 21 also serves.

Stale or wrong:

- `:99`: "The error does not own its message." That holds for `Error.new` and `fail "literal"`. `fail` of an `f"..."` takes its memory over, and `fail` of any other `str` copies it, by the entry on R017.

## Chapter 7, optionals

Stale or wrong:

- `:428`: "antic 0.2.0 accepts `let x = try e;` on an optional". Decision 21 refuses it. `build/guide-plan/c7/t.anti` prints "`try` forwards an error, and a value that may be `none` has none: use `let ... else` or `??`". The text becomes a refused block.

To add: a `?U` passes where a `??U` is expected, by the entry that closed T048. `dup` of a `?T` needs its test, by R071. `as ?T` is refused for `as? T`, by R058.

## Chapter 8, structs and enums

To add:

- Simd structs: the constraints a simd struct meets, by Q18, which the task names. A simd struct meets `add`, `sub`, `mul`, `div` and `neg` where its lanes take the operator. It does not meet `lt` or `Number`, a mask meets none, and the other hooks stay unmet. Chapter 10 states it at `:338`, and this section shows it with an example.
- packed and align: `packed` keeps the padding before an atomic field and a field that holds a Mutex, by the entry that closed T022. R060 refuses `packed` before `enum`, `simd struct` and `synchronized class`.
- Structs: a field of function type wins over a method of its name. R035 refuses `==` on a struct that holds a simd struct. R036 and R099 complete the default operators.
- Unions: method-call sugar does not reach a union. A union may declare `operator fn eq`, and the default `==` of its struct then compares through it, by the entry on R034.
- Enums: an empty enum has the size of `c_int`, by R052, and two values may share a number, by R053. `n as E` is no constant expression, and a value from `size_of` is refused. The functions of an enum body wait for Eddie's answer, see "Open points for Eddie".
- Variants: no constant or default has a variant type, by R040. R056 and R057 hold, and `v is Maybe<int>.Some` names a case of a generic variant.

Stale or wrong:

- `:466`: the header writes `ANTI_ALIGNAS(N)` alone. `ANTI_ALIGNED(N)` stands on the type for the `align(N)` of a packed type, a variant and any class, and a packed class has its pack, by `src/antic/header.c:574-588`.

## Chapter 9, classes

Stale or wrong:

- `:1870`: "A class that replaces `hash` and keeps the default `==`, or the reverse, breaks the rule", and "Nothing checks it." `require_equals_and_hash` of `src/antic/sema_class.c:729` refuses both, by decision 27.
- `:656`: "A class whose chain declares no `destruct` and owns nothing has no teardown". A class that replaces `destroyed` has one, by R115.

To add:

- The refusals of decision 27 that the chapter lacks. They are an `abstract fn` in a class not marked `abstract`, a direct call of `destruct`, `mutable` on a plain class and `static atomic` of an `f64`. They include `==` on pointers of two unrelated classes as well.
- R117, R105 and R034 on operators, R083 on a part of a singleton, R139 and R119.
- Static fields, `:1351`: an atomic field takes a `char`, an `f16` and a plain function pointer as well, by the entry that closed T059.
- Reflection: `Payload` of `anti.reflect`.

Built and stated: the bare constant of Q20 at `:693`, R100 at `:701`, R025 and R114 at `:367`.

## Chapter 10, generics

To add:

- A section "Generics of library files", which the task names. A library file carries the checked tree, and a module makes its copies under the path of the generic's module. A library file carries the constants of a generic class, which a copy computes with its arguments, by the decision of 2026-10-26 that closed T202. What `sema_strip_generics` keeps out stays refused with the message of `:812`.
- A section or a part "Constant parameters", which the task names. It holds R045 and the default of T125. R029 is not checked again in a copy, and a copy refuses `<<%` by a count below zero, by Q13.
- Refusals: R060 for a union and a `type` alias, R065 with its message, R067, and R069. `--front-end` and `anti check` compile the copies, by R068.

Built and stated: Q19 at `:34`, R066 at `:38`, Q18 at `:338`, and `--icf=safe` at `:756`.

## Chapter 11, ownership

Form: "References to locals" and "Uses after the end" become one feature section with the five parts. Its cost is nothing at run time. It names what the run-time checks keep, `--memory-checks` and the versions of a `Pool`, and closes with `unchecked` against a change of the code.

Stale or wrong:

- `:36`: "A program that stores the address of a local there, or one pointer in two `own` fields, compiles." `escaping-local` refuses the store into a place that a parameter or `self` reaches. A local stored into an `own` field is dead after it, by the entry on E2 of 2026-10-24. The sentence holds for memory the function allocated alone. `:623` repeats it.

To add:

- dup and moves, `:649`: a local moves into an `own` field by `=` or by a literal, into an array literal by R079, through `*slot(to) = b` by R082, and by `yield`.
- E2: a dead `own` field of `self` on one path makes `=` tear nothing down, which leaks. E2 reads a `defer` at each exit and a closure where it stands. One death gives one message.

Built and stated: `take(x.f)` at `:28` and `:300`, R074, R081, R083, E1 with its correction for `parallel` and `dispatch`, and E2.

## Chapter 12, memory

Stale or wrong:

- `:83`: "A pointer to a local may be returned, and a pointer may be read after free. Both compile". `escaping-local` refuses the first. `build/guide-plan/c12/s.anti` returns a slice of a local array and prints "a reference to `a` is returned, and `a` ends at line 5 [escaping-local]". The read after `free` compiles, as `f.anti` shows.
- `:667`: the Java pitfall says the checker does not refuse the return of a slice of a local array. It does.

Built and stated: C1 at `:43` and C2 at `:704`.

## Chapter 13, threads

Stale or wrong:

- `:439`: "`by n` with `n` of zero or less takes the count of the workers". R087 refuses a constant below zero and stops a dev build for one at run time. A release build takes it as zero, and `by 0` stays the count of the machine.
- `:373`: "The arguments of a job stand in the frame of the function that wrote the `dispatch`". `:342`, `:409` and decision 23 say that the job holds a copy, as `anti_rt_dispatch` does. The excerpt at `:328` shows the copy, and the prose after it no longer explains it.
- `:1438`: "an atomic field of `f64` fails in opt with an error of LLVM". `build/guide-plan/c13/a.anti` prints "an atomic field holds an integer, a `bool`, a `char` or a pointer, found `f64`".

To add:

- Locks: "Waits under a lock", which the task names as C5 for this chapter. `14-concurrent-classes.md:1249` holds it today.
- The worker of another module after the arrow is `m.f`, by R086. `dispatch` and `parallel` fill in the defaults of the worker, by R088. A job tears down its arguments, by the entries that closed T039, T040 and T128.
- R077, R096, and the link to `escaping-local` for `dispatch` of the address of a local.
- Atomics, `:1097`: the types gain `f16` and a plain function pointer. The refusals of T060 read as "`add` takes an atomic integer, and `on` holds `bool`", which `b.anti` shows.

## Chapter 14, concurrent classes

Form: "The check at compile time" goes into "The feature", or into a section of its own. "A wait under a lock" moves to chapter 13 with a link from here.

Stale or wrong:

- `:728`: "Two functions that take two locks in opposite orders compile". `lock-order` refuses two `Mutex` fields taken both ways, as `:1210` shows. What it cannot see is two objects of one class, a call through a table, a function value or a plugin, and a `Mutex` that no field names.
- `:242`: two objects of one class locked in opposite orders. Add that C4 sees no cycle there, since an edge from a lock to itself is none.

To add: the cost of C4 is nothing at run time. A library file carries the edges and the locks of each function.

## Chapter 15, hooks

Stale or wrong: `:253` names `root_names`, see "Where it lives".

To add:

- A write to a `transient` field calls no `changed` hook, by R120.
- A plugin that uses a module its host does not import fails with "<path> uses `anti.time`, which this program does not import", by R133.
- A host keeps every function and datum of its imports, by the entry that closed T093.
- A provider path that is the `get` of a singleton is refused, by R119.

## Chapter 16, collections

Stale or wrong:

- `:394`: "The checker does not refuse a write to it". Decision 25 refuses `=`, a compound assignment and `&` of `count` outside the module, with "`count` of `List<int>` is read-only outside its module".
- `:2214`: "One without the other gives two equal objects in two buckets". The checker refuses one without the other.

To add: `deserialize` of a collection is not built. An `own` argument of a function of a collection counts as stored into it for `escaping-local`. A collection of plain pointers owns the pointers, by R081.

## Chapter 17, C interop

Stale or wrong:

- `:310`: `export` stands before `fn`, `struct`, `union`, `const` and `type`. It takes `class`, `enum` and `variant` as well.
- `:1050`: "The start of a static library is at its first use and not before `main`." `:927` and decision 5 give the constructor at load.
- `:1022`: the header table lacks `ANTI_ALIGNED(N)`, see chapter 8.

To add: R127 one signature per C function, R129, and the refusal of a default and of `lent`, `concurrent` and `keep own` on an `extern fn`. R131 `nothing-exported`, the three refusals of R132, R150, R151 and R152 complete "Libraries for C".

## Chapter 18, the tool

To add, three sections the task names:

- "Modules and imports". It covers the module path under a search root by R147, and `-I` with the library file of an import. It covers the import forms, the direct import and its clash, the alias, R121, R123 and R124. The cycle of R125, a keyword as a segment, and the module each form needs complete it.
- "Visibility across modules". It holds the four levels and `protected`, R139 and R100.
- "Library files". It names what a `.antl` carries: the interface, the checked generics and the IR. It adds the singleton mark, the private names, and the constants and static fields of a class. It adds a field default that names a function, the lock edges and first waits, and the type spellings. One file serves six targets, and `--strip-docs` drops the doc text.

Stale or wrong:

- `:517`, `:495` and `:500`: "`f64` reads `float`, its other name". By R178 a page writes the type as the source spells it. `build/guide-plan/c18` prints `pub fn feet(metres: f64) -&gt; f64`.

To add to the tables of messages: R156 to R160 for `anti build` and `anti run`, R164 to R168 for the manifest and the lock file, Q5, and Q15. `anti doc` gains R179 and R180.

## Chapter 19, tests

Stale or wrong:

- `:355`: "`-I src` matches `src/com/example/purse.anti` and does not match `./src/com/example/purse.anti`, whose module is then `purse` alone." By R147 the probe of `build/guide-plan/c19` names a file `./src/com/example/change.anti` and prints `ok com.example.change.ten_is_two_fives`.
- `:436`, `:439` and `:492`: the end of a runner reads `antic: ... ended by signal 6`, "and its prefix is `antic` although the tool is `anti`". The run now prints `anti: build/tests/com_example_change.runner ended by signal 6`.

Verified: a `setup` with a parameter compiles and its runner fails, `:219`. A dev run of a module with `main` fails at the link, `:523`.

## Chapter 20, targets

Stale or wrong: `:120` names the file of `link_runtime_library`, see "Where it lives". The numbers of `:243-250`, `:329` and `:351-355` come from an older build.

Verified: the target table, script mode not built, and no variables under `-g`.

## Chapter 21, performance

To add:

- Rows of the first table for every section the plan adds, and the row of `never` under its new chapter.
- The checks of R027, R031, R032, R048, R087, R093, T017 and T054 in "Checks of a release build".
- A table of the safety checks that stand in every build. They are `escaping-local`, `use-after-free`, `uninitialized-read`, `index-out-of-range`, `lock-order`, `unguarded-field` and `exponential-pattern`. Each `unchecked(name, "reason")` overrules one and gives up its proof. `blocking-under-lock` and `unused-result` complete it as warnings.

The bench tables name `a9e8ffd0` and `2d850efa` and stay true of them.

## The overview against the guide

Each section of `docs/anti-syntax-overview.md` with the guide section that teaches it. "Add" names the section this plan adds.

| Overview section | Guide section |
|---|---|
| Program structure | 1 Hello, for `main`. Add: 2 Source text |
| Modules | Add: 18 Modules and imports |
| Types | 3 Types and literals, 12 Slices and views |
| Literals | 3 Types and literals, 3 Text and bytes |
| Variables and constants | 3 Variables and constants |
| Operators | 3 Integers, 3 Floats and f16. Add: 3 Operators |
| Statements | 4 if and while, 4 switch, 7 Reading an optional, 3 Integers |
| Loops | 4 if and while, 4 for over ranges and collections, 4 break and continue |
| Functions | 5 Declaration and parameters, 5 Defaults and source locations |
| Anonymous functions and closures | 5 Anonymous functions and closures, 5 snapshot fn and own fn |
| Tuples | 3 Tuples |
| Errors | 6 Failing functions, 6 try and handlers |
| Structs | 8 Structs |
| Simd structs | 8 Simd structs |
| Enums | 8 Enums |
| Variants | 8 Variants, 8 Pattern matching |
| Classes | 9 Classes and the object in memory, 9 construct and destruct, 9 Fields and methods |
| Interfaces | 9 Interfaces and dispatch |
| Generics | 10, all four sections. Add: 10 Generics of library files |
| Ownership | 11, all sections |
| Pointers | 7 Nullable pointers, 12 Pointers |
| Optional values | 7 Optional values, 7 Reading an optional |
| Reflection | 9 Reflection |
| Operators on classes | 9 Operators |
| Static fields and singletons | 9 Static fields and singletons |
| Threads | 13, all sections |
| Concurrent classes | 14, all sections |
| Hooks and tracing | 15 Hooks, 15 Tracing |
| Injection | 15 Injection |
| Plugins | 15 Plugins, 17 Plugins across the boundary |
| Runtime configuration | 15 Runtime configuration |
| Tests | 19 tests and fixtures blocks, 19 anti test |
| C interop | 17, all sections |
| Compile-time targets | none. Not built, and the guide states no feature that is not |
| Source locations | 5 Defaults and source locations |
| Checks and debugging | 6 assert and the dev checks, 20 -g and debugging, 21 Checks of a release build |
| Processor levels | 20 The processor levels |
| Warnings and safety checks | Add: 6 Warnings, allow and unchecked |
| Wire formats | none. Not built |
| Script mode | 20 Script mode |
| Regular expressions | Add: 3 Pattern literals |
| Bytes | 3 Text and bytes. The byte patterns go to 3 Pattern literals |
| Collections | 16, all sections |
| Standard library | 15 Injection for the six standard interfaces. No chapter of the task holds the other modules |
| Tools | 1 antic and anti, 18, 19 anti test, 20 --target and --cpu |
| Reserved words | Add: 2 Source text |

## Older documents

- C1 of "Compile-time checks of 2026-10-20" in `docs/decisions.md` says that `alloc(T, n)` "zeroes" its elements. `docs/anti-object-model.md:186` and the code zero a class or an owning element alone. The excerpt at `12-memory.md:273` passes 0 for an `int`. The guide follows the code.
- `index.md` sends modules, patterns and warnings to the overview, against the chapter list of this run.

No defect showed. Every probe did what `docs/decisions.md` and the specifications say, so no row went into `docs/audit/tests-findings.md`.

## Questions answered from the code

- Does an excerpt match a line exactly? No. Each line matches as a substring, and lines without `...` between them stand on adjacent lines.
- Does the test lower a block without `main`? No. It runs `--front-end --tests` on it.
- Does `anti check` without a file check the project? Yes, every source of `[layout]`, as the probe of `build/guide-plan/c18/units` shows.
- Does the cache key of `anti build` hold the imports? Yes, one `import <path> <digest>` line per library file.

## Open points for Eddie

- The functions and constants of an enum body compile, and the overview describes them. The question of `docs/reports/2026-10-26-docs-overview-gaps.md`, whether the form stays, has no answer yet. Chapter 8 waits on it.
- The modules of the standard library beyond the six interfaces, as `anti.json`, `anti.args`, `anti.fs` and `anti.log`, have no chapter in the list of this run.
- The plan is longer than two pages, since the task asks for every finding per chapter.

## Gates

- `guide_examples` and `overview_examples` passed, 2 of 2, before the report and again after it, in `build/drive/logs/guide-plan-examples-start.log` and `build/drive/logs/guide-plan-examples-gate.log`. The guide ran 133 blocks, 96 programs, 458 refusals and 246 excerpts. The overview ran 63 blocks, 7 programs and 16 refusals, with 9 skipped as not built.
- The docs-style checker reports nothing on this report.
- No suite ran, since this report is the one changed file.
