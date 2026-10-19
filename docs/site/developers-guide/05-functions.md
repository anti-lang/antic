---
title: "Functions"
description: "Declaration, parameters, defaults, source locations, the calling convention, inlining, closures, snapshot fn and own fn, function values."
summary: "Declaration and parameters, default values and source locations, the calling convention, inlining, anonymous functions and closures with their captures, `snapshot fn` and `own fn`, and function values with their layout, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T20:24:00+02:00
draft: true
weight: 50
tags: [compilers, programming-languages]
keywords: [functions, parameters, calling convention, inlining, closures, snapshot fn, own fn, function values]
---

A function of Anti is a C function. It follows the C calling convention of its target, so a debugger, a profiler and a C caller read it as they read C. A call allocates nothing. The one function value that owns heap memory is an `own fn`, which holds a snapshot of what it captured.

## Declaration and parameters

### What you know

A C function has one name and one signature, and its arguments are copied into its parameters. C++ adds overloading, references and default arguments. Java passes primitives by value and objects by reference, and overloads by the types of the parameters. In C a function is visible to the linker unless it is `static`. Java and C++ write the visibility on the member.

### The feature

`fn name(params) -> R { }` declares a function. A parameter is a name, a colon and a type. A function without a result has no `->` and leaves with `return;` or at its closing brace. A function with a result ends every path in `return v;`.

An argument is copied into its parameter. A parameter is a variable of the function, and the body may assign it. A struct, a tuple and an array are copied whole, so a change in the callee does not reach the caller. A function changes a value of its caller through a pointer, `*T`. A slice and a `str` are two words that are copied, and the elements they point to are shared.

A name has one function. Anti has no overloading by signature. The functions of a file may stand in any order and may call themselves. A named function stands at module level or in a class body, never inside another function. The form inside a body is the anonymous function of a later section.

A call gives every parameter without a default one argument, by position. The result of a call may be dropped, unless the function may fail, which [chapter 6]({{% relref "06-errors" %}}) describes.

The word before `fn` sets who may call it.

| Marking | Visible to |
|---|---|
| none | the file |
| `internal` | the modules under the same package root |
| `pub` | every module |
| `export` | every module, and C: the function stands in the header under its plain name |

Four words mark a parameter. `own` takes the argument over, and the caller no longer has it. `lent` borrows a pointer or a slice for the call and keeps no copy. [Chapter 11]({{% relref "11-ownership" %}}) describes both. `keep` marks a parameter of function type that the function stores, and `concurrent` one that it passes on to another thread. A later section describes both.

`main` is `fn main() -> int`, `fn main(args: []str) -> int` or `fn main(args: []str, env: []str) -> int`. Its result is the exit status of the program.

```anti
//! name: decl
import anti.io;
import anti.text;

struct Point { x: int, y: int }

struct Box { low: Point, high: Point, tag: int }

pub fn area(w: int, h: int) -> int
{
	return w * h;
}

fn clamp(n: int, low: int, high: int) -> int
{
	if n < low {
		n = low;
	}
	if n > high {
		n = high;
	}
	return n;
}

fn moved(p: Point) -> Point
{
	p.x += 10;
	return p;
}

fn widen(b: *Box)
{
	b.high.x += 1;
}

fn width(b: Box) -> int
{
	return b.high.x - b.low.x;
}

fn first(items: []int) -> int
{
	return items[0];
}

fn consume(own p: *Point) -> int
{
	let x = p.x;
	free(p);
	return x;
}

export fn twice(n: int) -> int
{
	return clamp(n, 0, 100) * 2;
}

fn main() -> int
{
	let p = Point { x: 1, y: 2 };
	let q = moved(p);
	let b = Box { low: p, high: q, tag: 0 };
	widen(&b);
	let items = [7, 8, 9];
	let heap = alloc Point { x: 5, y: 6 };
	let line = f"area {area(3, 4)}, clamp {clamp(120, 0, 100)}, p.x {p.x}, q.x {q.x}, width {width(b)}";
	io.println(line);
	free(line.ptr);
	line = f"first {first(items[0..3])}, consume {consume(heap)}, twice {twice(30)}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Point {size_of(Point)}, Box {size_of(Box)}, []int {size_of([]int)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
area 12, clamp 100, p.x 1, q.x 11, width 11
first 7, consume 5, twice 60
sizes: Point 16, Box 40, []int 16
```

`moved` adds 10 to its copy, and `p.x` of the caller stays 1. `widen` takes a pointer and changes the `Box` of the caller.

The checker refuses each of the following.

```anti refused
//! error: `add` is already declared
fn add(a: int, b: int) -> int { return a + b; }
fn add(a: f64, b: f64) -> f64 { return a + b; }
```

```anti refused
//! error: `add` takes 2 arguments, found 1
fn add(a: int, b: int) -> int { return a + b; }

fn caller() -> int
{
	return add(1);
}
```

```anti refused
//! error: expected `int`, found a float literal
fn add(a: int, b: int) -> int { return a + b; }

fn caller() -> int
{
	return add(1, 2.5);
}
```

```anti refused
//! error: `log` returns no value
fn log(n: int) { }

fn caller()
{
	let x = log(1);
}
```

```anti refused
//! error: `return` needs a value of type `int`
fn count() -> int
{
	return;
}
```

```anti refused
//! error: unknown type `void`
fn log(n: int) -> void { }
```

```anti refused
//! error: `keep` marks a parameter of function type, and this one is `*int`
fn read(keep p: *int) -> int
{
	return *p;
}
```

```anti refused
//! error: `main` must be fn main() -> int, fn main(args: []str) -> int or fn main(args: []str, env: []str) -> int
fn main(n: int) -> int
{
	return 0;
}
```

Named arguments, `f(by: 2)`, are not built.

### How antic builds it

Each function is one function of the LLVM module. Its name is the module path, a dot and the name, and an `export fn` keeps its plain name. A dev build compiles each module into an object of its own and writes every function `hidden`. Such a function is visible to the other objects of the program and not outside it.

```llvm excerpt=decl target=linux-x86_64 mode=dev
define hidden noundef i64 @decl.area(i64 noundef %p0, i64 noundef %p1) #0 {
...
define hidden { i64, i64 } @decl.moved(i64 %p0.0, i64 %p0.1) #0 {
...
define hidden void @decl.widen(ptr noundef nonnull dereferenceable(40) %p0) #0 {
...
define hidden noundef i64 @decl.width(ptr byval([40 x i8]) align 8 %p0) #0 {
...
define hidden noundef i64 @decl.first(i64 %p0.0, i64 %p0.1) #0 {
...
define hidden noundef i64 @decl.consume(ptr noalias noundef nonnull dereferenceable(16) %p0) #0 {
...
define internal noundef i64 @twice(i64 noundef %p0) #0 {
...
define hidden noundef i64 @decl.main() noinline #0 {
```

