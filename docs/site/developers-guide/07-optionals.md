---
title: "Optionals"
description: "Nullable pointers, ?T and its layout, if let, ?? and the refusals of the checker."
summary: "`*T` and `?*T`, narrowing, `?T` of any type with its layout, and the forms that read an optional, `if let`, `let ... else`, `??`, `?.` and the guards of `catch`, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T20:13:52+02:00
draft: true
weight: 70
tags: [compilers, programming-languages]
keywords: [optional values, nullable pointers, none, narrowing, if let, null coalescing]
---

The type of a value says whether it can be absent. `*T` always points at a `T`, and `?*T` and `?T` may hold `none`. The checker refuses every read of one that may be `none`, so a release build tests only where the source tests. A `?*T` is one pointer, and a `?T` of a value is the value and one flag byte.

## Nullable pointers

### What you know

Every reference of Java may be `null`, and a dereference of one throws `NullPointerException` at run time. A C pointer may be `NULL`, and a dereference of one is undefined behaviour, in practice a fault of the processor. C++ adds references, which cannot be null by rule, though nothing checks the rule. In all three the type of a pointer does not say whether null is allowed.

### The feature

`none` is the pointer that points at nothing. `*T` never holds it. `?*T` may hold it and cannot be dereferenced until a test proves that it holds a pointer. Anti has no `null`.

A `*T` converts to a `?*T` wherever one is expected. The other direction needs a test.

A comparison with `none` narrows a local or a parameter of type `?*T`.

- In the block of `if p != none { }`, `p` has type `*T`.
- After `if p == none { ... }` whose block leaves, `p` has type `*T` for the rest of the enclosing block.
- Right of `p != none &&`, and in the block that follows the whole condition, `p` is narrowed. Right of `p == none ||` it is narrowed as well.
- In a `while p != none do { }`, `p` is narrowed in the body.
- An assignment to the name ends the narrowing.

Narrowing reaches a name and nothing else. A field such as `n.next` keeps its declared type after a test, because any call in between may write it. Such a value is copied into a local first, or bound with a form of the last section.

`alloc T { }` gives a `*T`: when no memory is left, the program ends. `alloc(T, n)` gives a `?*T` to raw memory. Every pointer in the signature of an `extern fn` is a `?*T`, since C says nothing about null. A function value follows the same rule: `fn(A) -> R` never holds `none`, and `?fn(A) -> R` may.

```anti
//! name: nodes
import anti.io;
import anti.text;

struct Node { value: int, next: ?*Node }

export struct Point { x: int, y: int }

fn length(n: ?*Node) -> int
{
	let count = 0;
	let at = n;
	while at != none do {
		count += 1;
		at = at.next;
	}
	return count;
}

fn second(n: *Node) -> int
{
	let next = n.next;
	if next == none {
		return -1;
	}
	return next.value;
}

export fn origin_or(p: ?*Point) -> int
{
	if p == none {
		return 0;
	}
	return p.x;
}

export fn x_of(p: *Point) -> int
{
	return p.x;
}

fn main() -> int
{
	let c = Node { value: 3, next: none };
	let b = Node { value: 2, next: &c };
	let a = Node { value: 1, next: &b };
	let at = Point { x: 7, y: 8 };
	let line = f"length {length(&a)} {length(none)}, second {second(&a)} {second(&c)}, origin_or {origin_or(&at)} {origin_or(none)}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: *Node {size_of(*Node)}, ?*Node {size_of(?*Node)}, Node {size_of(Node)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
length 3 0, second 2 -1, origin_or 7 0
sizes: *Node 8, ?*Node 8, Node 16
```

In `length` the local `at` is narrowed inside the loop, so `at.next` compiles, and the assignment `at = at.next` ends the narrowing for the next test. `second` copies the field into the local `next` before it tests it.

The checker refuses each of the following.

```anti refused
//! error: `p` may be `none`, check it or use `?*T`
fn read(p: ?*int) -> int
{
	return *p;
}
```

```anti refused
//! error: `*int` cannot hold `none`
fn nothing() -> *int
{
	return none;
}
```

```anti refused
//! error: `null` is `none` in Anti
let p: ?*int = null;
```

