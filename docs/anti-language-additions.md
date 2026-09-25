# Anti language additions

Rules for the features decided after `docs/anti-object-model.md` and outside it. `docs/decisions.md` refers to the additions document and repeats none of it. The book covers what the compiler does for each. The rest is the language's and lives on anti-lang.com.

Settled on 2026-09-20. The small things, wire formats, binary I/O and the SDK were settled on 2026-09-23. Failing functions, tuples, error origins, source locations, the symbols tooling, the CPU levels, the simd structs and the round-three small items were settled on 2026-09-21. The base in the class header was settled on 2026-09-22. Round four was settled on 2026-09-23: regular expressions, bytes, the language hooks, iteration, anonymous functions and closures, concurrent classes, nested types, and errors, warnings and checks. Round five was settled on 2026-09-24: generics, and the collections they make possible with the language features they rest on, `Shared<T>` and `--memory-checks`.

Contents:

- [Timing](#timing)
- [Nullable pointers](#nullable-pointers)
- [Failing functions](#failing-functions)
- [Tuples](#tuples)
- [Error origin and stack traces](#error-origin-and-stack-traces)
- [Source locations](#source-locations)
- [Dev-mode checks](#dev-mode-checks)
- [Wrapping and saturating operators](#wrapping-and-saturating-operators)
- [Flags](#flags)
- [CPU levels](#cpu-levels)
- [Simd structs](#simd-structs)
- [Tests and fixtures](#tests-and-fixtures)
- [Sum types](#sum-types)
- [Locking and channels](#locking-and-channels)
- [Debug information](#debug-information)
- [Symbols tooling](#symbols-tooling)
- [Namespaces](#namespaces)
- [Hooks and tracing](#hooks-and-tracing)
- [Injection](#injection)
- [Standard interfaces](#standard-interfaces)
- [Plugins](#plugins)
- [Versions](#versions)
- [Runtime configuration](#runtime-configuration)
- [Small things](#small-things)
- [Small items, round three](#small-items-round-three)
- [Base in the class header](#base-in-the-class-header)
- [Wire formats](#wire-formats)
- [Binary I/O](#binary-io)
- [SDK and frameworks](#sdk-and-frameworks)
- [Regular expressions](#regular-expressions)
- [Bytes](#bytes)
- [Language hooks](#language-hooks)
- [Iteration](#iteration)
- [Anonymous functions and closures](#anonymous-functions-and-closures)
- [Concurrent classes](#concurrent-classes)
- [Nested types](#nested-types)
- [Errors, warnings and checks](#errors-warnings-and-checks)
- [Generics](#generics)
- [Optional values](#optional-values)
- [Ownership at a call](#ownership-at-a-call)
- [Lending](#lending)
- [Walking a collection](#walking-a-collection)
- [Hashing and order](#hashing-and-order)
- [Direct imports](#direct-imports)
- [Collections](#collections)
- [Thread-safe collections](#thread-safe-collections)
- [Shared ownership](#shared-ownership)
- [Memory checks](#memory-checks)
- [Messages](#messages)
- [Keywords](#keywords)

## Timing

Before the first public release: nullable pointers, dev-mode checks, debug information, tests and fixtures, the CPU levels and `none`. The base in the class header comes before it as well. Each changes signatures or output that a user would otherwise depend on. The CPU levels change what a release is built for, and `none` is a rename. The class header moves a line of every class with a base.

After the first release, in this order: the wrapping and saturating operators with `Flags`, sum types, locking and channels. Then `may fail` with tuples, error origins and stack traces, then the symbols tooling, then the small things and wire formats. Then injection, hooks and tracing, plugins and runtime configuration, which belong together. Then generics and closures, which `docs/anti-object-model.md` names.

The round-three small items and the simd structs stay where the small things are.

Round four has no place in this order yet. None of it is work for a session until Eddie names it in a work order or a session message.

Round five, generics and collections, follows round four. Generics come first, since every collection is generic. Round five is no work for a session either until Eddie names it.

## Nullable pointers

- `*T` never holds `none`. The compiler refuses `none` for it, refuses an uninitialised one, and never asks for a check before use.
- `?*T` may hold `none`. It cannot be dereferenced, called, indexed or passed where `*T` is expected until the program has checked it.
- `none` has type `?*T` for every `T`, and `?T` for every `T` as [Optional values](#optional-values) gives it.
- A function value follows the same rule. `fn(...)` never holds `none`, `?fn(...)` may, and it is called only after the program has checked it. Narrowing works on it as it does on a pointer.
- Narrowing is per block. Inside `if p != none { }` the name `p` has type `*T`. After `if p == none { return; }` it has type `*T` for the rest of the enclosing block. After the block that narrowed it, `p` is `?*T` again. Assigning to `p` inside a narrowed block ends the narrowing for that block.
- Narrowing follows `&&` and `||`. In `p != none && p.n > 0` the right operand reads `p` as `*T`, and so does the body of the `if`. In `p == none || p.n > 0` the right operand reads it as `*T` and the body does not, because either side may have decided the chain.
- `let m = p else { leave };` binds `m` as `*T`, and the `else` block must leave the enclosing block. `p catch fatal` and `p catch e { yield q; }` follow the error forms, with the error `anti.lang.NoneDereference`.
- `alloc T { }` and `alloc T(args)` return `*T`. Out of memory is fatal. `alloc(T, n)` returns `?*T`, because `malloc` does.
- `p as *T` returns `*T` and traps. `p as? *T` returns `?*T`. `dup(p)` returns the type of `p`. `is` works on both.
- A class field of type `*T` without a default must be set in every literal or in `construct`. A field of type `?*T` may default to `none`.
- Every pointer in an `extern fn`, in a bound struct and in a C callback is `?*T`. The generated header maps `*T` and `?*T` both to `T *`, with `/* non-null */` on the first. An exported function with a `*T` parameter checks nothing at run time.
- The standard library returns `?*T` wherever "not found" is an answer, and takes `*T` wherever `none` would be a bug.
- Nothing is emitted for any of this. The checks are the ones the program wrote.
- A failing function is written with `may fail`. See [Failing functions](#failing-functions).

## Failing functions

A `may fail` function has two channels. On success it returns its value through the normal return. On failure it returns a `*Error` through a separate error channel, which `catch`, `try` and the `yield` of a handler read. A function without `may fail` has the value channel alone, so a `*Error` in its return type is an ordinary value and no failure. The compiler knows a failure by the `may fail` marking and never by the return type.

The compiler generates the ABI of a `may fail` function: a `?*Error` result and an out pointer for the value. A `may fail` function and a C caller meet in the header, and neither writes the two channels by hand.

- `fn divide(a: int, b: int) -> (int, int) may fail` declares a function with two channels. `return v;` leaves on the result channel. `fail e;` leaves on the error channel with `e`, a `*Error`. `fn close(f: *File) may fail` has no result and returns success at its closing brace.
- `fail "text";` is sugar for `fail Error.new(0, "text");`. Code zero means "no code", and `fatal` turns it into exit status 1.
- A call to a `may fail` function must be handled with `catch`, `try`, `catch fatal`, `catch none` or a `try` block, which is the existing rule. `try` inside a `may fail` function forwards the error. Inside any other function `try` is refused.
- `catch none` stands beside `catch fatal`. It counts a failure as `none`, and applies where the result can be `none`. [Regular expressions](#regular-expressions) shows it on a match.
- `fn(A) -> R may fail` is a function type of its own. A `may fail` function converts to it and to no other function type. A call through a value of it is handled as a direct call is, and the header writes it in the ABI form.
- `undo` runs on the `fail` path and not on `return`.
- A `may fail` function with no `fail` and no `try` is a warning from `anti check`, not an error, since an interface function may fail in one implementation and not another.
- The ABI is the old convention: `?*Error f(args, R *out)`, with `out` absent for a function without a result. The header writes that form and the doc comment says the function may fail. A `.antl` records the flag, so a caller in another module handles it.
- A `construct` that can fail takes the same form, `fn construct(self, args...) may fail`, and `alloc T(args)` and `T(args)` carry its error. "Literals and construction" in `docs/anti-object-model.md` gives the rules, the call of a base's `construct` among them.
- `may fail` is the one failing form. A C binding returns its error as an ordinary value, a `?*Error` or an integer code, and a `may fail` wrapper in Anti inspects that value and turns it into a `fail`. Bindings produce ordinary functions and never a failing form. The standard library uses `may fail` everywhere, and the rewrite of its signatures is one session when the parser has the form.

```anti
fn divide(a: int, b: int) -> (int, int) may fail
{
	if b == 0 { fail "division by zero"; }
	return (a / b, a % b);
}

let (q, r) = divide(7, 2) catch fatal;
let (q, r) = divide(7, 0) catch e { yield (0, 0); };
```

## Tuples

- A tuple type is an anonymous struct with C layout. `(int, str)` is a struct of two fields in that order, laid out as the target lays out structs. It is passed and returned by value under the struct rules. Two tuple types are the same when their element types are the same in order.
- A tuple value is `(a, b)`. Its elements are `t.0`, `t.1`, up to the last. Destructuring is `let (a, b) = e;` and `for i, x in items`, and nowhere else: not in a parameter list and not nested.
- `for i, x in items` is destructuring of an `(int, T)` per element, and `for i, x in &items` of an `(int, *T)`. `let (result, flags) = a + b;` is destructuring of a `(T, Flags)` result.
- The C header writes a tuple as a named struct, `struct anti_tuple_int_str { int64_t _0; const struct anti_str *_1; }`, one per distinct tuple type used in an exported signature.
- Tuples are for functions that have two answers and no name for the pair: `divmod`, `min_max`, a coordinate pair, the flags. A tuple of more than three elements is a struct that has not been named yet. So is one that crosses a module boundary. The site says so.

## Error origin and stack traces

- `Error` gains `pub at: SourceLocation`, set by `fail` at the position of the `fail` statement when the error has no location yet. The first `fail` wins, so an error forwarded through `try` keeps its origin, and the `cause` chain shows the path it took.
- `Error` gains `own frames: ?*StackTrace`, captured by the same `fail` when backtraces are on. On in dev mode, off in release. `--anti.backtrace` and `backtrace = true` in the runtime configuration turn it on for a release build. When off the field is `none` and `fail` costs what it costs today.
- `e.text()` prints `file:line:column: ` and the error, then the cause chain, then the trace when there is one. The error is `error N: message`, or the message alone when its code is 0. `print` and `fatal` inherit it.
- `Error.new` takes no location. `fail` supplies it.
- `anti.lang.StackTrace`: `StackTrace.capture(skip: int = 0) -> *StackTrace` walks frame pointers and stores the return addresses with, per frame, the module the address is in, that module's build id and its load base. `t.frames` is the raw form. `t.text()` prints one address per line with the modules on top. `t.symbolize() -> []Frame` fills `Frame { address, function: str, file: str, line: int }` from the symbol table for the function and from the line table for file and line, so a release build gives functions and a `-g` build gives everything. Symbolising is lazy and never runs for an error that was handled.
- `anti.debug.backtrace(n)` is the short form returning a `StackTrace`.
- Every Anti executable and shared library carries a build id, the digest of its code, in `anti_licenses` beside the version.

## Source locations

- `here` is a keyword whose value is an `anti.lang.SourceLocation` for the position it is written at: `file`, `line`, `column`, `function`, `module`. `file` is the root-relative path the checks use. `function` is the full name, `module.Class.f`. The value is constant data, so it costs the loads.
- `here` as a default parameter value is evaluated at the call site, so `fn log(level: Level, msg: str, at: SourceLocation = here)` sees the caller's position. That is how a logger, `show` and a traced error get the caller's line without a macro.
- `here` in an ordinary expression gives the position of that expression, which is rarely what a message wants. The site says so.

## Dev-mode checks

The switch is the one `assert` uses: emitted in dev mode, absent in release, decided in the back end, so a `.antl` gets the checks whenever the program that links it is a dev build. `--checks` and `--no-checks` override either mode.

- Bounds. Every index into an array, a slice or a `str` is compared against its length before the access. A raw pointer index `p[i]` has no length and is not checked.
- Conversions. `char` and an enum are checked with the integers. A value converted to `char` must be a Unicode scalar value, and one converted to an enum must be a value the enum declares. Both are narrowing in the sense above, and the same routine reports them.
- Overflow. `+`, `-` and `*` on signed integers are checked. The back end detects overflow with the target's cheapest sequence. Where the instruction sets the overflow flag the check branches on it, which covers `+` and `-` on both architectures and `*` on x86_64. Where no instruction sets it the check multiplies into twice the width and compares the halves against the sign of the result, which covers `*` on ARM64. A narrowing `as` checks the range. Unsigned arithmetic wraps and is not checked.
- Division and shifts. `/` and `%` by zero are checked on every target. ARM64 returns zero for them and x86_64 traps by itself. That trap is a signal with no message, and the check reports the file, the line and the values as every other check does. Release keeps the raw instruction on both. A shift count negative or at or above the width is checked on both.
- None. Not needed. A dereference of `none` cannot be written without a check the compiler demanded.
- A failed check calls the runtime's failure routine. It prints the file, the line, the operation and the values, then aborts.
- Cost in a dev build: a compare and a not-taken branch per checked operation. Cost in release: none.

## Wrapping and saturating operators

- `+% -% *% <<%` give the modulo result in every build and never trap. A reader sees the `%` and knows wrap was intended.
- `+| -| *|` clamp at the type's minimum or maximum. Both CPUs have a two-instruction form.
- Both families apply to integer types only, both operands of one type, and follow the precedence of the plain operator.
- The plain operators keep their meaning: a trap on overflow in dev mode, a wrap in release.
- Each has a compound assignment, `+%= -%= *%= <<%=` and `+|= -|= *|=`, which gives what `x = x op e` gives.
- `mul_high(a, b) -> T` is a built-in like `size_of` and returns the upper half of the full product.

## Flags

- `let (result, flags) = e;` destructures a `(T, Flags)` result whose right side is one arithmetic operation on an integer type: `+ - *`, `<< >>`, or unary `-`. `result` has the operand type and holds the wrapped value. `flags` has the built-in struct type `Flags`. See [Tuples](#tuples).
- `Flags` has four `bool` fields: `overflow`, `carry`, `zero`, `negative`. It is a struct of four bytes, and C sees it as such.
- No trap in any build. Asking for the flags states that overflow is expected.
- The lowering is the instruction plus one flag read per field the program uses. Unused fields cost nothing.
- Carry in. When the last operand of `+` is the `carry` field of a `Flags` value, the lowering is `adc` on x86_64 and `adcs` on ARM64, and the result's `carry` is the carry out. The same holds for `-` with `borrow`, which is the `carry` field read the way subtraction uses it.
- The result name is always new. The flags name may be a new variable or an existing `Flags` variable in scope, which is then assigned. The plain form `(result, flags) = e;` assigns to two existing names.
- The pair is a tuple and the two names are its destructuring, so the form is the one [Tuples](#tuples) gives.

```anti
let (lo, f) = a.lo + b.lo;
let (hi, f) = a.hi + b.hi + f.carry;
if f.overflow {
	fail lang.Error.new(1, "sum does not fit");
}
```

## CPU levels

- The x86_64 baseline for everything Anti ships and for release builds is x86-64-v3. `--cpu v1` and `--cpu v2` stay available for a program that must run older hardware. The runtime archive's native libraries are built for v3.
- The ARM64 baseline is per operating system. `macos-arm64` is `armv8.5`, since every Apple Silicon Mac is an M1 or later. `linux-arm64` is `armv8.0`, for the Pi 4 and older boards. `windows-arm64` is `armv8.2`, since every Windows-on-ARM machine sold is a Snapdragon 8cx or later. `--cpu` overrides on every target. `armv8.2` and above make atomics one instruction and add half-precision arithmetic and the dot products.
- The runtime archive holds one `anti_rt` per target and level, in `lib/<target>/<level>/`. The runtime is small and it is what every program links, so it comes in every level its target supports. A program built with `--cpu` below its target's default links the runtime of its own level, and `--cpu v1` therefore gives a program that runs on older hardware.
- The native libraries stay at the default level only, in `lib/<target>/`. A program below the default that imports a bundled library is refused at link: "anti.raylib is built for x86-64-v3, this program targets v1".
- `src/rt/start.c` checks the processor once at start, against the level of the runtime that was linked. When the machine has less than the program needs, it exits with a message naming the level. The message reads "this program needs a processor with AVX2 (x86-64-v3, 2013 or later)".
- A level is a code-generation setting, not a target. The six targets stay six.
- The vector byte cap of [Simd structs](#simd-structs) is one constant in the level table, 256 bytes to start. It caps the size of a `simd struct`, not the width of a register.

## Simd structs

- `simd struct Vec4 { x: f32, y: f32, z: f32, w: f32 }` declares a vector. Every field is the same primitive type and the field count is a power of two. The size is a multiple of eight bytes up to the level table's cap, 256 bytes to start. The alignment is the size or sixteen, whichever is smaller. Fields are visible and named, so `v.x` is a lane.
- `+ - * /`, the bitwise operators on integer lanes, comparisons and unary minus apply element-wise, built in. A comparison yields a mask, a `simd struct` of `bool` with the same lane count. `simd.select(mask, a, b)`, `simd.any(mask)` and `simd.all(mask)` live in `anti.simd`.
- Built-ins on the type and on values, lowered straight to instructions: `Vec4.splat(v)`, `Vec4.load(slice, i)`, `v.store(slice, i)`, `v.shuffle(...)` with constant indexes, `v.sum()`, `v.min()`, `v.max()`, `a.dot(b)`. Anything that costs more than one instruction on the native width is a named function, so a reader sees it.
- `as` between a `simd struct` and the array or plain struct of the same bytes is free, both ways. A raylib `Vector4` becomes a `Vec4` that way.
- C layout: a 16-byte `simd struct` is `float32x4_t` on ARM64 and `__m128` on x86_64, passed and returned in vector registers. The header writes the vector type.
- The back end maps every operation to the target's native width. One instruction where the width exists, two or more where it does not. The same result everywhere. A `f32x8` is one instruction on x86_64 at v3 and two on ARM64. A `f32x64` is sixteen on ARM64. The lane count is the programmer's, the instruction count is the machine's.
- `f16` lanes are storage only, as `f16` is: every operation converts to `f32` lanes and back.
- Above the cap it is an array and a loop, with a message that says which. Variable-length vectors, scatter and gather, and vectorisation of scalar loops are not part of it.

## Tests and fixtures

- `tests { }` and `fixtures { }` are blocks at module level, at most one of each per module. Both see every private item of the module, the insides of its classes included.
- Every `fn` in `tests` is a test: no parameters, no return, named after what it checks. Every `fn` in `fixtures` may take parameters and return anything, is callable from the module's tests and fixtures, and is never run by itself.
- `setup` and `teardown` in `fixtures` are called before and after every test of the module when they exist.
- Both blocks are compiled only by `anti test`. A dev build, a release build and a `.antl` contain none of it.
- `anti test` compiles each module with its blocks, links a runner, runs every test in dev mode, and reports `module.test_name` with the file and line of the first failed `assert`. `--release` runs the tests again with checks and assertions off.
- The book keeps its programs-with-expected-output form for the compiler's own suite.

```anti
fixtures
{
	fn full_stack() -> Stack
	{
		let s = Stack.new(4);
		for 0..4 { s.push(1); }
		return s;
	}
}

tests
{
	fn push_grows()
	{
		let s = Stack.new(4);
		s.push(1);
		assert(s.top == 1);
	}
}
```

## Sum types

- `variant Shape { Circle { r: f32 }, Square { side: f32 }, Empty }` declares a tagged union. Each case has a name and zero or more fields.
- Layout is a struct of a tag and a union: the tag is the smallest unsigned integer that holds the case count, followed by a union of the cases' field sets, laid out by the target's C rules. `packed` and `align(N)` apply. C sees `struct Shape { uint8_t tag; union { struct { float r; } Circle; struct { float side; } Square; } u; }`, and the header writes the tag values as an `enum`.
- A literal names the case: `Shape.Circle { r: 2.0 }` and `Shape.Empty`.
- `switch` on a variant names the cases and binds the fields: `Circle c => c.r`, `Square s => s.side`, `Empty => 0.0`. Without `else` it must cover every case, and the message names the missing ones.
- `v is Shape.Circle` gives a `bool`. `v.tag` is the tag as its enum. There is no other access to a case's fields than `switch`, and `if let Circle c = s { }`, which is a `switch` of one arm.
- A variant is a value type with C layout. It may be a struct field, an array element, a parameter and a result. It passes by value under the struct rules. It cannot have functions, since it is a struct.
- `anti bind` never produces a variant, because C declares none. An exported variant is written as above.

## Locking and channels

- `anti.lang.Mutex` is created with `Mutex.new()` and released with `m.destroy()`. It is one word of the program's own memory and cannot be copied, as [Locks](#locks) gives.
- `sync m { }` locks `m` for the block and unlocks it on every exit, including `return`, `break`, `continue` and the error forms. Nested `sync` on the same mutex is a compile error when both are in one function and a run-time deadlock otherwise, which the chapter states.
- A field written inside a `sync` block and read outside any `sync` is a warning from the whole-program analysis. The warning fires when both sites are in functions a worker reaches.
- `chan T` is a bounded queue of `T` values, `T` pointer-free by the `parallel` rule. `let c = chan int(16);` creates one. `send(c, v)` blocks when full, `recv(c) -> ?*T` blocks when empty and returns `none` after `close(c)`. `select` waits on more than one channel and is written like `switch` over the channels. `chan`, `send`, `recv`, `select` and `sync` were reserved from chapter 2.
- Both are implemented in `anti.rt` over the platform layer of the threading chapter.

## Debug information

- `antic -g` writes a `.loc` directive before the first instruction of every statement and a `.file` directive per source file. llvm-mc turns them into DWARF on ELF and Mach-O and CodeView on COFF. The link step then keeps debug sections instead of stripping them.
- antic also writes the compile unit itself, because a line table alone names no source file and a debugger resolves nothing without one. `docs/decisions.md` holds its shape.
- With `-g` a debugger shows Anti source lines, sets breakpoints by file and line, and prints a backtrace with function names. Variables are not described in the first version, so `print x` shows nothing. That is the next step, and the closing guide names it.
- `anti build` passes `-g` in dev mode. Release mode never does.
- `.loc` costs nothing in the emitted code and is never a reason for a program to behave differently.

## Symbols tooling

The release binary carries no symbol data. Every deliverable ships a symbols archive beside it. `anti symbols` manages those archives.

- `anti build --release` writes, beside `prog`, `prog-symbols.zip` holding `prog.debug`, the same link with the debug sections kept, and `prog.map`, a text map of address ranges to function, file and line. Both carry the build id. A shared library or a plugin is its own deliverable with its own archive. Zip, because every host opens one without a tool.
- On Windows the twin is a PDB. lld-link writes `prog.pdb` when the link passes `/DEBUG`, so the archive of a Windows program holds `prog.pdb` beside the map of its sections. A release of Anti carries one PDB per program of each Windows host.
- `anti symbols inventory --conf config.toml [--from dir] [--out symbols.zip]` reads the runtime configuration, finds the program, the `plugins` directories and the `[injections]` libraries, reads every Anti binary's build id, and folds each one's symbols archive into one `symbols.zip` for the deployment, keyed by id, with an `index.toml` of module, id, version and source. It reports what it could not find.
- `anti symbols check --conf config.toml [--symbols symbols.zip]` walks the same binaries and reports per module whether its symbols are present, stale because the binary changed, or missing. Non-zero exit when anything is missing, so a deployment script can stop a rollout.
- `anti symbols resolve trace.txt --symbols symbols.zip` turns a raw trace into function names and lines, matching each frame to an archive by build id. More than one `--symbols` is allowed. An archive whose id matches no frame is ignored. A frame whose id matches no archive is printed raw.
- The `.debug` twin is preferred over the map when both are present, because it carries inlining. The twin holds root-relative paths and names and nothing else, so keeping it is a matter of size, not secrecy.
- The release layout on the site and on GitHub keeps `symbols/` beside each release, never in the package a user downloads.

## Namespaces

- `anti.lang` holds every type the compiler knows by name: `Object`, `Error`, `NoneDereference`, `SourceLocation`, `StackTrace`, `Flags`, `Job`, `Mutex`, `Trace`, `TraceHandler`. `anti.rt` is the C runtime and holds no Anti module a program imports. Everything that only helps lives elsewhere: `anti.error` for error conveniences, `anti.trace` for the stock handlers, `anti.log`, `anti.time` and the rest.
- The rule for a reader: if the compiler needs it, it is in `anti.lang`. If it only helps, it is not.
- `anti.lang` is the root of the standard library and imports nothing. Every other module imports it and names `*anti.lang.Error`, so no import cycle forms. `anti.error` imports `anti.lang` and holds `SystemError`, `on_fatal` and `check`.
- `src/std/anti/lang.anti` holds `Error`, `NoneDereference`, `SourceLocation`, `StackTrace` and the hook that `fatal` reads. The compiler declares `Object` and `Job` itself, under `anti.lang`. `Flags` and `Mutex` come into the code with their own steps.

## Hooks and tracing

- No default base class per package and no replacement of the root. Instrumentation is a build option and never a change to an object's layout.
- `anti.lang.Object` declares nine hooks with empty bodies. Lifecycle, always compiled: `created(self)` after `construct`, `destroyed(self)` before the `destruct` chain, `copied(self, from: *Object)` after `dup`. Threads, always compiled: `dispatched(self)` and `joined(self)`. Calls, under tracing: `enter(self, name: str)` and `leave(self, name: str)` around every `pub` function of an instrumented class, `failed(self, name: str, e: *Error)` when one returns an error. Data, under `--trace writes`: `changed(self, field: *FieldDescriptor)` after a write to a field of an instrumented class.
- `anti.lang.TraceHandler` is an abstract class with the same nine functions taking the object as a parameter. `anti.lang.Trace.install(h)` stores one handler in a static atomic pointer.
- Every hook site does two things. It calls the installed handler when there is one. Then it dispatches the object's own hook. On `leave` the order is reversed, so handler and object hook nest. A class that replaced a hook does not silence the handler, and the handler does not replace the class's hook.
- `trace` is a contextual word before `class` or `fn`. It marks code whose author wants the call hooks. It follows the `assert` rule: on in dev mode, off in release. `--trace <pattern>` instruments code that did not ask, a package or a class by name, in any mode. `--trace` without a pattern and `--no-trace` override the mode for `trace`-marked code.
- Cost without a handler and without tracing: one load and one compare per object creation, destruction, copy, dispatch and join. Nothing per call. `--no-hooks` removes the five always-on hooks as well, for a target that cannot afford them.
- `anti.trace` ships handlers that implement `TraceHandler`: `LeakTracker`, `Profiler`, `CallLogger`, `ErrorMonitor`, `ThreadMonitor`, `ChangeJournal` and `Composite`. The runtime key `trace` selects one by name at start. Each is tested with the `tests` block. Together they show that the nine hooks are enough.

## Injection

- `inject name: *Interface` is a field marker like `own`. The field's type is a pointer to an abstract class. No literal may set it and the class never constructs it. Before `construct` runs, the field is filled by calling a provider.
- The provider is chosen in the manifest, per interface, per build:

```toml
[inject]
"anti.log.Logger" = "net.niese.ConsoleLogger.get"
"anti.time.Clock" = "net.niese.SystemClock.get"

[inject.test]
"anti.log.Logger" = "net.niese.tests.FakeLogger.get"
```

- A provider is a static function returning a pointer of the interface type that is never `none`, or a class name whose `get` is a singleton's. `anti build` passes the table to `antic` as `--inject Interface=Provider`. `anti test` passes the test table.
- An interface with no provider is a link-time error naming the class that needs it. A provider of the wrong type is a compile error. A cycle through providers that allocate is detected on the provider graph at link.
- Every `inject` field is resolved through a slot, a static atomic pointer per interface, initialised to the compiled-in provider. Construction calls through the slot. The run-time configuration may replace what a slot holds. See [Runtime configuration](#runtime-configuration).
- `inject final name: *Interface` marks a field the run-time configuration may not replace.
- A program with any `inject` field exports its runtime symbols and its descriptors, so a plugin can share them. `--closed` builds it without the exports, and the program then accepts no run-time replacement.
- Programmers declare and inject their own interfaces the same way. A library ships an interface under its own root with a default provider. Its users override it in their manifest.
- A `.antl` records which interfaces its classes inject, so `anti build` reports what a dependency needs.

## Standard interfaces

- Six abstract classes ship with a default implementation and a default provider, so `inject log: *Logger` works with no manifest entry: `anti.log.Logger`, `anti.time.Clock`, `anti.random.Source`, `anti.fs.FileSystem`, `anti.mem.Allocator`, `anti.config.Config`. A fake of each makes tests deterministic, and a replacement changes a whole program's behaviour.
- The language's `alloc` and `free` stay bound to libc. `Allocator` is what libraries and classes ask for explicitly, and the containers in `anti.collection` take one. `Object.deserialize` takes one as well, and `delete(p, from)` and `destroy(p, from)` give memory back to one. Text that should live in an allocator is built with `anti.text.Builder`, whose `take_in(from)` takes the allocator explicitly.
- Interfaces for services where a second implementation is likely are published before any driver exists, each with a reference implementation in the runtime archive and a conformance `tests` block that any implementation runs: `anti.db` with SQLite as the reference and default, `anti.http` with a client over `anti.net` and a handler interface for a server, `anti.serialize` with `anti.json` as the reference, `anti.crypto` with Mbed TLS as the reference.
- `anti.db` standardises the plumbing: connect, prepare, bind, execute, rows and columns, transactions, the error class. `?` placeholders are bound by position and translated by each driver. The SQL itself passes through untouched, and the site names the portable subset.
- SQLite joins the runtime archive as a native library, public domain, between PCRE2 and Mbed TLS in the build order. `anti.db` opens it by default with a path the program gives.
- `anti.graphics` and `anti.audio` are Anti classes over raylib and miniaudio. No raylib or miniaudio type appears in a `pub` signature of either, so a backend can be swapped through `[inject]` later without designing one now. A program that needs more imports `anti.raylib` or `anti.miniaudio` directly and is raylib's or miniaudio's from then on.
- Interfaces under `anti.*` are the language's and change by version. Implementations live under their author's root.

## Plugins

- Loaded code is a shared library built by `anti build --lib shared --no-runtime`. It uses the host's runtime, heap, pool and descriptor registry. There is no compilation at run time.
- `provides Interface as Class;` at module level declares what a library offers, one line per interface, any number per library. The compiler checks that the class is complete and implements or inherits the interface. It writes a table into the library: interface descriptor, class descriptor, factory, versions.
- `plugin.load(path) -> *Library` opens the library, checks its runtime version and each interface against the host, runs its constructor, which registers its descriptors, and returns a handle. `lib.instance(Interface)` constructs the class the library provides for that interface and returns a pointer of the interface type. `lib.supports(Interface, "name")` reports whether a slot is filled. `lib.unload()` refuses while any object created from the library exists, counted through `created` and `destroyed`.
- Every class in a loaded library is a full citizen: `is`, `as`, `dup`, `delete`, `reflect.new` and `serialize` work across the boundary.
- Static atomics in a loaded library belong to that load and are not shared with the host's.
- Two providers select a plugin in the manifest: `"plugin:path"` loads that library and asks it for the interface, and `"discover"` searches. Discovery reads an index file, `anti-plugins.toml`, in each directory, written by `anti build --lib shared` and `anti publish`, listing each library with its interfaces, versions and digest. Discovery never opens a library to find out what is inside. It searches the directories of the `plugins` key in their order, then the libraries that shipped with Anti, and never the working directory unless the key names it. Two libraries providing one interface is an error naming both paths unless the configuration picks one. A library is verified against its digest before it is opened, and a runtime version mismatch is logged and skipped.

## Versions

- Every class descriptor carries the version of the package that declared the class. An abstract class also carries a structural hash of its tables. The hash covers the entries in order with their names and signatures. It carries a chain of the hashes of its earlier versions with the slots each had.
- `compatible 1.1;` in an abstract class body sets the lowest version a plugin may have been built for. `compatible` is a contextual word in that position. Without it the floor is where the chain says the structure last changed incompatibly.
- The runtime carries `anti_rt_version`. A library records the runtime version it was built against. `load` requires an exact match, because the library uses the host's runtime.
- The program carries, per injectable interface, the set of table slots it can reach. The whole-program analysis pass computes it and stores it as a bitmap. A program that uses `reflect.call` on an interface counts as reaching every slot.
- At load, per provided interface, three checks and no compilation. The plugin's hash is in the program's chain, or refuse. No field was added between the plugin's version and the program's, or refuse. Every slot the program reaches is in the plugin's table, or refuse naming the function. A newer plugin in an older program passes when the same rules held between the two versions. For a reflection-reached slot the plugin lacks, the loader installs a stub. The stub fails with the plugin's name and version when called.
- The rule for interface authors. An interface stays compatible with its older versions while every new function has a default body. Adding a field ends that.

## Runtime configuration

- A program reads a configuration file only when `--anti.conf=<path>` or the environment variable `ANTI_CONF` names one, or when `main` calls `rt.configure(path)` as its first statement. The runtime never searches for a file. Without one the program runs as it was built. There are no other environment variables.
- The file is TOML:

```toml
include = [
	'/etc/anti/injections.toml',
]

[runtime]
threads = 4
logger = '/etc/some_program/log.toml'
plugins = ['/opt/some_program/lib']
trace = 'leaks'

[injections]
"anti.log.Logger" = 'lib/CISOLogger.so'
```

- `include` is processed first, in order, depth first, and then the file's own keys, so the including file wins per key. A relative path is relative to the file that names it. A missing include and a cycle are startup errors naming the path.
- Every key of `[runtime]` is also a command-line option with the same name under the reserved prefix: `--anti.threads=4`, `--anti.logger=`, `--anti.plugins=dir:dir`, `--anti.trace=leaks`, and `--anti.inject=Interface=path` once per interface. `--anti.conf` names the file and is the one option that is not a key. `--anti.inspect` prints the effective value of every key with the layer it came from, the injectable interfaces with versions and used slots, the loaded plugins and the runtime version, then exits. `--anti.help` lists the runtime's options.
- The runtime consumes every `--anti.` argument before `main` sees `args`, in one pass, and hands the rest to `main` untouched and in order. A program cannot define an option under the prefix. An unknown `--anti.` name is a startup error listing the keys.
- Precedence per key, highest first: the command line, the configuration file with its includes, the build.
- `[injections]` replaces the provider in the slot of the named interface with the class the named library provides. The version checks of [Versions](#versions) run first. A line for an interface the program does not have is a startup error naming what it does have. So is a line for an `inject final` field.
- `os.user_config_dir(app)`, `os.user_data_dir(app)` and `os.user_cache_dir(app)` give the per-user directories of the platform, and they are the ones an install of Anti itself uses. Each of them may fail, `-> str may fail`. A missing home is a real failure, and the message names the variable that is absent, `HOME` or `LOCALAPPDATA`. On Linux and macOS they are `$XDG_CONFIG_HOME/<app>`, `$XDG_DATA_HOME/<app>` and `$XDG_CACHE_HOME/<app>`, which fall back to `~/.config/<app>`, `~/.local/share/<app>` and `~/.cache/<app>`. On Windows they are `config\` and `cache\` of `%LOCALAPPDATA%\<app>`, and `%LOCALAPPDATA%\<app>` itself. `os.site_config_dir(app)` gives the machine-wide directory that a program reads and never writes, `/etc/<app>` on Linux, `/Library/Application Support/<app>` on macOS and `%ProgramData%\<app>` on Windows. A desktop application passes one of them to `rt.configure`. A program ships no configuration file, since the build is the default, and a file holds only what someone changed.
- `anti sdk export` on a Mac and `anti sdk import` on any host move the stubs of Apple's SDK. A program that needs a framework links against that SDK, and every other program against the stubs of the runtime archive. Both are in [SDK and frameworks](#sdk-and-frameworks).

## Small things

Each is compile-time only. Each removes something people write by hand. None costs anything in a release build unless it says so.

- `embed("shaders/basic.glsl")` is a constant of type `[]byte` holding the file's bytes. The file is read at compile time relative to the source file and emitted in read-only data. A missing file is a compile error naming the path.
- `undo stmt;` runs `stmt` only when the enclosing block exits through an error, in reverse order of declaration, before the `defer` statements of the same block run. `defer` is "always", `undo` is "only if this block fails".
- `unreachable;` traps in dev mode with file and line. In release mode it is undefined and the optimizer may drop the branch that leads to it.
- `let buf: [65536]byte = undefined;` opts one local out of mandatory initialisation. Dev mode fills it with `0xAA`. Allowed for locals only, not for fields. Reading it before a write is what the fill makes visible.
- Labels. `outer: for ... { }`, `retry: while ... { }` and `cleanup: { }`. `break outer;` and `continue outer;` name the loop or block. There is no `goto`. A label is an identifier followed by `:` before `for`, `while` or `{`.
- `show(expr)` prints `file:line: expr = value` to stderr and yields the value, so it sits inside any expression. Gone in release, like `assert`. The value is printed through `to_text` for a class and through the primitive formats otherwise.
- `if let Circle c = v { }` on a variant `v` runs the block with `c` bound to the case's fields when `v` holds that case. It is a `switch` with one arm and no `else`, and `else { }` may follow.
- `p ?? q` on a `?*T` yields `p` as `*T` when it is not `none` and `q` otherwise. `q` has type `*T` or `?*T`, and the result has the wider of the two.
- `p?.x` and `p?.f(args)` on a `?*T` yield `none` when `p` is `none` and otherwise the field or the call. The result has type `?*U` when the field or result is a pointer, and is refused otherwise, since Anti has no optional values. Chains follow the first `none`.
- Default parameter values: `fn open(path: str, mode: Mode = Mode.Read) -> *File may fail`. The default is a constant expression. Named arguments: `open("x", mode: Mode.Write)`. Positional arguments come first and in order. Named ones follow in any order, each at most once. No positional may follow a named one. Defaults fill what is not given.
- `switch` on a `str` compares with `text.equal` in a chain, in arm order. The chapter says it is a chain and not a table.
- `x in lo..hi` is `x >= lo && x < hi`. `in` applies to ranges only. Membership in a slice is `slice.contains(x)`, a call, because it is a search.
- Format specifications in `f"..."`: `f"{x:08.3f}"`, `f"{name:>20}"`, `f"{n:x}"`, `f"{n:b}"`. Width, precision, alignment with `<`, `>` and `^`, zero padding, `x`, `X`, `b`, `o` and `e`. Parsed at compile time into calls of `anti.text`. An unknown specification is a compile error.
- `f"..."` and `rf"..."` take the memory of their text from the C library, and the program frees it with `free(s.ptr)`. They take no allocator. Text that should live in an `ArenaAllocator` is built with `anti.text.Builder` and taken with `take_in(from)`.
- Compile-time targets. `target.os`, `target.cpu` and `target.mode` are constants of the enums `Os { Linux, MacOS, Windows }`, `Cpu { X86_64, Arm64 }` and `Mode { Dev, Release }`. A `switch` on one of them is allowed at module level, where its arms hold declarations, and inside functions. It follows the exhaustiveness rule of every `switch`: every value or `else`. Lowering keeps every arm in the IR, tagged with its condition, and the back end keeps the arm for its target and drops the rest before optimisation. Two arms may declare the same name. A library file therefore serves all six targets.
- `anti check --targets all` runs the front end once per target. A program that type-checks on the host is then proven to type-check on all six.
- Script mode. A file whose first line is `#!/usr/bin/env anti` runs with `./tool.anti`. `anti file.anti` compiles the file as a dev build into `<cache>/scripts/<digest>/` of the user cache directory, keyed by the file's digest and the compiler version, and runs the result. A second run is a cache hit. No manifest, and `anti.io`, `anti.text` and the rest of the standard library are available.

## Small items, round three

- `fallthrough;` as the last statement of a `switch` arm continues into the next arm's body without testing its values. Not in the last arm, and not into an arm that binds a variant's fields.
- `rf"..."` and `rf#"..."#`: interpolation without escape processing. `{expr}` and the format specifications work as in `f"..."`, every backslash is literal, `{{` and `}}` write a brace. `fr` is refused with a message naming `rf`.
- `x"00 AB CC"`: a `[]byte` literal of hex pairs with whitespace ignored. An odd digit count or a non-hex character is an error naming the position. No hash delimiters, since the content is hex and spaces.
- The string prefixes are `r`, `b`, `br`, `f`, `rf`, `x` and `re`, letters only, one meaning each, listed in one table. No word-form prefixes. `re"..."` is a pattern literal, see [Regular expressions](#regular-expressions).
- `f16` is a storage type: sixteen bits in a field, an array or a slice, read as `f32`, written with `as f16`. No arithmetic on it. The conversion is one instruction on ARM64 and on x86_64 at v3, and a runtime routine at `v1` and `v2`.
- The compiler checks every pattern literal itself, and a malformed one is a compile error with the position PCRE2 names. It replaces the rule of round three, under which `anti check` compiled the constant patterns passed to `regex.compile`. See [Patterns](#patterns).
- `none`, replacing `null`: the value of a `?*T` or `?fn` that points at nothing. On every current target it is represented as address 0, so that C's `NULL` and Anti's `none` are one value across a call. The language does not promise that representation, and a target where address 0 is memory may choose another.
- A static function is namespaced by its class and may share a name with a static in the chain. The redeclaration rule covers fields, functions that take `self`, and constants.
- Considered and declined: a power operator `**`. It would be the one arithmetic operator that compiles to a library call, and `math.pow` says that it is one. Considered and declined: multiple names in one `let`, `let a, b = e;`, since a `let` binds one name and the two-name form is tuple destructuring.

## Base in the class header

- `class Circle inherits Shape { }` names the base in the header, and `inherits Shape,` as the first line of the body is gone. The base is no member: it sits at offset 0, has no name of its own, is reached as `self.super`, and a class has at most one, so it belongs to the header. `implements` and `use` stay in the body, because each is a named sub-object with a place in the layout.
- The header takes the base after every modifier: `abstract class`, `final class`, `singleton class`, `pub class` and `packed class`. A base of another module is qualified by it, as in `class Circle inherits shapes.Shape { }`.
- `inherits` in the body is refused, and the message gives the header the programmer means.

## Wire formats

A wire format is a description of bytes from which `anti format` generates a parser, a writer and the classes that hold the parsed data. It is a tool, like `anti bind`. The language gains no keyword. The description lives in a `.fmt` file and the generated module is ordinary Anti over `anti.binary`.

```text
format NetworkPacket endian big
{
	ip: [4]byte,
	ip6: [16]byte,
	seq: u64,
	packet_length: u16,
	body: packet_length bytes Body
	{
		magic: str : 4 = "ANTI",
		flags: u16
		{
			compressed: bit 0,
			encrypted: bit 1,
			version: bits 2..6,
			_: bits 6..16,
		},
		header_length: u8,
		headers: [header_length] Header
		{
			kind: u8 as Kind,
			name_length: u8,
			name: str : name_length,
			value: str until 0x00,
		},
	},
}
```

Rules:

- `format Name endian big|little { }`. The byte order is required. A field may override it: `seq: u64 endian little`.
- Fixed fields: the sized integers, `f32`, `f64`, `bool` as one byte, `[N]byte` and `[N]T` for a fixed-size type `T`. `str : n` and `[]byte : n` take `n` bytes, where `n` is a constant or a path to an earlier field.
- Terminated fields: `str until 0x00`, `str until "\r\n"`, `[]byte until ','`. The terminator is consumed and not part of the value. `until ... keep` leaves it in the value. `[]byte until end` takes the rest of the enclosing group.
- Literals: `magic: str : 4 = "ANTI"` and `version: u8 = 2`. The parser checks and fails naming the expected and actual value. The writer writes the constant. The field is not in the generated class.
- Enums: `kind: u8 as Kind` for an enum declared in the same file or imported. A value outside the enum is a parse error.
- Groups: `name: size bytes ClassName { }` for a group that occupies that many bytes, and `name: [count] ClassName { }` for a group repeated that many times. The class name is optional, and a group without one gets the format's name followed by the field name, `NetworkPacketBody`. Groups nest.
- Paths: a bare name resolves from the innermost group outward. A qualified name is a path from a named group, `body.header_length`, or from the format, `NetworkPacket.packet_length`. A bare name that matches at two levels is refused with both paths. A field may reference only fields that come before it in the byte stream.
- Bit containers: an integer field followed by `{ }` with one line per bit field. Each names its bits in the assembled value, `bit 0`, `bits 2..6`, or a comma-separated list of pieces from most to least significant, `bits 2..4, bit 15, bit 12`. Bit 0 is the least significant. Ranges are half-open. Every bit of the container is named exactly once, by a field or by `_`, and an unaccounted or doubly named bit is an error naming the bits. A one-bit field is a `bool` in the generated class and a wider one the smallest unsigned type. The sequence form, `msb` or `lsb` after the container and fields with a width only, is allowed as an alternative and never mixed with positions in one container.
- Padding: `pad: 3 bytes` and `align 4`. Both write zero.
- The generated module holds one class per group, all at module level, since a user works with them directly. [Nested types](#nested-types) does not apply to them. The format's class has `fn construct(self, data: []byte) may fail`, so `alloc NetworkPacket(data) catch e { }` parses a buffer, and a static `parse(r: *binary.Reader)` for a packet inside a stream. `write(self, w: *binary.Writer)` and `size(self) -> int` are generated. Length and count fields are computed on write and are not fields of the class.
- `str` and `[]byte` fields are slices into the buffer, so the object is valid while the buffer is. `format Name endian big copy { }` makes them `own` copies instead.
- Conditions, `if version >= 2 { }`, are the second version.

## Binary I/O

`anti.binary` is what the generated code and hand-written parsers use.

- `Reader` over a `[]byte` with a position. `take(T) -> *T` returns a pointer to a packed struct at the position and advances by `size_of(T)`. `u8()`, `u16_le()`, `u16_be()`, `u32_le()`, `u32_be()`, `u64_le()`, `u64_be()`, `f32_le()` and the rest name their byte order. `bytes(n) -> []byte`, `str(n) -> str`, `until(terminator) -> []byte`, `rest() -> []byte`. Every function returns `*Error` at the end of the buffer and never reads past it. `position()`, `seek(n)`, `remaining()`.
- `Writer` is the mirror, over a growable buffer or a fixed one. It has the same names, and `finish() -> []byte`.
- A fixed layout needs neither: a `packed struct` over the bytes is the parse, and filling one is the write.

## SDK and frameworks

- The runtime archive carries libSystem stubs for macOS 11 and later in `sysroot/macos-<cpu>/`. They are taken from a pinned Zig release under Zig's MIT licence. They are the default on every host, the Mac included, so the same object links to the same bytes anywhere. A program that needs no framework links against them.
- A program that needs a framework links against Apple's SDK. A binding declares its frameworks with `link framework "CoreAudio";` at module level. `anti bind` writes the line from a table, and `anti` passes the names to antic as `--framework <name>`. A program never names a framework itself.
- On a Mac the SDK is found from the Command Line Tools. Elsewhere a repository build passes `APPLE_SDK=<path>` to `get-sysroot.cmake`. An installed Anti uses two commands. `anti sdk export` on macOS packs the SDK's `.tbd` stubs and version into `apple-sdk-<version>.tar.xz`. `anti sdk import <file>` on any host unpacks that bundle into the sysroot and records its digest. `import` reads a file and fetches nothing. Both use the host's `tar`.
- Without the SDK a framework link fails with a message naming the frameworks and where a Mac keeps the SDK. Nothing of Apple's is ever served from the download area. The README says in three sentences what the shipped stubs cover and that frameworks need the bundle from a Mac. It says that Apple's licence governs where the bundle may be used.
- Two builds of a framework program are byte-identical across hosts only when the same SDK version was used. Builds of every other program are byte-identical across hosts. A test links one program for `macos-arm64` on the Mac and on the Linux VM and compares the files.

## Regular expressions

Regular expressions belong to `str`, as they belonged to text in Perl, without Perl's hidden global variables. The engine is PCRE2, built in `src/native/`. A program links it only when it uses a pattern.

### Patterns

- `re"..."` is a pattern literal of type `Regex`. It is raw: a backslash is a backslash. It takes the hash delimiters of every string form, `re#"..."#`.
- The compiler checks every pattern literal. A malformed pattern is a compile error with the position PCRE2 names.
- Flags are PCRE2's inline flags, `i`, `m`, `s` and `x`, over the whole pattern or a group: `re"(?i)abc"`, `re"(?i:abc)D"`. There is no suffix form.
- In a text pattern, `\d`, `\w` and `\s` mean their ASCII sets: `\d` is `0` to `9`, and `\w` is `[A-Za-z0-9_]`. Unicode classes are written out, `\p{L}` for any letter and `\p{Nd}` for any decimal digit. `(*UCP)` at the start of a pattern gives the three their Unicode meaning. The default keeps `\d` from accepting a digit that `text.parse_int` rejects.
- `Regex.compile(text: str) -> Regex may fail` compiles a pattern at run time. It fails with `regex.BadPattern`, which carries PCRE2's message and position. A pattern built from parts uses `rf"..."`, which interpolates without escape processing.

### Failures, by where a pattern comes from

A call fails exactly when a failure is possible. That depends on where its pattern comes from, which the compiler knows.

- A pattern literal cannot make a method fail. A replacement template that names a group the pattern lacks is a compile error. A pattern that can take exponential time, such as `(a+)+`, fails the safety check `exponential-pattern`. The compiler finds nested repeats over text that overlaps. Its message names PCRE2's possessive quantifiers and atomic groups, `a++` and `(?>...)`, which forbid the backtracking. Where the input is known to be short, `unchecked(exponential-pattern, "reason")` overrules it.
- A pattern literal can still reach the match limit on a large input, as `.*a.*a.*a` can on a megabyte. That is a mistake in the program. It stops the program with the pattern and the line, as an index out of bounds does. It cannot be caught.
- A pattern compiled at run time makes every method that takes it `may fail`. The failures are classes of `anti.regex`: `TooExpensive` for the match limit and `MissingGroup` for a template that names a group the pattern lacks. Both inherit from `regex.Error`, as does `BadPattern`.
- One name serves both origins. `line.matches(re"...")` needs no handler, and `line.matches(r)` with a compiled `r` needs one. The compiler enforces which.

`catch none` treats a failure as `none`, where the result can be `none`. It is how a program chooses, at the call, to count a failure as no match:

```anti
let m = line.matches(r) catch none;
let m = line.matches(r) catch e {
	if e is *regex.TooExpensive {
		io.println("that pattern is too slow on this text");
	}
	yield none;
};
```

### Thread safety

Nothing a match reads or writes is global.

- A compiled `Regex` never changes. Any number of threads may use one at once, and it may be passed to a worker as a `str` may.
- Every pattern literal is compiled once, at program start before `main`. No pattern is compiled lazily, so no hidden flag guards a first use.
- All state of one match belongs to that call. The engine's working memory and the capture positions live in the caller's frame or in the returned match.
- A match's texts are slices of the searched text. Nothing is copied, and a match is valid while that text is.
- The match limit that stops a runaway pattern is fixed when the pattern is compiled. No thread can change it for another.

### Methods of `str`

- `s.matches(r) -> ?Match` gives the first match, or `none`.
- `s.find_all(r, limit: int = 0)` gives every match, left to right, for a `for` loop: `for m in s.find_all(r) { }`.
- `s.replace(r, with, limit: int = 0) -> str` replaces matches. `with` is a template or a function, as described below.
- `s.split(r, limit: int = 0)` gives the pieces between the matches, left to right, for a `for` loop, and `to_slice()` collects them into a `[]str`.

`limit` of 0 means every match. A positive `limit` takes that many matches from the start, and a negative one that many from the end. The search itself always runs from the start. A negative limit picks the last of the matches found that way, so the result never depends on direction. Text is stored in logical order, the order it is read. So "from the start" is the reading direction of every script, right-to-left ones included. No locale or text direction is consulted, since either would be a global setting.

### The match

A match result behaves as a nullable value, with the rules of `?*T`. It compares with `none`, narrows after a test, and works with `if let` and `let ... else`. Its fields are unreachable until a test proves it matched.

```anti
let m = line.matches(re"(?<year>\d{4})-(?<month>\d\d)");
if m == none {
	fail "no date";
}
io.println(f"{m.year} / {m.month}");
```

A match result also stands on its own as a condition, in `if`, `while`, `&&`, `||` and `!`, meaning "matched": `if line.matches(r) { }`. It converts to `bool` nowhere else. When a result is only tested and never bound, the compiler calls the cheaper engine path that computes no captures.

| Field | Meaning | Type |
|---|---|---|
| `m.all` | the whole matched text | `str` |
| `m.group(n)`, `m.group("name")` | a group by position or name | `str` |
| `m.1`, `m.year` | a group as a field, for a pattern literal only | `str` |
| `m.took_part(n)`, `m.took_part("name")` | whether a group took part | `bool` |
| `m.count` | the number of groups | `int` |
| `m.pre` | the text before the first match | `str` |
| `m.post` | the text after the last match | `str` |

- A group that did not take part gives the empty `str`. `took_part` tells it apart from a group that matched nothing.
- `pre`, the matches and `post` together always cover the whole text. With no matches, `pre` is the whole text and `post` is empty.
- For a pattern literal the compiler knows every group, so `m.1` and `m.year` are checked at compile time. A pattern compiled at run time has `group` and `took_part` alone.

### Replacement

- A template names groups with `$1` and `${name}`. `$` reads every digit that follows it, so `$10` is group 10, and group 1 followed by a `0` is `${1}0`. `$$` writes one `$`. Against a pattern literal, a group the pattern lacks is a compile error. Against a compiled pattern, it is a `regex.MissingGroup` failure.
- A function receives each match and returns its replacement: `with: fn(Match) -> str`. It may be a named function or an anonymous one, described below.

```anti
let d = "2026-09-23".replace(re"(?<y>\d{4})-(?<m>\d\d)-(?<d>\d\d)", "${d}/${m}/${y}");
// "23/09/2026"
```

## Bytes

The pattern methods of `str` work on `[]byte` too, in byte mode, for binary data such as file formats and network streams.

### Byte patterns

- There are two pattern types. `Regex` searches text, and `ByteRegex` searches bytes. In byte mode `.` matches any byte, `\x89` means the byte 0x89, and nothing needs to be valid UTF-8.
- A pattern literal takes its mode from where it is used, as an anonymous function takes its types from its target. `data.find_all(re"...")` is byte mode because `data` is a `[]byte`. A literal stored in a variable is a `Regex` unless the variable says otherwise: `let r: ByteRegex = re"\x89PNG";`.
- `ByteRegex.compile(text: str) -> ByteRegex may fail` compiles a byte pattern at run time.
- A `Regex` passed to a method of `[]byte`, or a `ByteRegex` to a method of `str`, is a compile error.
- In a byte pattern, `\d`, `\w` and `\s` are the ASCII sets.
- A character outside ASCII in a byte pattern stands for its UTF-8 bytes. `re"versión"` matches the bytes of `"versión".to_bytes()`, since the source file is UTF-8 as well.
- A class in a byte pattern lists ASCII bytes and ASCII ranges freely. It may also list characters outside ASCII. Each becomes a choice of its byte sequence: `[óa]` is `(?:\xC3\xB3|a)`. A negated class that holds such a character is a compile error that names text mode. So is a range that reaches beyond ASCII. Byte mode cannot know where a character starts in either.

### Text in bytes

UTF-8 gives every byte of a character beyond ASCII a value of 0x80 or more. So an ASCII byte never stands inside one. A byte pattern built only from ASCII pieces matches only whole ASCII bytes, and never cuts a character. A pattern that matches bytes of 0x80 and up can match part of a character, through `.` or an explicit `\xC3`. A replacement can then leave invalid UTF-8. Data that is text in any script is converted with `to_text()` and searched in text mode. There a match never splits a character.

A letter can be encoded as one code point or as a base letter and a combining mark. The two look alike and compare different in both modes. Matching text from mixed sources normalizes both sides to one form first, which is a standard-library function for later.

### Conversions

- `s.to_bytes() -> []byte` copies a `str` into new bytes. It must copy, since a `[]byte` can be written and a `str` never changes. The name says a new value is made, as `to_text()` does.
- `data.to_text() -> str may fail` makes a `str` from bytes. It checks the bytes are valid UTF-8 and fails when they are not.

### Byte methods

- `matches`, `find_all`, `replace` and `split` work on `[]byte` as they work on `str`, with the same `limit`, the same failures by pattern origin and the same rules for threads.
- A match's fields are `[]byte` slices of the searched data, so they copy nothing and go straight to the readers of `anti.binary`.
- A replacement template is a byte string with the same group references, `b"...$1..."`.
- `replace` returns a new `[]byte`, since a replacement may differ in length from what it replaces. Its memory comes from the C library and is freed with `free(result.ptr)`.

### Patching in place

`patch` writes bytes over part of what it finds, where it stands, and allocates nothing. It finds either an exact byte sequence or a pattern.

```anti
data.patch(x"80 10 20 30", x"81");            // 81 10 20 30
data.patch(x"80 10 20 30", x"FF", at: 2);     // 80 10 FF 30

let v = "version: 1.10.1".to_bytes();
v.patch(re"version: \d+\.\d+\.(\d+)", b"2"); // version: 1.10.2
```

- `data.patch(find, with: []byte, into = 0, at: int = 0, limit: int = 0) -> int` gives the number of places it patched.
- `find` is a `[]byte` or a byte pattern. For a `[]byte`, the span to write into is the whole occurrence. For a pattern with one group it is that group, and without groups it is the whole match. A pattern with more groups names its group with `into`, by number or by name. For a literal pattern, leaving it out is a compile error.
- `with` is written at offset `at` inside that span. It must fit: `at` plus the length of `with` is at most the span's length. The rest of the span keeps its bytes. The data never changes length.
- A `with` that does not fit is a compile error when `find`, `with` and `at` are all literals. When any of them is computed at run time, the call `may fail` with `LengthMismatch`, by the same rule of origin as patterns.
- `limit` counts as for `replace`: 0 for every place, a positive number from the start, a negative one from the end.
- A span written by pattern can be longer than `with`. `"version: 1.10.10"` patched with `b"2"` gives `1.10.20`, not `1.10.2`: the span `10` keeps its second byte. `patch` fits fields of a fixed size. Data whose length may change uses `replace`, which returns new bytes.
- `str` has no `patch`. Text never changes, so a `str` can be shared between threads and sliced safely, and only `replace`, which returns new text, applies to it. Text that must be edited in place goes through a `text.Builder`, or a copy made with `to_bytes()` that is patched and turned back with `to_text()`.

## Language hooks

A type joins a construct of the language through `operator fn`, as it already does for `+`. The name comes from a fixed table, so the compiler checks it: `operator fn nxt` is an error that lists the valid names, and a hook with the wrong signature is an error that states the right one. An `operator fn` is also an ordinary method, so `a.add(b)` is the same call as `a + b`.

| Construct | Operator names |
|---|---|
| `+ - * / %`, unary `-` | `add`, `sub`, `mul`, `div`, `rem`, `neg` |
| `==`, `<`, and the comparisons derived from them | `eq`, `lt` |
| bitwise operators | `and`, `or`, `xor`, `shl`, `shr`, `not` |
| `for x in e` | `iter` on the collection, `next` and `value` on the iterator |
| `e[i]`, `e[x, y]` | `index` to read, `set_index` for `e[i] = v` and `e[x, y] = v` |
| a key of a hashing collection | `hash` |

`index` and `set_index` take any number of indices, each with its own type. `operator fn index(self, x: int, y: int) -> T` makes `g[x, y]` read, and the matching `operator fn set_index(self, x: int, y: int, v: T)` makes `g[x, y] = v` write.

`f"..."` writes a class through `to_text`, which every class has.

The guide gives this table as the one place a programmer looks to make a type work with the language.

## Iteration

`for x in e` walks more than ranges and slices. Two roles make it work, and neither allocates.

- An iterator has `operator fn next(self) -> bool` and `operator fn value(self) -> T`, and holds a position. `for` calls `next`, and while it gives `true`, binds `x` to `value()` and runs the body.
- A collection has `operator fn iter(self)`, which returns a new iterator on every call. `for x in e` on a collection calls `iter()` first, so every loop starts at the beginning and nested loops each keep their own position. The collection itself never changes, so any number of loops and threads can walk it at once.
- Each iterator declares its own `value()` with its concrete type, so iteration needs no generics.
- Ranges and slices keep their built-in forms.

`while` uses the same hooks, called by name, when the iterator itself is needed after the loop or halfway through it:

```anti
let it = people.iter();
while it.next() do {
	let p = it.value();
	if p.age >= 65 {
		break;
	}
}
```

`for x in e` is the one form for walking a collection, and `for x in &e` walks its elements in place, as [Walking a collection](#walking-a-collection) gives it. There is no binding form of `while`.

`find_all` and `split` return iterators. They are lazy: each step finds the next match or piece, and nothing is allocated. `.to_slice()` collects everything into a new `[]Match` or `[]str`, freed with `free(result.ptr)`. The `to_` says a new value is made. `limit` stops the iterator early. The iterator of `find_all` carries `pre`, the text before the first match, and `post`, the text after the last, which is known once the loop has ended.

```anti
for m in line.find_all(r) { }
let parts = line.split(re",\s*").to_slice();
```

With generics, `anti.collection.Iterable<T>` and `Iterator<T>` follow as interfaces, for a function that takes any collection of a given element type. A class with the `iter` hook implements them without extra code. [Collections](#collections) places them in `anti.collection`.

## Anonymous functions and closures

### Anonymous functions

`fn(params) -> R { body }` written as an expression is an anonymous function. Passed to a parameter of function type, it may leave out its own types. They come from that parameter: `fn(m) { ... }`. The types come from the target, never from the body. A named function always writes its full signature. The signature is the contract its callers, the library file and the header depend on. An anonymous function that uses nothing from the enclosing function is an ordinary function value. It may be stored, returned and passed anywhere a named function may.

### Closures

An anonymous function that uses a local variable or parameter of the enclosing function is a closure. It captures that variable by reference, and may read and change it.

```anti
fn censor(text: str, words: Regex) -> (str, int)
{
	let hits = 0;
	let clean = text.replace(words, fn(m: Match) -> str {
		hits += 1;
		return "*".repeat(m.all.char_count());
	});
	return (clean, hits);
}
```

- A closure never outlives the variables it captures. It may only be passed as an argument to a parameter that does not keep it. It may also be held in a local of the same function that is used the same way. It is refused in a field, a return value, a global, and any parameter marked `keep`.
- The captured variables stay in the enclosing function's frame, and the closure reaches them through a context pointer. Creating a closure allocates nothing.
- A parameter of function type does not keep its argument unless it is marked `keep`: `fn on_click(keep f: fn(Event))`. A `keep` parameter, a field and a global of function type accept named functions and non-capturing anonymous ones alone. An `own` field and a `keep own` parameter accept a snapshot as well, as [Snapshots](#snapshots) says.
- The checker enforces `keep`. A function that stores a parameter not marked `keep` is a compile error.
- A closure may change what it captures only when it is called from one thread at a time. Each thread that runs a function makes its own closures over its own frame. Closures made on different threads share nothing.
- A parameter of function type that a function passes on to `parallel` or `dispatch` may be called from more than one thread at once. It is marked `concurrent`, and the checker requires the mark. A closure passed to a `concurrent` parameter may read what it captures. It may change a captured variable only when that variable's type is concurrent, as [Concurrent classes](#concurrent-classes) defines. Otherwise the write is refused with the variable's name. The creating thread waits in `parallel` until every worker finishes, so the reads cannot race a write.
- `concurrent` is a permission the function reserves, not a promise: it may use threads, not that it does. Removing the mark later never breaks a caller. Adding it later does, since a closure that writes a captured variable stops compiling.
- A worker may take a function value as a parameter, and a closure only through a `concurrent` parameter.
- A closure may fail when the parameter's type says so: `fn(Match) -> str may fail`.

### Snapshots

`snapshot fn(...) { }` is a closure that takes the values it uses when it is made. It never depends on the caller's variables again.

```anti
let name = "report";
on_click(b, snapshot fn(e) { save(name); });
name = "draft";                      // the closure still saves "report"
```

- The snapshot is read-only. A snapshot closure that writes a captured value is refused. If it could write, two threads calling the same closure would write the same snapshot at once.
- A snapshot may hold only values that copy fully. Those are numbers, `bool`, `char`, structs of those, and `str`, whose bytes are copied into it. Capturing a pointer or a slice is refused. The copy would hold an address, not what it points at.
- A snapshot closure is accepted at an `own` field, at a `keep own` parameter and at a `concurrent` parameter, where a closure by reference is refused. At a plain `keep` parameter it is refused, and the message names `keep own` as the fix. Each refusal of a closure by reference names `snapshot fn` as the fix.
- Where the snapshot lives follows from where the closure goes, not from the word. At a `concurrent` parameter it sits in the caller's frame, since the caller waits. Nothing is allocated. At an `own` field or a `keep own` parameter it must outlive the frame, so it goes on the heap. Its owner frees it, as [Calling convention](#calling-convention) says.

```anti
class Button
{
	own handler: fn(Event) = ignore,

	pub fn on_click(self, keep own f: fn(Event))
	{
		self.handler = f;
	}
}
```
- The word is `snapshot` and not `copy`. The values are taken at that moment and stay as they were. A copy suggests something its holder may change.

### Calling convention

`own` chooses the representation of a function value that is kept.

- A plain function value, `fn(...)`, stays one C function pointer. It holds a named function or an anonymous one that captures nothing. A field, a global and a `keep` parameter hold it. So do the parameters of an `extern fn`, bindings and C callbacks such as raylib's, which are unchanged.
- An owned function value, `own fn(...)`, is two words, the code and the address of a snapshot, and frees the snapshot with its owner. It is written where the value is kept: `own handler: fn(Event)` for a field, and `keep own f: fn(Event)` for a parameter that keeps and owns what it is given. A named function or a non-capturing one fits there too, with no snapshot, and holds `none` in the second word.
- An owned function value is freed with its owner, is not copied by `=`, and is copied by `dup`, which copies its snapshot.
- A snapshot is one block that starts with its size in bytes and holds the captured values and the bytes of any captured `str`. Freeing and copying it need nothing specific to the closure. `anti_rt_snapshot_free` gives the block back, and one runtime function copies it for `dup` by that size. Freeing `none` does nothing.
- A parameter that does not keep its argument is passed as two words, the code and a context pointer. The context is `none` for a function that captures nothing. An owned value passes there too, and lends its snapshot for the call.
- The header writes a parameter of two words the C way, as a callback and a `void *` context. It writes an `own fn` field as a struct of the code pointer and the snapshot pointer, and declares `anti_rt_snapshot_free`.
- An `extern fn` takes plain C function pointers only, so a closure cannot reach C. Passing an `own fn` where a plain C function pointer is expected is refused.

## Concurrent classes

Any value may be read from more than one thread at once. Changing a value from more than one thread at once needs a type built for it, a thread-safe type. The built-in thread-safe types are `Mutex`, `chan T` and the atomics. A class becomes thread-safe in one of two ways, and the compiler checks both.

| Declaration | Who handles concurrency | What the compiler checks |
|---|---|---|
| `synchronized class` | Anti: one lock around every public function | everything |
| `concurrent class` | the programmer: own locks, `guarded by`, atomics | every field is guarded, atomic or fixed |
| `concurrent class ... unchecked("reason")` | the programmer, fully | nothing, by explicit choice |

The rules for closures and workers use this term. A closure passed to a `concurrent` parameter may change a captured variable of a thread-safe type.

### Synchronized classes

`synchronized class` gives each object a hidden lock, which the program never declares. Every public function of the class runs under it. The lock is taken when the function starts, and released on every exit, `return` and `fail` included. A public function that calls another on the same object does not wait for itself. A private function runs inside the lock of the public function that called it.

```anti
pub synchronized class PeopleList
{
	struct Node
	{
		person: Person,
		next: ?*Node,
	}

	head: ?*Node = none,
	count: int = 0,

	pub fn add(self, p: Person)
	{
		self.head = alloc Node { person: p, next: self.head };
		self.count += 1;
	}

	pub fn remove(self, test: fn(Person) -> bool) -> int
	{
		...
	}
}
```

- A field of a synchronized class is reached only by the functions of the class. `construct` and `destruct` are exempt, since no other thread can see the object then.
- Every operation on the object waits for every other. That is always correct and fast enough for most objects. A structure whose threads wait on each other more than a profiler allows becomes a concurrent class.
- `anti doc` and the C header mark every public function of a synchronized class as running under the object's lock. The documentation says it at the function.
- `sync obj { }` takes the hidden lock of a synchronized object for a whole block, for a sequence that no method covers.

### Concurrent classes

`concurrent class` leaves the locking to the programmer, for finer locks that let unrelated work run at once. The compiler checks that every field is one of three kinds. A field of none of them is a compile error that names the three:

- Guarded: `field: T guarded by lock`, reached only inside `sync` on that lock. The lock is a `Mutex` field of the same object, or of an enclosing object named by its class, `guarded by PeopleList.lock`.
- Atomic: reached only through atomic operations.
- Fixed: written only in `construct`, and read-only after that.

```anti
pub concurrent class PeopleList
{
	struct Node
	{
		lock: Mutex,
		person: Person guarded by lock,
		next: ?*Node guarded by PeopleList.lock,
	}

	lock: Mutex,
	head: ?*Node = none guarded by lock,

	pub fn add(self, p: Person)
	{
		let n = alloc Node { person: p };
		sync self.lock {
			n.next = self.head;
			self.head = n;
		}
	}
}
```

`add` holds the list's lock only for the two pointer moves. A change to one person holds only that node's lock, so both run at once. The check proves every field is guarded. It cannot prove the design right. The order in which locks are taken, and whether a sequence of calls is safe, stay the programmer's responsibility.

A field that is none of the three fails the safety check `unguarded-field`. `unchecked` overrules it where the checker cannot follow, with the rules of [Errors, warnings and checks](#errors-warnings-and-checks). After a field's type it covers that field alone, so every other field stays checked. A lock-free structure marks the fields it swaps with `compare_swap`. In the class header it covers the whole class. That suits a class that wraps a C library doing its own locking.

```anti
pub concurrent class Queue
{
	head: ?*Node unchecked(unguarded-field, "swapped with compare_swap, see push and pop"),
	tail: ?*Node unchecked(unguarded-field, "swapped with compare_swap, see push and pop"),
	lock: Mutex,
	count: int guarded by lock,
}
```

### Rules for both

- A public function of a thread-safe class never gives out a pointer or a slice into the object's fields. It does not return one, and it does not pass one to a parameter of function type. It returns and passes copies, since a caller holding such a pointer would reach the data without the lock.
- A closure passed to a function of a synchronized class runs inside the lock. It runs on one thread at a time, so it is a plain parameter.
- `atomic` also marks a local: `let hits: atomic int = 0;`. An atomic local is thread-safe.
- A worker may take a pointer to a thread-safe object. It is the one exception to the rule that a worker's parameters hold no pointer.

### Locks

`Mutex` is one word of the program's own memory, and the operating system keeps nothing for it until a thread has to wait: a `futex` word on Linux, `os_unfair_lock` on macOS, `SRWLOCK` on Windows. A lock in every node of a large structure therefore costs memory alone, four or eight bytes each, and `size_of(Mutex)` is documented per target. Where even that is too much, a fixed set of locks shared by address, lock striping, keeps the count constant.

A `Mutex` cannot be copied or assigned. A class that holds one follows the ownership rules for values that cannot be copied.

### Operations that decide for themselves

Locking every function makes each call safe, never a sequence of calls. Between `list.size()` and `list.remove(0)` another thread can empty the list. A thread-safe class therefore takes criteria, not positions: `list.remove(fn(p) { return p.age >= 65; })` finds and removes under one lock, where an index returned by one call can be stale by the next. The compiler cannot tell a position from a count in an `int`, so this is a design rule for the standard library and for every thread-safe class, and the guide teaches it.

### Deadlock

Two threads that take two locks in opposite orders wait for each other forever. The compiler cannot see that in general. A dev build records the order in which each thread takes locks. It reports two orders that conflict, with both call sites, before they hang in production.

## Nested types

A class may declare a `struct`, `enum` or `class` inside its body. The nested type is private to that class.

- Only the enclosing class can name the type. Code outside can neither create one nor receive one. A public signature of the enclosing class that names it is a compile error.
- Its full name is the enclosing class's name followed by its own, `PeopleList.Node`, which the symbols and the C header use.
- A nested type suits what belongs to one class alone, such as the node of a list. A type that users work with directly stays at module level.
- In a thread-safe class, a nested type can never leave the class, so a node can never leak by construction.

## Errors, warnings and checks

The compiler reports four kinds of problem, and each is handled in its own way. The guide explains them together, with how dev and release builds treat each.

| Kind | Example | Dev build | Release build | Overruled by |
|---|---|---|---|---|
| Error | a type mismatch, an unhandled failing call | stops | stops | nothing |
| Warning | a shadowed name | prints and carries on | stops | `allow(name, "reason")` |
| Safety check | an unguarded field in a concurrent class | stops | stops | `unchecked(name, "reason")` |
| Run-time check | an index out of bounds, an overflow | traps with file and line | not compiled | `--checks` puts them in a release build |

- An error is a program the compiler cannot accept. Nothing silences it.
- A warning is code that looks suspicious and is often fine. A dev build prints it and carries on, so work in progress is not blocked. A release build accepts none, so every warning there is fixed or allowed.
- A safety check guards against a class of bug the compiler can prove absent, such as a data race. It stops every build, dev included, so the bug is found when it is written. Where the programmer knows better than the checker, `unchecked` overrules it, with a reason, at the smallest place it applies.
- A run-time check tests at run time what the compiler cannot know, such as an index. Dev builds carry them, and a failure stops the program with the file, the line and the values.

`allow` and `unchecked` look alike, a word and a reason where they apply. They mean different things, so each has its own word.

### Warnings

- Every warning has a stable name, printed at the end of its message: `` `e` shadows the outer `e` [shadowed-catch] ``. The names are listed in one place in the documentation, each with its meaning and its fix.
- A release build accepts no warning. In a release build every warning is an error, always, with no option to turn that off. A warning is fixed, or silenced with `allow`. A dev build prints warnings and carries on. `antic --warnings-as-errors` gives the release behaviour in a dev build, and `anti check` uses it.
- `allow(name, "reason")` silences one warning, and the reason is required. It applies to what it belongs to, at three levels:
  - a statement, when it stands directly before that statement;
  - a function, class or struct, when it stands in that declaration's header, last, after `-> R` and `may fail` or after `inherits`, before the brace;
  - the whole file, when it stands at the top of the module and ends with `;`.
- Silencing more than one warning takes one `allow` clause for each, with its own reason. A long header wraps onto indented lines before the brace.
- `allow` is not part of a signature. The library file, the C header and `anti doc` leave it out. Two functions that differ only in their `allow` have the same type.
- An `allow` that silences nothing is a warning, `[unused-allow]`, so a stale one does not outlive the code it was for.
- Only warnings can be named. An error is never silenced, and `allow` naming one is refused.

```anti
fn parse_all(lines: []str) may fail
	allow(shadowed-catch, "handlers reuse e on purpose")
{
	...
}
```

### Safety checks

`unchecked` works as `allow` does, for safety checks instead of warnings.

- Every safety check has a stable name, printed at the end of its message: `` `head` is not guarded, atomic or fixed [unguarded-field] ``. The names are listed in the documentation with the warnings, each with its meaning and its fix. The first two are `unguarded-field` and `exponential-pattern`.
- `unchecked(name, "reason")` overrules one check, and both the name and the reason are required. It applies at the levels of `allow`. It stands before a statement, last in a declaration's header, or at the top of the file ending with `;`. It also applies after a field's type, for that field alone.
- An `unchecked` that overrules nothing is a warning, `[unused-unchecked]`.
- Only safety checks can be named. An error is never overruled, and `unchecked` naming one is refused.
- `unchecked` is not part of a signature, as `allow` is not.

## Generics

Generics add type parameters to the language. Every collection of [Collections](#collections) is generic.

### Syntax

Type parameters and type arguments stand between `<` and `>`, as in C, C++ and Rust.

```anti
fn max<T: lt>(a: T, b: T) -> T { ... }
struct Pair<A, B> { first: A, second: B, }
class List<T> { ... }

let p: Pair<int, str> = Pair { first: 1, second: "one" };
let list = List<Person>.new();
let m = max(3, 7);
```

- In a type position, `<` after a type name always opens type arguments. Type positions are after `:`, after `->`, after `alloc`, and in a field or parameter declaration.
- In an expression, `<` after a name opens type arguments under two conditions. What follows parses as a list of types closed by `>`, and the token after the `>` is `(`, `.` or `{`. Otherwise `<` is a comparison. This is the rule of C#. `List<int>.new()` and `max<int>(a, b)` are generic, and `a < b > c` stays two comparisons.
- That rule can read a generic call where the name is not generic, as in `f(g < a, b > (c))` with an ordinary `g`. The compiler then refuses it and names the fix: parentheses around the comparison.
- `>>` closes two lists of type arguments: `List<List<int>>`.
- Type arguments are inferred from the arguments of a call where they can be. So `max(3, 7)` is `max<int>`. They are written out where nothing gives them, as in `List<Person>.new()`. Inference reads the arguments, never the body and never later uses of the result.

### Compilation

Every use of a generic with concrete arguments gets its own compiled copy, as in C++ and Rust. `List<int>` holds plain `int` values and calls its functions directly, with no boxing and no indirection. Generic code costs what hand-written code costs.

- Two uses with the same arguments are the same type. `List<int>` in one module and in another are one type, passed freely between them, and compiled once.
- The whole-program pass merges copies whose code is identical. `List<*Person>` and `List<*Order>` compile to the same code, since both hold pointers, and one copy serves both. This removes most of the size that one copy per type would cost.
- In a dev build, where modules compile apart, each copy is made in the module that uses it. It is cached by the generic's identity and its arguments. The link keeps one copy of each.
- `size_of(T)` inside a generic is the size of the argument in that copy.

### Constraints

A generic states what it needs from each type parameter. The compiler checks the body against those needs where the generic is written. It checks every use against them where it is used. An error therefore lands in the code that made it. That is either a body that uses more than it declared, or a call with a type that lacks something. The message names the missing hook or interface.

```anti
fn max<T: lt>(a: T, b: T) -> T { ... }
fn sum<T: add>(items: []T) -> T { ... }
fn count<C: iter>(c: C) -> int { ... }
fn save<T: Serializable>(items: []*T) { ... }
fn find<K: eq + hash, V>(m: Map<K, V>, k: K) -> V { ... }
```

- A hook from the table of language hooks is a constraint: `lt`, `add`, `iter` and the rest. A type meets it when it has that hook. The built-in types have their operators as hooks, so `int` meets `lt`, and a struct or class meets it with `operator fn lt`.
- An interface is a constraint: `T: Serializable` requires a class that declares it implements `Serializable`.
- Constraints combine with `+`.
- `constraint Ordered = eq + lt;` names a set for reuse, used as `T: Ordered`. `anti.lang` ships `constraint Number = add + sub + mul + div + neg + lt;` for numeric code. A user's own number type joins it by having those hooks, so the set is open.
- A type parameter without constraints can be stored, copied, moved and passed on, and measured with `size_of`. That is what a container needs, so containers usually take unconstrained parameters.
- A constraint violation at a call gives: `` `Circle` has no `lt`, which `max` needs for `T` ``.

Considered and left for later: requiring an ordinary method of a type, such as `T: fn area(self) -> f32`. Nothing designed so far needs it. A type that must provide a method can be a class implementing an interface. It can be added when real code needs it, and adding it breaks nothing.

### What can be generic

- Functions, structs, classes, variants and interfaces take type parameters.

```anti
variant Result<T, E> { Ok { value: T }, Err { error: E }, }
abstract class Iterable<T> { ... }
```

- A parameter may be an integer constant instead of a type, written `N: int`. Its argument is a constant the compiler knows. It serves sizes that belong to the type, such as the length of a fixed array: `struct Ring<T, N: int> { items: [N]T, ... }`, used as `Ring<Sample, 1024>`, or `Matrix<f32, 4, 4>`. Only integers are allowed.
- A method may take type parameters of its own: `pub fn map<U>(self, f: fn(T) -> U) -> List<U>`. Such a method is not in the class's table, since every `U` makes a separate copy and no table holds them all. It is called directly, and it cannot be `abstract` or replaced with `concrete fn`. The compiler refuses the combination.
- A nested type of a generic class sees the class's type parameters: `class List<T> { struct Node { value: T, next: ?*Node, } }`.
- A generic class may be `synchronized` or `concurrent`, and its rules apply to every copy.
- A generic `worker fn` is checked against the worker rules in every copy, with the concrete types.

### Other features with generics

- Language hooks work in generic types. `class List<T>` may declare `operator fn iter`, and `for x in list` then works for every `List<T>`.
- A generic function may be `may fail`, with the usual two channels.
- Function types, closures and snapshots may appear as type arguments: `List<fn(int) -> int>`. The rules of `keep` and `concurrent` apply as for any value of a function type.
- Each copy of a generic class has its own descriptor, named with its arguments: `List<Person>`. Reflection and `type_name` give that name.

### Libraries and C

- A library file stores each generic body as its checked syntax tree. Its names are resolved, its types are checked against its constraints, and its type parameters stay open. Its constraints stand in the public interface. A use with concrete arguments lowers that tree with those arguments into ordinary IR. A copy from the program's own source is made the same way. Code that is not generic stays IR. The IR gives every value a concrete type. An operation on `T` lowers to different instructions per type, so no single IR stands for an open body. The tree has a section of its own in the library file, written byte for byte the same on every host. A library ships generics like any other code, with no source and no templates in headers.
- The C header cannot show an open generic, since C has none. A library offers a copy to C by naming it: `export type PersonList = List<Person>;`. The header then writes it as any exported class, `struct anti_PersonList` with its functions.
- `type Name = Generic<Args>;` without `export` names a copy for use in Anti alone.
- An `export fn` has concrete types throughout. A generic `export fn` is refused, and the message suggests a named copy.
- `anti doc` shows generics with their parameters and constraints, and a named copy links to its generic.


## Optional values

`?T` is an optional value of any type: a `T`, or `none`. It follows every rule `?*T` already has.

```anti
fn find_user(id: int) -> ?User { ... }

let age = ages.get("Ann") ?? 0;
if let a = ages.get("Ann") { ... }
let first = queue.first() else { return; };
```

- A `?T` compares with `none`, narrows after a test, and works with `if let`, `let ... else` and `??`. A `?T` is used as a `T` only after a test proves it holds one.
- `?T` of a value type is the value and one flag byte, padded to the type's alignment. `?*T` stays one pointer, with `none` as address zero, as now.
- The match result of [The match](#the-match) is a `?Match` under this rule.
- `none` means that nothing is there, and that is normal. A failure is `may fail`. `map.get(key)` returns `?V`. `text.parse_int(s)` stays `may fail`, since bad input is an error and not an absence.
- The C header writes `?T` as a struct of the value and a `bool`.

## Ownership at a call

- `own` before a parameter takes ownership of the argument: `pub fn push(self, own item: T)`. Passing a local moves it, and naming the local again is refused: `` `c` was moved into `shapes` by `push` ``. A literal or a call result passed there needs nothing. This extends the `own` parameter of errors to every type.
- For a type that owns no memory, a move is a copy of its bytes. For one that does, the move is what keeps one owner.
- A struct or a tuple is owning when any of its parts owns something, transitively: a class value, a collection, an `own fn`, a `?T` of an owning type, or an owning struct or tuple. It follows the value rules of a class value. It is torn down part by part at the end of its block on every exit, `=` refuses it and `dup` copies it. It moves when returned or passed to an `own` parameter, naming it after a move is refused, and it is torn down with its owner as a field, an array element, an element inside a collection or inside a `?T`. A struct or a tuple that owns nothing keeps the rules of plain C data: `=` copies it and nothing is torn down. The C header writes an owning struct with its layout unchanged and marks it as owning in a comment. Structs still take no `own` field.

## Lending

A collection lends an element to a function instead of handing out a pointer to it.

```anti
list.read(3, fn(p) { io.println(p.name); });
list.modify(3, fn(p) { p.age += 1; });
```

- `lent` before a pointer parameter says the pointer is valid only during the call: `fn(lent *T)`. The function may read and change through it, and pass it on to another `lent` parameter. It may not store it, return it, capture it in a closure that outlives the call, or pass it to a `keep` or `own` place. The checker enforces this as it enforces `keep`.
- `lent` before a slice parameter, `fn(lent []T)`, follows the same rules. The slice is valid only for the call, and is never stored, returned, captured beyond the call or passed to a `keep` or `own` place.
- A pointer or a slice derived from a lent one is lent as well, under the same rules: `s.ptr`, `&p.field`, `&s[i]` and a part of a slice. The one exit is an argument of an `extern fn` call, since C cannot be checked. Whether C keeps it is C's contract, as for every pointer given to C, and the guide says so.
- A lending function runs while the collection holds the element in place, so the pointer never outlives the element.
- The word is `lent`, chosen over `borrowed` and `scoped`. `borrowed` would suggest Rust's whole system of references.

## Walking a collection

```anti
for p in people { }      // p is a Person: a copy of each element, read-only
for p in &people { }     // p is a lent *Person: the element itself, for this turn
```

- `for x in c` gives a copy of each element. The loop variable is read-only, so a change that would only reach the copy is a compile error: `` `p` is a copy of each element of `people`. Walk with `&people` to change the elements ``.
- `for x in &c` gives each element as a `lent` pointer, valid for one turn of the loop. The element may be changed in place through it.
- The same two forms hold for slices, with the same read-only rule for the copy form.
- The iterator decides what walking with `&` gives. `for x in &c` uses the iterator of `c`, and the `value` of that iterator returns exactly what the loop binds: a `lent *T` for a list, and a `(K, lent *V)` for a map, whose keys never change in place. A `SortedMap` whose keys and values stand in two arrays returns `(K, lent *V)` as well.
- A tuple may hold a lent pointer only as the value of such an iterator, bound by its loop. It cannot be stored, returned or passed on as a whole.
- The loop owns every part of the iterator's value that it receives by value, and tears each one down at the end of its turn, as a `let` binding is torn down at the end of its block, including on `break`, `continue`, `return` and `fail` out of the body. The lent parts are borrowed and never torn down. A part whose type owns nothing, such as a plain `str`, costs nothing. The same holds for `for x in c`, where the copy is torn down each turn.
- `for (k, v) in m` takes apart the tuple of each element, over any iterator whose value is a tuple. The parts of a pattern are what the value type says, in any position: `(K, int, lent *V)` gives a copy, a copy and a lent pointer. `for (k, v) in &m` over a map therefore gives `k` as a copy and `v` as a lent pointer.
- A collection must not change its size while a loop walks it. Every collection keeps a count of its changes, and its iterator remembers the count. A dev build traps when they differ, naming the collection and both places: `` `people` was changed while `for` walked it ``. A release build carries no check. Removing while walking is `remove_all(test)`, which is built for it.

## Hashing and order

- `operator fn hash(self) -> u64` joins the table of language hooks. The built-in types have it. A struct or class gets a default that hashes its fields in order, and may replace it.
- Two values that are equal by `eq` have the same `hash`. The default keeps that rule, and a replacement must.
- A struct or class gets a default `eq` as well and no default `lt`, since what order means is the type's own choice. A type used where `Ordered` is needed declares `operator fn lt`, and the refusal says so.
- `anti.lang` ships `constraint Ordered = eq + lt;`.
- `str` has the hooks `eq`, `lt` and `hash`, so it meets `eq`, `Ordered` and `hash`. `==` compares the text, `<` compares it byte by byte, which for UTF-8 is the order of the code points, and `hash` hashes the bytes.
- A hashing collection mixes its hash with a random seed chosen when the program starts. Keys chosen by an attacker then cannot all land in one bucket. The seed never decides the order a program sees.

## Direct imports

`import anti.collection.map.{Map, HashMap};` makes the listed names of a module visible in the importing file without the module's name. Code then writes `Map<str, int>` rather than `map.Map<str, int>`.

- The listed names must be public items of that module. A listed name that clashes with a name already visible in the file is refused, naming both.
- The module itself stays reachable by its name, as with a plain `import`.
- `anti fmt` keeps the list sorted.
- It serves every module: `import anti.regex.{Regex};`, `import anti.mem.{Shared, ArenaAllocator};`.

## Collections

### Modules

The collections are split into modules by family, one file each. Each can then be built and read on its own:

| Module | Items |
|---|---|
| `anti.collection` | `Iterable<T>`, `Iterator<T>`, and the parts every collection shares |
| `anti.collection.list` | `List<T>` |
| `anti.collection.deque` | `Deque<T>` |
| `anti.collection.ring` | `Ring<T, N>` |
| `anti.collection.grid` | `Grid<T>` |
| `anti.collection.map` | `Map<K, V>`, `HashMap<K, V>` |
| `anti.collection.set` | `Set<T>`, `HashSet<T>`, `BitSet` |
| `anti.collection.sorted` | `SortedMap<K, V>`, `SortedSet<T>` |
| `anti.collection.pool` | `Pool<T>`, `Handle<T>` |
| `anti.collection.tree` | `Tree<T>` |
| `anti.collection.queue` | `PriorityQueue<T>` |
| `anti.collection.sync` | `SyncList<T>`, `SyncMap<K, V>`, `SyncSet<T>`, `SyncPool<T>` |
| `anti.collection.concurrent` | `ConcurrentMap<K, V>`, `SpscRing<T, N>` |

### Principles

- A collection is a class used as a value. It owns its storage and is freed at the end of its block. It moves on return, is refused by `=` and is copied by `dup`.
- Elements are stored by value, in the collection's own memory. A collection of pointers, such as `List<*Circle>`, owns the pointers and not what they point at.
- An element that owns something, a class value or an owning struct or tuple, is torn down when the collection removes it or ends. A copy of such an element copies what it owns.
- No pointer to an element ever leaves a collection, except through a `lent` parameter. Reading gives a copy, as a `?T` where nothing may be there. Changing hands a new value in by value, or runs inside the collection.
- An element is addressed by key in a map, and by position in a list while one thread walks it. It is addressed by criteria in any collection, and by handle in a `Pool` or a `Tree`.
- Criteria come in three explicit forms and never an ambiguous one: `_all` acts on every match and gives the count, `_first` on the first match in the collection's order, and `_one` on the one match. `_one` fails when nothing matches, and fails when more than one does, with the count. It is how criteria stand in for an address.
- Every collection takes an optional allocator when it is made, `List<Person>.new(from: &arena)`, with the C library as the default. Everything it allocates comes from that allocator and goes back to it.
- Every collection works with `for` through the `iter` hook.
- A collection that grows takes an initial capacity when it is made, `List<Person>.new(capacity: 1000)`, and allocates once for it. Without one it starts small and doubles as it grows. `reserve(n)` makes room for `n` more elements in one allocation, and `shrink()` gives unused room back. `List`, `Deque`, the maps and the sets have all three.
- A collection has the `eq` hook when its elements have it, comparing element by element, so `a == b` works for two lists. `to_text` writes it as `[1, 2, 3]`, or `{"Ann": 41}` for a map. `serialize` and `deserialize` write and read it as a JSON array, or as an object for a map with text keys and as an array of pairs for other maps.

### Operations every collection has

| Operation | Meaning |
|---|---|
| `T.new(from: &alloc)` | create, with an optional allocator |
| `c.count`, `c.is_empty()` | size |
| `for x in c` | walk |
| `c.find_all(test)`, `find_first(test)`, `find_one(test)` | copies of matches |
| `c.update_all(test, change)`, `update_first`, `update_one` | change matches inside the collection |
| `c.remove_all(test)`, `remove_first`, `remove_one` | remove matches |
| `c.clear()` | remove everything |

### Sequences

**`List<T>`**, one contiguous buffer.

| Operation | Meaning |
|---|---|
| `push(own x)`, `pop() -> ?T` | at the end |
| `insert(i, own x)`, `remove_at(i) -> T` | at a position |
| `l[i]`, `l[i] = x` | a copy, or a replacement. Out of range traps in a dev build, as an array does |
| `get(i) -> ?T`, `first()`, `last()` | `none` when out of range or empty |
| `read(i, f)`, `modify(i, f)` | lend one element |
| `lend_slice(f)` | lend the whole buffer as one `lent` slice, for C and simd |
| `sort()`, `sort_by(key)` | a stable sort. `sort` needs `T: Ordered` |
| `reverse()`, `map<U>(f) -> List<U>`, `filter(test) -> List<T>` | reshape. `map` and `filter` make a new list |

**`Deque<T>`**, a ring buffer that grows: `push_front`, `push_back`, `pop_front`, `pop_back`, `front` and `back`, the last four giving `?T`.

**`Ring<T, N: int>`**, a ring buffer of fixed capacity `N` that allocates nothing after it is made, for real-time code such as an audio callback: `push(own x) -> bool`, which gives `false` when full, `pop() -> ?T`, `peek() -> ?T`, `is_full()`.

**`Grid<T>`**, a two-dimensional array: `Grid<T>.new(width, height, fill)`, `g[x, y]` with bounds checks in a dev build, `get(x, y) -> ?T`, `width`, `height`, and `lend_slice(f)`. It is stored as one buffer, row by row.

### Maps

| Type | Order when walked | Lookup | Key |
|---|---|---|---|
| `Map<K, V>` | insertion order | constant time on average | `K: eq + hash` |
| `SortedMap<K, V>` | key order | logarithmic | `K: Ordered` |
| `HashMap<K, V>` | none, and different on every walk | constant time on average | `K: eq + hash` |

| Operation | Meaning |
|---|---|
| `get(k) -> ?V`, `contains(k)` | look up |
| `set(k, own v)` | insert or replace |
| `add(k, own v) -> bool` | insert only a new key |
| `remove(k) -> ?V` | remove and return |
| `update(k, change) -> bool` | change one value inside the map |
| `read(k, f)`, `modify(k, f)` | lend a value |
| `for (k, v) in m`, `keys()`, `values()` | walk |

`SortedMap` adds `range(lo, hi)`, from `lo` up to and not including `hi`, `floor(k)`, `ceiling(k)`, `first()`, `last()`, `pop_first()` and `pop_last()`.

- `Map` keeps its entries in one array in insertion order, with a hash table of positions into it. Its walking order is the same on every run and every machine.
- `SortedMap` is a B-tree. A node holds many keys in one block, so a lookup touches few cache lines.
- `HashMap` keeps its entries in the hash table itself, with no order to maintain. It suits large maps with frequent removal. Its walking order is random on purpose, on every walk, so no program comes to depend on it. `serialize` writes a `HashMap` sorted by key where the key type has `lt`, so saved files stay stable.

### Sets

`Set<T>`, `SortedSet<T>` and `HashSet<T>` follow the three maps: the same orders and the same constraints on `T`. Each has `add(own x) -> bool`, `remove(x) -> bool`, `contains(x)`, `union(o)`, `intersect(o)` and `minus(o)`, which make new sets, and `is_subset(o)`. `SortedSet` adds `range`, `floor`, `ceiling`, `first` and `last`. `union` stays a keyword, and it names a function after `fn` and a member after `.`, as `alloc` and `free` do, so a set declares and calls `union(o)`.

**`BitSet`**, a set of small integers, one bit per value: `add(n)`, `remove(n)`, `contains(n)`, `count`, the set operations, and `for n in b` in increasing order.

### Stable elements

**`Pool<T>`** keeps its elements at fixed addresses. It grows by adding blocks, each twice the size of the one before. Element `i` is then found with a few bit operations. Removing an element frees its slot for reuse and moves nothing.

| Operation | Meaning |
|---|---|
| `add(own x) -> Handle<T>` | insert |
| `get(h) -> ?T`, `contains(h)` | `none` for a handle whose element is gone |
| `set(h, own x) -> bool`, `remove(h) -> ?T` | `false` or `none` for a stale handle |
| `read(h, f)`, `modify(h, f)` | lend an element |
| `get_versioned(h) -> ?(T, int)`, `set(h, own x, if_version: v) may fail` | compare and set |

- A handle is a slot index and a generation. Removing an element increments its slot's generation, so an old handle stops matching even after the slot is reused. A handle never finds the wrong element: at worst it finds nothing.
- A handle may be copied, stored and passed between threads.
- Versioned `set` is on `Pool` alone among these, since only its elements carry a version already. The thread-safe collections gain it too.

**`Tree<T>`** holds a hierarchy, such as a scene graph, a menu or a document, on handles as a `Pool` does: `add_root(own x)`, `add_child(parent, own x)`, `parent(h) -> ?Handle<T>`, `children(h)`, `depth_first(h)`, `breadth_first(h)`, `remove(h)`, which removes the whole subtree, and the lending and reading forms of `Pool`.

### Ordered retrieval

**`PriorityQueue<T: Ordered>`**, a binary heap in one array: `push(own x)`, `pop() -> ?T` and `peek() -> ?T`, which give the smallest element first. It serves schedulers, event queues, timers and path finding.

### Left out, with the reason

- A stack: `List` is one, with `push` and `pop`.
- A map with more than one value per key: `Map<K, List<V>>` expresses it.
- A map in both directions: two maps.
- A cache that drops the least recently used entry: a policy over `Map` and `Deque`, for a later `anti.cache`.
- Tries, interval trees and graphs: specialised, for libraries of their own.
- A list that keeps its first elements inline: an optimisation, and `Ring` covers the real-time case.
- A second balanced search tree: `SortedMap` is one, and a B-tree outperforms a binary tree on modern processors.

## Thread-safe collections

Collections shared between threads follow [Concurrent classes](#concurrent-classes). Each is a thread-safe type, so a closure at a `concurrent` parameter and a worker may change it.

| Type | Kind | For |
|---|---|---|
| `SyncList<T>` | `synchronized` | a list more than one thread adds to and searches |
| `SyncMap<K, V>` | `synchronized` | a shared lookup table with modest traffic |
| `ConcurrentMap<K, V>` | `concurrent` | a busy shared map, split into parts locked apart, so threads using different keys do not wait for each other |
| `SyncSet<T>` | `synchronized` | a shared membership set |
| `SyncPool<T>` | `synchronized` | objects shared between threads by handle |
| `SpscRing<T, N: int>` | `concurrent`, lock-free | one producing thread and one consuming thread, with no lock |

- They have the operations of the plain collections, less every operation by position: no `l[i]`, no `get(i)`, no `insert(i, x)`. Between threads a position can go stale at any moment, so an element is addressed by criteria, key or handle.
- They have no `lend_slice`, since a slice of the whole buffer would be reached without the lock.
- They all have versioned `set`, so a thread can read, compute, and write back only if nothing changed meanwhile.
- `read`, `modify` and the functions given to `update_*` and `remove_*` run inside the lock. A slow function holds up other threads, and the documentation says so.
- `SpscRing` passes samples or messages between an audio thread and the rest of a program. Neither side ever waits, since it holds only two atomic counters, one per side. It is marked `unchecked` with its reason, because its correctness rests on atomics in a pattern the checker cannot follow. Its tests exercise both sides at full speed on every host.

Left out: a queue between threads, since `chan T` is one. Also left out are synchronized versions of the sorted and hash-ordered maps and sets, and of `PriorityQueue`, `Grid`, `Tree` and `BitSet`. Sharing those is rare, and a `synchronized` class around the plain one covers it.

## Shared ownership

`anti.mem.Shared<T>` gives an object more than one owner, by explicit reference counting.

```anti
let c = Shared<Circle>.new(Circle { r: 2.0 });
let list = List<Shared<Circle>>.new();
list.push(c.share());
```

- `share()` gives another handle and increments the count, visibly. `=` stays refused, so a count never changes silently.
- The object is destroyed, through its `destruct`, when its last handle is freed. The count is atomic, so handles may be shared between threads. `T` must be thread-safe for more than one thread to change the object.
- `read(f)` and `modify(f)` lend the object.
- It costs only where it is used: every other pointer stays a plain address.
- Two shared objects that hold each other are never freed. The documentation says so, and the fix is a plain pointer for one of the links.

## Memory checks

`--memory-checks` makes a build detect memory errors while the program runs: `antic --memory-checks`, and the same option for `anti build`, `anti test` and `anti run`.

- It catches a use after free, a double free, and a read or write outside a heap block. At exit it reports the leaks, with where each block was allocated.
- A pointer into a collection kept across a change of its size is a use after free. This is how such a mistake is found.
- The back end emits the checks of AddressSanitizer around every load and store. The build links the AddressSanitizer runtime of the pinned clang. Its reports carry the names and lines of `-g`.
- Windows on ARM64 has no AddressSanitizer runtime, and the option is refused there with a message that says so.
- The option is off by default in every build, since it makes a program run two to three times slower.

## Messages

- `` `n` may be `none`, check it or use `?*T` ``
- `` `*T` cannot hold `none` ``
- `` index 12 is out of bounds for length 8 `` at run time, with file and line
- `` `2147483647 + 1` overflows `i32` `` at run time, with file and line
- `` `flags` in a two-name `let` must be a `Flags` variable ``
- `` `a + b + c` is not one operation, `Flags` need one ``
- `` `tests` block already declared in `stack` ``
- `` `switch` on `Shape` lacks `Empty` ``
- `` `Shape.Circle.r` is reachable only through `switch` ``
- `` `sync m` inside `sync m` deadlocks ``
- `` `anti.log.Logger` has no provider, needed by `Renderer` ``
- `` `CISOLogger.so` provides `Logger` 1.2 and the program calls `flush`, added in 1.3 ``
- `` `Logger` 1.4 adds a field since 1.2, `CISOLogger.so` cannot load ``
- `` `anti.log.Logger` is provided by both `fancy.so` and `ciso.so`, pick one in `[injections]` ``
- `` `--anti.thread` is not a runtime key, the keys are `threads`, `logger`, `plugins`, `trace` ``
- `` `log` is `inject final` in `Renderer` and cannot be replaced ``
- `` `shaders/basic.glsl` not found for `embed` ``
- `` `undefined` is allowed for a local only ``
- `` no loop or block named `outer` ``
- `` `?.` on `p.count`, which is not a pointer ``
- `` positional argument after named argument ``
- `` `in` takes a range ``
- `` unknown format `{x:q}` ``
- `` `switch target.os` lacks `Windows` ``
- `` `divide` may fail and its error is not handled ``
- `` `try` outside a function that may fail ``
- `` `close` may fail and never does `` as a warning from `anti check`
- `` `fallthrough` in the last arm ``
- `` `fr"` is not a prefix, write `rf"` ``
- `` odd digit count in `x"..."` at column 7 ``
- `` `f16` has no arithmetic, convert with `as f32` ``
- `` pattern `[a-` at column 3: missing terminating ] ``
- `` `e` shadows the outer `e` [shadowed-catch] `` as a warning
- `` `head` is not guarded, atomic or fixed [unguarded-field] ``
- `` `Vec4` lanes must share one type ``
- `` `f32x128` exceeds the vector cap, use an array ``
- `` this program needs a processor with AVX2 (x86-64-v3, 2013 or later) `` at run time
- `` inherits belongs in the class header: class Circle inherits Shape ``
- `` `Circle` has no `lt`, which `max` needs for `T`. A struct or a class has no default order and declares `operator fn lt` ``
- `` `max` uses `+` on `T`, which its constraints do not give. Add `add` to them ``
- `` `g` is not generic. Put the comparison in parentheses ``
- `` `map` takes type parameters, so it cannot be `abstract` or replaced ``
- `` `size` must be a constant for `N` ``
- `` an `export fn` cannot be generic. Export a named copy with `export type` ``
- `` `a` may be none. Test it or use `??` ``
- `` `c` was moved into `shapes` by `push` ``
- `` `push` takes a `Circle`, and `c` is a `*Circle` ``
- `` `p` is lent for the call and cannot be stored ``
- `` `Circle` has no `hash`, which `Map` needs for `K` ``
- `` `find_one` found 3 elements where it needs exactly one ``
- `` `p` is a copy of each element of `people`. Walk with `&people` to change the elements ``
- `` `people` was changed while `for` walked it ``
- `` `--memory-checks` is not available for windows-arm64 ``
- `` `Map` is already visible here, from `anti.collection.map` and from `geo` ``

## Keywords

- Keywords added: `variant`, `tests`, `fixtures`, `provides`, `undo`, `unreachable`, `undefined`, `show`, `embed`, `fail`, `here`, `fallthrough`. `sync`, `chan`, `send`, `recv`, `select` were reserved.
- Contextual words added: `trace` before `class` or `fn`, `inject` and `inject final` before a field, `compatible` in an abstract class body, `in` after a value and before a range, `may fail` after a signature, `simd` before `struct`. Round four adds `snapshot` before an anonymous `fn`, `keep`, `keep own` and `concurrent` before a parameter of function type, `synchronized` and `concurrent` before `class`, `guarded by` and `unchecked` after a field's type, `unchecked` in a class header, `allow` for silencing a warning, and `none` after `catch`.
- Round five adds the keywords `constraint` and `type`, and the contextual word `lent` before a pointer or slice parameter.
- String prefixes added: `rf`, `x` and `re`.
- Labels added: an identifier and `:` before `for`, `while` or a block.
- Tokens added: `?*`, `+% -% *% <<%`, `+| -| *|`, `+%= -%= *%= <<%=`, `+|= -|= *|=`, the two-name `let` form `let (a, b) =`. Round five adds `<` and `>` around type parameters and type arguments, `>>` closing two lists of them, `?` before any type, and `.{` of a direct import.
- Built-in types added: `f16`, `Flags`, `Mutex`, `chan T`, `Regex`, `ByteRegex`.
- Built-ins added: `mul_high`.
