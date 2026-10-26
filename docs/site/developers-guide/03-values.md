---
title: "Values"
description: "The types and literals of Anti, variables and constants, integers that wrap or saturate, overflow checks, floats and f16, text and bytes, tuples and inference."
summary: "Types, literals, variables and constants, integers with `+%` and `+|` and the overflow checks of dev mode, floats and `f16`, `str` and `[]byte` with their layout, tuples and type inference, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-26T12:00:00+01:00
draft: true
weight: 30
tags: [compilers, programming-languages]
keywords: [types, literals, integer overflow, wrapping arithmetic, saturating arithmetic, f16, strings, tuples, type inference]
---

A scalar value of Anti has a size that is the same on all six targets, and no number converts to another type without `as`. Signed arithmetic is checked in a dev build and wraps in a release build. The wrapping and the saturating operators state the intent of a line in both. This chapter covers the scalar types, text and bytes, and tuples. [Chapter 8]({{% relref "08-structs-enums" %}}) covers structs, enums and variants, and [chapter 12]({{% relref "12-memory" %}}) covers arrays and slices.

Some examples mark a function `export fn`. An exported function keeps its plain name and stays in the release module, so a release excerpt can show it. [Chapter 17]({{% relref "17-c-interop" %}}) describes `export fn`.

## Types and literals

### What you know

Java has eight primitive types of fixed width, an `int` of 32 bits and a `char` that is one UTF-16 code unit. It widens a number implicitly, as from `int` to `long`. In C and C++ the width of `int` and `long` depends on the platform, and `char` is signed on some and unsigned on others. Both promote a narrow integer to `int` before arithmetic. All three fix the type of a literal with a suffix such as `L`, `u` or `f`.

### The feature

The scalar types have fixed widths, and four of them have a second name.

| Type | Second name | Bits | Values |
|---|---|---|---|
| `i8`, `i16`, `i32`, `i64` | `int` for `i64` | 8 to 64 | signed, two's complement |
| `u8`, `u16`, `u32`, `u64` | `byte` for `u8`, `uint` for `u64` | 8 to 64 | unsigned |
| `f32`, `f64` | `float` for `f64` | 32, 64 | IEEE 754 binary |
| `f16` | | 16 | IEEE 754 half, storage alone |
| `bool` | | 8 | `true` and `false` |
| `char` | | 32 | one Unicode scalar value |

The compound types are `str`, the array `[N]T`, the slice `[]T` and the pointers `*T` and `?*T`. The optional `?T`, the function type `fn(A) -> R` and the tuple `(A, B)` complete them. The C types `c_int`, `c_long` and the others exist for bindings and stand in [chapter 17]({{% relref "17-c-interop" %}}).

A literal has no suffix. It takes its type from where it stands, and an integer literal without such a context is an `int`, a float literal a `float`.

| Literal | Forms | Rules |
|---|---|---|
| Integer | `42`, `1_000_000`, `0xFF_FF` | `_` stands between two digits. The prefix `0x` is lower case. No `0b`, no octal, no leading `0` |
| Float | `3.14`, `1.0e9`, `1.5E-3` | digits on both sides of the `.`, so `1.`, `.5` and `1e9` are no literals |
| Character | `'q'`, `'\n'`, `'\u{1F600}'` | exactly one character |
| Boolean | `true`, `false` | |
| Array | `[1, 2, 3]`, `[0; 16]` | `[v; n]` copies `v` into `n` elements |

A literal must fit its type. The checker reads `-128` as one value, so it fits an `i8`.

```anti
let count = 42;
let small: i8 = -128;
let mask: u32 = 0xFFFF_0000;
let ratio = 3.14;
let half: f32 = 0.5;
let letter = 'q';
let smile = '\u{1F600}';
let zeros = [0; 16];
let bytes: [3]u8 = [1, 2, 3];
```

No number converts to another type by itself. `x as T` converts, between any two integer types, between an integer and a float and between the two float types. A `bool` converts to an integer and not back. A `char` converts to `u32` and back, and to no other type.

```anti
let narrow: i32 = 70000;
let wide: int = narrow as int;
let real = narrow as f64;
let whole = 2.9 as int;
let code = 'q' as u32;
let back = 113 as u32 as char;
let one = true as int;
```

The two operands of a binary operator have one type. A literal operand takes the type of the other operand.

```anti
let b: u8 = 200;
let c = b + 1;
let d: u8 = c;
```

The checker refuses each of the following.

```anti refused
//! error: `300` does not fit `i8`
let x: i8 = 300;
```

```anti refused
//! error: invalid character in numeric literal
let big = 1e9;
```

```anti refused
//! error: a decimal literal other than 0 does not start with 0
let mode = 0755;
```

```anti refused
//! error: expected `i64`, found `i32`
let x: i32 = 5;
let y: i64 = x;
```

```anti refused
//! error: the operands of `+` have the types `i32` and `int`
let x: i32 = 1;
let y: int = 2;
let z = x + y;
```

```anti refused
//! error: expected `f32`, found an integer literal
let x: f32 = 1;
```

```anti refused
//! error: cannot convert `char` to `int`
let c = 'a';
let n = c as int;
```

```anti refused
//! error: cannot convert `int` to `bool`
let flag = 1 as bool;
```

### How antic builds it

A scalar value has no header and no box. It is its bytes. They stand in a register while an expression uses them and in a slot of the frame for a local. A field or an element stands inline in its struct, array or object. Each scalar type is one type of the IR and one type of LLVM, on every target.

| Anti | IR of antic | LLVM |
|---|---|---|
| `bool` | `i8` | `i8`, with the range 0 to 1 on a result |
| `i8`, `u8` | `i8` | `i8` |
| `i16`, `u16` | `i16` | `i16` |
| `i32`, `u32`, `char` | `i32` | `i32` |
| `i64`, `u64` | `i64` | `i64` |
| `f32` | `f32` | `float` |
| `f64` | `f64` | `double` |
| `f16` | `i16`, the bits of the half | `i16`, cast to `half` where it converts to or from `f32` |
| `*T`, `?*T` | `ptr` | `ptr` |

Signedness stands in the operation and not in the type: `sdiv` or `udiv`, `sext` or `zext`. A literal is an operand of an instruction and has no code of its own.

```anti
//! name: lits
import anti.io;

fn count() -> i32
{
	return 42;
}

fn ratio() -> f32
{
	return 0.5;
}

fn letter() -> char
{
	return 'q';
}

fn flag() -> bool
{
	return true;
}

fn main() -> int
{
	if count() == 42 && ratio() == 0.5 && letter() == 'q' && flag() {
		io.println("all four");
	}
	return 0;
}
```

```text output
all four
```

```llvm excerpt=lits target=linux-x86_64 mode=dev
define hidden noundef i32 @lits.count() #0 {
b0:
  ret i32 42
}
...
define hidden noundef float @lits.ratio() #0 {
b0:
  ret float 0x3FE0000000000000
}
...
define hidden noundef i32 @lits.letter() #0 {
b0:
  ret i32 113
}
...
define hidden noundef zeroext range(i8 0, 2) i8 @lits.flag() #0 {
b0:
  ret i8 1
}
```

`ret i32 42` returns the literal as an immediate value. `0x3FE0000000000000` is 0.5, which the text writes as the bits of a `double` for both float types. `'q'` is the `i32` 113, its code point. `true` is the `i8` 1. `zeroext` says that the caller may read the byte as a full register. `range(i8 0, 2)` says that it is 0 or 1.

`llvm_emit_call.c` writes a narrow integer parameter or result with `signext` or `zeroext`, by the signedness of its type.

Where it lives: `src/antic/lexer.c` reads a literal. `sema_const.c` and `sema_expr.c` of `src/antic/` give it its type and check that it fits. `src/antic/types.c` holds the types and `src/antic/layout.c` their sizes. `lower_cast` of `src/antic/lower_expr.c` lowers `as`.

### What it costs

The program prints `size_of` of each type. The numbers hold on all six targets.

```anti
//! name: sizes
import anti.io;
import anti.text;

fn show(name: str, bytes: int)
{
	let line = f"{name:<10}{bytes:>5}";
	io.println(line);
	free(line.ptr);
}

fn main() -> int
{
	show("bool", size_of(bool));
	show("byte", size_of(byte));
	show("i16", size_of(i16));
	show("i32", size_of(i32));
	show("int", size_of(int));
	show("f16", size_of(f16));
	show("f32", size_of(f32));
	show("float", size_of(float));
	show("char", size_of(char));
	show("*int", size_of(*int));
	show("[4]i16", size_of([4]i16));
	show("[1000]i32", size_of([1000]i32));
	show("[1000]int", size_of([1000]int));
	return 0;
}
```

```text output
bool          1
byte          1
i16           2
i32           4
int           8
f16           2
f32           4
float         8
char          4
*int          8
[4]i16        8
[1000]i32  4000
[1000]int  8000
```

| Cost | Dev | Release |
|---|---|---|
| Bytes per value | the table above | the same |
| Heap allocations | 0 | 0 |
| Calls into the runtime | 0 | 0 |
| A literal | an immediate operand | the same |
| A widening `as` | one `sext`, `zext` or `fpext` | the same |
| A narrowing `as` between integers | one `trunc`, and a conversion back, a compare and a branch for the range check | one `trunc` |

`size_of` is folded at compile time and costs nothing at run time. The integer section below shows the range check of a narrowing `as`.

The `f"..."` of `show` formats its values into a text in new memory, which `free(line.ptr)` gives back. Most programs of the guide print that way, and "Text and bytes" below describes the form.

The width of an integer is a choice of memory. `int` is the type of every length and index: `s.len`, `xs.len` and the variable of `for i in 0..n` are each an `int`. A count or an index kept in an `int` therefore needs no `as`. A narrower type pays where many values stand side by side, as the last two lines of the program show: `[1000]i32` takes half the bytes of `[1000]int`. Each use of a narrow value beside an `int` costs one `as`, and a narrowing `as` carries a range check in dev mode. `byte` holds raw data and `char` one code point. A text is a `str` of UTF-8, one to four bytes per character, where an array of `char` takes four for every one.

### Pitfalls

For a Java programmer:

- `int` is 64 bits and `float` is 64 bits. The 32-bit types are `i32` and `f32`.
- `char` is 32 bits and holds a whole code point, never half of a surrogate pair.
- `byte` is unsigned, 0 to 255.
- No widening happens by itself. An `i32` passed where an `int` is expected is an error.
- No suffix exists. `1L` and `1.5f` are errors, and the type comes from the context.

For a C programmer:

- No integer promotion happens. `b + 1` on a `u8` is a `u8` and wraps at 256.
- `char` is no byte. A byte is `byte`, and `'a'` is a `char` that converts to `u32` alone.
- A leading `0` is an error, not octal.
- `1e9`, `1.` and `.5` are errors. The literal is `1.0e9`.
- A `bool` is no integer. `if n` on an `int` is an error, and the test is `if n != 0`.

For a C++ programmer:

- No implicit conversion exists in either direction. A narrowing and a widening one are both written `as`.
- A float literal for an `f32` needs no `f`, and an integer literal is never a float.
- `'ab'` is an error. A character literal holds one character.

## Variables and constants

### What you know

Java refuses to read a local before it is assigned, and `final` makes a local single-assignment. C and C++ leave a local without an initialiser indeterminate. A `const` object of C has storage and an address, and `constexpr` of C++ is a value the compiler computes.

### The feature

`let` declares a local variable and always initialises it. The type is optional when the initialiser gives one.

```anti
let x = 5;
let y: f32 = 1.0;
x = x + 1;
y = 2.0;
```

A local is mutable. Its type never changes, so `x = "five"` after `let x = 5;` is an error. A local lives until the end of its block.

