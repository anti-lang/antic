# Anti syntax overview

Every feature of the language in one place, each with what it does and an example where one helps. The rules are in `docs/decisions.md`, `docs/anti-object-model.md` and `docs/anti-language-additions.md`. This overview follows them and adds nothing. Where a feature is not built yet, its section says so.

Contents:

- [Program structure](#program-structure)
- [Modules](#modules)
- [Types](#types)
- [Literals](#literals)
- [Variables and constants](#variables-and-constants)
- [Operators](#operators)
- [Statements](#statements)
- [Loops](#loops)
- [Functions](#functions)
- [Errors](#errors)
- [Structs](#structs)
- [Enums](#enums)
- [Variants](#variants)
- [Classes](#classes)
- [Interfaces](#interfaces)
- [Ownership](#ownership)
- [Pointers](#pointers)
- [Reflection](#reflection)
- [Operators on classes](#operators-on-classes)
- [Static fields and singletons](#static-fields-and-singletons)
- [Threads](#threads)
- [Injection](#injection)
- [Plugins](#plugins)
- [Tests](#tests)
- [C interop](#c-interop)
- [Compile-time targets](#compile-time-targets)
- [Checks and debugging](#checks-and-debugging)
- [Wire formats](#wire-formats)
- [Script mode](#script-mode)
- [Reserved words](#reserved-words)

## Program structure

A program is a set of modules. One module is one file. `main` returns the process exit code.

```anti
import anti.io;

fn main() -> int
{
	io.println("hello");
	return 0;
}
```

`main` has one of three signatures: `fn main() -> int`, `fn main(args: []str) -> int`, `fn main(args: []str, env: []str) -> int`. Comments are `//` to the end of the line and `/* */`, which do not nest. Source is UTF-8. Indentation is a tab. An item body opens `{` on its own line, a statement block opens `{` on the statement's line.

## Modules

A module path is dotted lowercase identifiers that mirror a directory tree. Items are private unless marked.

```anti
import anti.text;
import com.niese.render as r;

pub fn area(w: f32, h: f32) -> f32 { return w * h; }
internal fn helper() { }
fn private_here() { }

let s = text.equal(a, b);
let c = r.Canvas.new();
```

`pub` exports to every module, `internal` to modules under the same package root, none to the file. `import x as y` renames the local name. Paths under `anti.` are the language's. Third parties use a root they own.

## Types

Sized numbers `i8 i16 i32 i64 u8 u16 u32 u64 f32 f64`, with `int` for `i64`, `uint` for `u64`, `float` for `f64`, `byte` for `u8`. `bool`. `char`, a 32-bit Unicode scalar. `str`, immutable UTF-8, pointer plus length, NUL-terminated outside its length. Fixed arrays `[N]T`. Slices `[]T`, pointer plus length. Pointers `*T` and nullable pointers `?*T`. Function pointers `fn(i32) -> i32`. Structs, enums, variants, classes. The C types `c_int`, `c_long`, `c_wchar` and the rest for bindings. No implicit conversions between numbers.

```anti
let n: i32 = 5;
let xs: [4]int = [1, 2, 3, 4];
let s: []int = xs[1..3];
let p: *Rect = &r;
let f: fn(int) -> int = double;
```

## Literals

Integers in decimal and hex with `_` separators. Floats with digits on both sides of `.`. Strings `"..."` with escapes, `r"..."` raw, `b"..."` bytes, `br"..."` raw bytes, and `#"..."#` with hashes for quotes inside. Interpolation `f"..."` with format specifications. `true`, `false`, `none`. Literals take their type from context.

```anti
let a = 1_000_000;
let b = 0xFF_FF;
let c = 3.14;
let d = r#"a "quoted" string"#;
let e = b"\x00\x01";
let g = f"{name:>10} costs {price:8.2f}";
let x: i8 = -128;
```

Not built yet: `f"..."`.

## Variables and constants

`let` declares a mutable local with mandatory initialisation. `const` declares a constant expression. Shadowing in an inner block is allowed. `undefined` opts one local out of initialisation.

```anti
let x = 5;
let y: f32 = 1.0;
const MAX: int = 64;
let buf: [4096]byte = undefined;
```

Not built yet: `undefined`.

## Operators

C precedence. Arithmetic `+ - * / %`, comparison `== != < <= > >=`, logic `&& || !`, bits `& | ^ ~ << >>`. Conversion `x as T`, checked downcast `p as *T`, nullable downcast `p as? *T`, type test `p is *T`. Wrapping `+% -% *% <<%` and saturating `+| -| *|`. Coalescing `??` and chaining `?.` over `none`. Ranges `lo..hi`, half-open. `x in lo..hi`. Compound assignment `+=` and the rest. No `++`, no `?:`.

```anti
let q = a / b;
let w = a +% b;
let s = a +| b;
let p = maybe ?? &fallback;
let v = node?.next?.value;
if c in 'a'..'z' { }
let (sum, f) = a + b;
if f.carry { }
```

`let (result, flags) = e;` binds the wrapped result and a `Flags` struct with `overflow`, `carry`, `zero` and `negative`. A carry in is `a + b + f.carry`.

Not built yet: wrapping and saturating operators, `Flags`, `??`, `?.`, `in`.

## Statements

`if`, `else if`, `else`. `switch` with no fallthrough. Assignment is a statement. Blocks `{ }` are statements. `defer` and `undo`. `assert`, `show`, `unreachable`. Labels on blocks.

```anti
if x > 0 {
	y = 1;
} else if x < 0 {
	y = -1;
} else {
	y = 0;
}

switch kind {
	Circle => draw_circle(),
	Square, Rect => draw_box(),
	else => { },
}

defer fs.close(f);
undo fs.remove(path);
assert(n > 0, "n must be positive");
let v = show(compute(x));
```

`switch` on an enum without `else` must cover every value. `switch` on a `str` is a comparison chain. `defer` runs at every exit of the block, `undo` only on an error exit. `assert` and `show` vanish in release. `unreachable` traps in dev and is undefined in release.

Not built yet: `undo`, `show`, `unreachable`, `switch` on `str`.

## Loops

`while cond do { }` for zero or more, `do { } while cond` for at least one. `for` over ranges and slices with an optional binding and a constant step. Labels for `break` and `continue`. Everything else is a `while`.

```anti
while i < n do { i = i + 1; }
do { i = i - 1; } while i > 0

for i in 0..10 { }
for 0..3 { }
for i in 0..10 by 2 { }
for i in 0..10 by -1 { }
for x in items { }
for i, x in items { }
for p in &items { }

outer: for a in xs {
	for b in ys {
		if a == b { break outer; }
	}
}
```

`by` takes a constant expression. `by -k` visits the same values as `by k` in reverse order. `by 0` is a compile error, the Heederik guardrail.

Not built yet: labels, `for i, x`.

## Functions

`fn name(params) -> R { }`. `return;` in a function without a result. Default parameter values and named arguments. Function values and bound functions.

```anti
fn add(a: int, b: int) -> int { return a + b; }
fn open(path: str, mode: Mode = Mode.Read) -> ?*Error { }

let e = open("x", mode: Mode.Write);
let f = add;
let g = c.area;         // bound to c, no captures
let n = g();
```

Positional arguments first, named ones after in any order. No overloading by signature.

Not built yet: default values and named arguments.

## Errors

Every function that can fail returns `?*Error`, `none` on success, with results through out pointers. The forms are `catch` at the call, `try` to propagate, `try { }` for a block, `catch fatal` to stop. A bare failing call is a compile error.

```anti
let n = text.parse_int(s) catch e {
	log.warn(f"bad number {s}");
	yield 0;
};

let m = text.parse_int(t) catch fatal;

fn load(path: str, out: *Config) -> ?*Error
{
	let f = try fs.open(path);
	...
	return none;
}

try {
	let f = fs.open(path);
	process(f);
} catch e {
	e.print();
}
```

A handler ends with `yield v` or leaves the block. The name after `catch` is any identifier, scoped to the handler, and the error is deleted when the handler exits unless returned. `Error` has `code`, `message` and an owned `cause`. Libraries subclass it and callers test with `is`.

## Structs

Fields only, exactly the bytes C declares. Every field is visible. Method-call sugar for free functions in the declaring module.

```anti
struct Rect
{
	x: f32,
	y: f32,
	w: f32,
	h: f32,
}

fn area(r: *Rect) -> f32 { return r.w * r.h; }

let r = Rect { x: 0.0, y: 0.0, w: 8.0, h: 4.0 };
let a = r.area();       // area(&r)
```

`packed struct` removes padding. `struct Foo align(16)` raises alignment. `union` has C layout. Bitfields `flags: u32 : 4`. `size_of(T)` is the size on the target.

## Enums

A named integer type with C layout.

```anti
enum Kind { Circle, Square, Empty }
enum Mode: u8 { Read = 1, Write = 2 }

let k = Kind.Circle;
let n = k as int;
```

## Variants

A tagged union with C layout: a tag and a union of the cases. `switch` binds the case's fields and must cover every case.

```anti
variant Shape
{
	Circle { r: f32 },
	Square { side: f32 },
	Empty,
}

let s = Shape.Circle { r: 2.0 };
switch s {
	Circle c => area = 3.14 * c.r * c.r,
	Square q => area = q.side * q.side,
	Empty => area = 0.0,
}

if let Circle c = s { }
```

Not built yet.

## Classes

An object with behaviour. The first field is a pointer to a table of the class's public functions. Every byte is declared in the C header.

```anti
abstract class Shape
{
	pub x: f32 = 0.0,
	pub y: f32 = 0.0,
	const MAX: f32 = 1000.0;

	abstract fn area(self) -> f32;

	pub fn move(self, dx: f32, dy: f32)
	{
		self.x = self.x + dx;
		self.y = self.y + dy;
	}
}

final class Circle
{
	inherits Shape,
	r: f32,

	fn construct(self, r: f32) -> ?*Error
	{
		if r <= 0.0 { return Error.new(1, "radius"); }
		self.r = r;
		return none;
	}

	concrete fn area(self) -> f32 { return 3.14 * self.r * self.r; }

	fn destruct(self) { }
}

let c = alloc Circle(2.0) catch fatal;
defer delete(c);
let a = c.area();
c.move(1.0, 1.0);
```

- `inherits Base,` first line, one base, `anti.lang.Object` when absent. `self.super` is the base part and `self.super.f()` the base's function.
- `pub fn` goes into the table and is visible everywhere, `protected fn` to the class and its chain, `fn` to the class only. A function without `self` is static: `Circle.new(...)`.
- `abstract class` is required when any function has no body. `final class` and `final fn` forbid inheritance and replacement.
- `concrete fn` replaces an inherited entry, `concrete fn Base::f` documents which, `concrete fn Iface::f` fills one interface's table only.
- Fields are private unless `pub` or `protected`. A literal outside the class names `pub` fields only. Defaults fill the rest, then `construct` runs. An inline class field without a default takes `T { }` when every field of `T` has a default or `T` has none, and `construct` runs on it. Otherwise the literal must name it.
- `alloc T { fields }` and `alloc T(args)` create on the heap and return `*T`. `T { fields }` and `T(args)` are values. `construct` with arguments may fail, and it assigns every field without a default on every path to `return none`, which the compiler checks.
- `delete(p)` runs `destruct` up the chain, frees owned fields, frees the object. `destroy(&v)` does the same without the free. A value is destroyed at the end of its block, and so is each element of a local array of them, last to first.
- `dup(p)` is a deep copy through ownership. `=` that copies an existing value with owned fields is refused. A fresh value on the right, a literal, `T(args)` or the result of `dup`, moves, and `=` destroys the value it replaces first.
- `p is *T`, `p as *T` checked, `p as? *T` gives `none` on a mismatch. `==` on class pointers is object identity.

Built. The root is `anti.rt.Object` until the namespaces move under `anti.lang`.

## Interfaces

An interface is an abstract class. A class implements any number.

```anti
abstract class Serializable
{
	abstract fn serialize(self, out: *text.Builder);
}

abstract class Drawable
{
	abstract fn draw(self);
	pub fn describe(self) -> str { return self.type_name(); }
}

class Circle
{
	inherits Shape,
	implements ser: Serializable,
	implements dr: Drawable,

	concrete fn Serializable::serialize(self, out: *text.Builder) { }
	concrete fn Drawable::draw(self) { }
}

let s = &c as *Serializable;
s.serialize(&b);
```

`implements name: Iface,` places the interface's table pointer and fields inside the object at a named field. `&c` converts to `*Serializable` implicitly. A `concrete fn` without a qualifier fills every table with that name. `use name: T,` is composition: `T`'s public names are reachable on the class and the class does not convert to `*T`.

Built.

## Ownership

`own` on a pointer or slice field says the object owns the memory. `destruct`, `dup`, `equals` and `serialize` follow it. Inline class and struct fields are owned by definition. The compiler writes a teardown and a copy for every class, which `delete`, `destroy` and `dup` call, so `--no-reflect` loses nothing about ownership. An `own` slice of class values destroys its elements last to first, as a local array does. `alloc(T, n)` of a class gives zeroed memory, so an element not filled yet has a zero table. `delete`, `destroy` and `dup` trap on a zero table with the class name in every mode, and `is`, `as` and a dispatch do in dev mode. `=` into such an element destroys nothing.

```anti
class Buffer
{
	own data: []byte,
	name: str,
	shared: *Texture,
}
```

## Pointers

`none` is the pointer that points to no value. It is a concept of the language, and zero is today's encoding of it. `*T` is never `none`. `?*T` may be `none` and must be checked before use. The check narrows the type for the block.

```anti
fn find(name: str) -> ?*Node { }

let n = find("root");
if n != none {
	n.value = 1;          // n is *Node here
}
let m = n else { return 1; };
let g = n catch fatal;    // an anti.error.NoneDereference on none
let k = n ?? &default_node;
```

`alloc T { }` returns `*T`, `alloc(T, n)` returns `?*T` as raw memory. Every pointer in an `extern fn` is `?*T`. No pointer arithmetic beyond indexing. A function value follows the same rule: `fn(...)` never holds `none` and `?fn(...)` may.

Built. Not built yet: `??`.

## Reflection

Every class has a descriptor: name, parent, size, fields with name, offset, type and ownership, functions with their table slot. `anti.reflect` reads it.

```anti
let d = reflect.describe(obj);
for f in reflect.fields(d) {
	io.println(f.name);
	let v = reflect.get(obj, f);
}
reflect.set(obj, f, Value { f32: 3.0 });
reflect.call(obj, m, []);
let fresh = reflect.new("Circle");
```

`anti.lang.Object` gives every class `type_name`, `to_text`, `equals`, `hash` and `serialize` with defaults over the descriptor, and `copy` and `destruct`, whose defaults the compiler writes per class. `--no-reflect` drops the field and function lists.

Built: descriptors, `get`, `set`, `call`, `new` and `Value`.

## Operators on classes

`operator fn` with a fixed name maps an operator to a function. Only the left operand's type is looked up.

```anti
class Vec2
{
	pub x: f32,
	pub y: f32,

	operator fn add(self, o: Vec2) -> Vec2 { return Vec2 { x: self.x + o.x, y: self.y + o.y }; }
	operator fn eq(self, o: Vec2) -> bool { return self.x == o.x && self.y == o.y; }
}

let v = a + b;
if a == b { }
```

The names: `add sub mul div rem neg eq lt and or xor shl shr not`. `!=`, `>`, `<=`, `>=` and compound assignments derive. For a struct the operator is a free function in the declaring module.

Built.

## Static fields and singletons

A static field belongs to the class and must be atomic. A singleton has one instance.

```anti
class Circle
{
	static atomic count: int = 0;
	...
	fn construct(self) { Circle.count.add(1); }
}

singleton class Config
{
	path: str = "config.toml",
	atomic requests: int = 0,
	mutable title: str = "untitled",
}

let n = Circle.count.load();
Config.get().requests.add(1);
```

Atomic operations: `load store add sub and or xor swap compare_swap`, sequentially consistent. In a singleton a plain field is read-only after creation and `atomic` fields are atomic. `mutable` fields may be written anywhere except from a worker, which the compiler checks.

Built.

## Threads

Structured fork-join over an array with `parallel`, and one object at a time with `dispatch`.

```anti
worker fn sum(chunk: []int) -> int { ... }
let parts = parallel data -> sum;
let parts2 = parallel data by 4 -> sum;

worker fn render(s: *Sprite, frame: int) { }
let job = dispatch sprite -> render(frame);
join(job);
```

Worker parameters are pointer-free. The table pointer and `own` fields do not count. `[]Circle` chunks like any array, `[]*Shape` is refused and points at `dispatch`. `ANTI_THREADS` is gone. `--anti.threads` and the configuration file set the pool size. `sync m { }` and `chan T` are the locking and channel forms.

Built: `parallel`, `dispatch`, `join`. Not built yet: `sync`, `chan`, `--anti.threads` and the configuration file. `ANTI_THREADS` still sets the pool size.

## Injection

A class declares what it needs, the manifest says who provides it, the run-time configuration may replace it.

```anti
class Renderer
{
	inject log: *Logger,
	inject final alloc: *Allocator,
}
```

```toml
[inject]
"anti.log.Logger" = "net.niese.ConsoleLogger.get"
[inject.test]
"anti.log.Logger" = "net.niese.tests.FakeLogger.get"
```

Six standard interfaces ship with defaults: `Logger`, `Clock`, `Random`, `FileSystem`, `Allocator`, `Config`. Standard interfaces for services come with a reference implementation: `anti.db` with SQLite, `anti.http`, `anti.serialize`, `anti.crypto`.

Not built yet.

## Plugins

A shared library declares what it provides. A host loads it and asks by interface. The run-time configuration can replace a provider without the author's help.

```anti
provides anti.log.Logger as FancyLogger;
```

```anti
let lib = plugin.load("plugins/fancy.so") catch fatal;
let log = lib.instance(anti.log.Logger) catch fatal;
```

```toml
[injections]
"anti.log.Logger" = "lib/CISOLogger.so"
```

Versions are checked at load: interface hash chain, no fields added, every slot the program reaches present. `--anti.conf`, `--anti.inspect` and the other `--anti.` options are the runtime's and are consumed before `main`.

Not built yet.

## Tests

A `tests` block per module holds tests, a `fixtures` block holds their helpers. Both see the module's private items and are compiled only by `anti test`.

```anti
fixtures
{
	fn full_stack() -> Stack { }
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

Not built yet.

## C interop

C functions in, Anti functions out, structs shared without marshalling.

```anti
extern fn printf(fmt: *byte, ...) -> c_int;
extern fn DrawCircleV(center: Vector2, radius: c_float, color: Color);

export fn dot(a: Vec2, b: Vec2) -> c_int { }
export struct Vec2 { x: c_int, y: c_int }

link framework "CoreAudio";
```

`anti bind raylib_api.json` and `anti bind --clang header.h` generate bindings. `anti build --lib static|shared` builds a library with a header. A class is exported with its layout, tables, `anti_<Class>_init` and the dispatch wrappers. `embed("file")` puts a file's bytes in the binary.

Built: `extern`, `export`, static and shared libraries, the header. Not built yet: `anti bind`, `link framework`, `embed`.

## Compile-time targets

`target.os`, `target.cpu` and `target.mode` are constants. A `switch` on them is allowed at module level and must cover every value or carry `else`. The back end keeps one arm.

```anti
switch target.os {
	Windows => {
		extern fn GetTickCount64() -> u64;
	},
	Linux, MacOS => {
		extern fn clock_gettime(id: c_int, ts: *Timespec) -> c_int;
	},
}
```

Not built yet.

## Checks and debugging

In dev mode every array, slice and `str` index is bounds-checked, signed arithmetic traps on overflow, division and shifts are checked, `assert` and `show` are active, `-g` writes line information. In release none of it is emitted. `--checks`, `--asserts`, `--trace` and `-g` override.

```anti
trace class Renderer { }
```

`trace` marks a class or function whose `pub` functions call the `enter` and `leave` hooks in dev mode. `--trace <pattern>` instruments code that did not ask. `anti.trace` ships `LeakTracker`, `Profiler`, `CallLogger` and the rest.

Not built yet: the checks, `-g`, `trace`.

## Wire formats

A description of bytes in a `.fmt` file, from which `anti format` generates a parser, a writer and the classes. Not part of the language.

```text
format Packet endian big
{
	magic: str : 4 = "ANTI",
	length: u16,
	flags: u8
	{
		compressed: bit 0,
		_: bits 1..8,
	},
	body: length bytes Body
	{
		name: str until 0x00,
	},
}
```

```anti
let p = alloc Packet(data) catch fatal;
let name = p.body.name;
```

Not built yet.

## Script mode

A file with `#!/usr/bin/env anti` runs directly. `anti file.anti` compiles into a cache keyed by digest and runs.

```anti
#!/usr/bin/env anti
import anti.io;

fn main(args: []str) -> int
{
	for a in args { io.println(a); }
	return 0;
}
```

Not built yet.

## Reserved words

Keywords: `fn extern let const struct union enum variant class import pub internal protected export if else switch while do for in break continue return defer undo try catch yield assert show unreachable undefined embed as is dup delete destroy alloc free size_of self super abstract concrete static singleton inherits implements use worker parallel dispatch join sync chan send recv select atomic true false none tests fixtures provides`.

Contextual words: `packed align by final own operator mutable trace inject compatible`.

Types with the aliases: the sized numbers, `int uint float byte bool char str`, and the `c_` types.
