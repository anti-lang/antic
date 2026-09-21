# Provisional entries for review

`docs/decisions.md` holds 171 `[provisional]` entries at commit `b6e81e0`. This report sorts them and changes none.

- A, language-visible: 148.
- B, internal: 23.

Each entry has a number for the answer, A1 to A148 and B1 to B23, and the line it stands on in `docs/decisions.md` at that commit. Both lists follow the order of the file. An entry that could belong to either list stands in A.

| Section | A | B |
|---|---|---|
| Scope and toolchain | 0 | 1 |
| Core language | 3 | 0 |
| Object model | 20 | 5 |
| Numeric types | 4 | 0 |
| Expressions and types | 12 | 0 |
| Lexical rules | 2 | 0 |
| Libraries and runtime | 9 | 2 |
| Threading | 3 | 0 |
| Standard library phase | 12 | 2 |
| Build tool and distribution | 8 | 2 |
| Compiler behaviour | 5 | 4 |
| Nullable pointers | 3 | 1 |
| Failing functions | 8 | 1 |
| Tuples | 5 | 1 |
| Default values and source locations | 5 | 0 |
| Error origins and stack traces | 10 | 1 |
| Build ids | 2 | 0 |
| Tests and fixtures | 4 | 2 |
| The release script | 2 | 0 |
| Fallthrough | 2 | 0 |
| String prefixes | 2 | 0 |
| Interpolation | 11 | 0 |
| `f16` | 7 | 1 |
| Small things | 9 | 0 |

## Notes

Two entries disagree with a document outside the decisions. The report records them and leaves both texts as they are.

- A7, on the error a handler binds, leaves it alive after `break` and `continue`. "Errors" in `docs/anti-object-model.md` deletes it at both.
- A120 refuses `f"..."` until interpolation is built. The status lines of `docs/anti-syntax-overview.md` list `f"..."` and `rf"..."` as built, and the eleven entries of "Interpolation" describe the built form.

## A. Language-visible

### Core language

**A1**, line 52. A module path comes from the source path under the first `-I` root, and an import finds `<root>/<path>.antl`.

> antic derives the module path of its input from its source path under the first root that holds it. Slashes become dots, and `.anti` is dropped. Outside every root the path is the file name alone. An import without a library file on the command line resolves to `<root>/<path>.antl` in the first root that has it. Reason: the path mirrors the directory tree, as decided.

**A2**, line 53. `--anti-internal` opens the `anti.` root, a bare `anti` is reserved, and no compilation may define `anti.rt`.

> The project-internal flag is `--anti-internal`. A bare `anti` is reserved like `anti.` for `antic -c`, and no compilation may define the runtime's module `anti.rt`. Reason: the runtime's entry symbol is `anti.rt.main`.

**A3**, line 62. `by -k` walks the values of `by k` in reverse, starting at the largest of them.

> `by -k` walks the values of `by k` in reverse, and it starts at the largest of them rather than at the high bound. Reason: the work order gives both a rule and a procedure. The rule is that the two forms walk the same values, and the procedure starts at `hi` and decrements. The two agree only when `k` divides the span, and `for i in 0..10 by -3` gives `7 4 1` under the procedure and `9 6 3 0` under the rule. The rule is what a program relies on, so it wins. Lowering computes the first value as `low + ((high - low - 1) / k) * k`, one division by a constant before the loop.

### Object model

**A4**, line 75. A `use` field and an `implements` sub-object have no visibility level of their own.

> A `use` field and an `implements` sub-object carry no visibility level of their own. Reason: the specification says a literal outside the class names public fields alone, and its own example names the `use` field `rect` of `Sprite` from `main`. Those fields are the shape of the class rather than data it hides, and what they promote keeps its own level.

**A5**, line 77. A failing function returns its result through its last parameter.

> A failing function writes its result through its last parameter. A call that gives one argument fewer leaves that place to the compiler, which passes the address of what the `let` declares. Reason: the specification says the compiler supplies the out pointer and does not say where it goes. The last place is the one a reader expects after the arguments, and the arity tells the checker which call means it.

**A6**, line 83. A static function of `Error` that returns an error builds one and is not read as a failure.

> A static function of the error class itself builds an error rather than reporting one, so `Error.new` is a call like any other. Reason: the convention reads a `*Error` result as a failure, and the makers of the class have the same result type. Nothing else distinguishes them.

**A7**, line 84. A bound error is deleted on `yield` and at the end of the handler, and not on `break`, `continue` or another `return`.

> The error a handler binds is deleted when the handler yields or falls off its end. A handler that leaves with `break`, `continue` or a `return` of another value does not delete it. Reason: the delete is emitted at the two exits the lowering can see. The exit-action list that `defer` uses would cover the rest, and it would also delete the error a `return e` hands to the caller.

**A8**, line 86. The checker reports a `delete` and a `mutable` singleton field in worker-reached functions of its module, with the line.

> The checker also walks every function of its own module that a worker reaches. It reports a `delete` and a `mutable` singleton field there with its position. The abstract class report stays in the checker and runs in release mode, which sees every class. Reason: the checker holds positions and types that the IR does not carry, so its reports name the line.

**A9**, line 91. `anti.reflect` offers `get`, `set`, `new` and `call` over descriptors.

> `anti.reflect` reads descriptors, reads and writes fields with `get` and `set`, builds objects with `new` and calls functions with `call`. The registry that `new` reads is written by the pass over the whole program.

**A10**, line 94. A struct descriptor holds name, size and fields, a union and `Job` have none, and `type_of(T)` does not exist.

> The descriptor of a struct holds the name, the size and the field list, and no chain. A union and a `Job` have none. `type_of(T)` does not exist yet. Reason: a struct has no base, no walk knows which field of a union holds the value, and no module declares a `Job`.

**A11**, line 95. The signatures and results of `reflect.get` and `reflect.set`, and the error codes of `set`.

> `reflect.get(object, d, index) -> Value` gives the field as the `Value` its type gives: `Int` for a signed integer and an enum over one, `Uint` for an unsigned one, `Float`, `Bool`, `Char`, `Str`, and `Ptr` for a pointer or a function pointer. A field of any other type and an index out of range give `None`. `reflect.set(object, d, index, value) may fail` takes a `Value` of that kind and narrows it to the field's type. It fails with code `BAD_INDEX`, `NOT_CARRIED` or `WRONG_KIND` and writes nothing when it cannot. Reason: `set` can fail, so it follows the error convention. The only failure of `get` is that no value is there, which `None` says.

**A12**, line 97. The kinds of `Value`, with every integer carried at 64 bits by signedness and `f32` as `Float`.

> The kinds of `Value` are `None`, `Int`, `Uint`, `Float`, `Bool`, `Char`, `Str` and `Ptr`. A signed integer of any width travels as `Int` at 64 bits, an unsigned one as `Uint`, and `f32` as `Float` at 64 bits. Reason: the specification lists the primitive types, `str` and pointers, and one kind per signedness keeps a caller from naming the width of each parameter.

**A13**, line 103. `reflect.new` takes `Class` or `module.Class` and gives `none` for a class it cannot build.

> `reflect.new(name)` takes the class's own name or `module.Class`. A name that two modules declare needs the module path. An abstract class, a singleton, a class whose `construct` takes arguments and an unknown name give `none`. Reason: the descriptor holds the name without its module, and `new` has no arguments for such a `construct`.

**A14**, line 104. `Object.deserialize` gives `none` on input that is no object of the program, not an error.

> `Object.deserialize(input: str) -> *Object` gives `none` when the text is not an object of a class of the program, instead of an error. Reason: the runtime owns `anti.rt`, and `anti.lang.Error` lives in the standard library, which `anti.rt` cannot name.