```anti refused
//! error: `n.next` may be `none`, check it or use `?*T`
struct Node { value: int, next: ?*Node }

fn second(n: *Node) -> int
{
	if n.next != none {
		return n.next.value;
	}
	return -1;
}
```

```anti refused
//! error: `p` may be `none`, check it or use `?*T`
fn read(p: ?*int) -> int
{
	if p != none {
		p = none;
		return *p;
	}
	return 0;
}
```

### How antic builds it

Both types are one machine pointer, and `none` is the address 0. The difference is in what antic tells LLVM about each.

```llvm excerpt=nodes target=linux-x86_64 mode=dev
define dso_local noundef i64 @origin_or(ptr noundef %p0) #0 {
...
define dso_local noundef i64 @x_of(ptr noundef nonnull dereferenceable(16) %p0) #0 {
```

The parameter of `x_of` is a `*Point`. It is `nonnull` and `dereferenceable(16)`: the optimizer may assume 16 readable bytes behind it. The parameter of `origin_or` is a `?*Point` and carries neither.

A test of `none` is a compare of the pointer with null, and a narrowed name is the same pointer.

```llvm excerpt=nodes target=linux-x86_64 mode=release
define dso_local noundef i64 @origin_or(ptr nofree noundef readonly captures(address_is_null) %p0) local_unnamed_addr #1 {
b0:
  %v1 = icmp eq ptr %p0, null
  br i1 %v1, label %common.ret, label %b2
...
b2:                                               ; preds = %b0
  %v6 = load i64, ptr %p0, align 8
```

`icmp eq ptr %p0, null` is `p == none`. `b2` is the rest of the function, where `p` is narrowed. The `load` reads `p.x` through the same `%p0`, with no second test.

```asm excerpt=nodes target=linux-x86_64 mode=release
<origin_or>:
testq %rdi, %rdi
je 0x9 <origin_or+0x9>
movq (%rdi), %rax
retq
xorl %eax, %eax
retq
...
<x_of>:
movq (%rdi), %rax
retq
```

`origin_or` is the test, the load and the two returns. `x_of` is the load alone: a `*Point` needs no test, in dev mode as in release mode. On arm64 the test is one `cbz`.

No dev check guards a dereference. A `*T` that holds 0 can only come from outside the rules, such as memory that C code filled. A read through it faults as in C.

Where it lives: `types_with_none` and `types_is_nullable` of `src/antic/types.c`. `checked_against_none` and the narrowing of conditions in `src/antic/sema_stmt.c`. `param_facts` of `src/antic/llvm_emit.c` writes `nonnull` and `dereferenceable`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of a `*T` | 8 | 8 |
| Bytes of a `?*T` | 8 | 8 |
| A test of `none` | one compare, one branch | the same, two instructions on x86-64 and one on arm64 |
| Narrowing | nothing: a rule of the checker | nothing |
| A read through a `*T` | one load, no test | one load, no test |
| Converting `*T` to `?*T` | nothing | nothing |
| Heap allocations | 0 | 0 |
| Checks inserted | none | none |

The cost of safety is the tests the program writes, each once. A function that takes `*T` moves the test to its callers, and a chain of such functions tests once at the top.

### Pitfalls

For a Java programmer:

- A `*T` cannot be null, so no `NullPointerException` waits behind a call that takes one. The compiler refuses the call with a `?*T`.
- A field of type `?*T` is tested through a local. A test of the field itself narrows nothing.
- `none` is written, not `null`.

For a C programmer:

- A pointer from an `extern fn` is a `?*T`. It is tested once and is a `*T` from there.
- `if p` is refused. The test is `p != none`.
- `alloc T { }` never gives `none`. The check of `malloc` against null is not written.

For a C++ programmer:

- `*T` is closer to a reference than to a pointer: it always refers to an object. It can still be reassigned.
- The narrowing ends at an assignment to the name. A loop that advances a pointer tests it again each turn.
- `nonnull` reaches the optimizer. A `*T` forged from 0 through C is undefined behaviour, as a null reference is.

## Optional values

### What you know

Java has `Optional<T>`, an object on the heap that itself may be null, and boxed types such as `Integer` that use null for absent. C++ has `std::optional<T>`, the value and a flag in place. C picks a sentinel, -1 or `NULL`, or adds a `bool` result beside an out parameter.

