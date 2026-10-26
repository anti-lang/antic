---
title: "C interop"
description: "extern fn, the C types, the ABI of a call, anti bind, link, libraries for C with --lib and --bundle-runtime, and plugins across the boundary."
summary: "`extern fn` and the C types, the ABI of a call, `anti bind`, `link`, libraries for C with `--lib` and `--bundle-runtime`, and plugins across the boundary, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-26T12:00:00+01:00
draft: true
weight: 170
tags: [compilers, programming-languages]
keywords: [foreign function interface, extern, C ABI, bindings, link, static library, shared library, plugins]
---

Anti calls a C function with one call instruction and is called from C the same way. Every Anti function uses the C convention of its target and every struct has C layout, so no value is converted at the boundary. `extern fn` declares a C function, `export` gives an Anti item its plain C name, and `antic --lib` writes a library with its header.

## extern fn and C types

### What you know

Java reaches C through JNI or the foreign function API, with a stub and a conversion of each argument. C++ declares a C function inside `extern "C"` and calls it directly. C includes the header, and the linker binds the name.

### The feature

`extern fn` declares a function that C defines. It has a name, parameters and a result, and it ends in `;` with no body. `...` after the last parameter declares a variadic function. The name is the symbol of C as it stands, with no module in it. `pub extern fn` lets other modules call it.

```anti
extern fn strlen(s: ?*byte) -> c_size_t;
extern fn snprintf(out: ?*byte, room: c_size_t, format: ?*byte, ...) -> c_int;
```

The C types name the types of C on the target. Eleven of the fourteen are other names of a sized type of Anti. The other three are types of their own, since their width follows the target.

| Anti | C | The same type as | Bytes |
|---|---|---|---:|
| `c_char` | `char`, read as signed | `i8` | 1 |
| `c_uchar` | `unsigned char` | `u8` and `byte` | 1 |
| `c_short`, `c_ushort` | `short`, `unsigned short` | `i16`, `u16` | 2 |
| `c_int`, `c_uint` | `int`, `unsigned int` | `i32`, `u32` | 4 |
| `c_longlong`, `c_ulonglong` | `long long`, `unsigned long long` | `int`, `u64` | 8 |
| `c_size_t` | `size_t` | `u64` | 8 |
| `c_float`, `c_double` | `float`, `double` | `f32`, `f64` | 4, 8 |
| `c_long`, `c_ulong` | `long`, `unsigned long` | none | 8, and 4 on Windows |
| `c_wchar` | `wchar_t` | none | 4, and 2 on Windows |

The rules of a declaration and of a call:

- Every pointer of an `extern fn` is written `?*T`, in a parameter and in the result. C gives no promise that a pointer is not null. A result is tested before it is read, as [chapter 7]({{% relref "07-optionals" %}}) describes.
- `?*byte` stands for `void *` and for `char *`.
- A call converts nothing. An `int` does not pass where a `c_int` stands, and `n as c_int` is the conversion.
- A variadic argument has type `i32`, `u32`, `int`, `u64`, `c_long`, `c_ulong`, `float` or a pointer. An `f32` is refused, since C promotes it to `f64`.
- A struct passes by value as the C struct of the same layout, which [chapter 8]({{% relref "08-structs-enums" %}}) describes.
- A parameter of function type takes a named function. A closure that captures a variable is refused, since C receives one pointer and no context.
- A text literal ends in a zero byte, so `"hello".ptr` is a C string. `text.from_c(p)` of `anti.text` gives the `str` of a C string.
- `may fail` and `export` are refused on an `extern fn`.
- A parameter of an `extern fn` takes no default value, and no `lent`, `own`, `concurrent` or `keep own`. C keeps what it likes, tears nothing down and receives one function pointer. `keep` before a function parameter is accepted and changes nothing, since every function pointer of C is one word.
- One C function has one signature in a program. A second `extern fn` of the same name in the module is `` `abs` is already declared ``, and one with another signature in another module stops the build with `` antic: app.anti declares `extern fn abs` with another signature than a library of the program ``.

The symbol comes from the C library of the target, which every program links. It may also come from an object or an archive on the command line of antic, `antic main.anti helper.o libshapes.a`, or from a library that a `link` line names.

```anti
//! name: cee
import anti.io;
import anti.text;

extern fn strlen(s: ?*byte) -> c_size_t;
extern fn abs(n: c_int) -> c_int;
extern fn snprintf(out: ?*byte, room: c_size_t, format: ?*byte, ...) -> c_int;
extern fn qsort(base: ?*byte, count: c_size_t, size: c_size_t, order: fn(?*byte, ?*byte) -> c_int);
extern fn getenv(name: ?*byte) -> ?*byte;

fn by_value(a: ?*byte, b: ?*byte) -> c_int
{
	let x = a as ?*i32 catch fatal;
	let y = b as ?*i32 catch fatal;
	if *x < *y {
		return -1;
	}
	if *x > *y {
		return 1;
	}
	return 0;
}

fn main() -> int
{
	let n = strlen("hello".ptr);
	let d = abs(-7);
	let room: [32]byte = [0; 32];
	let wrote = snprintf(&room[0], 32, "%d and %s".ptr, 42 as c_int, "text".ptr);
	let numbers: [5]i32 = [5, 3, 9, 1, 4];
	qsort(&numbers[0] as ?*byte, 5, 4, by_value);
	let missing = getenv("ANTI_GUIDE_UNSET".ptr) == none;
	let made = text.from_c(&room[0]);
	let line = f"strlen {n}, abs {d}, snprintf wrote {wrote}: {made}";
	io.println(line);
	free(line.ptr);
	line = f"sorted {numbers[0]} {numbers[1]} {numbers[2]} {numbers[3]} {numbers[4]}, unset variable is none: {missing}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: c_char {size_of(c_char)}, c_short {size_of(c_short)}, c_int {size_of(c_int)}, c_longlong {size_of(c_longlong)}, c_size_t {size_of(c_size_t)}, c_float {size_of(c_float)}, c_double {size_of(c_double)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
strlen 5, abs 7, snprintf wrote 11: 42 and text
sorted 1 3 4 5 9, unset variable is none: true
sizes: c_char 1, c_short 2, c_int 4, c_longlong 8, c_size_t 8, c_float 4, c_double 8
```

`qsort` calls `by_value` back from C. The function is an ordinary Anti function, and C calls it through its address.

The checker refuses the following.

```anti refused
//! error: every pointer of the parameter `path` of `extern fn open` is `?*T`
extern fn open(path: *byte) -> c_int;
```

```anti refused
//! error: `may fail` belongs to an Anti function, and an `extern fn` writes `-> ?*Error` by hand
extern fn parse(s: ?*byte) -> c_int may fail;
```

```anti refused
//! error: an `extern fn` cannot be exported
export extern fn tick() -> c_int;
```

```anti refused
//! error: an `extern fn` declares what C declares, and C has no default values
extern fn abs(n: c_int = 3) -> c_int;
```

```anti refused
//! error: an `extern fn` takes no `lent` parameter, since C may keep the pointer
extern fn use_it(lent p: ?*byte);
```