**A15**, line 105. How `Object.deserialize` rebuilds an object from its JSON, and which inputs fail the whole text.

> `Object.deserialize` builds the class that the member `type` names, prepared as a literal of the class would be, and fills the fields from the other members. A `construct` with arguments does not run, because the fields come from the text. A member that names no field is skipped. A field that `serialize` writes as `null` keeps its default. An `own` pointer and an `own` slice come back in new memory, and any other pointer or slice as the address that was written. A number outside the range of its field, a `bool` written as a number and a `char` of more than one character fail the whole text. A `type` of a class that cannot be the field's class gives `none`. Reason: the reader is the counterpart of the default `serialize`, and those are the forms it writes.

**A16**, line 107. `Object.deserialize` leaks the bytes of `str` fields until a form with an `Allocator` replaces it.

> `Object.deserialize` gives a `str` field bytes of its own on the heap, which nothing frees. Until `anti.mem` exists they come from libc. The form with an `Allocator` replaces this one. `Object.deserialize` then takes an `Allocator`, and every string and every owned sub-object it creates comes from it. The caller frees that memory at once when the object's life ends. It is the first real use of injected allocation. Reason: `own` is refused on a `str`, so no `destruct` could free them, and the input text belongs to the caller.

**A17**, line 114. A library file carries every field default and the `construct` and `destruct` of each class, whatever their level.

> A library file carries the value of every field default of its classes and structs, and the `construct` and `destruct` of a class whatever their level. A literal of a class of another module then writes the defaults of every level, and a class that inherits one runs the `construct` and `destruct` of its base. The checker evaluates each default, so a default that is not a constant expression is refused where the specification already said so. Reason: a literal outside the class names `pub` fields alone, so the defaults of the other fields must reach every module that builds it.

**A18**, line 117. `self.super.construct` must be the first statement of a `construct`.

> The top of the body is its first statement. A call of `self.super.construct` anywhere else is refused, in another function as well. Reason: the specification says "at the top of its body", and the first statement is the reading a check can hold. The base part is then complete before the body below reads it.

**A19**, line 118. `T(args)` runs only the `construct` that `T` declares, never one of its base.

> `T(args)` and `alloc T(args)` run the `construct` that `T` itself declares. A class that declares none has no `construct` with arguments, whatever its base declares, and a literal builds it. Reason: `construct` runs one body per level. The checker used to take the base's `construct` for the call while lowering ran none, so its error never reached the handler.

**A20**, line 120. A `return` of a local by name moves it to the caller without destroying it.

> A `return` of a local by name hands that local to the caller. The local is not destroyed on the way out, and its owned memory belongs to the returned value. Reason: the specification refuses `=` between two class values that own memory, and the standard library's `new` functions return such a value. A return that destroyed the local would free what the caller received.

**A21**, line 130. `transient` marks a `?*T` or `?fn` field that the field list omits and that a copy sets to `none`.

> `transient` is a contextual word before a class field of type `?*T` or `?fn(...)`, and it is refused together with `own`. The field list of the descriptor leaves a transient field out rather than carrying a bit in its record, so every walk of the list skips it, reflection included, and the runtime does not change. The library file carries the bit. Reason: `none` is the value the copy writes, and a `*T` has none. A class that frees the field in its `destruct` would free it twice under `own`. Leaving the record out is the smaller of the two forms of a flag.

**A22**, line 140. The reach of the definite-assignment check on the fields of a `construct`.

> The check covers a `construct` with arguments, since a literal names every other required field. A call of a base's `construct` sets the fields from that base up. An assignment counts when it is `self.f = v`, and not when a called function makes it. What a loop body or a handler assigns counts only inside it. A path that fails needs nothing. The closing brace is a path that succeeds, in a `construct` that cannot fail as well. Reason: a loop body may not run and a handler runs on a path of its own, and a failed `construct` leaves no object behind.

**A23**, line 146. `SystemError.from_win32` exists on every target and gives code 0 away from Windows.

> `SystemError.from_win32` is one function on every target. On Windows it reads `GetLastError` and formats the message, and everywhere else it gives code 0 and an empty message. Reason: no Windows machine here runs the test suite. A function that exists on one target alone would also make a program for six targets fail to compile for five.

### Numeric types

**A24**, line 153. `uint` and every `c_` type name are keywords.

> `uint` and the `c_` type names are keywords. Reason: the lexical rules make every type name a keyword.

**A25**, line 157. A constant of `c_long`, `c_ulong` or `c_wchar` must fit the narrower width of its type.

> A literal or constant of `c_long`, `c_ulong` or `c_wchar` fits the narrower width of the type. Reason: the constant then has one value on every target.

**A26**, line 158. A conversion to or from `c_long`, `c_ulong` or `c_wchar` truncates or extends, and is a copy where the widths agree.

> A conversion to or from `c_long`, `c_ulong` or `c_wchar` truncates when the target type is never wider and extends otherwise. The back end makes it a copy where both widths are equal. Reason: one IR instruction serves every target.

**A27**, line 172. A constant error message spells a float without a literal as `nan`, `inf` or `-inf` and a folded size as `size_of(T)`.

> A float value without an Anti literal shows as `nan`, `inf` or `-inf`, and a value from `size_of` as `size_of(T)`. Reason: the rule asks for the value, and neither has a spelling of the reader or a literal.

### Expressions and types

**A28**, line 200. A union literal is not a constant expression.

> A union literal is not a constant expression. Reason: a constant holds a value for every field of an aggregate, and a union literal names one field.

**A29**, line 202. A union may hold bitfields, each starting at bit 0.

> A union may hold bitfields, and every bitfield of a union starts at bit 0. Reason: C allows them, and a binding emits a C union one to one.

**A30**, line 204. A zero-width bitfield is written `_: T : 0`.

> A zero-width bitfield is written `_: T : 0`. The name `_` names no other field, and no literal, access or constant mentions it. Reason: C's unnamed field needs a spelling, and `_` is the smallest one that the grammar already reads as a field name.

**A31**, line 205. A union holds no zero-width bitfield.

> A union holds no zero-width bitfield. Reason: a union has no unit to break, and C compilers disagree on its effect, as MSVC gives `union { char a : 3; long long : 0; }` a size of 8 with alignment 1.

**A32**, line 207. A bitfield that spans more than 8 bytes in a packed struct is an error from the back end.

> A bitfield that needs more than 8 bytes from its first byte, possible only in a packed struct, is an error from the back end. Reason: the load and store lowering reads one integer of at most 8 bytes.

**A33**, line 209. `align(N)` takes a constant power of two, and a value from `size_of` is refused.

> The N of `align(N)` is a constant expression of type `int` with a power of two as its value, and a value computed from `size_of` is refused. Reason: C's `_Alignas` takes a constant power of two, and the IR stores N as a number.

**A34**, line 210. An `align(N)` below the natural alignment is an error from the back end that names the type and the target.

> An `align(N)` below the alignment that the fields give is an error from the back end that names the type and the target. Reason: C refuses an `_Alignas` below the natural alignment, and only the back end knows that alignment.

**A35**, line 212. The fields of an `export struct` or `export union` follow the export signature rule, fixed arrays allowed.

> The fields of an `export struct` or `export union` follow the export signature rule, and a field may also be a fixed-size array of such a type. Reason: the generated header spells every field in C.

**A36**, line 213. An exported aggregate with `align(N)` may not start with a bitfield.

> An `export struct` or `export union` with `align(N)` has no bitfield as its first field. Reason: the header writes `align(N)` as `_Alignas` on the first field, whose offset is 0 on every target, and C refuses `_Alignas` on a bitfield.