### The feature

`?T` is a `T` or `none`, for any type `T`: `?int`, `?str`, `?Point`. A value of type `T` converts to `?T` wherever one is expected, and `none` is the empty one.

A `?T` follows every rule of a `?*T`. It compares with `none`, a test narrows a local or a parameter to `T`, and no operation reads the value before a test.

Two `?T` values compare with `==` when `T` has `==`. They are equal when both are `none`, or when both hold values that are equal.

`none` means that nothing is there, which is an ordinary answer. A lookup that finds no entry gives `none`. A failure that the caller should hear about is an error of chapter 6. The standard library follows that line: `map.get(key)` gives a `?V`, and `text.parse_int(s)` may fail.

A `?T` whose `T` is a pointer or a function type is that pointer, with `none` as 0. `??T` is an optional of an optional, with two flags.

```anti
//! name: opts
import anti.io;
import anti.text;

export struct Point { x: int, y: int }

export fn positive(n: int) -> ?int
{
	if n > 0 {
		return n;
	}
	return none;
}

fn corner(n: int) -> ?Point
{
	if n == 0 {
		return none;
	}
	return Point { x: n, y: n };
}

fn main() -> int
{
	let a = positive(5);
	let b = positive(-5);
	let got = -1;
	if a != none {
		got = a + 1;
	}
	let same = a == b;
	let filled: ?int = 5;
	let equal = a == filled;
	let c = corner(3);
	let cx = 0;
	if c != none {
		cx = c.x;
	}
	let line = f"got {got}, b none {b == none}, same {same}, equal {equal}, cx {cx}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: ?int {size_of(?int)}, ?bool {size_of(?bool)}, ?u8 {size_of(?u8)}, ?f32 {size_of(?f32)}, ?Point {size_of(?Point)}, ?str {size_of(?str)}, ??int {size_of(??int)}, ?*Point {size_of(?*Point)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
got 6, b none true, same false, equal true, cx 3
sizes: ?int 16, ?bool 2, ?u8 2, ?f32 8, ?Point 24, ?str 24, ??int 24, ?*Point 8
```

The checker refuses each of the following.

```anti refused
//! error: `v` may be `none`, test it before reading it
fn next(v: ?int) -> int
{
	return v + 1;
}
```

```anti refused
//! error: `v` may be `none`, test it before reading it
fn plain(v: ?int) -> int
{
	let n: int = v;
	return n;
}
```

```anti refused
//! error: expected `?int`, found `str`
fn or_text(v: ?int) -> int
{
	return v ?? "none";
}
```

### How antic builds it

A `?T` of a value is a struct of two fields with the layout C gives it. The field `value`, of type `T`, stands at offset 0, and `has` after it is one byte that is 1 or 0. The size is rounded up to the alignment of `T`. A `?int` is 8 bytes of value, 1 of flag and 7 of padding, 16 in all. A `?u8` is 2 bytes.

The value stands at offset 0, so the address of a `?T` is the address of its value. A narrowed name reads its value there with no adjustment.

The calling convention treats a `?T` as that struct. A `?int` of 16 bytes travels in two registers on System V and AAPCS64.

```llvm excerpt=opts target=linux-x86_64 mode=release
define dso_local { i64, i64 } @positive(i64 noundef %p0) local_unnamed_addr #3 {
b0:
  %v1 = icmp sgt i64 %p0, 0
  %v14 = insertvalue { i64, i64 } poison, i64 %p0, 0
  %v15 = insertvalue { i64, i64 } %v14, i64 1, 1
  %common.ret.op = select i1 %v1, { i64, i64 } %v15, { i64, i64 } { i64 undef, i64 0 }
  ret { i64, i64 } %common.ret.op
}
```

The result `{ i64, i64 }` is the value and the word that holds the flag. `%v15` is the filled optional: `n` and the flag 1. The other operand of the `select` is `none`: the flag 0 and a value that is `undef`, since nobody may read it.

```asm excerpt=opts target=linux-x86_64 mode=release
<positive>:
movq %rdi, %rax
xorl %edx, %edx
testq %rdi, %rdi
setg %dl
retq
```

The value comes back in `rax` and the flag in `rdx`. The function has no branch: the flag is the result of the compare, and the value is `n` in both cases.

