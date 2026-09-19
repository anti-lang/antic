# Anti language additions

Rules for the features decided after `docs/anti-object-model.md` and outside it. `docs/decisions.md` refers to the additions document and repeats none of it. The book covers what the compiler does for each. The rest is the language's and lives on anti-lang.com.

Settled on 2026-09-20. The small things, wire formats, binary I/O and the SDK were settled on 2026-09-23.

Contents:

- [Timing](#timing)
- [Nullable pointers](#nullable-pointers)
- [Dev-mode checks](#dev-mode-checks)
- [Wrapping and saturating operators](#wrapping-and-saturating-operators)
- [Flags](#flags)
- [Tests and fixtures](#tests-and-fixtures)
- [Sum types](#sum-types)
- [Locking and channels](#locking-and-channels)
- [Debug information](#debug-information)
- [Namespaces](#namespaces)
- [Hooks and tracing](#hooks-and-tracing)
- [Injection](#injection)
- [Standard interfaces](#standard-interfaces)
- [Plugins](#plugins)
- [Versions](#versions)
- [Runtime configuration](#runtime-configuration)
- [Small things](#small-things)
- [Wire formats](#wire-formats)
- [Binary I/O](#binary-io)
- [SDK and frameworks](#sdk-and-frameworks)
- [Messages](#messages)
- [Keywords](#keywords)

## Timing

Before the first public release: nullable pointers, dev-mode checks, debug information, tests and fixtures. Each changes signatures or output that a user would otherwise depend on.

After the first release, in this order: the wrapping and saturating operators with `Flags`, sum types, locking and channels, then the small things and wire formats. Then injection, hooks and tracing, plugins and runtime configuration, which belong together. Then generics and closures, which `docs/anti-object-model.md` names.

## Nullable pointers

- `*T` never holds `null`. The compiler refuses `null` for it, refuses an uninitialised one, and never asks for a check before use.
- `?*T` may hold `null`. It cannot be dereferenced, called, indexed or passed where `*T` is expected until the program has checked it.
- `null` has type `?*T` for every `T`.
- Narrowing is per block. Inside `if p != null { }` the name `p` has type `*T`. After `if p == null { return; }` it has type `*T` for the rest of the enclosing block. After the block that narrowed it, `p` is `?*T` again. Assigning to `p` inside a narrowed block ends the narrowing for that block.
- `let m = p else { leave };` binds `m` as `*T`, and the `else` block must leave the enclosing block. `p catch fatal` and `p catch e { yield q; }` follow the error forms, with the error `anti.error.NullPointer`.
- `alloc T { }` and `alloc T(args)` return `*T`. Out of memory is fatal. `alloc(T, n)` returns `?*T`, because `malloc` does.
- `p as *T` returns `*T` and traps. `p as? *T` returns `?*T`. `dup(p)` returns the type of `p`. `is` works on both.
- A class field of type `*T` without a default must be set in every literal or in `construct`. A field of type `?*T` may default to `null`.
- Every pointer in an `extern fn`, in a bound struct and in a C callback is `?*T`. The generated header maps `*T` and `?*T` both to `T *`, with `/* non-null */` on the first. An exported function with a `*T` parameter checks nothing at run time.
- The standard library returns `?*T` wherever "not found" is an answer, and takes `*T` wherever null would be a bug.
- Nothing is emitted for any of this. The checks are the ones the program wrote.

## Dev-mode checks

The switch is the one `assert` uses: emitted in dev mode, absent in release, decided in the back end, so a `.antl` gets the checks whenever the program that links it is a dev build. `--checks` and `--no-checks` override either mode.

- Bounds. Every index into an array, a slice or a `str` is compared against its length before the access. A raw pointer index `p[i]` has no length and is not checked.
- Overflow. `+`, `-` and `*` on signed integers branch on the overflow flag. A narrowing `as` checks the range. Unsigned arithmetic wraps and is not checked.
- Division and shifts. `/` and `%` by zero are checked on ARM64, which returns zero, and trap by themselves on x86_64. A shift count negative or at or above the width is checked on both.
- Null. Not needed. A null dereference cannot be written without a check the compiler demanded.
- A failed check calls the runtime's failure routine. It prints the file, the line, the operation and the values, then aborts.
- Cost in a dev build: a compare and a not-taken branch per checked operation. Cost in release: none.

## Wrapping and saturating operators

- `+% -% *% <<%` give the modulo result in every build and never trap. A reader sees the `%` and knows wrap was intended.
- `+| -| *|` clamp at the type's minimum or maximum. Both CPUs have a two-instruction form.
- Both families apply to integer types only, both operands of one type, and follow the precedence of the plain operator.
- The plain operators keep their meaning: a trap on overflow in dev mode, a wrap in release.
- `mul_high(a, b) -> T` is a built-in like `size_of` and returns the upper half of the full product.

## Flags

- `let (result, flags) = e;` is a two-name `let` whose right side is one arithmetic operation on an integer type: `+ - *`, `<< >>`, or unary `-`. `result` has the operand type and holds the wrapped value. `flags` has the built-in struct type `Flags`.
- `Flags` has four `bool` fields: `overflow`, `carry`, `zero`, `negative`. It is a struct of four bytes, and C sees it as such.
- No trap in any build. Asking for the flags states that overflow is expected.
- The lowering is the instruction plus one flag read per field the program uses. Unused fields cost nothing.
- Carry in. When the last operand of `+` is the `carry` field of a `Flags` value, the lowering is `adc` on x86_64 and `adcs` on ARM64, and the result's `carry` is the carry out. The same holds for `-` with `borrow`, which is the `carry` field read the way subtraction uses it.
- The result name is always new. The flags name may be a new variable or an existing `Flags` variable in scope, which is then assigned. The plain form `(result, flags) = e;` assigns to two existing names.
- There is no pair type. The two-name `let` is the only place two names bind from one expression. Nothing else in the language takes or returns a pair.

```anti
let (lo, f) = a.lo + b.lo;
let (hi, f) = a.hi + b.hi + f.carry;
if f.overflow {
	return error.Error.new(1, "sum does not fit");
}
```

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
- `v is Shape.Circle` gives a `bool`. `v.tag` is the tag as its enum. There is no other access to a case's fields than `switch`.
- A variant is a value type with C layout. It may be a struct field, an array element, a parameter and a result. It passes by value under the struct rules. It cannot have functions, since it is a struct.
- `anti bind` never produces a variant, because C declares none. An exported variant is written as above.

## Locking and channels

- `anti.rt.Mutex` is a struct wrapping the platform's mutex, created with `Mutex.new()` and released with `m.destroy()`.
- `sync m { }` locks `m` for the block and unlocks it on every exit, including `return`, `break`, `continue` and the error forms. Nested `sync` on the same mutex is a compile error when both are in one function and a run-time deadlock otherwise, which the chapter states.
- A field written inside a `sync` block and read outside any `sync` is a warning from the whole-program analysis. The warning fires when both sites are in functions a worker reaches.
- `chan T` is a bounded queue of `T` values, `T` pointer-free by the `parallel` rule. `let c = chan int(16);` creates one. `send(c, v)` blocks when full, `recv(c) -> ?T` blocks when empty and returns `null` after `close(c)`. `select` waits on more than one channel and is written like `switch` over the channels. `chan`, `send`, `recv`, `select` and `sync` were reserved from chapter 2.
- Both are implemented in `anti.rt` over the platform layer of the threading chapter.

## Debug information

- `antic -g` writes a `.loc` directive before the first instruction of every statement and a `.file` directive per source file. llvm-mc turns them into DWARF on ELF and Mach-O and CodeView on COFF. The link step then keeps debug sections instead of stripping them.
- With `-g` a debugger shows Anti source lines, sets breakpoints by file and line, and prints a backtrace with function names. Variables are not described in the first version, so `print x` shows nothing. That is the next step, and the closing guide names it.
- `anti build` passes `-g` in dev mode. Release mode never does.
- `.loc` costs nothing in the emitted code and is never a reason for a program to behave differently.

## Namespaces

- `anti.lang` holds every type the compiler knows by name: `Object`, `Error`, `NullPointer`, `Flags`, `Job`, `Mutex`, `Trace`, `TraceHandler`. `anti.rt` is the C runtime and holds no Anti module a program imports. Everything that only helps lives elsewhere: `anti.error` for error conveniences, `anti.trace` for the stock handlers, `anti.log`, `anti.time` and the rest.
- The rule for a reader: if the compiler needs it, it is in `anti.lang`. If it only helps, it is not.
- `docs/anti-object-model.md` names `anti.rt.Object` and `anti.error.Error`. Both are `anti.lang` under this rule, and the object model document is corrected when it is next edited.

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

- A provider is a static function returning a non-null pointer of the interface type, or a class name whose `get` is a singleton's. `anti build` passes the table to `antic` as `--inject Interface=Provider`. `anti test` passes the test table.
- An interface with no provider is a link-time error naming the class that needs it. A provider of the wrong type is a compile error. A cycle through providers that allocate is detected on the provider graph at link.
- Every `inject` field is resolved through a slot, a static atomic pointer per interface, initialised to the compiled-in provider. Construction calls through the slot. The run-time configuration may replace what a slot holds. See [Runtime configuration](#runtime-configuration).
- `inject final name: *Interface` marks a field the run-time configuration may not replace.
- A program with any `inject` field exports its runtime symbols and its descriptors, so a plugin can share them. `--closed` builds it without the exports, and the program then accepts no run-time replacement.
- Programmers declare and inject their own interfaces the same way. A library ships an interface under its own root with a default provider. Its users override it in their manifest.
- A `.antl` records which interfaces its classes inject, so `anti build` reports what a dependency needs.

## Standard interfaces

- Six abstract classes ship with a default implementation and a default provider, so `inject log: *Logger` works with no manifest entry: `anti.log.Logger`, `anti.time.Clock`, `anti.random.Source`, `anti.fs.FileSystem`, `anti.mem.Allocator`, `anti.config.Config`. A fake of each makes tests deterministic, and a replacement changes a whole program's behaviour.
- The language's `alloc` and `free` stay bound to libc. `Allocator` is what libraries and classes ask for explicitly, and the containers in `anti.collection` take one.
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
- `os.site_config_dir(app)` and `os.user_config_dir(app)` give the platform's directories for machine-wide and per-user configuration, `/Library/Application Support/<app>` and `~/Library/Application Support/<app>` on macOS, `/etc/<app>` and `$XDG_CONFIG_HOME/<app>` on Linux, `%ProgramData%\<app>` and `%APPDATA%\<app>` on Windows. A desktop application passes one of them to `rt.configure`. A program ships no configuration file, since the build is the default, and a file holds only what someone changed.
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
- `p ?? q` on a `?*T` yields `p` as `*T` when it is not null and `q` otherwise. `q` has type `*T` or `?*T`, and the result has the wider of the two.
- `p?.x` and `p?.f(args)` on a `?*T` yield `null` when `p` is null and otherwise the field or the call. The result has type `?*U` when the field or result is a pointer, and is refused otherwise, since Anti has no optional values. Chains follow the first null.
- Default parameter values: `fn open(path: str, mode: Mode = Mode.Read) -> *Error`. The default is a constant expression. Named arguments: `open("x", mode: Mode.Write)`. Positional arguments come first and in order. Named ones follow in any order, each at most once. No positional may follow a named one. Defaults fill what is not given.
- `for i, x in slice { }` binds the index and the element. `for i, x in &slice { }` binds the index and a pointer.
- `switch` on a `str` compares with `text.equal` in a chain, in arm order. The chapter says it is a chain and not a table.
- `x in lo..hi` is `x >= lo && x < hi`. `in` applies to ranges only. Membership in a slice is `slice.contains(x)`, a call, because it is a search.
- Format specifications in `f"..."`: `f"{x:08.3f}"`, `f"{name:>20}"`, `f"{n:x}"`, `f"{n:b}"`. Width, precision, alignment with `<`, `>` and `^`, zero padding, `x`, `X`, `b`, `o` and `e`. Parsed at compile time into calls of `anti.text`. An unknown specification is a compile error.
- Compile-time targets. `target.os`, `target.cpu` and `target.mode` are constants of the enums `Os { Linux, MacOS, Windows }`, `Cpu { X86_64, Arm64 }` and `Mode { Dev, Release }`. A `switch` on one of them is allowed at module level, where its arms hold declarations, and inside functions. It follows the exhaustiveness rule of every `switch`: every value or `else`. Lowering keeps every arm in the IR, tagged with its condition, and the back end keeps the arm for its target and drops the rest before optimisation. Two arms may declare the same name. A library file therefore serves all six targets.
- `anti check --targets all` runs the front end once per target. A program that type-checks on the host is then proven to type-check on all six.
- Script mode. A file whose first line is `#!/usr/bin/env anti` runs with `./tool.anti`. `anti file.anti` compiles the file as a dev build into `~/.anti/cache/<digest>/`, keyed by the file's digest and the compiler version, and runs the result. A second run is a cache hit. No manifest, and `anti.io`, `anti.text` and the rest of the standard library are available.

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
- The generated module holds one class per group, all at module level. The format's class has `fn construct(self, data: []byte) -> *Error`, so `alloc NetworkPacket(data) catch e { }` parses a buffer, and a static `parse(r: *binary.Reader)` for a packet inside a stream. `write(self, w: *binary.Writer)` and `size(self) -> int` are generated. Length and count fields are computed on write and are not fields of the class.
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

- `` `n` may be null, check it or use `?*T` ``
- `` `*T` cannot hold `null` ``
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

## Keywords

- Keywords added: `variant`, `tests`, `fixtures`, `provides`, `undo`, `unreachable`, `undefined`, `show`, `embed`. `sync`, `chan`, `send`, `recv`, `select` were reserved.
- Contextual words added: `trace` before `class` or `fn`, `inject` and `inject final` before a field, `compatible` in an abstract class body, `in` after a value and before a range.
- Labels added: an identifier and `:` before `for`, `while` or a block.
- Tokens added: `?*`, `+% -% *% <<%`, `+| -| *|`, the two-name `let` form `let (a, b) =`.
- Built-in types added: `Flags`, `Mutex`, `chan T`.
- Built-ins added: `mul_high`.