- `area` takes two `i64`. `noundef` says that an `int` is always a defined value.
- `moved` takes its `Point` of 16 bytes as two `i64` and returns two.
- `widen` takes a pointer. A `*Box` is never `none` and points at 40 bytes, which `nonnull dereferenceable(40)` tells the optimizer.
- `width` takes a `Box` by value. `byval([40 x i8])` is a copy of 40 bytes that the caller puts on the stack.
- `first` takes its slice as two words, the pointer and the length.
- `consume` takes an `own` pointer, which is `noalias`: no other pointer of the callee reaches the object.
- `twice` is the `export fn`, under its plain name. A program of Linux that links musl statically and no object of C makes the symbol `internal`, kept by `llvm.used`, so that it takes the place of no function of the C library.
- `main` is `noinline`, so that the runtime finds it as a function.

`#0` is one set of attributes for every function. It holds `nounwind`, since no Anti function throws, and `"frame-pointer"="non-leaf"`.

A release build compiles the whole program into one module, and every function that is not exported is `internal`.

```llvm excerpt=decl target=linux-x86_64 mode=release
define internal noundef range(i64 0, 201) i64 @twice(i64 noundef %p0) #4 {
...
define internal noundef i64 @decl.main() #5 {
```

Only two functions of the example are left. `opt` inlined `area`, `clamp`, `moved`, `widen`, `width`, `first` and `consume` into `main` and removed them, which `internal` allows. `twice` stays, since C may call it. `opt` also found that its result lies between 0 and 200, the `range` on its result.

On the two Windows targets a function has no `hidden` marking, since COFF has none, and its name is written without dots.

Where it lives: `parser_item.c` and `parser_decl.c` of `src/antic/` parse a function, and `sema_check_main` and the `return` rules of `src/antic/sema_stmt.c` check it. `lower_function` of `src/antic/lower_function.c` writes its IR. `linkage` and `param_facts` of `src/antic/llvm_emit.c` write the line of the module.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A call | one call instruction, and one store of each parameter into the frame of the callee | one call, or none when `opt` inlines |
| A scalar argument | one register | one register |
| A struct of up to 16 bytes | two registers on System V and AAPCS64 | the same |
| A larger struct by value | one copy of its bytes by the caller | the same, unless inlined |
| A pointer, `*T` | one register | one register |
| A slice or a `str` | two registers on System V and AAPCS64 | the same |
| Heap allocations | 0 | 0 |
| Calls into the runtime | 0 | 0 |
| Checks inserted | none | none |

The sizes that decide the row are those the example prints: a `Point` is 16 bytes, a `Box` 40 and a slice 16. `width(b)` copies 40 bytes per call, and a `width` that takes `*Box` copies 8.

Code that nothing calls costs nothing in a release program, since the whole-program build removes it. A dev build keeps every function of every module it compiles.

### Pitfalls

For a Java programmer:

- A class value passed as a parameter is copied or moved, not shared. A method that changes its argument takes a pointer.
- Two functions cannot share a name. `add_int` and `add_float` are two names.
- A missing `pub` makes a function private to its file, not to its package.

For a C programmer:

- A parameter list of `()` means no parameters. `void` is no type.
- A function without `->` returns nothing, and its call cannot stand in an expression.
- A function is used before its declaration without a prototype.
- A struct of 40 bytes passed by value is copied, as in C. The pointer is written at the call, `&b`.

For a C++ programmer:

- Anti has no reference parameters. `*T` is the form, and it cannot be `none`.
- No overloading and no templates by overload. A generic function of [chapter 10]({{% relref "10-generics" %}}) takes the place of an overload set.
- `own` on a parameter is a move that the checker enforces: the caller cannot name the argument afterwards.

## Defaults and source locations

### What you know

C++ gives a parameter a default argument, which is an expression evaluated at each call. Java has no defaults and writes one overload per shorter list. C has neither. `__FILE__` and `__LINE__` of C name the place where they stand, so a function learns its caller only through a macro. C++20 has `std::source_location::current()` as a default argument.

### The feature

`name: T = value` gives a parameter a default. A call may leave out the arguments of the last parameters that have one. The default is a constant expression: a literal, a `const`, or an operation on those. It cannot name another parameter and cannot call a function. Every parameter after one with a default has a default too.

`here` is a value of type `anti.lang.SourceLocation`. It holds the `file`, the `line` and the `column` where it stands, the full name of the `function` around it and its `module`. As the default of a parameter, `here` is the place of the call, not the place of the declaration. That is how a function learns who called it.

A received location is passed on as an ordinary argument. It then still names the first caller.

A default travels with the function. A call from another module fills it as a call inside the module does.

```anti
//! name: dflt
import anti.io;
import anti.text;

const BASE: int = 10;

fn scaled(n: int, by: int = 2, plus: int = BASE * 2) -> int
{
	return n * by + plus;
}

fn trace(what: str, at: lang.SourceLocation = here)
{
	let line = f"{at.file}:{at.line}:{at.column} in {at.function} of {at.module}: {what}";
	io.println(line);
	free(line.ptr);
}

fn checked(n: int, at: lang.SourceLocation = here) -> int
{
	if n < 0 {
		trace("negative", at);
	}
	return n;
}

export fn caller(n: int) -> int
{
	return scaled(n) +% scaled(n, 3) +% scaled(n, 3, 4);
}

fn main() -> int
{
	let line = f"scaled {scaled(4)} {scaled(4, 3)} {scaled(4, 3, 1)}";
	io.println(line);
	free(line.ptr);
	trace("direct");
	let n = checked(-1);
	let where = here;
	trace("passed on", where);
	line = f"size_of(SourceLocation) {size_of(lang.SourceLocation)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
scaled 28 32 13
dflt.anti:32:2 in dflt.main of dflt: direct
dflt.anti:33:10 in dflt.main of dflt: negative
dflt.anti:34:14 in dflt.main of dflt: passed on
size_of(SourceLocation) 64
```

The second line of the trace names the call of `checked` in `main`, although `trace` was called from inside `checked`. The line numbers count the lines of the module that the test of this page builds from the example. That module has no blank lines between its items. The third names the place of the `here` that `main` wrote itself.

The checker refuses each of the following.

```anti refused
//! error: `b` has no default and follows `a`, which has one
fn sum(a: int = 2, b: int) -> int
{
	return a + b;
}
```

```anti refused
//! error: unknown name `a`
fn sum(a: int, b: int = a) -> int
{
	return a + b;
}
```

```anti refused
//! error: a call is not a constant expression
fn start() -> int { return 1; }

fn next(a: int = start()) -> int
{
	return a + 1;
}
```

```anti refused
//! error: `next` takes at most 1 argument, found 2
fn next(a: int = 1) -> int { return a + 1; }

fn caller() -> int
{
	return next(1, 2);
}
```

