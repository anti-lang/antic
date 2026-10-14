---
title: "Memory"
description: "Pointers, slices and views, the aliasing of views, the bounds checks, anti.mem, the allocator, alloc and free, and --memory-checks."
summary: "Pointers, slices and views with their aliasing, the bounds checks of dev mode, `anti.mem` and the allocator, `alloc` and `free`, and `--memory-checks`, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T20:23:44+02:00
draft: true
weight: 120
tags: [compilers, programming-languages]
keywords: [pointers, slices, views, aliasing, bounds checks, allocator, AddressSanitizer, memory checks]
---

A pointer is one machine address, and a slice is an address and a length. `alloc` is a call of `malloc` and `free` a call of `free`. A dev build checks every index of an array, a slice and a `str` against its length, and a release build drops that check. `--memory-checks` puts a call of AddressSanitizer before every load and store through a pointer.

## Pointers

### What you know

A C pointer is an address with arithmetic: `p + 1`, `p - q` and a cast to and from an integer. C++ keeps all of that and adds references. A Java reference is an address without any of it, and the collector may move the object behind it.

### The feature

`*T` is a pointer to a `T`, and `?*T` is one that may hold `none`, as [chapter 7]({{% relref "07-optionals" %}}) gives. The forms that make and use a pointer follow.

| Form | What it gives |
|---|---|
| `&x` | the address of a place: a local, a field `&s.f`, an element `&a[i]` |
| `*p` | the value behind `p`, to read or to assign |
| `p.f` | the field `f` of what `p` points at, without a `*` |
| `p[i]` | the element `i` counted from `p`, of a `*T` that points into a run of `T` |
| `alloc T { }` | a new `T` on the heap with the fields of the literal, as a `*T` |
| `alloc(T, n)` | room for `n` values of `T` on the heap, as a `?*T` |
| `free(p)` | gives the memory of `alloc` back |
| `p as *U` | the same address as a pointer of another type, a view of the next section |
| `p == q` | whether two pointers of one type point at the same place. Two class pointers compare their objects, as [chapter 9]({{% relref "09-classes" %}}) gives |

`alloc T { }` never gives `none`: the program ends with a message when no memory is left. `alloc(T, n)` gives `none` then, so its result is tested or bound with `catch`.

The memory of `alloc(T, n)` stands at the alignment of `T`. `malloc` aligns to 16, and a type that `align(N)` raises above that takes its memory from `anti_rt_alloc_aligned` of the runtime, which `free` releases as well. On Windows that memory stands at 16, since the C library there has none above it that `free` releases.

The memory of `alloc(T, n)` is not written. Its elements hold what the allocator left there, and the program stores each before it reads it. One case differs. Where `T` is a class or owns something, the memory is zeroed. Every element then has a zero table until the program fills it.

A pointer has no arithmetic. `p + 1` and `q - p` are refused, a pointer does not convert to an integer or back, and `<` does not compare two pointers. `&p[i]` is the one form that moves a pointer.

`free` takes memory that `alloc` gave. It runs no teardown. An object of a class that owns something ends with `delete(p)`, which [chapter 11]({{% relref "11-ownership" %}}) holds.

The checker does not follow the life of what a pointer points at. A pointer to a local may be returned, and a pointer may be read after `free`. Both compile, and both are errors of the program. `lent` of [chapter 11]({{% relref "11-ownership" %}}) is the checked form for a pointer that must not outlive a call.

```anti
//! name: ptrs
import anti.io;
import anti.text;

export struct Point { x: int, y: int }

export fn make(x: int) -> *Point
{
	return alloc Point { x: x, y: 0 };
}

export fn four(n: int) -> ?*int
{
	return alloc(int, n);
}

export fn drop(p: *Point)
{
	free(p);
}

export fn y_of(p: *Point) -> *int
{
	return &p.y;
}

export fn at(p: *int, i: int) -> int
{
	return p[i];
}

fn bump(p: *int)
{
	*p += 1;
}

fn main() -> int
{
	let n = 41;
	let p = &n;
	bump(p);
	let corner = Point { x: 1, y: 2 };
	bump(&corner.y);
	let nums: [4]int = [1, 2, 3, 4];
	let third = &nums[2];
	*third = 30;
	let h = make(5);
	h.x += 1;
	*y_of(h) = 7;
	let raw = four(4) catch fatal;
	for i in 0..4 {
		raw[i] = i;
	}
	raw[3] = 9;
	let last = &raw[3];
	let same = last == &raw[3];
	let line = f"n {n}, corner.y {corner.y}, nums[2] {nums[2]}, h {h.x} {h.y}, raw {at(raw, 0)} {at(raw, 3)}, same {same}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: *int {size_of(*int)}, ?*int {size_of(?*int)}, *Point {size_of(*Point)}, Point {size_of(Point)}";
	io.println(line);
	free(line.ptr);
	free(raw);
	drop(h);
	return 0;
}
```

```text output
n 42, corner.y 3, nums[2] 30, h 6 7, raw 0 9, same true
sizes: *int 8, ?*int 8, *Point 8, Point 16
```

The checker refuses each of the following.

```anti refused
//! error: `-` needs numeric operands, found `*int`
fn distance(p: *int, q: *int) -> int
{
	return q - p;
}
```

```anti refused
//! error: cannot convert `*int` to `int`
fn address(p: *int) -> int
{
	return p as int;
}
```

```anti refused
//! error: `<` needs numeric, `char` or `str` operands, found `*int`
fn before(p: *int, q: *int) -> bool
{
	return p < q;
}
```

```anti refused
//! error: the operands of `==` have the types `*int` and `*f64`
fn same(p: *int, q: *f64) -> bool
{
	return p == q;
}
```

```anti refused
//! error: unary `&` needs a place
fn five() -> *int
{
	return &5;
}
```

```anti refused
//! error: a constant has no address
const LIMIT: int = 5;

fn limit() -> *int
{
	return &LIMIT;
}
```

```anti refused
//! error: unary `*` needs a pointer, found `int`
fn read(n: int) -> int
{
	return *n;
}
```

```anti refused
//! error: expected `*f64`, found `*int`
fn as_float(p: *int) -> *f64
{
	return p;
}
```

```anti refused
//! error: the value may be `none`, check it or use `?*T`
fn one() -> *int
{
	return alloc(int, 1);
}
```

```anti refused
//! error: `free` needs a pointer, found `int`
fn end(n: int)
{
	free(n);
}
```

### How antic builds it

A pointer is the type `ptr` of LLVM in both forms. `alloc T { }` is a call of `malloc` with the size of `T`, a test of the result and the stores of the literal.