```anti refused
//! error: an `extern fn` takes plain C function pointers, and `keep own` holds a snapshot
extern fn run(keep own f: fn(c_int));
```

```anti refused
//! error: `abs` is already declared
extern fn abs(n: c_int) -> c_int;
extern fn abs(n: c_long) -> c_long;
```

```anti refused
//! error: expected `i32`, found `int`
extern fn abs(n: c_int) -> c_int;

fn size(n: int) -> c_int
{
	return abs(n);
}
```

```anti refused
//! error: expected `?*byte`, found `str`
extern fn strlen(s: ?*byte) -> c_size_t;

fn size() -> c_size_t
{
	return strlen("abc");
}
```

```anti refused
//! error: a variadic argument has type i32, u32, int, u64, c_long, c_ulong, float or a pointer, found `str`
extern fn printf(format: ?*byte, ...) -> c_int;

fn show() -> c_int
{
	return printf("%s".ptr, "text");
}
```

```anti refused
//! error: the value may be `none`, check it or use `?*T`
extern fn getenv(name: ?*byte) -> ?*byte;

fn first() -> byte
{
	return *getenv("HOME".ptr);
}
```

```anti refused
//! error: a closure is never kept, and this one captures `n`, so a `snapshot fn` at a `keep own` parameter or an `own` field takes its value
extern fn qsort(base: ?*byte, count: c_size_t, size: c_size_t, order: fn(?*byte, ?*byte) -> c_int);

fn count(a: *[4]i32)
{
	let n = 0;
	qsort(a as ?*byte, 4, 4, fn(x, y) {
		n += 1;
		return 0;
	});
}
```

```anti refused
//! error: expected `int`, found `c_long`
fn widen(a: c_long) -> int
{
	return a;
}
```

A `c_long` converts with `as`, since it is 4 bytes on one target and 8 on another.

### How antic builds it

An `extern fn` is a `declare` line of the LLVM module, and a call of it is one `call`. The compiler writes no stub, no wrapper and no conversion. The three functions below call C and do nothing else.

```anti
//! name: tocee
extern fn strlen(s: ?*byte) -> c_size_t;
extern fn printf(format: ?*byte, ...) -> c_int;
extern fn qsort(base: ?*byte, count: c_size_t, size: c_size_t, order: fn(?*byte, ?*byte) -> c_int);

export fn length(s: ?*byte) -> c_size_t
{
	return strlen(s);
}

export fn report(n: c_int, ratio: f64) -> c_int
{
	return printf("%d of %f\n".ptr, n, ratio);
}

fn by_value(a: ?*byte, b: ?*byte) -> c_int
{
	let x = a as ?*i32 catch fatal;
	let y = b as ?*i32 catch fatal;
	return *x - *y;
}

export fn sort4(numbers: ?*i32)
{
	qsort(numbers as ?*byte, 4, 4, by_value);
}
```

```llvm excerpt=tocee target=linux-x86_64 mode=release
define dso_local noundef i64 @length(ptr
b0:
  %v1 = tail call i64 @strlen(ptr nonnull dereferenceable(1) %p0)
  ret i64 %v1
...
  %v3 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @tocee.
...
  tail call void @qsort(ptr %p0, i64 4, i64 4, ptr nonnull @tocee.by_value)
...
declare noundef i64 @strlen(ptr noundef captures(none))
```

- `length` is one call of `strlen` and a return. The call is a tail call, so the machine code is a jump.
- LLVM knows `strlen` by its name and marks its argument as a pointer to at least one byte. An `extern fn` of the C library gets the attributes LLVM keeps for that name.
- The call of `printf` carries its type `i32 (ptr, ...)`, which marks the variadic call.
- The last argument of `qsort` is the address of `tocee.by_value`. A function passes to C as one pointer.
- The `declare` line is all that the module holds of `strlen`.

The machine code of `report` shows the one rule of a variadic call on x86-64.

```asm excerpt=tocee target=linux-x86_64 mode=release
<report>:
movl %edi, %esi
leaq (%rip), %rdi
movb $0x1, %al
jmp
```

- `n` arrives in `edi` and moves to `esi`, the second integer argument of `printf`. The `leaq` puts the address of the format into `rdi`.
- `ratio` arrives in `xmm0` and stays there.
- `movb $0x1, %al` is the count of vector registers a variadic call uses, which System V asks for.
- The `jmp` is the tail call of `printf`.

On linux-arm64 the same function moves `w0` to `w1`, loads the format into `x0` and branches to `printf`, with `ratio` in `d0`. On macos-arm64 it stores `n` and `ratio` on the stack before the call, since Apple's convention passes every variadic argument there.

A dev build stores the argument in a slot of the frame and loads it again around the same call. It adds no check to a call of C.

Where it lives: `src/antic/sema_export.c` checks the declaration, and `declare_function` of `src/antic/lower.c` names the function. `declaration` of `src/antic/llvm_emit.c` writes the `declare` and `llvm_emit_call` of `src/antic/llvm_emit_call.c` the call, and `src/antic/abi.c` classifies the arguments for each target.

### What it costs

The costs are the same in dev and in release.

| Cost | Count |
|---|---|
| A call of an `extern fn` | one `call`, or one `jmp` in tail position. No stub and no conversion |
| Bytes of a C type | `size_of` of the table above: 1, 2, 4 and 8 |
| Heap allocations | none |
| Calls into the runtime | none |
| Atomic operations and locks | none |
| Checks inserted | none, in either mode. The test of a `?*T` result is code the program writes |
| Copies of code | none. One `declare` per function and module |

The other ways to reach code that C compiled:

- An `extern fn` written by hand suits a few functions of the C library, as `strlen` or `qsort`. Nothing checks it against a header.
- `anti bind` writes the declarations of a whole header and a probe that checks every layout. It suits a library of more than a few functions, and an inline function needs its shim.
- A plugin of Anti, the last section, is the form for code that is Anti on both sides. Its calls go through a table, two loads and an indirect call each, where a call of C is one `call`.

`text.from_c(p)` is one call of `anti_rt_text_from_c`, which runs `strlen` and copies no byte. The `str` it gives is a view of the C string.

### Pitfalls

For a Java programmer:

- No exception crosses the boundary and nothing checks the arguments. A wrong pointer is a crash in C.
- A `str` is no C string. `s.ptr` of a literal ends in a zero byte. `s.ptr` of a part of a text runs on into the rest of the text.

For a C programmer:

- `c_char` is `i8`, a signed byte, on every target. `char` of C is unsigned on Linux arm64.
- `int` of Anti is 64 bits. `c_int` is the `int` of C, and the two do not mix without `as`.
- `c_long` is 4 bytes on Windows and 8 elsewhere. Code that stores one in an `int` converts with `as`.
- The declaration is not checked against the header. A wrong parameter type links and passes the wrong register.

For a C++ programmer:

- No `extern "C"` block exists. Each function is declared on its own line, and `anti bind` writes the lines from a header.
- A closure never passes to C. C code that takes a callback and a `void *` context receives a named function and a pointer to the state.
- `extern fn puts(s: str)` is refused. A `str` is two words, the pointer and the length, and C takes a `?*c_char`. A slice, a tuple, an optional value and an `own` parameter are refused there as well, and a `char` passes as a 32-bit integer.