```anti refused
//! error: expected `int`, found `SourceLocation`
fn line_of(at: int = here) -> int
{
	return at;
}
```

### How antic builds it

A default is not part of the function. The checker writes the missing arguments into each call, and lowering sees a call with every argument.

```ir excerpt=dflt target=linux-x86_64 mode=dev
export fn dflt.caller(%0: i64) -> i64 {
b0:
    %1 = call i64 @dflt.scaled(%0, 2, 20)
    %2 = call i64 @dflt.scaled(%0, 3, 20)
    %3 = add i64 %1, %2
    %4 = call i64 @dflt.scaled(%0, 3, 4)
```

The first call passes 2 for `by` and 20 for `plus`, which is `BASE * 2` already computed. The callee `scaled` has three parameters and no code for a missing one.

A `here` is a constant record of the module, one per place where a location is needed. The call passes its address.

```llvm excerpt=dflt target=linux-x86_64 mode=dev
define hidden void @dflt.trace(i64 %p0.0, i64 %p0.1, ptr byval([64 x i8]) align 8 %p1) #0 {
...
define hidden noundef i64 @dflt.checked(i64 noundef %p0, ptr byval([64 x i8]) align 8 %p1) #0 {
```

A `SourceLocation` is 64 bytes. A `str` of 16 bytes holds the file, two `int` the line and the column, and two more `str` the function and the module. It is larger than 16 bytes, so System V passes it `byval`: the caller copies the 64 bytes of the record onto the stack. On the arm64 targets and on windows-x86_64 the caller makes the copy in its frame and passes a pointer to it.

The texts of the record are literals of the module. Two locations in one function share the bytes of the file, the function and the module name.

Where it lives: `check_defaults` of `src/antic/sema.c` checks a default, with `sema_eval_const` of `src/antic/sema_const.c` for the constant. `default_argument` of `src/antic/sema_call.c` fills a call. The `EXPR_HERE` case of `lower_address` in `src/antic/lower_expr.c` takes the address of the record, which `lower_location_value` of `src/antic/lower_place.c` writes. `SourceLocation` is declared in `src/std/anti/lang.anti`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A default that is a number | one constant argument at the call | the same, or nothing after inlining |
| A `here` | one address of a constant record, and a copy of 64 bytes when the call passes it by value | the same, or nothing when `opt` inlines the callee and the location is unused |
| Read-only data per `here` | 64 bytes, and the three texts once per function | the same for every location that is still used |
| Heap allocations | 0 | 0 |
| Calls into the runtime | 0 | 0 |
| Checks inserted | none | none |

A parameter `at: SourceLocation = here` is paid at every call, whether the callee reads it or not, until the callee is inlined. A function on a hot path takes the location only when it reports with it.

### Pitfalls

For a Java programmer:

- A default takes the place of the overloads `f(a)` and `f(a, b)`. The function exists once.
- `here` costs no stack walk. It is a constant that the compiler writes at the call.

For a C programmer:

- `here` in a default names the caller. A macro around `__FILE__` and `__LINE__` is not needed.
- The file of a location is the path below the search root, not an absolute path.

For a C++ programmer:

- A default is a constant, fixed at compile time. It is not an expression that runs at each call, so it cannot call a function or read a variable.
- A default cannot name an earlier parameter.
- A changed default reaches a caller in another module when that module is compiled again.

## The calling convention

### What you know

Each platform has one C calling convention: System V on x86-64 Linux and macOS, AAPCS64 on arm64, and the Microsoft convention on x86-64 Windows. It fixes the registers of the arguments and of the result, and how a struct travels. C follows it. C++ follows it for `extern "C"` and adds rules of its own for `this`, references and exceptions. Java hides it behind the JVM and reaches it through JNI.

### The feature

Every Anti function uses the C convention of its target, exported or not, and no marking selects another.

| Value | How it travels |
|---|---|
| An integer, a `bool`, a `char`, a float, a pointer | one register |
| A struct, a tuple, a variant, a `?T` | as the C struct of the same layout |
| A slice, a `str` | as a struct of a pointer and a length |
| A class value | as the C struct of its layout, table pointer included |
| `self` | a pointer, as the first argument |
| A parameter of function type | two pointers: the code and a context |
| A `keep` parameter of function type, a function result | one pointer to the code |
| The result of a function that may fail | the error in the result register, and the value through a pointer that is the last argument |

The three conventions differ in how a struct travels.

| Struct | System V, x86-64 | AAPCS64, arm64 | Microsoft, x86-64 |
|---|---|---|---|
| Up to 16 bytes | up to two registers, integer or vector by its fields | up to two integer registers, or up to four vector registers when all fields are one float type | one register when its size is 1, 2, 4 or 8 bytes, otherwise a pointer to a copy |
| Larger | a copy on the stack | a pointer to a copy | a pointer to a copy |
| As a result, larger than the registers | through a pointer the caller passes | the same | the same |

```anti
//! name: conv
import anti.io;
import anti.text;

export struct Point { x: int, y: int }

struct Vec2 { x: f64, y: f64 }

struct Box { low: Point, high: Point, tag: int }

class Counter
{
	pub n: int = 0,

	pub fn add(self, k: int) -> int
	{
		self.n += k;
		return self.n;
	}
}

export fn flip(p: Point) -> Point
{
	return Point { x: p.y, y: p.x };
}

fn dot(a: Vec2, b: Vec2) -> f64
{
	return a.x * b.x + a.y * b.y;
}

fn grow(b: Box, by: int) -> Box
{
	b.high.x += by;
	return b;
}

fn count(items: []int, name: str) -> int
{
	return items.len + name.len;
}

fn half(n: int) -> int may fail
{
	if (n & 1) != 0 {
		fail "odd";
	}
	return n >> 1;
}

fn apply(n: int, f: fn(int) -> int) -> int
{
	return f(n);
}

fn double(n: int) -> int
{
	return n * 2;
}

fn main() -> int
{
	let p = flip(Point { x: 1, y: 2 });
	let d = dot(Vec2 { x: 1.0, y: 2.0 }, Vec2 { x: 3.0, y: 4.0 });
	let b = grow(Box { low: p, high: p, tag: 7 }, 5);
	let items = [1, 2, 3];
	let c = Counter { };
	let h = half(8) catch { yield -1; };
	let n = count(items[0..3], "ab");
	let line = f"flip {p.x} {p.y}, dot {d}, grow {b.high.x}, count {n}, add {c.add(4)}, half {h}, apply {apply(21, double)}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Point {size_of(Point)}, Vec2 {size_of(Vec2)}, Box {size_of(Box)}, Counter {size_of(Counter)}, str {size_of(str)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
flip 2 1, dot 11, grow 7, count 5, add 4, half 4, apply 42
sizes: Point 16, Vec2 16, Box 40, Counter 16, str 16
```