**A37**, line 214. An `export const` is numeric, `bool` or `str`.

> An `export const` has a numeric type, `bool` or `str`. Reason: the header writes numbers and bools as `#define` and strings as `static const`.

**A38**, line 215. `main` cannot be exported.

> `main` cannot be exported. Reason: the C `main` of the runtime starts the program, and an exported `main` would take its symbol.

**A39**, line 216. Whole-program optimisation keeps every `export fn`.

> Whole-program optimisation keeps every `export fn`, even one that no Anti code calls. Reason: C code may call it.

### Lexical rules

**A40**, line 225. `////`, `/**/` and `/***` open ordinary comments, not doc comments.

> `////`, `/**/` and `/***` open ordinary comments. Reason: rule lines and banners stay comments, as they do for Doxygen and rustdoc.

**A41**, line 228. Two doc comments of one marker on one item join with a blank line.

> Two doc comments with one marker before one item join with a blank line between them. Reason: no text is lost, and the blank line is the paragraph break of the doc markup.

### Libraries and runtime

**A42**, line 249. antic takes the package header from its `--package-*`, `--dependency`, `--license*` and `--attribution` options.

> antic takes the package header from `--package-name`, `--package-version`, `--dependency <name>,<constraint>,<url>`, `--license`, `--license-text <file>` and `--attribution`. Without them it writes the module path, version `0.0.0` and empty licence fields. Reason: `anti` passes what `anti.toml` holds, and antic keeps working without a manifest.

**A43**, line 250. A library file holds the `//!` text of the module, and `--strip-docs` removes it.

> The public interface of a library file also holds the `//!` text of the module, and `--strip-docs` removes it with the rest. Reason: user docs are built from a `.antl` alone.

**A44**, line 257. The back end folds a symbolic value with wrapping, and a division by zero there is an error that names the target.

> The back end folds a symbolic value with wrapping at the width of its type. A division by zero is an error that names the target. Reason: the fold agrees with the same arithmetic at run time.

**A45**, line 259. A value from `size_of` converts only to an integer type in a constant expression.

> A value computed from `size_of` converts only to an integer type in a constant expression. Reason: the back end folds symbolic values as integers.

**A46**, line 261. Packages are published per target as assets of the GitHub release, and only after the suite ran on that target.

> antic and the runtime archive are published per target, as `anti-<version>-<target>.tar.xz` among the assets of the GitHub release of the tag `v<version>`. A target is published only after the test suite has run on that target, in a VM or on a runner. Reason: a binary that nobody executed is not a release.

**A47**, line 265. The installer takes the native package, and `--arm`, `--intel` or `ANTI_ARCH` installs the other beside it.

> The installer takes the package of the processor it runs on, and `--arm` or `--intel` takes the other one. `ANTI_ARCH` carries the same choice, for the `irm | iex` one-liner of Windows. That pipe passes no arguments to the script. PowerShell can pass them through a script block, and that line is long enough that a variable reads better. The rule against environment variables in `docs/decisions.md` covers the options of antic, not the installer, which answers each of its questions from a variable of its own name. A package of the other processor lands in the data directory of the name `anti-<cpu>` beside the native one, keeps its own executables in the `bin/` of that tree, and the installer offers no PATH entry for it. Reason: an Apple Silicon Mac and a Windows machine on ARM both run the x86_64 package under emulation, which is the only way to test that package without a machine of that processor.

**A48**, line 266. The installer asks before it fetches the Microsoft CRT or the stubs of the Command Line Tools, and `ANTI_YES=yes` answers both.

> The installer asks before xwin fetches the Microsoft CRT. For a package without Zig's stubs it also asks before it takes those of the Command Line Tools. `ANTI_YES=yes` answers both for an unattended install. Reason: Apple and Microsoft license those to the user, so the user is the one who accepts.

**A49**, line 268. The installer downloads the pinned CMake into the install directory when the host has none.

> The installer downloads that CMake from the Kitware release when the host has none, and puts it under the install directory. Reason: the installer then runs the CMake scripts of the repository rather than a second copy of them in a shell.

**A50**, line 269. What a published package holds, and which targets link after an install with nothing further.

> A published package holds antic and anti for its host, the runtime library of all six targets and the standard library. It holds the two Linux sysroots and the two macOS sysroots of Zig's stubs. The installer adds the five LLVM tools from a release of `anti-lang/llvm-tools`, the one that `tools/llvm-pin` of the package names. It therefore links a program for the two Linux and the two macOS targets with nothing further installed. The Windows targets need the CRT that xwin fetches. A macOS program that names a framework needs Apple's SDK. Reason: those parts are ours to redistribute, and both Linux sysroots together are 24 MB. The LLVM tools left the package when they moved to a repository of their own. Each host has one archive of them rather than a copy inside every package.

### Threading

**A51**, line 289. The syntax `parallel a by N -> f(x, y)`, with the worker after the arrow and its arguments passed to every chunk.

> The worker stands after the arrow, alone or called: `parallel a -> f` and `parallel a by 4 -> f(x, y)`. The arguments of that call reach every chunk after the chunk itself, and `by` is a contextual word rather than a keyword. Reason: a worker that takes a factor or a limit needs those values, and the safety rule already allows pointer-free values beside the slice.

**A52**, line 291. How `parallel` splits an array, with a chunk count above the length and an empty array.

> The split gives the first `count % chunks` chunks one element more than the rest. A chunk count above the element count becomes the element count, and an empty array yields an empty result without touching the pool. Reason: contiguous disjoint slices that cover the array are what makes the construct free of data races.

**A53**, line 292. The dispatching thread runs chunks too, the pool holds one thread fewer, and a busy pool runs the chunks inline.

> The thread that writes `parallel` takes chunks beside the pool, and the pool holds one thread fewer than the worker count. A `parallel` that meets a busy pool runs its chunks in that same thread. Reason: a machine of one processor still runs every chunk, and a worker that dispatches cannot wait for a pool that only it could free.

### Standard library phase

**A54**, line 306. The standard library ships as `.antl` files in `std/` of the runtime archive, searched after the `-I` roots with `--runtime`.

> The CMake build writes the standard library as library files into `std/` of the runtime archive, as the package `anti` with the antic version and 0BSD, and antic searches that directory after the `-I` roots when `--runtime` is given. Reason: a program imports `anti.io` without a manifest, and every module of a package lies under the package root, which `anti.io` and `anti.text` do under `anti` and not under `anti.std`.

**A55**, line 307. `anti.text` has `byte_count` and `char_count`, and `anti.io` has `print`, `println`, `eprint`, `eprintln` and `exit`.

> The lengths of `anti.text` are `byte_count` and `char_count`, and `char_count` counts the bytes that start a UTF-8 sequence. `anti.io` has `print`, `println`, `eprint`, `eprintln` and `exit`. Reason: a `str` holds valid UTF-8, so the starting bytes are the characters.

**A56**, line 317. `SystemError.from_errno` and `from_win32` store the number of the system in both `errno` and `code`.

> `SystemError.from_errno()` and `SystemError.from_win32()` put the number the system gave in both `errno` and `code`. Reason: `fatal` exits with the code, as it did when `from_errno` set the code alone, and a caller that tests `code` keeps working.

**A57**, line 319. The text of an error and of its cause chain, as `print` and `fatal` write it.

> The text of an error is `error N: message`, or the message alone when the code is 0, and each cause follows after a newline and `  caused by: ` in the same form. `print` writes the text and a newline, and `fatal` prints it. Reason: the object model gives the order origin, code, message, and code 0 means no code. The origin goes before the error once error origins exist.

