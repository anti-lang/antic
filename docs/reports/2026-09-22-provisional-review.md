# Provisional entries for review

`docs/decisions.md` holds 169 `[provisional]` entries at commit `6bdfe95`. This report sorts them and changes none.

- A-review, a real choice or a rule a program depends on: 38. This is the list to go through.
- A-settled, a built detail that agrees with the specifications: 108.
- B, internal: 23.

The numbers R1 to R38, S1 to S108 and B1 to B23 replace those of the first version of this report, which had 171 entries. The two it flagged are resolved: the handler rule is corrected in `22f2927`, and the refusal of `f"..."` is gone in `0a81962`. Each entry names the line it stands on at that commit. R14 lost the reason that no Windows machine runs the suite in `75ad1fe`. A-review carries the text of each entry. A-settled and B carry the line on what each decides, and the file holds the text.

| Section | A-review | A-settled | B |
|---|---|---|---|
| Scope and toolchain | 0 | 0 | 1 |
| Core language | 2 | 1 | 0 |
| Object model | 12 | 7 | 5 |
| Numeric types | 1 | 3 | 0 |
| Expressions and types | 1 | 11 | 0 |
| Lexical rules | 0 | 2 | 0 |
| Libraries and runtime | 3 | 6 | 2 |
| Threading | 1 | 2 | 0 |
| Standard library phase | 7 | 5 | 2 |
| Build tool and distribution | 0 | 8 | 2 |
| Compiler behaviour | 0 | 5 | 4 |
| Nullable pointers | 2 | 1 | 1 |
| Failing functions | 0 | 8 | 1 |
| Tuples | 0 | 5 | 1 |
| Default values and source locations | 0 | 5 | 0 |
| Error origins and stack traces | 0 | 10 | 1 |
| Build ids | 0 | 2 | 0 |
| Tests and fixtures | 1 | 3 | 2 |
| The release script | 0 | 2 | 0 |
| Fallthrough | 0 | 2 | 0 |
| String prefixes | 0 | 1 | 0 |
| Interpolation | 4 | 7 | 0 |
| `f16` | 2 | 5 | 1 |
| Small things | 2 | 7 | 0 |

## Staleness checks

The first split marked R14 and S2 as possibly stale. R14 lost its false reason in `75ad1fe`. S2 is live, as below, and is marked where it stands.

- S2. The checker still holds this rule. `is_failing` in `src/sema.c` reads any pointer to `Error` or a subclass as a failing result and ignores whether it is nullable. `makes_error` then exempts the static functions of `Error` and its subclasses, which keeps `Error.new` an ordinary call. The rule is live, and dropping it would make every call of `Error.new` need a handler. The finding under Notes says more.

## Notes

- The corrected handler rule is not built. Item 20 of the first sessions in `CLAUDE.md` carries the fix and its test.
- `may fail` is lowered in `function_type` of `src/sema.c` into a `?*Error` result and an out parameter. Every later check of a call reads the result type through `is_failing`, which takes `*Error` as well as `?*Error`. A hand-written `-> *Error` outside the classes of `Error` therefore still reads as a failing function. The additions document gives a failing function the result `?*Error` alone.

## A-review

### Core language

**R1**, line 52. A module path comes from the source path under the first `-I` root, and an import finds `<root>/<path>.antl`.

> antic derives the module path of its input from its source path under the first root that holds it. Slashes become dots, and `.anti` is dropped. Outside every root the path is the file name alone. An import without a library file on the command line resolves to `<root>/<path>.antl` in the first root that has it. Reason: the path mirrors the directory tree, as decided.

**R2**, line 62. `by -k` walks the values of `by k` in reverse, starting at the largest of them.

> `by -k` walks the values of `by k` in reverse, and it starts at the largest of them rather than at the high bound. Reason: the work order gives both a rule and a procedure. The rule is that the two forms walk the same values, and the procedure starts at `hi` and decrements. The two agree only when `k` divides the span, and `for i in 0..10 by -3` gives `7 4 1` under the procedure and `9 6 3 0` under the rule. The rule is what a program relies on, so it wins. Lowering computes the first value as `low + ((high - low - 1) / k) * k`, one division by a constant before the loop.

### Object model

**R3**, line 75. A `use` field and an `implements` sub-object have no visibility level of their own.