`half` may fail, and its handler `catch { yield -1; }` gives -1 in place of the result when it does. [Chapter 6]({{% relref "06-errors" %}}) describes handlers, and [chapter 9]({{% relref "09-classes" %}}) the class `Counter` with its `self`.

### How antic builds it

antic classifies every parameter and every result itself and writes the outcome into the signature of the LLVM function. The signature therefore shows the convention.

```llvm excerpt=conv target=linux-x86_64 mode=dev
define hidden noundef i64 @conv.Counter.add(ptr noundef nonnull dereferenceable(16) %p0, i64 noundef %p1) #0 {
...
define internal { i64, i64 } @flip(i64 %p0.0, i64 %p0.1) #0 {
...
define hidden noundef double @conv.dot(double %p0.0, double %p0.1, double %p1.0, double %p1.1) #0 {
...
define hidden void @conv.grow(ptr sret([40 x i8]) align 8 %sret, ptr byval([40 x i8]) align 8 %p0, i64 noundef %p1) #0 {
...
define hidden noundef i64 @conv.count(i64 %p0.0, i64 %p0.1, i64 %p1.0, i64 %p1.1) #0 {
...
define hidden noundef ptr @conv.half(i64 noundef %p0, ptr noundef nonnull dereferenceable(8) %p1) #0 {
...
define hidden noundef i64 @conv.apply(i64 noundef %p0, ptr noundef %p1, ptr noundef %p2) #0 {
```

This is System V.

- `add` takes `self` as `%p0`, a pointer to the 16 bytes of a `Counter`, and `k` as `%p1`.
- `flip` takes a `Point` as two `i64`, in `rdi` and `rsi`, and returns one in `rax` and `rdx`.
- `dot` takes each `Vec2` as two `double`, in `xmm0` to `xmm3`.
- `grow` returns a `Box` of 40 bytes through `%sret`, a pointer in `rdi`. Its `Box` parameter is a copy on the stack, and `by` is in `rsi`.
- `count` takes the slice and the `str` as two `i64` each, four registers.
- `half` returns a pointer, the error or null. Its result goes through `%p1`, a pointer to 8 bytes.
- `apply` takes its function as `%p1`, the code, and `%p2`, the context.

```asm excerpt=conv target=linux-x86_64 mode=release
<flip>:
movq %rsi, %rax
movq %rdi, %rdx
retq
```

`flip` is two register moves. `p.x` arrives in `rdi` and leaves in `rdx`, the second word of the result.

```llvm excerpt=conv target=linux-arm64 mode=dev
define internal [2 x i64] @flip([2 x i64] %p0.0) #0 {
...
define hidden noundef double @conv.dot([2 x double] %p0.0, [2 x double] %p1.0) #0 {
...
define hidden void @conv.grow(ptr sret([40 x i8]) align 8 %sret, ptr %p0, i64 noundef %p1) #0 {
...
define hidden noundef i64 @conv.count([2 x i64] %p0.0, [2 x i64] %p1.0) #0 {
```

On arm64 a `Point` is one array of two `i64`, in `x0` and `x1`. A `Vec2` is an array of two `double`, in `d0` and `d1`, because both fields are one float type. The large `Box` is a plain pointer, `%p0`, to a copy that the caller made.

```llvm excerpt=conv target=windows-x86_64 mode=dev
define dso_local void @flip(ptr sret([16 x i8]) align 8 %sret, ptr %p0) #0
...
define noundef double @_A4conv_dot(ptr %p0, ptr %p1) #0
...
define noundef i64 @_A4conv_count(ptr %p0, ptr %p1) #0
```

On windows-x86_64 a struct of 16 bytes fits no register. `flip` takes a pointer to a copy of its `Point` and returns through `%sret`. `dot` and `count` take two pointers each. A function that takes many small structs therefore costs more memory traffic on this target than on the other five.

Where it lives: `src/antic/abi.c` holds the three classifications, `sysv`, `aapcs64` and `windows_x64`, and `abi_classify` chooses by the target. `src/antic/layout.c` gives the sizes they read. `param_facts` and the signature code of `src/antic/llvm_emit.c` write what they decide.

### What it costs

| Cost | linux-x86_64, macos-x86_64 | the three arm64 targets | windows-x86_64 |
|---|---|---|---|
| A scalar argument | one of 6 integer or 8 vector registers, then the stack | one of 8 integer or 8 vector registers, then the stack | one of 4 registers, then the stack |
| A `Point`, 16 bytes | 2 registers | 2 registers | a copy of 16 bytes and a pointer |
| A `Box`, 40 bytes | a copy of 40 bytes on the stack | a copy of 40 bytes and a pointer | a copy of 40 bytes and a pointer |
| A slice or a `str` | 2 registers | 2 registers | a copy of 16 bytes and a pointer |
| A result of 40 bytes | a hidden pointer, written by the callee | the same | the same |
| The result of a function that may fail | one more pointer argument, one store through it | the same | the same |
| A parameter of function type | 2 registers | 2 registers | 2 registers |

The convention is the same in dev and in release. A release build removes the cost of a call that `opt` inlines. It keeps the cost of a call through a pointer, of a call from C and of a call `opt` chose not to inline.

No call takes a lock, an atomic operation or heap memory, and no check is inserted for one.

### Pitfalls

For a Java programmer:

- A struct argument has a cost by its size. Past 16 bytes it is copied at every call, and a pointer avoids the copy.
- `self` is an ordinary first argument, a pointer to the object. [Chapter 9]({{% relref "09-classes" %}}) describes when a call goes through the table of the class.

For a C programmer:

- An Anti function is called from C with the prototype the header gives. No wrapper stands between.
- A slice is a struct of two words. The C side receives a pointer and a length in the order the header writes.
- A function that may fail returns the error. Its value comes through the last pointer, which the C caller provides.

For a C++ programmer:

- No hidden `this` adjustment and no exception tables exist. A function is `nounwind`.
- A struct with a destructor in C++ travels by pointer. An Anti struct travels by its layout alone, whatever it owns.
- A result above the register size is written in place through the hidden pointer. No copy follows the return.

## Inlining

### What you know

`inline` in C and C++ is a hint and a linkage rule, and the optimizer decides. A C++ function in a header is inlined across files, and one in another translation unit only under link-time optimization. The JIT of Java inlines at run time by the counts it measures.

### The feature

Anti has no `inline` word and no attribute for it. The build mode decides.

A dev build inlines nothing. Every call in the source is a call in the object, so a debugger steps into each function and a stack trace names each frame.

A release build compiles the whole program as one LLVM module and runs `opt` over it with the pipeline `default<O3>` and an inline threshold of 225. `opt` weighs each call by the size of the callee and inlines below the threshold. A function that is `internal` and inlined into every caller is removed. The modules of the standard library are part of that module, so a small library function is inlined as one of the program is.

