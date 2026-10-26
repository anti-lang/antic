---
title: "Generics"
description: "Generic functions and classes, constraints, constant parameters, one copy per use, what can be generic, generics of library files and the size of the code."
summary: "Generic functions and classes with their type parameters, constraints checked where the generic is written and at each use, constant parameters, one compiled copy per use, what can be generic, generics of library files, and the size of the code, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-26T12:00:00+01:00
draft: true
weight: 100
tags: [compilers, programming-languages]
keywords: [generics, type parameters, constraints, constant parameters, monomorphization, templates, library files, code size]
---

A generic is checked once, where it is written, against the constraints of its type parameters. Each use with concrete type arguments then compiles a copy of its own, with the arguments in place. A copy is the code a programmer would have written by hand for those types. It has no boxing, no table of types and no test at run time. A release build merges copies whose code is identical.

## Generic functions and classes

### What you know

Java checks a generic against its bounds and erases the type parameters. One class serves every use, values are boxed, and a cast stands where a value is read. C++ compiles a template per set of arguments and checks its body at each use, so one template gives many functions. C writes the function once per type, or once with `void *` and a size, or as a macro.

### The feature

A function, a struct, a class, a variant and an abstract class take type parameters between `<` and `>` after the name.

- `fn largest<T: lt>(own a: T, own b: T) -> T` has one type parameter with a constraint, which the next section covers.
- `struct Pair<A, B> { first: A, second: B }` has two.
- `struct Ring<T, N: int>` takes a constant beside a type. `N` reads as a value in the body, here the length of an array.
- `variant Maybe<T>`, `class Cell<T>` and `abstract class Source<T>` follow the same form, and a class implements a generic interface as `implements src: Source<int>`.
- `type Point = Pair<int, int>;` gives a use a name.

A use names its arguments, or the compiler infers them.

- A call infers the type arguments from its arguments: `largest(3, 7)` is `largest<int>`.
- A class argument gives them through its bases. With `class Countdown inherits Source<int>`, `drain(&c)` for a parameter `*Source<T>` is `drain<int>`. The walk follows `inherits` up the chain, which has one path, and the first copy of `Source` decides. A class that names `Source<int>` in an `implements` line gives no type argument and writes it out, `drain<int>(&c)`.
- A call writes them out where nothing gives them, `largest<f64>(2.5, 1.5)`.
- A type always writes them: `Pair<int, str>`, `Maybe<int>.Some { value: 6 }`, `Cell<f64> { value: 2.5 }`.
- A literal takes them from the type the context expects: `let p: Pair<int, str> = Pair { first: 1, second: "one" };`.
- A literal of a generic struct infers them from its field values where the context gives none, as a call infers them from its arguments. `Pair { first: 1, second: "one" }` is a `Pair<int, str>`. Where the fields do not settle every argument, the literal writes them out. A field given `none` settles nothing.

In an expression, `<` after a name opens type arguments when a list of types closed by `>` follows it. The token after that `>` must be `(`, `.` or `{`. So `m < 9 && 2 > 1` stays two comparisons. `>>` closes two lists.

Two uses with the same arguments are one type, in every module. A generic that nothing uses compiles to nothing.

A union and a `type` alias take no type parameters. A struct holds the generic field where a union would.

A value of a type parameter moves. A parameter that the function keeps, returns or passes on is marked `own`, and `dup(x)` copies one. [Chapter 11]({{% relref "11-ownership" %}}) gives the rule and its reason.

```anti
//! name: gen
import anti.io;
import anti.text;

fn largest<T: lt>(own a: T, own b: T) -> T
{
	if a < b {
		return b;
	}
	return a;
}

fn largest_int(a: int, b: int) -> int
{
	if a < b {
		return b;
	}
	return a;
}

fn first<T>(items: []T) -> T
{
	return items[0];
}

struct Pair<A, B> { first: A, second: B }

struct Ring<T, N: int> { items: [N]T, head: int }

variant Maybe<T> { Some { value: T }, Nothing }

class Cell<T>
{
	pub value: T,
	pub reads: int = 0,

	pub fn get(self) -> T
	{
		self.reads += 1;
		return self.value;
	}
}

abstract class Source<T>
{
	abstract fn next(self) -> ?T;
}

class Countdown
{
	implements src: Source<int>,

	pub left: int = 3,

	concrete fn next(self) -> ?int
	{
		if self.left == 0 {
			return none;
		}
		self.left -= 1;
		return self.left;
	}
}

type Point = Pair<int, int>;

fn drain(s: *Source<int>) -> int
{
	let total = 0;
	let v = s.next();
	while v != none do {
		total += v;
		v = s.next();
	}
	return total;
}

fn or_zero(m: Maybe<int>) -> int
{
	if let Some s = m {
		return s.value;
	}
	return 0;
}

fn main() -> int
{
	let p: Pair<int, str> = Pair { first: 1, second: "one" };
	let q = Pair<f32, f32> { first: 2.0, second: 3.0 };
	let at = Point { first: 4, second: 5 };
	let r = Ring<u8, 4> { items: [1, 2, 3, 4], head: 0 };
	let some = Maybe<int>.Some { value: 6 };
	let c = Cell<f64> { value: 2.5 };
	let down = Countdown { };
	let m = largest(3, 7);
	let f = largest<f64>(2.5, 1.5);
	let less = m < 9 && 2 > 1;
	let line = f"largest {m} {f} {largest_int(3, 7)}, first {first(r.items[0..4])}, pair {p.first} {p.second} {q.second} {at.second}, maybe {or_zero(some)} {or_zero(Maybe<int>.Nothing)}, cell {c.get()}, drain {drain(&down)}, less {less}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Pair<int, str> {size_of(Pair<int, str>)}, Pair<f32, f32> {size_of(Pair<f32, f32>)}, Pair<u8, bool> {size_of(Pair<u8, bool>)}, Ring<u8, 4> {size_of(Ring<u8, 4>)}, Ring<int, 8> {size_of(Ring<int, 8>)}, Maybe<int> {size_of(Maybe<int>)}, Maybe<u8> {size_of(Maybe<u8>)}, Cell<f64> {size_of(Cell<f64>)}, Cell<u8> {size_of(Cell<u8>)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```
```text output
largest 7 2.5 7, first 1, pair 1 one 3 5, maybe 6 0, cell 2.5, drain 3, less true
sizes: Pair<int, str> 24, Pair<f32, f32> 8, Pair<u8, bool> 2, Ring<u8, 4> 16, Ring<int, 8> 72, Maybe<int> 16, Maybe<u8> 2, Cell<f64> 24, Cell<u8> 24
```

Each copy of a type has the layout of its arguments. `Pair<u8, bool>` is 2 bytes and `Pair<int, str>` is 24. `Ring<u8, 4>` holds four bytes inline, and `Ring<int, 8>` holds 64.

Inference through the base classes follows `inherits` up the chain. `Longer` below inherits `Countdown`, which inherits `Source<int>`, so `sum(&l, 0)` finds `T = int` two levels up. `Letters` names `Source<char>` in an `implements` line, which gives no type argument, and its call writes `count<char>(&w)`.