> A `use` field and an `implements` sub-object carry no visibility level of their own. Reason: the specification says a literal outside the class names public fields alone, and its own example names the `use` field `rect` of `Sprite` from `main`. Those fields are the shape of the class rather than data it hides, and what they promote keeps its own level.

**R4**, line 77. A failing function returns its result through its last parameter.

> A failing function writes its result through its last parameter. A call that gives one argument fewer leaves that place to the compiler, which passes the address of what the `let` declares. Reason: the specification says the compiler supplies the out pointer and does not say where it goes. The last place is the one a reader expects after the arguments, and the arity tells the checker which call means it.

**R5**, line 95. The signatures and results of `reflect.get` and `reflect.set`, and the error codes of `set`.

> `reflect.get(object, d, index) -> Value` gives the field as the `Value` its type gives: `Int` for a signed integer and an enum over one, `Uint` for an unsigned one, `Float`, `Bool`, `Char`, `Str`, and `Ptr` for a pointer or a function pointer. A field of any other type and an index out of range give `None`. `reflect.set(object, d, index, value) may fail` takes a `Value` of that kind and narrows it to the field's type. It fails with code `BAD_INDEX`, `NOT_CARRIED` or `WRONG_KIND` and writes nothing when it cannot. Reason: `set` can fail, so it follows the error convention. The only failure of `get` is that no value is there, which `None` says.

**R6**, line 97. The kinds of `Value`, with every integer carried at 64 bits by signedness and `f32` as `Float`.

> The kinds of `Value` are `None`, `Int`, `Uint`, `Float`, `Bool`, `Char`, `Str` and `Ptr`. A signed integer of any width travels as `Int` at 64 bits, an unsigned one as `Uint`, and `f32` as `Float` at 64 bits. Reason: the specification lists the primitive types, `str` and pointers, and one kind per signedness keeps a caller from naming the width of each parameter.

**R7**, line 103. `reflect.new` takes `Class` or `module.Class` and gives `none` for a class it cannot build.

> `reflect.new(name)` takes the class's own name or `module.Class`. A name that two modules declare needs the module path. An abstract class, a singleton, a class whose `construct` takes arguments and an unknown name give `none`. Reason: the descriptor holds the name without its module, and `new` has no arguments for such a `construct`.

**R8**, line 104. `Object.deserialize` gives `none` on input that is no object of the program, not an error.

> `Object.deserialize(input: str) -> *Object` gives `none` when the text is not an object of a class of the program, instead of an error. Reason: the runtime owns `anti.rt`, and `anti.lang.Error` lives in the standard library, which `anti.rt` cannot name.

**R9**, line 105. How `Object.deserialize` rebuilds an object from its JSON, and which inputs fail the whole text.

> `Object.deserialize` builds the class that the member `type` names, prepared as a literal of the class would be, and fills the fields from the other members. A `construct` with arguments does not run, because the fields come from the text. A member that names no field is skipped. A field that `serialize` writes as `null` keeps its default. An `own` pointer and an `own` slice come back in new memory, and any other pointer or slice as the address that was written. A number outside the range of its field, a `bool` written as a number and a `char` of more than one character fail the whole text. A `type` of a class that cannot be the field's class gives `none`. Reason: the reader is the counterpart of the default `serialize`, and those are the forms it writes.

**R10**, line 107. `Object.deserialize` leaks the bytes of `str` fields until a form with an `Allocator` replaces it.

> `Object.deserialize` gives a `str` field bytes of its own on the heap, which nothing frees. Until `anti.mem` exists they come from libc. The form with an `Allocator` replaces this one. `Object.deserialize` then takes an `Allocator`, and every string and every owned sub-object it creates comes from it. The caller frees that memory at once when the object's life ends. It is the first real use of injected allocation. Reason: `own` is refused on a `str`, so no `destruct` could free them, and the input text belongs to the caller.

**R11**, line 118. `T(args)` runs only the `construct` that `T` declares, never one of its base.

> `T(args)` and `alloc T(args)` run the `construct` that `T` itself declares. A class that declares none has no `construct` with arguments, whatever its base declares, and a literal builds it. Reason: `construct` runs one body per level. The checker used to take the base's `construct` for the call while lowering ran none, so its error never reached the handler.

**R12**, line 120. A `return` of a local by name moves it to the caller without destroying it.