The link of a release program is a full LTO link by default. The linker lld applies the same threshold to the program and the bitcode of the runtime. A small function of the runtime is therefore inlined too. `--lto none` links objects and leaves every call into the runtime a call.

Three kinds of function are never inlined.

- `main`, which the runtime calls by name.
- A function that captures a stack trace and skips frames by count, such as `StackTrace.capture`. Inlined, it would skip a frame of its caller.
- A function reached only through a pointer or a table at a call that `opt` cannot resolve.

```anti
//! name: inl
import anti.io;
import anti.text;

fn square(n: int) -> int
{
	return n *% n;
}

export fn hyp2(a: int, b: int) -> int
{
	return square(a) +% square(b);
}

fn main() -> int
{
	let line = f"hyp2 {hyp2(3, 4)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
hyp2 25
```

### How antic builds it

A dev build writes the two calls as they stand.

```llvm excerpt=inl target=linux-x86_64 mode=dev
define internal noundef i64 @hyp2(i64 noundef %p0, i64 noundef %p1) #0 {
...
  %v1 = call i64 (i64) @inl.square(i64 %v0)
...
  %v3 = call i64 (i64) @inl.square(i64 %v2)
```

```asm excerpt=inl target=linux-x86_64 mode=dev
<hyp2>:
pushq %rbp
movq %rsp, %rbp
subq $0x30, %rsp
movq %rdi, -0x28(%rbp)
movq %rsi, -0x10(%rbp)
callq 0x15 <hyp2+0x15>
movq %rax, -0x8(%rbp)
movq -0x10(%rbp), %rdi
callq 0x22 <hyp2+0x22>
movq %rax, -0x20(%rbp)
addq -0x8(%rbp), %rax
movq %rax, -0x18(%rbp)
addq $0x30, %rsp
popq %rbp
retq
```

`hyp2` builds a frame of 48 bytes, stores its two parameters there and calls `square` twice. The target of each `callq` is filled in by the linker. Each result goes through a slot of the frame before the `addq`.

```llvm excerpt=inl target=linux-x86_64 mode=release
define internal noundef i64 @hyp2(i64 noundef %p0, i64 noundef %p1) #4 {
b0:
  %v2.i = mul i64 %p0, %p0
  %v2.i1 = mul i64 %p1, %p1
  %v6 = add i64 %v2.i1, %v2.i
  ret i64 %v6
}
```

In the release module the body of `square` stands twice in `hyp2`, as the two `mul` lines. Their names end in `.i`, the mark of an inlined instruction. The module holds no function `inl.square`.

```asm excerpt=inl target=linux-x86_64 mode=release
<hyp2>:
imulq %rdi, %rdi
imulq %rsi, %rsi
leaq (%rsi,%rdi), %rax
retq
```

The function is four instructions and has no frame.

Under `-g` a release build records an inlined call in the debug information, so a debugger and a stack trace with line numbers still name `square`. [Chapter 20]({{% relref "20-targets" %}}) describes that.

Where it lives: `llvm_opt_options` and `run_opt` of `src/antic/llvm_run.c` hold the pipeline and the threshold, which `LLVM_INLINE_THRESHOLD` of `src/antic/llvm_run.h` names. `src/antic/linker.c` passes the same threshold to the LTO link. `frames_kept` and `main_kept` of `src/antic/llvm_emit.c` mark the functions that stay.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A call of a small function | one call, one return, one frame | none: the body stands in the caller |
| A call of a large function | the same | one call |
| Code size | one copy of each function | one copy per call site of an inlined function, and no copy of a function inlined everywhere |
| A call into the runtime | one call | one call under `--lto none`, and inlined where lld chooses under the default full LTO |
| Compile time | no optimizer runs | `opt` and the LTO link run |

The threshold is a cost in the units of `opt`, not a count of source lines. No option of antic changes it.

### Pitfalls

For a Java programmer:

- Inlining is fixed when the program is built. No run-time profile changes it. `--profile-generate` and `--profile-use` of [chapter 21]({{% relref "21-performance" %}}) give `opt` a profile at build time.
- A dev build inlines no call and keeps every local in memory, as the excerpt of `hyp2` shows.

For a C programmer:

- No header and no `static inline` is needed. A release build sees every function of the program and the library.
- A function pointer taken from a function keeps that function in the program, even when every direct call was inlined.

For a C++ programmer:

- No `inline`, `always_inline` or `noinline` exists in the language.
- Inlining across modules needs no LTO, since a release build compiles the modules together. LTO adds the runtime, which is C.
- A stack trace of a release build without `-g` lacks the frames of inlined functions.

## Anonymous functions and closures

### What you know

A C function pointer carries code and nothing else, so a callback takes a `void *` beside it. A C++ lambda is an object that holds its captures, by copy or by reference as its list says. One that captures by reference may outlive what it refers to. A Java lambda captures effectively final variables by value and is an object on the heap.

### The feature

`fn(params) -> R { body }` in the place of an expression is an anonymous function. Passed to a parameter of function type it may leave out the types of its parameters and its result, which come from that parameter. Anywhere else it writes them.

An anonymous function that names a local of the function around it is a closure. It captures that local by reference: it reads the current value and may assign it, and the enclosing function sees the change.

A closure never outlives what it captures, and the checker enforces that through where a closure may go.

- It is passed to a parameter of function type that is not marked `keep`.
- It is held in a local, which is then used in the same way. The local does not outlive a variable the closure captures.
- It is never returned, never stored in a field and never passed to a `keep` parameter.

The receiver of a function parameter without `keep` may call it and may pass it on to another such parameter. It cannot store it.

A parameter marked `concurrent` hands its function on to `parallel` or `dispatch`. A closure passed there may change a captured variable only when the type of the variable is thread-safe. [Chapter 13]({{% relref "13-threads" %}}) describes that rule.

An anonymous function that captures nothing is an ordinary function. It may go wherever a named function goes.

```anti
//! name: clo
import anti.io;
import anti.text;

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
		if (n & 1) == 0 {
			hits +%= 1;
		}
	});
	return hits;
}

fn fold(items: []int, start: int, f: fn(int, int) -> int) -> int
{
	let acc = start;
	for x in items {
		acc = f(acc, x);
	}
	return acc;
}

fn show(n: int)
{
	let line = f"item {n}";
	io.println(line);
	free(line.ptr);
}

fn main() -> int
{
	let items = [3, 4, 6, 7];
	let s = items[0..4];
	each(s[0..2], show);
	let limit = 5;
	let above = 0;
	let test = fn(n: int) {
		if n > limit {
			above += 1;
		}
	};
	each(s, test);
	let sum = fold(s, 0, fn(a, b) -> int { return a + b; });
	let line = f"even {count_even(s)}, above {above}, sum {sum}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
item 3
item 4
even 2, above 2, sum 20
```