A C header writes a `?T` as `struct anti_opt_<T>` with the fields `value` and `has`, `struct anti_opt_int` for a `?int`.

A `?T` of a class value that owns something is torn down only when its flag is set. Chapter 11 describes teardown.

Where it lives: `TYPE_OPTIONAL` and `types_with_none` of `src/antic/types.c` make the type, and `src/antic/layout.c` lays it out as a struct. `lower_optional_flag` and `lower_set_optional` of `src/antic/lower.c` read and write the flag. `src/antic/header.c` writes the C struct.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of a `?T` of a value | the size of `T` plus one byte, rounded up to the alignment of `T` | the same |
| Bytes of a `?T` of a pointer or a function | 8, the pointer | 8 |
| Making one from a value | one store of the value, one store of the flag | the same, or two registers |
| Making `none` | one store of the flag | the same |
| A test of `none` | one load of the flag byte, one compare, one branch | one test of a register or a byte |
| Reading a narrowed value | one load at offset 0 | the same |
| Passing and returning | as the C struct: a `?int` in two registers on System V and AAPCS64 | the same |
| Heap allocations | 0 | 0 |
| Checks inserted | none | none |

The sizes are those the example prints. The flag costs the alignment of `T`: 8 bytes for a `?int`, 4 for a `?f32`, 1 for a `?bool`. An array of a million `?int` is 16 megabytes where an array of `int` is 8. A struct that holds four optionals of 8-byte types takes 64 bytes. With one field of flags, or with a sentinel where the range of the values allows one, it takes 40 or 32.

### Pitfalls

For a Java programmer:

- A `?int` is a value in place. It is not boxed, has no identity and allocates nothing.
- A `?T` never is `null` itself. It holds a value or it is `none`, and no third state exists.
- `none` is no error. A function that can explain why nothing was found is written `may fail`.

For a C programmer:

- A `?int` has the layout of `struct { int64_t value; uint8_t has; }`. The header writes that struct, and C reads `has` before `value`.
- The value of a `none` is not defined. The flag alone says what is there.
- A `?*T` is one pointer, not a struct. Null is `none`.

For a C++ programmer:

- `?T` is `std::optional<T>` with the checker in place of `value()`: a read without a test does not compile, and no `bad_optional_access` exists.
- No `operator*` reads an optional unchecked. The narrowed name is the value.
- An optional of a pointer has the size of the pointer, with no flag beside it.

## Reading an optional

### What you know

Java reads an `Optional` with `isPresent` and `get`, `orElse` and `map`. C++ has `has_value`, `value_or` and, since C++23, `and_then`. C tests the pointer or the sentinel with `if`. Kotlin and C# have `?:` or `??` and `?.` for the same purpose.

### The feature

Five forms read a `?*T` or a `?T`. Each evaluates the optional once.

| Form | What it does |
|---|---|
| `if let x = e { }` | binds `x` to the value in the block when `e` is not `none`. `else if let` and `else` continue the chain |
| `let x = e else { };` | binds `x` to the value for the rest of the block. The `else` block runs on `none` and leaves the enclosing block |
| `e ?? d` | gives the value of `e`, or `d` when `e` is `none`. `d` is evaluated only then |
| `p?.next`, `p?.f(args)` | reads a pointer field, or calls a function that gives a pointer, when `p` is not `none`. The result is a `?*U`, and `none` when `p` is |
| `e catch fatal`, `e catch err { }` | ends the program, or runs the handler, when `e` is `none` |

`if let` and `let ... else` bind a new name, so they also read a field or the result of a call, which narrowing does not reach.

`?.` reaches a pointer alone. A field or a result of another type is refused, since the result must itself be able to hold `none`. A chain continues with `?.`: `n?.next?.next`.

`e catch` on an optional turns `none` into an error of the class `anti.lang.NoneDereference`. `catch fatal` prints it and ends the program. `catch err { }` binds it, and the handler ends with `yield v` or leaves, as the handler of a failing call does.