## The ABI of a call

### What you know

A C compiler passes the first arguments in registers and the rest on the stack, by the convention of the platform. A C++ compiler mangles the name of a function with its parameter types, and `extern "C"` turns that off. Java has no native calling convention a program can see.

### The feature

`export` gives an item of a module its place in C. It stands before `fn`, `struct`, `union`, `enum`, `variant`, `class`, `const` and `type`, and it implies `pub`.

| Item | What C sees |
|---|---|
| `export fn dot(a: Vec2, b: Vec2) -> c_int` | the symbol `dot`, with no module in its name, and a prototype in the header |
| `export struct Vec2 { x: c_int, y: c_int }` | a `typedef struct Vec2` with the same fields |
| `export union Bits { whole: u32, parts: [4]u8 }` | a `typedef union Bits` |
| `export enum Color { Red, Green }` | an `enum Color` with the values `Color_Red` and `Color_Green` |
| `export variant Shape { Circle { r: f32 }, Empty }` | an `enum Shape_tag` and a `typedef struct Shape` of the tag and a union of the cases |
| `export class Counter { ... }` | a `struct Counter` behind its table pointer, the struct of its table, its descriptor and the helpers `anti_Counter_init`, `anti_Counter_delete` and the others of [chapter 9]({{% relref "09-classes" %}}) |
| `export const LIMIT: c_int = 8;` | a `#define`, or a `static const` for a `str` |
| `export type IntList = List<int>;` | one copy of a generic struct or class, under the name `IntList` |

A function without `export` has the symbol `module.name`, with the dot, which no C program can write. Two modules of one program may not export the same name, and an exported name is none that `<stdbool.h>`, `<stddef.h>` or `<stdint.h>` defines, as `SIZE_MAX`, since the header includes the three.

The signature of an `export fn` and the fields of an `export struct` hold types that C can write.

| Allowed | Refused |
|---|---|
| the sized integers, `int`, `uint`, `float`, `f32`, `f64`, `bool` | `char` |
| the C types | `str` and every slice |
| a pointer to an allowed type, and `?*byte` for `void *` | an array by value as a parameter |
| an `export struct` or `export union`, by value or by pointer | a struct that is not exported |
| a function pointer of an allowed signature | a closure |
| a fixed array as a field of a struct | a generic function |

A `str` or a slice crosses as two parameters, the pointer and the length, which the function puts together.

An `export fn` may be `may fail`. C then sees the form every failing function has. The result is a pointer to the error, null on success. The value goes through a pointer that is the last parameter. The header writes it as `struct anti_Error *checked(int32_t n, int32_t *out)`.

A pointer parameter that is written `*T` promises that it is not null, and the header marks it `/* non-null */`. A pointer that C may leave null is written `?*T`, and the function tests it.

```anti
//! name: geometry
import anti.lang;

extern fn qsort(base: ?*byte, count: c_size_t, size: c_size_t, order: fn(?*byte, ?*byte) -> c_int);

export struct Vec2
{
	x: c_int,
	y: c_int,
}

export struct Box
{
	low: Vec2,
	high: Vec2,
	depth: f64,
}

export const LIMIT: c_int = 8;

export fn dot(a: Vec2, b: Vec2) -> c_int
{
	return a.x * b.x + a.y * b.y;
}

export fn width(b: Box) -> c_int
{
	return b.high.x - b.low.x;
}

export fn parse(text: ?*byte, out: ?*c_int) -> c_int
{
	let p = text else {
		return -1;
	};
	let to = out else {
		return -1;
	};
	*to = (p[0] - 48) as c_int;
	return 0;
}

export fn checked(n: c_int) -> c_int may fail
{
	if n < 0 {
		fail "negative";
	}
	return n * 2;
}

fn by_value(a: ?*byte, b: ?*byte) -> c_int
{
	let x = a as ?*i32 catch fatal;
	let y = b as ?*i32 catch fatal;
	return *x - *y;
}

export fn sort4(numbers: ?*i32)
{
	qsort(numbers as ?*byte, 4, 4, by_value);
}
```

The checker refuses the following.

```anti refused
//! error: the parameter `name` of export fn `greet` has type `str`, which C cannot represent
export fn greet(name: str) -> c_int
{
	return 0;
}
```

```anti refused
//! error: the parameter `a` of export fn `first` has type `[]int`, which C cannot represent
export fn first(a: []int) -> int
{
	return a[0];
}
```

```anti refused
//! error: the parameter `a` of export fn `total` has type `[4]int`, which C cannot represent
export fn total(a: [4]int) -> int
{
	return a[0];
}
```

```anti refused
//! error: the parameter `c` of export fn `code` has type `char`, which C cannot represent
export fn code(c: char) -> int
{
	return 0;
}
```

```anti refused
//! error: `operator fn eq` is not exported, since C has no operators
export struct V
{
	x: c_int,
}

export operator fn eq(a: V, b: V) -> bool
{
	return a.x == b.x;
}
```

```anti refused
//! error: the parameter `p` of export fn `px` has type `P`, and `P` is not exported
struct P
{
	x: int,
}

export fn px(p: P) -> int
{
	return p.x;
}
```

### How antic builds it

An `export fn` is the same function as any other, under another name and with another linkage. The convention of [chapter 5]({{% relref "05-functions" %}}) is the C convention, so a C caller and an Anti caller put the arguments in the same places.

```llvm excerpt=geometry target=linux-x86_64 mode=release
define dso_local noundef i32 @dot(i64 %p0.0, i64 %p1.0)
...
define dso_local noundef i32 @width(ptr nofree readonly byval([24 x i8]) align 8 captures(none) %p0)
...
define dso_local noundef ptr @checked(i32 noundef %p0, ptr nofree noundef nonnull writeonly captures(none) dereferenceable(4) %p1)
...
define internal noundef i32 @geometry.by_value(ptr
```

- `dso_local` and the plain name `dot` are the export. `by_value` has no `export`, so it is `internal` and carries its module in its name.
- A `Vec2` is 8 bytes, and System V passes a struct of up to 16 bytes in registers. Each `Vec2` arrives as one `i64`.
- A `Box` is 24 bytes and passes in memory. `byval([24 x i8])` says that the caller copied it onto the stack.
- `checked` returns a pointer, the error, and takes a second parameter, the address the `c_int` result goes to.

The machine code of `dot` is what a C compiler writes for the same function.

```asm excerpt=geometry target=linux-x86_64 mode=release
<dot>:
movq %rsi, %rax
movl %edi, %ecx
shrq $0x20, %rdi
movq %rsi, %rdx
shrq $0x20, %rdx
imull %ecx, %eax
imull %edi, %edx
addl %edx, %eax
retq
```

- `a` arrives in `rdi` and `b` in `rsi`, each with `x` in its low half and `y` in its high half.
- The two shifts by 32 bring each `y` down, and the two multiplies and the add are the product.
- The result leaves in `eax`. The function has no prologue, no check and no call.

On arm64 the two structs arrive in `x0` and `x1` the same way. The Microsoft convention passes a struct of 8 bytes in one register as well, and a `Box` by a pointer to a copy.