`each` takes a named function, `show`, and two closures. The closure in `count_even` changes `hits`, and the one held in `test` reads `limit` and changes `above`. The function passed to `fold` captures nothing.

The checker refuses each of the following.

```anti refused
//! error: a closure is never kept, and this one captures `k`, so a `snapshot fn` at a `keep own` parameter or an `own` field takes its value
fn scale_by() -> fn(int) -> int
{
	let k = 2;
	return fn(n: int) -> int { return n * k; };
}
```

```anti refused
//! error: a closure is never kept, and this one captures `total`
fn store(keep f: fn(int)) { }

fn caller()
{
	let total = 0;
	store(fn(n) { total += n; });
}
```

```anti refused
//! error: `f` is not marked `keep` and cannot be stored
fn store(keep f: fn(int)) { }

fn pass_on(f: fn(int))
{
	store(f);
}
```

```anti refused
//! error: `n` needs a type, which an anonymous function takes from a parameter of function type
fn caller()
{
	let f = fn(n) { };
}
```

```anti refused
//! error: the anonymous function takes 2 parameters, and `fn(int)` gives 1
fn each(f: fn(int)) { f(1); }

fn caller()
{
	each(fn(a, b) { });
}
```

```anti refused
//! error: `total` is changed in a closure at a `concurrent` parameter, and `int` is not thread-safe
fn spread(concurrent f: fn(int)) { f(1); }

fn caller()
{
	let total = 0;
	spread(fn(n) { total += n; });
}
```

### How antic builds it

An anonymous function becomes a function of the module, named after the function around it and a number: `clo.count_even.0`. A closure takes one more parameter, the last: a pointer to its context.

The context is a record in the frame of the enclosing function with one pointer per captured variable. A function value at a parameter without `keep` is two words, the address of the code and the address of the context.

```ir excerpt=clo target=linux-x86_64 mode=dev
fn clo.count_even(%0: agg []int) -> i64 {
b0:
    %1 = slot i64
    %3 = slot count_even.0.context
    %4 = slot fn(...)
    lifestart %1
    store i64 0, %1
    %2 = addr @clo.count_even.0
    store ptr %1, %3
    store ptr %2, %4
    %5 = ptradd %4, offset_of fn(...).context
    store ptr %3, %5
    %6 = load ptr %4
    %7 = ptradd %4, offset_of fn(...).context
    %8 = load ptr %7
    call void @clo.each(%0, %6, %8)
    %9 = load i64 %1 !type i64
    ret i64 %9
}
```

`%1` is `hits`, which has a slot in the frame because the closure takes its address. `%3` is the context: `store ptr %1, %3` puts the address of `hits` into it. `%4` is the pair. The code, `@clo.count_even.0`, goes into its first word and the address of the context into its second. The call of `each` passes the slice and the two words of the pair as three arguments. After the call the function loads `hits` from its slot, where the closure wrote it.

```ir excerpt=clo target=linux-x86_64 mode=dev
fn clo.each(%0: agg []int, %1: ptr, %2: ptr) {
b0:
    %3 = slot fn(...)
    store ptr %1, %3
    %4 = ptradd %3, offset_of fn(...).context
    store ptr %2, %4
...
    %13 = load ptr %3
    %14 = ptradd %3, offset_of fn(...).context
    %15 = load ptr %14
    call void %13 via @clo.fn.0(%12, %15)
```

`each` receives the code as `%1` and the context as `%2`. The call in the loop is indirect, through `%13`, and passes the element and then the context.

```ir excerpt=clo target=linux-x86_64 mode=dev
fn clo.count_even.0(%0: i64, %1: ptr) {
b0:
    %2 = load ptr %1
```

The closure loads the address of `hits` from its context once, at its entry. Every read and write of `hits` in its body then goes through `%2`.

A named function such as `show` is passed with a context of null. The callee passes that null as one more argument, which `show` does not read. The C conventions of all six targets allow a caller to pass an argument the callee ignores.

A release build often removes all of it. The release module of this example holds one function of the program, `clo.main`. `opt` inlined `each`, `count_even`, `fold`, `show` and the three anonymous functions into it, which leaves no indirect call and no context.

```llvm excerpt=clo target=linux-x86_64 mode=release
define internal noundef i64 @clo.main() #4 {
...
  call void @anti.text.Builder.append_int(ptr %s2.i, i64 3, i64 10, i8 zeroext 0, i64 0, i8 zeroext 1, i8 zeroext 0)
...
  call void @anti.text.Builder.append_int(ptr %s2.i, i64 4, i64 10, i8 zeroext 0, i64 0, i8 zeroext 1, i8 zeroext 0)
```

The two calls of `append_int` are `each(s[0..2], show)`. The loop of `each` and the call through the function value are gone. Each call of `show` stands in `main` with its element as a constant, 3 and then 4.

Where it lives: `sema_check_anonymous`, `sema_capture` and `sema_check_closure_lifetime` of `src/antic/sema_closure.c` check a closure. `lower_closure` and `lower_anonymous_function` of `src/antic/lower_function.c` write the context and the function. `lower_add_param` of `src/antic/lower.c` splits a function parameter into its two words.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Making a closure | one store per captured variable, two stores for the pair | the same, or nothing when the callee is inlined |
| Bytes in the frame | 8 per captured variable, and 16 for the pair | the same, or none |
| A captured variable | lives in memory, not in a register | the same, unless the closure is inlined |
| Calling through a function parameter | one indirect call, with one more argument | the same, or a direct or inlined call when `opt` knows the target |
| Entry of a closure | one load per captured variable | the same |
| Heap allocations | 0 | 0 |
| Locks and atomic operations | 0 | 0 |
| Checks inserted | none | none |

A closure costs no allocation in either mode, because its context is part of a frame that outlives every use of it. That is what the rules of the checker buy.

### Pitfalls

For a Java programmer:

- A closure changes the locals it captures. They need not be effectively final.
- A closure cannot be stored or returned. A value that outlives the call is a `snapshot fn` of the next section.
- A closure is no object. It has no identity and no class.

For a C programmer:

- The `void *` beside a callback is built in: a function parameter is the code and the context together.
- A function pointer for an `extern fn` is one C pointer. A closure cannot be passed there.

For a C++ programmer:

- Every capture is by reference, as `[&]`. No capture list exists. Capture by copy is `snapshot fn`.
- A dangling capture is a compile error, not undefined behaviour.
- A closure does not convert to a plain function pointer, even with no state that matters to the caller.

## snapshot fn and own fn

### What you know

A C++ lambda with `[=]` copies its captures into the lambda object, and `std::function` moves that object to the heap when it is too large. Java lambdas always capture by value and always live on the heap. C keeps a callback for later as a function pointer and a `void *` to memory that somebody must free.