```anti
//! name: rd
import anti.io;
import anti.text;

struct Point { x: int, y: int }

struct Node { value: int, next: ?*Node }

fn find(key: int) -> ?int
{
	if key == 1 {
		return 10;
	}
	return none;
}

fn lookup(key: int, p: *Point) -> ?*Point
{
	if key == 1 {
		return p;
	}
	return none;
}

fn sum(a: int, b: int) -> int
{
	let total = 0;
	if let x = find(a) {
		total += x;
	} else if let y = find(b) {
		total += y;
	} else {
		total = -1;
	}
	return total;
}

fn main() -> int
{
	let origin = Point { x: 7, y: 8 };
	let third = Node { value: 3, next: none };
	let second = Node { value: 2, next: &third };
	let first: ?*Node = &second;

	let a = find(1) ?? 0;
	let b = find(2) ?? -1;
	let c = lookup(2, &origin) ?? &origin;
	let d = first?.next;
	let e = first?.next?.next;
	let h = find(1) else {
		return 1;
	};
	let k = lookup(1, &origin) catch fatal;
	let m = lookup(2, &origin) catch err {
		yield &origin;
	};
	let line = f"a {a}, b {b}, c {c.x}, d {d != none}, e {e != none}, h {h}, k {k.y}, m {m.y}";
	io.println(line);
	free(line.ptr);
	line = f"sum {sum(1, 1)} {sum(2, 1)} {sum(2, 2)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
a 10, b -1, c 7, d true, e false, h 10, k 8, m 8
sum 10 10 -1
```

`first?.next` is the node after `second`, which exists. `first?.next?.next` is the one after that, and `third.next` is `none`.

The checker refuses each of the following.

```anti refused
//! error: `??` follows a value of type `?*T` or `?T`, found `*int`
fn read(p: *int) -> int
{
	let q = p ?? p;
	return *q;
}
```

```anti refused
//! error: `if let` takes a variant or a value that may be `none`, found `*int`
fn read(p: *int) -> int
{
	if let q = p {
		return *q;
	}
	return 0;
}
```

```anti refused
//! error: the `else` of a `let` follows a value of type `?*T` or `?T`, found `*int`
fn read(p: *int) -> int
{
	let q = p else {
		return 0;
	};
	return *q;
}
```

```anti refused
//! error: the `else` of a `let` leaves the block it stands in
fn read(p: ?*int) -> int
{
	let q = p else {
		let missing = 1;
	};
	return *q;
}
```

```anti refused
//! error: `?.` on `n.value`, which is not a pointer
struct Node { value: int, next: ?*Node }

fn value_of(n: ?*Node) -> int
{
	let v = n?.value;
	return 0;
}
```

```anti refused
//! error: `catch none` counts a failure as `none`, and a `catch` on a pointer guards no failure
fn read(p: ?*int) -> int
{
	let q = p catch none;
	return 0;
}
```

### How antic builds it

Every form is one test and a branch. For a `?*T` the test compares the pointer with 0, and for a `?T` it loads the flag.

```anti
//! name: reads
struct Node { value: int, next: ?*Node }

export fn or_default(v: ?int, d: int) -> int
{
	return v ?? d;
}

export fn must(p: ?*int) -> int
{
	let q = p catch fatal;
	return *q;
}

fn third(n: ?*Node) -> ?*Node
{
	return n?.next?.next;
}
```

```ir excerpt=reads target=linux-x86_64 mode=dev
export fn reads.or_default(%0: agg ?int, %1: i64) -> i64 {
b0:
    %2 = ptradd %0, offset_of ?int.has
    %3 = load i8 %2
    %4 = copy i64 0
    %5 = ne i8 %3, 0
    branch %5, b1, b2
b1:
    %6 = load i64 %0
    %4 = copy i64 %6
    jump b3
b2:
    %4 = copy i64 %1
    jump b3
b3:
    ret i64 %4
}
```

`%3` is the flag, loaded from the offset of `has`. `b1` runs when it is set and loads the value at offset 0. `b2` takes the default. `d` is a parameter here. Were it a call, the call would stand in `b2` and run only for `none`.

```asm excerpt=reads target=linux-x86_64 mode=release
<or_default>:
movq %rdi, %rax
testb %sil, %sil
cmoveq %rdx, %rax
retq
```

In the release build the optional arrives in two registers, the value in `rdi` and the flag in the low byte of `rsi`. `testb` tests the flag and `cmoveq` takes the default when it is 0. The branch is gone.