**A58**, line 320. `Error.text()` stops at a NUL, is formed once, and gives the message alone when memory runs out.

> `text()` reads a formed text back up to its NUL, so a message that holds a NUL byte ends there. A change to the fields after the first call does not show. When the memory for the text runs out, `text()` gives the message alone and keeps nothing. Reason: the field holds one pointer and no length. Read up to its NUL, the text never reaches past the buffer it sits in.

**A59**, line 321. The code still names the root `anti.rt.Object` and keeps `Job` under `anti.rt`, while the documents say `anti.lang`.

> The compiler and the runtime still name the root class `anti.rt.Object`, with the symbols `anti_rt_Object_*`, and `Job` stays under `anti.rt` too. The documents name both under `anti.lang`, and the rename in the code follows later. Reason: the step that made `anti.lang` moved `Error` alone, and the rename changes the runtime and the symbol of every descriptor.

**A60**, line 326. An error of `anti.args` points its message into a builder the parser owns.

> An error of `anti.args` points its message at a builder the parser owns, so the parser outlives every error it gave. Reason: `Error` keeps the `str` it is handed rather than a copy of the bytes. A message that names the offending argument is worth more than one that does not.

**A61**, line 327. `text.parse_int(s) may fail` replaces `text.to_int(s, fallback)`.

> `text.parse_int(s) -> int may fail` replaces `text.to_int(s, fallback)`. It reads an optional `+` or `-` and the digits, and fails with code 0 on a text without a digit, on any other byte and on a value outside the range of `int`. `toml.Document.int_of` and `args.Parser.int_of` keep their `fallback` and yield it from a `catch`. Reason: the syntax overview writes the conversion as `text.parse_int(s)` under `catch`, and a fallback gave the failure as a value. A text past the range spells no `int`, and a dev build would stop on the overflow check inside the library.

**A62**, line 328. `anti.fs.File` is a struct, with `open`, `close`, `read`, `write`, `size` and the enum `Mode`.

> `anti.fs.File` is a struct that holds the C stream. `open(path, mode) -> *File may fail` allocates it and `close(f) may fail` closes and frees it, also when the close fails. `Mode` is `Read` or `Write`, both binary, and `Write` creates the file or empties it. `Mode.Read` is the default of the mode, as the specification writes it. `read(f, into: []byte) -> int` gives the count it read, 0 at the end, `write(f, bytes: []byte)` writes every byte, and `size(f) -> int` is the offset of the end of the stream, which counts what was written and not flushed yet and fails on a pipe. Reason: a type is a struct unless it needs a table, and the specification writes `-> *File may fail` and `fs.close(f)`.

**A63**, line 329. `fs.list` returns the names of a directory in system order, as one block freed with `free(names.ptr)`.

> `fs.list(path) -> []str may fail` gives the names of a directory without `.` and `..`, in the order the system gives. The slice and the names are one block that the runtime lays out, and the program frees it with `free(names.ptr)`. Reason: the result of `parallel` is freed the same way, the IR carries no size of a `str`, and an order of its own is a choice the specification does not make.

**A64**, line 330. `json.unquote` may fail, and `json.member` keeps its `bool` and its out pointer.

> `json.unquote(source, out) may fail` fails with code 0 on a text that is no JSON string, and `out` keeps the bytes written before the fault. `json.member(source, name, out: *str) -> bool` keeps its `bool` and its out pointer. Reason: `unquote` fails on a malformed text, as `text.parse_int` does. The only failure of `member` is a member the object does not hold, which a function may give as `bool`, and a tuple that crosses a module boundary is a struct that has not been named yet.

**A65**, line 332. Every function of `anti.fs` fails with `SystemError.from_errno()`, a path with a NUL is refused, and `rename` follows C per system.

> Every function of `anti.fs` fails with `SystemError.from_errno()`. `rt/fs.c` copies each path with its length, refuses one that holds a NUL byte with `EINVAL`, and calls the wide functions of the C runtime on Windows with the path as UTF-16, since those set `errno` too. `remove` and `rename` are the functions of C, so `rename` onto a file that exists replaces it on Linux and macOS and fails on Windows. Reason: one kind of error on every system, and a path goes to the system as the program wrote it.

### Build tool and distribution

**A66**, line 338. The `SHA256SUMS.sig` of each version stands at `downloads/anti/<version>/` of the site.

> The signature of a version stands in a directory of its own, `downloads/anti/<version>/SHA256SUMS.sig` of the site. Reason: the signature of every release stays fetchable, so the installer of an older version keeps working. A single path would hold the newest alone.

**A67**, line 349. `antic --lib static` and `--lib shared` write a C library and `<name>.h`, and `--llvm-ar` names llvm-ar.

> antic writes a library for C with `--lib static` or `--lib shared`, which the design gives to `anti build --lib`, and writes `<name>.h` beside it. The name is the file name of `-o` without `lib` and its suffix, or the last segment of the module path. `--llvm-ar` names llvm-ar. Reason: the output is compiler work, so it goes into antic and its driver.

**A68**, line 350. How the generated header spells each `c_` type in C.

> The header maps `c_long`, `c_ulong` and `c_wchar` to `long`, `unsigned long` and `wchar_t`, and the other `c_` types to their `<stdint.h>` names, as `int32_t` for `c_int`. Reason: the fixed-size `c_` types are the sized types, and a library file records types and not spellings.

**A69**, line 352. A header name that C or C++ reserves gets a trailing `_`.

> In the generated header a parameter or field name that C11 or C++17 reserves, or that a macro of `<stdbool.h>`, `<stddef.h>` or `<stdint.h>` takes, gets a trailing `_`, as `default_`. Reason: the header compiles as C and C++, and the suffix keeps the Anti name readable.

**A70**, line 354. How `--bundle-runtime` joins the runtime into a library on each object format.

> `--bundle-runtime` joins the library object and every runtime member except the object of `rt/start.c` into one relocatable object with `ld -r`, keeping hidden symbols global on macOS. On Windows the runtime members go into the archive beside the library object. Reason: one object makes two bundled runtimes a duplicate symbol at link time, and COFF has no relocatable link.

**A71**, line 355. The line format of `anti_licenses`, which `anti license --from` prints.

> `anti_licenses` holds lines between `ANTI_LICENSES_BEGIN` and `ANTI_LICENSES_END`: `build <id>`, `package <name> <version> <license>`, `attribution <line>`, then `text for <names>` and each distinct text once. The runtime is the package `anti.rt` with the antic version and 0BSD. A package name appears once, with the fields of its first occurrence. Reason: plain text lines are what `anti license --from` prints.

**A72**, line 356. `--soname` needs `--package-version`, and shared libraries follow the naming of each platform.

> `--soname` needs `--package-version`. On Linux the library is `lib<name>.so.<major>` with the soname of that file and a `lib<name>.so` symlink. On macOS the install name is `@rpath/lib<name>.dylib` with compatibility version `<major>.0.0` and the full current version. Reason: these are the platform conventions that the design names.

**A73**, line 357. A shared library starts the runtime from its constructor, and a DLL exports only its export items and `anti_licenses`.

> The shared library's constructor calls `anti_rt_init` from `rt/init.c`, which an executable's `main` calls first. A Windows DLL's `.def` file lists every export fn and `anti_licenses DATA`. Reason: one initialisation function serves both, and the `.def` file keeps the exports of a DLL to the export items.

### Compiler behaviour

**A74**, line 411. A Linux executable is a static PIE against musl, with the debug sections stripped.