### The feature

`snapshot fn(params) { body }` is an anonymous function that copies the variables it names at the moment it is made. The body reads the copies and cannot assign them. A later change of the original does not reach the snapshot.

A snapshot holds values that copy fully: numbers, `bool`, `char`, structs of those, and `str`. It refuses a class value, a pointer and a slice.

A snapshot may be kept. The places that keep one own it.

- An `own` field of function type, `own handler: fn(int)`.
- A parameter marked `keep own`, which the function then stores in such a field.

Such a value is an `own fn`. Its owner frees the snapshot: a class in its teardown, an assignment to the field before it stores the new value. `=` does not copy an `own fn`, and `dup` of [chapter 11]({{% relref "11-ownership" %}}) copies it with its snapshot. A named function fits an `own fn` place as well, with no snapshot.

A snapshot also goes where a closure goes, to a parameter without `keep`. It then lives in the frame, as a closure does, and costs no allocation.

```anti
//! name: snap
import anti.io;
import anti.text;

class Panel
{
	own handler: fn(int) = ignore,
	plain: fn(int) = ignore,

	pub fn on_click(self, keep own f: fn(int))
	{
		self.handler = f;
	}

	pub fn click(self, n: int)
	{
		self.handler(n);
	}
}

fn ignore(n: int) { }

fn save(name: str, n: int)
{
	let line = f"save {name} {n}";
	io.println(line);
	free(line.ptr);
}

fn run(f: fn(int))
{
	f(1);
}

fn main() -> int
{
	let p = Panel { };
	let name = "report";
	let scale = 10;
	p.on_click(snapshot fn(n) { save(name, n * scale); });
	name = "draft";
	scale = 20;
	p.click(2);
	run(snapshot fn(n) { save(name, n * scale); });
	let q = dup(p);
	q.click(3);
	let line = f"sizes: Panel {size_of(Panel)}, fn(int) {size_of(fn(int))}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
save report 20
save draft 20
save report 30
sizes: Panel 32, fn(int) 8
```

The handler of `p` still says `report` and multiplies by 10 after `main` changed both variables. The second snapshot is made after the change and reads `draft` and 20. `q` is a copy of `p` with a snapshot of its own.

The checker refuses each of the following.

```anti refused
//! error: `k` is changed in a snapshot, which is read-only
fn each(f: fn(int)) { f(1); }

fn caller()
{
	let k = 2;
	each(snapshot fn(n) { k += n; });
}
```

```anti refused
//! error: a snapshot holds values that copy fully, and `b` is `Tally`
class Tally { pub v: int = 0, }

fn each(f: fn(int)) { f(1); }

fn caller()
{
	let b = Tally { };
	each(snapshot fn(n) { let m = b.v; });
}
```

```anti refused
//! error: a `snapshot fn` is kept where it is owned, by a `keep own` parameter or an `own` field
fn store(keep f: fn(int)) { }

fn caller()
{
	let k = 2;
	store(snapshot fn(n) { let m = n * k; });
}
```

### How antic builds it

A snapshot is a record: its size in bytes first, then one field per captured variable. The function reads its captures from that record, which it receives as its context.

For an `own fn` the record lives on the heap. Lowering computes the size of the record plus the length of every captured `str` and one byte for its NUL, and calls `anti_rt_snapshot_new` for the block. It copies each `str` and a NUL behind the record with `anti_rt_snapshot_text`. The snapshot therefore owns the bytes of its texts and does not depend on the `str` it was made from, and the `ptr` of each text is a C string, as for every `str`.

```ir excerpt=snap target=linux-x86_64 mode=release
    %16 = add i64 size_of main.0.snapshot, %15
    %17 = add i64 %16, 1
    %18 = call ptr @anti_rt_snapshot_new(%17)
    %19 = ptradd %18, offset_of main.0.snapshot.name
...
    call void @anti_rt_snapshot_text(%18, size_of main.0.snapshot, %22, %21)
    store i64 size_of main.0.snapshot, %19
...
    %26 = ptradd %18, offset_of main.0.snapshot.scale
```

`%15` is the length of `name`. `%17` is the size of the record plus that length and its NUL, and `%18` the new block. `anti_rt_snapshot_text` copies the bytes of `name` and a NUL to the offset where the record ends. The `store` writes that offset into the field of `name`, in place of a pointer, so the block can be copied byte by byte. `%26` is the field of `scale`, which takes the value 10.

`anti_rt_snapshot_new` is `malloc`, a write of the size and one atomic add to the count of live snapshots, which a leak check reads. `anti_rt_snapshot_free` is one atomic subtract and `free`. `anti_rt_snapshot_dup` reads the size, calls `anti_rt_snapshot_new` for a block of it and copies the block.

An `own fn` is two words, the code and the pointer to the snapshot. In `Panel` the `own` field is 16 bytes and the plain field 8, which with the table pointer gives the 32 bytes the example prints. A named function in an `own fn` place has a null snapshot, and freeing it does nothing.

The snapshot passed to `run` is the same record in a slot of the frame of `main`. Its `str` is copied as two words, and nothing calls the runtime.

Where it lives: `check_snapshot` of `src/antic/sema_closure.c` and `require_owned` of `src/antic/sema_expr.c`. `lower_snapshot` of `src/antic/lower_function.c`, and `lower_free_snapshot` and `lower_dup_snapshot` of `src/antic/lower_owning.c`. The four runtime functions are in `src/rt/snapshot.c`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A `snapshot fn` at a parameter without `keep` | one store per capture into the frame | the same, or nothing when inlined |
| Making an `own fn` | one call of `anti_rt_snapshot_new`: one `malloc` and one atomic add. One `anti_rt_snapshot_text` per captured `str` | the same |
| Bytes on the heap | 8 for the size, the size of each capture, and the bytes of each `str` with a NUL after them | the same |
| Bytes of an `own fn` value | 16 | 16 |
| Freeing an `own fn` | one call of `anti_rt_snapshot_free`: one atomic subtract and one `free` | the same |
| `dup` of an `own fn` | one call of `anti_rt_snapshot_dup`: one `malloc`, one atomic add and one copy of the block | the same |
| Calling it | one indirect call with the snapshot as the last argument | the same |
| Checks inserted | none | none |

For the snapshot of the example the block is 8 bytes of size, 16 for the `str`, 8 for the `int` and 6 for the text `report`, which is 38 bytes.

### Pitfalls

For a Java programmer:

- A snapshot is the capture of a Java lambda: a copy, read-only. It cannot hold an object, only values.
- An `own fn` is freed by its owner at a known place. No collector finds it.

For a C programmer:

- The snapshot replaces the `void *user_data` and the question of who frees it. The owner of the `own fn` does.
- A captured `str` is copied with its bytes. The snapshot stays valid after the original text is freed.