```anti
//! name: bases
import anti.io;
import anti.text;

abstract class Source<T>
{
	abstract fn next(self) -> ?T;
}

class Countdown inherits Source<int>
{
	pub left: int = 3,

	concrete fn next(self) -> ?int
	{
		if self.left == 0 {
			return none;
		}
		self.left -= 1;
		return self.left;
	}
}

class Longer inherits Countdown { }

class Letters
{
	implements src: Source<char>,

	pub left: int = 2,

	concrete fn next(self) -> ?char
	{
		if self.left == 0 {
			return none;
		}
		self.left -= 1;
		return 'a';
	}
}

fn sum<T: add>(s: *Source<T>, own zero: T) -> T
{
	let total = zero;
	let v = s.next();
	while v != none do {
		total = total + v;
		v = s.next();
	}
	return total;
}

fn count<T>(s: *Source<T>) -> int
{
	let n = 0;
	while s.next() != none do {
		n += 1;
	}
	return n;
}

fn main() -> int
{
	let c = Countdown { };
	let l = Longer { left: 4 };
	let w = Letters { };
	let line = f"sum {sum(&c, 0)} {sum(&l, 0)}, count {count<char>(&w)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
sum 3 6, count 2
```

```ir excerpt=bases target=linux-x86_64 mode=release
    %18 = call i64 @bases.sum<int>(%0, 0)
...
    %23 = call i64 @bases.sum<int>(%1, 0)
...
    %28 = ptradd %2, offset_of bases.Letters.src
    %29 = call i64 @bases.count<char>(%28)
...
fn bases.sum<int>(%0: ptr nonnull deref(size_of bases.Source<int>), %1: i64) -> i64 {
```

The two calls of `sum` reach one copy, `sum<int>`, with the address of each object. A class that inherits `Source<int>` holds it at offset 0, so the address needs no change. The call of `count` adds the offset of the part `src` first, since `Letters` holds `Source<char>` as a sub-object.

The checker refuses each of the following.

```anti refused
//! error: `s` of `count` takes `*Source<T>`, found `*Letters`
abstract class Source<T>
{
	abstract fn next(self) -> ?T;
}

class Letters
{
	implements src: Source<char>,

	concrete fn next(self) -> ?char { return none; }
}

fn count<T>(s: *Source<T>) -> int
{
	return 0;
}

fn letters() -> int
{
	let w = Letters { };
	return count(&w);
}
```

```anti refused
//! error: `items` of `first` takes `[]T`, found `int`
fn first<T>(items: []T) -> T
{
	return items[0];
}

fn one(n: int) -> int
{
	return first(n);
}
```

```anti refused
//! error: a union takes no type parameters
union Bits<T> { whole: T, raw: u64 }
```

```anti refused
//! error: a `type` alias takes no type parameters
type Items<T> = []T;
```

```anti refused
//! error: nothing in the arguments gives `T` of `width`, so the call writes it out after the name
fn width<T>() -> int
{
	return size_of(T);
}

fn bytes() -> int
{
	return width();
}
```

```anti refused
//! error: `largest` takes 1 type argument, found 2
fn largest<T: lt>(own a: T, own b: T) -> T
{
	if a < b {
		return b;
	}
	return a;
}

fn pick() -> int
{
	return largest<int, int>(1, 2);
}
```

```anti refused
//! error: expected `int`, found a float literal
fn largest<T: lt>(own a: T, own b: T) -> T
{
	if a < b {
		return b;
	}
	return a;
}

fn pick() -> int
{
	return largest(1, 2.0);
}
```

```anti refused
//! error: `Pair` takes 2 type arguments
struct Pair<A, B> { first: A, second: B }

fn make() -> int
{
	let p: Pair = Pair { first: 1, second: 2 };
	return p.first;
}
```

```anti refused
//! error: `Ring` takes a constant for `N`, found a type
struct Ring<T, N: int> { items: [N]T, head: int }

fn make() -> int
{
	let r: Ring<int, int> = Ring<int, int> { items: [1], head: 0 };
	return r.head;
}
```

```anti refused
//! error: `a` belongs to the caller and does not move. Mark it `own` to take it over, or copy it with `dup(a)`
fn keep<T>(a: T) -> T
{
	let b = a;
	return b;
}
```

### How antic builds it

At run time a generic does not exist. Each use with concrete arguments is a function or a type of its own, as if the program had written it out for those types. A call reaches it directly. No value carries a type at run time, nothing is boxed, and no table of types is passed. The checker makes each copy from the checked syntax tree of the generic, with the arguments in every type, and checks it as written code. The IR of antic and the LLVM module hold the copies alone, with no type parameter left.

```ir excerpt=gen target=linux-x86_64 mode=release
type gen.Cell<float> = struct { super: anti.lang.Object, value: f64, reads: i64 }
...
type gen.Pair<int, str> = struct { first: i64, second: str }
...
type gen.Ring<byte, 4> = struct { items: [4]byte, head: i64 }
...
fn gen.largest_int(%0: i64, %1: i64) -> i64 {
b0:
    %2 = slt i8 %0, %1
    branch %2, b1, b2
...
fn gen.largest<int>(%0: i64, %1: i64) -> i64 {
b0:
    %2 = slt i8 %0, %1
    branch %2, b1, b2
...
fn gen.largest<float>(%0: f64, %1: f64) -> f64 {
b0:
    %2 = flt i8 %0, %1
```

- A copy is named after its generic and its arguments, `gen.Pair<int, str>`. The name spells an argument by its plain name, so `f64` reads `float` and `u8` reads `byte`.
- Each copy of a type is an ordinary struct, class or variant of the IR. `Cell<float>` has a table pointer and its own table, as any class has.
- `largest_int` is the function written by hand, and `largest<int>` is the copy. Their bodies are the same two instructions: `slt`, a signed compare of two `i64`, and the branch.
- `largest<float>` is a second function. Its compare is `flt`, on two `f64` that travel in float registers.

The LLVM text writes the brackets of a name as escapes, since a symbol of an object file takes neither `<` nor `>`.

```llvm excerpt=gen target=linux-x86_64 mode=dev
  %v44 = call i64 (i64, i64) @gen.largest$3cint$3e(i64 3, i64 7)
...
define weak_odr hidden noundef i64 @gen.largest$3cint$3e(i64 noundef %p0, i64 noundef %p1) #0 {
```

`$3c` is `<` and `$3e` is `>`, each byte outside letters, digits, `_` and `.` as `$` and two hex digits. A debugger and a stack trace show the name as the program writes it, `gen.largest<int>`. The call is direct, to the one copy the arguments chose. `weak_odr` is the linkage of a copy in a dev object, which the section on copies explains.

A library file stores a `pub` generic as its checked syntax tree with the parameters open. A module that uses it makes its copies from that tree, under the path of the module that declares the generic. The section on library files at the end of this chapter shows it.

Where it lives: `sema_generic_call` and `sema_subst` of `src/antic/sema_generic.c` infer and substitute. `sema_compile_copies` of `src/antic/sema_copies.c` clones the tree of each copy. `types_symbol_name` of `src/antic/types.c` writes the name, and `target_mangle` of `src/antic/target.c` the escapes. `docs/notes/generics.md` holds the choices of the passes.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of a copy of a type | the layout of its arguments: `Pair<u8, bool>` 2, `Pair<int, str>` 24 | the same |
| A call of a generic function | one direct call of its copy | the same, or inlined |
| The body of a copy | the instructions of the same function written by hand | the same |
| Boxing, casts, type tests | none | none |
| Heap allocations | 0 beyond what the body allocates | the same |
| Calls into the runtime | 0 beyond those of the body | the same |
| Code | one function per distinct set of arguments that the program uses | the same, less the merged copies |
| Read-only data per copy of a class | one table and one descriptor | the same |