> A `return` of a local by name hands that local to the caller. The local is not destroyed on the way out, and its owned memory belongs to the returned value. Reason: the specification refuses `=` between two class values that own memory, and the standard library's `new` functions return such a value. A return that destroyed the local would free what the caller received.

**R13**, line 130. `transient` marks a `?*T` or `?fn` field that the field list omits and that a copy sets to `none`.

> `transient` is a contextual word before a class field of type `?*T` or `?fn(...)`, and it is refused together with `own`. The field list of the descriptor leaves a transient field out rather than carrying a bit in its record, so every walk of the list skips it, reflection included, and the runtime does not change. The library file carries the bit. Reason: `none` is the value the copy writes, and a `*T` has none. A class that frees the field in its `destruct` would free it twice under `own`. Leaving the record out is the smaller of the two forms of a flag.

**R14**, line 146. `SystemError.from_win32` exists on every target and gives code 0 away from Windows.

> `SystemError.from_win32` is one function on every target. On Windows it reads `GetLastError` and formats the message, and everywhere else it gives code 0 and an empty message. Reason: a function that exists on one target alone would make a program for six targets fail to compile for five.

### Numeric types

**R15**, line 157. A constant of `c_long`, `c_ulong` or `c_wchar` must fit the narrower width of its type.

> A literal or constant of `c_long`, `c_ulong` or `c_wchar` fits the narrower width of the type. Reason: the constant then has one value on every target.

### Expressions and types

**R16**, line 204. A zero-width bitfield is written `_: T : 0`.

> A zero-width bitfield is written `_: T : 0`. The name `_` names no other field, and no literal, access or constant mentions it. Reason: C's unnamed field needs a spelling, and `_` is the smallest one that the grammar already reads as a field name.

### Libraries and runtime

**R17**, line 265. The installer takes the native package, and `--arm`, `--intel` or `ANTI_ARCH` installs the other beside it.

> The installer takes the package of the processor it runs on, and `--arm` or `--intel` takes the other one. `ANTI_ARCH` carries the same choice, for the `irm | iex` one-liner of Windows. That pipe passes no arguments to the script. PowerShell can pass them through a script block, and that line is long enough that a variable reads better. The rule against environment variables in `docs/decisions.md` covers the options of antic, not the installer, which answers each of its questions from a variable of its own name. A package of the other processor lands in the data directory of the name `anti-<cpu>` beside the native one, keeps its own executables in the `bin/` of that tree, and the installer offers no PATH entry for it. Reason: an Apple Silicon Mac and a Windows machine on ARM both run the x86_64 package under emulation, which is the only way to test that package without a machine of that processor.

**R18**, line 266. The installer asks before it fetches the Microsoft CRT or the stubs of the Command Line Tools, and `ANTI_YES=yes` answers both.

> The installer asks before xwin fetches the Microsoft CRT. For a package without Zig's stubs it also asks before it takes those of the Command Line Tools. `ANTI_YES=yes` answers both for an unattended install. Reason: Apple and Microsoft license those to the user, so the user is the one who accepts.

**R19**, line 268. The installer downloads the pinned CMake into the install directory when the host has none.

> The installer downloads that CMake from the Kitware release when the host has none, and puts it under the install directory. Reason: the installer then runs the CMake scripts of the repository rather than a second copy of them in a shell.

### Threading

**R20**, line 289. The syntax `parallel a by N -> f(x, y)`, with the worker after the arrow and its arguments passed to every chunk.

> The worker stands after the arrow, alone or called: `parallel a -> f` and `parallel a by 4 -> f(x, y)`. The arguments of that call reach every chunk after the chunk itself, and `by` is a contextual word rather than a keyword. Reason: a worker that takes a factor or a limit needs those values, and the safety rule already allows pointer-free values beside the slice.

### Standard library phase

**R21**, line 307. `anti.text` has `byte_count` and `char_count`, and `anti.io` has `print`, `println`, `eprint`, `eprintln` and `exit`.

> The lengths of `anti.text` are `byte_count` and `char_count`, and `char_count` counts the bytes that start a UTF-8 sequence. `anti.io` has `print`, `println`, `eprint`, `eprintln` and `exit`. Reason: a `str` holds valid UTF-8, so the starting bytes are the characters.

**R22**, line 320. `Error.text()` stops at a NUL, is formed once, and gives the message alone when memory runs out.