`const` declares a constant. It names its type, and its value is a constant expression: literals, other constants, operators, `as`, `size_of` and `.len` of a constant array or `str`. A constant stands at module level, in a block or in a class body. Inside the class its bare name reaches it, in the fields, their defaults and the functions, and outside it `Name.NAME` does. A local or a parameter of the same name wins over the constant. `pub const` makes one of a module visible to importers.

```anti
class Limits
{
	pub const MAX: int = 64;
}
const STEP: int = 2;
const BYTES: int = STEP * size_of(int);
const NAMES: [2]str = ["on", "off"];
let m = Limits.MAX + BYTES;
```

A module holds no variables. A `let` stands in a body alone, and at module level the parser refuses it with `expected an item`. State that outlives a call lives in an object, in a singleton or in a static field of a class. A static field must be `atomic`, so each read is a `load()` and each write a `store(v)` or an `add(n)`, and no two threads race on it. [Chapter 9]({{% relref "09-classes" %}}) covers static fields and singletons.

```anti
//! name: counter
class Counter
{
	pub static atomic hits: int = 0;
	const STEP: int = 2;

	pub fn bump()
	{
		Counter.hits.add(STEP);
	}
}
Counter.bump();
let n = Counter.hits.load();
```

```anti refused
//! error: `count` is `static` and must be `atomic`
class Hits
{
	static count: int = 0;
}
```

A name may be declared again in an inner block, with any type. The inner name hides the outer one until its block ends. A parameter may be hidden the same way in the body of its function.

The program below uses a module constant, a block constant and a hidden name.

```anti
//! name: consts
import anti.io;
import anti.text;

const LIMIT: int = 64;
const GREETING: str = "tea";

export fn limit() -> int
{
	const HALF: int = LIMIT / 2;
	let n = HALF;
	n += LIMIT;
	return n;
}

fn main() -> int
{
	let n = limit();
	{
		let n = GREETING;
		io.println(n);
	}
	let line = f"{n}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
tea
96
```

The checker refuses each of the following.

```anti refused
//! error: expected `=`
let x: int;
```

```anti refused
//! error: `x` is already declared in this block
let x = 5;
let x = 6;
```

```anti refused
//! error: expected `int`, found `str`
let x = 5;
x = "five";
```

```anti refused
//! error: expected `:`
const A = 5;
```

```anti refused
//! error: a variable is not a constant expression
fn next() -> int
{
	let n = 3;
	const B: int = n + 1;
	return B;
}
```

```anti refused
//! error: a call is not a constant expression
fn f() -> int { return 1; }
const A: int = f();
```

```anti refused
//! error: cannot assign to this expression
const A: int = 5;
A = 6;
```

```anti refused
//! error: a constant has no address
const LIMIT: int = 64;
let p = &LIMIT;
```

### How antic builds it

A local whose address is never taken is a temporary of the IR. In the LLVM text each temporary is an `alloca` in the entry block, a store per assignment and a load per use. A local whose address is taken with `&`, and every struct, array, `str` and tuple, is a `slot` of the IR, an `alloca` of its bytes.

A constant has no storage. The checker computes its value, and each use is that value as a literal. This is `limit` of the program above in dev mode.

```llvm excerpt=consts target=linux-x86_64 mode=dev
define internal noundef i64 @limit() #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca ptr, align 8
  store i64 32, ptr %t0, align 8
  %v0 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 32, i64 64)
  %v1 = add nsw i64 32, 64
```

`%t0` is `n`. `store i64 32` is `let n = HALF;` with `HALF` already computed as 32. `LIMIT` is the `64` in the addition. The module holds no global for `LIMIT` or `HALF`. The addition is not folded in dev mode, since it carries the overflow check.

In release mode `opt` turns the three `alloca` into values in registers and folds the function to its result.

```llvm excerpt=consts target=linux-x86_64 mode=release
define internal noundef i64 @limit() #4 {
b0:
  ret i64 96
}
```

A `str` constant is the global of its literal, shared with every equal literal of the module. `GREETING` and a `"tea"` written elsewhere in the module are one global.

Where it lives: `sema_const.c` of `src/antic/` computes a constant expression, and `lower_stmt.c` lowers a `let`. `src/antic/llvm_emit_function.c` writes the `alloca` of each temporary and slot.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A scalar local | one stack slot of its size, a store per write and a load per read | a register, or nothing when `opt` folds it |
| A scalar constant | 0 bytes. Each use is an immediate operand | the same |
| A `str` or array constant | its bytes once in the read-only data | the same |
| Heap allocations | 0 | 0 |
| Calls into the runtime | 0 | 0 |
| Checks | none for the declaration | none |

Initialising every local costs one store in dev mode. In release mode `opt` removes a store that no read follows.

A value known at compile time belongs in a `const`. It costs no slot, no store and no load in either mode, and a use folds into the instruction that reads it. A `let` of the same value is a slot and a store in dev mode, which a release build folds as well. A `const` of a class body and one of the module cost the same and differ in the name that reaches them. A static atomic field is a global of the executable, named `counter.Counter.hits` in the LLVM text of the block `counter` above. Each `add` is one `atomicrmw` instruction and each `load()` one atomic load, sequentially consistent, in both modes.

```llvm excerpt=counter target=linux-x86_64 mode=dev
  store ptr @counter.Counter.hits, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = atomicrmw add ptr %v0, i64 2 seq_cst, align 8
...
  store ptr @counter.Counter.hits, ptr %t1, align 8
  %v1 = load ptr, ptr %t1, align 8
  %v2 = load atomic i64, ptr %v1 seq_cst, align 8
```

The first three lines are `bump`: dev mode keeps the address of the global in a slot, loads it and adds the immediate 2, the value of `STEP`, with one `atomicrmw`. The last three are the `load()` of the block. A release build uses the address of the global directly in both instructions.

### Pitfalls

For a Java programmer:

- A `let` is never `final`. The one read-only name is a `const`, and its value is known at compile time.
- An inner block may declare a name of an outer block again. Java refuses that for locals.

For a C programmer:

- No local is uninitialised, and no flag turns the initialisation off.
- A `const` is no object. It has no address and takes no storage, as an `enum` constant or a `#define` of C.
- A `const` needs its type written.

For a C++ programmer:

- `const` means `constexpr`. A value computed at run time is a `let`.
- `let` is no `auto` for a reference. It always makes a new variable with a copy of the value.
- A function call is no constant expression, so no `constexpr` function exists.

## Operators

### What you know

Java, C and C++ share one precedence table, in which `&` binds looser than `==`, so `x & 1 == 0` of C tests `x & (1 == 0)`. Java evaluates the operands and the arguments of a call from left to right. C leaves that order unspecified, and C++ fixed it in C++17 for some operators, as `<<` and `=`, but not for the arguments of a call. All three have `?:`, `++` and `--`, and an assignment is an expression whose value can be used.

### The feature

The binary operators bind in the order of the table, from the loosest to the tightest. Each level groups from left to right, apart from `??`, which groups from the right.

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
| | `- ! ~ & *` before an operand |
| tightest | `.`, `?.`, a call and an index after an operand |

The order is that of C. `as` binds tighter than every binary operator and looser than a prefix operator, so `-x as u8` is `(-x) as u8`. `1 << 2 + 3` is `1 << 5`. `a ?? b ?? c` is `a ?? (b ?? c)`. `x in lo..hi` binds as `<` does, and [chapter 4]({{% relref "04-flow" %}}) describes it.

The types keep the traps of the C table out. `&&`, `||` and `!` take a `bool` alone, and `&`, `|`, `^` and `~` an integer alone, so `x & 1 == 0` is refused rather than computed. A comparison gives a `bool`, and a chain such as `a < b < c` compares a `bool` with an `int`, which is refused. A condition is a `bool`, never a number or a pointer.

The operands of an operator and the arguments of a call run from left to right. An assignment computes its place before its value, so in `xs[i()] = v()` the call `i()` runs first. `&&` and `||` run their right operand only when the left one does not decide the result.

An assignment is a statement and gives no value. Anti has no `?:`, no `++` and no `--`. A compound assignment, `+=` and the rest, is a statement as well.

```anti
//! name: order
import anti.io;
import anti.text;

fn side(name: str, v: int) -> int
{
	io.println(name);
	return v;
}

fn pair(a: int, b: int) -> int
{
	return a - b;
}

export fn both(a: int, b: int) -> bool
{
	return a > 0 && b > 0;
}

fn main(args: []str) -> int
{
	let n = side("a", 1) + side("b", 2) * side("c", 3);
	let d = pair(side("first", 10), side("second", 4));
	let xs = [0, 0, 0];
	xs[side("place", 1)] = side("value", 5);
	let one = args.len;
	let shift = one << 2 + 3;
	let low = -7 / (one + 1);
	let flag = true as int;
	let line = f"n {n}, d {d}, xs[1] {xs[1]}, shift {shift}, low {low}, flag {flag}, both {both(one, 0)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
a
b
c
first
second
place
value
n 7, d 6, xs[1] 5, shift 32, low -3, flag 1, both false
```

`side("b", 2) * side("c", 3)` binds first, and still `side("a", 1)` runs before it, since the operands run in the order of the text. `true as int` gives 1, and `false as int` 0.

The checker refuses each of the following.

```anti refused
//! error: the operands of `&` have the types `int` and `bool`
let x = 6;
let y = x & 1 == 0;
```

```anti refused
//! error: the operands of `<` have the types `bool` and `int`
let a = 1;
let b = 2;
let c = 3;
let t = a < b < c;
```

```anti refused
//! error: a condition has type `bool`, found `int`
let n = 3;
if n { }
```

```anti refused
//! error: `&&` needs `bool` operands, found `int`
let a = 5;
let b = a && true;
```

```anti refused
//! error: `&` needs integer operands, found `bool`
let a = true;
let b = a & false;
```

```anti refused
//! error: unary `!` needs a `bool`, found `int`
let a = 1;
let b = !a;
```

```anti refused
//! error: expected `;`
let x = 1;
let y = x > 0 ? 1 : 2;
```

```anti refused
//! error: expected `)`
let x = 1;
let y = (x = 2);
```

### How antic builds it

An operator on two scalars is one instruction or one intrinsic of LLVM, and the integer and float sections below give each. No operator of this chapter calls a function, apart from `==` and `<` on a `str` and the conversions of an `f16` below the level `v3`, which call the runtime. An operator of a class is a call, as [chapter 9]({{% relref "09-classes" %}}) describes.

Lowering writes the operands in the order of the text, one temporary each, and then the operation. This is the start of `main` above, after lowering.

```ir excerpt=order target=linux-x86_64 mode=release
fn order.main(%0: agg []str) -> i64 {
...
    %6 = call i64 @order.side(%3, 1)
...
    %10 = call i64 @order.side(%7, 2)
...
    %14 = call i64 @order.side(%11, 3)
    %15 = mulov nsw i64 %10, %14
...
    %17 = addov nsw i64 %6, %15
```

The three calls stand in the order of the text. `mulov` multiplies the results of the second and the third, and `addov` adds the result of the first. The left-out lines build the `str` argument of each call and the overflow checks of `*` and `+`.

`&&` and `||` are branches. The right operand stands in a block of its own, which runs only when the left operand leaves the result open.

```ir excerpt=order target=linux-x86_64 mode=release
fn order.both(%0: i64, %1: i64) -> i8 zeroext range 0, 2 {
b0:
    %2 = sgt i8 %0, 0
    %3 = copy i8 %2
    branch %2, b1, b2
b1:
    %4 = sgt i8 %1, 0
    %3 = copy i8 %4
    jump b2
b2:
    ret i8 %3
}
```

`%3` holds the result. It takes the left comparison, and `b1` replaces it with the right one when the left one is true. The IR is not in SSA form, so `%3` is assigned on both paths. The LLVM text of a dev build keeps the branch. In release mode `opt` sees that the right operand cannot trap and has no effect, and computes both comparisons with no branch.