The cost of a generic is its code size and nothing at run time. The section on copies counts the functions of a program.

The choice among the forms that serve more than one type:

- A generic makes one copy per distinct set of arguments, and each copy runs as the hand-written function: `largest<int>` is the two instructions of `largest_int`. Its cost is code, and a release build merges copies whose IR is the same.
- A function over an interface pointer, `*Source<int>` or `*Shape`, has one body for every class, and each call through it is two loads and an indirect call. It suits code that holds objects of more than one class at once, and code whose size matters more than the call.
- A function written once per type is what a generic writes for the program, with the same code. It is the form when the bodies differ, since a generic has one body and no specialisation.
- A `*byte` with a size in the style of C loses the type, and the checker no longer checks the body. No form of Anti needs it for a container.

### Pitfalls

For a Java programmer:

- `Cell<u8>` and `Cell<f64>` are two classes with two layouts and two tables. A `Cell<f64>` holds its `f64` inline, where `Cell<Double>` holds a reference to a box.
- A type argument is any type, `int` and a struct included.
- No raw type exists. `Pair` without arguments is refused wherever nothing gives them.

For a C programmer:

- The copy is typed. Nothing passes a `void *` and a size, and the checker knows the type of every value in the body.

For a C++ programmer:

- The body is checked where the generic is written, against its constraints. An error in it does not wait for an instantiation.
- No specialisation and no overloading by type argument exist. One generic has one body.
- A parameter of type `T` is not copied on `let` or `return`. It moves, and `own` or `dup` says which is meant.

## Constraints

### What you know

Java bounds a type parameter with `T extends Comparable<T>`, and the body may call what the bound declares. C++ checked nothing about a template parameter before concepts: the body compiled or failed at each instantiation, with the error inside the template. C++20 concepts state the requirement in the signature. A C macro checks nothing.

### The feature

A constraint after `:` states what the generic needs from a type argument. The body may use exactly what the constraints give.

| Constraint | What the body may write | Met by |
|---|---|---|
| none | store, copy with `dup`, move, pass on, `size_of(T)` | every type |
| `lt`, `eq` | `a < b` and its derived comparisons, `a == b` | a type with that operator, built in or by `operator fn` |
| `add`, `sub`, `mul`, `div`, `neg` and the other operator names | the operator | the same, and a simd struct for the first five where its lanes take the operator |
| `hash` | `x.hash()` | every type with a hash |
| `iter`, `index` | `for x in c`, `c[i]` | a type with that hook |
| an abstract class, `S: Shape` | the `pub` functions of the interface on a `*S` | a class that implements or inherits it |
| `A + B` | what both give | a type that meets both |

`constraint Summable = add + lt;` names a set, and the name stands wherever a constraint may. `anti.lang` declares `Number`, the arithmetic operators with `lt`, and `Ordered`, which is `eq + lt`.

A simd struct meets `add`, `sub`, `mul` and `div` on lanes of numbers and `neg` on signed or float lanes, through its built-in operators, so `twice<T: add>` takes a `Vec4` and adds it lane by lane. A comparison of a simd struct gives a mask of its lanes and no `bool`. A simd struct therefore meets neither `lt` nor `eq`, and not `Number`.

The checker checks twice. The body is checked against the constraints where the generic is written. Each use is checked where it stands: the type argument must meet every constraint. So an error that a constraint catches names the code that made it, the body or the call. A few rules hold for one copy alone, as the refusals of the next sections show. Their message stands at the line of the generic that the copy breaks.

```anti
//! name: cons
import anti.io;
import anti.text;

struct Money { cents: int }

operator fn lt(a: Money, b: Money) -> bool
{
	return a.cents < b.cents;
}

operator fn add(a: Money, b: Money) -> Money
{
	return Money { cents: a.cents + b.cents };
}

abstract class Shape
{
	abstract fn area(self) -> int;
}

class Square
{
	implements shape: Shape,

	pub side: int = 1,

	concrete fn area(self) -> int
	{
		return self.side * self.side;
	}
}

constraint Summable = add + lt;

fn largest<T: lt>(own a: T, own b: T) -> T
{
	if a < b {
		return b;
	}
	return a;
}

fn capped<T: Summable>(own a: T, own b: T, own top: T) -> T
{
	let sum = a + b;
	if top < sum {
		return top;
	}
	return sum;
}

fn mean<T: Number>(a: T, b: T, two: T) -> T
{
	return (a + b) / two;
}

fn same<T: Ordered>(a: T, b: T) -> bool
{
	return a == b || !(a < b) && !(b < a);
}

fn area_of<S: Shape>(s: *S) -> int
{
	return s.area();
}

fn bytes<T>(a: T) -> int
{
	return size_of(T);
}

fn main() -> int
{
	let m = largest(Money { cents: 5 }, Money { cents: 9 });
	let c = capped(Money { cents: 40 }, Money { cents: 70 }, Money { cents: 100 });
	let sq = Square { side: 3 };
	let texts = same("a", "b");
	let words = bytes("text");
	let line = f"largest {largest(3, 7)} {m.cents}, capped {capped(4, 5, 100)} {c.cents}, mean {mean(3.0, 4.0, 2.0)} {mean(7, 2, 2)}, same {same(2, 2)} {texts}, area {area_of(&sq)}, bytes {bytes(1 as u8)} {bytes(m)} {words}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```
```text output
largest 7 9, capped 9 100, mean 3.5 4, same true false, area 9, bytes 1 8 16
```

The checker refuses each of the following.

```anti refused
//! error: `largest` uses `<` on `T`, which its constraints do not give. Add `lt` to them
fn largest<T>(own a: T, own b: T) -> T
{
	if a < b {
		return b;
	}
	return a;
}
```

```anti refused
//! error: `Point` has no `lt`, which `largest` needs for `T`. A struct or a class has no default order and declares `operator fn lt`
fn largest<T: lt>(own a: T, own b: T) -> T
{
	if a < b {
		return b;
	}
	return a;
}

struct Point { x: int }

fn pick() -> int
{
	let p = largest(Point { x: 1 }, Point { x: 2 });
	return p.x;
}
```

```anti refused
//! error: `str` has no `add`, which `twice` needs for `T`
fn twice<T: add>(a: T) -> T
{
	return a + a;
}

fn doubled() -> str
{
	return twice("a");
}
```

```anti refused
//! error: `T` has no field `count`
fn count_of<T>(a: T) -> int
{
	return a.count;
}
```

```anti refused
//! error: `Other` has no `Shape`, which `area_of` needs for `S`
abstract class Shape
{
	abstract fn area(self) -> int;
}

class Square
{
	implements shape: Shape,

	concrete fn area(self) -> int { return 4; }
}

class Other { pub x: int = 0, }

fn area_of<S: Shape>(s: *S) -> int
{
	return s.area();
}

fn measure() -> int
{
	let o = Other { };
	return area_of(&o);
}
```