For a C++ programmer:

- `snapshot fn` is `[=]` with every capture `const`. It has no `mutable`.
- An `own fn` is closer to `std::unique_ptr` of a callable than to `std::function`. `=` does not copy it, and `dup` does.
- A snapshot cannot capture a pointer, so it cannot dangle.

## Function values

### What you know

A C function pointer is one address, called through `(*f)(x)` or `f(x)`. `std::function` of C++ is an object larger than one pointer that may own heap memory, and a lambda has a type of its own. Java has no function type: a lambda is an instance of an interface with one method.

### The feature

`fn(A, B) -> R` is a type. The name of a function is a value of it. A value of function type is called as a function is, `f(x)`. Two function values compare with `==`.

`fn(A) -> R may fail` is the type of a function that may fail. A call through it is handled as a direct call is. A function that may fail does not fit a type without the marking.

`?fn(A) -> R` may hold `none` and is tested before the call, as a `?*T` is. A plain `fn(A) -> R` never holds `none`.

The form of a function value depends on where it stands.

| Place | Holds | Words |
|---|---|---|
| A field, a static field, a result, a `keep` parameter | the address of a function | 1 |
| A parameter without `keep`, a local | the code and a context | 2 |
| An `own` field, a `keep own` parameter | the code and an owned snapshot | 2 |
| A parameter of an `extern fn` | a C function pointer | 1 |

A named function and an anonymous function without captures fit every place. A closure fits the second row alone, and a `snapshot fn` the second and the third.

```anti
//! name: fnval
import anti.io;
import anti.text;

struct Op
{
	name: str,
	run: fn(int, int) -> int,
	check: ?fn(int) -> bool,
}

fn add(a: int, b: int) -> int { return a + b; }
fn mul(a: int, b: int) -> int { return a * b; }
fn positive(n: int) -> bool { return n > 0; }

fn pick(name: str) -> fn(int, int) -> int
{
	if text.equal(name, "mul") {
		return mul;
	}
	return add;
}

export fn call_it(f: fn(int, int) -> int, a: int, b: int) -> int
{
	return f(a, b);
}

fn half(n: int) -> int may fail
{
	if (n & 1) != 0 {
		fail "odd";
	}
	return n >> 1;
}

fn main() -> int
{
	let ops = [
		Op { name: "add", run: add, check: positive },
		Op { name: "mul", run: mul, check: none },
	];
	for op in ops {
		let ok = true;
		if let c = op.check {
			ok = c(-1);
		}
		let line = f"{op.name}: {op.run(6, 7)}, checked {ok}";
		io.println(line);
		free(line.ptr);
	}
	let f = pick("mul");
	let h: fn(int) -> int may fail = half;
	let r = h(10) catch { yield -1; };
	let same = f == mul;
	let line = f"pick {f(3, 4)}, call_it {call_it(add, 3, 4)}, half {r}, same {same}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: fn {size_of(fn(int, int) -> int)}, ?fn {size_of(?fn(int) -> bool)}, Op {size_of(Op)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
add: 13, checked false
mul: 42, checked true
pick 12, call_it 7, half 5, same true
sizes: fn 8, ?fn 8, Op 32
```

The checker refuses each of the following.

```anti refused
//! error: `h` may be `none`, check it or use `?fn(...)`
fn caller() -> int
{
	let h: ?fn(int) -> int = none;
	return h(1);
}
```

```anti refused
//! error: expected `fn(int) -> int`, found `fn(int) -> int may fail`
fn half(n: int) -> int may fail
{
	if (n & 1) != 0 {
		fail "odd";
	}
	return n >> 1;
}

fn caller() -> int
{
	let f: fn(int) -> int = half;
	return f(4);
}
```

### How antic builds it

A function value of one word is the address of the function, a `ptr` in the LLVM module. `?fn` is the same pointer, with null for `none`. An `Op` is therefore 16 bytes of `str` and two pointers, 32 bytes, and its layout is that of the C struct with two function pointers.

A call through a value of one word is an indirect call with the arguments of the signature. A call through a value of two words adds the context as the last argument.

```llvm excerpt=fnval target=linux-x86_64 mode=release
define internal noundef i64 @call_it(ptr nofree noundef readonly captures(none) %p0, ptr noundef %p1, i64 noundef %p2, i64 noundef %p3) #0 {
b0:
  %v4 = tail call i64 %p0(i64 %p2, i64 %p3, ptr %p1) #6
  ret i64 %v4
}
```

`call_it` receives the code as `%p0` and the context as `%p1`. The call goes through `%p0` and passes `a`, `b` and the context. It is a tail call, so the function builds no frame.

```asm excerpt=fnval target=linux-x86_64 mode=release
<call_it>:
movq %rdx, %rax
movq %rsi, %rdx
movq %rdi, %r8
movq %rax, %rdi
movq %rcx, %rsi
jmpq *%r8
```

The five moves put `a` and `b` into the first two argument registers and the context into the third. `jmpq *%r8` jumps to the code.

The C header of `call_it` writes the pair as a callback and a `void *`. [Chapter 17]({{% relref "17-c-interop" %}}) shows the header.

Where it lives: `types_fn_form` of `src/antic/types.c` holds the forms of a function type. `sema_param_form` of `src/antic/sema.c` chooses one and `require_fn_form` of `src/antic/sema_expr.c` converts between them. `lower_call` of `src/antic/lower_expr.c` writes the call, and `lower_push_argument` of `src/antic/lower.c` splits a pair.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A value in a field or a result | 8 bytes | 8 bytes |
| A value at a parameter or in a local | 16 bytes | 16 bytes, or none when inlined |
| `?fn` | 8 bytes, the same as `fn` | 8 bytes |
| A call through it | one indirect call | one indirect call, or a direct or inlined one when `opt` proves the target |
| The test of a `?fn` | one compare with null | the same |
| Heap allocations | 0 | 0 |
| Checks inserted | none | none |

An indirect call is not inlined unless `opt` learns the target. A table of function values, such as the `ops` of the example, is called through the pointer in both modes.

### Pitfalls

For a Java programmer:

- A function type is no interface. `fn(int) -> int` has no methods and no implementing class.
- A method of an object as a value, `c.area`, is bound to that object. [Chapter 9]({{% relref "09-classes" %}}) describes bound functions.

For a C programmer:

- A function value in a field is the C function pointer. Its struct has the C layout.
- A function parameter is two words unless it is `keep`. The header shows both forms.
- `fn` never holds null. `?fn` does, and a call needs the test first.

For a C++ programmer:

- No function value allocates. The `own fn` of the last section is the one that owns heap memory, and its type says so.
- A closure in a field is refused. A field takes a named function, or with `own` a snapshot.
- Two function values are equal when they hold the address of one function. `==` does not compare what two functions do.