```llvm excerpt=order target=linux-x86_64 mode=release
define internal noundef zeroext range(i8 0, 2) i8 @both(i64 noundef %p0, i64 noundef %p1) #4 {
b0:
  %v1 = icmp sgt i64 %p0, 0
  %v7 = icmp sgt i64 %p1, 0
  %spec.select = and i1 %v1, %v7
```

`and i1` joins the two comparisons. `opt` removes a branch only where running the right operand early changes nothing. A right operand that may trap, or that has an effect such as a call that writes, keeps its branch.

Where it lives: `parser_precedence` and `binary_links` of `src/antic/parser_expr.c` hold the table. `src/antic/sema_expr.c` checks the types of the operands, and `src/antic/lower_expr.c` writes the operands in order and the branches of `&&` and `||`.

### What it costs

| Operation | Dev | Release |
|---|---|---|
| a comparison of two scalars | one instruction | one instruction |
| `&&`, `\|\|` | one branch per operator | one `and` or `or` when the right operand is free of effects, a branch otherwise |
| `!`, `~` | one instruction | one instruction |
| `b as int` of a `bool` | one `zext` | one `zext` |
| the order of evaluation | none: each operand is computed once, where it stands | none |
| Heap allocations | 0 | 0 |
| Calls into the runtime | 0, apart from `==` and `<` on a `str` and an `f16` below `v3` | the same |

The order of evaluation adds no instruction: lowering computes each operand once, in the order of the text, as the IR of `order` shows. `==` and `<` on a `str` call `anti_rt_same_bytes` and `anti_rt_compare_bytes` of `src/rt/text.c` with the pointer and the length of both sides. A condition of two cheap tests costs no branch in a release build, as `both` shows.

### Pitfalls

For a Java programmer:

- An assignment gives no value. `while ((line = next()) != none)` has no counterpart, and the loop reads the value in its body.
- `?:` does not exist. A `let` before an `if` that assigns it in each arm takes its place.
- `&` and `|` take no `bool`. The non-short-circuit forms of Java have no counterpart.

For a C programmer:

- `x & 1 == 0` is refused, not computed. The parentheses are written: `(x & 1) == 0`.
- The order of the operands is defined, so `f(i(), j())` runs `i` first on every target.
- `if (p)` and `if (n)` are refused. The tests are `p != none` and `n != 0`.

For a C++ programmer:

- `++` and `--` do not exist. `i += 1` is the statement.
- An operator never converts its operands. `a < b` of an `i32` and an `int` is refused, where C++ promotes.
- The arguments of a call run from left to right, which C++ leaves open.

## Integers

### What you know

Java wraps every integer operation in two's complement and throws on a division by zero. `Math.addExact` is the checked form. In C and C++ an unsigned operation wraps. A signed overflow, a division by zero and a shift by the width or more are undefined behaviour there. Compilers offer checked builtins such as `__builtin_add_overflow`.

### The feature

Anti has three families of arithmetic operators. Each states what happens when a result leaves the range of its type.

| Operators | On overflow | Types |
|---|---|---|
| `+ - *` | signed: a dev build stops with a message, a release build wraps. Unsigned: wraps in both | every number |
| `+% -% *% <<%` | wraps in every build | integers |
| `+\| -\| *\|` | clamps at the least or the greatest value of the type | integers |

Every operator has a compound assignment, `+=`, `+%=` and `+|=` among them. Anti has no `++` and no `--`.

The other integer operators are `/` and `%`, the shifts `<<` and `>>`, the bit operators `& | ^ ~` and unary `-`.

- `/` truncates toward zero, and `%` takes the sign of the left operand: `-7 / 2` is `-3` and `-7 % 2` is `-1`.
- `>>` on a signed type keeps the sign, and on an unsigned type shifts zeros in.
- Unary `-` takes a signed integer or a float. On an unsigned type it is an error.
- `mul_high(a, b)` gives the upper half of the full product of two integers of one type.

A dev build inserts a check before each operation of the table. A failed check prints the file, the line, the operation and the values to the standard error and aborts. A release build drops the check and gives the result in the last column, the same on every target.

| Operation | Dev build prints | Release build gives |
|---|---|---|
| signed `+ - *` out of range | `overflow in +: left 9223372036854775807, right 1` | the wrapped result |
| `/` or `%` by zero | `division by zero in /: left 7` | 0 |
| the least value divided by `-1`, or its remainder | `overflow in /: left -9223372036854775808, right -1` | the least value, and 0 for the remainder |
| unary `-` of the least value | `overflow in -: value -9223372036854775808` | the least value |
| shift count below 0 or at or above the width | `shift count out of range for <<: count 65, width 64` | the shift by the count modulo the width |
| `<<%` by a count below 0 | `shift count out of range for <<%: count -1, width 64` | 0 |
| `as` to an integer type that cannot hold the value | `value out of range for i16: value 70000` | the low bits |
| `as` to `char` outside the Unicode scalar values | `value out of range for char: value 1114112` | the value, unchecked |

Each message starts with the file and the line, as in `ints.anti:7: overflow in +`. Two cases have a defined result and no check in either mode. `<<%` gives 0 for a count at or above the width. An unsigned `+`, `-` or `*` wraps.

With constant operands the checker refuses every operation of the table. `127 + 1` as an `i8`, `7 / 0`, `1 << 70`, `70000 as i16` and `0xD800 as u32 as char` are compile errors. So are the least value divided by `-1`, unary `-` of the least value and `<<%` by a count below zero. A `const` is held to the range of an unsigned type as well: `const N: u8 = 200 + 100;` is refused, and `200 +% 100` gives 44.

The program runs each case in release mode.

```anti
//! name: ints
import anti.io;
import anti.text;

export fn checked(a: int, b: int) -> int
{
	return a + b;
}

export fn wrapping(a: int, b: int) -> int
{
	return a +% b;
}

export fn saturating(a: int, b: int) -> int
{
	return a +| b;
}

export fn divide(a: int, b: int) -> int
{
	return a / b;
}

export fn narrow(a: int) -> i16
{
	return a as i16;
}

fn show(name: str, value: int)
{
	let line = f"{name} = {value}";
	io.println(line);
	free(line.ptr);
}

fn main(args: []str) -> int
{
	let max: int = 9223372036854775807;
	let one = args.len;
	show("max + 1", checked(max, one));
	show("max +% 1", wrapping(max, one));
	show("max +| 1", saturating(max, one));
	show("7 / 0", divide(7, one - 1));
	show("-7 / 2", divide(-7, one + 1));
	show("-7 % 2", -7 % (one + 1));
	show("1 << 65", 1 << (one + 64));
	show("1 <<% 65", 1 <<% (one + 64));
	show("-8 >> 1", -8 >> one);
	show("70001 as i16", narrow(70000 + one) as int);
	let b: u8 = 200;
	show("200 + 100 as u8", (b + 100 * (one as u8)) as int);
	show("200 +| 100 as u8", (b +| 100 * (one as u8)) as int);
	show("mul_high(max, 4)", mul_high(max, one + 3));
	return 0;
}
```

```text output
max + 1 = -9223372036854775808
max +% 1 = -9223372036854775808
max +| 1 = 9223372036854775807
7 / 0 = 0
-7 / 2 = -3
-7 % 2 = -1
1 << 65 = 2
1 <<% 65 = 0
-8 >> 1 = -4
70001 as i16 = 4465
200 + 100 as u8 = 44
200 +| 100 as u8 = 255
mul_high(max, 4) = 1
```

A dev build of the same program stops at the first line of `main` that calls `checked`. The texts of its checks stand in the dev module as constants.

```llvm excerpt=ints target=linux-x86_64 mode=dev
overflow in +\00"
...
division by zero in /\00"
...
value out of range for i16\00"
...
shift count out of range for <<\00"
```

`let (result, flags) = e;` gives the wrapped result of one integer operation and its flags. The form takes a pair apart into two variables, as "Tuples" below describes. `e` is one `+`, `-`, `*`, `<<`, `>>` or unary `-`. `flags` has the built-in struct type `Flags` with four `bool` fields: `overflow`, `carry`, `zero` and `negative`. The form never stops a program, in any build.

`a + b + f.carry` adds the carry of an earlier operation, and `a - b - f.carry` subtracts its borrow.

```anti
//! name: wide
import anti.io;
import anti.text;

export struct Wide
{
	lo: u64,
	hi: u64,
}

export fn add_wide(a: *Wide, b: *Wide, sum: *Wide)
{
	let (lo, f) = a.lo + b.lo;
	sum.lo = lo;
	sum.hi = a.hi + b.hi + f.carry;
}

fn main() -> int
{
	let a = Wide { lo: 18446744073709551615, hi: 0 };
	let b = Wide { lo: 1, hi: 0 };
	let sum = Wide { lo: 0, hi: 0 };
	add_wide(&a, &b, &sum);
	let line = f"lo {sum.lo} hi {sum.hi}";
	io.println(line);
	free(line.ptr);
	let small: u8 = 200;
	let (r, flags) = small + 100;
	let report = f"r {r} carry {flags.carry} overflow {flags.overflow} zero {flags.zero}";
	io.println(report);
	free(report.ptr);
	let big: i8 = 127;
	let w: i8 = 0;
	let g = Flags { overflow: false, carry: false, zero: false, negative: false };
	(w, g) = big + 1;
	let (c, h) = 127 as i8 + 1;
	let again = f"w {w} overflow {g.overflow} negative {g.negative}, c {c} overflow {h.overflow}";
	io.println(again);
	free(again.ptr);
	return 0;
}
```

```text output
lo 0 hi 1
r 44 carry true overflow false zero false
w -128 overflow true negative true, c -128 overflow true
```

`overflow` is the signed overflow and `carry` the unsigned one. 200 plus 100 in a `u8` carries out of 8 bits. Read as signed bytes the sum is -56 plus 100, which fits.

The form assigns as well: `(w, g) = e;` writes two variables that exist, and a variable whose address the program took is such a place too. The operation of the form is no constant expression. `127 as i8 + 1` alone is refused for its range, and inside the form it gives the wrapped result and its flags, as `c` shows. A shift keeps its rule, so a constant count below zero stays refused there.

The checker refuses each of the following.

```anti refused
//! error: `+%` needs integer operands, found `float`
let a = 1.5;
let b = a +% a;
```

```anti refused
//! error: unary `-` needs a signed integer or a float, found `u8`
let u: u8 = 5;
let v = -u;
```

```anti refused
//! error: `%` needs integer operands, found `float`
let a = 1.5;
let b = a % 2.0;
```

```anti refused
//! error: expected an expression
let x = 5;
let y = x++;
```

```anti refused
//! error: `127 + 1` does not fit `i8`
let c = 127 as i8 + 1;
```

### How antic builds it

An integer operation is the machine instruction of its width, on values in registers. A dev build follows each checked operation with a test and a branch to a cold block, which calls `anti_rt_check_failed` with the operands and never returns. A program that passes its checks pays the test and the branch: one `jo` after an `addq` on x86_64. A release build removes the cold block before the LLVM text is written, so the instruction stands alone. Its result carries no `nsw`, so LLVM never assumes that a signed result stays in range.

Lowering writes a checked `+` as the IR operation `addov` followed by `branchov`, as [chapter 2]({{% relref "02-how-antic-works" %}}) shows for `*`. The LLVM text writes the pair as the signed `with.overflow` intrinsic of the width and a branch on its bit. This is `checked` in dev mode.

```llvm excerpt=ints target=linux-x86_64 mode=dev
define internal noundef i64 @checked(i64 noundef %p0, i64 noundef %p1) #0 {
...
  %v2 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %v0, i64 %v1)
  %v3 = add nsw i64 %v0, %v1
  %v4 = extractvalue { i64, i1 } %v2, 1
  store i64 %v3, ptr %t2, align 8
  br i1 %v4, label %b1, label %b2, !prof !3
...
  call void (ptr, i64, i32, i64, i64) @anti_rt_check_failed(ptr %v5, i64 26, i32 1, i64 %v6, i64 %v7)
  unreachable
```