```anti refused
//! error: `missing` is no hook of the operator table, no interface and no constraint
constraint Key = eq + missing;
```

```anti refused
//! error: no operand of `operator fn add` names `T`, which no use of the operator can give
struct Money { cents: int }

operator fn add<T>(a: Money, b: Money) -> Money
{
	return a;
}
```

The receiver of a function of an interface constraint needs one path to the interface. A class that reaches `Closable` through two of its parts leaves the copy two sub-objects to choose from. The checker refuses the call in the copy.

```anti refused
//! error: `t` converts to `*Closable` through `ser` and through `w`, name one
abstract class Closable
{
	abstract fn close(self) -> int;
}

abstract class Serializable inherits Closable
{
	abstract fn write(self) -> int;
}

abstract class Writer inherits Closable
{
	abstract fn flush(self) -> int;
}

class Both
{
	implements ser: Serializable,
	implements w: Writer,

	concrete fn close(self) -> int { return 1; }

	concrete fn write(self) -> int { return 2; }

	concrete fn flush(self) -> int { return 3; }
}

fn close_of<T: Closable>(t: *T) -> int
{
	return t.close();
}

fn main() -> int
{
	let b = Both { };
	return close_of(&b);
}
```

### How antic builds it

A constraint is a rule of the checker and leaves no code. In a copy, an operator on a value of the type argument is checked again with the concrete type. It becomes what that type gives it.

```ir excerpt=cons target=linux-x86_64 mode=release
fn cons.largest<cons.Money>(%0: agg cons.Money, %1: agg cons.Money) -> agg cons.Money {
b0:
    %2 = call i8 @cons.lt(%0, %1)
    branch %2, b1, b2
...
fn cons.largest<int>(%0: i64, %1: i64) -> i64 {
b0:
    %2 = slt i8 %0, %1
    branch %2, b1, b2
...
fn cons.area_of<cons.Square>(%0: ptr nonnull deref(size_of cons.Square)) -> i64 {
b0:
    %1 = ptradd %0, offset_of cons.Square.shape
    %2 = load ptr %1 !table
    %3 = mul i64 17, size_of ptr
    %4 = ptradd %2, %3
    %5 = load ptr %4 !entry
    %6 = call i64 %5 via @cons.fn.1(%1) table @cons.Shape.descriptor 17
```

- In `largest<cons.Money>` the `<` of the body is a direct call of `cons.lt`, the `operator fn` of the struct. No table of operators is passed, and nothing is looked up at run time.
- In `largest<int>` the same `<` is the instruction `slt`.
- In `area_of<cons.Square>` the constraint is an interface. The copy takes the address of the `shape` part of the `Square` and calls `area` through the table of that part. [Chapter 9]({{% relref "09-classes" %}}) shows that code for any interface call. A release build binds the call when one class fills the entry.

The three constraint forms therefore cost a direct call, an instruction, or a dispatch, and the form of the type argument decides which.

Where it lives: `sema_meets_hook` and `sema_meets_param` of `src/antic/sema_generic.c` test a type against a constraint. The copy checks an operator again through `sema_check_expr`, as `docs/notes/generics.md` describes.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A constraint | nothing at run time | nothing |
| An operator on a built-in type argument | one instruction, with its dev check where the operator has one | one instruction |
| An operator on a struct or class argument | one direct call of its `operator fn` | the same, or inlined |
| An operator on a simd struct argument | the operation of the simd struct, one vector instruction where the target has the width, as in chapter 8 | the same |
| A function of an interface constraint | one dispatch through the interface table | a dispatch, or a direct call when one class fills the entry |
| Heap allocations, calls into the runtime | 0 | 0 |
| Work of the checker | the body once, and each use once | the same |

The choice among the constraints:

- An operator constraint, `T: lt` or `T: add`, costs an instruction for a built-in type. For a struct or a class it costs a direct call of the `operator fn`, with no dispatch. It is the form for arithmetic and comparison over many types.
- An interface constraint, `S: Shape`, makes a copy per class that still calls through the table of the interface part, as `area_of<cons.Square>` shows. Its gain over a plain `*Shape` parameter is the type: a result or a second parameter of type `S` keeps the class.
- No constraint at all gives a container that stores, moves and measures its values. Every collection of [chapter 16]({{% relref "16-collections" %}}) starts there and adds `eq`, `hash` or `lt` where an operation needs it.

### Pitfalls

For a Java programmer:

- A constraint names an operator, `T: lt`, where Java names an interface with a method. `a < b` is then written on values of `T`.
- An interface constraint, `S: Shape`, takes a pointer to the class itself. The copy still calls the functions of the interface through its table, as a call through a `Shape` would.

For a C programmer:

- A missing operator is an error at the call that passes the type, with the name of the generic and of the type.

For a C++ programmer:

- A body may use only what its constraints give. A use that happens to work for today's arguments, without a constraint, is refused.
- A constraint is checked at each use by name. No substitution failure silently removes a candidate.
- A struct has no `<` unless its module declares `operator fn lt`, so `Ordered` is not met by a struct by default.
- A simd struct meets `add` but not `Number`. A generic that compares its values with `<` takes no simd struct.

## Constant parameters

### What you know

C++ takes a value as a template argument, as `std::array<T, N>` takes its length, and each value gives a type and code of its own. Java has no such parameter: the length of an array is a field of the object, read at run time. C writes a length into a type with a macro or keeps it beside a pointer.

### The feature

`N: int` in the list of a generic is a constant parameter. Its argument is a number that the compiler knows, and in the body `N` reads as a constant of type `int`.

- The argument is an integer literal or the name of a constant, `Row<3>` or `Row<SIDE>`. An argument that is no constant is refused, and so is a type where `N` stands.
- `N` serves where a constant may stand: the length of an array, `[N]int`, the default of a field and a constant of a class, `N * 2`.
- A call infers `N` from the length of an array argument, as it infers a type: `total([1, 2, 3])` is `total<3>`.
- A default of a field that names `N`, as `[0; N]` or `N + 1`, takes the argument of each copy. So does a constant of the class, so `Row<5>.WIDTH` is 10.
- A copy runs the checks that a constant expression runs. `N <<% (N - 4)` with `N = 3` is refused in the copy, as `3 <<% -1` is.
- The check that a constant expression stays in the range of its type is made once, in the generic, where `N` has no value. A copy whose default leaves the range takes what the back end computes. `pub n: i8 = (N + 100) as i8` gives -56 in `Row<100>`.
- The parameter is `int`. `N: i8`, a float and a `bool` are refused.

```anti
//! name: rows
import anti.io;
import anti.text;

const SIDE: int = 4;

class Row<N: int>
{
	pub cells: [N]int = [0; N],
	pub n: int = N + 1,
	pub const WIDTH: int = N * 2;

	pub fn width(self) -> int
	{
		return Row.WIDTH;
	}
}

fn total<N: int>(a: [N]int) -> int
{
	let t = 0;
	for x in a {
		t += x;
	}
	return t + N;
}

fn main() -> int
{
	let r = Row<3> { };
	let s = Row<SIDE> { };
	r.cells[2] = 7;
	let line = f"n {r.n} {s.n}, cell {r.cells[2]}, width {r.width()} {s.width()} {Row<5>.WIDTH}, total {total([1, 2, 3])}, sizes {size_of(Row<3>)} {size_of(Row<SIDE>)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
n 4 5, cell 7, width 6 8 10, total 9, sizes 40 48
```