A call from C needs no exported name. `sort4` hands `qsort` the address of `by_value`, and `qsort` calls it for each pair of elements. The machine code of both:

```asm excerpt=geometry target=linux-x86_64 mode=release
<geometry.by_value>:
pushq %rbp
movq %rsp, %rbp
testq %rdi, %rdi
je
testq %rsi, %rsi
je
movl (%rdi), %eax
subl (%rsi), %eax
popq %rbp
retq
...
<sort4>:
leaq (%rip), %rcx
movl $0x4, %esi
movl $0x4, %edx
jmp
```

- C passes the two pointers in `rdi` and `rsi`, the first two integer registers, and `by_value` reads them there. Anti and C agree on the registers, so `qsort` needs no stub.
- The two tests against zero are the `catch fatal` of the two conversions. A null pointer from C jumps to the end, which calls `NoneDereference.new` and then `Error.fatal`, and the program stops.
- The two loads and the `subl` are `*x - *y`, and the result leaves in `eax`, where `qsort` reads it.
- `pushq %rbp` and `popq %rbp` keep the frame pointer, which the stack traces of [chapter 6]({{% relref "06-errors" %}}) walk.
- `sort4` loads the address of `by_value` into `rcx`, the fourth argument, sets the count and the size to 4, and jumps to `qsort`.

The C program below calls the library that "Libraries for C" builds from the module `geometry`.

```c
#include <stdio.h>
#include "geometry.h"

int main(void)
{
    Vec2 a = {3, 4};
    Vec2 b = {2, 5};
    int32_t doubled = 0;
    int32_t numbers[4] = {4, 2, 3, 1};
    struct anti_Error *failed;

    printf("dot %d\n", dot(a, b));
    failed = checked(21, &doubled);
    printf("checked(21): %s, out %d\n", failed ? "error" : "NULL", doubled);
    failed = checked(-1, &doubled);
    printf("checked(-1): %s\n", failed ? "error" : "NULL");
    sort4(numbers);
    printf("sorted %d %d %d %d\n", numbers[0], numbers[1], numbers[2],
           numbers[3]);
    return 0;
}
```

It prints this, built on macos-arm64 with the pinned clang against `libgeometry.a`:

```text
dot 26
checked(21): NULL, out 42
checked(-1): error
sorted 1 2 3 4
```

Where it lives: `src/antic/sema_export.c` and `src/antic/sema_declare.c` hold the signature rule. `llvm_emit_linkage` of `src/antic/llvm_emit_call.c` writes `dso_local` or `internal`, `target_c_symbol` of `src/antic/target.c` the C name, and `src/antic/abi.c` the places of the arguments. `src/antic/header.c` writes the prototypes.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A call from C into an `export fn` | one `call` | one `call` |
| Conversion of the arguments | none | none |
| `size_of(Vec2)`, `size_of(Box)` | 8, 24, the sizes C computes | the same |
| Heap allocations | none | none |
| Calls into the runtime | none for the call itself | the same |
| A `may fail` function that fails | one error object on the heap, as [chapter 6]({{% relref "06-errors" %}}) counts | the same |
| Checks of dev mode inside the function | the overflow checks of `a.x * b.x + a.y * b.y`, three in `dot` | none |
| Atomic operations and locks | none | none |
| Copies of code | one function per `export fn`. A release build may inline it into Anti callers and keeps the symbol |

A struct of up to 16 bytes crosses in registers, and a larger one is copied to the stack by the caller. A C interface that passes large structs passes them by pointer, `*Box`, which costs one register. A `str` or a slice crosses as a pointer and a length, which the function puts back together. A `may fail` function adds one out pointer and a test of the result in C. It costs an object on the heap only on the path that fails.

A check of dev mode that fails inside an exported function ends the process of the C program. `--no-checks` drops the checks from a dev build.

### Pitfalls

For a Java programmer:

- A failing `export fn` returns its error as a pointer. C has to test the result of every call, and nothing unwinds.
- The error is an object on the heap. A library that lets C end it exports a function that takes it back.

For a C programmer:

- `int` in a signature is `int64_t` in the header. `c_int` is the type for an `int` of C.
- A `*T` parameter is not tested for null. C that passes null to one reads or writes address 0 in a release build.
- A `bool` is one byte, as `bool` of `<stdbool.h>`.

For a C++ programmer:

- No name is mangled by its types, so two exported functions cannot share a name.
- A struct passes by value as its bytes. No constructor and no destructor of C++ runs at the boundary.
- The header is `extern "C"` under C++ and writes `alignas` for an aligned struct.

## anti bind

### What you know

A Java binding to C is generated by `jextract` or written by hand as JNI. C++ includes the C header as it is. Other languages parse the header with a library of clang and write declarations in their own syntax.

### The feature

`anti bind` writes a binding module, a file of Anti source that declares what a C header declares. It runs once, and the module is committed as source.

| Form | Input |
|---|---|
| `anti bind --clang <header>` | a C header, read by clang |
| `anti bind <api.json>` | a description in the JSON format of raylib's parser |
| `anti bind --header <file.antl>` | a library file of Anti. It writes the C header, the other direction |

| Option | Effect |
|---|---|
| `-o <dir>` | the directory of the files it writes |
| `--module <path>` | the module path of the binding. The default is `anti.<name of the header>` |
| `--probe` | write the ABI probe as well, one program in C and one in Anti |
| `--target <t>` | read the header for that target. The host is the default |
| `-I <dir>`, `-D <name>[=<value>]` | pass an include directory and a macro to clang |

`--clang` runs a clang of the user's machine, the first `clang` on `PATH`. No package ships one. `anti bind` accepts the major version of the clang that built Anti and names it in its message otherwise.

A header maps as follows.

| C | Anti |
|---|---|
| a function | `pub extern fn`, every pointer `?*T` |
| a variadic function | `pub extern fn` with `...` |
| a `static inline` function | `pub extern fn`, and a C function of that name in the shim |
| a struct, a union | `pub struct`, `pub union` with the same fields |
| an enum | `pub enum` with the same values |
| a `#define` of a constant expression | `pub const`, typed as C types the expression |
| a `typedef` | the type it names. Anti has no alias in a binding |
| a function pointer type | `?fn(...)` of the same signature |
| `const`, `volatile`, `restrict` | dropped |
| `int64_t`, `uint8_t` and the other names of `<stdint.h>` | `i64`, `u8` and the like, on every host |

A macro with parameters and a macro whose text is no constant expression are not bound. A macro without a value is passed over with a warning.

A name that is a word of Anti takes a trailing `_` in the binding, so `struct type` becomes `pub struct type_`. A function of such a name is left out with a warning, since its name is its symbol, and so is a macro. A header that declares a name that is no C identifier, as `a$b`, is refused.

The header below is the input of the example.