> `text()` reads a formed text back up to its NUL, so a message that holds a NUL byte ends there. A change to the fields after the first call does not show. When the memory for the text runs out, `text()` gives the message alone and keeps nothing. Reason: the field holds one pointer and no length. Read up to its NUL, the text never reaches past the buffer it sits in.

**R23**, line 321. The code still names the root `anti.rt.Object` and keeps `Job` under `anti.rt`, while the documents say `anti.lang`.

> The compiler and the runtime still name the root class `anti.rt.Object`, with the symbols `anti_rt_Object_*`, and `Job` stays under `anti.rt` too. The documents name both under `anti.lang`, and the rename in the code follows later. Reason: the step that made `anti.lang` moved `Error` alone, and the rename changes the runtime and the symbol of every descriptor.

**R24**, line 326. An error of `anti.args` points its message into a builder the parser owns.

> An error of `anti.args` points its message at a builder the parser owns, so the parser outlives every error it gave. Reason: `Error` keeps the `str` it is handed rather than a copy of the bytes. A message that names the offending argument is worth more than one that does not.

**R25**, line 328. `anti.fs.File` is a struct, with `open`, `close`, `read`, `write`, `size` and the enum `Mode`.

> `anti.fs.File` is a struct that holds the C stream. `open(path, mode) -> *File may fail` allocates it and `close(f) may fail` closes and frees it, also when the close fails. `Mode` is `Read` or `Write`, both binary, and `Write` creates the file or empties it. `Mode.Read` is the default of the mode, as the specification writes it. `read(f, into: []byte) -> int` gives the count it read, 0 at the end, `write(f, bytes: []byte)` writes every byte, and `size(f) -> int` is the offset of the end of the stream, which counts what was written and not flushed yet and fails on a pipe. Reason: a type is a struct unless it needs a table, and the specification writes `-> *File may fail` and `fs.close(f)`.

**R26**, line 329. `fs.list` returns the names of a directory in system order, as one block freed with `free(names.ptr)`.

> `fs.list(path) -> []str may fail` gives the names of a directory without `.` and `..`, in the order the system gives. The slice and the names are one block that the runtime lays out, and the program frees it with `free(names.ptr)`. Reason: the result of `parallel` is freed the same way, the IR carries no size of a `str`, and an order of its own is a choice the specification does not make.

**R27**, line 332. Every function of `anti.fs` fails with `SystemError.from_errno()`, a path with a NUL is refused, and `rename` follows C per system.

> Every function of `anti.fs` fails with `SystemError.from_errno()`. `rt/fs.c` copies each path with its length, refuses one that holds a NUL byte with `EINVAL`, and calls the wide functions of the C runtime on Windows with the path as UTF-16, since those set `errno` too. `remove` and `rename` are the functions of C, so `rename` onto a file that exists replaces it on Linux and macOS and fails on Windows. Reason: one kind of error on every system, and a path goes to the system as the program wrote it.

### Nullable pointers

**R28**, line 455. An assignment to a narrowed name ends the narrowing in the block that holds the record, outer blocks included.

> An assignment to a narrowed name ends the narrowing in the block that holds the record. A nested block therefore ends it for the blocks outside as well. Reason: the branch that assigned may have run.

**R29**, line 457. The `ptr` of a `str` and of a slice is `?*T`.

> The `ptr` of a `str` and of a slice is `?*T`. Reason: a slice of no elements holds no address, and `[]T { ptr: none, len: 0 }` is how a program writes one.

### Tests and fixtures

**R30**, line 558. A test that returns prints `ok <module>.<test>`, and the run of a module stops at its first failure.

> A test that returned prints `ok <module>.<test>`, and the run of a module stops at its first failed assertion. Reason: the specification names the first failed assert and the runtime ends the process there. Running the rest would need a jump out of a test, which no other part of the language has.

### Interpolation

**R31**, line 646. Which parts of a specification apply to integers, floats and other values, and how a negative value prints in a radix.

> An integer takes a width, an alignment, `0` and the kinds `x`, `X`, `b` and `o`. A negative value is written with `-` before its magnitude in every radix, so `{-42:x}` is `-2a`, and no radix writes a prefix. A float takes a width, an alignment, `0`, a precision and the kinds `e` and `f`. A precision gives the digits after the point, `{x:.2}` is `{x:.2f}`, and `e` or `f` without a precision gives six. A `bool`, a `char`, a `str` and an object take a width and an alignment alone. A specification that does not apply to the type is refused with `` unknown format `{n:.2}` for `int` ``. Reason: C's `printf` and Python give these meanings to the forms the examples write. A sign and magnitude needs no width of a type, which keeps the call free of sizes.