`Row<3>` is its table pointer, three `int` cells and `n`, 40 bytes. `Row<4>` holds one cell more.

The checker refuses each of the following.

```anti refused
//! error: a constant parameter is written `N: int`
struct Row<N: i8> { cells: [N]int }
```

```anti refused
//! error: `n` must be a constant for `N`
struct Ring<T, N: int> { items: [N]T, head: int }

fn make(n: int) -> int
{
	let r: Ring<u8, n> = Ring<u8, n> { items: [0; 4], head: 0 };
	return r.head;
}
```

```anti refused
//! error: `3 <<% -1` shifts out of range
fn cells<N: int>() -> int
{
	let a: [N <<% (N - 4)]int = [0; N <<% (N - 4)];
	return a.len;
}

fn main() -> int
{
	return cells<3>();
}
```

### How antic builds it

Each value of `N` is a copy of its own, and in the copy `N` is a number. Nothing reads `N` at run time: an array of the copy has its length in its type, and a default or a constant that names `N` is a number in the code.

```ir excerpt=rows target=linux-x86_64 mode=release
type rows.Row<3> = struct { super: anti.lang.Object, cells: [3]int, n: i64 }
...
type rows.Row<4> = struct { super: anti.lang.Object, cells: [4]int, n: i64 }
...
b1:
    %6 = slt i8 %5, 3
...
    store i64 4, %10
...
fn rows.Row<3>.width(%0: ptr nonnull deref(size_of rows.Row<3>)) -> i64 {
b0:
    ret i64 6
...
fn rows.total<3>(%0: agg [3]int) -> i64 {
```

- `Row<3>` and `Row<4>` are two classes with two layouts, two tables and two descriptors. `Row<SIDE>` is `Row<4>`, since the name stands for its value.
- The default `[0; N]` of `r` is a loop over the three cells, `slt i8 %5, 3`, that stores 0 into each. The default `N + 1` is the store of 4.
- `width` returns the constant `WIDTH` of its copy, 6 in `Row<3>`.
- `total<3>` takes an array of three `int` by value. The length came from the argument.

`Row<5>.WIDTH` makes the copy `Row<5>`, its table and its functions, although `main` holds no value of it. A dev object keeps all of them. A release build folds the constant to 10 where it stands and drops the copy, since nothing reaches its table.

Where it lives: `sema_compile_copies` of `src/antic/sema_copies.c` makes the copies, and `sema_subst` and `sema_subst_const` of `src/antic/sema_generic.c` put the number in place of `N`. `lower_class_init` of `src/antic/lower_class.c` writes the defaults of each copy.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of a copy | the layout with `N` in place: `Row<3>` 40, `Row<4>` 48 | the same |
| A read of `N` | a constant | a constant |
| A default `[0; N]` | a loop of `N` stores | the same loop before `opt` |
| Code | one copy of each function per distinct value of `N` | the same, less merged copies and copies nothing reaches |
| Read-only data | one table and one descriptor per copy of a class | the same, for the copies something reaches |
| Heap allocations, calls into the runtime | 0 | 0 |
| Checks inserted | the bounds of an index into `[N]T`, as for any array | none |

The choice among the forms that hold a count:

- A constant parameter puts the count into the type. The array lies inline, with no pointer and no allocation, and each distinct count is one more copy of the code. It suits a few counts known when the program is written, as `Ring<T, N>` of [chapter 16]({{% relref "16-collections" %}}) does for a buffer that never allocates.
- A slice, `[]int`, carries its count at run time in its second word. One function serves every count, and the elements lie wherever the slice points.
- A collection such as `List<T>` holds its count and grows on the heap. It suits a count that changes while the program runs.

### Pitfalls

For a Java programmer:

- `Row<3>` and `Row<4>` are two types with two layouts. A function that takes a `Row<3>` refuses a `Row<4>`.
- Each count adds code. A program that uses 50 lengths has 50 copies of each function it calls.

For a C programmer:

- The length belongs to the type and travels with it, where C passes it beside a pointer.
- An argument is a literal or the name of a constant. `Row<2 + 1>` does not parse as an argument.

For a C++ programmer:

- The parameter is `int`. No `bool`, enum, pointer or type of a class is a constant parameter.
- A constant in a copy is not checked again for its range. A default computed from an argument at the edge of its type wraps, and no message names it.

## Copies per use

### What you know

A Java generic has one compiled body for every use, and its cost is the boxing and the casts. A C++ template is instantiated per set of arguments in every translation unit that uses it. The linker keeps one of the duplicates, and may fold functions whose machine code is identical. The size of the program grows with the number of distinct uses.

### The feature

Every use of a generic with concrete arguments has a compiled copy, and two uses with the same arguments share one.

- A copy of a function is one function, `pick<int>`.
- A copy of a class is its functions, the functions the compiler writes for every class, its table and its descriptor.
- A dev build compiles one module at a time. Each object carries the copies its module uses, and the linker keeps one of each name.
- A release build compiles the whole program, so each copy exists once. It then merges the copies whose code is identical and keeps the first.
- `export type PersonList = List<Person>;` writes a copy into the C header of a library as an exported class. A generic `export fn` is refused, since C has no open generic.
- Every build checks every copy, `antic --front-end` and `anti check` included. An error that a copy alone shows, such as a worker rule broken by one argument, stands in their output as in a build.
- A generic whose copies name ever deeper copies of it, as `deep<T>` calling `deep<Tag<T>>`, would make copies without end, and the checker refuses it.

```anti
//! name: copies
import anti.io;
import anti.text;

export struct Person { age: int }

export struct Order { total: int }

fn pick<T>(items: []T, i: int) -> T
{
	return items[i];
}

class Slot<T>
{
	pub value: T,

	pub fn get(self) -> T
	{
		return self.value;
	}
}

fn oldest(people: []*Person) -> int
{
	return pick(people, 0).age;
}

fn largest(orders: []*Order) -> int
{
	return pick(orders, 0).total;
}

fn first(numbers: []int) -> int
{
	return pick(numbers, 0);
}

fn small(bytes: []u8) -> u8
{
	return pick(bytes, 0);
}

fn main() -> int
{
	let p = Person { age: 30 };
	let o = Order { total: 12 };
	let people = [&p];
	let orders = [&o];
	let numbers = [5];
	let bytes: [2]u8 = [7, 8];
	let sp = Slot<*Person> { value: &p };
	let so = Slot<*Order> { value: &o };
	let si = Slot<int> { value: 9 };
	let line = f"picked {oldest(people[0..1])} {largest(orders[0..1])} {first(numbers[0..1])} {small(bytes[0..2])}, slots {sp.get().age} {so.get().total} {si.get()}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```
```text output
picked 30 12 5 7, slots 30 12 9
```

The program uses `pick` with four type arguments and `Slot` with three.

The checker refuses each of the following.

```anti refused
//! error: an `export fn` cannot be generic. Export a named copy with `export type`
export fn id<T>(own a: T) -> T
{
	return a;
}
```