```llvm excerpt=ptrs target=linux-x86_64 mode=release
define dso_local noundef nonnull ptr @make(i64 noundef %p0)
b0:
  %v0 = tail call dereferenceable_or_null(16) ptr @malloc(i64 16)
  %v2 = icmp eq ptr %v0, null
  br i1 %v2, label %b1, label %b2

b1:
  tail call void @anti_rt_out_of_memory(i64 16)
  unreachable

b2:
  store i64 %p0, ptr %v0, align 8
  %v9 = getelementptr i8, ptr %v0, i64 8
  store i64 0, ptr %v9, align 8
  ret ptr %v0
```

- `malloc(i64 16)` asks for the 16 bytes of a `Point`. antic calls `malloc` of the C library itself and wraps nothing around it.
- The compare and the branch test for null. `b1` calls `anti_rt_out_of_memory`, which prints the size and ends the program.
- `b2` stores `x` at offset 0 and `y` at offset 8, and returns the address.
- The result is `nonnull`, which the type `*Point` promises every caller.

`alloc(T, n)` multiplies and calls, and tests nothing. The test is the caller's.

```llvm excerpt=ptrs target=linux-x86_64 mode=release
  %v2 = shl i64 %p0, 3
  %v4 = tail call ptr @malloc(i64 %v2)
  ret ptr %v4
```

`shl` by 3 is `n` times the 8 bytes of an `int`. For an element type that is a class or owns something, the call is `calloc(n, size)`, which zeroes.

The other three functions of the example are one instruction each.

```asm excerpt=ptrs target=linux-x86_64 mode=release
<drop>:
jmp 0x5 <drop+0x5>
...
<y_of>:
leaq 0x8(%rdi), %rax
retq
...
<at>:
movq (%rdi,%rsi,8), %rax
retq
```

- `drop` is a jump to `free`. The disassembly of an object prints the jump without the name of its target.
- `y_of` is `&p.y`: the address in `%rdi` plus the offset 8.
- `at` is `p[i]`: one load at `p + i * 8`. No check stands before it in dev mode either, since a pointer has no length. On arm64 it is `ldr x0, [x0, x1, lsl #3]`.

Dev mode adds nothing to a dereference, to `alloc` or to `free`, and a release build removes nothing.

Where it lives: the cases `EXPR_ALLOC` and `EXPR_FREE` of `lower_expr` in `src/antic/lower_expr.c`, and `lower_new_memory` beside them. `anti_rt_out_of_memory` of `src/rt/mem.c`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of a `*T` and of a `?*T` | 8 | 8 |
| `&x`, `&s.f`, `&a[i]` | one address computation | the same |
| `*p` and `p.f` | one load or store | the same |
| `p[i]` | one load or store, no check | the same |
| `alloc T { }` | one `malloc`, one compare, one branch, one store per field | the same |
| `alloc(T, n)` | one multiplication, one `malloc` or `calloc`. One call of the runtime for a type aligned above 16 | the same |
| `free(p)` | one `free` | the same |
| Checks inserted | none | none |
| Atomic operations and locks | 0 | 0 |

A program for Linux links statically against musl and takes mimalloc as its `malloc` and `free`. A program for macOS or Windows, and a Linux program that links against glibc, takes the allocator of that C library.

### Pitfalls

For a Java programmer:

- A pointer may outlive its object. A pointer to a local is wrong once the function returns, and a pointer to heap memory is wrong after `free`.
- The memory of `alloc(T, n)` is not zeroed for numbers. Java zeroes every new array.
- `p == q` compares addresses, as `==` on two references does.

For a C programmer:

- `p + 1` does not compile. `&p[1]` is the next element, and `p[i]` reads any element.
- A pointer does not convert to an integer, so no tag lives in the low bits of an address.
- `alloc T { }` cannot give `none`, and the test for it is not written. `alloc(T, n)` can, and the checker demands the test.
- `p.f` reaches a field through a pointer. No `->` exists.

For a C++ programmer:

- `alloc T { }` runs no `construct`, and `free(p)` runs no `destruct`. `alloc T(args)` and `delete(p)` of [chapter 9]({{% relref "09-classes" %}}) run them.
- `free(p)` of an object that owns something leaks what it owns. `delete(p)` is the form with the teardown.
- A `*T` is not a reference: it can be assigned again, and it is copied as an address.

## Slices and views

### What you know

A Java array is an object with its length, checked at every index. A C array decays to a pointer and loses its length, and the length travels beside it by convention. C++ has `std::span<T>` for a pointer with a length and `std::array<T, N>` for a fixed run of values. C and C++ forbid, by the strict aliasing rule, most reads of one object through a pointer of another type.

### The feature

`[N]T` is an array: `N` values of `T` in place, where `N` is a constant of at least 1. An array is a value. `=` copies all of it, a parameter of type `[N]T` receives a copy, and a struct holds it inline. `a.len` is `N`.

`[]T` is a slice: the address of the first element and a length. A slice owns nothing. It refers to elements that live elsewhere, in an array, in memory of `alloc` or in a collection.

| Form | What it gives |
|---|---|
| `[1, 2, 3]` | an array literal of type `[3]T` |
| `[v; N]` | an array of `N` copies of `v` |
| `a[lo..hi]` | a slice of the array or the slice `a`, from element `lo` up to and not including `hi` |
| `[]T { ptr: p, len: n }` | a slice from a pointer and a length |
| `s.ptr` and `s.len` | the two parts of a slice. `s.ptr` is a `?*T` |
| `s[i]` | the element `i`, to read or to assign |
| `for x in s` | a copy of each element |
| `for x in &s` | a pointer to each element, lent for one turn |
| `for i, x in s` | the index and a copy of each element |

A part of a slice **aliases** the slice it came from. Both refer to the same elements, and a write through one is read through the other. No form copies the elements of a slice. An array converts to a slice by `a[0..a.len]` alone, and a slice never converts to an array.

`lo` and `hi` are both written. A slice has no `==` and takes no `operator fn eq`, and its length is not assigned.

A **view** is `p as *U`: the address of `p` read as another type. It reinterprets the bytes and converts nothing.

```anti
let one: f32 = 1.0;
let bits = *(&one as *u32);
```

Rules say which two accesses may refer to the same memory once a view exists. A release build assumes that two accesses the rules keep apart never meet, and moves and removes loads and stores by that. A program that breaks a rule is wrong, and a release build of it may give any result.