The intrinsic gives the sum and the overflow bit. `add nsw` is the sum as a value, and `nsw` holds because the branch leaves on overflow. `!prof !3` marks the failure arm as cold. The call passes the text, its length 26, the kind 1 for an overflow and the two operands. On x86_64 `llc` turns the block into `addq` and `jo`, and on arm64 into `adds` and `b.vs`.

In release mode the check is gone before the LLVM text is written. `checked` and `wrapping` are then the same function. The add carries no `nsw`, so LLVM may not assume that it stays in range.

```llvm excerpt=ints target=linux-x86_64 mode=release
define internal noundef i64 @checked(i64 noundef %p0, i64 noundef %p1) #4 {
b0:
  %v2 = add i64 %p1, %p0
  ret i64 %v2
}
...
define internal noundef i64 @wrapping(i64 noundef %p0, i64 noundef %p1) #4 {
b0:
  %v2 = add i64 %p1, %p0
  ret i64 %v2
}
...
define internal noundef i64 @saturating(i64 noundef %p0, i64 noundef %p1) #4 {
b0:
  %v2 = tail call i64 @llvm.sadd.sat.i64(i64 %p0, i64 %p1)
  ret i64 %v2
}
```

`+|` is the intrinsic `llvm.sadd.sat`, or `llvm.uadd.sat` on an unsigned type. `-|` is `ssub.sat` or `usub.sat`. `*|` multiplies in twice the width and clamps the product with selects. The machine code of the saturating add has no branch.

```asm excerpt=ints target=linux-x86_64 mode=release
<saturating>:
leaq (%rdi,%rsi), %rcx
sarq $0x3f, %rcx
movabsq $-0x8000000000000000, %rax
xorq %rcx, %rax
addq %rsi, %rdi
cmovnoq %rdi, %rax
retq
```

The first four instructions compute the clamp value from the sign of the wrapped sum. It is the greatest `int` when the sum wrapped to a negative number and the least otherwise. `addq` adds again and sets the overflow flag, and `cmovnoq` takes the sum when the flag is clear. On arm64 the function is four instructions: `adds`, `asr`, `eor` and `csel`.

A release build keeps the defined results of a division in the code. The dev check is dropped, and the selects stay.

```llvm excerpt=ints target=linux-x86_64 mode=release
define internal noundef i64 @divide(i64 noundef %p0, i64 noundef %p1) #4 {
b0:
  %v2 = icmp eq i64 %p1, 0
  %v3 = icmp eq i64 %p1, -1
...
  %v5 = select i1 %v4, i64 1, i64 %p1
  %v6 = sdiv i64 %p0, %v5
  %v7 = sub i64 0, %p0
  %v8 = select i1 %v3, i64 %v7, i64 %v6
  %v9 = select i1 %v2, i64 0, i64 %v8
  ret i64 %v9
}
```

`%v5` is the divisor, replaced by 1 when it is 0 or -1. `sdiv` therefore never sees a pair that LLVM leaves undefined. `%v8` takes `0 - a` for a divisor of -1, and `%v9` takes 0 for a divisor of 0. On arm64 the function is eight instructions and `ret`, with one `sdiv` and no branch. On x86_64 `llc` adds a branch of its own: a 64-bit `idivq` is slow, so the code tests whether both operands fit in 32 bits and then takes `divl`. A divisor that `opt` can prove to be neither 0 nor -1, such as a constant, leaves the `sdiv` alone.

A shift masks its count, `and i64 %count, 63` before `shl`, which x86_64 and arm64 do in the shift instruction itself, so the machine code holds no `and`. `<<%` compares its count with the width and selects 0 at or above it, a `cmp` and a `cmov` or `csel`. A narrowing `as` is one `trunc` in release mode. In dev mode the value is also converted back and compared with the original, which is the range check.

A carry in becomes one add-with-carry instruction. This is `add_wide` in release mode.

```asm excerpt=wide target=linux-x86_64 mode=release
<add_wide>:
movq (%rdi), %rax
addq (%rsi), %rax
movq %rax, (%rdx)
movq 0x8(%rsi), %rax
adcq 0x8(%rdi), %rax
movq %rax, 0x8(%rdx)
retq
```

`addq (%rsi), %rax` adds the low words and sets the carry flag. The two `movq` lines after it leave the flag alone. `adcq 0x8(%rdi), %rax` adds the high words and that carry in one instruction. arm64 writes the pair as `adds` and `adc`. A flag that a function never reads costs nothing.

Where it lives: `lower_binary_checks` of `src/antic/lower_check.c` writes the checks of an operator and `lower_narrow_check` of the same file the one of `as`. `optimize_drop_failures` of `src/antic/optimize.c` drops them. `src/antic/arith.c` computes the wrapping and saturating results of constants, and `src/antic/llvm_emit_function.c` writes the intrinsics. `anti_rt_check_failed` of `src/rt/check.c` prints the message.

### What it costs

| Operation | Dev | Release |
|---|---|---|
| signed `+ -` | the instruction and one branch on the overflow flag | the instruction |
| signed `*` | x86_64: `imulq` and `jo`. arm64: `smulh`, `mul`, `cmp`, `b.ne` | the instruction |
| unsigned `+ - *`, and `+% -% *%` | the instruction | the instruction |
| `+\| -\|` | no check. x86_64: 6 instructions. arm64: 4 | the same |
| `/` and `%` | a compare and a branch for the zero check, and on a signed type a second test and branch for the least value divided by -1, then the release code | the division and three selects |
| `<< >>` | a compare and a branch on the count, then the release code | the shift |
| `<<%` | on a signed type a compare and a branch for a count below zero, then the release code | the shift, a compare and a select |
| narrowing `as` | a conversion back, a compare and a branch | one `trunc` |
| flags form | no check. One instruction per flag read | the same |

A flag costs one instruction where the function reads it, as `setb` after the add on x86_64 or `cset` on arm64.

```anti
//! name: flagcost
export fn carry_of(a: u64, b: u64) -> bool
{
	let (sum, f) = a + b;
	return f.carry;
}
```

```asm excerpt=flagcost target=linux-x86_64 mode=release
<carry_of>:
addq %rsi, %rdi
setb %al
retq
```

The sum is computed for its flag alone, and `setb` copies the carry flag into the result. A flag the function never reads costs nothing, as `add_wide` above shows for `zero` and `negative`.

An integer value takes the bytes of its type, 1 to 8. No operation of this section allocates memory or takes a lock. The one call into the runtime is `anti_rt_check_failed` on the failure arm, which a program that passes its checks never runs.

`--no-checks` drops the checks of a dev build, and `--checks` keeps them in a release build. Without the checks a signed overflow wraps without a message, a division by zero gives 0 and a wrong shift count is masked. A program that relied on a check to stop then continues with the defined result.

The operator states the intent, and the counts above follow from it.

| Intent | Form | Cost in release |
|---|---|---|
| ordinary arithmetic, where an overflow is a defect | `+ - *` | one instruction, and the dev build finds the defect |
| arithmetic modulo 2 to the width, as in a hash or a checksum | `+% -% *% <<%` | one instruction, and no dev check stops the program |
| a value held at the end of its range, as an audio sample | `+\| -\| *\|` | for an `int`, six instructions for `+\|` on x86_64 and four on arm64, thirteen and twelve for `*\|`, with no branch |
| a carry into the next word, or an overflow the program handles | the flags form | the instruction, and one more per flag read |
| the upper half of a product | `mul_high` | one instruction: `mulx` at `v3` or `imulq` on x86_64, `umulh` or `smulh` on arm64 |

`+%` is the form for a computation that is meant to wrap, since `+` stops it in a dev build. `+` is the form for any other, since its release code is the same instruction as that of `+%`, and a dev build checks it.

### Pitfalls

For a Java programmer:

- A signed overflow stops a dev build. Java wraps without a word. `+%` is the Java `+`.
- A division by zero throws no exception. A dev build aborts, and a release build gives 0.
- `>>>` does not exist. An unsigned type with `>>` shifts zeros in.
- `byte` is unsigned, and `b + 1` on a `byte` stays a `byte`.

For a C programmer:

- A signed overflow is defined: a stop in dev mode, a wrap in release mode. The compiler never assumes that it cannot happen.
- A narrow unsigned type is not promoted. `200 + 100` in a `u8` is 44.
- A shift by the width or more and a division by zero are defined in release mode, as the table gives them.
- Unsigned overflow is never checked. A `uint` that goes below 0 wraps in a dev build too.

For a C++ programmer:

- No integer conversion is implicit, so no comparison mixes signed and unsigned.
- A narrowing `as` is checked in dev mode. `static_cast` never is.
- `+|` replaces `std::add_sat` of C++26, and the flags form replaces the checked builtins.

## Floats and f16

### What you know

`float` and `double` of Java, C and C++ are the IEEE 754 binary types of 32 and 64 bits. C and C++ convert between them and from integers implicitly, and a compiler may contract `a * b + c` into one fused instruction. C has `_Float16` on some targets, and Java converts half-precision bits with `Float.float16ToFloat`.

### The feature

`f32` and `f64` are the two float types, and `float` is a second name for `f64`. A float literal without a context is a `float`.

The operators are `+ - * /`, unary `-` and the six comparisons. `%` takes no float. Arithmetic follows IEEE 754 on every target: a division by zero gives an infinity, `0.0 / 0.0` gives a NaN, and nothing is checked in any build. A comparison with a NaN is false, except `!=`, which is true.

`as` converts in four directions.

| Conversion | Result |
|---|---|
| integer to float | the nearest float, ties to even |
| float to integer | the value truncated toward zero. Out of range it saturates at the least or the greatest value of the type, and a NaN gives 0 |
| `f32` to `f64` | exact |
| `f64` to `f32` | the nearest `f32` |

`f16` is the half-precision type, for storage alone. A field, an array element, a slice element or a variable may have it. A read of an `f16` gives an `f32`, so arithmetic on halves is `f32` arithmetic. A write takes `as f16`, which rounds to the nearest half. A value above the range of a half becomes an infinity.

```anti
//! name: floats
import anti.io;
import anti.text;

export struct Sample
{
	level: f16,
	gain: f32,
}

export fn scale(x: f64, y: f64) -> f64
{
	return x * y + 0.5;
}

export fn below(x: f64, y: f64) -> bool
{
	return x < y;
}

export fn to_int(x: f64) -> int
{
	return x as int;
}

export fn level(s: *Sample) -> f32
{
	return s.level * s.gain;
}

export fn set_level(s: *Sample, v: f32)
{
	s.level = v as f16;
}

fn say(line: str)
{
	io.println(line);
	free(line.ptr);
}

fn main(args: []str) -> int
{
	let one = args.len as float;
	let zero = one - 1.0;
	let third: f32 = (one as f32) / 3.0;
	say(f"f32 1/3 = {third}");
	say(f"f64 1/3 = {one / 3.0}");
	say(f"0.1 + 0.2 = {0.1 * one + 0.2}");
	say(f"1 / 0 = {one / zero}");
	let nan = zero / zero;
	say(f"nan == nan is {nan == nan}, nan != nan is {nan != nan}");
	say(f"2.9 as int = {to_int(2.9)}, -2.9 as int = {to_int(-2.9)}");
	say(f"1.0e30 as int = {to_int(1.0e30)}, nan as int = {to_int(nan)}");
	say(f"scale(3, 4) = {scale(3.0, 4.0)}, below = {below(one, 2.0)}");
	let s = Sample { level: 0.0 as f16, gain: 2.0 };
	set_level(&s, third);
	say(f"f16 1/3 = {s.level}, level = {level(&s)}");
	set_level(&s, 65536.0);
	say(f"65536 as f16 = {s.level}");
	say(f"size_of(Sample) = {size_of(Sample)}");
	return 0;
}
```

