---
title: "Values"
description: "The types and literals of Anti, variables and constants, integers that wrap or saturate, overflow checks, floats and f16, text and bytes, tuples and inference."
summary: "Types, literals, variables and constants, integers with `+%` and `+|` and the overflow checks of dev mode, floats and `f16`, `str` and `[]byte` with their layout, tuples and type inference, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T17:00:00+02:00
draft: true
weight: 30
tags: [compilers, programming-languages]
keywords: [types, literals, integer overflow, wrapping arithmetic, saturating arithmetic, f16, strings, tuples, type inference]
---

A value of Anti has a fixed size that is the same on all six targets, and no number converts to another type without `as`. Signed arithmetic is checked in a dev build and wraps in a release build. The wrapping and the saturating operators state the intent of a line in both. This chapter covers the scalar types, text, bytes and tuples. Chapter 8 covers structs, arrays, enums and variants.

Some examples mark a function `export fn`. An exported function keeps its plain name and stays in the release module, so a release excerpt can show it. Chapter 17 describes `export fn`.

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

The compound types are `str`, the array `[N]T`, the slice `[]T`, the pointers `*T` and `?*T`, the optional `?T`, the function type `fn(A) -> R` and the tuple `(A, B)`. The C types `c_int`, `c_long` and the others exist for bindings and stand in chapter 17.

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
//! error: expected `int`, found `i32`
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

Each scalar type is one type of the IR and one type of LLVM, on every target.

| Anti | IR of antic | LLVM |
|---|---|---|
| `bool` | `i8` | `i8`, with the range 0 to 1 on a result |
| `i8`, `u8` | `i8` | `i8` |
| `i16`, `u16` | `i16` | `i16` |
| `i32`, `u32`, `char` | `i32` | `i32` |
| `i64`, `u64` | `i64` | `i64` |
| `f32` | `f32` | `float` |
| `f64` | `f64` | `double` |
| `f16` | `i16`, the bits of the half | `half` at the load and the store |
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

`ret i32 42` returns the literal as an immediate value. `0x3FE0000000000000` is 0.5, which the text writes as the bits of a `double` for both float types. `'q'` is the `i32` 113, its code point. `true` is the `i8` 1. `zeroext` says that the caller may read the byte as a full register, and `range(i8 0, 2)` that it is 0 or 1.

`llvm_emit.c` writes a narrow integer parameter or result with `signext` or `zeroext`, by the signedness of its type.

Where it lives: `src/antic/lexer.c` reads a literal, `sema_const.c` and `sema_expr.c` of `src/antic/` give it its type and check that it fits, `src/antic/types.c` holds the types, `lower_cast` of `src/antic/lower_expr.c` lowers `as`, and `src/antic/layout.c` gives each type its size.

### What it costs

The program prints `size_of` of each type. The numbers hold on all six targets.

```anti
//! name: sizes
import anti.io;
import anti.text;

fn show(name: str, bytes: int)
{
	let line = f"{name:<8}{bytes:>3}";
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
	return 0;
}
```

```text output
bool      1
byte      1
i16       2
i32       4
int       8
f16       2
f32       4
float     8
char      4
*int      8
[4]i16    8
```

| Cost | Dev | Release |
|---|---|---|
| Bytes per value | the table above | the same |
| Heap allocations | 0 | 0 |
| Calls into the runtime | 0 | 0 |
| A literal | an immediate operand | the same |
| A widening `as` | one `sext`, `zext` or `fpext` | the same |
| A narrowing `as` between integers | one `trunc`, and a compare with a branch for the range check | one `trunc` |

`size_of` is folded at compile time and costs nothing at run time. The integer section below shows the range check of a narrowing `as`.

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

- No implicit conversion exists, narrowing or widening. Every one is an `as`.
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

`const` declares a constant. It names its type, and its value is a constant expression: literals, other constants, operators, `as` and `size_of`. A constant stands at module level, in a block or in a class body, where `Name.NAME` reaches it. `pub const` makes one of a module visible to importers.

```anti
class Limits
{
	const MAX: int = 64;
}
const STEP: int = 2;
const BYTES: int = STEP * size_of(int);
const NAMES: [2]str = ["on", "off"];
let m = Limits.MAX + BYTES;
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
define dso_local noundef i64 @limit() #0 {
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
define dso_local noundef i64 @limit() local_unnamed_addr #3 {
b0:
  ret i64 96
}
```

A `str` constant is the global of its literal, shared with every equal literal of the module. `GREETING` and a `"tea"` written elsewhere in the module are one global.

Where it lives: `sema_const.c` of `src/antic/` computes a constant expression, `lower_stmt.c` lowers a `let`, and `src/antic/llvm_emit.c` writes the `alloca` of each temporary and slot.

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