- Two accesses of the same type may meet. An integer meets the integer of the same width and the other sign.
- An access as `u8` or `i8` may meet any memory. A view as `*byte` reads and writes the bytes of any value.
- Every pointer is one type for these rules, a `str` and the `ptr` of a slice included.
- A struct, a class, a tuple, a variant and a `?T` meet each part they hold. An array meets its elements, and a simd struct its lanes.
- Two structs of which neither holds the other are kept apart, and so are an `int` and an `f64`.
- A view used where it stands, through a local, a field or an element along one path, reaches any memory. The first example reads an `f32` as a `u32` this way.
- A view passed on, as an argument, a result or a stored pointer, is a pointer of its own type. Its accesses follow the rules above.
- Memory of `alloc` and of an allocator holds no type until a store gives it one.

`as` between two class pointers of one chain, or to an interface, is no view. It is the checked conversion of [chapter 9]({{% relref "09-classes" %}}).

```anti
//! name: slices
import anti.io;
import anti.text;

export struct Pixel { r: u8, g: u8, b: u8, a: u8 }

export fn sum(p: *int, n: int) -> int
{
	let xs = []int { ptr: p, len: n };
	let total = 0;
	for x in xs {
		total += x;
	}
	return total;
}

export fn inner(p: *int, n: int) -> int
{
	let xs = []int { ptr: p, len: n };
	let mid = xs[1..n - 1];
	return mid.len;
}

export fn bits(x: *f32) -> u32
{
	return *(x as *u32);
}

export fn keep_apart(a: *int, b: *f64) -> int
{
	*a = 1;
	*b = 2.0;
	return *a;
}

export fn may_meet(a: *int, b: *byte) -> int
{
	*a = 1;
	*b = 2;
	return *a;
}

fn double_all(xs: []int)
{
	for x in &xs {
		*x *= 2;
	}
}

fn main() -> int
{
	let nums: [5]int = [1, 2, 3, 4, 5];
	let copy = nums;
	copy[0] = 100;
	let all = nums[0..5];
	let mid = all[1..4];
	mid[0] = 20;
	double_all(mid);
	let line = f"nums {nums[0]} {nums[1]} {nums[2]} {nums[3]} {nums[4]}, copy[0] {copy[0]}";
	io.println(line);
	free(line.ptr);
	let weighted = 0;
	for i, x in mid {
		weighted += i * x;
	}
	line = f"len {all.len} {mid.len} {nums.len}, sum {sum(&nums[0], 5)}, inner {inner(&nums[0], 5)}, weighted {weighted}";
	io.println(line);
	free(line.ptr);
	let one: f32 = 1.0;
	let px = Pixel { r: 1, g: 2, b: 3, a: 4 };
	let bytes = &px as *byte;
	let word = &px as *u32;
	line = f"bits {bits(&one):x}, bytes {bytes[0]} {bytes[3]}, word {*word:x}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: []int {size_of([]int)}, [5]int {size_of([5]int)}, Pixel {size_of(Pixel)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
nums 1 40 6 8 5, copy[0] 100
len 5 3 5, sum 60, inner 3, weighted 22
bits 3f800000, bytes 1 4, word 4030201
sizes: []int 16, [5]int 40, Pixel 4
```

`copy` is a second array, so the 100 stays out of `nums`. `mid` is a part of `all`, which is a slice of `nums`: the write of 20 and the doubling land in `nums[1]` to `nums[3]`. The word `4030201` is the four bytes of the `Pixel` read as one `u32` on a little-endian target.

The checker refuses each of the following.

```anti refused
//! error: expected `[4]int`, found `[3]int`
fn short() -> int
{
	let a: [4]int = [1, 2, 3];
	return a[0];
}
```

```anti refused
//! error: expected `[]int`, found `[4]int`
fn whole() -> int
{
	let a: [4]int = [1, 2, 3, 4];
	let s: []int = a;
	return s.len;
}
```

```anti refused
//! error: expected `[4]int`, found `[]int`
fn fixed(xs: []int) -> [4]int
{
	return xs;
}
```

```anti refused
//! error: cannot assign to this expression
fn shrink(xs: []int)
{
	xs.len = 2;
}
```

```anti refused
//! error: `==` is not defined on `[]int`
fn same(xs: []int, ys: []int) -> bool
{
	return xs == ys;
}
```

```anti refused
//! error: `x` is a copy of each element of `xs`. Walk with `&xs` to change the elements
fn raise(xs: []int)
{
	for x in xs {
		x += 1;
	}
}
```

```anti refused
//! error: `xs.ptr` may be `none`, check it or use `?*T`
fn first(xs: []int) -> *int
{
	return xs.ptr;
}
```

```anti refused
//! error: cannot convert `[]int` to `[]byte`
fn raw(xs: []int) -> []byte
{
	return xs as []byte;
}
```

```anti refused
//! error: an array length is at least 1
fn none_at_all() -> int
{
	return size_of([0]int);
}
```

```anti refused
//! error: a variable is not a constant expression
fn sized(n: int) -> int
{
	let a: [n]int = [0; n];
	return a[0];
}
```

### How antic builds it

A slice is two words wherever it goes. As a parameter it is two integer registers on linux-x86_64 and macos-x86_64.

```llvm excerpt=slices target=linux-x86_64 mode=dev
define hidden void @slices.double_all(i64 %p0.0, i64 %p0.1) #0 {
```

`%p0.0` is the address and `%p0.1` the length. The three arm64 targets pass the two words as one `[2 x i64]`, which is two registers as well. On windows-x86_64 the parameter is one `ptr` to a copy of the two words, which the caller makes in its frame.

An array is its elements in place, in the frame or in the value that holds it, with no header.

A part of a slice is an address computation and a subtraction. In the release build of `inner` both fold into the result.

```llvm excerpt=slices target=linux-x86_64 mode=release
  %v3 = add i64 %p1, -2
  ret i64 %v3
```

`xs[1..n - 1]` has the length `(n - 1) - 1`, which is `n - 2`. No memory is touched and nothing is copied.

A view where it stands is a plain load of the other type.

```llvm excerpt=slices target=linux-x86_64 mode=release
define dso_local noundef i32 @bits(
b0:
  %v1 = load i32, ptr %p0, align 4
  ret i32 %v1
```

The parameter is a `*f32`, and the load reads an `i32` from the same address. No conversion instruction stands between them.

The aliasing rules reach LLVM as type facts on each load and store, the `!tbaa` at the end of a line.

```llvm excerpt=slices target=linux-x86_64 mode=release
define dso_local noundef i64 @keep_apart(
b0:
  store i64 1, ptr %p0, align 8, !tbaa
  store double 2.000000e+00, ptr %p1, align 8, !tbaa
  ret i64 1
```