```text output
f32 1/3 = 0.33333334
f64 1/3 = 0.3333333333333333
0.1 + 0.2 = 0.30000000000000004
1 / 0 = inf
nan == nan is false, nan != nan is true
2.9 as int = 2, -2.9 as int = -2
1.0e30 as int = 9223372036854775807, nan as int = 0
scale(3, 4) = 12.5, below = true
f16 1/3 = 0.33325195, level = 0.6665039
65536 as f16 = inf
size_of(Sample) = 8
```

The runtime computes the digits of a float from its exact value with integers and never asks the C library. Every target therefore prints the same text.

The checker refuses each of the following.

```anti refused
//! error: expected `f64`, found `f32`
let f: f32 = 1.5;
let g: f64 = f;
```

```anti refused
//! error: expected `int`, found a float literal
let x = 3;
let y = x * 2.5;
```

```anti refused
//! error: the operands of `<` have the types `int` and `float`
let a = 7;
let b = 2.0;
let c = a < b;
```

```anti refused
//! error: expected `f16`, found a float literal
let h: f16 = 1.5;
```

```anti refused
//! error: `1.0e40` does not fit `f32`
let f: f32 = 1.0e40;
```

### How antic builds it

A float lives in a floating-point register of the processor: an `xmm` register on x86_64, and a `d` or `s` register on arm64. Each operation is one instruction on it. No mode checks a float, so an operation compiles to the same instruction in both modes. A dev build keeps the locals in memory around it.

A float operation is one LLVM instruction: `fadd`, `fsub`, `fmul`, `fdiv` or `fcmp`. The text carries no fast-math flag, so LLVM reorders nothing and fuses no multiply with an add.

```llvm excerpt=floats target=linux-x86_64 mode=release
define internal noundef double @scale(double noundef %p0, double noundef %p1) #4 {
b0:
  %v2 = fmul double %p0, %p1
  %v4 = fadd double %v2, 5.000000e-01
  ret double %v4
}
...
  %v2 = fcmp olt double %p0, %p1
...
  %v1 = tail call i64 @llvm.fptosi.sat.i64.f64(double %p0)
```

`fmul` and `fadd` stay two instructions, `vmulsd` and `vaddsd`, on a processor of level `v3` that has a fused one. The result of `x * y + 0.5` is therefore the same bits on all six targets. `fcmp olt` is the ordered less-than, false for a NaN. The other comparisons are `oeq`, `ole`, `ogt` and `oge`, and `!=` is `une`. `llvm.fptosi.sat` is the saturating conversion to a signed integer, and `llvm.fptoui.sat` the one to an unsigned integer.

An `f16` is an `i16` of its bits in the IR of antic. The LLVM text loads the `i16`, casts it to a `half` and widens it, or narrows a `float` to a `half` and stores its bits. In release mode `opt` folds the load and the cast into a `load half`.

```llvm excerpt=floats target=linux-x86_64 mode=release
  %v12 = load half, ptr %p0, align 2, !tbaa !18
  %v4 = fpext half %v12 to float
  %v6 = getelementptr inbounds nuw i8, ptr %p0, i64 4
  %v8 = load float, ptr %v6, align 4, !tbaa !22
  %v11 = fmul float %v8, %v4
  ret float %v11
}
...
  %v1 = fptrunc float %p1 to half
  store half %v1, ptr %p0, align 2, !tbaa !18
```

`load half` reads the two bytes of `level` at offset 0, and `fpext` widens them. `gain` stands at offset 4, after two bytes of padding. `fptrunc` rounds the `f32` to a half before the store.

At `v3` the conversion is one instruction, `vcvtph2ps` or `vcvtps2ph`.

```asm excerpt=floats target=linux-x86_64 mode=release
<level>:
vpinsrw $0x0, (%rdi), %xmm0, %xmm0
vcvtph2ps %xmm0, %xmm0
vmulss 0x4(%rdi), %xmm0, %xmm0
retq
```

arm64 converts with `fcvt s0, h0` and `fcvt h0, s0` at every level. The x86_64 levels `v1` and `v2` have no such instruction. There the LLVM text calls `anti_rt_f16_to_f32` and `anti_rt_f32_to_f16` of `src/rt/f16.c`, which take and give the half in the low 16 bits of an integer register.

A conversion to an integer is one instruction on arm64, `fcvtzs`, which saturates by itself. On x86_64 `to_int` is seven instructions: `vcvttsd2si` and two compares with conditional moves for the upper bound and the NaN.

Where it lives: `src/antic/llvm_emit_function.c` writes the float instructions and both forms of the `f16` conversion. `lower_cast` of `src/antic/lower_expr.c` lowers `as`. `src/rt/f16.h` holds the conversion that the checker, the optimizer and the runtime share. `anti_rt_builder_float` of `src/rt/text.c` writes the digits.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes per value | `f16` 2, `f32` 4, `f64` 8 | the same |
| `+ - * /` and a comparison | one instruction | one instruction |
| Checks | none | none |
| `as` between `f32` and `f64`, integer to float | one instruction, apart from `u64` to float on x86_64: 5 | the same |
| float to integer | arm64: 1 instruction. x86_64: 7 to a signed type, 15 to `u64` | the same |
| read or write of an `f16` | arm64 and `v3`: one conversion instruction. `v1`, `v2`: one call into the runtime | the same |
| Heap allocations | 0 | 0 |

`Sample` above is 8 bytes: 2 for the `f16`, 2 of padding and 4 for the `f32`. An array `[1000]f16` is 2,000 bytes where `[1000]f32` is 4,000.

A dev build and a release build compute the same float results, since neither adds a check or a fast-math flag.

The width is a choice of precision and memory. `float` holds 15 to 17 significant decimal digits and is the type of a float literal without a context. `f32` holds 6 to 9 in half the bytes. A scalar operation costs one instruction at either width, so `f32` pays in an array, in a simd struct of [chapter 8]({{% relref "08-structs-enums" %}}) and in data for a C library, where twice the values fit the same bytes. `f16` holds 3 to 4 digits in a quarter of the bytes, and every read and write adds a conversion. It pays for a large array that the program reads far more often than it computes on.

### Pitfalls

For a Java programmer:

- `float` is 64 bits. The 32-bit type is `f32`.
- `1` is no float. `let x: f32 = 1;` is an error, and the literal is `1.0`.
- A float converted to an integer saturates, as in Java, and a NaN gives 0, as in Java.

For a C programmer:

- `float` is the `double` of C. A struct for a C library uses `f32` or `c_float`.
- No float converts to a wider float or to an integer by itself, and no integer to a float.
- `%` takes no float. The remainder of two floats is `fmod` of the C library through a binding.
- A float out of range converted to an integer is defined. In C it is undefined behaviour.
- `a * b + c` is never contracted, whatever the processor level.

For a C++ programmer:

- An `f16` is no arithmetic type. An expression reads it as `f32`, and only `as f16` writes one.
- `let h: f16 = 1.5;` is an error, since a literal is never a half. The form is `1.5 as f16`.
- An x86_64 program built with `--cpu v1` or `--cpu v2` calls the runtime for every `f16` read and write.

## Text and bytes

### What you know

A `String` of Java is an immutable object of UTF-16 code units that the collector frees. Its `length()` counts those units. A string of C is a `char *` to bytes that end at a NUL, so its length costs a scan. `std::string` of C++ owns its bytes, knows its size and frees them in its destructor.

### The feature

`str` is a text: a pointer to UTF-8 bytes and their count. It is immutable and owns nothing. A file name that the system gives, as `list` of `anti.fs` does, keeps its bytes where they are no UTF-8, so it still opens the file. Copying a `str` copies the pointer and the count, never the bytes.

| Expression | Type | Result |
|---|---|---|
| `s.len` | `int` | the number of bytes |
| `s.ptr` | `?*byte` | the address of the first byte |
| `s[i]` | `byte` | one byte, with a bounds check in dev mode |
| `s[lo..hi]` | `[]byte` | the bytes from `lo` up to `hi`, sharing the memory |
| `a == b`, `a != b` | `bool` | whether the bytes are equal |
| `a < b` and the other three | `bool` | the order of the bytes, which is the order of the code points |

`+` takes no `str`. A text is built with `f"..."` or with `anti.text.Builder`.

A `char` is one Unicode scalar value in 32 bits. A character literal is a `char`, and `s[i]` is a `byte`, so neither converts to the other without `as`.

`[]byte` is a slice of bytes with no encoding. [Chapter 12]({{% relref "12-memory" %}}) covers slices.

The string literals have seven prefixes.

| Literal | Type | Contents |
|---|---|---|
| `"..."` | `str` | text with escapes |
| `r"..."` | `str` | text with every backslash kept |
| `f"..."` | `str` | text with `{expr}` values, built at run time |
| `rf"..."` | `str` | the same with every backslash kept |
| `b"..."` | `[]byte` | bytes with escapes |
| `br"..."` | `[]byte` | bytes with every backslash kept |
| `x"00 AB CC"` | `[]byte` | bytes written as hex pairs |

`re"..."` is a pattern literal of type `Regex`, which "Pattern literals" below describes. Every form but `x"..."` takes hash delimiters, `#"a "quoted" word"#`, for a text that holds quotes. `fr"` is no prefix, and the lexer names the right one.

```anti refused
//! error: `fr"` is not a prefix, write `rf"`
import anti.text;
let n = 1;
let s = fr"{n}";
```

The escapes are `\n \r \t \\ \" \' \0`, `\xHH` and `\u{H}` with one to six hex digits. In a `str` and a `char`, `\xHH` stops at `\x7F` and `\0` is refused, so a `str` literal is always valid UTF-8 without a NUL inside. A byte string takes both.

An `f"..."` writes each `{expr}` into the text. The values it writes are integers, floats, `bool`, `char`, `str` and objects, which it writes through their `to_text`. `{{` and `}}` write a brace. A format specification follows a colon, as `[align][0][width][.precision][kind]`.

| Part | Values | Applies to |
|---|---|---|
| align | `<` left, `>` right, `^` centre | every value. A number stands on the right by default and every other value on the left |
| `0` | fill with zeros after the sign | numbers, with a width |
| width | one to nine digits, counted in characters | every value |
| precision | the digits after the point | floats |
| kind | `x`, `X`, `b`, `o` for integers. `e`, `f` for floats | numbers |

A float without a precision is written with the fewest digits that read back as the same value.

The text of an `f"..."` is new memory from the C library on every evaluation. It belongs to the program, and `free(s.ptr)` gives it back. A module that writes one imports `anti.text`.

`anti.text` holds the functions on a `str`.

| Function | Result |
|---|---|
| `text.byte_count(s)`, `text.char_count(s)` | the bytes, and the characters, which costs a pass over the bytes |
| `text.equal(a, b)` | what `a == b` gives |
| `text.slice(s, from, to)` | a `str` of that byte range, sharing the memory of `s` |
| `text.find_byte(s, b)` | the index of the first `b`, or -1 |
| `text.parse_int(s)` | the `int` the text writes. It may fail |
| `text.from_c(p)`, `text.from_bytes(b)` | a `str` over a C string or over bytes, sharing their memory |
| `text.to_bytes(s)` | a copy of the bytes in new memory, freed with `free(result.ptr)` |
| `text.to_text(b)` | a `str` over a copy of the bytes in new memory, freed with `free(result.ptr)`. It fails when the bytes are no valid UTF-8 |

