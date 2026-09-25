# Anti syntax overview

Every feature of the language in one place, each with what it does and an example where one helps. The rules are in `docs/decisions.md`, `docs/anti-object-model.md` and `docs/anti-language-additions.md`. This overview follows them and adds nothing. Where a feature is not built yet, its section says so.

The test `overview_examples` runs every `anti` block of this page through the front end of antic, so an example that stops compiling fails the suite. A block that opens with `anti not-built` shows a feature that is not built yet, and the test passes over it. The status line of its section names that feature. A hidden block before an example declares the names the example uses without showing them.

Contents:

- [Program structure](#program-structure)
- [Modules](#modules)
- [Direct imports](#direct-imports)
- [Types](#types)
- [Literals](#literals)
- [Variables and constants](#variables-and-constants)
- [Operators](#operators)
- [Statements](#statements)
- [Loops](#loops)
- [Functions](#functions)
- [Anonymous functions and closures](#anonymous-functions-and-closures)
- [Tuples](#tuples)
- [Errors](#errors)
- [Structs](#structs)
- [Simd structs](#simd-structs)
- [Enums](#enums)
- [Variants](#variants)
- [Classes](#classes)
- [Interfaces](#interfaces)
- [Generics](#generics)
- [Ownership](#ownership)
- [Pointers](#pointers)
- [Optional values](#optional-values)
- [Reflection](#reflection)
- [Operators on classes](#operators-on-classes)
- [Static fields and singletons](#static-fields-and-singletons)
- [Threads](#threads)
- [Concurrent classes](#concurrent-classes)
- [Hooks and tracing](#hooks-and-tracing)
- [Injection](#injection)
- [Plugins](#plugins)
- [Tests](#tests)
- [C interop](#c-interop)
- [Compile-time targets](#compile-time-targets)
- [Source locations](#source-locations)
- [Checks and debugging](#checks-and-debugging)
- [Warnings and safety checks](#warnings-and-safety-checks)
- [Wire formats](#wire-formats)
- [Script mode](#script-mode)
- [Regular expressions](#regular-expressions)
- [Bytes](#bytes)
- [Collections](#collections)
- [Standard library](#standard-library)
- [Later](#later)
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

`main` has one of three signatures: `fn main() -> int`, `fn main(args: []str) -> int`, `fn main(args: []str, env: []str) -> int`. Comments are `//` to the end of the line and `/* */`, which do not nest. `///` documents the next item and `//!` the module, and `//#` and `//#!` do the same for the developers of a library. Source is UTF-8. Indentation is a tab. An item body opens `{` on its own line, a statement block opens `{` on the statement's line.

## Modules

A module path is dotted lowercase identifiers that mirror a directory tree. Items are private unless marked.

<!-- overview: context, docs-style:ignore
```anti
let a = "left";
let b = "right";
```
-->
```anti
import anti.text;
import anti.io as out;

pub fn area(w: f32, h: f32) -> f32 { return w * h; }
internal fn helper() { }
fn private_here() { }

let s = text.equal(a, b);
out.println("renamed");
```

`pub` exports to every module, `internal` to modules under the same package root, none to the file. `import x as y` renames the local name. Paths under `anti.` are the language's. Third parties use a root they own.

## Direct imports

`import anti.collection.map.{Map, HashMap};` makes the listed names of a module visible in the file without the module's name, so code writes `Map<str, int>` rather than `map.Map<str, int>`. Each listed name is a public item of that module, and one that clashes with a name already visible in the file is refused, naming both. The module stays reachable by its name, as with a plain `import`. `anti fmt` keeps the list sorted.

```anti not-built
import anti.collection.map.{HashMap, Map};
import anti.mem.{ArenaAllocator, Shared};
import anti.regex.{Regex};

let ages = Map<str, int>.new();
let seen = map.HashMap<str, bool>.new();
```

Not built yet: direct imports.

## Types

Sized numbers `i8 i16 i32 i64 u8 u16 u32 u64 f32 f64`, with `int` for `i64`, `uint` for `u64`, `float` for `f64`, `byte` for `u8`. `f16` is storage only, read as `f32` and written with `as f16`. `bool`. `char`, a 32-bit Unicode scalar. `str`, immutable UTF-8, pointer plus length, NUL-terminated outside its length. Fixed arrays `[N]T`. Slices `[]T`, pointer plus length. Pointers `*T` and nullable pointers `?*T`. Optional values `?T` of any type. Function pointers `fn(i32) -> i32` and nullable ones `?fn(i32) -> i32`. Tuples `(int, str)`, anonymous structs with C layout. Structs, enums, variants, classes. The built-in types `Flags`, `Mutex` and `chan T`. The C types `c_int`, `c_long`, `c_wchar` and the rest for bindings. No implicit conversions between numbers.

<!-- overview: context, docs-style:ignore
```anti
struct Rect { w: f32, h: f32 }
fn double(n: int) -> int { return 2 * n; }
let r = Rect { w: 8.0, h: 4.0 };
```
-->
```anti
let n: i32 = 5;
let xs: [4]int = [1, 2, 3, 4];
let s: []int = xs[1..3];
let p: *Rect = &r;
let f: fn(int) -> int = double;
let t: (int, str) = (1, "one");
let h: f16 = 1.5 as f16;
```

Built: `f16`, one conversion instruction on ARM64 and at x86-64-v3 and a call of the runtime at `v1` and `v2`. Not built yet: `?T` beyond `?*T`, `?fn(...)` and `?Match`, see [Optional values](#optional-values).

## Literals

Integers in decimal and hex with `_` separators. Floats with digits on both sides of `.`. Strings `"..."` with escapes, `r"..."` raw, `b"..."` bytes, `br"..."` raw bytes, and `#"..."#` with hashes for quotes inside. Interpolation `f"..."` with format specifications, and `rf"..."` for interpolation without escapes. The text of an `f"..."` is memory of the C library, freed with `free(s.ptr)`, and text that should live in an `ArenaAllocator` is built with `anti.text.Builder`, whose `take_in(from)` takes the allocator. Bytes in hex `x"00 AB CC"`. A pattern literal `re"..."`, see [Regular expressions](#regular-expressions). The prefixes are `r`, `b`, `br`, `f`, `rf`, `x` and `re`, one meaning each. `true`, `false`, `none`. Literals take their type from context.

<!-- overview: context, docs-style:ignore
```anti
import anti.text;
let name = "tea";
let price = 2.5;
```
-->
```anti
let a = 1_000_000;
let b = 0xFF_FF;
let c = 3.14;
let d = r#"a "quoted" string"#;
let e = b"\x00\x01";
let g = f"{name:>10} costs {price:8.2f}";
let h = rf"C:\tools\{name}";
let i = x"00 AB CC";
let x: i8 = -128;
```

Built: `f"..."`, `rf"..."` and `re"..."`.

## Variables and constants

`let` declares a mutable local with mandatory initialisation. `const` declares a constant expression. Shadowing in an inner block is allowed. `undefined` opts one local out of initialisation.

```anti
let x = 5;
let y: f32 = 1.0;
const MAX: int = 64;
```

```anti not-built
let buf: [4096]byte = undefined;
```

Not built yet: `undefined`.

## Operators

C precedence. Arithmetic `+ - * / %`, comparison `== != < <= > >=`, logic `&& || !`, bits `& | ^ ~ << >>`. Conversion `x as T`, checked downcast `p as *T`, nullable downcast `p as? *T`, type test `p is *T`. Wrapping `+% -% *% <<%` and saturating `+| -| *|`. Coalescing `??` and chaining `?.` over `none`. Ranges `lo..hi`, half-open. `x in lo..hi`. Compound assignment `+=` and the rest, `+%=` and `+|=` among them. No `++`, no `?:`.

<!-- overview: context, docs-style:ignore
```anti
struct Node { value: int, next: ?*Node }
let a = 7;
let b = 2;
let c = 'q';
let fallback = Node { value: 0, next: none };
let maybe: ?*Node = none;
let node: ?*Node = &fallback;
```
-->
```anti
let q = a / b;
let w = a +% b;
let s = a +| b;
let p = maybe ?? &fallback;
let v = node?.next?.next;
if c in 'a'..'z' { }
let (sum, f) = a + b;
if f.carry { }
```

`let (result, flags) = e;` binds the wrapped result and a `Flags` struct with `overflow`, `carry`, `zero` and `negative`. A carry in is `a + b + f.carry`. `mul_high(a, b)` gives the upper half of the full product.

Built: `x in lo..hi`, `??` and `?.`, the wrapping and saturating operators with their compound assignments, `mul_high` and `Flags`, with `adc` and `adcs` for a carry in.

## Statements

`if`, `else if`, `else`. `switch` names one value per arm, and an enum value with its type, `Kind.Circle`. It falls through only where an arm ends in `fallthrough;`. Assignment is a statement. Blocks `{ }` are statements. `defer` and `undo`. `assert`, `show`, `unreachable`. Labels on blocks.

<!-- overview: context, docs-style:ignore
```anti
import anti.fs;
enum Kind { Circle, Square, Rect }
fn draw_circle() { }
fn draw_box() { }
let x = 3;
let y = 0;
let n = 1;
let kind = Kind.Square;
let path = "out.txt";
let f = fs.open(path, fs.Mode.Write) catch fatal;
```
-->
```anti
if x > 0 {
	y = 1;
} else if x < 0 {
	y = -1;
} else {
	y = 0;
}

switch kind {
	Kind.Circle => draw_circle(),
	Kind.Square => {
		draw_box();
		fallthrough;
	},
	else => { },
}

defer fs.close(f) catch fatal;
undo fs.remove(path) catch e { e.print(); };
assert(n > 0, "n must be positive");
```

```anti not-built
let v = show(compute(x));
if x < 0 { unreachable; }
```

`switch` on an enum without `else` must cover every value. `switch` on a `str` is a comparison chain. `fallthrough;` as an arm's last statement enters the next arm's body without testing its values, and is refused in the last arm and into an arm that binds a variant's fields. `defer` runs at every exit of the block, `undo` only on an error exit. `assert` and `show` vanish in release. `unreachable` traps in dev and is undefined in release.

Built: `fallthrough` and `switch` on `str`. Not built yet: `show`, `unreachable`.

## Loops

`while cond do { }` for zero or more, `do { } while cond` for at least one. `for` over ranges and slices with an optional binding and a constant step, and over collections and iterators. Labels for `break` and `continue`. Everything else is a `while`.

<!-- overview: context, docs-style:ignore
```anti
let i = 0;
let n = 4;
let items: [3]int = [1, 2, 3];
```
-->
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
```

```anti not-built
outer: for a in xs {
	for b in ys {
		if a == b { break outer; }
	}
}
```

`by` takes a constant expression. `by -k` visits the same values as `by k` in reverse order. `by 0` is a compile error, the Heederik guardrail.

`for x in e` also walks a collection. The collection has `operator fn iter(self)`, which gives a new iterator on every call, and the iterator has `operator fn next(self) -> bool` and `operator fn value(self) -> T`. Nothing allocates, and nested loops each keep their own position. `for x in it` walks an iterator in its place. `while` calls the same hooks by name when the iterator is needed after the loop. `.to_slice()` collects what an iterator gives into a new slice, freed with `free(s.ptr)`.

<!-- overview: context, docs-style:ignore
```anti
class Person { pub age: int = 0, }
class People
{
	pub all: [2]Person = [Person { }; 2],
	operator fn iter(self) -> PeopleIter { return PeopleIter { list: self, at: -1 }; }
}
class PeopleIter
{
	pub list: *People,
	pub at: int,
	operator fn next(self) -> bool { self.at = self.at + 1; return self.at < 2; }
	operator fn value(self) -> Person { return self.list.all[self.at]; }
}
let people = People { };
```
-->
```anti
for p in people { }

let it = people.iter();
while it.next() do {
	let p = it.value();
	if p.age >= 65 {
		break;
	}
}

let all = people.iter().to_slice();
free(all.ptr);
```

`for x in c` gives a copy of each element, and the loop variable is read-only, so a change that would reach the copy alone is refused. `for x in &c` gives each element as a `lent` pointer for one turn of the loop, and a change through it reaches the element. Both forms hold for slices and for the collections of [Collections](#collections). A collection must not change its size while a loop walks it. A dev build traps when it does, naming the collection and both places, and `remove_all(test)` removes while it walks.

```anti not-built
for p in people { }       // p is a copy of each Person, read-only
for p in &people {
	p.age += 1;           // p is a lent *Person
}
```

Built: `for` over a collection through `iter`, `next` and `value`, and `to_slice`. Not built yet: labels, `for x in &c` over a collection, the read-only loop variable of the copy form, and the trap on a collection that changes its size while a loop walks it.

## Functions

`fn name(params) -> R { }`. `return;` in a function without a result. Default parameter values and named arguments. Function values and bound functions.

<!-- overview: context, docs-style:ignore
```anti
import anti.fs;
class Circle
{
	r: f32 = 1.0,

	pub fn area(self) -> f32 { return 3.14 * self.r * self.r; }
}
let c = Circle { };
```
-->
```anti
fn add(a: int, b: int) -> int { return a + b; }
fn scaled(n: int, by: int = 2) -> int { return n * by; }

let h = fs.open("x", fs.Mode.Write) catch fatal;
let k = scaled(4);
let f = add;
let g = c.area;         // bound to c, no captures
let a = g();
```

```anti not-built
let h = fs.open("x", mode: fs.Mode.Write) catch fatal;
```

Positional arguments first, named ones after in any order. No overloading by signature.

Built: default values, a constant expression or `here`, over a module boundary as well. Not built yet: named arguments.

## Anonymous functions and closures

`fn(params) -> R { body }` as an expression is an anonymous function. At a parameter of function type it may leave out its types, which come from that parameter. One that uses a local of the enclosing function is a closure, and captures that local by reference.

```anti
fn each(items: []int, f: fn(int))
{
	for x in items {
		f(x);
	}
}

fn count_even(items: []int) -> int
{
	let hits = 0;
	each(items, fn(n) {
		if n % 2 == 0 {
			hits += 1;
		}
	});
	return hits;
}

fn on_click(keep f: fn(int)) { }
fn map_sum(items: []int, concurrent f: fn(int) -> int) -> int { return 0; }
```

```anti
class Panel
{
	own handler: fn(int) = ignore,

	pub fn on_click(self, keep own f: fn(int))
	{
		self.handler = f;
	}
}

fn ignore(n: int) { }
fn save(name: str, n: int) { }

fn wire(p: *Panel)
{
	let name = "report";
	p.on_click(snapshot fn(n) { save(name, n); });
	name = "draft";
}
```

A closure never outlives what it captures. It is passed to a parameter that does not keep it, or held in a local used the same way, and never stored in a field, a return value, a global or a `keep` parameter. Creating one allocates nothing. A parameter that passes its function on to `parallel` or `dispatch` is marked `concurrent`, and a closure there may change a captured variable only when its type is thread-safe. A worker takes a closure through a `concurrent` parameter alone. `snapshot fn` copies what it uses when it is made, is read-only, and holds numbers, `bool`, `char`, structs of those and `str`. It is accepted at an `own` field, a `keep own` parameter and a `concurrent` parameter, and a plain `keep` parameter refuses it. A parameter that does not keep its argument is two words, the code and a context pointer, and a plain kept function value stays one C function pointer. `own fn` is two words as well, the code and a snapshot on the heap that its owner frees. `=` does not copy it, and `dup` copies it with its snapshot. The header writes the two words of a parameter as a callback and a `void *` context, and an `own fn` field as a struct of the code and the snapshot. An `extern fn` takes C function pointers alone.

Built: everything above. That covers anonymous functions with their types from the target, closures that capture by reference, `keep` and `concurrent` with the checks on both sides, the two words of a parameter that does not keep its argument and the rule of the worker. `snapshot fn` is built as well, with `own` fields and `keep own` parameters. `programs/closures.anti` and `programs/snapshots.anti` run them, and `errors/closures.anti` and `errors/snapshots.anti` hold the refusals.

## Tuples

An anonymous struct with C layout, for a function with two answers and no name for the pair.

<!-- overview: context, docs-style:ignore
```anti
let items: [3]str = ["a", "b", "c"];
```
-->
```anti
fn divmod(a: int, b: int) -> (int, int) { return (a / b, a % b); }

let t = divmod(7, 2);
let q = t.0;
let (d, r) = divmod(7, 2);
for i, x in items { }
```

Elements are `t.0`, `t.1` and on. Destructuring is `let (a, b) = e;` and `for i, x in items`, and nowhere else: not in a parameter list and not nested. Two tuple types are the same when their element types are the same in order. A tuple of more than three elements, or one that crosses a module boundary, is a struct that has not been named yet.

Built.

## Errors

Mark a function that can fail with `may fail`. It leaves on one of two channels: `return v;` with the result, `fail e;` with a `*Error`. The forms at the call are `catch`, `try` to propagate, `try { }` for a block, `catch fatal` to stop, and `catch none` to count a failure as `none` where the result can be `none`. A bare failing call is a compile error.

<!-- overview: context, docs-style:ignore
```anti
import anti.fs;
import anti.log;
import anti.text;
struct Config { size: int }
fn parse(f: *fs.File) -> Config may fail { return Config { size: try fs.size(f) }; }
fn process(f: *fs.File) may fail { try fs.write(f, b"x"); }
let s = "12";
let t = "7";
let path = "anti.toml";
```
-->
```anti
let n = text.parse_int(s) catch e {
	log.warn(f"bad number {s}");
	yield 0;
};

let m = text.parse_int(t) catch fatal;

fn load(path: str) -> Config may fail
{
	let f = try fs.open(path, fs.Mode.Read);
	defer fs.close(f) catch fatal;
	if try f.size() == 0 { fail "empty configuration"; }
	return try parse(f);
}

try {
	let f = fs.open(path, fs.Mode.Read);
	process(f);
	fs.close(f);
} catch e {
	e.print();
}
```

A handler ends with `yield v` or leaves the block. The name after `catch` is any identifier, scoped to the handler, and the error is deleted on every exit of the handler unless `return e` or `fail e` hands it on or a call moved it into an `own` parameter. `Error.wrap` takes its cause so. `fail "text"` is `fail Error.new(0, "text")`. `try` forwards the error and is refused outside a function that may fail. `undo` runs on the `fail` path and not on `return`. `Error` has `code`, `message`, an owned `cause`, the origin `at` that `fail` fills, and the frames of a trace when backtraces are on. It lives in `anti.lang`, the root of the standard library, which imports nothing. `e.text()` gives `error N: message`, or the message alone for code 0, with each cause on a line of its own, and the error keeps those bytes until it is deleted. Libraries subclass it and callers test with `is`. `anti.error` adds `SystemError`, with `from_errno()` and `from_win32()`, and `on_fatal` and `check`.

The ABI of a `may fail` function is `?*Error f(args, R *out)`, with the result through an out pointer. `fn(A) -> R may fail` is its type as a value, a call through it is handled as a direct call is, and the header writes the ABI form. The compiler supplies that pointer over storage whose table it zeroes. The binding is destroyed at the end of its block like any other local. `may fail` is the one failing form. A C binding returns its error as an ordinary value, and a `may fail` wrapper turns it into a `fail`.

Built: `catch`, `try`, the `try` block and `catch fatal`, `may fail` and `fail`, `may fail` on a `construct` with arguments, and `undo` on the fail path. `Error` and `NoneDereference` live in `anti.lang`. Every test program uses `may fail`, and `errors/construct_forms.anti` checks that a `construct` refuses `-> ?*Error`. A function fails by its `may fail` marking alone, and one that returns `*Error` or `?*Error` without it gives the error as a value. `text.parse_int` may fail, and no other function of `anti.text` or `anti.io` fails. The three user directories of `anti.os` may fail, and so does every function of `anti.fs`: `open`, `read`, `write`, `size`, `close`, `list`, `remove` and `rename`. `toml.Document.read`, `log.FileSink.new`, `args.Parser.parse`, `reflect.set`, `reflect.call` and `json.unquote` may fail. `try` stands wherever the call stands, after `return` and inside an expression as well. `fn(A) -> R may fail` is a type, and a call through a value of it takes a handler. A handler moves its error into an `own` parameter, and `Error.wrap` takes its cause so. The first `fail` of an error writes its origin `at` and, when backtraces are on, its frames, and `e.text()` names the position, the causes and the trace. `--anti.backtrace` and `backtrace = true` of the runtime configuration turn the frames on in a release build. The checker warns where the name a `catch` binds shadows a variable in scope, `shadowed-catch`, and where a `may fail` function holds no `fail` and no `try`, `never-fails`. `catch none` counts a failure as `none` where the result can be `none`, and deletes the error as every handler does. `programs/catch_none.anti` runs it and `errors/catch_none.anti` holds the refusals.

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

## Simd structs

`simd struct` declares a vector whose fields are its lanes. Every field has the same primitive type and the count is a power of two. The size is a multiple of eight bytes up to the vector cap, 256 bytes to start, one constant of the CPU level table.

```anti
import anti.simd;

simd struct Vec4 { x: f32, y: f32, z: f32, w: f32 }

let a = Vec4 { x: 1.0, y: 2.0, z: 3.0, w: 4.0 };
let b = Vec4.splat(2.0);
let c = a * b;
let m = a < b;
let d = simd.select(m, a, b);
let s = c.sum();
```

Arithmetic, the bitwise operators on integer lanes, comparisons and unary minus apply lane by lane. A comparison yields a mask, a `simd struct` of `bool` with the same lane count. `simd.select`, `simd.any` and `simd.all` are in `anti.simd`. `splat`, `load`, `store`, `shuffle`, `sum`, `min`, `max` and `dot` are built in. `as` between a `simd struct` and the array or plain struct of the same bytes is free. The back end maps each operation to the target's native width, so the lane count is the programmer's and the instruction count is the machine's. Above the vector cap it is an array and a loop.

Built: the declaration and its rules, the element-wise operators, the masks of the comparisons, `simd.select`, `simd.any` and `simd.all` of `anti.simd`, the eight built-ins and `as` to an array or a plain struct of the same bytes. A `simd struct` of 16 bytes is the vector type of C in the header and passes in a vector register. One of another size passes as the struct of its lanes. An `f32x8` is one instruction at x86-64-v3 and two everywhere else, and a `simd struct` above the cap is an array and a loop, which a warning at its declaration names.

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

<!-- overview: context, docs-style:ignore
```anti
let area: f32 = 0.0;
```
-->
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
let round = s is Shape.Circle;
let n = s.tag as int;
```

Built: declarations with `packed` and `align(N)`, the literals of the cases, `switch` that names every case or has `else`, `if let`, `is`, `tag`, a variant of another module and the enum of the tags in the C header. `fallthrough` never enters an arm that binds a case's fields.

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

final class Circle inherits Shape
{
	r: f32,

	fn construct(self, r: f32) may fail
	{
		if r <= 0.0 { fail lang.Error.new(1, "radius"); }
		self.r = r;
	}

	concrete fn area(self) -> f32 { return 3.14 * self.r * self.r; }

	fn destruct(self) { }
}

let c = alloc Circle(2.0) catch fatal;
defer delete(c);
let a = c.area();
c.move(1.0, 1.0);
```

- `class Name inherits Base` names the base in the header, after the name and after `align(N)` when the class has one. One base, `anti.lang.Object` when absent, and `module.Class` for a base of another module. The base is no member, so `inherits` in the body is refused, and `implements` and `use` stay there as named sub-objects. `self.super` is the base part and `self.super.f()` the base's function, which is refused when that function is abstract.
- `pub fn` goes into the table and is visible everywhere, `protected fn` to the class and its chain, `fn` to the class only. A function without `self` is static: `Circle.new(...)`.
- `abstract class` is required when any function has no body. `final class` and `final fn` forbid inheritance and replacement.
- `concrete fn` replaces an inherited entry, `concrete fn Base::f` documents which, `concrete fn Iface::f` fills one interface's table only.
- Fields are private unless `pub` or `protected`, and a field may be a bitfield as in a struct. A literal outside the class names `pub` fields only. Defaults fill the rest, then `construct` runs. An inline class field without a default takes `T { }` when every field of `T` has a default or `T` has none, and `construct` runs on it. Otherwise the literal must name it.
- `alloc T { fields }` and `alloc T(args)` create on the heap and return `*T`. `T { fields }` and `T(args)` are values. A `construct` with arguments that can fail is written `may fail`, leaves with `fail` and succeeds at its end. Every path that succeeds assigns every field without a default, which the compiler checks. `T(args)` carries the error of `construct` to the handler at the call, and a failed `alloc` frees the object.
- A `construct` below that wants its base initialised calls `self.super.construct(args)` as the first statement of its body, and `try` forwards a failure of the base. One that leaves the call out keeps the defaults of the base's fields.
- `delete(p)` runs `destruct` up the chain, frees owned fields, frees the object. `destroy(&v)` does the same without the free. `delete(p, from)` and `destroy(p, from)` give every piece of owned memory back to the `anti.mem.Allocator` `from`, and `delete` the object as well. Memory goes back to the allocator it came from. A value is destroyed at the end of its block, and so is each element of a local array of them, last to first.
- `dup(p)` is a deep copy through ownership. `=` that copies an existing value with owned fields is refused. A fresh value on the right, a literal, `T(args)` or the result of `dup`, moves, and `=` destroys the value it replaces first.
- `p is *T`, `p as *T` checked, `p as? *T` gives `none` on a mismatch. `==` on class pointers is object identity.

A class may declare a `struct`, `enum` or `class` inside its body. The nested type is private: only the enclosing class names it, and a public signature that names it is a compile error. Its full name is `PeopleList.Node` in the symbols and the header. A type that users work with directly stays at module level.

<!-- overview: context, docs-style:ignore
```anti
struct Person { age: int }
```
-->
```anti
class PeopleList
{
	struct Node
	{
		person: Person,
		next: ?*Node,
	}

	head: ?*Node = none,
}
```

Built: everything above, nested types included. The C header writes a nested type as `PeopleList_Node`, and one that the layout of an `export class` reaches stands before the class with its layout alone.

## Interfaces

An interface is an abstract class. A class implements any number.

<!-- overview: context, docs-style:ignore
```anti
import anti.text;
abstract class Shape
{
	pub x: f32 = 0.0,
}
```
-->
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

class Circle inherits Shape
{
	implements ser: Serializable,
	implements dr: Drawable,

	concrete fn Serializable::serialize(self, out: *text.Builder) { }
	concrete fn Drawable::draw(self) { }
}

let c = Circle { };
let b = text.Builder.new();
let s = &c as *Serializable;
s.serialize(&b);
```

`implements name: Iface,` places the interface's table pointer and fields inside the object at a named field. `&c` converts to `*Serializable` implicitly. A `concrete fn` without a qualifier fills every table with that name that no qualified body of its class fills, and the nearest body wins down the chain. Two qualified bodies of one name fill the tables of two interfaces whose signatures differ, and `c.f()` on the class is ambiguous when two or more interfaces have qualified bodies of `f` and no unqualified body exists. A single qualified body is reached through its interface's table. `use name: T,` is composition: `T`'s public names are reachable on the class and the class does not convert to `*T`.

Built.

## Generics

Functions, structs, classes, variants and interfaces take type parameters between `<` and `>`. A constraint after `:` states what the generic needs from a type: a hook of the operator table such as `lt`, `add`, `iter` or `hash`, an interface, or a set named with `constraint`, combined with `+`. The compiler checks the body against the constraints where the generic is written and every use where it is used, so an error lands in the code that made it. A parameter without constraints can be stored, copied, moved, passed on and measured with `size_of`, which is what a container needs. `N: int` takes an integer constant instead of a type, such as the length of a fixed array. `anti.lang` ships `constraint Number = add + sub + mul + div + neg + lt;` and `constraint Ordered = eq + lt;`.

<!-- overview: context, docs-style:ignore
```anti
class Person
{
	pub name: str,
}

class List<T>
{
	count: int,

	pub fn new() -> List<T>
	{
		return List<T> { count: 0 };
	}
}
```
-->
```anti
fn max<T: lt>(a: T, b: T) -> T
{
	if a < b {
		return b;
	}
	return a;
}

struct Pair<A, B> { first: A, second: B, }
struct Ring<T, N: int> { items: [N]T, head: int, }
variant Result<T, E> { Ok { value: T }, Err { error: E }, }
constraint Key = eq + hash;

let p: Pair<int, str> = Pair { first: 1, second: "one" };
let m = max(3, 7);                     // max<int>
let people = List<Person>.new();

type People = List<Person>;
export type PersonList = List<Person>;
```

Type arguments are inferred from the arguments of a call and written out where nothing gives them. In an expression, `<` after a name opens type arguments when a list of types closed by `>` follows and the token after it is `(`, `.` or `{`, so `a < b > c` stays two comparisons. `>>` closes two lists. Every use with concrete arguments gets its own compiled copy, with no boxing, and two uses with the same arguments are one type in every module. The whole-program pass merges copies whose code is identical. A method with type parameters of its own is called directly and never stands in the table, so it cannot be `abstract` or replaced. A library file stores a generic as its checked syntax tree with its parameters open, and each use lowers that tree with its arguments. C sees no open generic: `export type PersonList = List<Person>;` writes a copy into the header as an exported class, and a generic `export fn` is refused.

Built: the syntax of generics and its checks. Type parameters in `<>` on functions, structs, classes, variants, interfaces and the functions of a class body, `N: int`, type arguments in every type, the rule of C# in an expression with the refusal of a name that is not generic, and `>>` closing two lists. `constraint` and `type` are built, and `export type` is checked. The checker infers the type arguments of a call from its arguments. A generic that nothing uses compiles to nothing. `tests/dump/generics.anti` and `tests/errors/generics.anti` hold the forms and the refusals. The constraints are built: hooks, interfaces, `+`, named sets and `Number` of `anti.lang`, the check of a body where it is written and of every use where it stands, and what a parameter without constraints allows. `tests/dump/constraints.anti` and `tests/errors/constraints.anti` hold both sides. The compiled copies are built for the generics of the module's own source: one copy per use with concrete arguments, `size_of(T)` per copy, a descriptor per copy of a class, link-once copies in a dev build and the whole-program merge of identical copies in a release build. `tests/programs/generic_copies.anti` runs in both modes. A generic, a `type` and a `constraint` in a library file are built. The file carries the checked tree of every generic, a module that uses one makes its copies from it, and one copy is one type across modules. `export type` writes its copy into the C header as an exported class, and `anti doc` shows the parameters of a generic with their constraints. `tests/modules/generics/` runs a program and two libraries in both modes, and `clib_generics` calls a named copy from C. Not built yet: the cache of copies in a dev build. A function of a class with type parameters of its own is refused at its first call. `Ordered` of `anti.lang` is not built either.

## Ownership

`own` on a pointer or slice field says the object owns the memory. `destruct`, `dup`, `equals` and `serialize` follow it. Inline class and struct fields are owned by definition. The compiler writes a teardown and a copy for every class, which `delete`, `destroy` and `dup` call, so `--no-reflect` loses nothing about ownership. An `own` slice of class values destroys its elements last to first, as a local array does. `alloc(T, n)` of a class gives zeroed memory, so an element not filled yet has a zero table. So does the storage the compiler supplies for the out pointer of a `catch` binding. `delete`, `destroy` and `dup` trap on a zero table with the class name in every mode, and `is`, `as` and a dispatch do in dev mode. `=` into such an element destroys nothing.

`transient` on a `?*T` or `?fn(...)` field marks derived state, such as a cache. `dup` gives the copy `none` there, and the default `equals`, `hash` and `serialize` pass over the field. The class frees it in its own `destruct`.

<!-- overview: context, docs-style:ignore
```anti
class Texture
{
	pub id: int = 0,
}
```
-->
```anti
class Buffer
{
	own data: []byte,
	name: str,
	shared: *Texture,
}
```

`own` before a parameter takes ownership of the argument, for a value of any type. Passing a local moves it, and naming the local again is refused. `lent` before a pointer parameter says the pointer is valid only during the call. The function may read and change through it and pass it on to another `lent` parameter, and may not store it, return it, capture it in a closure that outlives the call or pass it to a `keep` or `own` place. `anti.mem.Shared<T>` gives an object more than one owner through an atomic count: `share()` gives another handle and counts it, `=` stays refused, and the object is destroyed when its last handle is freed. Two shared objects that hold each other are never freed.

```anti not-built
fn grow(lent p: *Person) { p.age += 1; }

let c = Shared<Circle>.new(Circle { r: 2.0 });
let shapes = List<Shared<Circle>>.new();
shapes.push(c.share());
```

Built: `own` and `transient` fields, and the `own` parameter that takes an error. Not built yet: the `own` parameter of any other type, `lent` and `Shared<T>`.

## Pointers

`none` is the pointer that points to no value. It is a concept of the language, and zero is today's encoding of it. `*T` is never `none`. `?*T` may be `none` and must be checked before use. The check narrows the type for the block.

<!-- overview: context, docs-style:ignore
```anti
struct Node { value: int }
let default_node = Node { value: 0 };
```
-->
```anti
fn find(name: str) -> ?*Node { return none; }

let n = find("root");
if n != none {
	n.value = 1;          // n is *Node here
}
let m = n else { return 1; };
let g = n catch fatal;    // an anti.lang.NoneDereference on none
let k = n ?? &default_node;
```

`alloc T { }` returns `*T`, `alloc(T, n)` returns `?*T` as raw memory. Every pointer in an `extern fn` is `?*T`. No pointer arithmetic beyond indexing. A function value follows the same rule: `fn(...)` never holds `none` and `?fn(...)` may.

Built.

## Optional values

`?T` is a `T` or `none`, for a value of any type, and follows every rule of `?*T`. It compares with `none`, narrows after a test, and works with `if let`, `let ... else` and `??`, and it is used as a `T` only after a test proves it holds one. `none` means that nothing is there, which is normal, and a failure is `may fail`: `map.get(key)` gives `?V`, and `text.parse_int(s)` stays `may fail`. A `?T` of a value type is the value and one flag byte, padded to the type's alignment, and `?*T` stays one pointer. The C header writes `?T` as a struct of the value and a `bool`.

```anti not-built
fn find_user(id: int) -> ?User { return none; }

let age = ages.get("Ann") ?? 0;
if let a = ages.get("Ann") { }
let first = queue.first() else { return 1; };
```

Built: `?*T`, `?fn(...)` and `?Match`, a struct of `anti.lang`. Not built yet: `?T` of any other type.

## Reflection

Every class has a descriptor: name, parent, size, fields with name, offset, type and ownership, functions with their table slot. `anti.reflect` reads it. `describe` gives the descriptor of an object, `field_count` and `field` read the field list, and `function_count` and `function` the function list. `get` and `set` read and write a field by its index as a `Value`, `call` calls a function by its index, and `new` builds an object of a class it names, or gives `none`.

<!-- overview: context, docs-style:ignore
```anti
import anti.io;
import anti.reflect;
class Circle
{
	pub r: f64 = 1.0,

	pub fn area(self) -> f64 { return 3.14 * self.r * self.r; }
}
let obj = alloc Circle { };
let no_args: [1]reflect.Value = [reflect.nothing()];
```
-->
```anti
let d = reflect.describe(obj as *byte) else { return 1; };
for i in 0..reflect.field_count(d) {
	io.println(reflect.field(d, i).name);
	let v = reflect.get(obj as *byte, d, i);
}
reflect.set(obj as *byte, d, 0, reflect.of_float(3.0)) catch fatal;
let r = reflect.call(obj, 0, no_args[0..0]) catch fatal;
let fresh = reflect.new("Circle");
```

`anti.lang.Object` gives every class `type_name`, `to_text`, `equals`, `hash` and `serialize` with defaults over the descriptor, and `copy` and `destruct`, whose defaults the compiler writes per class. `copy(self, to: *Object)` fills an object that `dup` has already allocated at its concrete size, and a replacement fills the fields and never allocates. `Object.deserialize(text, from)` reads the text of `serialize` back into an object, which comes with its strings and the objects it owns from the `anti.mem.Allocator` `from`. The caller gives them back through `from` at once. `--no-reflect` drops the field and function lists.

<!-- overview: context, docs-style:ignore
```anti
import anti.text;
let b = text.Builder.new();
```
-->
```anti
import anti.mem;

let arena = mem.ArenaAllocator.new(mem.LibcAllocator.get(), 4096);
let back = Object.deserialize(b.text(), &arena);
destroy(back, &arena);
arena.free_all();
```

Built: descriptors, `get`, `set`, `call`, `new`, `Value` and `Object.deserialize` with an `Allocator`. Not built yet: `type_of(T)`, the descriptor of any type.

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

let a = Vec2 { x: 1.0, y: 2.0 };
let b = Vec2 { x: 3.0, y: 4.0 };
let v = a + b;
if a == b { }
```

The names: `add sub mul div rem neg eq lt and or xor shl shr not`. `!=`, `>`, `<=`, `>=` and compound assignments derive. For a struct the operator is a free function in the declaring module.

The same table holds the language hooks: `iter`, `next` and `value` for `for x in e`, and `index` and `set_index` for `e[i]` and `e[i] = v`. A name outside the table is an error that lists the valid names, and a hook with the wrong signature is an error that states the right one. An `operator fn` is also an ordinary method, so `a.add(b)` is `a + b`. `f"..."` writes a class through `to_text`. `operator fn hash(self) -> u64` is the hook of a key of a hashing collection. A struct or class gets a default that hashes its fields in order, and two values equal by `eq` have the same `hash`.

Built: the operators and the hooks `iter`, `next`, `value`, `index` and `set_index`. Not built yet: the hook `hash`.

## Static fields and singletons

A static field belongs to the class and must be atomic. A singleton has one instance.

```anti
class Circle
{
	r: f32 = 1.0,
	static atomic count: int = 0;

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

Atomic operations: `load store add sub and or swap compare_swap`, sequentially consistent. `add`, `sub`, `and`, `or` and `swap` give the value the field held before. In a singleton a plain field is read-only after creation and `atomic` fields are atomic. `mutable` fields may be written anywhere except from a worker, which the compiler checks.

Built.

## Threads

Structured fork-join over an array with `parallel`, and one object at a time with `dispatch`.

<!-- overview: context, docs-style:ignore
```anti
class Sprite
{
	pub x: int = 0,
}
let numbers: [8]int = [1, 2, 3, 4, 5, 6, 7, 8];
let data = numbers[0..8];
let sprite = alloc Sprite { };
let frame = 1;
```
-->
```anti
worker fn sum(chunk: []int) -> int
{
	let total = 0;
	for x in chunk { total += x; }
	return total;
}
let parts = parallel data -> sum;
let parts2 = parallel data by 4 -> sum;

worker fn render(s: *Sprite, frame: int) { }
let job = dispatch sprite -> render(frame);
join(job);
let jobs = [dispatch sprite -> render(frame + 1)];
join_all(jobs[0..1]);
```

Worker parameters are pointer-free. The table pointer and `own` fields do not count. `[]Circle` chunks like any array, `[]*Shape` is refused and points at `dispatch`. `ANTI_THREADS` is gone. `--anti.threads` and the configuration file set the pool size.

`sync m { }` holds a `Mutex` for its block and unlocks it on every exit. `chan T` is a bounded queue of pointer-free values, and `select` waits on more than one channel, written like `switch` over them.

<!-- overview: context, docs-style:ignore
```anti
fn take(x: ?*int) { }
let total = 0;
let a = chan int(4);
let b = chan int(4);
```
-->
```anti
let m = Mutex.new();
sync m {
	total += 1;
}
m.destroy();

let c = chan int(16);
send(c, 42);
close(c);
let v = recv(c) else { return 0; };     // v is *int, none once closed and empty

select {
	a x => take(x),                     // x is ?*int
	b y => take(y),
}
delete(c);
```

A `sync` on the mutex of an enclosing `sync` in the same function is refused. `close` stops what a channel takes, and what it holds is still received. `delete(c)` ends a channel. A worker takes a `chan T` beside its values, and a pointer to a `Mutex`, which cannot be copied.

Built: `parallel`, `dispatch`, `join`, `join_all`, `Mutex`, `sync`, `chan T` with `send`, `recv` and `close`, and `select`. `--anti.threads` and the `threads` key of the configuration file set the pool size, and `ANTI_THREADS` is gone. Not built yet: the warning on a field written inside `sync` and read outside it.

## Concurrent classes

Any value may be read from more than one thread at once. Changing one from more than one thread needs a thread-safe type: `Mutex`, `chan T`, the atomics, or a class declared thread-safe, which the compiler checks.

<!-- overview: context, docs-style:ignore
```anti
struct Person { age: int, }
```
-->
```anti
pub synchronized class Counter
{
	count: int = 0,

	pub fn add(self, n: int) { self.count += n; }
}

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
	tail: ?*Node unchecked(unguarded-field, "swapped with compare_swap"),

	pub fn reset(self) { self.tail = none; }
}

let hits: atomic int = 0;
```

A `synchronized class` gives each object a hidden lock that every public function runs under, released on every exit, and `sync obj { }` holds it for a block. A `concurrent class` leaves the locking to the programmer, and every field is guarded by a `Mutex`, atomic, or fixed after `construct`. `unchecked(unguarded-field, "reason")` after a field's type or in the class header overrules the check, and `compare_swap` takes a field of one word that it marks. A public function of either never gives out a pointer or a slice into the object's fields. A worker may take a pointer to a thread-safe object, and `atomic` marks a local as well. A `Mutex` is one word of the program's memory and cannot be copied. A dev build records the order in which each thread takes locks and reports two orders that conflict. `size_of(Mutex)` is 4 on Linux and macOS and 8 on Windows.

Built: everything above. That covers `synchronized class` with its hidden lock, `sync obj { }`, `concurrent class` with `guarded by`, atomic and fixed fields and the safety check `unguarded-field`, `unchecked` after a field's type and in the class header, the rule on pointers into the fields, atomic locals, the pointer to a thread-safe object in a worker, the one-word `Mutex`, the report of lock orders in a dev build, and `compare_swap` on a field of one word that `unchecked` marks.

## Hooks and tracing

`anti.lang.Object` declares nine hooks with empty bodies, and every class inherits them. `created`, `destroyed` and `copied` are the lifecycle, `dispatched` and `joined` are the threads, and every build compiles the five. `enter`, `leave` and `failed` need tracing, and `changed` needs `--trace writes`. `anti.lang.TraceHandler` declares the same nine with the object after `self`, and `anti.lang.Trace.install(h)` stores one handler.

```anti
class Leaks inherits lang.TraceHandler
{
	atomic live: int = 0,

	concrete fn created(self, o: *Object) { self.live.add(1); }
	concrete fn destroyed(self, o: *Object) { self.live.sub(1); }
	concrete fn copied(self, o: *Object, from: *Object) { self.live.add(1); }
	concrete fn dispatched(self, o: *Object) { }
	concrete fn joined(self, o: *Object) { }
	concrete fn enter(self, o: *Object, name: str) { }
	concrete fn leave(self, o: *Object, name: str) { }
	concrete fn failed(self, o: *Object, name: str, e: *lang.Error) { }
	concrete fn changed(self, o: *Object, field: *FieldDescriptor) { }
}

trace class Renderer
{
	pub fn draw(self) { }
}
```

Every hook site calls the installed handler and then dispatches the object's own hook. `leave` reverses the two, so the handler and the object's hook nest around the call. A class that replaced a hook does not silence the handler, and the handler does not replace the class's hook.

`--no-hooks` drops every site, the five always-on ones as well. Instrumentation is a build option and never a change to an object's layout.

Built: the nine hooks and their entries in the table of every class, `TraceHandler`, `Trace.install`, the order of the two calls with `leave` reversed, the contextual `trace` before `class` and before `fn` in a class body, `--trace`, `--no-trace`, `--trace <pattern>`, `--trace writes` and `--no-hooks`. A hook site is one call of the runtime. `anti.trace` is built with `LeakTracker`, `Profiler`, `CallLogger`, `ErrorMonitor`, `ThreadMonitor`, `ChangeJournal` and `Composite`. `trace.start` reads the runtime key `trace` and installs what it names, and `trace.install` takes the same text from a program. Not built yet: the cost of one load and one compare at a site without a handler.

## Injection

A class declares what it needs, the manifest says who provides it, the run-time configuration may replace it.

```anti
import anti.log;
import anti.mem;

class Renderer
{
	inject log: *log.Logger,
	inject final alloc: *mem.Allocator,
}
```

```toml
[inject]
"anti.log.Logger" = "net.niese.ConsoleLogger.get"
[inject.test]
"anti.log.Logger" = "net.niese.tests.FakeLogger.get"
```

Six standard interfaces ship with defaults: `Logger`, `Clock`, `Source`, `FileSystem`, `Allocator`, `Config`. Standard interfaces for services come with a reference implementation: `anti.db` with SQLite, `anti.http`, `anti.serialize`, `anti.crypto`. They wait for the native libraries of the runtime archive.

A provider is a module function that gives a pointer of the interface, or the `get` of a singleton that implements it. The name of that singleton alone names its `get`.

Built: `inject name: *Interface` and `inject final name: *Interface`. The `[inject]` and `[inject.test]` tables of the manifest reach `antic` as `--inject Interface=Provider`, and `anti test` passes them. One slot per interface holds the provider, and every site calls through it. The link refuses an interface with no provider, a provider that is no function of the program or is no such interface, and a cycle through the providers. The six standard interfaces are built, each an abstract class with a default implementation and a default provider, so an `inject` field of one needs no entry in the manifest: `anti.log.Logger` with `SinkLogger`, `anti.time.Clock` with `SystemClock`, `anti.random.Source` with `SharedRandom`, `anti.fs.FileSystem` with `SystemFileSystem`, `anti.mem.Allocator` with `LibcAllocator` and `anti.config.Config` with `FileConfig`. The default provider of an interface is a static function `default` of it, which the manifest overrides. `anti.mem` also holds `ArenaAllocator`, which hands out memory from blocks and gives them all back at once, and `Object.deserialize` and `f"..."(from)` take an `Allocator`. The language's `alloc` and `free` stay bound to the C library. The run-time replacement and `--closed` are built with plugins. Not built yet: the containers of `anti.collection` that take an `Allocator`, and the interfaces for services.

## Plugins

A shared library declares what it provides. A host loads it and asks by interface. The run-time configuration can replace a provider without the author's help.

<!-- overview: context, docs-style:ignore
```anti
import anti.log;
pub class FancyLogger
{
	implements logger: log.Logger,

	pub concrete fn log(self, level: log.Level, message: str) { }
}
```
-->
```anti
provides anti.log.Logger as FancyLogger;
```

<!-- overview: context, docs-style:ignore
```anti
import anti.log;
import anti.plugin;
```
-->
```anti
let lib = plugin.load("plugins/fancy.so") catch fatal;
let logger = lib.instance(log.Logger) catch fatal;
```

```toml
[injections]
"anti.log.Logger" = "lib/CISOLogger.so"
```

Versions are checked at load: interface hash chain, no fields added, every slot the program reaches present. `--anti.conf`, `--anti.inspect` and the other `--anti.` options are the runtime's and are consumed before `main`.

Built: `provides Interface as Class;` and `antic --lib shared --no-runtime`, which writes a library that links no runtime and is bound against the host that loads it. `plugin.load` gives a `Library`, `lib.instance(Interface)` builds the class it provides, `lib.supports(Interface, "f")` reads its functions, and `lib.unload()` refuses while an object of the library is alive. The load checks the runtime version exactly and that every interface is the host's. The classes of a library join the registry, so `reflect.new` finds one by name. antic writes `anti-plugins.toml` beside a library, with the interfaces, the runtime version and the digest of each library of the directory. `"plugin:path"` and `"discover"` of the manifest take a provider from a library, discovery reads the index and checks the digest, and `[injections]` and `--anti.inject` replace a provider at start. `--closed` builds a program without the exports a plugin binds against. The runtime configuration is built as before: `--anti.conf=<path>`, `ANTI_CONF` and `rt.configure(path)` of `anti.runtime` name the file, the includes run first and in order, every key of `[runtime]` is an option of the same name, `--anti.inspect` prints the effective value of each with the layer it came from, and `--anti.help` lists the options. Precedence per key is the command line, the file, the build. `--anti.inspect` prints the version and the used slots of each injectable interface. The version checks are built: every class descriptor carries the version of the package that declared it, an abstract class carries the chain of its structural hashes and the floor a `compatible <version>;` line names, and a plugin carries the chain, the fields, the size and the version of each interface it was built against. The load compares the two chains at the length of the shorter, refuses a field added between the two versions, refuses a slot the program's calls reach and the library lacks, and fills a slot that `reflect.call` alone may reach with a stub that names the plugin. A program loads a library on macOS, Linux and Windows. On Linux a program that can load one links dynamically against glibc. On Windows its link writes an import library, and a plugin links against the import library of the program that loads it.

## Tests

A `tests` block per module holds tests, a `fixtures` block holds their helpers. Both see the module's private items and are compiled only by `anti test`.

<!-- overview: context, docs-style:ignore
```anti
class Stack
{
	pub top: int = 0,

	pub fn new(size: int) -> *Stack { return alloc Stack { }; }
	pub fn push(self, v: int) { self.top = v; }
}
```
-->
```anti
fixtures
{
	fn full_stack() -> *Stack
	{
		let s = Stack.new(4);
		s.push(1);
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

`setup` and `teardown` in `fixtures` run before and after every test of the module. `anti test` compiles each module with `--tests`, writes a runner that calls its tests, links it and runs it. The dev run links an object of every module the one under test imports, so a module of the standard library carries a `tests` block too. It prints `ok <module>.<test>` for a test that returned, and a failed assertion prints `FAIL <module>.<test>` and then the file and the line. `anti test --release` runs the same tests with the checks and the assertions off.

Built.

## C interop

C functions in, Anti functions out, structs shared without marshalling.

<!-- overview: context, docs-style:ignore
```anti
struct Vector2 { x: f32, y: f32 }
struct Color { r: u8, g: u8, b: u8, a: u8 }
```
-->
```anti
extern fn printf(fmt: ?*byte, ...) -> c_int;
extern fn DrawCircleV(center: Vector2, radius: c_float, color: Color);

export fn dot(a: Vec2, b: Vec2) -> c_int { return a.x * b.x + a.y * b.y; }
export struct Vec2 { x: c_int, y: c_int }

link framework "CoreAudio";
```

`anti bind raylib_api.json` and `anti bind --clang header.h` generate bindings. `anti build --lib static|shared` builds a library with a header. A class is exported with its layout, tables, `anti_<Class>_init`, `anti_<Class>_construct` for a `construct` with arguments, and the dispatch wrappers. `embed("file")` puts a file's bytes in the binary.

Built: `extern`, `export`, static and shared libraries, the header, `link framework`, `link linux`, `anti bind` in its three forms. A Linux program that reaches a `link linux` line links dynamically against glibc 2.35, and every other one statically against musl. Not built yet: `embed`.

## Compile-time targets

`target.os`, `target.cpu` and `target.mode` are constants. A `switch` on them is allowed at module level and must cover every value or carry `else`. The back end keeps one arm.

```anti not-built
switch target.os {
	Os.Windows => {
		extern fn GetTickCount64() -> u64;
	},
	else => {
		extern fn clock_gettime(id: c_int, ts: *Timespec) -> c_int;
	},
}
```

Built: `anti check --targets all`, which runs the front end of every source once per target. Not built yet: `target.os`, `target.cpu` and `target.mode`, and the `switch` on one of them.

## Source locations

`here` is the position it is written at, as a `SourceLocation` with `file`, `line`, `column`, `function` and `module`. As a default parameter value it is evaluated at the call site, which is how a logger reads its caller's line without a macro.

```anti
fn warn(msg: str, at: lang.SourceLocation = here) { }

warn("disk is full");       // at is the caller's position
let p = here;               // the position of this expression
```

The value is constant data. `here` in an ordinary expression gives the position of that expression, which is rarely what a message wants.

Built: `here`, in an expression and as the default of a parameter, and `SourceLocation` in `anti.lang`.

## Checks and debugging

In dev mode every array, slice and `str` index is bounds-checked, signed arithmetic traps on overflow, a narrowing `as` checks its range, a conversion to `char` or to an enum checks the value, division and shifts are checked, `assert` and `show` are active, `-g` writes line information. In release none of it is emitted. `--checks`, `--asserts`, `--trace` and `-g` override.

```anti
trace class Renderer { }
```

`trace` marks a class or function whose `pub` functions call the `enter` and `leave` hooks in dev mode. `--trace <pattern>` instruments code that did not ask. `anti.trace` ships `LeakTracker`, `Profiler`, `CallLogger` and the rest, and `trace.start` installs the one that the runtime key `trace` names. "Hooks and tracing" above holds the nine hooks and the options that decide them.

A release binary carries no symbol data. `anti build --release` writes a symbols archive beside it, and `anti symbols inventory`, `check` and `resolve` collect the archives of a deployment and turn a raw trace into names and lines. `anti.debug.backtrace` captures one at run time, and `--anti.backtrace` turns the frames of an error on in a release build.

The x86_64 baseline for a release build is x86-64-v3. The ARM64 baseline is `armv8.5` on macOS, `armv8.2` on Windows and `armv8.0` on Linux. `--cpu` overrides on every target, and a program refuses to start on a processor below its level. A level is a code-generation setting, not a target. The runtime archive holds one runtime per target and level, so a program below the default links a runtime of its own level. The native libraries are built for the default level alone, and a program below it that imports one is refused at link.

`--memory-checks`, for `antic`, `anti build`, `anti test` and `anti run`, makes a build find a use after free, a double free and a read or write outside a heap block while the program runs, and report the leaks at exit with where each block was allocated. The back end emits the checks of AddressSanitizer around every load and store, and the build links its runtime from the pinned clang. The option is off by default, since the program runs two to three times slower, and is refused on windows-arm64, which has no such runtime.

Built: the checks, with `--checks` and `--no-checks`, `-g`, which writes the line of every statement and keeps the debug sections of the link, the build id in `anti_licenses` of every executable and shared library, the backtraces, with `StackTrace`, `anti.debug.backtrace` and `--anti.backtrace`, and the CPU levels, with `--cpu`, the start-up check and the runtime archive with one runtime per target and level. `symbolize` names the function of a frame in every build and its file and line in a `-g` build. `trace` and the options that decide it are built, and "Hooks and tracing" above names them. The symbols archive of `anti build --release` is built: `<program>-symbols.zip` beside the program, with the same link with its debug sections kept, the map of the program's functions and, on Windows, the PDB. `anti symbols inventory`, `check` and `resolve` are built. They read the runtime configuration and the build id of every binary it reaches, and `resolve` names a frame from the debug link and fills what it lacks from the map. Not built yet: the variables of `-g`, the symbols archive of a shared library and of a plugin, which `anti build` does not write, and the names of a Windows frame in `resolve`, which reads no PDB and leaves the frame raw. Windows has not run a trace. Not built yet: `--memory-checks`.

## Warnings and safety checks

The compiler reports four kinds of problem. An error stops every build and nothing silences it. A warning prints and carries on in a dev build and stops a release build, and `allow(name, "reason")` silences it. A safety check stops every build, and `unchecked(name, "reason")` overrules it. A run-time check traps with file and line in a dev build and is not compiled in release unless `--checks` asks.

```anti
import anti.text;

fn sum_all(lines: []str) -> int may fail
	allow(shadowed-catch, "the handler hands the error on")
{
	let e = 0;
	for l in lines {
		let n = text.parse_int(l) catch e {
			fail e;
		};
		e = e + n;
	}
	return e;
}
```

Every warning and every safety check has a stable name at the end of its message, as in `` `e` shadows the outer `e` [shadowed-catch] ``. `allow` and `unchecked` stand before a statement, last in a declaration's header, or at the top of the file ending with `;`, and `unchecked` also after a field's type. Each takes one name and a required reason, and is not part of a signature. One that silences nothing is the warning `unused-allow` or `unused-unchecked`. `antic --warnings-as-errors` gives the release behaviour in a dev build, and `anti check` uses it. The first safety checks are `unguarded-field` and `exponential-pattern`.

Built: the name of every warning, `allow` and `unchecked` at every level, `unused-allow` and `unused-unchecked`, the refusal of a clause that names an error, `--warnings-as-errors`, `anti check` with it, and a release build that refuses a warning. `docs/notes/warnings.md` lists the names with their meaning and their fix. The safety check `unguarded-field` is built with concurrent classes, and `exponential-pattern` with pattern literals.

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

```anti not-built
let p = alloc Packet(data) catch fatal;
let name = p.body.name;
```

Not built yet.

## Script mode

A file with `#!/usr/bin/env anti` runs directly. `anti file.anti` compiles into a cache keyed by digest and runs.

```anti not-built
#!/usr/bin/env anti
import anti.io;

fn main(args: []str) -> int
{
	for a in args { io.println(a); }
	return 0;
}
```

Not built yet.

## Regular expressions

A pattern literal `re"..."` is raw and has type `Regex`. The compiler checks it, knows its groups and compiles it once before `main`. `Regex.compile(text)` compiles one at run time and may fail. The engine is PCRE2, linked only when a program uses a pattern.

<!-- overview: context, docs-style:ignore
```anti
import anti.io;
import anti.regex;
import anti.text;
let line = "on 2026-09-23, 7, 8";
let input = r"\d+";
```
-->
```anti
let m = line.matches(re"(?<year>\d{4})-(?<month>\d\d)");
if m == none {
	fail "no date";
}
io.println(f"{m.year} / {m.month}");

for m in line.find_all(re"\d+") { }
let parts = line.split(re",\s*").to_slice();
let d = "2026-09-23".replace(re"(?<y>\d{4})-(?<m>\d\d)-(?<d>\d\d)", "${d}/${m}/${y}");

let r = Regex.compile(input) catch fatal;
let n = line.matches(r) catch none;
```

`matches`, `find_all`, `replace` and `split` are methods of `str`. `find_all` and `split` are iterators, and `to_slice()` collects either. `limit` takes that many matches from the start, or from the end when negative. A match behaves as a `?*T` does and stands alone as a condition. Its fields are `all`, `group(n)`, `took_part(n)`, `count`, `pre` and `post`, and for a pattern literal a group is a field, `m.1` or `m.year`. A template names groups with `$1` and `${name}`, and a function may give each replacement. A call with a pattern literal never fails. A call with a compiled pattern may fail with `regex.TooExpensive` or `regex.MissingGroup`. Flags are PCRE2's inline flags, `(?i)`, and `\d`, `\w` and `\s` mean their ASCII sets. A pattern that can take exponential time fails the safety check `exponential-pattern`. No match reads or writes anything global.

Built: `catch none`, as a form of every failing call, the pattern literal `re"..."` with its check at compile time and the safety check `exponential-pattern`, the literals compiled once before `main`, `Regex.compile`, the inline flags, the ASCII meaning of `\d`, `\w` and `\s` with `(*UCP)` for the Unicode one, and the classes `Error`, `BadPattern`, `TooExpensive` and `MissingGroup` of `anti.regex`. A module that writes a pattern imports `anti.regex`. The methods `matches`, `find_all`, `replace` and `split` with `limit` in both directions, the match with its fields, `?Match`, the test of a match in `if`, `while`, `&&`, `||` and `!`, `if let` and `let ... else` on one, the groups of a literal as fields, templates checked at compile time, a function as the replacement, and the match limit with the failure by the origin of the pattern are built. `ByteRegex` and the byte forms of the methods are built, see [Bytes](#bytes).

## Bytes

The same methods work on `[]byte` with a `ByteRegex`, where `.` matches any byte and nothing needs to be valid UTF-8. A pattern literal takes its mode from where it is used. `patch` writes bytes over part of what it finds, in place, and allocates nothing.

<!-- overview: context, docs-style:ignore
```anti
import anti.regex;
import anti.text;
let data = "0123".to_bytes();
```
-->
```anti
data.patch(x"80 10 20 30", x"81");

let v = "version: 1.10.1".to_bytes();
v.patch(re"version: \d+\.\d+\.(\d+)", b"2");
let s = v.to_text() catch fatal;
```

```anti not-built
data.patch(x"80 10 20 30", x"FF", at: 2);
```

`s.to_bytes()` copies a `str` into new bytes, and `data.to_text()` checks for valid UTF-8 and may fail. A match's fields are slices of the searched bytes. `replace` returns new bytes, and `patch` never changes the length, so a `with` that does not fit its span is a compile error or a `LengthMismatch` failure by the origin of its operands. `str` has no `patch`.

Built: `ByteRegex` and `ByteRegex.compile`, the mode of a pattern literal taken from where it is used, the methods `matches`, `find_all`, `replace` and `split` of `[]byte` with the match `ByteMatch`, the class rules of byte patterns, `to_bytes` and `to_text`, and `patch` with a byte sequence or a pattern, `into`, `at`, `limit` and the fit rule. Until named arguments are built, `into`, `at` and `limit` come by position, `data.patch(x"80 10 20 30", x"FF", 0, 2)`. Not built yet: `at: 2` and the other named arguments of `patch`.

## Collections

The collections of `anti.collection` are generic classes used as values, one module per family: `List<T>`, `Deque<T>`, `Ring<T, N>` and `Grid<T>`, `Map<K, V>` and `HashMap<K, V>`, `Set<T>`, `HashSet<T>` and `BitSet`, `SortedMap<K, V>` and `SortedSet<T>`, `Pool<T>` with `Handle<T>`, `Tree<T>` and `PriorityQueue<T>`. `anti.collection` itself holds `Iterable<T>` and `Iterator<T>`. A collection owns its storage and stores its elements by value. It is freed at the end of its block, moves on return, is refused by `=` and is copied by `dup`. No pointer to an element leaves it except through a `lent` parameter: reading gives a copy, as a `?T` where nothing may be there, and `read` and `modify` lend an element to a function.

```anti not-built
import anti.collection.list.{List};
import anti.collection.map.{Map};
import anti.text;

let people = List<Person>.new(capacity: 1000);
people.push(Person { name: "Ann", age: 41 });
let first = people.get(0) ?? Person { };
people.modify(0, fn(p) { p.age += 1; });
let n = people.update_all(fn(p) { return p.age >= 65; }, fn(p) { p.retired = true; });
let ann = people.find_one(fn(p) { return text.equal(p.name, "Ann"); }) catch fatal;

let ages = Map<str, int>.new(from: &arena);
ages.set("Ann", 41);
let age = ages.get("Ann") ?? 0;
for (k, v) in ages { }
```

Criteria come in three forms: `_all` acts on every match and gives the count, `_first` on the first match in the collection's order, and `_one` on the one match, failing when none or more than one matches. Every collection takes an optional allocator, `from: &arena`, and one that grows takes an initial `capacity`, with `reserve(n)` and `shrink()`. `Map` walks in insertion order, the same on every run. `SortedMap` is a B-tree and walks in key order. `HashMap` walks in an order that differs on every walk, so no program depends on it. A hashing collection mixes its hashes with a seed chosen at start. `Ring<T, N>` allocates nothing after it is made, for real-time code. A `Pool` keeps its elements at fixed addresses and gives handles of a slot and a generation, so a stale handle finds nothing, and a `Tree` holds a hierarchy on the same handles.

The thread-safe collections of `anti.collection.sync` and `anti.collection.concurrent` follow [Concurrent classes](#concurrent-classes). `SyncList<T>`, `SyncMap<K, V>`, `SyncSet<T>` and `SyncPool<T>` are `synchronized`. `ConcurrentMap<K, V>` is `concurrent`, split into parts locked apart. `SpscRing<T, N>` passes values from one thread to one other with two atomic counters and no lock. They have no operation by position and no `lend_slice`, and every one has versioned `set`. A queue between threads is `chan T`.

Built: `List`, `Map` and `IntMap` of `anti.collection`, which hold `*Object` values under no key, a `str` key and an `int` key. Not built yet: every collection of this section, its thread-safe versions and the modules under `anti.collection`.

## Standard library

The modules under `anti.` ship with the compiler, and a program imports them without `-I`. `anti.lang` is the root and imports nothing. It holds `Error`, `NoneDereference`, `SourceLocation`, `StackTrace`, `Trace` and `TraceHandler`, and the compiler declares `Object`, `Job`, `Flags`, `Mutex` and `FieldDescriptor` there.

```anti
import anti.io;
import anti.text;

let n = text.parse_int("42") catch fatal;
let s = f"{n} items";
io.println(s);
free(s.ptr);
```

Built: `anti.lang`, `anti.io`, `anti.text`, `anti.license`, `anti.error`, `anti.time`, `anti.os`, `anti.fs`, `anti.reflect`, `anti.random`, `anti.collection`, `anti.toml`, `anti.config`, `anti.args`, `anti.json`, `anti.log`, `anti.debug`, `anti.mem`, `anti.runtime`, `anti.simd`, `anti.trace`, `anti.plugin` and `anti.regex` over PCRE2. Not built yet: the other modules over the native libraries of the runtime archive, `anti.net`, `anti.raylib` and `anti.miniaudio`, the interfaces for services with `anti.db` over SQLite, and `anti.binary`, which the code of a wire format uses, the modules of [Collections](#collections) under `anti.collection`, `Shared<T>` of `anti.mem`, and `Ordered` of `anti.lang`.

## Later

Round five, generics and collections, follows round four, as "Timing" in `docs/anti-language-additions.md` orders it, with generics first. [Generics](#generics), [Optional values](#optional-values), [Direct imports](#direct-imports) and [Collections](#collections) hold it. The syntax of generics, its constraints, the compiled copies and the generics of library files and of C are built, and none of the rest. With generics come `anti.collection.Iterable<T>` and `Iterator<T>`, which a class with the `iter` hook implements. Closures are built, in [Anonymous functions and closures](#anonymous-functions-and-closures).

## Reserved words

Keywords: `fn extern let const struct union enum variant class import pub internal protected export if else switch while do for break continue return defer undo try catch yield fail assert show unreachable undefined embed here fallthrough as is dup delete destroy alloc free size_of self super abstract concrete static singleton inherits implements use worker parallel dispatch join join_all sync chan send recv select atomic true false none tests fixtures provides constraint type`.

Contextual words: `packed align by in final own transient operator mutable trace inject compatible simd`, `fatal` and `none` after `catch`, and `may fail` after a signature. Round four adds `snapshot keep concurrent synchronized unchecked allow` and `guarded by`, and round five adds `lent`. `alloc` and `free` name a function of a class after `fn` and a member after `.`.

String prefixes: `r b br f rf x re`.

Types with the aliases: the sized numbers, `int uint float byte bool char str`, and the `c_` types. Built-in functions: `mul_high`.

Built of round four: `keep`, `keep own`, `concurrent`, `snapshot`, `unchecked` and `allow`. `synchronized` and `guarded by` are built as well. Not built yet: `show`, `unreachable`, `undefined` and `embed`, which the lexer reads as names today. Built of round five: `constraint` and `type`. Not built yet: `lent`, which the lexer reads as a name today.