- `*a = 1` and `*b = 2.0` are the two stores, each with the fact of its type.
- An `int` and an `f64` are kept apart, so the store through `b` cannot have changed `*a`. The function returns the constant 1 and never loads.

```llvm excerpt=slices target=linux-x86_64 mode=release
define dso_local noundef i64 @may_meet(
b0:
  store i64 1, ptr %p0, align 8, !tbaa
  store i8 2, ptr %p1, align 1
  %v3 = load i64, ptr %p0, align 8, !tbaa
  ret i64 %v3
```

- The second store writes a byte, and a byte may meet any memory. That store carries no type fact.
- The function loads `*a` again after it.

```asm excerpt=slices target=linux-x86_64 mode=release
<keep_apart>:
movq $0x1, (%rdi)
movabsq $0x4000000000000000, %rax
movq %rax, (%rsi)
movl $0x1, %eax
retq
...
<may_meet>:
movq $0x1, (%rdi)
movb $0x2, (%rsi)
movq (%rdi), %rax
retq
```

`keep_apart` ends in `movl $0x1, %eax`, the constant. `may_meet` ends in `movq (%rdi), %rax`, the second load.

A dev build writes the same facts into its module and runs no optimizer. Every load of the source is a load of the code there.

Where it lives: `ir_type_access` of `src/antic/ir.c` gives each access its type. The emitter of `src/antic/llvm_emit.c` writes the `!tbaa` facts. "Aliasing of views" in `docs/anti-language-additions.md` holds the full rules.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of a `[]T` | 16 | 16 |
| Bytes of a `[N]T` | `N` times the size of `T`, 40 for `[5]int` | the same |
| `a[lo..hi]` | one address computation, one subtraction, no copy, no allocation | the same |
| `[]T { ptr: p, len: n }` | two stores | the same |
| `=` of an array, and an array as an argument | a copy of all its bytes | the same |
| `p as *U` | nothing | nothing |
| A load after a store of a type the rules keep apart | one load | none: the optimizer reuses the value it knows |
| Heap allocations | 0 | 0 |

A slice passes in two registers on five targets and as a pointer to a copy of its two words on windows-x86_64. An array of more than 16 bytes passes in memory, as a copy. A function that reads a large array takes a slice or a pointer and copies nothing.

### Pitfalls

For a Java programmer:

- An array is a value. `let b = a;` copies every element, and a change of `b` leaves `a`.
- A slice does not keep its elements alive. A slice of a local array is wrong once the function returns, and the checker does not refuse its `return`.
- A part of a slice shares the elements. Java's `Arrays.copyOfRange` copies them, and `s[lo..hi]` refers to the originals.

For a C programmer:

- An array does not decay. A function that takes `[4]int` receives 32 bytes by value.
- A view as `*byte` may read anything, as `char *` may. A view as `*u32` of memory that holds an `f32` is safe where it stands and wrong once passed on.
- The length of a slice is trusted. `[]T { ptr: p, len: n }` with a wrong `n` passes every check that reads `len`.

For a C++ programmer:

- The aliasing rules are those of the language and not of C++. A signed and an unsigned integer of one width meet, every pointer type meets every other, and a view where it stands reaches any memory.
- A slice is a `std::span`. It has no capacity and cannot grow. `List<T>` of [chapter 16]({{% relref "16-collections" %}}) is the growing form.
- A dev build does not find a broken aliasing rule. The wrong result shows in a release build alone.

## Bounds checks

### What you know

Java checks every array index and throws `ArrayIndexOutOfBoundsException`. C checks nothing, and an index past the end is undefined behaviour. C++ checks in `at()` and not in `operator[]`, so the check is a choice at every use.

### The feature

A dev build checks the index of `a[i]`, `s[i]` and `text[i]` against the length. An index below zero or at or past the length ends the program with the file, the line and both numbers.

```text
main.anti:5: index out of bounds: index 4, length 4
```

A release build drops the check, with every other check of dev mode. An index out of range then reads or writes whatever stands at that address.

Two options of antic override the mode. `--checks` keeps the checks in a release build, and `--no-checks` drops them from a dev build. The build that compiles the program decides.

Three accesses are not checked in any mode.

- `p[i]` of a pointer. A pointer has no length.
- `s[lo..hi]`. The part of a slice is taken without a test of `lo` or `hi` against the length.
- An index that is a constant. `a[4]` of a `[4]int` compiles, and the check of dev mode finds it when it runs.

`for x in s` walks from the first element to the last by the length of the slice and makes no check per element.

```anti
//! name: bounds
import anti.io;
import anti.text;

export fn pick(p: *int, n: int, i: int) -> int
{
	let xs = []int { ptr: p, len: n };
	return xs[i];
}

export fn fixed(i: int) -> int
{
	let a: [4]int = [10, 20, 30, 40];
	return a[i];
}

export fn raw(p: *int, i: int) -> int
{
	return p[i];
}

export fn total(p: *int, n: int) -> int
{
	let xs = []int { ptr: p, len: n };
	let sum = 0;
	for i in 0..xs.len {
		sum += xs[i];
	}
	return sum;
}

fn letter(s: str, i: int) -> byte
{
	return s[i];
}

fn main() -> int
{
	let nums: [4]int = [1, 2, 3, 4];
	let name = "anti";
	let line = f"pick {pick(&nums[0], 4, 3)}, fixed {fixed(2)}, raw {raw(&nums[0], 1)}, total {total(&nums[0], 4)}, letter {letter(name, 0)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
pick 4, fixed 30, raw 2, total 10, letter 97
```

The checker refuses an index of another type.

```anti refused
//! error: expected `int`, found `str`
fn named(xs: []int) -> int
{
	return xs["first"];
}
```

### How antic builds it

Lowering writes the check in every build. It is one unsigned comparison of the index with the length and a branch to a block that fails.

```ir excerpt=bounds target=linux-x86_64 mode=dev
    %6 = load i64 %5
    %7 = load ptr %3
    %8 = ult i8 %2, %6
    branch %8, b2, b1
b1:
    %9 = addr @bounds.0
    call void @anti_rt_check_failed(%9, 34, 0, %2, %6)
    unreachable
b2:
    %10 = mul i64 %2, size_of i64
    %11 = ptradd %7, %10
    %12 = load i64 %11 !type i64
    ret i64 %12
```

- `%6` is the length of the slice and `%7` its address. `%2` is the index `i`.
- `ult` is an unsigned less-than. A negative index is a large unsigned number, so the one comparison finds both ends.
- `b1` is the failure. `anti_rt_check_failed` takes the text that names the file and the line, its length of 34 bytes, the kind 0 for an index, and the two numbers.
- `b2` is the access: the index times the size of an `int`, added to the address, and the load.