```anti
//! name: texts
import anti.io;
import anti.text;

fn same(a: str, b: str) -> bool
{
	return a == b;
}

fn third(s: str) -> byte
{
	return s[2];
}

fn label(n: int) -> str
{
	return f"n = {n}";
}

fn say(line: str)
{
	io.println(line);
	free(line.ptr);
}

fn main(args: []str) -> int
{
	let s = "héllo";
	say(f"len {s.len}, chars {text.char_count(s)}");
	say(f"s[1] = {s[1]}, third = {third(s)}");
	let part = s[0..3];
	let head = text.slice(s, 0, 3);
	say(f"part.len = {part.len}, head = {head}");
	let equal = same(s, "héllo");
	let less = "abc" < "abd";
	let upper = "Z" < "a";
	say(f"equal {equal}, less {less}, upper first {upper}");
	let c = 'é';
	say(f"char {c} is {c as u32}");
	let raw = b"\xFF\x00a";
	let hex = x"00 AB CC";
	say(f"raw.len {raw.len}, raw[0] {raw[0]}, hex[1] {hex[1]}");
	let name = "tea";
	let price = 2.5;
	say(f"{name:>6}|{name:<6}|{price:8.2f}|{args.len:04}|{255:x}");
	let n = text.parse_int("42") catch fatal;
	let made = label(n);
	say(f"{made}, size_of(str) = {size_of(str)}, size_of(char) = {size_of(char)}");
	free(made.ptr);
	return 0;
}
```

```text output
len 6, chars 5
s[1] = 195, third = 169
part.len = 3, head = hé
equal true, less true, upper first true
char é is 233
raw.len 3, raw[0] 255, hex[1] 171
   tea|tea   |    2.50|0001|ff
n = 42, size_of(str) = 16, size_of(char) = 4
```

`é` is two bytes in UTF-8, 195 and 169, so `"héllo"` has a length of 6 and 5 characters. `"Z" < "a"` holds because the comparison is by bytes and 90 is below 97. `text.parse_int` may fail, and `catch fatal` after the call prints the error and ends the program when it does. [Chapter 6]({{% relref "06-errors" %}}) covers failing functions and their handlers.

The checker refuses each of the following.

```anti refused
//! error: `+` needs numeric operands, found `str`
let a = "abc";
let b = a + "def";
```

```anti refused
//! error: cannot assign to this expression
let s = "abc";
s[0] = 65;
```

```anti refused
//! error: expected `char`, found `byte`
let s = "abc";
let c: char = s[0];
```

```anti refused
//! error: expected `str`, found `[]byte`
let t: str = b"ab";
```

```anti refused
//! error: `\xHH` stops at `\x7F` outside byte strings
let s = "\xFF";
```

```anti refused
//! error: NUL is not allowed here
let s = "a\0b";
```

```anti refused
//! error: a character literal holds one character
let c = 'ab';
```

