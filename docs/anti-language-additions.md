# Anti language additions

Rules for the features decided after `docs/anti-object-model.md` and outside it. `docs/decisions.md` refers to the additions document and repeats none of it. The book covers what the compiler does for each. The rest is the language's and lives on anti-lang.com.

Settled on 2026-09-20. The small things, wire formats, binary I/O and the SDK were settled on 2026-09-23. Failing functions, tuples, error origins, source locations, the symbols tooling, the CPU levels, the simd structs and the round-three small items were settled on 2026-09-21. The base in the class header was settled on 2026-09-22.

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
- [Messages](#messages)
- [Keywords](#keywords)

## Timing

Before the first public release: nullable pointers, dev-mode checks, debug information, tests and fixtures, the CPU levels and `none`. The base in the class header comes before it as well. Each changes signatures or output that a user would otherwise depend on. The CPU levels change what a release is built for, and `none` is a rename. The class header moves a line of every class with a base.

After the first release, in this order: the wrapping and saturating operators with `Flags`, sum types, locking and channels. Then `may fail` with tuples, error origins and stack traces, then the symbols tooling, then the small things and wire formats. Then injection, hooks and tracing, plugins and runtime configuration, which belong together. Then generics and closures, which `docs/anti-object-model.md` names.

The round-three small items and the simd structs stay where the small things are.

## Nullable pointers

- `*T` never holds `none`. The compiler refuses `none` for it, refuses an uninitialised one, and never asks for a check before use.
- `?*T` may hold `none`. It cannot be dereferenced, called, indexed or passed where `*T` is expected until the program has checked it.
- `none` has type `?*T` for every `T`.
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
- A call to a `may fail` function must be handled with `catch`, `try`, `catch fatal` or a `try` block, which is the existing rule. `try` inside a `may fail` function forwards the error. Inside any other function `try` is refused.
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
- `rt/start.c` checks the processor once at start, against the level of the runtime that was linked. When the machine has less than the program needs, it exits with a message naming the level. The message reads "this program needs a processor with AVX2 (x86-64-v3, 2013 or later)".
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

- `anti.lang.Mutex` is a struct wrapping the platform's mutex, created with `Mutex.new()` and released with `m.destroy()`.
- `sync m { }` locks `m` for the block and unlocks it on every exit, including `return`, `break`, `continue` and the error forms. Nested `sync` on the same mutex is a compile error when both are in one function and a run-time deadlock otherwise, which the chapter states.
- A field written inside a `sync` block and read outside any `sync` is a warning from the whole-program analysis. The warning fires when both sites are in functions a worker reaches.
- `chan T` is a bounded queue of `T` values, `T` pointer-free by the `parallel` rule. `let c = chan int(16);` creates one. `send(c, v)` blocks when full, `recv(c) -> ?T` blocks when empty and returns `none` after `close(c)`. `select` waits on more than one channel and is written like `switch` over the channels. `chan`, `send`, `recv`, `select` and `sync` were reserved from chapter 2.
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
- `std/anti/lang.anti` holds `Error`, `NoneDereference`, `SourceLocation`, `StackTrace` and the hook that `fatal` reads. The compiler declares `Object` and `Job` itself, under `anti.lang`. `Flags` and `Mutex` come into the code with their own steps.

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
- The language's `alloc` and `free` stay bound to libc. `Allocator` is what libraries and classes ask for explicitly, and the containers in `anti.collection` take one. `Object.deserialize` and `f"..."(from)` take one as well.
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
- `f"..."(from)` and `rf"..."(from)` take the memory of their text from `from`, an `anti.mem.Allocator`, and the program gives it back through `from`. `f"..."` without one takes it from the C library.
- Compile-time targets. `target.os`, `target.cpu` and `target.mode` are constants of the enums `Os { Linux, MacOS, Windows }`, `Cpu { X86_64, Arm64 }` and `Mode { Dev, Release }`. A `switch` on one of them is allowed at module level, where its arms hold declarations, and inside functions. It follows the exhaustiveness rule of every `switch`: every value or `else`. Lowering keeps every arm in the IR, tagged with its condition, and the back end keeps the arm for its target and drops the rest before optimisation. Two arms may declare the same name. A library file therefore serves all six targets.
- `anti check --targets all` runs the front end once per target. A program that type-checks on the host is then proven to type-check on all six.
- Script mode. A file whose first line is `#!/usr/bin/env anti` runs with `./tool.anti`. `anti file.anti` compiles the file as a dev build into `<cache>/scripts/<digest>/` of the user cache directory, keyed by the file's digest and the compiler version, and runs the result. A second run is a cache hit. No manifest, and `anti.io`, `anti.text` and the rest of the standard library are available.

## Small items, round three

- `fallthrough;` as the last statement of a `switch` arm continues into the next arm's body without testing its values. Not in the last arm, and not into an arm that binds a variant's fields.
- `rf"..."` and `rf#"..."#`: interpolation without escape processing. `{expr}` and the format specifications work as in `f"..."`, every backslash is literal, `{{` and `}}` write a brace. `fr` is refused with a message naming `rf`.
- `x"00 AB CC"`: a `[]byte` literal of hex pairs with whitespace ignored. An odd digit count or a non-hex character is an error naming the position. No hash delimiters, since the content is hex and spaces.
- The string prefixes are `r`, `b`, `br`, `f`, `rf` and `x`, letters only, one meaning each, listed in one table. No word-form prefixes.
- `f16` is a storage type: sixteen bits in a field, an array or a slice, read as `f32`, written with `as f16`. No arithmetic on it. The conversion is one instruction on ARM64 and on x86_64 at v3, and a runtime routine at `v1` and `v2`.
- `anti check` compiles every constant pattern passed to `regex.compile` with PCRE2. It reports a syntax error with the pattern's file, line and the position PCRE2 names. A pattern built at run time is not checked. The check is skipped with a note when the runtime archive is absent.
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
- The generated module holds one class per group, all at module level. The format's class has `fn construct(self, data: []byte) may fail`, so `alloc NetworkPacket(data) catch e { }` parses a buffer, and a static `parse(r: *binary.Reader)` for a packet inside a stream. `write(self, w: *binary.Writer)` and `size(self) -> int` are generated. Length and count fields are computed on write and are not fields of the class.
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
- `` pattern `[a-` at column 3: missing terminating ] `` from `anti check`
- `` `Vec4` lanes must share one type ``
- `` `f32x128` exceeds the vector cap, use an array ``
- `` this program needs a processor with AVX2 (x86-64-v3, 2013 or later) `` at run time
- `` inherits belongs in the class header: class Circle inherits Shape ``

## Keywords

- Keywords added: `variant`, `tests`, `fixtures`, `provides`, `undo`, `unreachable`, `undefined`, `show`, `embed`, `fail`, `here`, `fallthrough`. `sync`, `chan`, `send`, `recv`, `select` were reserved.
- Contextual words added: `trace` before `class` or `fn`, `inject` and `inject final` before a field, `compatible` in an abstract class body, `in` after a value and before a range, `may fail` after a signature, `simd` before `struct`.
- String prefixes added: `rf` and `x`.
- Labels added: an identifier and `:` before `for`, `while` or a block.
- Tokens added: `?*`, `+% -% *% <<%`, `+| -| *|`, the two-name `let` form `let (a, b) =`.
- Built-in types added: `f16`, `Flags`, `Mutex`, `chan T`.
- Built-ins added: `mul_high`.