The check of an array compares with the constant `N`. A `str` reads its length beside its pointer, as a slice does.

A release build runs a pass over the IR that replaces the branch into every failing block by a jump to the other arm. The comparison, the call and the text of the message then have no use and go.

```asm excerpt=bounds target=linux-x86_64 mode=dev
<pick>:
...
cmpq %rsi, %rdx
setb -0x1(%rbp)
jae 0x47 <pick+0x47>
```

```asm excerpt=bounds target=linux-x86_64 mode=release
<pick>:
movq (%rdi,%rdx,8), %rax
retq
```

The dev code compares the index in `%rdx` with the length in `%rsi` and jumps to the failure at `jae`, above or equal. The release code is the load alone and never reads `%rsi`. On arm64 the release code is `ldr x0, [x0, x2, lsl #3]`.

A release build with `--checks` keeps the comparison in optimized code.

```llvm excerpt=bounds target=linux-x86_64 mode=release option=checks
define dso_local noundef i64 @pick(
b0:
  %v2 = icmp ult i64 %p2, %p1
  br i1 %v2, label %b2, label %b1
```

In that build of the example, the loop of `total` holds no index check. `i` runs from 0 to below `xs.len`, and the optimizer proves the comparison true. The one call of `anti_rt_check_failed` left in `total` is the overflow check of `sum +=`.

Where it lives: `lower_bounds_check` of `src/antic/lower_check.c` writes the check. `optimize_drop_failures` of `src/antic/optimize.c` removes it, and `back_end` of `src/antic/driver.c` decides by the mode and the four options. `anti_rt_check_failed` of `src/rt/check.c` prints the message.

### What it costs

| Access | Dev | Release | Release with `--checks` |
|---|---|---|---|
| `s[i]` of a slice or a `str` | one load of the length, one compare, one branch | nothing | one compare, one branch |
| `a[i]` of an array | one compare with a constant, one branch | nothing | the same compare and branch |
| `s[i]` in `for i in 0..s.len` | the same as `s[i]` | nothing | nothing, where the optimizer proves the range |
| `p[i]` of a pointer | nothing | nothing | nothing |
| `s[lo..hi]` | nothing | nothing | nothing |
| `for x in s` | nothing per element | nothing | nothing |

Each check adds the text of its message to the read-only data of a build that keeps it, 34 bytes for the check of `pick`. The check calls the runtime only when it fails. It allocates nothing and takes no lock.

`--no-checks` gives a dev build the accesses of a release build and gives up the message. An index out of range is then a silent read or write of other memory. `--checks` gives a release build the message at the cost of the compare and the branch.

### Pitfalls

For a Java programmer:

- A release build does not check. A program that relies on the check to stop a bad index is tested in dev mode or built with `--checks`.
- A failed check writes its line and calls `abort`. No `catch` receives it.

For a C programmer:

- The check reads the `len` of the slice. An index within `len` passes even when `len` is larger than the memory behind `ptr`.
- `s[lo..hi]` is unchecked. A part taken past the end gives a slice whose every index then passes the check against the wrong length.
- A pointer is never checked. A length that must be checked travels as a slice.

For a C++ programmer:

- `s[i]` is `at()` in a dev build and `operator[]` in a release build, by the mode and not by the spelling.
- No iterator is checked, since a walk makes no check. A collection that changes under a walk is found by the change check of [chapter 4]({{% relref "04-flow" %}}).

## anti.mem and the allocator

### What you know

C has one allocator, `malloc` and `free`, and a program that wants another passes function pointers around by hand. C++ gives a container an allocator as a template argument, or a `std::pmr::memory_resource` at run time. Java has one heap, tuned from outside the program.

### The feature

`alloc` and `free` of the language are bound to the C library. A class or a function that lets its caller choose where memory comes from takes a `*anti.mem.Allocator` and asks it.

`Allocator` is an abstract class with two functions.

| Function | Contract |
|---|---|
| `alloc(size, align) -> ?*byte` | `size` bytes at an address that is a multiple of `align`, a power of two. A negative `size`, an `align` that is no power of two and memory that has run out give `none`. A `size` of zero gives an address that `free` takes |
| `free(p)` | gives back memory that `alloc` of the same allocator gave. `none` does nothing |

`anti.mem` holds two allocators, and a program writes its own by inheriting `Allocator`.

- `LibcAllocator` is the allocator over the C library. It is a singleton, `mem.LibcAllocator.get()`, and the default provider of the interface, `mem.Allocator.default()`.
- `ArenaAllocator.new(from, block)` hands out memory from blocks that it takes from the allocator `from`. Each block holds `block` bytes, or one larger request alone. A request at an alignment above 16 takes a block of its own. `free` of one allocation gives nothing back. `free_all()` gives back every block at once, and `destruct` gives back what is left.

Memory goes back to the allocator that gave it. Memory of `LibcAllocator` goes back through its `free` and never through `free` of the language. Windows gives aligned memory back with a function of its own.

The memory an allocator gives is raw bytes, a `?*byte`. The program views it as the type it stores there, `raw as *Point`. The memory is not zeroed. An assignment of a class value into it reads the table of the value it replaces. Memory that will hold a class value is therefore zeroed first.

The parts of the language and the library that allocate take an allocator in a form of their own.

| Form | What it does |
|---|---|
| `delete(p, from)` | runs the teardown of `p`, gives every `own` field back to `from`, and then the object |
| `destroy(p, from)` | the same without the object itself |
| `Object.deserialize(input, from)` | takes the object, its strings and the objects it owns from `from` |
| `builder.take_in(from)` | gives the text of an `anti.text.Builder` in memory of `from` |
| a collection, when it is made | takes an optional allocator, as [chapter 16]({{% relref "16-collections" %}}) gives |
| `inject alloc: *Allocator` | a field that the provider of the interface fills, as [chapter 15]({{% relref "15-hooks" %}}) gives |

A module that writes `delete(p, from)` or `destroy(p, from)` imports `anti.mem`.