```anti refused
//! error: the copies of `deep` name ever deeper copies of it
struct Tag<T>
{
	v: T,
}

fn deep<T>(n: int) -> int
{
	return deep<Tag<T>>(n);
}

fn main() -> int
{
	return deep<int>(1);
}
```

### How antic builds it

A dev build keeps every copy. The dump of the IR after antic's own optimizer shows each call with the copy it reaches.

```opt excerpt=copies target=linux-x86_64 mode=dev
fn copies.oldest(%0: agg []*copies.Person) -> i64 {
b0:
    %1 = call ptr @copies.pick<*copies.Person>(%0, 0)
...
fn copies.largest(%0: agg []*copies.Order) -> i64 {
b0:
    %1 = call ptr @copies.pick<*copies.Order>(%0, 0)
...
fn copies.pick<*copies.Person>(%0: agg []*copies.Person, %1: i64) -> ptr {
...
    %9 = load ptr %8 !type ptr
    ret ptr %9
}
fn copies.pick<*copies.Order>(%0: agg []*copies.Order, %1: i64) -> ptr {
...
    %9 = load ptr %8 !type ptr
    ret ptr %9
}
fn copies.pick<int>(%0: agg []int, %1: i64) -> i64 {
```

`pick<*copies.Person>` and `pick<*copies.Order>` are two functions with the same instructions: both index a slice of pointers and load one pointer. `pick<int>` loads an `i64` and differs from them in the IR, and `pick<byte>` loads one byte.

```llvm excerpt=copies target=linux-x86_64 mode=dev
define weak_odr hidden noundef ptr @copies.pick$3c$2acopies.Person$3e(i64 %p0.0, i64 %p0.1, i64 noundef %p1) #0 {
...
define weak_odr hidden noundef ptr @copies.pick$3c$2acopies.Order$3e(i64 %p0.0, i64 %p0.1, i64 noundef %p1) #0 {
...
define weak_odr hidden noundef i64 @copies.pick$3cint$3e(i64 %p0.0, i64 %p0.1, i64 noundef %p1) #0 {
...
define weak_odr hidden noundef zeroext i8 @copies.pick$3cbyte$3e(i64 %p0.0, i64 %p0.1, i64 noundef %p1) #0 {
```

Each copy is `weak_odr` in a dev object. Two modules that both use `pick<int>` each compile it. The linker keeps one definition of the name, as it does for an inline function of C++. `$2a` is the `*` of the pointer type. The dev object of this program holds 22 functions with a type argument in their name. They are 4 copies of `pick`, 3 of `Slot.get`, and 5 functions the compiler writes for each of the 3 copies of `Slot`.

A release build runs `merge_copies` in the whole-program pass. It compares every pair of copies, instruction by instruction, and lets the earlier one serve the calls of the later one. It repeats until nothing changes, since a merge can make two callers identical.

```opt excerpt=copies target=linux-x86_64 mode=release
fn copies.oldest(%0: agg []*copies.Person) -> i64 {
b0:
    %1 = call ptr @copies.pick<*copies.Person>(%0, 0)
...
fn copies.largest(%0: agg []*copies.Order) -> i64 {
b0:
    %1 = call ptr @copies.pick<*copies.Person>(%0, 0)
...
    %54 = call ptr @copies.Slot<*copies.Person>.get(%6)
...
    %58 = call ptr @copies.Slot<*copies.Person>.get(%7)
...
    %62 = call i64 @copies.Slot<int>.get(%8)
...
fn copies.pick<*copies.Person>(%0: agg []*copies.Person, %1: i64) -> ptr {
...
fn copies.pick<int>(%0: agg []int, %1: i64) -> i64 {
```

- `largest` now calls `pick<*copies.Person>` with its slice of `*Order`. The copy over `*Order` is gone from the module.
- `so.get()` on a `Slot<*Order>` calls `Slot<*copies.Person>.get`. The table of `Slot<*Order>` points at the functions of `Slot<*Person>` as well.
- The copies over `int` and `byte` stay, because their IR differs in the type of the load.

```asm excerpt=copies target=linux-x86_64 mode=release
<copies.Slot$3c$2acopies.Person$3e.get>:
movq 0x8(%rdi), %rax
retq
...
<copies.Slot$3cint$3e.get>:
movq 0x8(%rdi), %rax
retq
...
<copies.Slot$3c$2acopies.Person$3e.equals>:
...
<copies.Slot$3cint$3e.equals>:
```

The release object holds 9 functions with a type argument in their name, against 22 in the dev object. No copy of `pick` is among them: after the merge, `opt` inlined the three that were left into their callers. The functions of `Slot` stay, because the tables hold their addresses.

`Slot<*Person>.get` and `Slot<int>.get` are the same two machine instructions and stay two functions. The merge works on the IR of antic, where a pointer and an `i64` are two types. The link of lld runs with `--icf=safe`, or `/OPT:SAFEICF` for a Windows target, which folds identical machine code of functions whose address nothing compares. A link with `--linker platform` folds nothing.

Where it lives: `llvm_emit_linkage` of `src/antic/llvm_emit_call.c` writes `weak_odr`. `merge_copies` and `same_function` of `src/antic/whole.c` merge. `src/antic/linker.c` passes `--icf=safe` and `/OPT:SAFEICF`. `tests/dump/copies_merge.anti` pins the merge with the goldens `copies_merge.opt` and `copies_merge.dev.opt`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Copies of a generic function | one per distinct set of arguments, in every object that uses it | one per set in the program, less the merged ones |
| Functions per copy of a class | its own, and `equals`, `hash`, `destroy`, `copy` and `init` | the same, less what is merged or never reached |
| Read-only data per copy of a class | one table, one descriptor, its field and function lists | the same |
| Copies in this program | 22 functions | 9 functions |
| Copies over two pointer types | two | one |
| Run-time cost of a merged copy | none | none, the calls are direct |
| Work of the build | every copy is checked and compiled | the same, and the pass over all pairs of copies |

A generic used with many value types of different layouts grows the program by one body per type. A generic used with many pointer types costs one body in a release build.

### Pitfalls

For a Java programmer:

- Each distinct type argument adds code. A collection of 20 struct types is 20 sets of functions, where Java has one.
- `Slot<*Person>` and `Slot<*Order>` stay two types to the checker, although a release build runs one body for both.

For a C programmer:

- The symbol of a copy escapes its brackets, as in `copies.pick$3cint$3e`. `anti symbols resolve` and a stack trace print `copies.pick<int>`.

For a C++ programmer:

- A dev build compiles a copy in every module that uses it, as C++ instantiates per translation unit. A release build compiles each once.
- No `extern template` and no explicit instantiation exist. `export type` is the form that puts one named copy into a library for C.
- The merge is by antic's IR. Two copies that differ only in a type of the same size, `i64` against a pointer, stay apart until the linker folds them.

## Generic declaration forms

### What you know

Java and C++ allow a generic method inside a generic class, and a nested class sees the parameters of its outer class. A C++ member template cannot be virtual. Java erases the parameters of a method as it erases those of a class.

### The feature

The generic forms combine with the other features of the language.