> ld.lld links a Linux executable statically against musl as a position-independent executable: `-static -pie --no-dynamic-linker --strip-debug` with `rcrt1.o`, `crti.o`, `libc.a`, `libclang_rt.builtins.a` and `crtn.o`. The debug sections come from the musl of Alpine, and antic writes none, so `--strip-debug` takes `tests/programs/letters.anti` for linux-arm64 from 98,392 bytes to 21,288. An ELF shared library links no C library. Reason: a static PIE runs on any Linux kernel without a C library on the system, and a shared library takes the C library of its process.

**A75**, line 418. `APPLE_SDK` and `anti sdk export` copy the stubs as regular files and leave linked directories out.

> `APPLE_SDK` and `anti sdk export` copy the stubs of `usr/lib` and `System/Library/Frameworks` as regular files. A linked stub becomes a copy, as the top-level stub of a framework is, and a linked directory such as `Versions/Current` stays out. The stubs of SDK 26.5 then take 158 MB per macOS sysroot, and the bundle 5.4 MB. Reason: lld reads no path through `Versions/Current`, and a Windows host keeps no symbolic link.

**A76**, line 420. `--framework` changes nothing for a Linux or Windows target.

> `--framework` changes nothing for a Linux or Windows target. Reason: frameworks exist on macOS alone, and one command line serves every target of a build.

**A77**, line 421. The installer keeps the step that takes the stubs of the Command Line Tools for a package without Zig's.

> The installer keeps the step that takes the stubs of the Command Line Tools for a package without Zig's, as 0.1.0 is. Reason: the site serves the newest installer to every version.

**A78**, line 422. `anti sdk import` writes the sysroot beside its `bin/`, and `--sysroot` names another.

> `anti sdk import` takes the sysroot beside the `bin/` of anti, and `--sysroot` names another. Reason: an installed anti sits in `bin/` beside antic, and antic finds its runtime the same way.

### Nullable pointers

**A79**, line 455. An assignment to a narrowed name ends the narrowing in the block that holds the record, outer blocks included.

> An assignment to a narrowed name ends the narrowing in the block that holds the record. A nested block therefore ends it for the blocks outside as well. Reason: the branch that assigned may have run.

**A80**, line 457. The `ptr` of a `str` and of a slice is `?*T`.

> The `ptr` of a `str` and of a slice is `?*T`. Reason: a slice of no elements holds no address, and `[]T { ptr: none, len: 0 }` is how a program writes one.

**A81**, line 472. The dev-mode check of `/` and `%` by zero is emitted on every target, x86_64 included.

> The check of `/` and `%` by zero is emitted for every target. The specification names ARM64, which returns zero, and x86_64 traps by itself. Reason: the IR is target-independent. A library file would otherwise hold the checks of the host that wrote it, and the trap alone gives no file, line or values.

### Failing functions

**A82**, line 486. `undo` runs on `fail`, a forwarding `try` and a `return` of an `*Error`, and not on `return none`.

> `undo` runs where the block leaves through an error. Those are a `fail` statement, a `try` that forwards, and a `return` of an `*Error` in a function that returns `?*Error`. `return none;` is the success path and runs no `undo`. The checker decides it per statement, in the `error_exit` field of the statement, so lowering reads one flag. Reason: the specification says "exits through an error" and the language has no unwinding, so the error exits are the ones a program writes. The type of the returned value is what tells the two returns of a hand-written failing function apart.

**A83**, line 487. Every `undo` of a block runs, last first, before its `defer` statements and teardowns.

> Every `undo` of a block runs, last declared first, before the `defer` statements and the teardowns of that block. Reason: the specification puts `undo` before `defer` of the same block, so lowering takes two reverse passes over the exit actions rather than one.

**A84**, line 489. A `construct` with arguments names no result, and a plain `construct` or a `destruct` cannot be `may fail`.

> A `construct` with arguments that names a result is refused, `-> ?*Error` included. A `construct` without arguments and a `destruct` refuse `may fail`. Reason: `construct` and `destruct` keep the forms the object model gives them, and it gives `may fail` alone to a `construct` that can fail. The hand-written form stays legal for bindings, which never declare a `construct`. A `construct` without arguments runs after every literal and cannot fail, and `destruct` has no error channel at all.

**A85**, line 490. An export class with a `construct` that takes arguments gives C `anti_<Class>_construct`, with an error only when it may fail.

> An export class whose `construct` takes arguments gives C `anti_<Class>_construct(self, args...)`. It prepares `self` as `anti_<Class>_init` does, then runs `construct`, and returns `struct anti_Error *` when the `construct` may fail and nothing otherwise. Its parameters follow the export signature rule whatever the level of the `construct`. Reason: the specification names the helper and its error, and calls it with the object and the arguments. A helper that left the init to C would let a program forget it, and a `construct` that cannot fail has no error to give.

**A86**, line 491. `may fail` is refused on an `extern fn`.

> `may fail` is refused on an `extern fn`. Reason: a binding declares what C declares, and C has no error channel. A C function that reports one is written `-> ?*Error` by hand, which stays legal.

**A87**, line 492. The out parameter of a `may fail` function is named `out` in the interface and the header.

> The out parameter of a `may fail` function is named `out` in the interface of the module and in the generated header. Reason: the specification writes the ABI as `?*Error f(args, R *out)`, and the name has to come from somewhere, since no declaration wrote it.

**A88**, line 493. An export signature may name `anti.lang.Error`, written as an opaque `struct anti_Error *`.

> An export signature may name `anti.lang.Error` without the class being an `export class`. The header declares `struct anti_Error;` once and writes the type as `struct anti_Error *`. Reason: `docs/anti-object-model.md` gives the generated helpers that C type already, and C passes the pointer on without reading the layout. A class below `Error` has no C name and stays refused.

**A89**, line 494. The C prototype of a failing function carries the comment `May fail: NULL on success, an error otherwise.`

> The prototype of a function that may fail carries one comment line, `May fail: NULL on success, an error otherwise.`. Reason: the specification says the doc comment says so. The C signature does not: `?*Error` and a pointer parameter are one type each.

### Tuples

**A90**, line 503. A tuple has two elements or more, and `()` and `(T)` are no types.

> A tuple has two elements or more. `()` and `(T)` are refused as types, `(a)` stays the grouping it is everywhere else, and a destructuring names two or more. Reason: the specification writes the value as `(a, b)` and the elements as `t.0` and `t.1`, so two is the fewest a pair has. One element is a value that has a name already.

**A91**, line 504. `t.0.1` reads element 1 of element 0.

> A number that stands right after a `.` takes no fraction of its own, so `t.0.1` reads element 1 of element 0. Reason: a float literal starts with a digit and never follows a `.`, and a range writes `..`, which is one token. The other route is a rule about where a tuple may stand, which the specification does not give.

**A92**, line 505. `for i, x in` walks a slice or an array, and a range takes one name.

> `for i, x in items` binds the index and the element and walks a slice or an array alone. A range counts in one name. Reason: the specification calls the two-name form the destructuring of the `(int, T)` of each element, and a range has no element. The index is the counter the loop already holds, so nothing builds a pair per turn.

**A93**, line 506. How the C header names a tuple struct and where it places it.

> The C header names a tuple `anti_tuple_` and the names of its elements joined by `_`, as `struct anti_tuple_int_str`. A pointer writes `ptr_` before what it points at, a nullable pointer `optr_`, an array its length as `a4_`, a slice `slice_` and a function `fn`. One struct per distinct tuple of an exported signature stands after the exported aggregates and before the prototypes. Reason: the specification gives the name of `(int, str)` and no name for the types that have no plain one. The elements are the fields of that struct, so an array among them crosses as a field of a struct does.