**R32**, line 648. A float without a precision prints the fewest digits that read back, in positional or exponent form by the bounds of Python.

> A float without a precision is written with the fewest digits that read back as the same value, the digits of an `f32` for an `f32`. It stands in positional form when its first digit is worth 10 to the power -4 up to 15, and as `1.5e+20` otherwise, with a two-digit exponent at least. `e` with a precision writes the form of C's `%e`, and `nan`, `inf` and `-inf` stand for the values without digits. Reason: the fewest digits are exact and short, and the bounds are those of Python's `repr`, without its `.0`.

**R33**, line 649. The types an `f"..."` can write, with `?*T` and enums refused.

> The values an `f"..."` writes are the integers, the floats, `bool`, `char`, `str` and objects, a class value or a `*T` to a class, which the literal writes with their `to_text`. Every other type, `?*T` and an enum included, is refused with `` `f"..."` cannot write a `Pair` ``. Reason: `show` prints a class through `to_text` and a value of another type through the primitive formats, and the specification gives no other rule for the text of a value.

**R34**, line 651. The text of every `f"..."` is heap memory that the program frees with `free(s.ptr)`.

> The text of an `f"..."` is memory of its own from the C library, which belongs to the program and which `free(s.ptr)` returns. Every literal makes new memory, one without an `{expr}` as well. Reason: a `str` owns nothing, the text outlives the expression that builds it, as a `to_text` that returns one shows, and `anti.mem` does not exist yet. A form with an `Allocator` can follow as it does for `Object.deserialize`.

### `f16`

**R35**, line 662. `as f16` takes an `f32` alone, and an `f16` converts to `f32` and to itself.

> `as f16` converts an `f32` and nothing else, and an `f16` converts to `f32` and to itself. A float literal before `as f16` is an `f32`, so `1.5 as f16` is `(1.5 as f32) as f16`. Any other conversion is refused with `` cannot convert `float` to `f16` ``. Reason: the specification pairs `f16` with `f32` in both directions and names one instruction. x86-64-v3 has none from `f64`, and a detour through `f32` rounds twice, which can land on the other neighbour.

**R36**, line 663. No parameter or result is `f16`.

> A parameter and a result of a function or of a function type are never `f16`. They are refused with `` a parameter cannot be `f16`, which is storage only `` and `` a result cannot be `f16`, which is storage only ``. A pointer to an `f16`, an array of them and a struct or tuple that holds one are allowed. Reason: the specification names a field, an array and a slice. What a function takes or gives is a value, and the value of an `f16` is the `f32` a read gives. C's `__fp16` has the same rule.

### Small things

**R37**, line 680. The precedence and grouping of `??`.

> `??` binds tighter than the comparisons and looser than the shifts, and it groups from the right. `a ?? b ?? c` is `a ?? (b ?? c)`, and `p ?? q == r` compares the pointer that `??` gives. Reason: its operands are pointers, which take no operator that binds tighter, and a comparison is the one operator a pointer result meets beside it. Swift places its `??` the same way.

**R38**, line 684. How `catch` and `try` attach to `p?.f(args)`.

> The `catch` after `p?.f(args)` handles the error of `f` when `f` may fail. Otherwise it guards the `?*U` that `?.` gives, and the `let` takes it over, as it does for a call that gives a `?*T`. `try p?.f(args)` forwards the error of `f`. Reason: the parser attaches a `catch` to the call, and `?.` gives a `?*U` whatever `f` gives, so each form keeps the reading it has on a call.

## A-settled

### Core language

- **S1**, line 53. `--anti-internal` opens the `anti.` root, a bare `anti` is reserved, and no compilation may define `anti.rt`.

### Object model

- **S2**, line 83. A static function of `Error` that returns an error builds one and is not read as a failure. Checked for staleness, see above.
- **S3**, line 86. The checker reports a `delete` and a `mutable` singleton field in worker-reached functions of its module, with the line.
- **S4**, line 91. `anti.reflect` offers `get`, `set`, `new` and `call` over descriptors.
- **S5**, line 94. A struct descriptor holds name, size and fields, a union and `Job` have none, and `type_of(T)` does not exist.
- **S6**, line 114. A library file carries every field default and the `construct` and `destruct` of each class, whatever their level.
- **S7**, line 117. `self.super.construct` must be the first statement of a `construct`.
- **S8**, line 140. The reach of the definite-assignment check on the fields of a `construct`.