- A function of a class takes type parameters of its own, `pub fn top<U>(self, fallback: T, f: fn(T) -> U) -> U`. It is called directly and has no entry in the table, so it cannot be `abstract` and cannot be replaced.
- A type nested in a generic class sees the parameters of the class. Each copy of the class has a copy of the nested type.
- A generic class may be `synchronized` or `concurrent`, and the rules of [chapter 14]({{% relref "14-concurrent-classes" %}}) hold in every copy.
- A generic `worker fn` is checked against the rules of workers of [chapter 13]({{% relref "13-threads" %}}) in every copy, with the concrete types.
- A generic function may be `may fail`.
- A generic type declares operators and language hooks, so `for x in list` works for every copy of a list with `operator fn iter`.
- A function type is a type argument, as in `List<fn(int) -> int>`.

A library file carries neither a function with type parameters of its own inside a class nor a type nested in a generic class. Both forms compile for the module that declares them, and the section on library files below gives the messages another module gets.

```anti
//! name: forms
import anti.io;
import anti.lang;
import anti.text;

class Stack<T>
{
	struct Node { value: T, next: ?*Node }

	head: ?*Node = none,
	pub count: int = 0,

	pub fn push(self, own v: T)
	{
		self.head = alloc Node { value: v, next: self.head };
		self.count += 1;
	}

	pub fn top<U>(self, fallback: T, f: fn(T) -> U) -> U
	{
		let h = self.head else {
			return f(fallback);
		};
		return f(h.value);
	}

	fn destruct(self)
	{
		let at = self.head;
		while at != none do {
			let next = at.next;
			free(at);
			at = next;
		}
	}
}

synchronized class Tally<T: add>
{
	total: T,

	fn construct(self, start: T)
	{
		self.total = dup(start);
	}

	pub fn add(self, v: T)
	{
		self.total = self.total + v;
	}

	pub fn read(self) -> T
	{
		return self.total;
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

fn checked<T: lt>(own v: T, low: T) -> T may fail
{
	if v < low {
		fail "below the lower bound";
	}
	return v;
}

fn half(n: int) -> f64
{
	return n as f64 / 2.0;
}

fn positive(n: int) -> bool
{
	return n > 0;
}

type Numbers = Stack<int>;

fn main() -> int
{
	let s = Numbers { };
	s.push(4);
	s.push(9);
	let empty = Stack<int> { };
	let t = Tally<int>(0);
	t.add(5);
	t.add(6);
	let numbers: [8]int = [1, 2, 3, 4, 5, 6, 7, 8];
	let parts = parallel numbers[0..8] by 4 -> total(0);
	let whole = 0;
	for part in parts {
		whole += part;
	}
	let ok = checked(3, 0) catch fatal;
	let low = checked(2.5, 4.0) catch e { yield -1.0; };
	let line = f"count {s.count}, top {s.top(0, half)} {s.top(0, positive)} {empty.top(-2, half)}, tally {t.read()}, parts {parts.len} {whole}, checked {ok} {low}";
	io.println(line);
	free(line.ptr);
	free(parts.ptr);
	return 0;
}
```
```text output
count 2, top 4.5 true -1, tally 11, parts 4 36, checked 3 -1
```

`s.top(0, half)` infers `U` as `f64` from the function it is given, and `s.top(0, positive)` infers `bool`.

The checker refuses each of the following.

```anti refused
//! error: `go` takes type parameters, so it cannot be `abstract` or replaced
abstract class Runner
{
	abstract fn go<T>(self, t: T) -> int;
}

class Fast inherits Runner
{
	concrete fn go<T>(self, t: T) -> int { return 1; }
}
```

```anti refused
//! error: `checked` may fail and its error is not handled
fn checked<T: lt>(own v: T, low: T) -> T may fail
{
	if v < low {
		fail "below the lower bound";
	}
	return v;
}

fn read() -> int
{
	return checked(3, 0);
}
```

### How antic builds it

Each form is a copy as the earlier sections describe, named by every argument that settles it.

```ir excerpt=forms target=linux-x86_64 mode=release
type forms.Stack<int> = struct { super: anti.lang.Object, head: ptr, count: i64 }
...
type forms.Tally<int> = struct { super: anti.lang.Object, total: i64, (lock): anti.lang.Object lock }
...
type forms.Stack<int>.Node = struct { value: i64, next: ptr }
...
    %85 = call f64 @forms.Stack<int>.top<float>(%0, 0, %82, %84)
...
    %96 = call i8 @forms.Stack<int>.top<bool>(%0, 0, %93, %95)
...
fn forms.checked<int>(%0: i64, %1: i64, %2: ptr nonnull deref(size_of i64)) -> ptr {
...
fn forms.checked<float>(%0: f64, %1: f64, %2: ptr nonnull deref(size_of f64)) -> ptr {
...
fn forms.Stack<int>.top<float>(%0: ptr nonnull deref(size_of forms.Stack<int>), %1: i64, %2: ptr, %3: ptr) -> f64 {
...
fn forms.Stack<int>.top<bool>(%0: ptr nonnull deref(size_of forms.Stack<int>), %1: i64, %2: ptr, %3: ptr) -> i8 zeroext range 0, 2 {
```

- `forms.Stack<int>.Node` is the nested struct of the copy `Stack<int>`, with `value` an `i64`. A `Stack<str>` would have a `Node` of its own with a `str`.
- `forms.Tally<int>` carries the hidden lock of a synchronized class after its field, in the copy as in a class written without parameters.
- `top` has two copies under the one copy of its class, `Stack<int>.top<float>` and `Stack<int>.top<bool>`. Both calls are direct, and neither function stands in the table of `Stack<int>`.
- A `may fail` copy has the signature of any failing function: the result goes through the out pointer `%2`, and the function returns the error or null.

Where it lives: `sema_compile_copies` of `src/antic/sema_copies.c` makes the copies of a nested type and of a function with parameters of its own. `src/antic/antl_tree.c` writes the tree of a generic into the library file.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A function with type parameters of its own | one copy per set of class arguments and function arguments used, called directly | the same, less merged copies |
| A nested type of a generic class | one copy per copy of the class | the same |
| A generic synchronized class | the hidden lock in each object of each copy | the same |
| A generic worker | one copy per type argument, started as any worker | the same |
| A generic `may fail` function | the out pointer and the error result of [chapter 6]({{% relref "06-errors" %}}) | the same |
| Table entries | none for a function with type parameters of its own | none |

The choice among the forms that add a type to a member:

- A function with type parameters of its own is a direct call, one copy per set of arguments, and no entry of the table. It suits a helper such as `top<U>` whose result type each caller chooses. A derived class cannot replace it.
- A `pub` function of the class with fixed types has one body and an entry of the table. A derived class replaces it, and a call may dispatch.
- A type nested in a generic class follows its copies, one `Node` per copy of `Stack`. A type at module level with parameters of its own, `Node<T>`, serves the same and reaches other modules through a library file.

### Pitfalls

For a Java programmer:

- A function with its own type parameters is never dispatched. A derived class cannot replace it.
- A class that nests a type and takes type parameters cannot be used from a library file. It is used inside the module that declares it, or its nested type moves to module level.

For a C programmer:

- The copies of a nested type have the name of their class copy in the symbols, `forms.Stack<int>.Node`.

For a C++ programmer:

- A function with type parameters of its own cannot be `abstract`, as a member template cannot be virtual.
- A generic worker is checked per copy. A copy whose argument holds a pointer is refused, although the generic compiles.