**A94**, line 507. A tuple has no descriptor, and reflection reads a tuple field as a struct without one.

> A tuple carries no descriptor, and reflection reads a field of tuple type as a struct without one, which is what a union gives. Reason: a descriptor is written by the module that declares the type, and no module declares a tuple.

### Default values and source locations

**A95**, line 514. A parameter without a default may not follow one with a default.

> A parameter without a default after one with a default is refused. Reason: named arguments are not built, so a call gives its arguments in order and leaves out only the last ones. A default before a required parameter could never be used.

**A96**, line 515. An `extern fn` takes no default values.

> An `extern fn` takes no default values. Reason: a binding declares what C declares, and C has none.

**A97**, line 517. `here` as a default gives the position where the call starts, with the function of the caller.

> `here` as a default is the position where the call starts, the receiver of a method call. Its function is the caller's. Reason: the specification evaluates it at the call site. The start of the call is where a message about the call points.

**A98**, line 518. The fields of `anti.lang.SourceLocation`, with columns from 1 and line 0 for a location nobody set.

> `anti.lang.SourceLocation` is a struct of `file: str`, `line: int`, `column: int`, `function: str` and `module: str`, in that order. The column counts from 1, and a tab counts as one, as in every message of antic. A location that no `here` and no `fail` wrote has line 0. Reason: the specification names the fields, and a type that is only data is a struct.

**A99**, line 519. A module that writes `here` imports `anti.lang`, and `here` is no constant expression.

> A module that writes `here` imports `anti.lang`, as one that writes `may fail` does, and the message says so. A call of a function whose default is `here` needs no import, because the type comes from the function. `here` is no constant expression, so a `const` cannot hold it. Reason: the path is the one lowering records and the function is the one that holds the position, so the value is data of that function.

### Error origins and stack traces

**A100**, line 523. The first `fail` of an error sets `at` and captures the frames, and a later one changes neither.

> `fail` writes the position of its statement into `at` when `at.line` is 0, and captures the frames there when backtraces are on. An error whose `at` holds a line keeps both, so a later `fail` of it changes neither. Reason: the specification lets the first `fail` win and captures the frames at the same `fail`.

**A101**, line 524. `e.text()` names a position only when the error has one, and prints the trace of the error alone.

> `e.text()` writes `file:line:column: ` before an error only when it has a position, and every cause carries its own. The trace that follows the causes is the one of the error itself. Reason: an error that no `fail` gave has no position to name. The trace of a cause would repeat the frames of the error that wraps it.

**A102**, line 525. The fields of `anti.lang.RawFrame` and `anti.lang.Frame`.

> `anti.lang.RawFrame` holds `address: u64`, `module: str`, `build_id: str` and `base: u64`, the raw form of `t.frames`. `anti.lang.Frame` holds `address: u64`, `function: str`, `file: str` and `line: int`. Outside every module both texts of a raw frame are empty and its base is 0. Reason: the specification names what a frame stores and the fields of `Frame`, and not the type of an address.

**A103**, line 526. A trace keeps the 64 innermost frames.

> A trace keeps the 64 innermost frames. Reason: the frames live in the stack of `capture` before they are copied, and a deeper trace of one error repeats itself.

**A104**, line 527. The text of `t.text()`: the modules first, then one line per frame.

> `t.text()` is one line per module, `module <n> <build id> <base> <path>`, in the order the frames reach them, then one line per frame, `<address> <n>+<offset>` or `<address> -`. A module without a build id writes `-`, and the text has no last newline. `e.text()` puts it after the causes on a line of its own. Reason: the specification gives the modules on top and one address per line, and a resolver needs the id and the offset of each.

**A105**, line 528. `t.symbolize()` returns a slice the trace owns, naming the line of each call.

> `t.symbolize()` returns a slice that the trace owns, formed at the first call and kept until the trace is deleted. The lookup takes the byte before a return address, so a frame names the line of its call. `file` is the name the line table holds, the path under the search root. Reason: the specification makes symbolising lazy, and a program deletes the trace, not its frames.

**A106**, line 529. A default of the whole program turns backtraces on in dev and off in release, and `--anti.backtrace` overrides it.

> Whether backtraces are on is a call of `anti_rt_backtrace_on`, which reads `anti_rt_backtrace_default` unless the command line named `--anti.backtrace`. The pass over the whole program writes the default into a program that reaches the call, as it writes the registry. It is 1 in dev mode and 0 in release. Reason: a library file serves both modes, so the build of the program decides, as it does for an assertion.

**A107**, line 530. The runtime takes every `--anti.` argument before `main`, knows `--anti.backtrace` alone, and exits with 70 on any other.

> The runtime takes every argument under `--anti.` before `main`, and `--anti.backtrace` is the one it knows. It takes no value, `=true` or `=false`. An unknown option or another value ends the program before `main` with status 70, the status of every refusal at start, and the message names the options. Reason: the specification makes an unknown option a startup error. The file of the runtime configuration is not built, so its key `backtrace` comes with it.

**A108**, line 531. `--anti.backtrace` reaches the runtime of the executable, not the one of a shared library.

> `--anti.backtrace` reaches the runtime of the executable. A shared library carries a runtime of its own, whose `fail` follows the default of its build. Reason: the command line belongs to the executable, and two runtimes share no state.

**A109**, line 532. `anti.debug.backtrace(skip)` is `StackTrace.capture(skip)`, starting at its caller.

> `anti.debug.backtrace(skip: int = 0)` is `StackTrace.capture` with the same parameter, and its first frame is its caller. Reason: the specification calls it the short form and gives no other meaning to `n`.

### Build ids

**A110**, line 537. The build id is the SHA-256 of the code a link takes from antic, in 64 lowercase hex digits.

> The build id of an executable and of a shared library is the SHA-256 of the code its link takes from antic. It is written in 64 lowercase digits. That is the assembly of the module that links, every object the command line adds and the runtime library. Reason: the specification makes the id the digest of the code. The notice holds the id and is left out, and so is every path, so two links of one program carry one id.

**A111**, line 538. The build id stands as a `build <id>` line in `anti_licenses` and not in the licence text.

> The id stands in `anti_licenses` as the line `build <id>`, right after the begin marker. The licence text that `anti.license` gives leaves the line out. Reason: a tool finds the id by the marker alone, and the id is a fact of the binary and no licence.

### Tests and fixtures

**A112**, line 557. A failed assertion under `anti test` prints `FAIL <module>.<test>` and the position.

> The runner names each test to the runtime before it calls it, with `anti_rt_test_running`. A failed assertion prints `FAIL <module>.<test>` and then the position the compiler built into the message. Reason: a failed assertion aborts, so nothing after it can name the test. The name is one pointer that the assertion reads on the path that was ending anyway.

**A113**, line 558. A test that returns prints `ok <module>.<test>`, and the run of a module stops at its first failure.

> A test that returned prints `ok <module>.<test>`, and the run of a module stops at its first failed assertion. Reason: the specification names the first failed assert and the runtime ends the process there. Running the rest would need a jump out of a test, which no other part of the language has.

**A114**, line 559. `anti test` runs one process per module and reports the count of modules that failed.

> `anti test` runs one process per module and reports the count of modules that failed. Reason: antic compiles one module per call, and a runner per module needs no cross-module name at all.

**A115**, line 560. `anti test` takes `.anti` files and `-I` roots, not a package.

> `anti test` takes `.anti` files and `-I` roots rather than a package. Reason: no manifest and no package layout are built, and `docs/tooling.md` describes an `anti` that does not exist yet. The files and the roots are what antic already takes.

### The release script