```c
#ifndef SHAPES_H
#define SHAPES_H

#include <stdint.h>
#include <stddef.h>

#define SHAPES_VERSION 3
#define SHAPES_NAME "shapes"

typedef enum ShapeKind { SHAPE_CIRCLE = 1, SHAPE_BOX = 2 } ShapeKind;

/* A point of the plane. */
typedef struct Point {
    float x;
    float y;
} Point;

typedef struct Shape {
    ShapeKind kind;
    Point at;
    double size;
    const char *label;
    uint8_t rgba[4];
} Shape;

typedef int (*ShapeVisitor)(const Shape *shape, void *context);

/* The area of a shape. */
double shape_area(const Shape *shape);
Point shape_center(Shape shape);
int shapes_visit(const Shape *shapes, size_t count, ShapeVisitor visit, void *context);
void shapes_log(const char *format, ...);
static inline float point_length_squared(Point p) { return p.x * p.x + p.y * p.y; }

#endif
```

```sh
anti bind --clang shapes.h --probe -o out
```

```text
anti: shapes.h: warning: the macro `SHAPES_H` has no value and is skipped
bind: anti.shapes from shapes.h, 5 functions and a shim, 1 warning
```

The command writes four files into `out`: `shapes.anti`, `shim_shapes.c`, `probe_shapes.c` and `probe_shapes.anti`. This is the body of `shapes.anti`.

```anti
pub const SHAPES_VERSION: c_int = 3;

pub const SHAPES_NAME: str = "shapes";

pub enum ShapeKind
{
	SHAPE_CIRCLE = 1,
	SHAPE_BOX = 2,
}

pub struct Point
{
	x: c_float,
	y: c_float,
}

pub struct Shape
{
	kind: ShapeKind,
	at: Point,
	size: c_double,
	label: ?*byte,
	rgba: [4]u8,
}

pub extern fn shape_area(shape: ?*Shape) -> c_double;

pub extern fn shape_center(shape: Shape) -> Point;

pub extern fn shapes_visit(shapes: ?*Shape, count: c_size_t, visit: ?fn(?*Shape, ?*byte) -> c_int, context: ?*byte) -> c_int;

pub extern fn shapes_log(format: ?*byte, ...);

pub extern fn point_length_squared(p: Point) -> c_float;
```

`--clang` reads the header against the sysroot of the target in the package, so a tree without a package passes `--runtime` with the runtime archive. The typedef `ShapeVisitor` is gone, and its function type stands where the header used the name. The comments of the header are not carried over.

An inline function has no symbol in any library, so a call of it could not link. The shim gives it one. `shim_shapes.c` is a C file that the user compiles with the library:

```c
#define point_length_squared anti_inline_point_length_squared
#include "shapes.h"
#undef point_length_squared

float point_length_squared(Point a0)
{
    return anti_inline_point_length_squared(a0);
}
```

The probe checks that the binding and the header agree on the layout. `probe_shapes.c` prints the size and the alignment of every struct, and the bytes of an object that is cleared and given one field. `probe_shapes.anti` prints the same from the binding. Two outputs that are equal prove that every field has its offset and its size on that target.

### How antic builds it

`anti bind` is part of the `anti` tool and no part of the compiler. It runs clang twice with the same options. `-E -dD` gives the macros and the `#pragma pack` lines, and `-fsyntax-only -Xclang -ast-dump=json` gives the declarations as JSON. The options aim clang at the target and at the headers of its sysroot in the package. A binding therefore reads the same headers on every host.

Both input forms fill one model of records, enums, functions and constants. The model holds C types by their spelling and no size. One writer turns it into the module, the shim and the probes. A JSON description and a header of one API therefore give the same binding.

The binding module is compiled as every module is. A call of a bound function is the call of an `extern fn` of the first section:

```anti
//! name: bound
pub struct Point
{
	x: c_float,
	y: c_float,
}

pub extern fn point_length_squared(p: Point) -> c_float;

export fn far(x: f32, y: f32) -> bool
{
	return point_length_squared(Point { x: x, y: y }) > 100.0;
}
```

```llvm excerpt=bound target=linux-x86_64 mode=release
define dso_local noundef zeroext range(i8 0, 2) i8 @far(float noundef %p0, float noundef %p1)
...
  %v8 = tail call float @point_length_squared(<2 x float>
...
declare noundef float @point_length_squared(<2 x float>)
```

- A `Point` of two `float` fields is 8 bytes of class SSE, so System V passes it in one vector register. antic writes that as `<2 x float>`.
- The call goes to the function of the shim. The inline body of the header stays in C, behind one call.

Where it lives: `src/anti/bind.c` holds the command, `bindclang.c` reads a header, `bindapi.c` a description, `bindtype.c` parses a C type, `bindexpr.c` evaluates a constant, and `bindwrite.c` writes the module, the shim and the probes.

### What it costs

A binding is declarations. It adds no code to a program, in dev or in release.

| Cost | Count |
|---|---|
| A bound function | one `declare` in each module that calls it, and one `call` per call |
| A bound inline function | one more call, into the shim, since its body is C |
| A bound struct | its C layout. `size_of` in Anti equals `sizeof` in C, which the probe prints |
| A bound constant | a constant of the module, folded where it is used |
| Heap allocations, calls into the runtime, atomic operations, locks, checks | none |
| Running `anti bind` | two runs of clang. It is a step of development and no step of a build |

`anti bind --clang` reads the header as clang reads it, `#if` blocks and typedefs included. `anti bind <api.json>` takes a description that a project such as raylib publishes, and needs no clang. Declarations written by hand are the choice for a few functions, and they take no probe.

### Pitfalls

For a Java programmer:

- The binding is source and not a jar. It is committed, and a new header version means a new run of `anti bind`.
- A bound function gives pointers that may be `none`. The binding cannot know which C function never returns null.

For a C programmer:

- A macro with parameters is not bound. A function in the shim, or Anti code, takes its place.
- The shim is C. A project that binds an inline function compiles `shim_<name>.c` with a C compiler and links the object.
- `const char *` becomes `?*byte`. The binding does not say whether C keeps the pointer.

For a C++ programmer:

- `anti bind` reads C headers. A C++ header with classes or templates is not bound.
- An anonymous struct member of C11 becomes a field named `anon0`, with a warning.
- The module is written once per target when the header has `#if` on the platform. `--target` picks the target.

## link

### What you know

A C or C++ build names its libraries on the link line, `-lX11` or `-framework CoreAudio`. A Java program names a native library at run time with `System.loadLibrary`. The source of a C program does not say what it links, apart from `#pragma comment(lib)` of one compiler.

### The feature

A `link` line at module level names a library of the system that the program links against. It stands in the module that declares the functions of that library, most often a binding.

```anti
link framework "CoreAudio";
link linux "X11";
```

| Form | Effect |
|---|---|
| `link framework "Name";` | a macOS program links the framework `Name` of Apple's SDK. Other targets ignore the line |
| `link linux "Name";` | a Linux program links `libName.so` of the glibc sysroot. Other targets ignore the line |

A Windows program names no library this way. It links the import libraries of the package.

`anti build` and `anti test` read the lines of every module the program reaches, library files included, and pass the names to the link. A program that imports a binding links what the binding needs without naming it. `antic` alone reads no `link` line. It takes the names as `--framework <name>` and `--linux-lib <name>`.