### Numeric types

- **S9**, line 153. `uint` and every `c_` type name are keywords.
- **S10**, line 158. A conversion to or from `c_long`, `c_ulong` or `c_wchar` truncates or extends, and is a copy where the widths agree.
- **S11**, line 172. A constant error message spells a float without a literal as `nan`, `inf` or `-inf` and a folded size as `size_of(T)`.

### Expressions and types

- **S12**, line 200. A union literal is not a constant expression.
- **S13**, line 202. A union may hold bitfields, each starting at bit 0.
- **S14**, line 205. A union holds no zero-width bitfield.
- **S15**, line 207. A bitfield that spans more than 8 bytes in a packed struct is an error from the back end.
- **S16**, line 209. `align(N)` takes a constant power of two, and a value from `size_of` is refused.
- **S17**, line 210. An `align(N)` below the natural alignment is an error from the back end that names the type and the target.
- **S18**, line 212. The fields of an `export struct` or `export union` follow the export signature rule, fixed arrays allowed.
- **S19**, line 213. An exported aggregate with `align(N)` may not start with a bitfield.
- **S20**, line 214. An `export const` is numeric, `bool` or `str`.
- **S21**, line 215. `main` cannot be exported.
- **S22**, line 216. Whole-program optimisation keeps every `export fn`.

### Lexical rules

- **S23**, line 225. `////`, `/**/` and `/***` open ordinary comments, not doc comments.
- **S24**, line 228. Two doc comments of one marker on one item join with a blank line.

### Libraries and runtime

- **S25**, line 249. antic takes the package header from its `--package-*`, `--dependency`, `--license*` and `--attribution` options.
- **S26**, line 250. A library file holds the `//!` text of the module, and `--strip-docs` removes it.
- **S27**, line 257. The back end folds a symbolic value with wrapping, and a division by zero there is an error that names the target.
- **S28**, line 259. A value from `size_of` converts only to an integer type in a constant expression.
- **S29**, line 261. Packages are published per target as assets of the GitHub release, and only after the suite ran on that target.
- **S30**, line 269. What a published package holds, and which targets link after an install with nothing further.

### Threading

- **S31**, line 291. How `parallel` splits an array, with a chunk count above the length and an empty array.
- **S32**, line 292. The dispatching thread runs chunks too, the pool holds one thread fewer, and a busy pool runs the chunks inline.

### Standard library phase

- **S33**, line 306. The standard library ships as `.antl` files in `std/` of the runtime archive, searched after the `-I` roots with `--runtime`.
- **S34**, line 317. `SystemError.from_errno` and `from_win32` store the number of the system in both `errno` and `code`.
- **S35**, line 319. The text of an error and of its cause chain, as `print` and `fatal` write it.
- **S36**, line 327. `text.parse_int(s) may fail` replaces `text.to_int(s, fallback)`.
- **S37**, line 330. `json.unquote` may fail, and `json.member` keeps its `bool` and its out pointer.

### Build tool and distribution

- **S38**, line 338. The `SHA256SUMS.sig` of each version stands at `downloads/anti/<version>/` of the site.
- **S39**, line 349. `antic --lib static` and `--lib shared` write a C library and `<name>.h`, and `--llvm-ar` names llvm-ar.
- **S40**, line 350. How the generated header spells each `c_` type in C.
- **S41**, line 352. A header name that C or C++ reserves gets a trailing `_`.
- **S42**, line 354. How `--bundle-runtime` joins the runtime into a library on each object format.
- **S43**, line 355. The line format of `anti_licenses`, which `anti license --from` prints.
- **S44**, line 356. `--soname` needs `--package-version`, and shared libraries follow the naming of each platform.
- **S45**, line 357. A shared library starts the runtime from its constructor, and a DLL exports only its export items and `anti_licenses`.

### Compiler behaviour

- **S46**, line 411. A Linux executable is a static PIE against musl, with the debug sections stripped.
- **S47**, line 418. `APPLE_SDK` and `anti sdk export` copy the stubs as regular files and leave linked directories out.
- **S48**, line 420. `--framework` changes nothing for a Linux or Windows target.
- **S49**, line 421. The installer keeps the step that takes the stubs of the Command Line Tools for a package without Zig's.
- **S50**, line 422. `anti sdk import` writes the sysroot beside its `bin/`, and `--sysroot` names another.