**A116**, line 616. `ANTI_GITHUB` and `ANTI_GITHUB_API` replace the release addresses the installers use.

> `ANTI_GITHUB` and `ANTI_GITHUB_API` name the release area and the latest-release API of the installers, and default to the two addresses of `tools/release-base`. The test `installer_github` stands a fake release on disk through them, and a fork names its own. Reason: the default route of an installer is the one a user takes, so a test of it reaches no server and pins the addresses against `tools/release-base` at the same time.

**A117**, line 617. Without `ANTI_VERSION` an installer takes the tag of the newest GitHub release.

> The newest version an installer takes without `ANTI_VERSION` is the tag of the newest release. It reads that tag from the latest-release API of GitHub and strips its leading `v`. The site serves no `latest` file. Reason: the release is the one place a version is published, so nothing else can name a version it does not hold.

### Fallthrough

**A118**, line 632. `fallthrough;` enters the arm written next, `else` included.

> The arm that `fallthrough;` enters is the one the text writes next, `else` included wherever it stands, and the checker reads the arms in that order. Reason: the specification names the next arm, and a `switch` takes `else` between two arms. In the order of the text, an assignment that ends a narrowing in one arm reaches the arm it falls into.

**A119**, line 633. `fallthrough;` stands only as the last statement of an arm written as a block, and the message for other places.

> `fallthrough;` stands in an arm written as a block, `A => { fallthrough; }`. An arm of one call or one assignment has no room for it. Every other place is refused with `` `fallthrough` is allowed as the last statement of a `switch` arm only ``: a nested block, an `if`, a loop, a `defer` and a function body outside any `switch`. Reason: an arm holds a block or one call or assignment, and `fallthrough;` is a statement, which a block holds.

### String prefixes

**A120**, line 639. `f"..."` is refused with a message of its own until interpolation is built.

> Until interpolation is built, `f"..."` is refused with `` `f"..."` is not built yet `` at the prefix. Reason: `f` is a prefix of the table, and an identifier `f` followed by a string would be refused later by the parser with a message that names neither.

**A121**, line 641. `fr#"..."#` is refused as `fr"..."` is, and the literal is then read as `rf`.

> `fr#"..."#` is refused like `fr"..."`, and the literal is then read as an `rf"..."`, so the one mistake gives one message. Reason: the prefix takes hash delimiters as every prefix but `x` does, and a literal read another way would report its content as further errors.

### Interpolation

**A122**, line 646. Where the expression of `{expr}` ends, how a quote inside it reads, and the messages for stray braces.

> The expression of an `{expr}` ends at the first `}` or `:` outside brackets, and it is lexed and parsed as any expression, at its positions in the file. A `"` inside it ends the literal as in every string, so an expression that holds a string stands in the hash form: `f#"{m["k"]}"#`. A single `}` is refused with `` single `}` in `f"..."`, write `}}` ``, an empty `{}` with `` empty `{}` in `f"..."` ``, and a `{` that the literal closes first with `` unterminated `{` in `f"..."` ``, each naming `rf"..."` for that form. Reason: the specification leaves the end of an expression open, and the rule of the quote keeps one reading of where a literal ends.

**A123**, line 647. The grammar of a format specification, with `f` added to the kinds.

> A format specification is `[align][0][width][.precision][kind]`, each part optional and in that order. The alignment is `<`, `>` or `^`, the kinds are `x`, `X`, `b`, `o`, `e` and `f`, and a width and a precision take one to nine digits. `0` needs a width and stands without an alignment. Any other text, `{x:}` included, is refused with `` unknown format `{x:q}` ``, which quotes the placeholder. Reason: the specification lists these parts, and its examples `{x:08.3f}` and `{price:8.2f}` add `f`. A fill character is not among them.

**A124**, line 648. Which parts of a specification apply to integers, floats and other values, and how a negative value prints in a radix.

> An integer takes a width, an alignment, `0` and the kinds `x`, `X`, `b` and `o`. A negative value is written with `-` before its magnitude in every radix, so `{-42:x}` is `-2a`, and no radix writes a prefix. A float takes a width, an alignment, `0`, a precision and the kinds `e` and `f`. A precision gives the digits after the point, `{x:.2}` is `{x:.2f}`, and `e` or `f` without a precision gives six. A `bool`, a `char`, a `str` and an object take a width and an alignment alone. A specification that does not apply to the type is refused with `` unknown format `{n:.2}` for `int` ``. Reason: C's `printf` and Python give these meanings to the forms the examples write. A sign and magnitude needs no width of a type, which keeps the call free of sizes.

**A125**, line 649. A number aligns right and any other value left, and a width counts characters.

> A number stands on the right of its field and every other value on the left. `^` puts the odd space on the right. The width counts characters as `text.char_count` does, and `0` fills after a leading `-`. Reason: C and Python align the same way, and a field of text is as wide as the characters it shows.

**A126**, line 650. A float without a precision prints the fewest digits that read back, in positional or exponent form by the bounds of Python.

> A float without a precision is written with the fewest digits that read back as the same value, the digits of an `f32` for an `f32`. It stands in positional form when its first digit is worth 10 to the power -4 up to 15, and as `1.5e+20` otherwise, with a two-digit exponent at least. `e` with a precision writes the form of C's `%e`, and `nan`, `inf` and `-inf` stand for the values without digits. Reason: the fewest digits are exact and short, and the bounds are those of Python's `repr`, without its `.0`.

**A127**, line 651. The types an `f"..."` can write, with `?*T` and enums refused.

> The values an `f"..."` writes are the integers, the floats, `bool`, `char`, `str` and objects, a class value or a `*T` to a class, which the literal writes with their `to_text`. Every other type, `?*T` and an enum included, is refused with `` `f"..."` cannot write a `Pair` ``. Reason: `show` prints a class through `to_text` and a value of another type through the primitive formats, and the specification gives no other rule for the text of a value.

**A128**, line 652. An `f"..."` lowers to calls of the public `anti.text.Builder`.

> An `f"..."` calls `anti.text.Builder`: `new`, `append` for each text, `append_int` or `append_uint` for an integer as `int` or `u64`, `append_float` or `append_f32`, `append_bool`, `append_char` and `append_text` for each value, with the `Align` of `anti.text`, then `take` for the `str`. Every function is `pub` and has defaults for the parameters of the specification, so a program calls them as well. Reason: the specification makes interpolation calls of `anti.text`, and `Builder` is the one type there that collects a text.

**A129**, line 653. The text of every `f"..."` is heap memory that the program frees with `free(s.ptr)`.

> The text of an `f"..."` is memory of its own from the C library, which belongs to the program and which `free(s.ptr)` returns. Every literal makes new memory, one without an `{expr}` as well. Reason: a `str` owns nothing, the text outlives the expression that builds it, as a `to_text` that returns one shows, and `anti.mem` does not exist yet. A form with an `Allocator` can follow as it does for `Object.deserialize`.

**A130**, line 654. A module that writes `f"..."` must import `anti.text`.

> A module that writes `f"..."` imports `anti.text`, and one that does not is refused with `` `f"..."` builds its text with `anti.text.Builder`, so the module imports `anti.text` ``. Reason: `here` and `may fail` ask the same of `anti.lang`. The import names what the program links.

**A131**, line 656. Whitespace in `x"..."` may stand anywhere, inside a pair as well.

> Whitespace in `x"..."` is a space, a tab, a CR or an LF, and it is ignored between any two digits, also between the two of one pair. A digit is `0` to `9`, `a` to `f` or `A` to `F`, as in an integer literal. Reason: the specification ignores whitespace without naming where, and the digits of a hex literal are already defined.