A `link linux` line changes how a Linux program is linked. A Linux program without one is static: it holds musl, the allocator and the runtime, and it needs no file of the system. A program that reaches a `link linux` line links dynamically against glibc 2.35, since a shared library of the system needs the C library of the system.

```anti
//! name: display
link linux "X11";

extern fn XOpenDisplay(name: ?*byte) -> ?*byte;

export fn has_display() -> bool
{
	return XOpenDisplay(none) != none;
}
```

The checker refuses the following.

```anti refused
//! error: `link linux` takes the name of a library as a string
link linux X11;
```

```anti refused
//! error: the framework `CoreAudio` is linked twice
link framework "CoreAudio";
link framework "CoreAudio";
```

```anti refused
//! error: expected an item
link windows "user32";
```

A name with a path separator or a space is refused as well, and so is an empty name.

### How antic builds it

A `link` line emits no code. The call of a function of the library is the call of an `extern fn`:

```llvm excerpt=display target=linux-x86_64 mode=release
define dso_local noundef zeroext range(i8 0, 2) i8 @has_display()
b0:
  %v0 = tail call ptr @XOpenDisplay(ptr null)
...
declare noundef ptr @XOpenDisplay(ptr noundef)
```

- The call passes `none` as a null pointer and receives a pointer.
- The module holds a `declare` of the function and no name of its library. The library file of the module records the names of its `link` lines beside its imports.

The names reach the linker. On Linux, lld links the program as a position independent executable against the glibc sysroot of the package. It names the dynamic linker, the libraries of the `link linux` lines, libm and libc. It takes the runtime that was built against the headers of glibc 2.35. On macOS, lld receives `-framework Name` and reads the framework from the SDK.

The table holds the two link modes of Linux, built on macOS for linux-x86_64 by antic of `c2806b61`. The first program has a `main` that calls `XOpenDisplay(none)` and prints `a display` with `io.println` when the result is not `none`. The second prints `a line` and calls nothing else.

| Program | Bytes | Needed libraries | Interpreter |
|---|---:|---|---|
| with `--linux-lib X11` | 29,112 | `libX11.so.6`, `libc.so.6`, `libm.so.6` | `/lib64/ld-linux-x86-64.so.2` |
| without | 228,800 | none | none |

The second column is the output of `llvm-readobj --needed-libs` of the pinned tools.

Where it lives: `parser_decl.c` and `sema_declare.c` of `src/antic/` read and check the line, `src/anti/units.c` collects the names of every module for the link, and `src/antic/driver_link.c` and `src/antic/linker.c` write the link of both modes.

### What it costs

| Cost | Count |
|---|---|
| A `link` line | no code and no data in the program |
| A call into the library | one `call`, which the dynamic linker binds to the library |
| A Linux program that reaches `link linux` | dynamic against glibc 2.35 or newer. It starts through the dynamic linker of the system |
| A Linux program without one | static, with musl and the allocator inside. About 200,000 bytes more in the example |
| Heap allocations, calls into the runtime, atomic operations, locks, checks | none added by the line |

A dynamic Linux program runs on a system whose glibc is 2.35 or newer and that has the named libraries installed. A static one runs on any Linux kernel of its processor.

### Pitfalls

For a Java programmer:

- The library is bound when the program is linked and loaded when it starts. A missing `libX11.so.6` stops the program before `main`.
- No search path is set by the line. The dynamic linker of the system finds the library.

For a C programmer:

- `antic main.anti` ignores a `link` line of `main.anti`. `anti build` passes the names, and so does `--linux-lib` by hand.
- One `link linux` line anywhere in the program changes the C library of the whole program from musl to glibc.
- The sysroot of the package holds the X11 and OpenGL libraries. A `link linux` of another library needs that library in the sysroot at link time.

For a C++ programmer:

- The line names a library and no version. The sysroot decides the version the link reads.
- `link framework` on Linux and `link linux` on macOS are ignored without a message, so one binding serves every target.

## Libraries for C

### What you know

A C library is an archive of objects or a shared object, and a header that declares it. A C++ library offers `extern "C"` functions when C is to call it. A Java library reaches C through JNI, with a virtual machine in the process.

### The feature

`antic --lib static` and `antic --lib shared` compile a module and what it imports into a library for C and write its header. `anti build --lib static|shared` does the same for a project. A project without `main` is a library project.

| Option | Effect |
|---|---|
| `--lib static` | an archive: `lib<name>.a`, and `<name>.lib` on Windows |
| `--lib shared` | a shared library: `lib<name>.so`, `lib<name>.dylib`, or `<name>.dll` with its import library |
| `--bundle-runtime` | with `--lib static`: put the runtime into the archive |
| `--soname` | with `--lib shared`: give the library the major version of the package. It needs `--package-version` |
| `--no-runtime` | with `--lib shared`: a plugin, which the last section describes |

Both forms write `<name>.h` beside the library. The header holds every `export` item of the module: the structs and unions, the constants, and a prototype per function. A `///` comment of an exported item becomes a `/** */` comment above it. The header is written again by every build.

The form of the library decides how the runtime reaches the C program.

| Form | Where the runtime is | What the C program links |
|---|---|---|
| `--lib static` | outside the archive | the archive, `libanti_rt.a` of the package, `-lpthread -lm` |
| `--lib static --bundle-runtime` | inside the archive | the archive, `-lpthread -lm` |
| `--lib shared` | inside the shared library, every symbol of it hidden | the shared library |

antic prints the link line of a static library when it has written it. On macOS the line lacks `-lpthread -lm`, which `libSystem` holds. Two static libraries of Anti in one C program share the one `libanti_rt.a`. Two bundled libraries in one program fail to link, with the duplicate symbol `anti_rt_bundle`.

A C program calls an exported function without a call that starts anything. A static or a shared library starts its runtime from a constructor, an entry of `.init_array` in its object. The constructor runs when the image that holds it loads. For a static library that is the start of the C program, before its `main`. No exported function checks for a start.

Memory crosses as between two C libraries. A library takes its memory from the `malloc` of the process it runs in: `libgeometry.so` imports `malloc` and `free` as open names. C may therefore `free` what an exported function returned. The comment of each function says who frees what.

An exported class crosses with its layout, its tables and the helpers `anti_<Class>_init` and `anti_<Class>_construct`, which [chapter 9]({{% relref "09-classes" %}}) describes.

The session below builds the module `geometry` of "The ABI of a call" in the three forms for linux-x86_64.

```sh
antic --target linux-x86_64 --lib static geometry.anti
antic --target linux-x86_64 --lib static --bundle-runtime geometry.anti
antic --target linux-x86_64 --lib shared geometry.anti
```

The first two commands print the link line:

```text
cc main.c libgeometry.a <package>/lib/linux-x86_64/v3/libanti_rt.a -lpthread -lm
cc main.c libgeometry.a -lpthread -lm
```

For a Windows target the line names the libraries alone, as `geometry.lib` and the `anti_rt.lib` of the target. A C compiler of Windows takes them in its own syntax.

| File | Bytes | Contents |
|---|---:|---|
| `libgeometry.a`, static | 51,904 | `geometry.o` and `geometry.package.o` |
| `libgeometry.a`, bundled | 381,062 | `geometry.bundled.o` and `geometry.package.o` |
| `libgeometry.so` | 86,816 | the module and the runtime in one image |

