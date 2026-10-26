# Anti syntax overview

Every feature of the language in one place, each with what it does and an example where one helps. The rules are in `docs/decisions.md`, `docs/anti-object-model.md` and `docs/anti-language-additions.md`. This overview follows them and adds nothing. Where a feature is not built yet, its section says so.

The test `overview_examples` runs every `anti` block of this page through the front end of antic, so an example that stops compiling fails the suite. A block that declares `main` is built and run as well, and the `text` block after it holds what the program prints. A block that opens with `anti refused` shows a program that antic refuses, and its line `//! error:` holds the text of the message the test expects. A block that opens with `anti not-built` shows a feature that is not built yet, and the test passes over it. The last sentence of its section names that feature. A hidden block before an example declares the names the example uses without showing them. The test writes `import anti.lang;` before every block that lacks it, so a block names `lang.Error` without the import. A file of its own writes that line.

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
- [Runtime configuration](#runtime-configuration)
- [Tests](#tests)
- [C interop](#c-interop)
- [Compile-time targets](#compile-time-targets)
- [Source locations](#source-locations)
- [Checks and debugging](#checks-and-debugging)
- [Processor levels](#processor-levels)
- [Warnings and safety checks](#warnings-and-safety-checks)
- [Wire formats](#wire-formats)
- [Script mode](#script-mode)
- [Regular expressions](#regular-expressions)
- [Bytes](#bytes)
- [Collections](#collections)
- [Standard library](#standard-library)
- [Tools](#tools)
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

```text
hello
```

`main` has one of three signatures: `fn main() -> int`, `fn main(args: []str) -> int`, `fn main(args: []str, env: []str) -> int`. Comments are `//` to the end of the line and `/* */`, which do not nest. `///` documents the next item and `//!` the module, and `//#` and `//#!` do the same for the developers of a library. The block forms `/** */`, `/*! */`, `/*# */` and `/*#! */` mean the same. `////`, `/**/` and a comment that opens with `/***` stay ordinary comments.

```anti
/*! The squares of the program. */

/// The area of a square of side `n`.
pub fn area(n: int) -> int { return n * n; }

/** The perimeter of a square of side `n`. */
pub fn perimeter(n: int) -> int { return 4 * n; }

//# The side of the test square, for the developers of the module.
fn side() -> int { return 2; }

//// An ordinary comment.
```

Source is UTF-8, and a byte order mark at its start is skipped. Identifiers are ASCII letters, digits and `_`, and UTF-8 stands in literals and comments. A CR LF line end in the text of a literal reads as LF. Indentation is a tab. An item body opens `{` on its own line, a statement block opens `{` on the statement's line.

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

`pub` exports to every module, `internal` to modules under the same package root, none to the file. `internal` marks an item of a module, and `protected` a member of a class. `import x as y` renames the local name. The `import` lines come first in a file, and an `allow` or `unchecked` clause for the whole file stands before them or among them. Paths under `anti.` are the language's. Third parties use a root they own. A segment of a path is an identifier and never a keyword, so a module `app.here` cannot be imported.

An import reads the library file that `antic -c` wrote for the module. `-I <dir>` names a search root, under which module `a.b` is `a/b.anti` and its library file `a/b.antl`, and the standard library is the last root. Two modules that import each other are refused, and the message names the cycle. A module is imported once in a file, so a plain import and a direct import of the same module are refused:

```anti refused
//! error: `text` is already declared
import anti.text;
import anti.text.{equal};
```

Some forms of the language need a module of the standard library, and the message of each names the import. `may fail`, `here` and `catch` on a `?*T` need `anti.lang`. `f"..."` and a `switch` on a `str` need `anti.text`, a pattern literal needs `anti.regex`, and `Object.deserialize` and an allocator argument of `destroy` and `delete` need `anti.mem`.

`import anti.collection.map.{Map, HashMap};` makes the listed names of a module visible in the file without the module's name, so code writes `Map<str, int>` rather than `map.Map<str, int>`. Each listed name is a public item of that module, and one that clashes with a name already visible in the file is refused, naming both. The module stays reachable by its name, as with a plain `import`. `anti fmt` keeps the list sorted. `import anti.regex.{Regex};` is refused, since `Regex` is declared in `anti.lang` and is no item of `anti.regex`.

```anti
import anti.collection.map.{HashMap, Map};
import anti.io.{println};
import anti.text.{Builder, equal};

let ages = Map<str, int>.new();
let seen = map.HashMap<str, bool>.new();
let b = Builder.new();
b.append("one");
if equal(b.text(), "one") && text.equal("a", "a") {
	println(b.text());
}
```

Two other lines stand at module level. `link framework "CoreAudio";` names a framework of Apple's SDK, and `link linux "X11";` a library of the glibc sysroot, see [C interop](#c-interop).

## Types

Sized numbers `i8 i16 i32 i64 u8 u16 u32 u64 f32 f64`, with `int` for `i64`, `uint` for `u64`, `float` for `f64`, `byte` for `u8`. `f16` is storage only, read as `f32` and written with `as f16`. `bool`. `char`, a 32-bit Unicode scalar. `str`, immutable UTF-8, pointer plus length, NUL-terminated outside its length. A file name the system gives keeps its bytes where they are no UTF-8, so it still opens the file. `==` compares the text of two `str` values, `<` compares their bytes, which for UTF-8 is the order of the code points, and a `str` meets `eq`, `Ordered` and `hash`. Fixed arrays `[N]T`. Slices `[]T`, pointer plus length. Pointers `*T` and nullable pointers `?*T`. Optional values `?T` of any type, and `??T` is a `?T` whose value is a `?T`. Function pointers `fn(i32) -> i32` and nullable ones `?fn(i32) -> i32`. Tuples `(int, str)`, anonymous structs with C layout. Structs, enums, variants, classes. No implicit conversions between numbers.

A slice and a `str` have `.len`, an `int`, and `.ptr`, a `?*T` or a `?*byte`, both read-only. An array has `.len` alone. The length `N` of `[N]T` is a constant `int` of at least 1. A length computed from `size_of` stands in the type of a field or a local alone, and the `.len` of such an array is no constant. `s[i]` on a `str` is a `byte`, and `s[lo..hi]` is a `[]byte`. `a[lo..hi]` on an array or a slice is a `[]T`, and both bounds are required.

The compiler declares `Object`, `Job`, `Flags`, `Mutex`, `chan T`, `Regex`, `ByteRegex`, `Match`, `ByteMatch`, `FieldDescriptor` and the constraint `Number` in `anti.lang`, and the source of `anti.lang` declares the rest of the root. The C types for bindings are `c_char c_uchar c_short c_ushort c_int c_uint c_long c_ulong c_longlong c_ulonglong c_size_t c_float c_double c_wchar`. `c_long`, `c_ulong` and `c_wchar` take the width of the target. A `void *` of C is a `*byte`, and a `?*byte` where it may be `NULL`.

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
let count = s.len;
let name = "tea";
let first = name[0];
let rest = name[1..3];
let again = []int { ptr: s.ptr, len: s.len };
let twice: ??int = none;
let word: [size_of(i64)]byte = [0; size_of(i64)];
```

## Literals

Integers in decimal and hex with `_` separators, and no `0b` or `0o` prefix. A decimal literal other than `0` does not start with `0`. Floats with digits on both sides of `.` and an optional exponent, `1.0e9`, so `1e9` is no literal. Characters `'q'` with the escapes of a string. Strings `"..."` with the escapes `\n \r \t \\ \" \' \0 \xHH \u{H}`, where `\u{}` takes one to six hex digits. In a `str` or a `char` `\xHH` stops at 7F and `\0` is refused, and a byte string takes both. `r"..."` raw, `b"..."` bytes, `br"..."` raw bytes. Hash delimiters `#"..."#` on every form but `x"..."`, for quotes inside. Interpolation `f"..."` with format specifications, and `rf"..."` for interpolation without escapes. The text of an `f"..."` is memory of the C library, freed with `free(s.ptr)`, and text that should live in an `ArenaAllocator` is built with `anti.text.Builder`, whose `take_in(from)` takes the allocator. Bytes in hex `x"00 AB CC"`. A pattern literal `re"..."`, see [Regular expressions](#regular-expressions). The prefixes are `r`, `b`, `br`, `f`, `rf`, `x` and `re`, one meaning each, and `fr"` is refused with a message that names `rf"`. Arrays `[1, 2, 3]` and the repeat `[v; n]`, which copies `v` into `n` elements. A slice from a pointer `[]T { ptr: p, len: n }`. `true`, `false`, `none`. Literals take their type from context, and without one an integer literal is an `int` and a float literal a `float`.

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
let big = 1.0e9;
let q = 'q';
let nl = '\n';
let d = r#"a "quoted" string"#;
let e = b"\x00\x01";
let g = f"{name:>10} costs {price:8.2f}";
let h = rf"C:\tools\{name}";
let i = x"00 AB CC";
let j = br#"raw "bytes""#;
let x: i8 = -128;
let zeros = [0; 16];
let pair = [1, 2];
```

A format specification after a colon is `[align][0][width][.precision][kind]`, each part optional and in that order. The alignment is `<`, `>` or `^`, and a width without one puts a number on the right and other text on the left. A `0` pads a numeric value to its width with zeros and stands without an alignment. The kinds `x`, `X`, `b` and `o` write an integer in hex, in upper-case hex, in binary and in octal, and `e` and `f` a float with an exponent and without one, with 6 digits unless a precision is given. A precision takes a float alone. `{{` and `}}` write a brace. A value in `{expr}` is a number, a `bool`, a `char`, a `str`, a class or a `*T` of a class, which writes itself through `to_text`, and every other type, an enum and a `?*T` among them, is refused. An `f"..."` is no constant expression.

```anti
import anti.io;
import anti.text;

fn main() -> int
{
	let n = 255;
	let x = 3.14159;
	let s = "ab";
	io.println(f"{n:x} {n:X} {n:b} {n:o} {n:08}");
	io.println(f"[{s:<5}] [{s:>5}] [{s:^6}]");
	io.println(f"{x:.2} {x:e} [{x:10.3f}] {{n}}");
	return 0;
}
```

```text
ff FF 11111111 377 00000255
[ab   ] [   ab] [  ab  ]
3.14 3.141590e+00 [     3.142] {n}
```

```anti refused
//! error: `fr"` is not a prefix, write `rf"`
let path = fr"C:\tools";
```

## Variables and constants

`let` declares a mutable local with mandatory initialisation, and `let x: atomic int = 0;` an atomic local. `const` declares a constant expression at module level, in a block and in a class body, where `Name.NAME` reaches it. A module holds no variables: a `let` stands in a body alone, and the one global that a program writes is a `static atomic` field of a class, see [Static fields and singletons](#static-fields-and-singletons). A constant whose `+`, `-`, `*` or unary `-` leaves the range of its type is refused, and `+%` and `+|` name a result outside it. Shadowing in an inner block is allowed. `let (a, b) = e;` takes a tuple apart, see [Tuples](#tuples), and `let m = p else { }` binds a checked pointer, see [Pointers](#pointers). `undefined` opts one local out of initialisation.

```anti
class Limits
{
	pub const MAX: int = 64;
}

let x = 5;
let y: f32 = 1.0;
const STEP: int = 2;
let hits: atomic int = 0;
let (lo, hi) = (1, 2);
let m = Limits.MAX;
```

```anti not-built
let buf: [4096]byte = undefined;
```

Not built yet: `undefined`.

## Operators

C precedence, with the differences below. Arithmetic `+ - * / %`, comparison `== != < <= > >=`, logic `&& || !`, bits `& | ^ ~ << >>`, unary `- ! ~`, address `&p` and dereference `*p`. Conversion `x as T`, which does nothing where `x` has the type `T` already, checked downcast `p as *T`, nullable downcast `p as? *T`, whose `?` stands at `as`, type test `p is *T`, and `v is Shape.Circle` on a variant, `geo.Shape.Circle` for a variant of another module. Wrapping `+% -% *% <<%` and saturating `+| -| *|`, which bind as the operator they extend. Coalescing `??` and chaining `?.` over `none`. Ranges `lo..hi`, half-open. `x in lo..hi`. Compound assignment `+=` and the rest, `+%=` and `+|=` among them, on numbers and lane by lane on a simd struct. A compound assignment calls no `operator fn`, as [Operators on classes](#operators-on-classes) gives. No `++`, no `?:`.

| Binds | Operators |
|---|---|
| loosest | `\|\|` |
| | `&&` |
| | `\|` |
| | `^` |
| | `&` |
| | `== !=` |
| | `< <= > >=` and `in` |
| | `??`, from the right |
| | `<< >> <<%` |
| | `+ - +% -% +\| -\|` |
| | `* / % *% *\|` |
| | `as`, `as?` and `is` |
| tightest | `- ! ~ & *` before an operand |

`as` binds tighter than every binary operator and looser than the unary ones, so `-x as u8` is `(-x) as u8`, and `is` binds as `as` does. `??` groups from the right, `a ?? b ?? c` is `a ?? (b ?? c)`. `in` binds as `<` does, and each bound takes the operators that bind tighter, so `i + 1 in 0..n * 2` needs no parentheses.

<!-- overview: context, docs-style:ignore
```anti
struct Node { value: int, next: ?*Node }
variant Shape { Circle { r: f32 }, Empty }
let a = 7;
let b = 2;
let c = 'q';
let fallback = Node { value: 0, next: none };
let maybe: ?*Node = none;
let node: ?*Node = &fallback;
let shape = Shape.Empty;
```
-->
```anti
let q = a / b;
let neg = -a;
let w = a +% b;
let s = a +| b;
let p = maybe ?? &fallback;
let v = node?.next?.next;
let addr = &a;
let back = *addr;
if c in 'a'..'z' { }
let (sum, f) = a + b;
if f.carry { }
let round = shape is Shape.Circle;
```

`/` on integers truncates toward zero, `%` takes the sign of the dividend, and `>>` is arithmetic on a signed type and logical on an unsigned one. `b as int` of a `bool` gives 1 or 0, and no number converts to `bool`, so a test writes `n != 0`. The operands of an operator and the arguments of a call run from left to right, and an assignment computes its place before its value.

```anti
import anti.io;
import anti.text;

fn side(name: str, v: int) -> int
{
	io.println(name);
	return v;
}

fn main() -> int
{
	let a = -7;
	io.println(f"{a / 2} {a % 2} {7 % -2} {a >> 1}");
	io.println(f"{true as int} {false as int}");
	let n = side("left", 1) + side("right", 2);
	let xs: [2]int = [0, 0];
	xs[side("place", 1)] = side("value", n);
	return 0;
}
```

```text
-3 -1 1 -4
1 0
left
right
place
value
```

`let (result, flags) = e;` binds the wrapped result and a `Flags` struct with `overflow`, `carry`, `zero` and `negative`. A carry in is `a + b + f.carry`. `mul_high(a, b)` gives the upper half of the full product.

## Statements

`if`, `else if`, `else`. `if let x = e { }` binds the value of a `?T`, or the fields of a variant case as `if let Circle c = s { }`, and `else if let` continues a chain. `switch` names one value per arm, and an enum value with its type, `Kind.Circle`. Two arms of one value are refused, also where one is written as `size_of(i64)` and the other as `8`. An arm is a call, an assignment or a block, and one `else` takes the rest. It falls through only where an arm ends in `fallthrough;`. Assignment is a statement, and `*p = f() catch e { }` handles the error of the call that fills a place. `(r, f) = e;` assigns both parts of the flags form of [Operators](#operators) to two variables, where `e` is a `+`, `-`, `*`, `<<`, `>>` or unary `-`. Blocks `{ }` are statements. `defer` and `undo` each hold one statement. A `defer` whose statement is another `defer` is refused, and so is an `undo` inside a `defer` or inside another `undo`. `assert(cond)` and `assert(cond, "message")`, whose message is a string literal. `yield v;` and `yield;` end the handler of a `catch`, see [Errors](#errors).

<!-- overview: context, docs-style:ignore
```anti
import anti.fs;
import anti.text;
enum Kind { Circle, Square, Rect }
variant Shape { Circle { r: f32 }, Empty }
fn draw_circle() { }
fn draw_box() { }
let x = 3;
let y = 0;
let n = 1;
let kind = Kind.Square;
let shape = Shape.Empty;
let maybe: ?int = none;
let slot = &y;
let path = "out.txt";
let f = try fs.open(path, fs.Mode.Write);
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

if let v = maybe {
	y = v;
} else if let Circle c = shape {
	y = 2;
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
*slot = text.parse_int("7") catch { yield 0; };
fs.flush(f) catch { yield; };

let (sum, carried) = x + n;
(sum, carried) = sum + n;
```

```anti not-built
let v = show(compute(x));
if x < 0 { unreachable; }
```

`switch` takes an enum, an integer, a `char`, a `str` or a variant, and a `bool` or a value of a type parameter is refused. A `switch` on a `char` takes `char` literals as its arms and compares them as integers. `switch` on an enum without `else` must cover every value. `switch` on a `str` is a comparison chain. `fallthrough;` as an arm's last statement enters the next arm's body without testing its values, and is refused in the last arm and into an arm that binds a variant's fields. A `switch` whose every arm and `else` end the path ends the path of its function. An arm ends it with `return`, `fail`, a call that never returns, or a `break` or `continue` of an outer loop. Without an `else` the `switch` is on an enum or a variant, where it names every value. A `while true` without a `break` of its own ends the path as well. So do a `do ... while` whose body ends it and holds no `break` or `continue` of its own, a `select` whose every arm ends it and a `try` block whose body and handler both end it. The `else` of a `let` may end with any of them. `defer` runs at every exit of the block, `undo` only on an error exit. An `undo` in the block of a `defer` or of another `undo` is refused, and one in a function that cannot fail, outside a `try` block, is the warning `undo-never-runs`. `return`, `break`, `continue`, `fail` and `try` inside a `defer` or an `undo` are refused, apart from a `break` or a `continue` of a loop inside the same statement. A `yield` inside a `defer` or an `undo` of its handler is refused. `assert` and `show` vanish in release, and a call in the condition of an `assert` is the warning `assert-call`. `unreachable` traps in dev and is undefined in release. Not built yet: `show`, `unreachable` and labels on blocks.

## Loops

`while cond do { }` for zero or more, `do { } while cond` for at least one, whose condition ends with its line. `for` over ranges and slices with an optional binding and a constant step, and over collections and iterators. Labels for `break` and `continue`. Everything else is a `while`.

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

`for x in c` gives a copy of each element, and the loop variable is read-only, so a change that would reach the copy alone is refused. `for x in &c` gives each element as a `lent` pointer for one turn of the loop, and a change through it reaches the element. Both forms hold for slices and for the collections of [Collections](#collections). The iterator decides what walking with `&` gives: its `value` returns what the loop binds, a `lent *T` for a list and a `(K, lent *V)` for a map. A tuple holds a lent pointer only as such a value, bound by its loop. The loop owns every part it receives by value and tears it down at the end of each turn, on every exit of the body, and never tears down a lent part. `for (k, v) in m` takes apart the tuple of each element, and its parts are what the value type says, so `for (k, v) in &m` over a map gives `k` as a copy and `v` as a lent pointer. A collection must not change its size or move its room, as `reserve` and `shrink` do, while a loop walks it. A dev build traps when it does, naming the collection and both places, and `remove_all(test)` removes while it walks.

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
	operator fn value(self) -> lent *Person { return &self.list.all[self.at]; }
}
let people = People { };
```
-->
```anti
for p in people { }       // p is a copy of each Person, read-only
for p in &people {
	p.age += 1;           // p is a lent *Person
}
```

Not built yet: labels.

## Functions

`fn name(params) -> R { }`. `return;` in a function without a result. `-> never` for a function that ends the program on every path. Default parameter values and named arguments. Function values and bound functions. `extern fn` declares a C function, takes `...` for a variadic one, and refuses `may fail` and `export`, see [C interop](#c-interop).

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

`-> never` refuses a reachable end and a `return` in the body, and no value has the type. Its call ends the path of the caller as `return` does.

```anti
extern fn exit(status: c_int) -> never;

fn half(n: int) -> int
{
	if n % 2 == 0 { return n / 2; }
	exit(3);
}
```

Default values are a constant expression or `here`, over a module boundary as well. A parameter with a default stands after every parameter without one, and an `extern fn` takes no default, since C has none. A default belongs to the function and not to its type, so a call through a function value gives every argument:

```anti refused
//! error: the call takes 2 arguments, found 1
fn scaled(n: int, by: int = 2) -> int { return n * by; }

let f = scaled;
let k = f(4);
```

Not built yet: named arguments.

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

An anonymous function stands in a function body alone, never at module level. `break` and `continue` inside it reach a loop inside it alone, and never one around it. A captured `?*T` keeps its declared type inside the closure, since the closure may run after the test that narrowed it. The closure tests it again:

```anti refused
//! error: `p` may be `none`, check it or use `?*T`
struct Node { value: int }
fn each(f: fn()) { f(); }

let a = Node { value: 1 };
let p: ?*Node = &a;
if p != none {
	each(fn() { let v = p.value; });
}
```

A closure never outlives what it captures. It is passed to a parameter that does not keep it, or held in a local used the same way, and never stored in a field, a return value, a global or a `keep` parameter. Creating one allocates nothing. A parameter that passes its function on to `parallel` or `dispatch` is marked `concurrent`, and a closure there may change a captured variable only when its type is thread-safe. A worker takes a closure through a `concurrent` parameter alone. `snapshot fn` copies what it uses when it is made, is read-only, and holds numbers, `bool`, `char`, structs of those and `str`. It is accepted at an `own` field, a `keep own` parameter and a `concurrent` parameter, and a plain `keep` parameter refuses it. A parameter that does not keep its argument is two words, the code and a context pointer, and a plain kept function value stays one C function pointer. `own fn` is two words as well, the code and a snapshot on the heap that its owner frees. `=` does not copy it, and `dup` copies it with its snapshot. An `own` field of type `?fn(...)` takes `none`, as its default and with `=`, which frees the snapshot the field held. The header writes the two words of a parameter as a callback and a `void *` context, and an `own fn` field as a struct of the code and the snapshot. An `extern fn` takes C function pointers alone.

## Tuples

An anonymous struct with C layout, for a function with two answers and no name for the pair.

<!-- overview: context, docs-style:ignore
```anti
let items: [3]str = ["a", "b", "c"];
let pairs: [2](int, str) = [(1, "a"), (2, "b")];
```
-->
```anti
fn divmod(a: int, b: int) -> (int, int) { return (a / b, a % b); }

let t = divmod(7, 2);
let q = t.0;
let (d, r) = divmod(7, 2);
for i, x in items { }
for (n, name) in pairs { }
```

Elements are `t.0`, `t.1` and on. Destructuring is `let (a, b) = e;` and `for (a, b) in items`, which takes each element apart, and nowhere else: not in a parameter list, not in an assignment and not nested. `for i, x in items` binds the index beside the element, and `(r, f) = e;` is the flags form of [Operators](#operators) alone. Two tuple types are the same when their element types are the same in order. A tuple of more than three elements, or one that crosses a module boundary, is a struct that has not been named yet.

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

A handler ends with `yield v` or leaves the block. The name after `catch` is any identifier, scoped to the handler, and `catch { }` binds none. The error is deleted on every exit of the handler unless `return e` or `fail e` hands it on or a call moved it into an `own` parameter. `Error.wrap` takes its cause so. `fail "text"` is `fail Error.new(0, "text")`. `fail` of any other `str`, an `f"..."` among them, makes an error of code 0 that owns a copy of the text. `try` forwards the error and is refused outside a function that may fail. Inside a `try` block it forwards to the handler of that block. That handler takes `yield;`, which ends it, and a `yield` belongs to the innermost handler that holds it. It stands wherever the call stands, after `return` and inside an expression as well. `undo` runs on the `fail` path and not on `return`. `Error` has `code`, `message`, an owned `cause`, the origin `at` that `fail` fills, and the frames of a trace when backtraces are on. It lives in `anti.lang`, the root of the standard library, which imports nothing. `e.text()` gives `error N: message`, or the message alone for code 0, with each cause on a line of its own, and the error keeps those bytes until it is deleted. `e.print()` writes that text to the standard error, and `e.fatal()` prints it and ends the program with `e.code` as its exit status, or 1 for code 0. `catch fatal` calls it, and in a worker it ends the whole program without running a `defer`. Libraries subclass it and callers test with `is`. `anti.error` adds `SystemError`, with `from_errno()` and `from_win32()`, and `on_fatal` and `check`.

The ABI of a `may fail` function is `?*Error f(args, R *out)`, with the result through an out pointer. `fn(A) -> R may fail` is its type as a value, a call through it is handled as a direct call is, and the header writes the ABI form. The compiler supplies that pointer over storage whose table it zeroes. The binding is destroyed at the end of its block like any other local. `may fail` is the one failing form. A C binding returns its error as an ordinary value, and a `may fail` wrapper turns it into a `fail`.

A function fails by its `may fail` marking alone, and one that returns `*Error` or `?*Error` without it gives the error as a value. A `construct` with arguments that can fail is written `may fail`, and one written `-> ?*Error` is refused. The first `fail` of an error writes its origin `at` and, when backtraces are on, its frames, and `e.text()` names the position, the causes and the trace. `--anti.backtrace` and `backtrace = true` of the runtime configuration turn the frames on in a release build. The checker warns where the name a `catch` binds shadows a variable in scope, `shadowed-catch`, and where a `may fail` function holds no `fail` and no `try`, `never-fails`. `catch none` deletes the error as every handler does. `destruct` cannot fail, since a teardown has no caller to hand an error to:

```anti refused
//! error: `destruct` cannot fail
class Log
{
	pub lines: int = 0,

	fn destruct(self) may fail { }
}
```

The failing functions of the standard library stand in the table of [Standard library](#standard-library).

## Structs

Fields only, exactly the bytes C declares. Every field is visible. Method-call sugar for free functions in the declaring module. Structs are nominal: two declarations are two types, and a literal of a struct of another module names it as `geo.Rect { }`.

```anti
struct Rect
{
	x: f32,
	y: f32,
	w: f32,
	h: f32,
}

packed struct Header
{
	kind: u8 : 4,
	flags: u8 : 4,
	_: u32 : 0,
	length: u32,
}

union Word
{
	whole: u64,
	halves: [2]u32,
}

fn area(r: *Rect) -> f32 { return r.w * r.h; }

let r = Rect { x: 0.0, y: 0.0, w: 8.0, h: 4.0 };
let a = r.area();       // area(&r)
let h = Header { kind: 1, flags: 2, length: 3 };
let w = Word { whole: 1 };
let bytes = size_of(Rect);
let at = lang.SourceLocation { file: "a.anti", line: 1, column: 1, function: "main", module: "app" };
```

A literal of a struct names every field exactly once, since a field of a struct has no default, which belongs to a class:

```anti refused
//! error: the literal of `Point` misses the field `y`
struct Point { x: int, y: int }

let p = Point { x: 1 };
```

`r.f()` on a struct with a field `f` of function type calls the field, and a free function `f` of the module stands back. Method-call sugar does not reach a union, whose functions are called as `f(&u)`.

`packed` before `struct`, `union`, `class` or `variant` removes padding, apart from the padding before an atomic field or a field that holds a Mutex, and `align(N)` after the name of any of the four raises its alignment. N is a constant power of two up to 4096, and a larger N is refused. `union` has C layout, and a literal of one names exactly one field. Bitfields `flags: u32 : 4`. A constant written to a bitfield fits its width, `+%=` wraps and `+|=` saturates at that width, and a dev build stops any other value that does not fit. `_: T : 0` is a zero-width bitfield, a unit break as in C, and a union holds none. A trailing comma is allowed in every comma-separated list. `size_of(T)` is the size on the target.

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

```anti refused
//! error: `%` does not apply to a `simd struct`
simd struct Vec2 { x: f32, y: f32 }

let a = Vec2 { x: 5.0, y: 7.0 };
let r = a % Vec2.splat(2.0);
```

Arithmetic, the bitwise operators on integer lanes, comparisons and unary minus apply lane by lane. So do the compound assignments `+=`, `-=`, `*=` and `/=`, and `&=`, `|=`, `^=`, `<<=` and `>>=` on integer lanes, as `v += w`. `+`, `-` and `*` on integer lanes wrap, and no build checks them for overflow. `%`, the wrapping and saturating operators, `&&`, `||` and `!` do not apply to a simd struct. A comparison yields a mask, a `simd struct` of `bool` with the same lane count. `simd.select`, `simd.any` and `simd.all` are in `anti.simd`. `splat`, `load`, `store`, `shuffle`, `sum`, `min`, `max` and `dot` are built in. `as` between a `simd struct` and the array or plain struct of the same bytes is free. The back end maps each operation to the target's native width, so the lane count is the programmer's and the instruction count is the machine's. An `f32x8` is one instruction at x86-64-v3 and two everywhere else. A `simd struct` of 16 bytes is the vector type of C in the header and passes in a vector register, and one of another size passes as the struct of its lanes. Above the vector cap it is an array and a loop, which the warning `above-vector-cap` names at its declaration. The built-in operators meet the constraints `add`, `sub`, `mul`, `div` and `neg` lane by lane, so `twice<T: add>` takes a `Vec4`. A simd struct does not meet `lt`, and so it does not meet `Number`.

## Enums

A named integer type with C layout, over `c_int` unless the declaration names the type. A value without `=` follows the one before it, and every value fits the underlying type. The expression after `=` names the values before it by their bare names, as numbers of the underlying type, `All = Read | Write`, and a later value is refused. `as` converts an enum to any numeric type and an integer to an enum, and a dev build checks a conversion to an enum. A float and a value of another enum convert to an enum through an integer. `==` and `!=` compare two values, and `switch` takes them with their type.

```anti
enum Kind { Circle, Square, Empty }
enum Mode: u8 { Read = 1, Write = 2 }

let k = Kind.Circle;
let n = k as int;
let m = 2 as Kind;
if k != m { }
```

An enum without values compiles, with the size of `c_int`. Two values may share a number, and a `switch` refuses two arms that name them both. An enum body takes functions and constants after its values. A function there takes `self` as a pointer to the enum and is called as `Speed.rank(&s)`, since method-call sugar does not reach it, and a constant is `Speed.LIMIT`.

```anti
import anti.io;
import anti.text;

enum Empty { }
enum Code { Ok = 0, Success = 0, Failed = 1 }

enum Speed
{
	Fast,
	Slow,

	fn rank(self) -> int
	{
		if *self == Speed.Fast { return 1; }
		return 2;
	}

	const LIMIT: int = 3;
}

fn main() -> int
{
	let s = Speed.Slow;
	io.println(f"{size_of(Empty)} {Code.Ok == Code.Success} {Speed.rank(&s)} {Speed.LIMIT}");
	return 0;
}
```

```text
4 true 2 3
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

A declaration takes `packed` and `align(N)`. A literal of a variant of another module is `geo.Shape.Circle { r: 2.0 }`, and the test of its case `v is geo.Shape.Circle`. A `switch` names every case or has `else`, and `fallthrough` never enters an arm that binds a case's fields. The C header writes the enum of the tags. A variant with an owning case is torn down by its tag and follows the value rules of an owning struct, and an arm that binds such a case gets a copy that the arm tears down. A variant literal is no constant expression, so a constant, the default of a field and the default of a parameter never have a variant type:

```anti refused
//! error: a variant is not a constant expression
variant Shape { Circle { r: f32 }, Empty }

const NOTHING: Shape = Shape.Empty;
```

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
- `pub fn` goes into the table and is visible everywhere, `protected fn` to the class and its chain, `fn` to the class only. A function without `self` is static: `Circle.new(...)`. A `const` of the body is reached by its bare name inside the class, in its fields, their defaults and its functions, and as `Shape.MAX` outside it. A local or a parameter of the same name wins. An `operator fn` at module level reaches the private and protected fields and functions of the classes of its own module. An ordinary function of the module and an operator of another module do not.
- `abstract class` is required when any function has no body. An abstract function takes `self`. `final class` and `final fn` forbid inheritance and replacement.
- `concrete fn` replaces an inherited entry, `concrete fn Base::f` documents which, `concrete fn Iface::f` fills one interface's table only.
- Fields are private unless `pub` or `protected`, and a field may be a bitfield as in a struct. A literal outside the class names `pub` fields only, and names a class of another module as `shapes.Circle { }`. Defaults fill the rest, then `construct` runs. An inline class field without a default takes `T { }` when every field of `T` has a default or `T` has none, and `construct` runs on it. Otherwise the literal must name it.
- `alloc T { fields }` and `alloc T(args)` create on the heap and return `*T`. `T { fields }` and `T(args)` are values. A class has one `construct`, and every other maker is a static function. `construct` runs after a literal and after `alloc`, and `destruct` runs in `delete` and `destroy`, so neither is called by name. A singleton is made by its `get` alone, and `alloc` of one and a literal of one outside its class are refused. A `construct` with arguments that can fail is written `may fail`, leaves with `fail` and succeeds at its end. Every path that succeeds assigns every field without a default, which the compiler checks. `T(args)` carries the error of `construct` to the handler at the call, and a failed `alloc` frees the object.
- A `construct` below that wants its base initialised calls `self.super.construct(args)` as the first statement of its body, and `try` forwards a failure of the base. One that leaves the call out keeps the defaults of the base's fields. A `construct` that fails tears down what was built before it, in reverse order: every field already set, then each base whose `construct` finished, with its `destruct`. Its own `destruct` does not run.
- `delete(p)` runs `destruct` up the chain, frees owned fields, frees the object. `destroy(&v)` does the same without the free. `destroy(&v)` of a local moves `v` out, so its block skips its teardown and naming `v` again is refused, and `destroy` of a part of a local is refused. `delete(p, from)` and `destroy(p, from)` give every piece of owned memory back to the `anti.mem.Allocator` `from`, and `delete` the object as well. Memory goes back to the allocator it came from. A value is destroyed at the end of its block, and so is each element of a local array of them, last to first.
- `dup(p)` is a deep copy through ownership. `=` that copies an existing value with owned fields is refused. A fresh value on the right, a literal, `T(args)` or the result of `dup`, moves, and `=` destroys the value it replaces first.
- `p is *T`, `p as *T` checked, `p as? *T` gives `none` on a mismatch. `==` on class pointers is object identity.

```anti refused
//! error: `Circle` has one `construct`, and every other maker is a static function
class Circle
{
	r: f32 = 1.0,

	fn construct(self) { }
	fn construct(self, r: f32) { self.r = r; }
}
```

A class may declare a `struct`, `enum` or `class` inside its body. The nested type is private: only the enclosing class names it, and a public signature that names it is a compile error. Its full name is `PeopleList.Node` in the symbols and `PeopleList_Node` in the header, where one that the layout of an `export class` reaches stands before the class with its layout alone. A class and the classes nested in it reach the private members of each other, in both directions. A type that users work with directly stays at module level.

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

`implements name: Iface,` places the interface's table pointer and fields inside the object at a named field. `&c` converts to `*Serializable` implicitly. A `concrete fn` without a qualifier fills every table with that name that no qualified body of its class fills, and the nearest body wins down the chain. Two qualified bodies of one name fill the tables of two interfaces whose signatures differ, and `c.f()` on the class is ambiguous when two or more interfaces have qualified bodies of `f` and no unqualified body exists. A single qualified body is reached through its interface's table. `use name: T,` is composition: `T`'s public names are reachable on the class and the class does not convert to `*T`. A program with an abstract class that no class fills gets the warning `unfilled-abstract`.

## Generics

Functions, structs, classes, variants and interfaces take type parameters between `<` and `>`. A constraint after `:` states what the generic needs from a type: a hook of the operator table such as `lt`, `add`, `iter` or `hash`, an interface, or a set named with `constraint`, combined with `+`. The compiler checks the body against the constraints where the generic is written and every use where it is used, so an error lands in the code that made it. A parameter without constraints can be stored, copied, moved, passed on and measured with `size_of`, which is what a container needs. `N: int` takes an integer constant instead of a type, such as the length of a fixed array, and reads as a value in the body. The compiler declares `constraint Number = add + sub + mul + div + neg + lt;` in `anti.lang`, and the source of `anti.lang` declares `constraint Ordered = eq + lt;`. `hash` is the constraint that `x.hash()` on a value of `T` needs. A simd struct meets `add`, `sub`, `mul`, `div` and `neg` through its operators, and neither `lt` nor `Number`.

<!-- overview: context, docs-style:ignore
```anti
import anti.collection.list.{List};

class Person
{
	pub name: str,
}
```
-->
```anti
fn max<T: lt>(own a: T, own b: T) -> T
{
	if a < b {
		return b;
	}
	return a;
}

struct Pair<A, B> { first: A, second: B, }
struct Ring<T, N: int> { items: [N]T, head: int, }
variant Result<T, E> { Ok { value: T }, Err { error: E }, }
abstract class Source<T> { abstract fn next(self) -> ?T; }
constraint Key = eq + hash;

let p: Pair<int, str> = Pair { first: 1, second: "one" };
let q = Pair<int, str> { first: 2, second: "two" };
let ok = Result<int, str>.Ok { value: 3 };
let m = max(3, 7);                     // max<int>
let people = List<Person>.new();

type People = List<Person>;
export type PersonList = List<Person>;
```

A generic interface is a constraint with its type arguments. `Iterable<T>` of `anti.collection` is met by any type whose `iter` hook gives an iterator of `T`, with no extra code.

<!-- overview: context, docs-style:ignore
```anti
import anti.collection;
import anti.collection.map.{Map};
```
-->
```anti
fn count<C: iter>(c: *C) -> int
{
	let n = 0;
	for x in c {
		n += 1;
	}
	return n;
}

fn total<C: collection.Iterable<int>>(c: *C) -> int
{
	let n = 0;
	for x in c {
		n += x;
	}
	return n;
}

fn find<K: eq + hash, V>(m: *Map<K, V>, k: K) -> ?V
{
	return m.get(k);
}
```

Type arguments are inferred from the arguments of a call, and those of a literal of a generic struct from its field values, and written out where nothing gives them. Inference walks up the bases of an argument, and the first base that matches decides. `drain(&c)` with `Countdown inherits Source<int>` gives `T = int` for a parameter `*Source<T>`. In an expression, `<` after a name opens type arguments when a list of types closed by `>` follows and the token after it is `(`, `.` or `{`, so `a < b > c` stays two comparisons. `>>` closes two lists. Every use with concrete arguments gets its own compiled copy, with no boxing, and two uses with the same arguments are one type in every module. A generic that nothing uses compiles to nothing. A dev build marks a copy link-once, and the whole-program pass of a release build merges copies whose code is identical. A method with type parameters of its own is called directly and never stands in the table, so it cannot be `abstract` or replaced. A library file stores a generic as its checked syntax tree with its parameters open, and each use lowers that tree with its arguments. C sees no open generic: `export type PersonList = List<Person>;` writes a copy into the header as an exported class, and a generic `export fn` is refused.

A type nested in a generic class sees the parameters of the class, and each copy of the class has a copy of it. A generic class may be `synchronized` or `concurrent`, and its rules hold in every copy. A generic `worker fn` is checked against the rules of workers in every copy, with the concrete types. A generic type declares language hooks, so `for x in list` works for every copy of a `List<T>` with `operator fn iter`. A generic function may be `may fail`. Function types, closures and snapshots appear as type arguments, `List<fn(int) -> int>`, under the rules of `keep` and `concurrent`.

```anti
class Stack<T>
{
	struct Node { value: T, next: ?*Node, }

	head: ?*Node = none,

	pub fn push(self, own v: T)
	{
		self.head = alloc Node { value: v, next: self.head };
	}

	pub fn top<U>(self, fallback: T, f: fn(T) -> U) -> U
	{
		let h = self.head else {
			return f(fallback);
		};
		return f(h.value);
	}
}

worker fn total<T: add>(chunk: []T, zero: T) -> T
{
	let sum = dup(zero);
	for x in chunk {
		sum = sum + x;
	}
	return sum;
}
```

Not built yet: the cache of copies in a dev build. A library file carries no function of a class with type parameters of its own and no type nested in a generic class.

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

A class value with `destruct`, a `destroyed` hook of its own, an `own` field or an owning part owns something, and so does a collection. `=` to an `own` field first tears down the value the field held, as `unique_ptr` does in C++, so a program never deletes the old value itself. `take(x.f)` moves the object out of an `own ?*T` field and leaves `none` there. The value is a fresh owner that moves on, and a field that cannot be `none` is refused. So is a statement that drops the value, since nothing would own the object. A struct or a tuple with a part that owns something follows the value rules of a class value: it is torn down part by part at the end of its block, `=` refuses it, `dup` copies it and it moves into an `own` parameter. A variant with a case that owns something follows the same rules and is torn down by its tag, the fields of the case it holds. One that owns nothing stays plain data. An owning local named in a tuple, array, struct, class or case literal moves into it, and naming it afterwards is refused. In generic code a local of a type parameter moves on `let` and `=` for every `T`, and `dup(x)` is the copy. `destroy(p)` tears down a value of any type in place, and `dup(x)` of a value of a type parameter copies the value. A parameter that is not `own` belongs to the caller: `let`, `=`, a literal, an `own` argument and `return` refuse one whose value owns something, or whose type is a type parameter. `own` takes it over and `dup(p)` copies it, and `dup(x)` of a class value gives a value. `*dup(p)` and `*alloc T { }` are refused, since nothing could free the new object, and `dup(*p)` copies the value instead. A value that owns nothing and holds no pointer is copied into an `own` parameter and stays with its caller. `own` before a parameter takes ownership of the argument, for a value of any type. A function deletes or destroys only what it received as `own`: `delete` and `destroy` of a parameter without `own`, and of a part of a singleton, are refused. A plain pointer owns nothing: `own x: T` whose `T` is `*Key` owns the pointer and not the object, which `own p: *Key` alone owns. Passing a local moves it, and naming the local again is refused. The function tears the value down at every exit unless it moves on by a call, `=`, `let` or `return`. `lent` before a pointer parameter says the pointer is valid only during the call. The function may read and change through it and pass it on to another `lent` parameter, and may not store it, return it, capture it in a closure that outlives the call or pass it to a `keep` or `own` place. `lent` before a slice parameter, `fn(lent []T)`, follows the same rules. A pointer or a slice derived from a lent one, `s.ptr`, `&p.field`, `&s[i]` or a part of a slice, is lent as well, and its one exit is an argument of an `extern fn`, whose contract decides whether C keeps it. `anti.mem.Shared<T>` gives an object more than one owner through an atomic count: `share()` gives another handle and counts it, `=` stays refused, `read(f)` and `modify(f)` lend the object, and the object is destroyed when its last handle is freed. Two shared objects that hold each other are never freed.

<!-- overview: context, docs-style:ignore
```anti
class Person
{
	pub age: int = 0,
}

class Texture
{
	pub id: int = 0,
}
```
-->
```anti
fn take(own t: Texture) { }
fn grow(lent p: *Person) { p.age += 1; }
fn lend(f: fn(lent *Person)) { let one = Person { }; f(&one); }
fn total(lent xs: []int) -> int { return xs.len; }

let t = Texture { id: 7 };
take(t);
lend(fn(p) { p.age += 1; });
```

<!-- overview: context, docs-style:ignore
```anti
import anti.collection.list.{List};
import anti.mem.{Shared};

class Circle
{
	pub r: f32 = 0.0,
}
```
-->
```anti
let c = Shared<Circle>.new(Circle { r: 2.0 });
let shapes = List<Shared<Circle>>.new();
shapes.push(c.share());
c.modify(fn(p) { p.r = 3.0; });
```

A list whose nodes own their successor pushes at its front with `take`.

```anti
class Node
{
	pub value: int = 0,
	pub own next: ?*Node = none,
}

class Chain
{
	own head: ?*Node = none,

	pub fn push_front(self, value: int)
	{
		let n = alloc Node { value: value };
		n.next = take(self.head);
		self.head = n;
	}
}
```

## Pointers

`none` is the pointer that points to no value. It is a concept of the language, and zero is today's encoding of it. `null` is refused with a message that names `none`. `*T` is never `none`. `?*T` may be `none` and must be checked before use. The check narrows the type for the block.

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

The check narrows a name alone, and an assignment to the name ends the narrowing. A field such as `n.next` stays a `?*T` after its test, so the code binds it to a name first, with `let m = n.next else { }` or `if let`:

```anti refused
//! error: `n.next` may be `none`, check it or use `?*T`
struct Node { value: int, next: ?*Node }

let last = Node { value: 2, next: none };
let n = Node { value: 1, next: &last };
if n.next != none {
	let v = n.next.value;
}
```

`alloc T { }` returns `*T`, `alloc(T, n)` returns `?*T` as raw memory at the alignment of `T`. Every pointer in an `extern fn` is `?*T`. No pointer arithmetic beyond indexing. A function value follows the same rule: `fn(...)` never holds `none` and `?fn(...)` may.

`p as *U` between two pointer types is a view, the same address read as another type. Bytes reach any memory, every pointer is one type and an integer meets its other sign. A struct holds its parts, an array its elements and a simd struct its lanes. Two structs of which neither holds the other keep apart. A view used where it stands reaches any memory, and a view passed on is a pointer of its own type. A release build orders loads and stores by these rules. "Aliasing of views" in `docs/anti-language-additions.md` holds them.

## Optional values

`?T` is a `T` or `none`, for a value of any type, and follows every rule of `?*T`. It compares with `none`, narrows after a test, and works with `if let`, `let ... else`, `??`, `?.` and the guards of `catch`, and it is used as a `T` only after a test proves it holds one. `none` means that nothing is there, which is normal, and a failure is `may fail`: `map.get(key)` gives `?V`, and `text.parse_int(s)` stays `may fail`. A `?T` of a value type is the value and one flag byte, padded to the type's alignment, and `?*T` and `?fn(...)` stay one pointer. `?Match` is a `?T` of `Match`. The C header writes `?T` as `struct anti_opt_<T>` with `value` and `has`.

<!-- overview: context, docs-style:ignore
```anti
import anti.collection.list.{List};
import anti.collection.map.{Map};

struct User { id: int, }
let ages = Map<str, int>.new();
let queue = List<int>.new();
```
-->
```anti
fn find_user(id: int) -> ?User { return none; }

let age = ages.get("Ann") ?? 0;
if let a = ages.get("Ann") { }
let first = queue.first() else { return 1; };
```

## Reflection

Every class has a descriptor: name, parent, size, fields with name, offset, type and ownership, functions with their table slot. `anti.reflect` reads it. `describe` gives the descriptor of an object, `parent` the descriptor of its base and `name` the class name. `field_count` and `field` read the field list as `Field` values with `name`, `offset`, `type_id`, `element` and `owned`, and `function_count` and `function` the function list as `Function` values with `name`, `slot` and `param_count`. `TypeId` names the kind of a field. `get` and `set` read and write a field by its index as a `Value`, which `nothing`, `of_int`, `of_uint`, `of_float`, `of_bool`, `of_char`, `of_str` and `of_ptr` build. `call` calls a function by its index, and `new` builds an object of a class it names, or gives `none`. `set` and `call` fail with the codes `BAD_INDEX`, `NOT_CARRIED`, `WRONG_KIND` and `WRONG_COUNT`.

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
io.println(reflect.name(d));
let base = reflect.parent(d);
for i in 0..reflect.field_count(d) {
	io.println(reflect.field(d, i).name);
	let v = reflect.get(obj as *byte, d, i);
}
reflect.set(obj as *byte, d, 0, reflect.of_float(3.0)) catch fatal;
let r = reflect.call(obj, 0, no_args[0..0]) catch fatal;
let fresh = reflect.new("Circle");
```

`anti.lang.Object` gives every class `type_name`, `to_text`, `equals`, `hash` and `serialize` with defaults over the descriptor, and `copy` and `destruct`, whose defaults the compiler writes per class. `copy(self, to: *Object)` fills an object that `dup` has already allocated at its concrete size, and a replacement fills the fields and never allocates. `Object.deserialize(text, from)` reads the text of `serialize` back into an object, which comes with its strings and the objects it owns from the `anti.mem.Allocator` `from`. The caller gives them back through `from` at once. A variant, a tuple and a `?T` of a value have descriptors of their own, so a field of each compares, hashes, serializes and reads back, and an array field is written as a JSON array of its elements, nested once per level. `--no-reflect` drops the field and function lists.

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

Not built yet: `type_of(T)`, the descriptor of any type.

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

The names: `add sub mul div rem neg eq lt and or xor shl shr not`. `!=`, `>`, `<=` and `>=` derive. A compound assignment does not, so `a += b` on a type with `operator fn add` is refused and `a = a + b` is the form. For a struct the operator is a free function in the declaring module. Two such functions may share a name when their first parameters name different types, and the operator picks one by the type of its left operand, so one module gives `Point` and `Rect` each its own `==`. A bare call `eq(a, b)` picks by its first argument in the same way. Ordinary functions keep one function per name.

```anti refused
//! error: `+=` does not apply to `Money`
class Money
{
	pub cents: int = 0,

	operator fn add(self, o: Money) -> Money { return Money { cents: self.cents + o.cents }; }
}

let total = Money { cents: 100 };
let more = Money { cents: 50 };
total += more;
```

The same table holds the language hooks: `iter`, `next` and `value` for `for x in e`, `index` and `set_index` for `e[i]` and `e[i] = v`, and `hash` for a key of a hashing collection. `index` and `set_index` take any number of indices, each with its own type, so `g[x, y]` reads and `g[x, y] = v` writes. A name outside the table is an error that lists the valid names, and a hook with the wrong signature is an error that states the right one. An `operator fn` is also an ordinary method, so `a.add(b)` is `a + b`. `f"..."` writes a class through `to_text`. `operator fn hash(self) -> u64` is the hook of a key of a hashing collection, and the built-in types have it. `x.hash()` gives a `u64` on every type: a scalar through the finalizer of MurmurHash3 with zero of either sign as one value, a `str` by its bytes, a pointer by its address, and a struct, a tuple, a variant, a `?T`, an array and a slice through the default over their parts in order. A struct or class gets a default that hashes its fields in order, and two values equal by `eq` have the same `hash`. A struct or class also gets a default `==` that compares its fields in order, and a class value compares through `equals`. Both defaults take each field by its kind: a `str` by its bytes, an `own` slice element by element, a plain slice or a pointer by address, and an inline struct, class or array field by these rules again, so a class and a struct with the same fields compare alike. A class with a union or a `Match` field has no default `==`. A lock is left out of `==` and `hash`, a channel and an `own fn` compare by identity, and a `Regex` by its pattern text. The compiler writes the default `==` and `hash` of each class as code, so both work under `--no-reflect`, which refuses a call of the default `serialize`, of `deserialize` and of `anti.reflect`. A tuple, a variant and a `?T` get the default `==` when every part has one, so a tuple can be the key of a map. Two arrays of one type compare element by element. Neither gets a default `<`, since what order means is the type's own choice, so a type used where `Ordered` is needed declares `operator fn lt`. A class replaces the default with `concrete fn hash(self) -> u64`, as it replaces every function of `Object`, and replaces `equals` beside it, since a class that replaces one of the two alone is refused.

```anti
class Name
{
	pub text: str = "",

	concrete fn hash(self) -> u64
	{
		return self.text.hash();
	}

	concrete fn equals(self, other: *Object) -> bool
	{
		let n = other as? *Name else {
			return false;
		};
		return n.text == self.text;
	}
}
```

## Static fields and singletons

A static field belongs to the class and must be atomic. A singleton has one instance.

```anti
class Circle
{
	r: f32 = 1.0,
	pub static atomic count: int = 0;

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

A singleton's `construct` takes no parameters, the class declares no `get`, no class inherits it, and `dup` of it and `*` of a pointer to it are refused.

Atomic operations: `load store add sub and or swap compare_swap`, sequentially consistent. `add`, `sub`, `and`, `or` and `swap` give the value the field held before. `add` and `sub` take an integer, and `and` and `or` an integer or a `bool`. In a singleton a plain field is read-only after creation and `atomic` fields are atomic. `mutable` fields may be written anywhere except from a worker, which the compiler checks.

An atomic field or local is read with `load()` and written with `store(v)`, and `=` and `+=` on one are refused. It holds an integer, a `bool`, a `char`, an `f16`, a pointer or a plain function pointer.

```anti refused
//! error: `count` is atomic, so it is read with `load()` and written with `store(v)`
class Circle
{
	pub static atomic count: int = 0;
}

Circle.count += 1;
```

## Threads

Structured fork-join over an array with `parallel`, and one object at a time with `dispatch`.

<!-- overview: context, docs-style:ignore
```anti
class Sprite
{
	pub x: int = 0,
}
let room = alloc(int, 8) else {
	return 1;
};
let data = []int { ptr: room, len: 8 };
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

`parallel` gives a `[]R` of one result per chunk, freed with `free(r.ptr)`. Arguments after the worker's name go to every chunk, and `by n` sets the count of chunks, where `by 0` takes the count of the machine. `join(job)` gives the result of the worker. A job is joined once: the checker refuses a second `join` it sees, and a dev build stops one it does not.

```anti
import anti.io;
import anti.text;

class Counter
{
	pub n: int = 21,
}

worker fn scaled(chunk: []int, k: int) -> int
{
	let total = 0;
	for x in chunk { total += x * k; }
	return total;
}

worker fn twice(c: *Counter) -> int { return 2 * c.n; }

fn main() -> int
{
	let data: [4]int = [1, 2, 3, 4];
	let parts = parallel data[0..4] by 2 -> scaled(10);
	io.println(f"{parts.len} {parts[0]} {parts[1]}");
	free(parts.ptr);
	let c = alloc Counter { };
	let job = dispatch c -> twice;
	let r = join(job);
	io.println(f"{r}");
	delete(c);
	return 0;
}
```

```text
2 30 70
42
```

Worker parameters are pointer-free. The table pointer and `own` fields do not count. A worker cannot `delete` an object, which another worker may hold. A `worker fn` that may fail is called directly with a handler, and `parallel` and `dispatch` refuse it. `[]Circle` chunks like any array, `[]*Shape` is refused and points at `dispatch`. `--anti.threads` and the key `threads` of the configuration file set the pool size.

`sync m { }` holds a `Mutex` for its block and unlocks it on every exit. A `sync` on a `Mutex` that its thread holds already is refused where the checker sees it, and stops a dev build otherwise. `chan T` is a bounded queue of pointer-free values, and `select` waits on more than one channel, written like `switch` over them.

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

A `sync` on the mutex of an enclosing `sync` in the same function is refused. `close` stops what a channel takes, and what it holds is still received. A `send` on a closed channel stops the program, and so does a channel made with a capacity below 1. The value `recv` gives, and the value an arm of `select` takes, belongs to the block that holds the `recv` or the `select`, the block of the local a `let` binds it to, and is torn down at its end. `delete(c)` ends a channel and tears down every value it still holds, the oldest first. An arm of a `select` is a call, an assignment or a block, as an arm of a `switch` is, and may leave the binding out. A worker takes a `chan T` beside its values, and a pointer to a `Mutex`, which cannot be copied. Not built yet: the warning on a field written inside `sync` and read outside it.

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

A `synchronized class` gives each object a hidden lock that every public function runs under, released on every exit, and `sync obj { }` holds it for a block. `sync a, b { }` holds the locks of two synchronized objects, taken in the order of their addresses and once for one object, and the default `==` of a synchronized class runs under it, its default hash under `sync a`. An operator that takes a synchronized class through pointers receives the objects themselves, and one that takes it by value is refused. Either kind inherits `anti.lang.Object`, an abstract class without fields or a class of its own kind. A field of a synchronized class, `pub` or not, is reached by the functions of the class and of the classes below it alone, which run under its lock:

```anti refused
//! error: `count` is a field of the synchronized class `Counter`, which its own functions reach alone
synchronized class Counter
{
	pub count: int = 0,
}

fn peek(c: *Counter) -> int { return c.count; }
```

A `concurrent class` leaves the locking to the programmer, and every field is guarded by a `Mutex`, atomic, or fixed after `construct`. It has no default `==` or hash and declares the ones it needs, and `=` never copies one, also when it holds atomic fields alone. `unchecked(unguarded-field, "reason")` after a field's type or in the class header overrules the check, and `compare_swap` takes a field of one word that it marks. A public function of either never gives out a pointer or a slice into the object's fields: not as its result, to a function value, through an out parameter or into another object. A variable that took a pointer to a guarded field inside its `sync` is used in that block alone, and not after it. A worker may take a pointer to a thread-safe object, and `atomic` marks a local as well. A `Mutex` is one word of the program's memory and cannot be copied, and no constant holds one. A dev build records the order in which each thread takes locks and reports two orders that conflict. `size_of(Mutex)` is 4 on Linux and macOS and 8 on Windows.

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

Every hook site calls the installed handler and then dispatches the object's own hook. `leave` reverses the two, so the handler and the object's hook nest around the call. A class that replaced a hook does not silence the handler, and the handler does not replace the class's hook. A hook site is one call of the runtime.

`trace` is a contextual word before `class` and before `fn` in a class body. `--trace` and `--no-trace` decide instead of the mode, `--trace <pattern>` reaches a class that did not ask, and `--trace writes` turns `changed` on. `anti.trace` ships `LeakTracker`, `Profiler`, `CallLogger`, `ErrorMonitor`, `ThreadMonitor`, `ChangeJournal` and `Composite`. `trace.start` reads the runtime key `trace` and installs the handler it names, and `trace.install` takes the same text from a program. The names are `leaks`, `profile`, `calls`, `errors`, `threads` and `writes`, for the six handlers in that order, and a list with commas between them installs a `Composite` of those handlers. A name that names no handler prints one line to the standard error and is passed over.

```anti
import anti.trace;

fn main() -> int
{
	let handler = trace.install("leaks,calls");
	return 0;
}
```

`--no-hooks` drops every site, the five always-on ones as well. Instrumentation is a build option and never a change to an object's layout. A program that loads plugins cannot be built with `--no-hooks`, since `unload` counts a library's objects in the hooks. antic refuses the combination with a message that names both. A plugin cannot be built with it either, because its own classes call those hooks. Not built yet: the cost of one load and one compare at a site without a handler.

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

`inject name: *Interface` marks a field that the provider of its interface fills before `construct` runs, and `inject final` one that the run-time configuration may not replace. A provider is a module function that gives a pointer of the interface, or the `get` of a singleton that implements it. The name of that singleton alone names its `get`. The `[inject]` and `[inject.test]` tables of the manifest reach `antic` as `--inject Interface=Provider`, and `anti test` passes them. One slot per interface holds the provider, and every site calls through it. The link refuses an interface with no provider, a provider that is no function of the program or is no such interface, and a cycle through the providers.

Six standard interfaces ship, each an abstract class with a default implementation and a default provider, so an `inject` field of one needs no entry in the manifest: `anti.log.Logger` with `SinkLogger`, `anti.time.Clock` with `SystemClock`, `anti.random.Source` with `SharedRandom`, `anti.fs.FileSystem` with `SystemFileSystem`, `anti.mem.Allocator` with `LibcAllocator` and `anti.config.Config` with `FileConfig`. The default provider of an interface is a static function `default` of it, which the manifest overrides. `anti.mem` also holds `ArenaAllocator`, which hands out memory from blocks and gives them all back at once, and `Object.deserialize` and `Builder.take_in(from)` of `anti.text` take an `Allocator`. `f"..."` takes none. The language's `alloc` and `free` stay bound to the C library. Every collection of `anti.collection` takes an `Allocator` when it is made, and the C library without one. The run-time replacement and `--closed` stand under [Plugins](#plugins). Not built yet: the interfaces for services.

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

`provides Interface as Class;` stands at module level, and `antic --lib shared --no-runtime` writes the library: the object of its own module alone, linked without the runtime and bound against the host that loads it. `plugin.load` gives a `Library`, `lib.instance(Interface)` builds the class it provides or fails with the reason it could not, `lib.supports(Interface, "f")` reads its functions, and `lib.unload()` refuses while an object of the library is alive. The load checks the runtime version exactly and that every interface is the host's. The classes of a library join the registry, so `reflect.new` finds one by name. antic writes `anti-plugins.toml` beside a library, with the interfaces, the runtime version and the digest of each library of the directory. `"plugin:path"` and `"discover"` of the manifest take a provider from a library, discovery reads the index and checks the digest, and `[injections]` and `--anti.inject` replace a provider at start. `--closed` builds a program without the exports a plugin binds against. A program loads a library on macOS, Linux and Windows. On Linux a program that can load one links dynamically against glibc. On Windows its link writes an import library, and a plugin links against the import library of the program that loads it.

Versions are checked at load. Every class descriptor carries the version of the package that declared it, `--package-version` and `0.0.0` without one. An abstract class carries the chain of its structural hashes and the floor a `compatible <version>;` line names, and a plugin carries the chain, the fields, the size and the version of each interface it was built against. The load compares the two chains at the length of the shorter, refuses a field added between the two versions, refuses a slot the program's calls reach and the library lacks, and fills a slot that `reflect.call` alone may reach with a stub that names the plugin.

## Runtime configuration

A program takes its runtime configuration at start. A TOML file, when one is named, and the `--anti.` options of the command line set the same keys.

```toml
include = ["base.toml"]

[runtime]
threads = 4
trace = "leaks,calls"
backtrace = true

[injections]
"anti.log.Logger" = "lib/CISOLogger.so"
```

`--anti.conf=<path>`, `ANTI_CONF` and `rt.configure(path)` of `anti.runtime` name the configuration file, which holds `include`, `[runtime]` and `[injections]`. The includes run first and in order, so the including file wins per key. Every key of `[runtime]` is an option of the same name, `--anti.inspect` prints the effective value of each with the layer it came from and the version and the used slots of each injectable interface, and `--anti.help` lists the options. Precedence per key is the command line, the file, the build. The `--anti.` options are the runtime's and are consumed before `main`, and `rt.get(key)` gives the effective value of a key. Injections are fixed at start, so `rt.configure` refuses a table `[injections]`, in its file and in every file that file includes, and names `--anti.conf` and `ANTI_CONF` instead.

The six keys of `[runtime]` are `threads`, the size of the worker pool, `logger`, the TOML file of `anti.log`, `plugins`, the directories that discovery reads, separated by `:`, or by `;` on Windows, `trace`, the handlers of [Hooks and tracing](#hooks-and-tracing), `backtrace`, the frames of an error in a release build, and `hash_seed`, the seed of the hashing collections. A key the runtime does not know, a value a key does not take, a file that cannot be read and a cycle of includes end the program with status 70 and a message that names the file and the line, and so does a line of `[injections]` that names no injectable interface of the program.

```anti
import anti.io;
import anti.runtime as rt;

fn main() -> int
{
	let threads = rt.get("threads") ?? "the default";
	io.println(threads);
	return 0;
}
```

```text
the default
```

## Tests

A `tests` block per module holds tests, a `fixtures` block holds their helpers, and a module has at most one of each. Both see the module's private items and are compiled only by `anti test`.

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

A function of a `tests` block takes no parameters and returns nothing, because the runner calls it:

```anti refused
//! error: a `tests` function takes no parameters and returns nothing, because the runner calls it
tests
{
	fn adds(n: int) { }
}
```

`setup` and `teardown` in `fixtures` run before and after every test of the module. `anti test` compiles each module with `--tests`, writes a runner that calls its tests, links it and runs it. The dev run links an object of every module the one under test imports, so a module of the standard library carries a `tests` block too. It prints `ok <module>.<test>` for a test that returned, and a failed assertion prints `FAIL <module>.<test>` and then the file and the line. `anti test --release` runs the same tests with the checks and the assertions off.

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
export union Bits { whole: u32, parts: [4]u8 }
export const LIMIT: c_int = 8;
export enum Mode: u8 { Read, Write }
export variant Reply { Number { n: c_int }, Nothing }

export class Counter
{
	pub n: c_int = 0,

	pub fn get(self) -> c_int { return self.n; }
}

link framework "CoreAudio";
link linux "X11";
```

`extern fn` declares a C function. Every pointer of one is `?*T`, `...` declares a variadic one, whose arguments are integers of 32 or 64 bits, `c_long` and `c_ulong` among them, `f64` or pointers, and `may fail` and `export` are refused on it. Its signature takes C types alone: a `str`, a slice, a tuple, an optional value and an `own` parameter are refused, and a `char` passes as a 32-bit integer. A `lent` parameter is refused, since C may keep the pointer, and so are `concurrent` and `keep own`, since C takes a plain function pointer. One C function has one signature in a program, and a second `extern fn` of its name with another one is refused. `export` on `fn`, `struct`, `union`, `class`, `enum`, `variant`, `const` and `type` implies `pub` and puts the item into the C header, a function under its unmangled name, an enum as a C enum and a variant as its struct with the enum of its tags. The signature of an `export fn` and the fields of an `export struct` or `export union` use sized integers, `int`, `uint`, `float`, `bool`, the C types, pointers to such types, exported structs and unions, and function pointers of such signatures. `char`, `str`, slices, arrays by value and non-exported structs are refused, and a field may be a fixed array of an allowed type. An `export const` has a numeric type, `bool` or `str`. An exported name that `<stdbool.h>`, `<stddef.h>` or `<stdint.h>` defines, as `SIZE_MAX`, is refused. An `export fn` of a class body is `pub` as well, `export` before an `operator fn` is refused, since C has no operators, and `export type` writes a copy of a generic, see [Generics](#generics). `link framework "Name";` links a macOS program against a framework of Apple's SDK, and `link linux "Name";` a Linux program against a library of the glibc sysroot. A Linux program that reaches a `link linux` line links dynamically against glibc 2.35, and every other one statically against musl.

`anti bind raylib_api.json` and `anti bind --clang header.h` generate bindings. `anti build --lib static|shared` builds a library with a header. A class is exported with its layout, tables, `anti_<Class>_init`, `anti_<Class>_construct` for a `construct` with arguments, and the dispatch wrappers. `embed("file")` puts a file's bytes in the binary. Not built yet: `embed`.

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

`anti check --targets all` runs the front end of every source once per target. Not built yet: `target.os`, `target.cpu` and `target.mode`, and the `switch` on one of them.

## Source locations

`here` is the position it is written at, as a `SourceLocation` with `file`, `line`, `column`, `function` and `module`. As a default parameter value it is evaluated at the call site, which is how a logger reads its caller's line without a macro.

```anti
fn warn(msg: str, at: lang.SourceLocation = here) { }

warn("disk is full");       // at is the caller's position
let p = here;               // the position of this expression
```

The value is constant data. `here` in an ordinary expression gives the position of that expression, which is rarely what a message wants.

## Checks and debugging

In dev mode every array, slice and `str` index is bounds-checked, signed arithmetic traps on overflow, the least value divided by minus one and unary `-` of it among it, a narrowing `as` checks its range, a value stored into a bitfield is checked against its width, a conversion to `char` or to an enum checks the value, division and shifts are checked, `<<%` for a count below zero alone, `assert` and `show` are active, `-g` writes line information. In release none of it is emitted, and every target gives the result of ARM64: a division or a remainder by zero gives 0, the least value divided by minus one gives itself, a shift takes its count modulo the width, and a float out of the range of its integer type saturates, with 0 for NaN. Where the operands are constants, the checker refuses what a dev build would stop, in every mode: a signed `+`, `-`, `*` or unary `-` out of range, a division by zero or of the least value by minus one, a shift count below zero or, for `<<` and `>>`, at or above the width, a count below zero of `<<%` in a length that a copy of a generic leaves, an `as` to an integer type that cannot hold the value an `as` to `char` of a value that is no Unicode scalar value, a constant index below zero on a fixed array, and a constant that does not fit the width of the bitfield it is stored into. `--checks` and `--no-checks`, `--asserts` and `--no-asserts`, `--trace` and `--no-trace` and `-g` override the mode.

```anti
trace class Renderer { }
```

`trace` marks a class or function whose `pub` functions call the `enter` and `leave` hooks in dev mode. `--trace <pattern>` instruments code that did not ask. `anti.trace` ships `LeakTracker`, `Profiler`, `CallLogger` and the rest, and `trace.start` installs the one that the runtime key `trace` names. [Hooks and tracing](#hooks-and-tracing) holds the nine hooks and the options that decide them.

`-g` writes the line of every statement and an entry per function, and the link keeps the debug sections, so lldb and gdb stop by file and line and print a backtrace of Anti function names. Every executable and shared library carries a build id in `anti_licenses`. A release binary carries no symbol data. `anti build --release` writes a symbols archive beside it, `<program>-symbols.zip`, with the same link with its debug sections kept, the map of the program's functions and, on Windows, the PDB. `anti symbols inventory`, `check` and `resolve` read the runtime configuration and the build id of every binary it reaches, collect the archives of a deployment and turn a raw trace into names and lines. `resolve` names a frame from the debug link and fills what it lacks from the map. `StackTrace` has `capture`, `frames`, `text` and `symbolize`, which names the function of a frame in every build and its file and line in a `-g` build. `anti.debug.backtrace(skip)` captures one at run time, and `--anti.backtrace` turns the frames of an error on in a release build.

`--memory-checks`, for `antic`, `anti build`, `anti test` and `anti run`, makes a build find a use after free, a double free and a read or write outside a heap block while the program runs, and report the leaks at exit with where each block was allocated. The back end emits the checks of AddressSanitizer around every load and store, and the build links the runtime of AddressSanitizer from the runtime archive. The option is off by default, since the program runs two to three times slower, and is refused on windows-arm64, which has no such runtime. `anti run` and `anti test` put the report through Anti's own symbolizer, so every frame reads as a name, a file and a line, and the leak report leaves out what the runtime keeps from start to exit on purpose. A pointer into a collection kept across a change of its size is a use after free, and the option finds it.

Not built yet: the variables of `-g`, the symbols archive of a shared library and of a plugin, the names of a Windows frame in `resolve` and the leak report on windows-x86_64. `anti build` writes the archive for a program alone, `resolve` reads no PDB and leaves a Windows frame raw, and the runtime of AddressSanitizer for windows-x86_64 has no leak check.

## Processor levels

A level is a code-generation setting, not a target. `--cpu` takes `v1`, `v2` or `v3` on x86_64 and `armv8.0`, `armv8.2` or `armv8.5` on ARM64, and each target has a default.

| Target | Default level |
|---|---|
| linux-x86_64, macos-x86_64, windows-x86_64 | `v3`, x86-64-v3 |
| macos-arm64 | `armv8.5` |
| windows-arm64 | `armv8.2` |
| linux-arm64 | `armv8.0` |

x86-64-v3 writes the VEX forms of the scalar float instructions, and an atomic operation is one instruction at `armv8.2` and above. The runtime archive holds one runtime per target and level in `lib/<target>/<level>/`, so a program below the default links a runtime of its own level. Every program checks the processor at start against the level of the runtime it linked, and refuses a machine below it. The native libraries are built for the default level alone, and a program below it that imports one is refused at link. `antic --print-cpu-levels` prints the table.

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

Every warning and every safety check has a stable name at the end of its message, as in `` `e` shadows the outer `e` [shadowed-catch] ``. `allow` and `unchecked` stand before a statement, last in a declaration's header, or at the top of the file ending with `;`, before the imports, and `unchecked` also after a field's type. Each takes one name and a required reason, and is not part of a signature. One that silences nothing is the warning `unused-allow` or `unused-unchecked`. `antic --warnings-as-errors` gives the release behaviour in a dev build, and `anti check` uses it. `docs/notes/warnings.md` gives the fix of each.

| Warning | Meaning |
|---|---|
| `shadowed-catch` | The name a `catch` binds hides a variable of an outer scope. |
| `never-fails` | A `may fail` function holds no `fail` and no `try`. |
| `undo-never-runs` | An `undo` stands in a function that cannot fail and in no `try` block. |
| `assert-call` | The condition of an `assert` calls a function, which a release build does not run. |
| `unfilled-abstract` | No class of the program fills an abstract class. |
| `above-vector-cap` | A `simd struct` is above the vector cap, so each operation on it is a loop. |
| `single-segment-path` | The module path of a library has one segment, which is for a program's own files. |
| `nothing-exported` | `--lib static` builds a module without an exported item. |
| `doc-markup` | A doc comment holds markup outside the doc subset. |
| `doc-unresolved` | A name in backticks of a doc comment resolves to nothing. |
| `doc-note-only` | A `pub` item has a `//#` note and no `///` comment. |
| `undocumented` | A `pub` item has no `///` comment, under `--warn-undocumented`. |
| `doc-dropped` | A doc comment stands where no item or field follows it. |
| `unused-allow` | An `allow` silences nothing. |
| `unused-unchecked` | An `unchecked` overrules nothing. |
| `blocking-under-lock` | A `recv`, a `send`, a `select`, a `join` or a `join_all`, or a call that leads to one, may wait while a lock is held. |
| `unused-result` | The program drops the result of a call that has no effects. |

The five `doc-` warnings and `undocumented` come with `--doc-warnings`, which `anti check` passes.

| Safety check | Meaning |
|---|---|
| `unguarded-field` | A field of a concurrent class is not guarded, atomic or fixed. |
| `exponential-pattern` | A pattern literal nests repeats over text that overlaps, which can take exponential time. |
| `escaping-local` | A reference to a local or a parameter would outlive it. |
| `use-after-free` | A value is used after `delete`, `destroy` or a move of it. |
| `uninitialized-read` | An element of `alloc(T, n)` is read before any path wrote it. |
| `index-out-of-range` | An index is out of range on every path that reaches it. |
| `lock-order` | Two places take the same two locks in opposite orders. |

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

A pattern literal `re"..."` is raw and has type `Regex`. The compiler checks it, knows its groups and compiles it once before `main`. `Regex.compile(text)` compiles one at run time and may fail with `BadPattern`. The engine is PCRE2, linked only when a program uses a pattern, and a module that writes a pattern imports `anti.regex`.

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

`matches`, `find_all`, `replace` and `split` are methods of `str`. `find_all` and `split` are iterators, and `to_slice()` collects either. `limit` takes that many matches from the start, or from the end when negative. A match is a `?Match`, behaves as a `?*T` does and stands alone as a condition, in `if`, `while`, `&&`, `||` and `!`, and `if let` and `let ... else` bind one. Its fields are `all`, `group(n)`, `took_part(n)`, `count`, `pre` and `post`, and for a pattern literal a group is a field, `m.1` or `m.year`. A template names groups with `$1` and `${name}`, is checked at compile time for a literal, and a function may give each replacement. A call with a pattern literal never fails and stops at the match limit. A call with a compiled pattern may fail with `regex.TooExpensive` or `regex.MissingGroup`. Flags are PCRE2's inline flags, `(?i)`, and `\d`, `\w` and `\s` mean their ASCII sets, with `(*UCP)` for the Unicode ones. A pattern that can take exponential time fails the safety check `exponential-pattern`. No match reads or writes anything global.

## Bytes

The same methods work on `[]byte` with a `ByteRegex`, where `.` matches any byte and nothing needs to be valid UTF-8. A pattern literal takes its mode from where it is used, and `ByteRegex.compile(text)` compiles one at run time. `patch` writes bytes over part of what it finds, in place, and allocates nothing.

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

`s.to_bytes()` copies a `str` into new bytes, and `data.to_text()` checks for valid UTF-8 and may fail. Both are calls of `anti.text`. A match of bytes is a `ByteMatch`, and its fields are slices of the searched bytes. `replace` returns new bytes, and `patch` never changes the length, so a `with` that does not fit its span is a compile error or a `LengthMismatch` failure by the origin of its operands. `patch` takes a byte sequence or a pattern, then `into`, `at` and `limit` by position, `data.patch(x"80 10 20 30", x"FF", 0, 2)`, until named arguments are built. `str` has no `patch`. Not built yet: `at: 2` and the other named arguments of `patch`.

## Collections

The collections of `anti.collection` are generic classes used as values, one module per family: `List<T>`, `Deque<T>`, `Ring<T, N>` and `Grid<T>`, `Map<K, V>` and `HashMap<K, V>`, `Set<T>`, `HashSet<T>` and `BitSet`, `SortedMap<K, V>` and `SortedSet<T>`, `Pool<T>` with `Handle<T>`, `Tree<T>` and `PriorityQueue<T>`. `anti.collection` itself holds `Iterable<T>`, `Iterator<T>` and `Collection<T>`, the operations every collection has. A collection owns its storage and stores its elements by value. It is freed at the end of its block, moves on return, is refused by `=` and is copied by `dup`, with room of its own. No pointer to an element leaves it except through a `lent` parameter: reading gives a copy, as a `?T` where nothing may be there, and `read` and `modify` lend an element to a function. `union` stays a keyword and names a function after `fn` and a member after `.`, as `alloc` and `free` do, so a set has `union(o)`.

<!-- overview: context, docs-style:ignore
```anti
import anti.mem;
let arena = mem.ArenaAllocator.new(mem.Allocator.default(), 4096);
```
-->
```anti
import anti.collection.list.{List};
import anti.collection.map.{Map};
import anti.io;
import anti.text;

class Person
{
	pub name: str = "",
	pub age: int = 0,
	pub retired: bool = false,
}

let people = List<Person>.new(1000);
people.push(Person { name: "Ann", age: 41 });
let first = people.get(0) ?? Person { };
people.read(0, fn(p) { io.println(p.name); });
people.modify(0, fn(p) { p.age += 1; });
let n = people.update_all(fn(p) { return p.age >= 65; }, fn(p) { p.retired = true; });
let ann = people.find_one(fn(p) { return text.equal(p.name, "Ann"); }) catch fatal;
for p in &people {
	p.age += 1;
}

let ages = Map<str, int>.new(0, &arena);
ages.set("Ann", 41);
let age = ages.get("Ann") ?? 0;
for (k, v) in ages { }
for (k, v) in &ages {
	*v += 1;
}
```

Criteria come in three forms: `_all` acts on every match and gives the count, `_first` on the first match in the collection's order, and `_one` on the one match, failing with `NotOne`, which carries the count, when none or more than one matches. `find_all` gives a `Copies<T>`. Every collection takes an optional allocator, `from`, and one that grows takes an initial `capacity`, with `reserve(n)` and `shrink()`. Until named arguments are built, `new` takes both by position, `capacity` first, as the example above does. The named form is the one the specification gives:

```anti not-built
let people = List<Person>.new(capacity: 1000);
let ages = Map<str, int>.new(from: &arena);
```

`Map` walks in insertion order, the same on every run. `SortedMap` is a B-tree and walks in key order. `HashMap` walks in an order that differs on every walk, so no program depends on it. A hashing collection mixes its hashes with a seed the runtime chooses at start from the random source of the system. `lang.hash_seed()` gives it, `lang.seeded(h, seed)` mixes it into a hash, and `--anti.hash_seed=<n>` or the key `hash_seed` fixes it for a test. `Ring<T, N>` allocates nothing after it is made, for real-time code. A `Pool` keeps its elements at fixed addresses and gives handles of a slot and a generation, so a stale handle finds nothing, and a `Tree` holds a hierarchy on the same handles. `get_versioned` gives an element with its version, and `set_if_version(h, x, version)` fails with `collection.Changed` when the element changed or is gone. `PriorityQueue<T>` gives its smallest element first, and `Grid<T>` is indexed by two positions. `sort`, `sort_by` and `map` are functions of `anti.collection.list` that a call on a list reaches as its own, and `map` names both its types, `scores.map<int, str>(f)`.

```anti
import anti.collection.grid.{Grid};
import anti.collection.list.{List};
import anti.collection.pool.{Pool};
import anti.collection.queue.{PriorityQueue};
import anti.collection.set.{BitSet, Set};

let scores = List<int>.new();
scores.push(3);
scores.push(1);
scores.sort();
let labels = scores.map<int, str>(fn(n) { return "score"; });

let seen = Set<str>.new();
seen.add("Ann");
let both = seen.union(Set<str>.new());
let small = BitSet.new();
small.add(3);
for n in small { }

let staff = Pool<str>.new();
let h = staff.add("Bo");
let entry = staff.get_versioned(h) else { return 1; };
staff.set_if_version(h, "Bea", entry.1) catch fatal;

let jobs = PriorityQueue<int>.new();
jobs.push(5);
let next = jobs.pop() ?? 0;

let g = Grid<int>.new(4, 4, 0);
g[1, 2] = 7;
```

A collection has `==` when its elements have `eq`, and a hash of its elements, so equal collections hash alike: a sequence in order, and a map or a set whatever the order of its entries. Its `equals` through `*Object` is its `==`. A copy of a `Pool` or a `Tree` keeps the handles of the original.

The thread-safe collections of `anti.collection.synchronized` and `anti.collection.concurrent`, each module named after the kind of class it holds, follow [Concurrent classes](#concurrent-classes). `SyncList<T>`, `SyncMap<K, V>`, `SyncSet<T>` and `SyncPool<T>` are `synchronized`. `ConcurrentMap<K, V>` is `concurrent`, split into parts locked apart. `SpscRing<T, N>` passes values from one thread to one other with two atomic counters and no lock. They have no operation by position and no `lend_slice`, and every one but the ring has versioned `set`. A thread-safe collection walks, and writes its text, over copies of its elements, and `read`, `modify` and the functions of the criteria forms run under its lock. `==` and the hash of each compare and hash its elements only, never a version, a change count or a lock, and `ConcurrentMap` holds the locks of all its parts in their fixed order meanwhile. A queue between threads is `chan T`.

```anti
import anti.collection.concurrent.{ConcurrentMap, SpscRing};
import anti.collection.synchronized.{SyncMap};

let hits = SyncMap<str, int>.new();
hits.set("home", 1);
let busy = ConcurrentMap<str, int>.new();
busy.set("home", hits.get("home") ?? 0);
let samples = SpscRing<f32, 1024>.new();
let pushed = samples.push(0.5);
let s = samples.pop() ?? 0.0;
```

Not built yet: named arguments, which the not-built example above writes, `deserialize` of a collection, and the versioned `set` of `SpscRing`, since the ring has no `set`.

## Standard library

The modules under `anti.` ship with the compiler, and a program imports them without `-I`. `anti.lang` is the root and imports nothing.

```anti
import anti.io;
import anti.text;

let n = text.parse_int("42") catch fatal;
let s = f"{n} items";
io.println(s);
free(s.ptr);
```

The 23 modules, with their public items and the functions among them that may fail:

| Module | Public items | May fail |
|---|---|---|
| `anti.lang` | `Error`, `NoneDereference`, `SourceLocation`, `StackTrace`, `RawFrame`, `Frame`, `Changes`, `Watch`, `Trace`, `TraceHandler`, `Ordered`, `hash_seed`, `seeded`, and the names the compiler declares, see [Types](#types) | |
| `anti.io` | `print`, `println`, `eprint`, `eprintln`, `exit` | |
| `anti.text` | `equal`, `from_c`, `from_bytes`, `to_bytes`, `to_text`, `byte_count`, `char_count`, `slice`, `find_byte`, `parse_int`, `Align`, `Builder` with `append`, `append_byte`, `append_int`, `append_uint`, `append_float`, `append_f32`, `append_bool`, `append_char`, `append_text`, `text`, `len`, `clear`, `take` and `take_in` | `to_text`, `parse_int` |
| `anti.license` | `text` | |
| `anti.error` | `SystemError` with `from_errno` and `from_win32`, `on_fatal`, `check` | |
| `anti.time` | `Duration` with `nanoseconds`, `microseconds`, `milliseconds`, `seconds`, `as_milliseconds`, `as_seconds`, `add` and `between`, `now`, `wall`, `sleep`, `Clock`, `SystemClock` | |
| `anti.os` | `INTERRUPT`, `BREAK`, `TERMINATE`, `on_signal`, `signal_pending`, `user_config_dir`, `user_data_dir`, `user_cache_dir` | the three directories |
| `anti.fs` | `Mode`, `File`, `open` with `Mode.Read` as its default, `read`, `write`, `flush`, `size`, `close`, `list`, `remove`, `rename`, `read_file`, `write_file`, `FileSystem`, `SystemFileSystem` | every function |
| `anti.reflect` | `TypeId`, `Field`, `Function`, `describe`, `parent`, `name`, `field_count`, `field`, `function_count`, `function`, `get`, `set`, `new`, `call`, `ValueKind`, `Value`, `nothing`, `of_int`, `of_uint`, `of_float`, `of_bool`, `of_char`, `of_str`, `of_ptr`, `BAD_INDEX`, `NOT_CARRIED`, `WRONG_KIND`, `WRONG_COUNT` | `set`, `call` |
| `anti.random` | `Source`, `Random`, `SharedRandom` | |
| `anti.collection` | `Iterable<T>`, `Iterator<T>`, `Collection<T>`, `Indexed<T>`, `IndexedWalk<T>`, `Copies<T>`, `NotOne`, `Changed`, `Snapshot<T>`, `allocator`, `equal`, the hashing helpers `hash_in_order`, `hash_in_order_by`, `hash_in_any_order`, `hash_sum` and `hash_of_sum`, and the writers of the text of a collection | `find_one`, `update_one` and `remove_one` of every collection |
| `anti.toml` | `Document`, `int_value`, `bool_value` | `Document.read` |
| `anti.config` | `Config`, `FileConfig`, `current`, `read` | `read` |
| `anti.args` | `Parser`, `UNKNOWN_OPTION`, `MISSING_VALUE`, `FLAG_WITH_VALUE` | `Parser.parse` |
| `anti.json` | `write`, `write_text`, `write_int`, `write_bool`, `member`, `unquote` | `unquote` |
| `anti.log` | `Level`, `Format`, `Sink`, `StderrSink`, `FileSink`, `CallbackSink`, `Logger`, `SinkLogger`, `Log`, `logger`, `set`, `trace`, `debug`, `info`, `warn`, `error`, `fatal`, `level_name`, `level_of`, `register` | `FileSink.new` |
| `anti.debug` | `backtrace` | |
| `anti.mem` | `Allocator`, `LibcAllocator`, `ArenaAllocator`, `Shared<T>` | |
| `anti.runtime` | `configure`, `get` | |
| `anti.simd` | nothing of its own: the compiler knows `select`, `any` and `all` by name | |
| `anti.trace` | `LeakTracker`, `Profiler`, `CallLogger`, `ErrorMonitor`, `ThreadMonitor`, `ChangeJournal`, `Composite`, `start`, `install`, `PROFILE_SLOTS`, `JOURNAL_SLOTS`, `MESSAGE_BYTES`, `COMPOSITE_SLOTS` | |
| `anti.plugin` | `Library` with `instance`, `supports`, `live` and `unload`, `load`, `NOT_LOADED`, `IN_USE` | `load`, `instance`, `unload` |
| `anti.regex` | `Error`, `BadPattern`, `TooExpensive`, `MissingGroup`, `LengthMismatch`, `compile`, `compile_bytes`, `Cursor`, `Matches`, `Pieces`, `ByteMatches`, `BytePieces`, and the functions behind the methods of `str` and `[]byte` | `compile`, `compile_bytes`, and a method with a compiled pattern |

The 12 modules under `anti.collection`, each with the `operator fn eq` and `operator fn hash` of its classes:

| Module | Public items |
|---|---|
| `anti.collection.list` | `List<T>`, `sort`, `sort_by`, `map` |
| `anti.collection.deque` | `Deque<T>` |
| `anti.collection.ring` | `Ring<T, N>` |
| `anti.collection.grid` | `Grid<T>` |
| `anti.collection.map` | `Map<K, V>`, `HashMap<K, V>`, `EntryTable<E>`, `SlotTable<E>`, and the walks `MapWalk`, `MapKeys`, `MapValues`, `HashMapWalk`, `HashMapKeys` and `HashMapValues` |
| `anti.collection.set` | `Set<T>`, `HashSet<T>`, `BitSet`, and the walks `SetWalk`, `HashSetWalk` and `BitWalk` |
| `anti.collection.sorted` | `SortedMap<K, V>`, `SortedSet<T>`, `BTree<T>`, and the walks `SortedWalk`, `SortedEntries`, `SortedKeys` and `SortedValues` |
| `anti.collection.pool` | `Pool<T>`, `Handle<T>`, `Slots<T>`, `SlotsWalk<T>` |
| `anti.collection.tree` | `Tree<T>` |
| `anti.collection.queue` | `PriorityQueue<T>`, `QueueWalk<T>` |
| `anti.collection.synchronized` | `SyncList<T>`, `SyncMap<K, V>`, `SyncSet<T>`, `SyncPool<T>`, `sort`, `sort_by`, `map` |
| `anti.collection.concurrent` | `ConcurrentMap<K, V>`, `SpscRing<T, N>` |

Not built yet: the modules over the other native libraries of the runtime archive, `anti.net`, `anti.raylib` and `anti.miniaudio`, the interfaces for services with `anti.db` over SQLite, and `anti.binary`, which the code of a wire format uses.

## Tools

The `anti` tool has twelve commands.

| Command | What it does |
|---|---|
| `anti new <name>` | Writes a starter project: `anti.toml`, `src/` with one module and `test/`. |
| `anti build` | Reads `anti.toml`, resolves the dependencies into `anti.lock`, fetches every library file and compiles the project. Dev mode writes one object per module, cached by digest, with `-g` on. `--release` compiles the whole program in one call and writes the symbols archive beside it. `--target`, `--cpu`, `--offline`, `--strip-docs`, `--lto`, `--memory-checks`, `--lib static\|shared`, `--bundle-runtime`, `--soname`, `--profile-generate` and `--profile-use`. It writes `NOTICE.txt` beside a program. |
| `anti run` | Builds for the host and runs what it wrote, with the options of `build` that apply to the host. |
| `anti sdk export` | Packs the stubs and the version of Apple's SDK into `apple-sdk-<version>.tar.xz`, on a Mac. |
| `anti sdk import <bundle>` | Unpacks that bundle into the sysroot of Anti on any host, for a program that names a framework. |
| `anti test` | Compiles each module with `--tests`, writes the runner, links it and runs it. `--release` and `--memory-checks`. |
| `anti check` | The front end on every source, the `anti` blocks of the doc comments, the doc warnings and the formatting, in that order. `--targets all` and `--warn-undocumented`. |
| `anti doc` | One page per module and an index, as HTML or with `--markdown`. `--dev` and `--private` read the syntax tree for the private items and the notes. |
| `anti fmt` | Writes the canonical form of each file, and `--check` reports a file that differs. |
| `anti bind` | `anti bind <api.json>` and `anti bind --clang <header>` write a binding module, with `--probe` the ABI probe. `anti bind --header <file.antl>` writes the C header of a library. |
| `anti symbols` | `inventory`, `check` and `resolve` over the symbols archives of a deployment, see [Checks and debugging](#checks-and-debugging). |
| `anti license` | Prints the licence texts of the package. `--project` and `--project --notice` print those of a project, `--from <binary>` reads a binary and `--from-archive <library>` a library. |

The options of `antic` that this page names, and the ones that decide how a program is built:

| Option | Meaning |
|---|---|
| `-o <file>`, `-c`, `-S` | The output file, a library file, the assembly text. |
| `-g` | The line of every statement, and the debug sections kept at the link. |
| `--dev` | Dev mode: the module alone into its object, linked when it defines `main`. Without it a build is a release build. |
| `--front-end`, `--tests` | The lexer, the parser and the checker alone, and the `tests` and `fixtures` blocks kept. |
| `--checks`, `--no-checks`, `--asserts`, `--no-asserts` | The run-time checks and the assertions, against the mode. |
| `--trace`, `--no-trace`, `--trace <pattern>`, `--trace writes`, `--no-hooks` | Tracing and the hook sites, see [Hooks and tracing](#hooks-and-tracing). |
| `--no-reflect` | Drops the field and function lists of the descriptors. |
| `--memory-checks` | The checks of AddressSanitizer at every load and store. |
| `--target <name>`, `--cpu <level>` | The target and the processor level, see [Processor levels](#processor-levels). |
| `--lto full\|thin\|none` | The link of the program and the runtime as bitcode, full by default in a release build, or as objects. |
| `--profile-generate`, `--profile-use <file>` | Instrument a program, and build with the profile that `llvm-profdata merge` wrote. |
| `--lib static\|shared`, `--bundle-runtime`, `--soname`, `--no-runtime`, `--closed` | A library for C with its header, the runtime inside the static one, the major version as the soname of the shared one, a plugin without the runtime, and a program without the exports a plugin binds against. `--bundle-runtime` beside `--lib shared`, `--no-runtime` and `--soname` beside `--lib static` and `--lib static` beside `--dev` are refused. |
| `--linker lld\|platform` | lld of the package, the default, or the linker of the platform. |
| `--framework <name>`, `--linux-lib <name>` | What `link framework` and `link linux` pass. |
| `--inject <I=P>`, `--package-name`, `--package-version` | The provider of an injectable interface, and the package of a library file. |
| `--warnings-as-errors`, `--doc-warnings`, `--warn-undocumented`, `--strip-docs` | Every warning refused, the doc warnings on, every `pub` item without a `///` comment reported, and the doc text left out of a library file. |
| `--dump-tokens`, `--dump-ast`, `--dump-types`, `--dump-ir`, `--dump-opt`, `--dump-llvm` | Print one stage of the compiler and stop. |
| `--print-host-target`, `--print-targets`, `--print-cpu-levels`, `--print-llvm-targets`, `--version` | Print the facts of the targets and the levels, or the version. |

## Reserved words

Keywords: `abstract alloc as assert atomic break catch chan class concrete const constraint continue defer delete destroy dispatch do dup else enum export extern fail fallthrough false fixtures fn for free here if implements import inherits internal is join join_all let none parallel protected provides pub recv return select self send singleton size_of static struct super switch sync tests true try type undo union use variant while worker yield`. The type names are keywords as well: `bool byte char f16 f32 f64 float i8 i16 i32 i64 int str u8 u16 u32 u64 uint` and the C types `c_char c_uchar c_short c_ushort c_int c_uint c_long c_ulong c_longlong c_ulonglong c_size_t c_float c_double c_wchar`. `thread` is reserved, and no rule of the grammar accepts it. `null` is refused with a message that names `none`.

Contextual words: `packed align by in final own transient operator mutable trace compatible simd snapshot keep concurrent synchronized unchecked allow lent`, `inject` and `inject final` before a field, `guarded by` after a field's type, `fatal` and `none` after `catch`, `may fail` after a signature, `never` after `->`, and `link framework` and `link linux` at module level. `alloc`, `free` and `union` name a function of a class after `fn` and a member after `.`.

Tokens beside the operators of C: `:: => -> .. ... ?* ?? ?.`, `as?`, the wrapping forms `+% -% *% <<%`, the saturating forms `+| -| *|`, their compound assignments `+%= -%= *%= <<%= +|= -|= *|=`, `<` and `>` around type parameters and type arguments with `>>` closing two lists, `?` before any type, and `.{` of a direct import.

String prefixes: `r b br f rf x re`.

Built-in functions known by name: `mul_high`, `close(c)` on a channel unless a function `close` is in scope, `take(x.f)` on an `own ?*T` field unless a function `take` is in scope, `splat`, `load`, `store`, `shuffle`, `sum`, `min`, `max` and `dot` of a simd struct, and `select`, `any` and `all` of `anti.simd`.

Not built yet: `show`, `unreachable`, `undefined` and `embed`, which the lexer reads as names.