```anti
//! name: pools
import anti.io;
import anti.text;
import anti.mem;

extern fn memset(p: ?*byte, c: c_int, n: c_size_t) -> ?*byte;

class Counting inherits mem.Allocator
{
	pub live: int = 0,
	pub taken: int = 0,

	concrete fn alloc(self, size: int, align: int) -> ?*byte
	{
		let p = mem.LibcAllocator.get().alloc(size, align) else {
			return none;
		};
		self.live += 1;
		self.taken += 1;
		return p;
	}

	concrete fn free(self, p: ?*byte)
	{
		if p != none {
			self.live -= 1;
		}
		mem.LibcAllocator.get().free(p);
	}
}

struct Point { x: int, y: int }

class Tag
{
	pub id: int = 0,

	fn destruct(self)
	{
		io.println("  destruct tag");
	}
}

fn tag(from: *mem.Allocator, id: int) -> *Tag
{
	let raw = from.alloc(size_of(Tag), 8) catch fatal;
	memset(raw, 0, size_of(Tag) as c_size_t);
	let made = raw as *Tag;
	*made = Tag { id: id };
	return made;
}

fn point(from: *mem.Allocator, x: int) -> *Point
{
	let raw = from.alloc(size_of(Point), 8) catch fatal;
	let p = raw as *Point;
	p.x = x;
	p.y = 0;
	return p;
}

fn main() -> int
{
	let counting = Counting { };
	let one = point(&counting, 1);
	let line = f"one {one.x}, live {counting.live}";
	io.println(line);
	free(line.ptr);
	counting.free(one as ?*byte);
	let t = tag(&counting, 7);
	line = f"tag {t.id}, live {counting.live}";
	io.println(line);
	free(line.ptr);
	delete(t, &counting);
	line = f"after delete: live {counting.live}";
	io.println(line);
	free(line.ptr);

	let arena = mem.ArenaAllocator.new(&counting, 4096);
	let sum = 0;
	for i in 0..100 {
		let p = point(&arena, i);
		sum += p.x;
	}
	line = f"100 points: sum {sum}, blocks taken {counting.taken - 2}, live {counting.live}";
	io.println(line);
	free(line.ptr);
	let big = arena.alloc(8000, 8) catch fatal;
	let wide = arena.alloc(64, 64) catch fatal;
	line = f"after two more: live {counting.live}";
	io.println(line);
	free(line.ptr);
	let b = text.Builder.new();
	b.append("text in a block");
	let s = b.take_in(&arena);
	io.println(s);
	arena.free_all();
	line = f"after free_all: live {counting.live}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: ArenaAllocator {size_of(mem.ArenaAllocator)}, *Allocator {size_of(*mem.Allocator)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
one 1, live 1
tag 7, live 1
  destruct tag
after delete: live 0
100 points: sum 4950, blocks taken 1, live 1
after two more: live 3
text in a block
after free_all: live 0
sizes: ArenaAllocator 56, *Allocator 8
```

- `Counting` hands every request on to the C library and counts the blocks it lent.
- `delete(t, &counting)` runs `destruct` of the `Tag` and gives its memory back to `counting`.
- The 100 points of 16 bytes come out of one block of 4096 bytes: `counting` saw one request.
- The request for 8000 bytes is larger than a block, and the request at an alignment of 64 is above 16. Each takes a block of its own, so three blocks are live.
- The text of the builder lands in a block, and `free_all()` gives all three back. No `free` is written for a point, for `big`, for `wide` or for the text.

The checker refuses each of the following.

```anti refused
//! error: `Allocator` is abstract and has no complete value, and a literal needs one
import anti.mem;

fn make() -> int
{
	let a = mem.Allocator { };
	return 0;
}
```

```anti refused
//! error: expected `?*int`, found `?*byte`
import anti.mem;

fn ints(from: *mem.Allocator) -> ?*int
{
	return from.alloc(8, 8);
}
```

```anti refused
//! error: expected `?*byte`, found `*int`
import anti.mem;

fn back(from: *mem.Allocator, p: *int)
{
	from.free(p);
}
```

```anti refused
//! error: `ArenaAllocator` has a `destruct`, use `dup` instead of `=`
import anti.mem;

fn two(from: *mem.Allocator) -> int
{
	let a = mem.ArenaAllocator.new(from, 4096);
	let b = a;
	return 0;
}
```

```anti refused
//! error: `Half` lacks `concrete fn free`
import anti.mem;

class Half inherits mem.Allocator
{
	concrete fn alloc(self, size: int, align: int) -> ?*byte
	{
		return none;
	}
}

fn main() -> int
{
	let h = Half { };
	return 0;
}
```

```anti refused
//! error: `delete` takes 1 argument, found 2
import anti.mem;

fn end(c: chan int)
{
	delete(c, mem.LibcAllocator.get());
}
```

### How antic builds it

An allocator is an object, and `from.alloc(size, align)` is a dispatch through its table. The functions `alloc` and `free` are the first two of `Allocator`. They take the entries 17 and 18, after the descriptor, the seven functions of the root and the nine hooks.

```ir excerpt=pools target=linux-x86_64 mode=dev
    %3 = load ptr %0 !table
    %4 = eq i8 %3, 0
    branch %4, b1, b2
b1:
    %5 = addr @pools.54
    call void @anti_rt_table_unset(%5, 9)
    unreachable
b2:
    %6 = mul i64 17, size_of ptr
    %7 = ptradd %3, %6
    %8 = load ptr %7 !entry
    %9 = call ptr %8 via @pools.fn.1(%0, size_of pools.Point, 8) table @anti.mem.Allocator.descriptor 17
```

- `%0` is the allocator, and `%3` its table pointer.
- The test for a zero table is the check of a dispatch in dev mode, which [chapter 9]({{% relref "09-classes" %}}) holds. A release build drops it.
- `17` times the size of a pointer is the offset of the entry, and `%8` is the function it holds.
- The call passes the allocator, the size of a `Point` as a symbol and the alignment 8.

The release code of `ArenaAllocator.alloc` shows the same dispatch, where it asks `from` for a new block.

```llvm excerpt=pools target=linux-x86_64 mode=release
  %v40.i = load ptr, ptr %v38.i, align 8
  %v42.i = getelementptr i8, ptr %v40.i, i64 136
  %v44.i = load ptr, ptr %v42.i, align 8
  %v49.i = tail call ptr %v44.i(ptr nonnull %v38.i, i64 %spec.select11.i, i64 %1)
```

136 is 17 times 8. The call is indirect, since three classes of the program implement `Allocator`.

A request that fits the head block of an `ArenaAllocator` makes no call in a release build. `alloc` rounds the used count up to the alignment, compares with the limit, adds the size and returns the address. A request that does not fit takes a new head block from `from`. The room left in the old head block is not handed out again.