```anti refused
//! error: unterminated `{` in `f"..."`
import anti.text;
let key = "k";
let line = f"{key == "k"}";
```

A `"` ends an `f"..."` inside an `{expr}` too. A value that needs a quote is bound to a name first, as `equal` is above, or the literal takes hash delimiters.

An `{expr}` writes a number, a `bool`, a `char`, a `str` or an object. An enum, a pointer and an optional have no text of their own, so each is refused there. An `f"..."` is built at run time, so it is no constant expression.

```anti refused
//! error: `f"..."` cannot write a `?*int`
import anti.text;
let p: ?*int = none;
let s = f"{p}";
```

```anti refused
//! error: `f"..."` cannot write a `Color`
import anti.text;
enum Color { Red, Green }
let c = Color.Red;
let s = f"{c}";
```

```anti refused
//! error: `f"..."` is not a constant expression
import anti.text;
const S: str = f"a";
```

### How antic builds it

A `str` is two words that point at bytes stored elsewhere: in the read-only data for a literal, on the heap for the text of an `f"..."`, or inside another text for a slice. Copying a `str` copies the two words, and no operation of the language writes through one. An `f"..."` is a row of calls on a builder, which grows one block of the heap and hands it to the program.

A `str` is a struct of 16 bytes: the pointer at offset 0 and the length at offset 8. A `[]T` has the same layout. The calling conventions of the targets pass it as the table shows.

| Target | A `str` parameter in the LLVM text |
|---|---|
| linux-x86_64, macos-x86_64 | two `i64`, in two registers |
| linux-arm64, macos-arm64, windows-arm64 | one `[2 x i64]`, in two registers |
| windows-x86_64 | a `ptr` to a copy the caller makes |

A string literal is a constant global of the module that holds the bytes and a NUL after them. The NUL is not counted in the length. Two literals with equal bytes are one global. `llc` puts the global into the read-only data: `.rodata` on ELF, `__TEXT,__const` on Mach-O and `.rdata` on COFF.

This is `label`, the function with the `f"n = {n}"`, in dev mode.

```llvm excerpt=texts target=linux-x86_64 mode=dev
define hidden { i64, i64 } @texts.label(i64 noundef %p0) #0 {
...
  store ptr @anti.text.Builder.new, ptr %t3, align 8
  %v0 = load ptr, ptr %t3, align 8
  call void (ptr) %v0(ptr sret([32 x i8]) align 8 %a0)
...
  store ptr @texts.1, ptr %t5, align 8
...
  store i64 4, ptr %v7, align 8
...
  call void (ptr, i64, i64) @anti.text.Builder.append(ptr %v8, i64 %v10, i64 %v12)
  %v13 = load ptr, ptr %t1, align 8
  %v14 = load i64, ptr %t0, align 8
  call void (ptr, i64, i64, i8, i64, i8, i8) @anti.text.Builder.append_int(ptr %v13, i64 %v14, i64 10, i8 zeroext 0, i64 0, i8 zeroext 1, i8 zeroext 0)
  %v15 = load ptr, ptr %t1, align 8
  %v16 = call { i64, i64 } (ptr) @anti.text.Builder.take(ptr %v15)
```

Line by line:

- The result `{ i64, i64 }` is the `str`, returned in two registers.
- `Builder.new` builds a `Builder` of 32 bytes into `%a0`, a slot of the frame. `sret` marks the pointer the result is written through.
- `@texts.1` is the global of the text `n = `, and the `store i64 4` is its length. The two stores build the `str` that `append` takes.
- `append_int` writes `n`. Its other arguments are the radix 10 and the parts of the format specification, all at their defaults here.
- `take` gives the collected bytes as a `str` and leaves the builder empty.

The checker writes those four calls in place of the literal, so lowering and every later step see ordinary calls of `anti.text`. A float value is `append_float`, a `bool` `append_bool`, a `char` `append_char` and a `str` `append_text`.

The `Builder` holds a pointer to its room, a length and a capacity. `append` copies into the room in place while the bytes and their NUL fit. `anti_rt_builder_append` of `src/rt/text.c` grows the room with `realloc`, to 32 bytes first and then by doubling. `take` hands the room over as it is, so the block of a text is 32 bytes or a power of two times that. A text of no bytes takes a block of one byte.

`a == b` on two `str` is a call of `anti_rt_same_bytes`. It compares the lengths first and calls `memcmp` only when they are equal. `a < b` calls `anti_rt_compare_bytes`.

```llvm excerpt=texts target=linux-x86_64 mode=dev
  %v18 = call i32 (ptr, i64, ptr, i64) @anti_rt_same_bytes(ptr %v14, i64 %v15, ptr %v16, i64 %v17)
```

`s[2]` loads the length, compares it with the index in one unsigned comparison and loads the byte. A release build drops the comparison.

Where it lives: `src/antic/lexer.c` reads the literal forms and the pieces of an `f"..."`, and `src/antic/sema_expr.c` writes its `Builder` calls. `src/std/anti/text.anti` holds `Builder` and the functions on a `str`, and `src/rt/text.c` holds `anti_rt_builder_append`, `anti_rt_builder_take`, `anti_rt_same_bytes` and `anti_rt_compare_bytes`.

### What it costs

| Operation | Dev | Release |
|---|---|---|
| a `str` value | 16 bytes | 16 bytes |
| a `char` value | 4 bytes | 4 bytes |
| a string literal | its bytes and one NUL in the read-only data, once per module | the same |
| copying or passing a `str` | 16 bytes, no allocation | the same |
| `s.len` | one load | one load |
| `s[i]` | a compare, a branch and a load | a load |
| `a == b` | one call of `anti_rt_same_bytes` | the same call, which the LTO link may inline |
| `text.char_count(s)` | one pass over the bytes | the same |
| `f"..."` | one heap block of at least 32 bytes, one call per text and per value | the same block, with `append` inlined |
| `text.slice`, `text.from_bytes` | no allocation | no allocation |
| `text.to_bytes` | one heap block of the length | the same |

An `f"..."` whose text and NUL fit in 32 bytes takes one allocation. A longer one calls `realloc` again each time an append passes the room, which then doubles until it holds the bytes. The program frees the block, and a text that is never freed is a leak that `--memory-checks` reports. Memory that runs out in a builder ends the program with `anti: out of memory for <n> bytes` on the standard error, so an `f"..."` never gives a partial text.

`Builder` is a class of `anti.text`. Making one calls `anti_rt_hook` once, the site of the hook `created`. With no trace handler installed, and no hook of its own in the class, the call reads the handler with one atomic load and the entry of the hook in the table of the class, and returns. [Chapter 15]({{% relref "15-hooks" %}}) describes the hooks.

The form of a text follows from the counts. A literal and a slice cost no allocation and no copy, so a text that exists already is passed as a `str` or cut with `s[lo..hi]` and `text.slice`. An `f"..."` costs a block of the heap and one call per piece, which pays for a text built once, such as a line of output. A loop that builds one long text calls `append` on one `Builder` and takes the text once at the end. It then grows one block by doubling, where an `f"..."` per turn would allocate and free a block each turn. Comparing two texts with `==` costs one call and no allocation.

No operation on a `str` takes a lock or an atomic operation. The bounds check of `s[i]` follows `--checks` and `--no-checks` as every check of dev mode does. Without it an index past the end reads memory outside the text.

### Pitfalls

For a Java programmer:

- `s.len` counts bytes, not characters. `"héllo".len` is 6.
- `==` compares the text. No `equals` is needed, and no identity comparison of two texts exists.
- `+` does not join texts. The form is `f"{a}{b}"`.
- The text of an `f"..."` is not collected. The program frees it with `free(s.ptr)`.
- `s[i]` is a byte of the UTF-8 form, not a `char`.

For a C programmer:

- A `str` is 16 bytes and carries its length. `strlen` is never needed, and a `str` may hold no NUL at its end.
- A literal and the text of an `f"..."` have a NUL after their bytes. A `str` from `text.slice` or `text.from_bytes` shares memory and has none of its own, so its `ptr` is no C string.
- `'a'` is a `char` of 4 bytes, not an `int` and not a byte.
- A `str` cannot be written through. `text.to_bytes` gives a copy that can.

For a C++ programmer:

- A `str` is a `std::string_view`, not a `std::string`. It owns nothing and no destructor runs for it.
- The text of an `f"..."` has no owner type. The program frees it once, by hand.
- A `str` has no buffer of its own for a short text. An `f"..."` of one byte takes a heap block of 32 bytes.

## Pattern literals

### What you know

Java compiles a regular expression with `Pattern.compile` at run time, and a malformed one throws `PatternSyntaxException` when that line runs. `String.matches` requires the whole text to match. C has `regcomp` and `regexec` of POSIX, whose compiled pattern the program frees with `regfree`. C++ has `std::regex`, compiled at run time from the ECMAScript grammar by default. In all three a pattern is a string until the program runs.

### The feature

`re"..."` is a pattern literal. It is raw, so a backslash stands for itself, and it takes hash delimiters as every string literal does. Its type is `Regex`, a struct of `anti.lang` that the compiler declares. A module that writes a pattern imports `anti.regex`, and the engine is PCRE2.

The compiler compiles every literal while it checks the module. A malformed literal is refused at the byte PCRE2 names, so no program runs with a pattern that does not compile. The compiler also knows the groups of a literal, and checks each use of a group and each template against them.

`str` has four methods that take a pattern. Each finds a match anywhere in the text, and none needs the pattern to cover the whole text.

| Method | Result |
|---|---|
| `s.matches(r)` | the first match, a `?Match`, or `none` |
| `s.find_all(r, limit)` | an iterator over the matches, from left to right |
| `s.split(r, limit)` | an iterator over the pieces between the matches |
| `s.replace(r, with, limit)` | a new text with each match replaced by the template `with` |

`limit` takes that many matches from the start, or from the end when it is below zero, and 0 takes every match. `to_slice()` collects an iterator into a slice in new memory. A template names groups with `$1` and `${name}`, and `$$` writes one `$`.

A `?Match` behaves as a `?*T` does. It stands alone as a condition, in `if`, `while`, `&&`, `||` and `!`, and `if let` and `let ... else` bind the match. A match has the fields `all`, `pre` and `post`, the texts of the match and of what stands before and after it, and `count`, the number of groups. `m.group(n)` and `m.took_part(n)` read a group by number. For a literal a group is a field as well: `m.1` by its number and `m.year` by its name.

A call with a literal at the call never fails. A search that reaches the match limit of PCRE2 stops the program with the pattern and the line. A pattern is a literal only where it stands in the call. A `Regex` bound to a local first, and one that `Regex.compile(text)` gives at run time, make the call one that may fail, with `regex.TooExpensive` or `regex.MissingGroup`. `Regex.compile` itself fails with `regex.BadPattern`, which holds the message and the position of PCRE2.

The same methods work on `[]byte`, where `.` matches any byte and nothing needs to be valid UTF-8. A literal takes its mode from where it stands, so `re"\xFFa"` searched in bytes is a `ByteRegex` and its match a `ByteMatch`, whose texts are slices of the searched bytes. `ByteRegex.compile(text)` compiles a byte pattern at run time.

`\d`, `\w` and `\s` mean their ASCII sets, and a pattern that starts with `(*UCP)` gives them their Unicode meaning. The flags are the inline flags of PCRE2, as `(?i)`.

```anti
//! name: pats
import anti.io;
import anti.regex;
import anti.text;

fn say(line: str)
{
	io.println(line);
	free(line.ptr);
}

fn main() -> int
{
	let line = "on 2026-09-23, 7, 8";
	if let d = line.matches(re"(?<year>\d{4})-(?<month>\d\d)") {
		say(f"{d.year} / {d.month}, all {d.all}, pre {d.pre}|, groups {d.count}");
	}
	if line.matches(re"\d") {
		io.println("has a digit");
	}
	let n = 0;
	for x in line.find_all(re"\d+") {
		n += 1;
	}
	say(f"numbers {n}");
	let parts = line.split(re",\s*").to_slice();
	say(f"parts {parts.len}, last {parts[2]}");
	free(parts.ptr);
	say("2026-09-23".replace(re"(?<y>\d{4})-(?<m>\d\d)-(?<d>\d\d)", "${d}/${m}/${y}"));
	let raw = x"00 FF 61 62";
	if let b = raw.matches(re"\xFFa") {
		say(f"bytes found {b.all.len} at {b.pre.len}");
	}
	let r = Regex.compile(r"\d+") catch fatal;
	let k = line.matches(r) catch none;
	if let hit = k {
		say(f"compiled {hit.all}");
	}
	say(f"Regex {size_of(Regex)}, Match {size_of(Match)}, ?Match {size_of(?Match)}");
	return 0;
}
```

```text output
2026 / 09, all 2026-09, pre on |, groups 2
has a digit
numbers 5
parts 3, last 8
23/09/2026
bytes found 2 at 1
compiled 2026
Regex 8, Match 96, ?Match 104
```

`replace` and `to_slice` give new memory, which `say` and `free(parts.ptr)` give back. The pieces of `split` are slices of `line` and own nothing. `catch none` turns a failure of the compiled pattern into no match.

The checker refuses each of the following.

```anti refused
//! error: malformed pattern: missing closing parenthesis
import anti.regex;
let m = "a".matches(re"(ab");
```

```anti refused
//! error: the pattern has no group `nope`
import anti.regex;
if let m = "a".matches(re"(?<x>a)") {
	let y = m.nope;
}
```

```anti refused
//! error: the template names group 2, and the pattern has 1 group
import anti.regex;
let s = "2026".replace(re"(\d+)", "$2");
```

```anti refused
//! error: `matches` may fail and its error is not handled
import anti.regex;
let r = re"\d+";
let m = "7 of 9".matches(r);
```

```anti refused
//! error: a pattern literal is not a constant expression
import anti.regex;
const R: Regex = re"a+";
```

The safety check `exponential-pattern` refuses nested repeats over text that overlaps, such as `(a+)+`. PCRE2 backtracks through every way to split the text between the two repeats, so a text that fails to match costs time exponential in its length. The message names the fixes, a possessive quantifier and an atomic group, which forbid the backtracking. The check lets `(a+\b)+` and `(a+(?=b))+` pass, since the assertion after the inner repeat stops the split. `unchecked(exponential-pattern, "reason")` overrules the check where the input is known to be short.

```anti refused
//! error: the repeat `a+` inside `(a+)+` can take exponential time
import anti.regex;
let m = "aaaa".matches(re"(a+)+b");
```

### How antic builds it

A `Regex` is one pointer to a compiled pattern on the heap. A literal is a global of its module that holds that pointer, one per distinct pattern and mode. A constructor of the module compiles every literal before `main` runs, so a use reads the global and never compiles. A method of `str` is a call of a function of `anti.regex`, which calls the runtime, which calls PCRE2. A `Match` is a value of 96 bytes that records where the search started and what it found, and it owns no memory. A group other than the whole match is found by running the search again.

The program below tests a literal.

```anti
//! name: tested
import anti.io;
import anti.regex;

fn main() -> int
{
	if "on 2026".matches(re"\d+") {
		io.println("digits");
	}
	return 0;
}
```

```text output
digits
```

This is its constructor in release mode, and the start of `main`.

```llvm excerpt=tested target=linux-x86_64 mode=release
define internal noundef i64 @tested.main() #0 {
b0:
  %a1.sroa.0.0.copyload = load i64, ptr @tested.pattern.0, align 8
  %0 = inttoptr i64 %a1.sroa.0.0.copyload to ptr
  %v13.i = tail call i64 @anti_rt_regex_test(ptr %0, ptr nonnull @tested.0, i64 7)
...
define internal void @tested.patterns.start() #1 {
b0:
  %v1 = tail call ptr @anti_rt_regex_literal(ptr nonnull @tested.1, i64 3)
  store ptr %v1, ptr @tested.pattern.0, align 8
  ret void
}
```

Line by line:

- `load ... @tested.pattern.0` reads the pointer of the compiled pattern from the global of the literal.
- `anti_rt_regex_test` searches the 7 bytes of `"on 2026"` at `@tested.0`. A match that stands as a condition alone needs no groups, so the checker calls `matched_literal` of `anti.regex`, which `opt` inlined, and that calls the test of the runtime.
- `@tested.patterns.start` is the constructor, which `@llvm.global_ctors` of the module names. It compiles the 3 bytes of `\d+` at `@tested.1` and stores the pointer.

A failed search gives a value below zero, and the left-out lines branch to `anti_rt_regex_stop_limit`, which prints the pattern and the line and ends the program. A call with a compiled pattern goes to `matches` in place of `matches_literal`, which fails with `TooExpensive` there.

A field of a match is a call. `m.year` calls `group_named` of `anti.regex` with the name, and `m.1` calls `group`. The runtime runs the search of the match again with room for every group and takes the group from it. A `Match` therefore stays a value of fixed size that a function returns and copies freely. `Match` holds the pattern, the three texts `all`, `pre` and `post`, `count`, the searched text, the start of the search and its options, `struct anti_match` of `src/rt/regex.h`. A `?Match` adds the flag byte and its padding.

Where it lives: `src/antic/pattern.c` compiles a literal in the checker with PCRE2. `src/antic/sema_pattern.c` checks the groups, the templates and `exponential-pattern`, and `lower_pattern` and `lower_patterns_start` of `src/antic/lower_pattern.c` write the globals and the constructor. `src/std/anti/regex.anti` holds the functions the methods call, and `src/rt/regex.c` and `src/rt/patterns.c` hold the runtime over PCRE2.

### What it costs

| Cost | Count |
|---|---|
| `size_of(Regex)` | 8 bytes |
| `size_of(Match)`, `size_of(?Match)` | 96 and 104 bytes |
| a literal at the start of the program | one compile by PCRE2 and three blocks of the heap, kept for the life of the program: the handle, a copy of the text and the code |
| a literal at a use | one load of its global |
| a search, a test, a step of `find_all` or `split` | one call into the runtime and PCRE2, and two blocks of the heap that the search frees before it returns: the match data of PCRE2 and its frames, at least 20 KiB |
| reading a group other than `all` | one more search, with the same two blocks |
| `replace`, `to_slice` | the result in new memory, which the program frees |
| `Regex.compile` | one compile by PCRE2 and the same three blocks as a literal. No form of the language gives them back, and `--memory-checks` reports them as a leak |
| a check of dev mode | none. A dev and a release build make the same calls |
| locks and atomic operations | none in a search |
| the executable | PCRE2 links when a program uses a pattern: 576,456 bytes against 228,768 for the same program with `text.find_byte`, release for linux-x86_64 at commit `ba2e424e` |

A match takes no lock and reads no global apart from the pattern, so threads search with one `Regex` at once. PCRE2 runs without its JIT and matches by backtracking, with its frames in the second block of a search.

The choice follows from the counts. A fixed text is cheaper as a `str` than as a pattern: `==`, `text.find_byte`, `text.equal` and `switch` on a `str` cost no PCRE2, no heap block and no 347,688 bytes of code. A pattern pays where the text has a shape. Then a literal is the better form wherever the pattern is known when the program is written: it is checked, its calls cannot fail, and its compile happens once at the start. `Regex.compile` serves a pattern that the user gives, and its calls take a handler. Each group read is a search of its own, so a group that a loop uses is read into a local once, before the loop.

### Pitfalls

For a Java programmer:

- `matches` finds a match anywhere, as `Matcher.find` does. A test of the whole text anchors the pattern, `re"^\d+$"`.
- A malformed literal is a compile error. `Regex.compile` is the run-time form, and its failure is an error value, not an exception.
- A group is a field, `m.year`, and its name is checked at compile time.

For a C programmer:

- The syntax is that of PCRE2, not POSIX. `\d` and `(?<name>...)` work, and `[[:digit:]]` stands beside them.
- A literal is compiled once by the program and never freed. A pattern of `Regex.compile` is never freed either.
- A `str` of a match points into the searched text. Freeing that text leaves the match pointing at freed memory.

For a C++ programmer:

- The grammar is that of PCRE2, not ECMAScript, and the engine backtracks. `exponential-pattern` refuses the nested repeats that make it slow.
- A `Match` owns no memory and holds no positions of its groups. Each group read runs the search again.
- `re"..."` is raw, so `re"\d"` is the `R"(\d)"` of C++.

## Tuples

### What you know

Java has no tuple, and a method that returns two values returns an object, such as a record. C returns a struct declared for the purpose, or writes through pointer parameters. C++ has `std::pair` and `std::tuple` with `std::get<0>` and structured bindings.

### The feature

A tuple type is two or more types in parentheses, `(int, str)`. A tuple value is two or more expressions in parentheses, `(1, "one")`. `t.0` and `t.1` name its elements, which can be read and assigned. A tuple is a value: an assignment or a call copies every element.

`let (a, b) = e;` takes a tuple apart into new variables, one per element. `for i, x in items` gives the index and the element of each turn. `for (k, v) in items` takes apart the tuple that each element of `items` is.

Two tuples of one type compare with `==` when every element has `==`. A tuple has no `<`. A tuple may be an element of a tuple, a field, an array element, a constant and the key of a map.

```anti
//! name: pairs
import anti.io;
import anti.text;

export fn min_max(a: int, b: int) -> (int, int)
{
	if a < b {
		return (a, b);
	}
	return (b, a);
}

fn say(line: str)
{
	io.println(line);
	free(line.ptr);
}

fn main() -> int
{
	let t = (1, "one");
	say(f"t.0 = {t.0}, t.1 = {t.1}");
	t.0 = 2;
	let (lo, hi) = min_max(9, 4);
	say(f"lo {lo}, hi {hi}, t.0 {t.0}");
	let nested: (int, (bool, str)) = (1, (true, "x"));
	say(f"nested.1.1 = {nested.1.1}");
	let same = (lo, hi) == (4, 9);
	say(f"equal {same}");
	let items = [10, 20, 30];
	for i, v in items {
		say(f"{i}: {v}");
	}
	let named = [(1, "one"), (2, "two")];
	for (k, v) in named {
		say(f"{k}={v}");
	}
	say(f"(i8, i8) {size_of((i8, i8))}, (i8, int) {size_of((i8, int))}, (int, str) {size_of((int, str))}");
	say(f"(i8, int, i8) {size_of((i8, int, i8))}, (int, i8, i8) {size_of((int, i8, i8))}");
	return 0;
}
```

```text output
t.0 = 1, t.1 = one
lo 4, hi 9, t.0 2
nested.1.1 = x
equal true
0: 10
1: 20
2: 30
1=one
2=two
(i8, i8) 2, (i8, int) 16, (int, str) 24
(i8, int, i8) 24, (int, i8, i8) 16
```

The flags form of the integer section, `let (result, flags) = a + b;`, is this destructuring over a pair `(T, Flags)`.

The checker refuses each of the following.

```anti refused
//! error: `(int, str)` has 2 elements, and `2` is none of them
let t = (1, "one");
let u = t.2;
```

```anti refused
//! error: `(int, str)` has 2 elements, and the destructuring names 3
let t = (1, "one");
let (a, b, c) = t;
```

```anti refused
//! error: a tuple has two or more elements
let t: (int) = (1);
```

```anti refused
//! error: `<` needs numeric, `char` or `str` operands, found `(int, int)`
let t = (1, 2);
let u = (1, 3);
let less = t < u;
```

### How antic builds it

A tuple is an anonymous struct with the layout C gives a struct of the same members. The elements stand in order, each at the next offset that its alignment allows. The size is a multiple of the largest alignment. `(i8, int)` is therefore 16 bytes: one byte, seven of padding and eight.

Two tuple types with the same element types are one type. A header that antic writes for a C library names it by its elements, `struct anti_tuple_int_int` for `(int, int)`.

The calling convention treats a tuple as that struct. On the System V targets a tuple of up to 16 bytes travels in two registers.

```llvm excerpt=pairs target=linux-x86_64 mode=release
define internal { i64, i64 } @min_max(i64 noundef %p0, i64 noundef %p1) #4 {
b0:
  %p0.p1 = tail call i64 @llvm.smin.i64(i64 %p0, i64 %p1)
  %p1.p0 = tail call i64 @llvm.smax.i64(i64 %p0, i64 %p1)
  %v16.pn = insertvalue { i64, i64 } poison, i64 %p0.p1, 0
  %common.ret.op = insertvalue { i64, i64 } %v16.pn, i64 %p1.p0, 1
  ret { i64, i64 } %common.ret.op
}
```

The result type `{ i64, i64 }` is the pair of registers. `opt` has turned the `if` into `smin` and `smax`, and the two `insertvalue` lines put the smaller value first.

```asm excerpt=pairs target=linux-x86_64 mode=release
<min_max>:
movq %rsi, %rdx
cmpq %rsi, %rdi
movq %rsi, %rax
cmovlq %rdi, %rax
cmovgq %rdi, %rdx
retq
```

The first element comes back in `rax` and the second in `rdx`. The function has no frame, no memory access and no branch.

A tuple above 16 bytes, such as `(int, str)`, is returned through a pointer that the caller passes. A function marked `may fail` returns its error in the register and writes its tuple through one out pointer, which [chapter 6]({{% relref "06-errors" %}}) describes.

In dev mode a tuple local is a slot of the frame, and `t.0` is a load at the offset of the element.

Where it lives: `src/antic/types.c` makes one type per distinct tuple, and `src/antic/layout.c` lays it out. `src/antic/abi.c` classifies it for each convention. `src/antic/sema_stmt.c` checks a destructuring, and `tuple_elements` of `src/antic/header.c` writes its C name.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes | the C struct of the elements, padding included | the same |
| Building one | one store per element | registers, when `opt` keeps it out of memory |
| `t.0` | one load | one register |
| `let (a, b) = e;` | one load and one store per element | none beyond the values |
| Passing and returning | up to 16 bytes in two registers on System V and AAPCS64, larger through a pointer. On windows-x86_64 a tuple of 1, 2, 4 or 8 bytes in one register, any other through a pointer | the same |
| Heap allocations | 0 | 0 |
| Calls into the runtime | 0 | 0 |
| Checks | none | none |

The order of the elements decides the padding. The last line of the program above prints 24 bytes for `(i8, int, i8)` and 16 for `(int, i8, i8)`.

A tuple and a struct of the same members have one layout and one cost, so the choice between them is one of names. A tuple suits a result that the caller takes apart at once, as `min_max` gives. A struct suits a value that travels further, since its fields have names, it takes an `operator fn lt` for `<`, and a C header names it as written. Returning two values in a tuple of up to 16 bytes costs two registers. Returning them through a pointer parameter costs a store and a load of each.

### Pitfalls

For a Java programmer:

- A tuple is a value, not an object. It has no identity, cannot be `null`, and an assignment copies it.
- Returning one allocates nothing.

For a C programmer:

- The layout is that of a C struct, so the order of the elements changes the size.
- `(x)` is a parenthesised expression, and a tuple has at least two elements.
- The comma is no operator. `(a, b)` is a pair, never `b` alone.

For a C++ programmer:

- An element is `t.0`, not `std::get<0>(t)`.
- `let (a, b) = e;` copies the elements. It binds no reference into the tuple.
- A tuple has no `<`. Sorting by two keys needs a struct with `operator fn lt`.

## Type inference

### What you know

`var` of Java and `auto` of C++ take the type of a local from its initialiser. C++ also deduces template arguments from the arguments of a call. C had no inference before `auto` of C23.

### The feature

Anti infers a type in three places.

A `let` without a type takes the type of its initialiser.

```anti
let n = 5;
let ratio = 0.5;
let name = "tea";
let pair = (n, name);
let bytes = [1, 2, 3];
```

A literal takes its type from where it stands. Five places give one. They are the initialiser of a typed `let`, an argument and a returned value. The other operand of a binary operator and an element of a typed array give one too. Without such a context an integer literal is an `int` and a float literal a `float`.

```anti
fn half(x: f32) -> f32 { return x / 2.0; }
let a: u8 = 200;
let b = a + 55;
let c = half(3.0);
let d: [3]i16 = [1, 2, 3];
```

A call of a generic function takes its type arguments from its arguments, from left to right. [Chapter 10]({{% relref "10-generics" %}}) covers generics.

```anti
fn is_less<T: Ordered>(a: T, b: T) -> bool { return a < b; }
let small: u8 = 3;
let x = is_less(3, 4);
let y = is_less(small, 4);
let z = is_less<u8>(3, 4);
```

The first call compares two `int`. The second fixes `T` as `u8` at its first argument, and the literal `4` then becomes a `u8`. The third names the type argument.

A literal of a generic struct takes its type arguments from its field values, where neither the literal nor its context writes them. Each value whose field names a parameter is checked alone, with no type expected, and binds the parameter. `Pair { a: 1.5, b: 2.5 }` is a `Pair<float>`. Two values that bind one parameter to two types are refused, and so is a literal whose fields settle nothing, as `none` does not. [Chapter 10]({{% relref "10-generics" %}}) covers generic structs.

```anti
struct Pair<T>
{
	a: T,
	b: T,
}
let p = Pair { a: 1.5, b: 2.5 };
let q = Pair { a: 3 as u8, b: 4 as u8 };
let r: Pair<u8> = Pair { a: 3, b: 4 };
```

The value of a field is checked before it binds, so in `q` the literal `4` is an `int` unless it is written `4 as u8`. The context of `r` gives the type, and the literals then take it.

Nothing else is inferred. A parameter and a result of a function always name their types. An inferred type never depends on a later use of the variable, and it never changes.

The checker refuses each of the following.

```anti refused
//! error: `none` needs a type that may be `none` from its context
let n = none;
```

```anti refused
//! error: expected `int`, found a float literal
let a = [1, 2.0];
```

```anti refused
//! error: expected `int`, found `u8`
fn is_less<T: Ordered>(a: T, b: T) -> bool { return a < b; }
let small: u8 = 3;
let n = is_less(4, small);
```

```anti refused
//! error: expected `i16`, found `int`
let xs = [1, 2, 3];
let n: i16 = xs.len;
```

```anti refused
//! error: expected `int`, found `float`
struct Pair<T>
{
	a: T,
	b: T,
}
let q = Pair { a: 1, b: 2.5 };
```

```anti refused
//! error: `Box` is generic, and nothing gives its type arguments here
struct Box<T>
{
	v: ?*T,
}
let q = Box { v: none };
```

In the third block the literal `4` comes first, so `T` is `int` before `small` is seen.

### How antic builds it

The checker fixes the type of every expression and writes it into the tree. `--dump-types` shows the result. This is `inferred` of the program below.

```anti
//! name: infer
export fn inferred(n: int) -> int
{
	let twice = n * 2;
	let big = twice > 100;
	if big {
		return 100;
	}
	return twice;
}

export fn written(n: int) -> int
{
	let twice: int = n * 2;
	let big: bool = twice > 100;
	if big {
		return 100;
	}
	return twice;
}

fn main(args: []str) -> int
{
	return inferred(args.len) - written(args.len);
}
```

```types excerpt=infer target=linux-x86_64 mode=release
export function inferred   fn(int) -> int
...
    let_stmt twice         int
      multiplicative *     int
        ident n            int
        int_lit 2          int
    let_stmt big           bool
      relation >           bool
        ident twice        int
        int_lit 100        int
```

`twice` is an `int` because `n * 2` is one, and the literal `2` is an `int` because `n` is one. `big` is a `bool` because a comparison gives one.

Lowering reads the types from the tree, so the IR and the LLVM module carry no trace of how a type was found. `inferred` and `written` compile to the same code.

```llvm excerpt=infer target=linux-x86_64 mode=release
  %v2 = shl i64 %p0, 1
  %common.ret.op = tail call i64 @llvm.smin.i64(i64 %v2, i64 100)
  ret i64 %common.ret.op
}
...
  %v2 = shl i64 %p0, 1
  %common.ret.op = tail call i64 @llvm.smin.i64(i64 %v2, i64 100)
  ret i64 %common.ret.op
}
```

The first three lines are the body of `inferred` and the last three the body of `written`.

Where it lives: `sema_stmt.c` of `src/antic/` gives a `let` its type. `sema_expr.c` and `sema_const.c` type a literal from its context, and `sema_generic.c` infers the type arguments of a call. `ast_dump_typed` of `src/antic/ast_dump.c` prints the checked tree.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes, instructions, calls, checks | the same as with the type written | the same as with the type written |

Inference runs in the checker and adds nothing to the program. Writing a type out is therefore a choice of reading, apart from the places where it changes the type: `let b: u8 = 200;` makes a `u8` where `let b = 200;` makes an `int`, and a type argument written on a generic literal or call settles what the values would not.

### Pitfalls

For a Java programmer:

- `let n = 5;` makes a 64-bit `int`, where `var n = 5;` of Java makes 32 bits.
- `let x = 1.0;` makes a 64-bit `float`.

For a C programmer:

- A literal has no type of its own. `200 + 100` in a `u8` context is checked against `u8`, and `300` there is an error.
- An untyped integer literal is always `int`, never a narrower type that would fit it.

For a C++ programmer:

- `let` never infers a reference. It declares a new variable.
- A function has no inferred result type. Every signature is written in full.
- The type arguments of a generic call are fixed from the left. Putting the typed argument first lets a literal after it follow.
