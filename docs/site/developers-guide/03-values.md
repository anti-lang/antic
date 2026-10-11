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
| shift count below 0 or at or above the width | `shift count out of range for <<: count 65, width 64` | the shift by the count modulo the width |
| `as` to an integer type that cannot hold the value | `value out of range for i16: value 70000` | the low bits |
| `as` to `char` outside the Unicode scalar values | `value out of range for char: value 1114112` | the value, unchecked |

Each message starts with the file and the line, as in `ints.anti:7: overflow in +`. Four cases have a defined result and no check in either mode. The least value divided by `-1` gives the least value, and its remainder is 0. Unary `-` of the least value gives the least value. `<<%` gives 0 for a count at or above the width. An unsigned `+`, `-` or `*` wraps.

The checker folds no check. `7 / 0` and `1 << 70` written as constants compile, and the dev build stops when the line runs.

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

### Flags and carry

`let (result, flags) = e;` gives the wrapped result of one integer operation and its flags. `e` is one `+`, `-`, `*`, `<<`, `>>` or unary `-`. `flags` has the built-in struct type `Flags` with four `bool` fields: `overflow`, `carry`, `zero` and `negative`. The form never stops a program, in any build.

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
	return 0;
}
```

```text output
lo 0 hi 1
r 44 carry true overflow false zero false
```

`overflow` is the signed overflow and `carry` the unsigned one. 200 plus 100 in a `u8` carries out of 8 bits. Read as signed bytes the sum is -56 plus 100, which fits.

The checker refuses each of the following.

```anti refused
//! error: `+%` needs integer operands, found `float`
let a = 1.5;
let b = a +% a;
```

```anti refused
//! error: unary `-` needs a signed integer or a float, found `byte`
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

### How antic builds it

Lowering writes a checked `+` as the IR operation `addov` followed by `branchov`, as chapter 2 shows for `*`. The LLVM text writes the pair as the signed `with.overflow` intrinsic of the width and a branch on its bit. This is `checked` in dev mode.

```llvm excerpt=ints target=linux-x86_64 mode=dev
define dso_local noundef i64 @checked(i64 noundef %p0, i64 noundef %p1) #0 {
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
define dso_local noundef i64 @checked(i64 noundef %p0, i64 noundef %p1) local_unnamed_addr #3 {
b0:
  %v2 = add i64 %p1, %p0
  ret i64 %v2
}
...
define dso_local noundef i64 @wrapping(i64 noundef %p0, i64 noundef %p1) local_unnamed_addr #3 {
b0:
  %v2 = add i64 %p1, %p0
  ret i64 %v2
}
...
define dso_local noundef i64 @saturating(i64 noundef %p0, i64 noundef %p1) local_unnamed_addr #3 {
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

The first four instructions compute the clamp value from the sign of the wrapped sum: the greatest `int` when the sum wrapped to a negative number, the least otherwise. `addq` adds again and sets the overflow flag, and `cmovnoq` takes the sum when the flag is clear. arm64 needs four instructions: `adds`, `asr`, `eor` and `csel`.

A release build keeps the defined results of a division in the code. The dev check is dropped, and the selects stay.

```llvm excerpt=ints target=linux-x86_64 mode=release
define dso_local noundef i64 @divide(i64 noundef %p0, i64 noundef %p1) local_unnamed_addr #3 {
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

`%v5` is the divisor, replaced by 1 when it is 0 or -1, so `sdiv` never sees a pair that LLVM leaves undefined. `%v8` takes `0 - a` for a divisor of -1, and `%v9` takes 0 for a divisor of 0. On arm64 the function is nine instructions with one `sdiv` and no branch. A divisor that `opt` can prove to be neither 0 nor -1, such as a constant, leaves the `sdiv` alone.

A shift masks its count, `and i64 %count, 63` before `shl`. A narrowing `as` is one `trunc` in release mode. In dev mode the value is also converted back and compared with the original, which is the range check.

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

Where it lives: `lower_binary_checks` of `src/antic/lower_check.c` writes the checks of an operator and `narrow_check` of `src/antic/lower_expr.c` the one of `as`. `optimize_drop_failures` of `src/antic/optimize.c` drops them, `src/antic/arith.c` computes the wrapping and saturating results of constants, `src/antic/llvm_emit.c` writes the intrinsics, and `anti_rt_check_failed` of `src/rt/check.c` prints the message.

### What it costs

| Operation | Dev | Release |
|---|---|---|
| signed `+ -` | the instruction and one branch on the overflow flag | the instruction |
| signed `*` | x86_64: `imulq` and `jo`. arm64: `smulh`, `mul`, `cmp`, `b.ne` | the instruction |
| unsigned `+ - *`, and `+% -% *%` | the instruction | the instruction |
| `+\| -\|` | no check. x86_64: 6 instructions. arm64: 4 | the same |
| `/` and `%` | a compare and a branch for the zero check, then the release code | the division and three selects |
| `<< >>` | a compare and a branch on the count, then the release code | the mask and the shift |
| narrowing `as` | a conversion back, a compare and a branch | one `trunc` |
| flags form | no check. One instruction per flag read | the same |

An integer value takes the bytes of its type, 1 to 8. No operation of this section allocates memory or takes a lock. The one call into the runtime is `anti_rt_check_failed` on the failure arm, which a program that passes its checks never runs.

`--no-checks` drops the checks of a dev build, and `--checks` keeps them in a release build. Without the checks a signed overflow wraps without a message, a division by zero gives 0 and a wrong shift count is masked. A program that relied on a check to stop then continues with the defined result.

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
//! error: expected `float`, found `f32`
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

A float operation is one LLVM instruction: `fadd`, `fsub`, `fmul`, `fdiv` or `fcmp`. The text carries no fast-math flag, so LLVM reorders nothing and fuses no multiply with an add.

```llvm excerpt=floats target=linux-x86_64 mode=release
define dso_local noundef double @scale(double noundef %p0, double noundef %p1) local_unnamed_addr #3 {
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

An `f16` is an `i16` of its bits in the IR of antic. The LLVM text reads it as a `half` and widens it, or narrows a `float` and stores the `half`.

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

Where it lives: `src/antic/llvm_emit.c` writes the float instructions and both forms of the `f16` conversion, `lower_cast` of `src/antic/lower_expr.c` lowers `as`, `src/rt/f16.h` holds the conversion that the checker, the optimizer and the runtime share, and `anti_rt_builder_float` of `src/rt/text.c` writes the digits.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes per value | `f16` 2, `f32` 4, `f64` 8 | the same |
| `+ - * /` and a comparison | one instruction | one instruction |
| Checks | none | none |
| `as` between `f32` and `f64`, integer to float | one instruction | one instruction |
| float to integer | arm64: 1 instruction. x86_64: 7 | the same |
| read or write of an `f16` | arm64 and `v3`: one conversion instruction. `v1`, `v2`: one call into the runtime | the same |
| Heap allocations | 0 | 0 |

`Sample` above is 8 bytes: 2 for the `f16`, 2 of padding and 4 for the `f32`. An array `[1000]f16` is 2,000 bytes where `[1000]f32` is 4,000.

A dev build and a release build compute the same float results, since neither adds a check or a fast-math flag.

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