```llvm excerpt=pools target=linux-x86_64 mode=release
define internal noundef ptr @anti.mem.ArenaAllocator.alloc(
b0:
  %v1.i = icmp sgt i64 %p1, -1
  %v7.i = icmp sgt i64 %p2, 0
...
  %v18.i = icmp samesign ult i64 %0, 2
...
  %v10.not = icmp eq ptr %v8, null
...
  %v6.i = srem i64 %v17, %p2
  %v8.i = icmp eq i64 %v6.i, 0
...
  %v34.not = icmp sgt i64 %spec.select.i11, %v31
  %v73 = sub i64 %v31, %spec.select.i11
  %v76.not = icmp sgt i64 %p1, %v73
...
  %v49.not = icmp sgt i64 %p2, %v46
```

- The first three compares test the arguments. The size is zero or more, the alignment is above zero, and the alignment has at most one bit set. `%0` is the count of its bits.
- `%v10.not` tests the head block for `none`.
- `srem` is the remainder of the used count by the alignment, and `%v8.i` tests it for zero.
- `%v34.not` compares the padding with the room left, and `%v76.not` the size with the room after the padding.
- `%v49.not` compares the alignment with the alignment of the head block.

The path that fits therefore runs one remainder and eight compares. The functions `valid` and `padding` of `anti.mem` are inlined here, as the `.i` at the end of a name says. A dev build compiles `anti.mem` in dev mode as well. Its `alloc` calls `valid` and `padding` as functions and checks the divisor and each of its additions and subtractions for overflow.

`LibcAllocator.alloc` calls `anti_rt_mem_alloc`, which calls `posix_memalign` on macOS and Linux and `_aligned_malloc` on Windows.

`delete(t, &counting)` is the call `anti_rt_delete_from(t, descriptor, from)`.

```ir excerpt=pools target=linux-x86_64 mode=dev
    call void @anti_rt_delete_from(%29, %47, %0)
```

The runtime runs the teardown of the object with `from` as its second argument. The teardown hands each `own` field to `anti_rt_give(from, p)`, as the excerpt of `Sprite.destroy` in [chapter 11]({{% relref "11-ownership" %}}) shows. `anti_rt_give` calls entry 18 of the table of `from`, its `free`. A null `from` stands for the C library and calls `free`.

Where it lives: `Allocator`, `LibcAllocator` and `ArenaAllocator` of `src/std/anti/mem.anti`. `anti_rt_mem_alloc` and `anti_rt_mem_free` of `src/rt/mem.c`. `anti_rt_give` and `anti_rt_delete_from` of `src/rt/object.c`, and `ANTI_ENTRY_ALLOC` and `ANTI_ENTRY_FREE` of `src/rt/object.h`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of a `*Allocator` | 8 | 8 |
| Bytes of an `ArenaAllocator` | 56 | 56 |
| `from.alloc` and `from.free` | two loads, one indirect call, one test for a zero table | two loads, one indirect call |
| `ArenaAllocator.alloc` that fits | two calls, of `valid` and `padding`, one remainder, eight compares and the overflow checks of dev mode | one remainder, eight compares, no call |
| `ArenaAllocator.alloc` that does not fit | one `alloc` of `from` | the same |
| `ArenaAllocator.free` | an empty function | the same |
| `free_all()` | one `free` of `from` per block | the same |
| Bytes of a block the allocator uses itself | 8, the link to the next block | the same |
| Locks and atomic operations | 0 | 0 |

An `ArenaAllocator` is not thread-safe: `alloc` reads and writes its fields without a lock. `LibcAllocator` is as thread-safe as the C library.

The 100 points of the example cost one call into `counting` in place of 100, and no call to give them back one by one.

### Pitfalls

For a Java programmer:

- Memory of an `ArenaAllocator` is freed all at once. A pointer into a block is wrong after `free_all()`, and nothing reports it.
- An allocator is passed by its address. It is an object like any other, and the program keeps it alive as long as its memory is used.

For a C programmer:

- `free(p)` of the language on memory of an `Allocator` is wrong. The memory goes back through `from.free(p)`.
- The memory of `alloc(size, align)` is not zeroed. A class value assigned into it needs a `memset` first, since `=` tears down what the bytes seem to hold.
- A request above the block size works. It takes a block of its own and wastes nothing.

For a C++ programmer:

- The allocator is no part of a type. A `List<int>` over one allocator has the same type as one over another.
- `delete(p, from)` gives every `own` field back to `from`. An object whose parts came from different allocators cannot be deleted in one call.
- `ArenaAllocator` runs no teardown of what stands in its blocks. `destroy(p, from)` runs it, and `free_all()` then gives back the memory.

## --memory-checks

### What you know

AddressSanitizer of clang and gcc instruments a C or C++ program. It reports a use after free, a double free and an access outside a block while the program runs. Valgrind finds them without a rebuild. Java needs neither, since its references cannot dangle and its arrays are checked.

### The feature

`--memory-checks` builds a program that reports memory errors while it runs. `antic`, `anti build`, `anti run` and `anti test` take the option, in dev mode and in release mode.

A checked program reports four kinds of error.

- A read or a write of heap memory after its `free`.
- A second `free` of one block.
- A read or a write outside a heap block.
- At exit, the heap blocks that the leak checker of AddressSanitizer finds lost, each with the place that allocated it. The Windows runtime reports no leaks.

A report ends the program with status 1. `anti run` and `anti test` put the report through the symbolizer of Anti, so each frame shows the function, the file and the line.

```anti
import anti.lang;

fn stale() -> int
{
	let p = alloc(int, 4) catch fatal;
	p[0] = 7;
	free(p);
	return p[0];
}
```

`stale` compiles, and the read in its last line is a use after free. Under `anti run --memory-checks` on macos-arm64 the report starts as follows. The addresses differ from run to run.

```text
==12421==ERROR: AddressSanitizer: heap-use-after-free on address 0x603000001c90 at pc 0x000104b88940 bp 0x00016b2767c0 sp 0x00016b2767a8
READ of size 8 at 0x603000001c90 thread T0
    #0 0x000104b8893c in main.stale main.anti:8
    #1 0x000104b8897c in main.main main.anti:13
```

The report goes on with the place of the `free`, at line 7, and the place of the `alloc`, at line 5.

The checks cover memory of the heap. An access to a local or to a global carries no check. The option therefore does not find an index past the end of a local array. The bounds check of dev mode finds that one.

The runtime marks the blocks it allocates at start and keeps until exit, so the leak report holds the leaks of the program alone.

The option has three limits.

- windows-arm64 has no runtime of AddressSanitizer. antic refuses the option there with "`--memory-checks` is not available for windows-arm64".
- A release build with the option links without LTO. On macOS, `--lto` together with the option is refused.
- The runtime of AddressSanitizer for x86_64 runs under Rosetta. It does not run under `qemu-x86_64` or under the x64 emulation of Windows on ARM.