## Generics of library files

### What you know

A C++ template ships as source in a header, and every translation unit that includes it compiles its instantiations. A Java generic ships in a `.class` file as one erased body. C ships a macro in a header, or a function over `void *` in a library.

### The feature

`antic -c` writes the library file of a module, `<module>.antl`, which [chapter 18]({{% relref "18-the-tool" %}}) describes. For every `pub` generic of the module, a function, a struct, a class, a variant, a `constraint` or a `type`, it carries the checked syntax tree with the type parameters open. It carries no source text.

- A module that imports the library makes its copies from that tree with its own arguments. A copy has the path of the module that declares the generic, `com.example.shelf.largest<int>`. Two modules that make one copy therefore make one type and one symbol.
- A copy reaches what the body of the generic reaches, the private functions and constants of the library module among them. `counted` below calls the private `clamp`.
- The library file carries every constant of a generic class that another module may name. Its type and its value keep the type parameters open, and a copy computes the constant with its own arguments. `shelf.Slot<u8>.BYTES` is 1 and `shelf.Slot<i32>.BYTES` is 4.
- The library file carries no function with type parameters of its own inside a class, and no type nested in a generic class. Another module that calls such a function is refused with `` `Plain` has no function `pick` ``. One that makes a copy of a class whose nested type its arguments do not settle is refused with `` antic compiles no copy of `Stack` yet, which names a type its arguments do not settle ``. Both forms compile in the module that declares them.

The library `com/example/shelf.anti`:

```anti
//! name: shelf
const LIMIT: int = 100;

fn clamp(n: int) -> int
{
	if n > LIMIT {
		return LIMIT;
	}
	return n;
}

pub fn largest<T: lt>(own a: T, own b: T) -> T
{
	if a < b {
		return b;
	}
	return a;
}

pub fn counted<T>(items: []T) -> int
{
	return clamp(items.len);
}

pub class Slot<T>
{
	pub const COUNT: int = 3;
	pub const BYTES: int = size_of(T) as int;

	pub value: T,

	pub fn get(self) -> T
	{
		return self.value;
	}
}
```

A program `client.anti` beside it, which needs the library file to compile and so stands in a `text` block:

```text
import anti.io;
import anti.text;
import com.example.shelf;

fn main() -> int
{
	let s = shelf.Slot<u8> { value: 7 };
	let w = shelf.Slot<i32> { value: 9 };
	let items: [3]f64 = [1.0, 2.0, 3.0];
	let line = f"largest {shelf.largest(3, 7)} {shelf.largest(2.5, 1.5)}, counted {shelf.counted(items[0..3])}, get {s.get()} {w.get()}, constants {shelf.Slot<u8>.COUNT} {shelf.Slot<u8>.BYTES} {shelf.Slot<i32>.BYTES}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

The library file goes under `out/`, and the program finds it there with `-I`:

```sh
antic -c -I . -o out/com/example/shelf.antl com/example/shelf.anti
antic -I out -o out/client client.anti out/com/example/shelf.antl
out/client
```

```text
largest 7 2.5, counted 3, get 7 9, constants 3 1 4
```

The tests `modules_generics_release`, `modules_generics_dev`, `modules_generic_constant_release` and `modules_generic_constant_dev` build the same forms in both modes. The tests `error_modules_generic_function_of_a_class` and `error_modules_type_nested_in_a_generic_class` hold the two refusals.

### How antic builds it

A library file holds two kinds of code. Ordinary code is IR, ready to compile. A generic is its checked tree, since no single IR stands for an open body. `a < b` is a signed compare for an `int`, a float compare for an `f64` and a call for a struct. The module that uses a generic compiles its copy from the tree, as if the generic stood in its own source. It names the copy after the module of the generic.

`antic --dump-ir` of the release build of `client.anti`, cut to the lines that matter:

```text
type com.example.shelf.Slot<byte> = struct { super: anti.lang.Object, value: i8 }
type com.example.shelf.Slot<i32> = struct { super: anti.lang.Object, value: i32 }
...
fn com.example.shelf.clamp(%0: i64) -> i64 {
...
    %58 = copy i64 size_of i8
...
    %62 = copy i64 size_of i32
...
fn com.example.shelf.largest<int>(%0: i64, %1: i64) -> i64 {
...
fn com.example.shelf.counted<float>(%0: agg []float) -> i64 {
b0:
    %1 = ptradd inbounds %0, offset_of []float.len
    %2 = load i64 %1 !type i64
    %3 = call i64 @com.example.shelf.clamp(%2)
```

- The two copies of `Slot` are classes of the program under the path of the library, each with the layout of its argument.
- `clamp` is the IR of the library file. A release build compiles the whole program in one call, so the library's own code joins the module of the program.
- `shelf.Slot<u8>.BYTES` is `size_of i8`, the constant `size_of(T) as int` with `u8` in place. The back end folds it to 1, and `Slot<i32>.BYTES` to 4. `COUNT` is the number 3.
- `counted<float>` is the copy that the program made. Its call of the private `clamp` is a direct call of the library's function.

A dev build compiles each module to an object of its own. The object of the library defines `com.example.shelf.clamp` and no copy. The object of the program defines each copy as a weak symbol, `.weak com.example.shelf.largest$3cint$3e`, and calls `clamp` by its symbol. The link keeps one definition of each copy that more than one module made.

Where it lives: `src/antic/antl_tree.c` writes and reads the tree of a generic, and `carried_value` of `src/antic/antl_write.c` the constants of a generic class. `sema_compile_copies` of `src/antic/sema_copies.c` makes the copies of a module and refuses the copy whose nested type it cannot make. `sema_strip_generics` of `src/antic/sema_generic.c` takes the generics out of a module before lowering, so its IR holds copies alone.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of a library file | the IR of the module and the trees of its generics: `shelf.antl` is 2871 bytes | the same |
| Code of a copy | one copy per module that uses it, kept once by the link | one copy per program |
| Code of the library's own functions | once, in the object of the library | once, in the program |
| A constant of a generic class | a number in place | a number in place |
| A call from a copy into the library | a direct call | a direct call, or inlined |
| Work of the build | each module checks and compiles the copies it uses | the same, once for the program |
| Run-time cost | none beyond that of the code | none |

The choice among the forms of a library that serves many types:

- A generic in a library costs each user a copy and nothing at run time. It suits containers and algorithms whose speed depends on the type.
- A function of the library over an interface pointer is compiled once, into the library, and every user calls the same code through the table. It suits behaviour whose cost lies in the work and not in the call. It also suits a plugin boundary of [chapter 15]({{% relref "15-hooks" %}}), where code that the library does not know joins later.
- `export type IntList = List<int>;` puts one named copy into a library for C, as the section on copies shows, since C has no open generic.

### Pitfalls

For a Java programmer:

- A library file holds no compiled body for a generic. Each user compiles its copies, so the generic of a library adds to the build time of its users.

For a C programmer:

- A library ships its generics without a header and without source. The tree in the library file is checked and holds no text.

For a C++ programmer:

- An error in the body of a library generic was reported when the library was built. A user sees an argument that lacks a constraint at its own use.
- A function with type parameters of its own inside a class, and a type nested in a generic class, stay inside their module. A C++ member template in a header has no such limit.