```ir excerpt=reads target=linux-x86_64 mode=dev
fn reads.third(%0: ptr) -> ptr {
b0:
    %1 = copy ptr %0
    %2 = copy ptr 0
    %3 = eq i8 %0, 0
    branch %3, b2, b1
b1:
    %4 = ptradd inbounds %1, offset_of reads.Node.next
    %5 = load ptr %4 !type reads.Node.next
    %2 = copy ptr %5
    jump b2
b2:
    %6 = copy ptr %2
    %7 = copy ptr 0
    %8 = eq i8 %2, 0
    branch %8, b4, b3
```

Each `?.` is the same four steps. `%2` starts as 0, the `none` result. `%3` tests the left side, and `b1` loads the field only when the left side is not `none`. `b2` begins the second `?.` on the result of the first. A chain of n `?.` is n tests, and it stops loading at the first `none`.

```llvm excerpt=reads target=linux-x86_64 mode=release
b0:
  %v3 = icmp eq ptr %p0, null
  br i1 %v3, label %b1, label %b2, !prof
...
b1:                                               ; preds = %b0
  %v7 = tail call ptr @anti.lang.NoneDereference.new()
  tail call void @anti.lang.Error.fatal(ptr %v7)
  unreachable
...
b2:                                               ; preds = %b0
  %v12 = load i64, ptr %p0, align 8
```

This is `must`. The guard is the compare with null. `b1` is the `none` path: it allocates a `NoneDereference` and calls `Error.fatal`, which does not return. The `!prof` on the branch marks that path as unlikely. `b2` reads through the pointer.

`catch err { }` on an optional builds the same `NoneDereference`, binds it to `err` and deletes it when the handler ends.

Where it lives: `check_optional` of `src/antic/sema_expr.c` checks `?.`, and `sema_if_let_none` of `src/antic/sema_pattern.c` rewrites `if let`. `coalesce` and `lower_optional` of `src/antic/lower_expr.c` write `??` and `?.`. `lower_let_unwrap` of `src/antic/lower_stmt.c` writes `let ... else`, and `lower_pointer_guard` and `lower_guard_missing` of `src/antic/lower_error.c` the guard of a `catch`. `NoneDereference` is in `src/std/anti/lang.anti`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| `if let`, `let ... else` | one test, one branch, one copy of the value into the new name | one test, and the copy where `opt` keeps it |
| `e ?? d` | one test, one branch | one test, and a conditional move where both sides are cheap |
| `p?.x` | one test, one branch, one load | the same |
| A chain of n `?.` | n tests, up to n loads | the same |
| `e catch fatal`, value there | one test, one branch not taken | the same |
| `e catch fatal`, `none` | one heap allocation of the error, then the program ends | the same |
| `e catch err { }`, `none` | one heap allocation and one `anti_rt_delete` of the error | the same |
| Heap allocations when the value is there | 0 | 0 |
| Checks inserted | none | none |

`if let` and `let ... else` copy the value into the new name. For a `?int` that is 8 bytes. For a `?T` of a large struct it is the size of the struct. A test of the name with narrowing reads the value in place.

`e catch err { }` allocates an error object of at least 120 bytes on the `none` path. Where `none` is expected, `??`, `if let` and `let ... else` handle it without an allocation.

### Pitfalls

For a Java programmer:

- `?.` does not reach an `int` field. `p?.count` is refused, and `if let q = p { }` reads `q.count`.
- `??` evaluates its right side only for `none`, as `orElseGet` does. `orElse` evaluates its argument always.
- Narrowing and `if let` differ in what they reach: narrowing tests a name, and `if let` binds any expression.

For a C programmer:

- `let q = p else { return 0; };` is the early return after `if (!p)`, with `q` a pointer that cannot be `none` afterwards.
- `p catch fatal` is an `assert(p)` that stays in a release build, since it is a rule of the program and not a check.
- The block of `let ... else` must leave. One that falls through is refused.

For a C++ programmer:

- `e ?? d` is `value_or(d)` with a lazy `d`.
- `if let x = e` copies the value, as `if (auto x = e)` does. Writing `x` does not change `e`.
- `?.` gives a pointer or `none` and reaches pointer members alone. A member of another type is read after `if let`.