**A132**, line 657. The positions and messages of the errors of `x"..."`, which takes no hash form.

> An odd digit count is reported at the digit that has no pair, the last one, and a character that is no hex digit at the first such character: `` odd digit count in `x"..."` at column 7 `` and `` non-hex character `G` in `x"..."` at column 4 ``, where the column is that of the digit or the character in the file. One literal reports one of the two, the character when there is one. `x#"..."#` is refused with `` `x"..."` takes no hash delimiters ``. Reason: the specification asks for the position and writes the column into the message. The file column is the one the diagnostic names, and a literal that spans lines has no other column that a reader can find.

### `f16`

**A133**, line 663. A read of an `f16` is an implicit `as f32`, and `as f16` is the one expression of type `f16`.

> A read of an `f16` is an implicit `as f32`, which the checker writes into the tree. The target of an assignment, the operand of `&` and the place of an atomic call stay `f16`. `as f16` is the one expression of type `f16`. Reason: the specification reads an `f16` as `f32` and writes one with `as f16`, so a read converts without a written `as` and a write does not. C's `__fp16` promotes the same way.

**A134**, line 664. `as f16` takes an `f32` alone, and an `f16` converts to `f32` and to itself.

> `as f16` converts an `f32` and nothing else, and an `f16` converts to `f32` and to itself. A float literal before `as f16` is an `f32`, so `1.5 as f16` is `(1.5 as f32) as f16`. Any other conversion is refused with `` cannot convert `float` to `f16` ``. Reason: the specification pairs `f16` with `f32` in both directions and names one instruction. x86-64-v3 has none from `f64`, and a detour through `f32` rounds twice, which can land on the other neighbour.

**A135**, line 665. No parameter or result is `f16`.

> A parameter and a result of a function or of a function type are never `f16`. They are refused with `` a parameter cannot be `f16`, which is storage only `` and `` a result cannot be `f16`, which is storage only ``. A pointer to an `f16`, an array of them and a struct or tuple that holds one are allowed. Reason: the specification names a field, an array and a slice. What a function takes or gives is a value, and the value of an `f16` is the `f32` a read gives. C's `__fp16` has the same rule.

**A136**, line 666. A local and a constant may be `f16`.

> A local variable and a constant may be `f16`. Reason: the syntax overview declares `let h: f16 = 1.5 as f16;`.

**A137**, line 667. An atomic `f16` field takes `load`, `store`, `swap` and `compare_swap` alone.

> An atomic `f16` field takes `load`, `store`, `swap` and `compare_swap`, which move and compare its bits. `add`, `sub`, `and` and `or` are refused with the message of arithmetic. Reason: the field is storage like any other, and the four refused calls are arithmetic.

**A138**, line 669. The header writes an `f16` as `uint16_t`, and an export constant cannot be `f16`.

> The C header writes an `f16` as `uint16_t`. An export constant of type `f16` is refused, because an export constant is a number, a bool or a `str`. Reason: C has no half type that the compiler of every target reads, MSVC among them. `uint16_t` has the layout of an `f16` and passes an aggregate the way antic passes it.

**A139**, line 670. Reflection reads an `f16` as `Float`, and the type id `F16` follows `Class`.

> Reflection reads an `f16` field as a `Float`, and `set` narrows a float to it through `f32`, as `as f16` takes an `f32`. The type id `F16` follows `Class`, so every earlier id keeps its number. `serialize` writes the `f32` that a read gives, which reads back to the same bits. Reason: `get`, `set`, `serialize` and `deserialize` cover every field kind, and an id at the end changes no existing record.

### Small things

**A140**, line 676. A module with a `switch` on a `str` must import `anti.text`.

> A module that writes a `switch` on a `str` imports `anti.text`, and one that does not is refused with `` a `switch` on a `str` compares with `anti.text.equal`, so the module imports `anti.text` ``. Reason: the specification compares with `text.equal`, and an `f"..."` asks the same import for `anti.text.Builder`.

**A141**, line 677. An arm of a `switch` on a `str` is a constant `str`, and a repeated text is refused.

> An arm of a `switch` on a `str` is a constant `str`, a literal or a `const`, and a second arm of the same text is refused with `this value already has an arm`, as on an integer. Reason: the arms of every `switch` are constants, and the chain would never reach the second arm.

**A142**, line 679. The precedence of `in`, its status as a contextual word, and its message.

> `in` binds as `<` does, and each bound of its range takes the operators that bind tighter, so `i + 1 in 0..n * 2` needs no parentheses. `in` stays a contextual word, as after `for`, so a variable may still be named `in`. `in` without a range is refused with `` `in` takes a range ``, the message of the specification. Reason: the rewrite is two comparisons, which bind at that level, and `in` is not among the keywords the decisions add.

**A143**, line 680. The order of evaluation and the typing of `x in lo..hi`.

> `x in lo..hi` computes `x` once, then `lo`, and `hi` only when `x` reaches `lo`, as the `&&` of the rewrite does. The first of the three operands that is no literal names the type, and the other two take it. A type with an `lt` operator takes it for both comparisons, and any other type is numeric or `char`. Reason: the rewrite names `x` twice, so a value with an effect would run twice. One value has one type, so the literals of both comparisons take the same one.

**A144**, line 682. The precedence and grouping of `??`.

> `??` binds tighter than the comparisons and looser than the shifts, and it groups from the right. `a ?? b ?? c` is `a ?? (b ?? c)`, and `p ?? q == r` compares the pointer that `??` gives. Reason: its operands are pointers, which take no operator that binds tighter, and a comparison is the one operator a pointer result meets beside it. Swift places its `??` the same way.

**A145**, line 683. The left side of `??` and `?.` must be a `?*T`, with the message for a `*T`.

> The left side of `??` and of `?.` is a `?*T`. A `*T` is refused with `` `??` follows a value of type `?*T`, found `*int` `` and the same words for `?.`, as `let ... else` and a `catch` that guards a pointer refuse one, and a narrowed name is a `*T`. The right side of `??` converts to the element of the left as any pointer does. Reason: a `*T` is never `none`, so the operator would decide nothing.

**A146**, line 684. `??` and `?.` apply to function values, a bound function excluded.

> A function value follows the pointer rule in both. `f ?? g` takes a `?fn(...)`, and `h?.done` of a field of type `fn(...)` gives a `?fn(...)`. A bound function is refused, since it is two words and never `none`. Reason: the specification makes a function value follow the rule of `?*T`, and `let ... else` and `catch` take one already.

**A147**, line 685. Where `?.` may stand, how a chain reads, and the message for a field that is no pointer.

> `?.` stands before a field, a tuple element or a call. `p?.x.y` reads `.y` on the `?*U` of `p?.x`, which is refused as every read through a `?*T` is, and a chain goes on with `?.`. A field or result that is no pointer is refused with the message of the specification, `` `?.` on `p.count`, which is not a pointer ``, which spells a call as `p.f()` or `p.f(...)`. Reason: the specification names the field and the call, and its chains follow the first `none` through each `?.`.

**A148**, line 686. How `catch` and `try` attach to `p?.f(args)`.

> The `catch` after `p?.f(args)` handles the error of `f` when `f` may fail. Otherwise it guards the `?*U` that `?.` gives, and the `let` takes it over, as it does for a call that gives a `?*T`. `try p?.f(args)` forwards the error of `f`. Reason: the parser attaches a `catch` to the call, and `?.` gives a `?*U` whatever `f` gives, so each form keeps the reading it has on a call.

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

- **B23**, line 668. The IR holds an `f16` as an `i16`, converted by `hext` and `htrunc`.