### Nullable pointers

- **S51**, line 472. The dev-mode check of `/` and `%` by zero is emitted on every target, x86_64 included.

### Failing functions

- **S52**, line 486. `undo` runs on `fail`, a forwarding `try` and a `return` of an `*Error`, and not on `return none`.
- **S53**, line 487. Every `undo` of a block runs, last first, before its `defer` statements and teardowns.
- **S54**, line 489. A `construct` with arguments names no result, and a plain `construct` or a `destruct` cannot be `may fail`.
- **S55**, line 490. An export class with a `construct` that takes arguments gives C `anti_<Class>_construct`, with an error only when it may fail.
- **S56**, line 491. `may fail` is refused on an `extern fn`.
- **S57**, line 492. The out parameter of a `may fail` function is named `out` in the interface and the header.
- **S58**, line 493. An export signature may name `anti.lang.Error`, written as an opaque `struct anti_Error *`.
- **S59**, line 494. The C prototype of a failing function carries the comment `May fail: NULL on success, an error otherwise.`

### Tuples

- **S60**, line 503. A tuple has two elements or more, and `()` and `(T)` are no types.
- **S61**, line 504. `t.0.1` reads element 1 of element 0.
- **S62**, line 505. `for i, x in` walks a slice or an array, and a range takes one name.
- **S63**, line 506. How the C header names a tuple struct and where it places it.
- **S64**, line 507. A tuple has no descriptor, and reflection reads a tuple field as a struct without one.

### Default values and source locations

- **S65**, line 514. A parameter without a default may not follow one with a default.
- **S66**, line 515. An `extern fn` takes no default values.
- **S67**, line 517. `here` as a default gives the position where the call starts, with the function of the caller.
- **S68**, line 518. The fields of `anti.lang.SourceLocation`, with columns from 1 and line 0 for a location nobody set.
- **S69**, line 519. A module that writes `here` imports `anti.lang`, and `here` is no constant expression.

### Error origins and stack traces

- **S70**, line 523. The first `fail` of an error sets `at` and captures the frames, and a later one changes neither.
- **S71**, line 524. `e.text()` names a position only when the error has one, and prints the trace of the error alone.
- **S72**, line 525. The fields of `anti.lang.RawFrame` and `anti.lang.Frame`.
- **S73**, line 526. A trace keeps the 64 innermost frames.
- **S74**, line 527. The text of `t.text()`: the modules first, then one line per frame.
- **S75**, line 528. `t.symbolize()` returns a slice the trace owns, naming the line of each call.
- **S76**, line 529. A default of the whole program turns backtraces on in dev and off in release, and `--anti.backtrace` overrides it.
- **S77**, line 530. The runtime takes every `--anti.` argument before `main`, knows `--anti.backtrace` alone, and exits with 70 on any other.
- **S78**, line 531. `--anti.backtrace` reaches the runtime of the executable, not the one of a shared library.
- **S79**, line 532. `anti.debug.backtrace(skip)` is `StackTrace.capture(skip)`, starting at its caller.

### Build ids

- **S80**, line 537. The build id is the SHA-256 of the code a link takes from antic, in 64 lowercase hex digits.
- **S81**, line 538. The build id stands as a `build <id>` line in `anti_licenses` and not in the licence text.

### Tests and fixtures

- **S82**, line 557. A failed assertion under `anti test` prints `FAIL <module>.<test>` and the position.
- **S83**, line 559. `anti test` runs one process per module and reports the count of modules that failed.
- **S84**, line 560. `anti test` takes `.anti` files and `-I` roots, not a package.

### The release script

- **S85**, line 616. `ANTI_GITHUB` and `ANTI_GITHUB_API` replace the release addresses the installers use.
- **S86**, line 617. Without `ANTI_VERSION` an installer takes the tag of the newest GitHub release.

### Fallthrough

- **S87**, line 632. `fallthrough;` enters the arm written next, `else` included.
- **S88**, line 633. `fallthrough;` stands only as the last statement of an arm written as a block, and the message for other places.

### String prefixes

- **S89**, line 639. `fr#"..."#` is refused as `fr"..."` is, and the literal is then read as `rf`.

### Interpolation