The sizes are those of antic of `c2806b61` for linux-x86_64. The header of all three is the same, apart from its first comment. In the bundled form that comment adds that the archive holds the runtime and that one such archive goes into a program:

```c
typedef struct Vec2 {
    int32_t x;
    int32_t y;
} Vec2;

typedef struct Box {
    Vec2 low;
    Vec2 high;
    double depth;
} Box;

#define LIMIT ((int32_t)8)

int32_t dot(Vec2 a, Vec2 b);
int32_t width(Box b);
int32_t parse(uint8_t *text, int32_t *out);
/* May fail: NULL on success, an error otherwise. */
struct anti_Error *checked(int32_t n, int32_t * /* non-null */ out);
void sort4(int32_t *numbers);
```

The lines above stand between an include guard, the includes of `<stdbool.h>`, `<stddef.h>` and `<stdint.h>`, and an `extern "C"` block for C++.

antic refuses a library form it cannot write, with one message each.

| Command line | Message |
|---|---|
| `--lib shared --soname` | `antic: --soname needs --package-version` |
| `--lib static --soname` | `antic: --soname gives a library of --lib shared its major version, and a library of --lib static has none` |
| `--lib shared --bundle-runtime` | `antic: --bundle-runtime puts the runtime into a library of --lib static, and a library of --lib shared links it` |
| `--lib static --no-runtime` | `antic: --no-runtime builds a plugin, which is a library of --lib shared` |
| `--lib static --dev` | `antic: --lib static builds in release mode, without --dev` |

A module that exports nothing gets the warning `nothing-exported` from `--lib static`, which the release build of a library refuses as it refuses every warning: `the module exports nothing, and --lib static writes a library for C`. `allow(nothing-exported, "reason")` at the top of the file silences it.

The name of a shared library follows its platform.

- On Linux, `--soname --package-version 1.2.3` writes `libgeometry.so.1` with the soname `libgeometry.so.1`, and the link `libgeometry.so` beside it.
- On macOS, every library has the install name `@rpath/libgeometry.dylib`, so a program finds it through its rpath. `--soname` adds the current version 1.2.3 and the compatibility version 1.0.0.
- On Windows, lld-link runs with `/Brepro`, so two links of one input give equal bytes. Two builds of `geometry.dll` and `geometry.lib` for windows-x86_64 compare equal.

### How antic builds it

A library is compiled as a program is: the module and everything it imports, in one whole-program compile. The exported functions take the place of `main` as the roots that keep code alive.

```llvm excerpt=geometry target=linux-x86_64 mode=release
define dso_local noundef i32 @dot(i64 %p0.0, i64 %p1.0)
...
define internal noundef i32 @geometry.by_value(ptr
...
define dso_local void @sort4(ptr noundef %p0)
b0:
  tail call void @qsort(ptr %p0, i64 4, i64 4, ptr nonnull @geometry.by_value)
```

- The exported functions are `dso_local` definitions under their C names.
- `by_value` is `internal`. It stays in the library because `sort4` passes its address to `qsort`, and no symbol of it leaves the object.

A static library is an archive that `llvm-ar` of the package writes. Its second member, `geometry.package.o`, holds the package header of the library, which `anti license --from-archive` reads. `--bundle-runtime` links the object of the library and the objects of the runtime into one relocatable object, on Linux and Windows. On macOS the archive holds the library object, the runtime members and one marker member.

A shared library is linked by lld with the runtime inside. Only the exported names are visible. The dynamic symbol table of `libgeometry.so` defines seven names: `dot`, `width`, `parse`, `checked`, `sort4`, `anti_rt_backtrace_default` and `anti_licenses`. The first five are the exported functions of the module. The other 43 dynamic symbols are functions of the C library that the image imports, such as `malloc`, `qsort` and `pthread_mutex_lock`. The image holds an `.init_array` section, whose entry starts the runtime when the library loads.

The header writes a C type for each Anti type.

| Anti | Header |
|---|---|
| `i8` to `u64`, and the C types that are the same types | `int8_t` to `uint64_t` |
| `int`, `uint`, `float` | `int64_t`, `uint64_t`, `double` |
| `c_long`, `c_ulong`, `c_wchar` | `long`, `unsigned long`, `wchar_t` |
| `*byte`, `?*byte` | `uint8_t *` |
| `*T` | `T * /* non-null */` |
| a `packed` struct | `#pragma pack(push, 1)` and `#pragma pack(pop)` around it |
| `align(N)` of a struct, a union or a variant that is not packed | `ANTI_ALIGNAS(N)` on its first field, `_Alignas` in C and `alignas` in C++ |
| `align(N)` of a packed type and of a class | `ANTI_ALIGNED(N)` on the type, `__attribute__((aligned(n)))` or `__declspec(align(n))` of MSVC, which no `#pragma pack` caps |

Where it lives: `src/antic/header.c` writes the header, `src/antic/driver_library.c` the archive and its bundle, and `src/antic/linker.c` the link of a shared library. `src/antic/llvm_emit.c` writes the constructor of a library, and `anti_rt_init` of `src/rt/init.c` is the start it runs.

### What it costs

| Cost | Count |
|---|---|
| A call from C | one `call`, as "The ABI of a call" shows |
| The static archive of the example | 51,904 bytes, and the C program links `libanti_rt.a` once |
| The bundled archive | 381,062 bytes, with the runtime of the level `v3` inside |
| The shared library | 86,816 bytes, with the runtime inside |
| Start of the runtime | once per process for a static library, before `main`. Once per shared library for a shared one, when it loads |
| Threads | none until the first `parallel` or `dispatch` of the library, which starts the pool, as [chapter 13]({{% relref "13-threads" %}}) describes |
| Checks | by the mode of the build, as for a program. A library of dev mode carries the checks into the C program |

The sizes are those of release builds for linux-x86_64.

The three forms answer three needs.

- `--lib static` suits a C program that links one or more libraries of Anti. They share one runtime, and the C program names `libanti_rt.a` on its link line.
- `--lib static --bundle-runtime` suits a C program that links one library of Anti and wants one archive to name. It costs the whole runtime inside the archive, 329,158 bytes more in the example, and a second bundled archive does not link.
- `--lib shared` suits a library loaded by programs that are not built with it, or by another language. Each shared library carries a runtime of its own, its threads and its hash seed.

### Pitfalls

For a Java programmer:

- A library for C needs no virtual machine and no start call. The process holds one runtime per shared library of Anti.
- An error that leaves an exported function is a pointer C must not ignore. Anti cannot make C handle it.

For a C programmer:

- A `c_char` parameter is `int8_t` in the header, and `*byte` is `uint8_t *`. A `char *` of C passes with a cast.
- A static library starts its runtime before `main` of the C program, from a constructor of its object. No exported function pays for a start.
- Two bundled archives in one program do not link. One of them is built without `--bundle-runtime`.
- A failure Anti cannot report calls `abort()`. The first lines of the header say so.

For a C++ programmer:

- The header is C. A class of Anti is a struct with a table pointer and free functions, and no C++ class.
- Two shared libraries of Anti in one process hold two runtimes, two pools of threads and two hash seeds.
- A static library carries every symbol of its objects. The names of functions that are not exported hold a dot and cannot collide with C names.

## Plugins across the boundary

### What you know

A plugin in C is a shared object opened with `dlopen`, and a table of function pointers found with `dlsym`. C++ plugins share classes with the host only when both were built by the same compiler with the same headers. Java loads a jar through a class loader and checks the classes as it links them.

### The feature

A plugin is the third kind of shared library antic writes. It is for an Anti program and not for C. [Chapter 15]({{% relref "15-hooks" %}}) holds how a module provides an interface with `provides Interface as Class;`, how a host loads the library with `plugin.load` and what the load checks. This section holds what crosses the boundary between the two binaries.

| Shared output | Built with | Holds a runtime | Visible symbols | Loaded by |
|---|---|---|---|---|
| A library for C | `--lib shared` | its own, hidden | the `export` items | the dynamic linker, for a C program |
| A plugin | `--lib shared --no-runtime` | none | `anti_rt_provides`, `anti_rt_plugin_modules` and `anti_licenses` | `plugin.load` of an Anti program |
| A host program | no option. `--closed` turns it off | its own, visible | its functions and data | the system |

A plugin holds the code of its own module and nothing else. Every function of the runtime, the root class and the descriptor of each interface are names it leaves open. The loader of the system binds them to the host when the plugin loads. A plugin and its host therefore share one runtime, one allocator, one pool of threads and one set of class descriptors.

What crosses is objects and not C functions. The host calls the plugin through the table of an object the plugin made, and the plugin calls the host through the open names. Both sides were compiled by antic, and both use the C convention, so a call across is an ordinary dispatch.

A plugin may hold `extern fn` declarations and `link` lines of its own. It takes the link facts of a program of its module: the frameworks, the libraries of `link linux` and the glibc mode.

A program that calls `plugin.load` or injects an interface is a host, and it is built to be bound against. On Linux it links dynamically against glibc, since loading a library needs the dynamic linker. `--closed` builds a program that no plugin can bind against, and on Linux that program is static again.

```anti
//! name: plugs
import anti.io;

pub abstract class Greeter
{
	pub abstract fn greet(self, name: str);
}

class Fancy inherits Greeter
{
	concrete fn greet(self, name: str)
	{
		io.println(name);
	}
}

provides Greeter as Fancy;
```

In a real plugin the interface stands in a module of its own that the host imports as well. The block holds both so that it compiles alone.

The checker refuses a `provides` line whose first name is no interface.

```anti refused
//! error: `Plain` is not abstract, and a `provides` line names an interface
class Plain
{
	pub n: int = 0,
}

class Fancy inherits Plain
{
}

provides Plain as Fancy;
```

### How antic builds it

The object of a plugin is the object of one module. The function a host reaches and the table it reaches it through, in the module of the example:

```llvm excerpt=plugs target=linux-x86_64 mode=dev
define hidden void @plugs.Fancy.greet(ptr noundef nonnull dereferenceable(8) %p0, i64 %p1.0, i64 %p1.1)
...
@plugs.Fancy.table = hidden constant <{ ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr }> <{ ptr @plugs.Fancy.descriptor, ptr @anti_lang_Object_type_name, ptr @anti_lang_Object_to_text,
...
declare void @anti_lang_Object_created(ptr noundef nonnull dereferenceable(8))
```

- `greet` takes `self` as a pointer and the `str` as two words, a pointer and a length. That is the C convention, and the host calls it through the table with the same three registers.
- The function is `hidden`, so no symbol of it leaves the library. The host finds it through entry 17 of the table of the object.
- The table has 18 entries: the descriptor, the seven functions of the root, the nine hooks and `greet`. Its second and third entries name functions of the root class, which the plugin does not define.
- The `declare` line is one of those open names. In a program the runtime in the same link defines it. In a plugin the loader binds it to the host.

The plugin of `tests/plugin`, the class `Fancy` of `net.example.fancy`, built for linux-x86_64 with its host by antic of `c2806b61`:

| Binary | Bytes | Dynamic symbols | Needed libraries |
|---|---:|---|---|
| `libfancy.so`, the plugin | 15,448 | 3 defined, 20 left open | none |
| the host | 330,512 | 913 defined, 85 left open | `ld-linux-x86-64.so.2`, `libc.so.6`, `libm.so.6` |
| the host with `--closed` | 388,216 | none | none |

The 20 open names of the plugin are `anti_rt_descriptor`, `anti_rt_hook`, the descriptor of `Greeter`, the descriptor of the root class and its 16 functions. The three names it defines are `anti_rt_provides`, the table `plugin.load` reads, `anti_rt_plugin_modules`, the modules the plugin reaches, and `anti_licenses`.

The closed host is larger than the open one because it is static. It holds musl and the allocator, where the open host takes the C library of the system.

Where it lives: `whole_hosts_plugins` and the test before `devirtualise` in `src/antic/whole.c`, `whole_write_provides` of `src/antic/whole_tables.c`, `src/antic/driver_link.c` and `src/antic/linker.c` for the three links, and `src/rt/plugin.c` and `src/rt/loaded.c` for the load.

### What it costs

| Cost | Count |
|---|---|
| A call from the host into a plugin | one dispatch through the table of the object: two loads and an indirect call |
| A call from a plugin into the runtime | one call through the procedure linkage table of the plugin |
| Bytes of the plugin of the example | 15,448, since it holds no runtime |
| Bytes a host pays to be bound against | its dynamic symbol table, 998 names in the example |
| A Linux host | dynamic against glibc 2.35 or newer, where a closed program is static |
| Heap allocations of an instance | one, of the size the descriptor of the class gives |
| Atomic operations and locks | those of `plugin.load` and of the count of live objects, which [chapter 15]({{% relref "15-hooks" %}}) counts |
| Checks at load | the runtime version, the hash chain of each interface and the slots the host calls, once per load |
| Devirtualisation | none. A release build of a host or of a plugin skips the pass, so every call through a table stays a dispatch |

`--closed` gives a program the static link of Linux and the devirtualisation of a release build, and gives up plugins. `--no-hooks` is refused for a host and for a plugin, since the count of live objects of a library is kept in the hooks.

### Pitfalls

For a Java programmer:

- A plugin is checked once, when it loads. A library built against another version of an interface is refused then, and never at a call.
- `unload` refuses while an object of the library is alive. Nothing collects the objects.

For a C programmer:

- A plugin is no library for C. It has no runtime, and a C program that opens it with `dlopen` finds 20 names it cannot bind.
- A plugin and its host share `malloc`. Memory that one side allocates may be freed by the other.
- A Linux host is dynamic. It does not run on a system without glibc, where a closed program does.

For a C++ programmer:

- The load compares the runtime version and the hash chain of each interface. It does not compare compilers or flags.
- No call across the boundary is inlined, and no exception exists to cross it. A `may fail` function of a plugin returns its error as a pointer.
- A release host keeps every dispatch, since the whole-program pass does not devirtualise a host. `--closed` gives the pass back.