```anti
//! name: checked
import anti.io;
import anti.text;

export fn read(p: *int, i: int) -> int
{
	return p[i];
}

export fn write(p: *int, i: int, v: int)
{
	p[i] = v;
}

export fn local(i: int) -> int
{
	let a: [4]int = [1, 2, 3, 4];
	return a[i & 3];
}

fn main() -> int
{
	let p = alloc(int, 4) catch fatal;
	for i in 0..4 {
		write(p, i, i * i);
	}
	let line = f"read {read(p, 3)}, local {local(2)}";
	io.println(line);
	free(line.ptr);
	free(p);
	return 0;
}
```

```text output
read 9, local 3
```

### How antic builds it

The back end puts a call before every load and every store of the program: `__asan_loadN(address, bytes)` or `__asan_storeN(address, bytes)`. The call is a function of the AddressSanitizer runtime. It looks the address up in its map of the heap and reports when the bytes are not allocated.

```llvm excerpt=checked target=linux-x86_64 mode=release option=memory-checks
define dso_local noundef i64 @read(
b0:
  %v2 = shl i64 %p1, 3
  %v5 = getelementptr i8, ptr %p0, i64 %v2
  tail call void @__asan_loadN(ptr %v5, i64 8)
  %v8 = load i64, ptr %v5, align 8
...
define dso_local void @write(
b0:
  %v2 = shl i64 %p1, 3
  %v5 = getelementptr i8, ptr %p0, i64 %v2
  tail call void @__asan_storeN(ptr %v5, i64 8)
  store i64 %p2, ptr %v5, align 8
```

- `%v5` is the address of `p[i]`.
- The call passes that address and 8, the bytes of an `int`. It returns when the 8 bytes are valid and reports otherwise.
- The load or the store follows the call.

An access whose address is a slot of the frame or a global gets no call.

```llvm excerpt=checked target=linux-x86_64 mode=release option=memory-checks
define dso_local noundef i64 @local(
b0:
  %s1 = alloca [32 x i8], align 8
...
  %v17 = getelementptr i8, ptr %s1, i64 %v14
  %v19 = load i64, ptr %v17, align 8
```

`%s1` is the array in the frame. The load from it stands without a call before it.

The machine code of `read` shows what the call costs around the one load.

```asm excerpt=checked target=linux-x86_64 mode=release option=memory-checks
<read>:
pushq %rbp
movq %rsp, %rbp
pushq %r14
pushq %rbx
movq %rsi, %rbx
movq %rdi, %r14
leaq (%rdi,%rsi,8), %rdi
movl $0x8, %esi
callq 0x1b <read+0x1b>
movq (%r14,%rbx,8), %rax
popq %rbx
popq %r14
popq %rbp
retq
```

Without the option `read` is two instructions, the load and `retq`. With it, the function saves two registers to keep `p` and `i` across the call, computes the address, calls and then loads.

The rest of the option is in the link and in the module that holds `main`.

- The program links the runtime of AddressSanitizer from the runtime archive, and `malloc` and `free` become those of that runtime.
- The module that holds `main` defines `__asan_default_options`, which turns the leak check on and makes a report exit with status 1.
- The same module defines `anti_rt_memory_kept`, which the runtime calls for each block it keeps. It hands the block to `__lsan_ignore_object`.
- The build writes the lines of `-g`, so a report names files and lines.

A Windows program gets neither of the two definitions, since the AddressSanitizer runtime of Windows has no leak check.

Where it lives: `memcheck_declare` and the pass over each function in `src/antic/memcheck.c`, with the design in `src/antic/memcheck.h`. `lto_mode` of `src/antic/driver.c` and the link in `src/antic/driver_link.c`.

### What it costs

| Cost | Without the option | With `--memory-checks` |
|---|---|---|
| A load or a store through a pointer | one instruction | one call of `__asan_loadN` or `__asan_storeN` before it |
| A load or a store of a local or a global | one instruction | the same |
| `alloc` and `free` | the C library | those of the AddressSanitizer runtime |
| The link of a release build | full LTO of the program and the runtime | no LTO, unless `--lto` asks for it. A macOS target refuses `--lto` with the option |
| Checks of dev mode | by the mode | by the mode, unchanged |

The check is a call and never an inline test. A loop over a slice therefore makes one call per element.

The table below holds the five programs of `tests/bench`, each run by `tests/run_bench.cmake` beside its C twin. Every time is the median of 5 runs of a release build.

| Program | C twin | Anti | Anti with `--memory-checks` | With the option, against without |
|---|---:|---:|---:|---:|
| `scalar_loop` | 282.7 ms | 281.0 ms | 15025.6 ms | 53.5 times |
| `objects` | 100.5 ms | 103.5 ms | 4458.6 ms | 43.1 times |
| `builder` | 44.3 ms | 55.9 ms | 1202.0 ms | 21.5 times |
| `simd_loop` | 111.1 ms | 111.3 ms | 4727.8 ms | 42.5 times |
| `map_work` | 124.3 ms | 134.9 ms | 1098.6 ms | 8.1 times |

| Condition | Value |
|---|---|
| Machine | Apple M4 Pro, 14 cores, 64 GB, macOS 26.6 |
| Target | macos-arm64 |
| Commit | `82157882` |
| Mode | release. The plain build links with full LTO, and the checked build without LTO |
| Command, columns two and three | `cmake --build build/host --target bench` |
| Command, column four | `cmake -DOPTIONS=--memory-checks -P tests/run_bench.cmake`, with the other values of the `bench` target |
| Load | another job ran on the machine during both runs. The C twins took 287.6, 104.0, 42.8, 114.8 and 129.1 ms in the second run |

The option is off in every build unless it is given. It is a tool for a test run and not for a program that ships.

### Pitfalls

For a Java programmer:

- The option reports an error when the program reaches it. Code that a run does not execute is not checked.
- A leak is reported at exit and not where the last pointer to the block was dropped. The report names the place of the `alloc`.

For a C programmer:

- An access outside a local array or a global is not found. The checks cover the heap.
- A checked program is far slower than a plain one, by the table above. A long test under the option needs a smaller input.
- The option replaces `malloc`. A program for Linux then runs on the allocator of AddressSanitizer and not on mimalloc.

For a C++ programmer:

- A `push` inside `for x in &list` may free the room that `x` points into. A release build then reads freed memory, and the option reports it as a use after free.
- No report comes from a Windows build about leaks, and windows-arm64 cannot build with the option at all.
- A release build with the option is not the release build that ships: it has no LTO, and it carries the lines of `-g`.