- **S90**, line 644. Where the expression of `{expr}` ends, how a quote inside it reads, and the messages for stray braces.
- **S91**, line 645. The grammar of a format specification, with `f` added to the kinds.
- **S92**, line 647. A number aligns right and any other value left, and a width counts characters.
- **S93**, line 650. An `f"..."` lowers to calls of the public `anti.text.Builder`.
- **S94**, line 652. A module that writes `f"..."` must import `anti.text`.
- **S95**, line 654. Whitespace in `x"..."` may stand anywhere, inside a pair as well.
- **S96**, line 655. The positions and messages of the errors of `x"..."`, which takes no hash form.

### `f16`

- **S97**, line 661. A read of an `f16` is an implicit `as f32`, and `as f16` is the one expression of type `f16`.
- **S98**, line 664. A local and a constant may be `f16`.
- **S99**, line 665. An atomic `f16` field takes `load`, `store`, `swap` and `compare_swap` alone.
- **S100**, line 667. The header writes an `f16` as `uint16_t`, and an export constant cannot be `f16`.
- **S101**, line 668. Reflection reads an `f16` as `Float`, and the type id `F16` follows `Class`.

### Small things

- **S102**, line 674. A module with a `switch` on a `str` must import `anti.text`.
- **S103**, line 675. An arm of a `switch` on a `str` is a constant `str`, and a repeated text is refused.
- **S104**, line 677. The precedence of `in`, its status as a contextual word, and its message.
- **S105**, line 678. The order of evaluation and the typing of `x in lo..hi`.
- **S106**, line 681. The left side of `??` and `?.` must be a `?*T`, with the message for a `*T`.
- **S107**, line 682. `??` and `?.` apply to function values, a bound function excluded.
- **S108**, line 683. Where `?.` may stand, how a chain reads, and the message for a field that is no pointer.

## B. Internal

### Scope and toolchain

- **B1**, line 31. On Linux the pinned clang targets glibc by default and musl only with `--target`.

### Object model

- **B2**, line 100. A function record names its signature by a text such as `i64.i32.str`, and the trampoline table is keyed by it.
- **B3**, line 102. The class registry is written only when the program reaches `reflect.new` or `Object.deserialize`.
- **B4**, line 108. The used-slot bitmaps treat every abstract class as injectable until `inject` exists.
- **B5**, line 109. Which calls count as reaching a table slot for the used-slot bitmaps.
- **B6**, line 142. Dev-mode lowering calls through the table except for `final` functions and classes, value receivers and functions without an entry.

### Libraries and runtime

- **B7**, line 267. `tools/package-api` versions the interface between an installer and the scripts of a package.
- **B8**, line 272. Every published binary is cross-compiled from one machine with the pinned clang and lld.

### Standard library phase

- **B9**, line 308. The C functions of the runtime that the standard library calls.
- **B10**, line 331. The test `std_may_fail_only` refuses a hand-written `?*Error` result in `std/anti`.

### Build tool and distribution

- **B11**, line 347. `tools/scripts/format_anti.py` stands in for `anti fmt`, and book listings stay fenced blocks.
- **B12**, line 353. A static library carries its package header in a `<name>.package.o` member.

### Compiler behaviour

- **B13**, line 395. llvm-readobj joins the pinned tools for the Windows unwind tests.
- **B14**, line 419. `sdk/digest` records what the SDK step read.
- **B15**, line 427. The digest of a Windows sysroot covers its regular files and leaves symbolic links out.
- **B16**, line 428. `tools/get-sysroot.cmake` refuses another xwin version and keeps its downloads under the sysroot.

### Nullable pointers

- **B17**, line 471. The dev-mode check of a narrowing `as` is a round trip through the target type.

### Failing functions

- **B18**, line 497. The test `tests_may_fail_only` refuses a hand-written `?*Error` result in `tests/`, with two named exceptions.

### Tuples

- **B19**, line 508. A library file records a tuple as the list of its element types.

### Error origins and stack traces

- **B20**, line 533. Windows walks the stack with `RtlCaptureStackBackTrace` and names frames with DbgHelp.

### Tests and fixtures

- **B21**, line 555. Under `--tests` the functions of both blocks are public.
- **B22**, line 556. `anti test` writes one runner per module as Anti source under `anti/test/`.

### `f16`

- **B23**, line 666. The IR holds an `f16` as an `i16`, converted by `hext` and `htrunc`.
