---
title: "Structs and enums"
description: "Structs and their layout, packed and align, unions and bitfields, simd structs, enums, variants with their tag and payload, and pattern matching."
summary: "Structs with C layout, `packed` and `align`, unions and bitfields, simd structs, enums, variants as a tag and a union of their cases, and the `switch` that takes a variant apart, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T20:23:44+02:00
draft: true
weight: 80
tags: [compilers, programming-languages]
keywords: [structs, memory layout, packed, alignment, unions, bitfields, simd, enums, variants, pattern matching]
---

A struct is the bytes C declares, in the order written, on every target. `packed`, `align(N)`, unions and bitfields change the layout as they do in C. A simd struct is one vector, an enum one integer, and a variant a tag and a union of its cases. None of them has a header, a table pointer or a hidden field.

## Structs

### What you know

A `struct` of C is its fields in order, with padding where a field needs its alignment, and `=` copies its bytes. C++ adds functions, constructors and access levels to the same layout. A class of Java is always an object on the heap behind a reference, with a header before its fields. The virtual machine chooses the order of the fields. Neither C nor C++ compares two structs with `==` unless the program writes the operator.

### The feature

A struct declares fields and nothing else. Each field is a name and a type, the fields are separated by commas, and a trailing comma is allowed.

- Every field is readable and writable wherever the struct is visible. A struct has no `pub`, no `protected` and no private field.
- A field has no default. A literal names every field, in any order.
- A struct holds no function, no constant and no `inherits`. Behaviour belongs to a class of [chapter 9]({{% relref "09-classes" %}}).
- Structs are nominal. Two declarations with the same fields are two types, and neither converts to the other.
- A struct holds another struct inline. A struct that holds itself is refused, and a pointer to itself is an ordinary field, as `next: ?*Node` is.
- A struct of another module is named with its module, `geo.Rect { x: 0.0, y: 0.0, w: 1.0, h: 1.0 }`.

A struct is a value. `let copy = r;` copies every byte, an argument of struct type is a copy, and a result is a copy. A function that changes the struct of its caller takes a `*Rect`. A struct that owns something, such as one with a collection inside, follows the value rules of [chapter 11]({{% relref "11-ownership" %}}) and is not copied by `=`.

The method-call form works on a struct. `r.area()` is `area(&r)`, and `r.widen(2.0)` is `widen(r, 2.0)`, when `area` and `widen` are free functions of the module that declares `Rect`. The first parameter decides between the address and the copy. The struct does not know the functions exist.

`==` and `!=` compare two structs field by field, each field by its kind. A number compares by its value, a `str` by its bytes, a pointer by its address and an inline struct by these rules again. `x.hash()` gives a `u64` over the same fields in the same order, so two structs that are equal hash alike. A struct has no default `<`. An `operator fn` at module level replaces a default or adds an operator, as the section on operators of [chapter 9]({{% relref "09-classes" %}}) shows.

`size_of(T)` is the size of a type in bytes on the target, padding included.

`export struct` writes the struct into the C header of a library. An `export fn` names only exported types in its signature.

```anti
//! name: rects
import anti.io;
import anti.text;

export struct Rect { x: f32, y: f32, w: f32, h: f32 }

export struct Mixed { tag: u8, count: u32, flag: bool, total: i64, last: u16 }

export struct Point { x: int, y: int }

struct Line { from: Point, to: Point, name: str }

struct Node { value: int, next: ?*Node }

fn area(r: *Rect) -> f32
{
	return r.w * r.h;
}

fn widen(r: Rect, by: f32) -> Rect
{
	return Rect { x: r.x, y: r.y, w: r.w + by, h: r.h };
}

export fn total(m: *Mixed) -> i64
{
	return m.total;
}

export fn same(a: *Point, b: *Point) -> bool
{
	return *a == *b;
}

export fn key(a: *Point) -> u64
{
	return a.hash();
}

export fn put(to: *Mixed, from: *Mixed)
{
	*to = *from;
}

fn main() -> int
{
	let r = Rect { x: 0.0, y: 0.0, w: 8.0, h: 4.0 };
	let wide = r.widen(2.0);
	let copy = r;
	copy.w = 1.0;
	let a = Point { y: 4, x: 3 };
	let b = Point { x: 3, y: 4 };
	let l = Line { from: a, to: Point { x: 5, y: 6 }, name: "edge" };
	let m = Mixed { tag: 1, count: 2, flag: true, total: 3, last: 4 };
	let n = Mixed { tag: 0, count: 0, flag: false, total: 0, last: 0 };
	put(&n, &m);
	let line = f"area {r.area()} {wide.area()} {copy.area()}, same {same(&a, &b)} {a != l.to}, hash {key(&a) == key(&b)}, total {total(&n)}, to {l.to.x}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Rect {size_of(Rect)}, Mixed {size_of(Mixed)}, Point {size_of(Point)}, Line {size_of(Line)}, Node {size_of(Node)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
area 32 40 4, same true true, hash true, total 3, to 5
sizes: Rect 16, Mixed 32, Point 16, Line 48, Node 16
```

`Mixed` holds 16 bytes of fields in 32 bytes. Three bytes of padding follow `tag`, seven follow `flag` and six follow `last`. `count` therefore starts at 4 and `total` at 16, and the size is a multiple of 8.

The checker refuses each of the following.

```anti refused
//! error: a field of a struct has no default, which belongs to a class
struct Size { w: int = 1, h: int = 1 }
```

```anti refused
//! error: a struct holds fields alone, and a function belongs to a class
struct Size
{
	w: int,
	fn area(self) -> int { return self.w; }
}
```

```anti refused
//! error: the literal of `Size` misses the field `h`
struct Size { w: int, h: int }

fn square(n: int) -> Size
{
	return Size { w: n };
}
```

```anti refused
//! error: expected `Size`, found `Extent`
struct Size { w: int, h: int }

struct Extent { w: int, h: int }

fn convert(e: Extent) -> Size
{
	return e;
}
```

```anti refused
//! error: struct `Chain` contains itself
struct Chain { value: int, next: Chain }
```

```anti refused
//! error: `<` is not defined on `Size`. A struct or a class has no default order and declares `operator fn lt`
struct Size { w: int, h: int }

fn smaller(a: Size, b: Size) -> bool
{
	return a < b;
}
```

```anti refused
//! error: the parameter `s` of export fn `width` has type `*Size`, and `Size` is not exported
struct Size { w: int, h: int }

export fn width(s: *Size) -> int
{
	return s.w;
}
```

### How antic builds it

The IR of antic names a struct and its fields and holds no size. An access is the address of the struct plus `offset_of` a field, a symbol that the back end fills in for the target.

```ir excerpt=rects target=linux-x86_64 mode=release
type rects.Mixed = struct { tag: i8, count: i32, flag: i8, total: i64, last: i16 }
...
export fn rects.total(%0: ptr nonnull deref(size_of rects.Mixed)) -> i64 {
b0:
    %1 = ptradd inbounds %0, offset_of rects.Mixed.total
    %2 = load i64 %1 !type rects.Mixed.total
```

The first line is the struct as lowering leaves it: the fields with their IR types, and `bool` as an `i8`. `size_of rects.Mixed` in the signature and `offset_of rects.Mixed.total` in the body are symbols. `ptradd` adds the offset to the pointer, and `load` reads the field.

The back end lays the struct out by the C rules of the target. Each field goes to the next multiple of its alignment. The struct takes the largest alignment of its fields, and its size is rounded up to it. The LLVM module then holds numbers and no struct type.

```llvm excerpt=rects target=linux-x86_64 mode=dev
define internal noundef i64 @total(ptr noundef nonnull dereferenceable(32) %p0) #0 {
...
  %v1 = getelementptr inbounds i8, ptr %v0, i64 16
...
  %v3 = load i64, ptr %v2, align 8
```

- `dereferenceable(32)` is `size_of(Mixed)`: the `*Mixed` points at 32 readable bytes.
- `getelementptr inbounds i8, ptr %v0, i64 16` is the address of `total`, 16 bytes into the struct. The element type is `i8`, so the index counts bytes.
- `load i64 ... align 8` reads the field with the alignment of its type.

A local struct is a slot of bytes in the frame, and a literal fills it field by field.

```llvm excerpt=rects target=linux-x86_64 mode=dev
define hidden noundef i64 @rects.main() noinline #0 {
...
  %s0 = alloca [16 x i8], align 4
...
  %s5 = alloca [48 x i8], align 8
  %s6 = alloca [32 x i8], align 8
...
  store float 0x4020000000000000, ptr %v6, align 4
```

`%s0` is `r`, a `Rect`: 16 bytes aligned to 4, since an `f32` aligns to 4. `%s5` is the `Line` and `%s6` a `Mixed`, both aligned to 8. This `[N x i8]` with its alignment is the only type the LLVM module gives a struct. The `store` writes `8.0` into the field `w` of the literal.

A copy of a struct is one `memcpy` of its size, which the code generator turns into moves.

```llvm excerpt=rects target=linux-x86_64 mode=dev
define internal void @put(ptr noundef nonnull dereferenceable(32) %p0, ptr noundef nonnull dereferenceable(32) %p1) #0 {
...
  call void @llvm.memcpy.p0.p0.i64(ptr %v0, ptr %v1, i64 32, i1 false)
```

```asm excerpt=rects target=linux-x86_64 mode=release
<total>:
movq 0x10(%rdi), %rax
retq
...
<same>:
movq (%rdi), %rax
cmpq (%rsi), %rax
jne 0x15 <same+0x15>
movq 0x8(%rdi), %rcx
movb $0x1, %al
cmpq 0x8(%rsi), %rcx
jne 0x15 <same+0x15>
retq
xorl %eax, %eax
retq
...
<put>:
vmovups (%rsi), %ymm0
vmovups %ymm0, (%rdi)
```

`total` is one load at offset `0x10`. `same` is the default `==` of `Point`. It compares `x`, leaves with `false` at the first difference, and compares `y`. The compiler writes that code where the `==` stands and calls no function. `put` copies the 32 bytes of a `Mixed` through one `ymm` register, which the default level x86-64-v3 has. On arm64 the copy is one `ldp` and one `stp` of two 16-byte registers, and `total` is `ldr x0, [x0, #0x10]`.

The default hash is written in place as well. `key` runs the finalizer of MurmurHash3 over each field and again over the running value, four multiplications per field. It reads no seed and calls nothing.

A value of struct type passes as the C convention of the target passes that struct, which [chapter 5]({{% relref "05-functions" %}}) covers. On linux-x86_64 a `Rect` travels in two `xmm` registers and comes back in two.

```llvm excerpt=rects target=linux-x86_64 mode=dev
define hidden { <2 x float>, <2 x float> } @rects.widen(<2 x float> %p0.0, <2 x float> %p0.1, float noundef %p1) #0 {
```

Lowering writes a descriptor for every struct of a module. It holds the name, the size and a record per field with the name, the offset and a type id. The descriptor is 136 bytes and a field record 48. A struct never points at its descriptor. Reflection and the default `serialize` of a class with a struct field reach it, and [chapter 9]({{% relref "09-classes" %}}) describes both. The release module of `rects` holds none of the five, because nothing refers to them.

Where it lives: `compute` of `src/antic/layout.c` lays out every aggregate, and `layout_resolve` replaces `size_of` and `offset_of`. `lower_field_at`, `lower_field_offset` and `lower_literal_address` of `src/antic/lower.c`. `lower_equals` of `src/antic/lower_eq.c` and `lower_hash` of `src/antic/lower_hash.c` write the defaults. `lower_struct_descriptor` of `src/antic/lower_desc.c`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of a struct | the fields and the padding of C: `Rect` 16, `Mixed` 32, `Line` 48 | the same |
| Bytes beside the fields | 0 | 0 |
| A field read or write | one load or store at a constant offset | the same, one instruction |
| A literal | one store per field | the same, or fewer after folding |
| A copy | one `memcpy` of the size | moves through registers up to a few words, a call of `memcpy` above |
| `a == b` | one compare and one branch per field, in place | the same |
| `a.hash()` | four multiplications per field, in place | the same, eight `imulq` in `key` for the two fields of `Point` |
| Heap allocations | 0 | 0 |
| Calls into the runtime | 0 | 0 |
| Checks inserted | none for a struct, the checks of the operators on its fields remain | none |
| Read-only data | a descriptor of 136 bytes and 48 per field, per struct | only the descriptors something refers to |

The padding is part of the cost. Reordering the fields of `Mixed` as `total`, `count`, `last`, `tag`, `flag` gives 16 bytes. The compiler never reorders, because the layout is the C layout.

### Pitfalls

For a Java programmer:

- A struct is a value. `let copy = r;` makes a second struct, and a change of `copy` leaves `r` alone.
- A struct has no identity. `==` compares the fields, and two structs with equal fields are equal.
- A struct has no functions of its own. `r.area()` calls a free function of the module.

For a C programmer:

- A literal names every field. No field is left zero by leaving it out.
- `==` on two structs compiles and compares field by field. The padding is never read.
- Two structs with the same fields are two types, and a cast between them is refused.

For a C++ programmer:

- A struct has no constructor, no destructor and no member function. A type that needs one is a class.
- A struct field has no default member initializer.
- The default `==` exists without a declaration, and `<` never does.

## packed and align

### What you know

C and C++ change a layout with `#pragma pack` or `__attribute__((packed))`, and raise an alignment with `_Alignas` or `alignas`. Both are used for wire formats, for file headers and to keep two hot values out of one cache line. Java gives the programmer no control over the layout of an object.

### The feature

`packed` before `struct`, `union`, `class` or `variant` removes the padding. Every field then starts at the byte after the one before it, and the type has alignment 1.

`align(N)` after the name raises the alignment of the type to `N` bytes, and its size becomes a multiple of `N`. `N` is a constant power of two. On a class, `align(N)` stands before `inherits`.

Both may stand on one declaration. `packed struct Frame align(4)` has no padding between its fields and is 8 bytes, a multiple of 4.

A packed struct inside another struct keeps its alignment of 1, so `Pair` below is 28 bytes and has no padding either.

```anti
//! name: wire
import anti.io;
import anti.text;

export struct Loose { tag: u8, count: u32, flag: u8, total: u64 }

export packed struct Tight { tag: u8, count: u32, flag: u8, total: u64 }

export struct Cell align(64) { hits: u64 }

packed struct Frame align(4) { tag: u8, count: u32 }

struct Pair { first: Tight, second: Tight }

export fn loose_total(p: *Loose) -> u64
{
	return p.total;
}

export fn tight_total(p: *Tight) -> u64
{
	return p.total;
}

export fn bump(c: *Cell)
{
	c.hits = c.hits +% 1;
}

fn main() -> int
{
	let l = Loose { tag: 1, count: 2, flag: 3, total: 4 };
	let t = Tight { tag: 1, count: 2, flag: 3, total: 4 };
	let c = Cell { hits: 0 };
	bump(&c);
	let at = &t.total;
	*at = 5;
	let line = f"totals {loose_total(&l)} {tight_total(&t)}, hits {c.hits}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Loose {size_of(Loose)}, Tight {size_of(Tight)}, Cell {size_of(Cell)}, Frame {size_of(Frame)}, Pair {size_of(Pair)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
totals 4 5, hits 1
sizes: Loose 24, Tight 14, Cell 64, Frame 8, Pair 28
```

`&t.total` is a `*u64` to byte 6 of the struct, an address that is no multiple of 8. The write through it works on all six targets, whose processors read and write an integer at any address.

The checker refuses each of the following.

```anti refused
//! error: an alignment is a power of two
struct Cell align(24) { hits: u64 }
```

```anti refused
//! error: unknown name `width`
struct Cell align(width) { hits: u64 }
```

```anti refused
//! error: a `simd struct` takes its alignment from its size and has no `align(N)`
simd struct Vec4 align(32) { x: f32, y: f32, z: f32, w: f32 }
```

An `align(N)` below the alignment that the fields give is refused by the back end, once it has laid the struct out for the target. `struct Low align(2) { a: u64 }` ends the build with this message.

```text
antic: `low.Low` has align(2), below its alignment 8 on linux-x86_64
```

### How antic builds it

`packed` and `align(N)` are two marks on the type in the IR, and the layout reads them.

```ir excerpt=wire target=linux-x86_64 mode=release
type wire.Loose = struct { tag: i8, count: i32, flag: i8, total: i64 }
type wire.Tight = packed struct { tag: i8, count: i32, flag: i8, total: i64 }
type wire.Cell = struct align(64) { hits: i64 }
type wire.Frame = packed struct align(4) { tag: i8, count: i32 }
```

For a packed type the layout gives every field the alignment 1. For `align(N)` it takes `N` as the alignment when `N` is larger than what the fields give, and rounds the size up to it.

```llvm excerpt=wire target=linux-x86_64 mode=dev
define internal noundef i64 @loose_total(ptr noundef nonnull dereferenceable(24) %p0) #0 {
...
  %v1 = getelementptr inbounds i8, ptr %v0, i64 16
...
define internal noundef i64 @tight_total(ptr noundef nonnull dereferenceable(14) %p0) #0 {
...
  %v1 = getelementptr inbounds i8, ptr %v0, i64 6
...
  %v3 = load i64, ptr %v2, align 8
```

`total` stands at offset 16 of `Loose` and at offset 6 of `Tight`. The two functions differ in that number and in the size of the struct, 24 and 14. The `load` of the packed field carries `align 8`, the alignment of a `u64`, although byte 6 of a struct of alignment 1 has no such alignment.

```llvm excerpt=wire target=linux-x86_64 mode=dev
define hidden noundef i64 @wire.main() noinline #0 {
...
  %s0 = alloca [24 x i8], align 8
  %s1 = alloca [14 x i8], align 1
  %s2 = alloca [64 x i8], align 64
```

The three slots are `l`, `t` and `c`. The `Tight` is 14 bytes at any address, and the `Cell` is 64 bytes at a multiple of 64, and `main` aligns its frame to 64 on entry for it.

```asm excerpt=wire target=linux-x86_64 mode=release
<loose_total>:
movq 0x10(%rdi), %rax
retq
...
<tight_total>:
movq 0x6(%rdi), %rax
retq
```

Both reads are one `movq`. On arm64 the aligned read is `ldr x0, [x0, #0x10]` and the packed one is `ldur x0, [x0, #0x6]`. The second is the form of the load that takes an offset that is no multiple of 8.

Where it lives: `item_level`, `struct_item`, `class_item` and `variant_item` of `src/antic/parser_item.c` read the two words. `compute` of `src/antic/layout.c` applies `t->packed` and `t->align`. `src/antic/header.c` writes `#pragma pack(push, 1)` and `ANTI_ALIGNAS(N)` into the C header. The header defines that macro as `_Alignas(n)` for C and `alignas(n)` for C++.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of `Loose` | 24 | 24 |
| Bytes of `Tight`, the same fields packed | 14 | 14 |
| Bytes of `Cell`, 8 bytes of fields at `align(64)` | 64 | 64 |
| A read of a packed field | one load, the same count as an aligned one | one instruction on x86-64 and arm64 |
| A frame that holds an `align(64)` local | realigned on entry, `andq $-0x40, %rsp` | the same while the local stays in memory |
| Heap allocations, calls into the runtime | 0 | 0 |
| Checks inserted | none | none |

`packed` saves 10 of 24 bytes here and costs no instruction. A read that crosses a cache line or a page is slower on the processor, which no count of instructions shows. `align(64)` gives a value a cache line of its own and costs 56 bytes per value.

### Pitfalls

For a Java programmer:

- The layout is part of the type. Adding `packed` changes the bytes a file or a socket sees.

For a C programmer:

- `packed` is a word of the declaration, written before `struct`. `align(N)` follows the name.
- `&t.total` of a packed struct compiles without a warning and gives an ordinary `*u64`. A C compiler warns about the same address.
- An `align(N)` below the natural alignment is an error, as `_Alignas` is in C, and it comes from the back end, without a line number.

For a C++ programmer:

- `align(N)` is `alignas(N)` on the type. A single field takes no `align`.
- A packed class keeps its table pointer as its first field, at offset 0.

## Unions and bitfields

### What you know

A `union` of C and C++ overlays its members at offset 0, and nothing records which member was written last. A bitfield packs small integers into one storage unit. The C standard leaves the order and the unit to the compiler, and the ABI of each platform fixes both. Java has neither. A Java program packs bits by hand with shifts and masks.

### The feature

`union` declares fields that share their bytes. Its size is the size of its largest field, rounded up to its alignment. A literal names exactly one field. Reading any field reads the bytes as that type, whatever was written.

A union has no default `==`, since no field says which bytes hold the value. A struct with a union field has none either. `u.hash()` hashes the bytes of the union.

A bitfield is a field with a width in bits after its type, `kind: u8 : 4`. The type is an integer of a fixed width, and its signedness decides how a read widens. The width is from 1 to the bits of the type.

- A read gives a value of the field's type. `mid: i32 : 5` holds -16 to 15 and reads as an `i32`.
- A write takes a value that fits the width. A constant that does not fit is refused, in a write, in a literal and in the default of a field. A dev build stops a write, a compound assignment and a literal whose value does not fit, as it stops a narrowing `as`. A release build keeps the low bits of the value.
- `+%=`, `-%=`, `*%=` and `<<%=` wrap at the width of the field, and `+|=`, `-|=` and `*|=` saturate at its ends. None of them is checked.
- A bitfield has no address. `&h.kind` is refused.
- `_: u32 : 0` is the zero-width bitfield of C. It ends the unit, so the next field starts at the next multiple of the alignment of its type. The name `_` names nothing, and no literal mentions it.
- A struct, a union and a class body hold bitfields. A union holds no zero-width one.

```anti
//! name: bits
import anti.io;
import anti.text;

export struct Header { kind: u8 : 4, mode: u8 : 4, _: u32 : 0, length: u32 }

export struct Bits { low: u32 : 3, mid: i32 : 5, high: u32 : 24, last: u32 : 1 }

export union Word { whole: u64, halves: [2]u32, small: [8]u8 }

struct Wide { a: u8 : 3, b: u16 : 10, c: u8 }

union Odd { a: u8, b: [3]u8 }

export fn mid(b: *Bits) -> i32
{
	return b.mid;
}

export fn set_mid(b: *Bits, v: i32)
{
	b.mid = v;
}

export fn high_half(w: *Word) -> u32
{
	return w.halves[1];
}

export fn wide_size() -> int
{
	return size_of(Wide);
}

fn main() -> int
{
	let h = Header { kind: 9, mode: 2, length: 7 };
	h.kind = 15;
	let b = Bits { low: 7, mid: -3, high: 1000, last: 1 };
	let before = mid(&b);
	set_mid(&b, -11);
	let w = Word { whole: 0x0000000100000002 };
	let line = f"kind {h.kind}, mode {h.mode}, mid {before} {mid(&b)}, high {b.high}, halves {w.halves[0]} {high_half(&w)}, byte {w.small[0]}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Header {size_of(Header)}, Bits {size_of(Bits)}, Wide {wide_size()}, Word {size_of(Word)}, Odd {size_of(Odd)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
kind 15, mode 2, mid -3 -11, high 1000, halves 2 1, byte 2
sizes: Header 8, Bits 8, Wide 4, Word 8, Odd 3
```

```text output windows
kind 15, mode 2, mid -3 -11, high 1000, halves 2 1, byte 2
sizes: Header 8, Bits 8, Wide 6, Word 8, Odd 3
```

`h.kind = 15` is the largest value of four bits, and `h.kind = 16` is refused with `` `16` does not fit the 4 bits of `kind` ``. `set_mid(&b, -11)` stores the five bits `10101`. A dev build stops `set_mid(&b, 21)`, whose five low bits are the same, with `value out of range for the 5 bits of mid: value 21`. `Bits` holds 33 bits: the first three fields fill one `u32`, and `last` starts a second one.

The checker refuses each of the following.

```anti refused
//! error: a bitfield of `u8` has 1 to 8 bits
struct Flags { kind: u8 : 9 }
```

```anti refused
//! error: a bitfield has a sized integer type, found `bool`
struct Flags { ready: bool : 1 }
```

```anti refused
//! error: a bitfield has no address
struct Flags { kind: u8 : 3 }

fn kind_of(f: *Flags) -> *u8
{
	return &f.kind;
}
```

```anti refused
//! error: the field `_` is a zero-width bitfield, written `_: T : 0`
struct Flags { _: u32 : 3, kind: u8 }
```

```anti refused
//! error: a literal of union `Word` names exactly one field
union Word { whole: u64, halves: [2]u32 }

fn both() -> Word
{
	return Word { whole: 1, halves: [1, 2] };
}
```

```anti refused
//! error: a union holds no zero-width bitfield
union Word { whole: u64, _: u32 : 0, half: u32 }
```

```anti refused
//! error: `==` is not defined on `Word`
union Word { whole: u64, halves: [2]u32 }

fn same(a: Word, b: Word) -> bool
{
	return a == b;
}
```

### How antic builds it

A union is an aggregate of the IR whose fields all have offset 0, and a bitfield is a field with a width and a signedness.

```ir excerpt=bits target=linux-x86_64 mode=release
type bits.Bits = struct { low: i32 : 3 zeroext, mid: i32 : 5 signext, high: i32 : 24 zeroext, last: i32 : 1 zeroext }
...
type bits.Word = union { whole: i64, halves: [2]u32, small: [8]byte }
type bits.Header = struct { kind: i8 : 4 zeroext, mode: i8 : 4 zeroext, _: i32 : 0, length: i32 }
```

The layout gives each bitfield a position in bits. The unit of a bitfield is the integer of 1, 2, 4 or 8 bytes that holds all its bits, starting at the byte of its first bit. An access loads the unit, and shifts and masks.

```llvm excerpt=bits target=linux-x86_64 mode=dev
define internal noundef i32 @mid(ptr noundef nonnull dereferenceable(8) %p0) #0 {
...
  %v1 = load i8, ptr %v0, align 1
  %v2 = ashr i8 %v1, 3
  %v3 = sext i8 %v2 to i32
...
define internal void @set_mid(ptr noundef nonnull dereferenceable(8) %p0, i32 noundef %p1) #0 {
...
  %v1 = add i32 %v0, 16
...
  %v3 = icmp ult i32 %v2, 32
...
  %v13 = load i8, ptr %v12, align 1
  %v14 = trunc i32 %v11 to i8
  %v15 = and i8 %v14, 31
  %v16 = shl i8 %v15, 3
  %v17 = and i8 %v13, 7
  %v18 = or i8 %v17, %v16
  store i8 %v18, ptr %v12, align 1
```

`mid` takes bits 3 to 7 of the struct, which lie in its first byte, so its unit is an `i8`.

- The read loads the byte. `ashr i8 %v1, 3` moves the field to the bottom and fills from its sign bit, and `sext` widens it to the `i32` of the field's type. An unsigned field shifts with `lshr` and masks.
- The write of a dev build checks the value first. `add i32 %v0, 16` moves the range -16 to 15 of the field to 0 to 31, and `icmp ult i32 %v2, 32` holds both ends in one comparison. A value outside them branches to the block that calls `anti_rt_check_failed`.
- The write loads the byte and truncates the new value. `and i8 %v14, 31` keeps five bits and `shl` moves them into place. `and i8 %v13, 7` clears the field in the old byte, `or` joins the two and the `store` writes the byte back.

The write is a load and a store of the unit. Two threads that write two bitfields of one unit therefore race, as they do in C.

```asm excerpt=bits target=linux-x86_64 mode=release
<mid>:
movzbl (%rdi), %eax
sarb $0x3, %al
movsbl %al, %eax
retq
...
<set_mid>:
movzbl (%rdi), %eax
shlb $0x3, %sil
andb $0x7, %al
orb %sil, %al
movb %al, (%rdi)
retq
...
<high_half>:
movl 0x4(%rdi), %eax
retq
```

`set_mid` needs no mask of the new value, since the shift by 3 in a byte drops every bit above the fifth. `high_half` reads `w.halves[1]` of the union as one load at offset 4: a union costs nothing beyond the access to the field.

Linux and macOS follow the System V rule for bitfields. A field goes right after the one before it unless it would cross a boundary of its own type. Windows follows the MSVC rule. A field whose type has another size than the unit in use starts a new unit. `Wide` holds a `u8`, a `u16` and a `u8`, and the two rules disagree on it.

```llvm excerpt=bits target=linux-x86_64 mode=dev
define internal noundef i64 @wide_size() #0 {
b0:
  ret i64 4
```

```llvm excerpt=bits target=windows-x86_64 mode=dev
define dso_local noundef i64 @wide_size() #0
b0:
  ret i64 6
```

`size_of(Wide)` is a symbol in the IR, and the back end of each target writes its own number. Both sizes are what the C compiler of the platform gives the same declaration.

Where it lives: `compute` and `bit_unit` of `src/antic/layout.c`. `lower_place` of `src/antic/lower_place.c` marks a place as a bitfield, and `ir_bitload` and `ir_bitstore` of `src/antic/ir.c` are its two instructions. `bits` of `src/antic/llvm_emit_function.c` writes the shifts and masks.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of a union | the largest field, rounded up to the alignment: `Word` 8, `Odd` 3 | the same |
| Bytes of bitfields | the bits, in units of the field types: `Header` 8, `Bits` 8, `Wide` 4, and 6 on Windows | the same |
| A read of a union field | one load | one instruction |
| A read of a bitfield | one load, one or two shifts or a mask, one widening | three instructions in `mid` |
| A write of a bitfield | one load, two masks, one shift, one `or`, one store | five instructions in `set_mid` |
| Heap allocations, calls into the runtime | 0 | 0 |
| Atomic operations and locks | 0, the write is not atomic | 0 |
| Checks inserted | one comparison and one branch per write of a value that is no constant | none, a write truncates |

### Pitfalls

For a Java programmer:

- A union does not know which field it holds. A variant of this chapter does.
- A write to a bitfield of a value that does not fit stops a dev build. A release build drops the bits that do not fit and reports nothing.

For a C programmer:

- The zero-width bitfield has a name, `_`. C leaves it unnamed.
- `bool` and an enum are no types of a bitfield. The field is a sized integer, and the program compares it with 0.
- A literal of a union names one field. It cannot be left empty.

For a C++ programmer:

- A union holds plain data. A member with a destructor is not torn down, since a union owns nothing.
- The layout of bitfields on Windows is the layout of MSVC, and it differs from Linux and macOS for fields of mixed types.

## Simd structs

### What you know

C and C++ reach the vector instructions through intrinsics such as `_mm_add_ps` or through the vector types of a compiler extension. A plain loop may be vectorized by the compiler. Each intrinsic names one instruction of one processor. Java has the Vector API, whose calls the virtual machine compiles to vector instructions where it can. In all three the width of the vector register decides how the code is written.

### The feature

`simd struct` declares a vector. Each field is one lane.

- Every lane has the same type: `bool`, an integer of a fixed width, `f16`, `f32` or `f64`. A `char` lane is refused.
- The count of lanes is a power of two, and the size is a multiple of 8 bytes.
- A simd struct takes no `packed`, no `align(N)` and no bitfield. Its alignment is its size or 16, whichever is less.
- A lane is read and written as a field, `v.x`.

The operators work lane by lane on two values of one simd struct.

| Form | Lanes | Result |
|---|---|---|
| `a + b`, `a - b`, `a * b`, `a / b`, `-a` | numbers | the simd struct |
| `a & b`, `a \| b`, `a ^ b`, `a << b`, `a >> b`, `~a` | integers | the simd struct |
| `a == b`, `a != b`, `a < b`, `a <= b`, `a > b`, `a >= b` | any | a mask |
| `T.splat(x)` | any | every lane holds `x` |
| `T.load(slice, i)`, `v.store(slice, i)` | any | the lanes from, or to, the elements from `i` on |
| `v.shuffle(i0, i1, ...)` | any | lane `k` of the result is lane `ik` of `v`, each `ik` a constant |
| `v.sum()`, `v.min()`, `v.max()`, `a.dot(b)` | numbers | one value of the lane type |
| `simd.select(m, a, b)` | any | lane by lane, `a` where `m` is true and `b` elsewhere |
| `simd.any(m)`, `simd.all(m)` | `bool` | a `bool` |
| `v as [4]f32`, `v as Plain` and back | any | the same bytes as an array or a plain struct |

A mask is a simd struct of `bool` with the lane names of the type compared. It has no spelling of its own: a program names it through the value of a comparison. A mask is no `bool`, so `if a < b` is refused, and `simd.any` or `simd.all` reduces it. `select`, `any` and `all` belong to `anti.simd`, which the module imports.

`+`, `-` and `*` on integer lanes wrap in every build. `/` and the shifts keep the rules of the scalar operators, with their checks in a dev build. `%`, the wrapping and saturating operators, `&&`, `||` and `!` do not apply.

`v.sum()` folds the upper half of the lanes onto the lower half until one lane is left. A float sum therefore gives the same result on every target.

A simd struct of more than 256 bytes is accepted with the warning `above-vector-cap`. It is an array, and each operation on it is a loop over its lanes. A release build refuses every warning, so such a declaration carries `allow(above-vector-cap, "reason")` after its name.

```anti
//! name: lanes
import anti.io;
import anti.simd;
import anti.text;

export simd struct Vec4 { x: f32, y: f32, z: f32, w: f32 }

export simd struct F32x8 { a: f32, b: f32, c: f32, d: f32, e: f32, f: f32, g: f32, h: f32 }

simd struct I32x4 { a: i32, b: i32, c: i32, d: i32 }

simd struct F64x2 { x: f64, y: f64 }

struct Plain { x: f32, y: f32, z: f32, w: f32 }

export fn add4(a: Vec4, b: Vec4) -> Vec4
{
	return a + b;
}

export fn add8(a: *F32x8, b: *F32x8, to: *F32x8)
{
	*to = *a + *b;
}

export fn clamp(a: Vec4, top: Vec4) -> Vec4
{
	return simd.select(a < top, a, top);
}

export fn total(a: Vec4) -> f32
{
	return a.sum();
}

fn main() -> int
{
	let a = Vec4 { x: 1.0, y: 2.0, z: 3.0, w: 4.0 };
	let b = Vec4.splat(2.0);
	let c = a * b;
	let m = a < b;
	let d = clamp(c, Vec4.splat(5.0));
	let r = a.shuffle(3, 2, 1, 0);
	let i = I32x4 { a: 1, b: 2, c: 3, d: 4 };
	let j = (i << I32x4.splat(2)) | I32x4.splat(1);
	let items: [6]f32 = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0];
	let l = Vec4.load(items[0..6], 2);
	l.store(items[0..6], 0);
	let p = a as Plain;
	let arr = a as [4]f32;
	let line = f"c {c.x} {c.y} {c.z} {c.w}, mask {m.x} {m.y} {m.z} {m.w}, any {simd.any(m)}, all {simd.all(m)}, d {d.x} {d.w}";
	io.println(line);
	free(line.ptr);
	line = f"sum {a.sum()}, min {a.min()}, max {a.max()}, dot {a.dot(b)}, r {r.x} {r.w}, j {j.a} {j.d}, items {items[0]} {items[3]}, p {p.y}, arr {arr[2]}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Vec4 {size_of(Vec4)}, F32x8 {size_of(F32x8)}, I32x4 {size_of(I32x4)}, F64x2 {size_of(F64x2)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```
```text output
c 2 4 6 8, mask true false false false, any true, all false, d 2 5
sum 10, min 1, max 4, dot 20, r 4 1, j 5 17, items 3 6, p 2, arr 3
sizes: Vec4 16, F32x8 32, I32x4 16, F64x2 16
```

The checker refuses each of the following.

```anti refused
//! error: a `simd struct` has a power of two of lanes, and `Three` has 3
simd struct Three { x: f32, y: f32, z: f32 }
```

```anti refused
//! error: every lane of `simd struct` `Mixed` is `f32`, and `y` is `f64`
simd struct Mixed { x: f32, y: f64 }
```

```anti refused
//! error: a `simd struct` is a multiple of 8 bytes, and `Tiny` is 4
simd struct Tiny { x: i16, y: i16 }
```

```anti refused
//! error: `&` needs integer lanes, and `Vec4` has `f32`
simd struct Vec4 { x: f32, y: f32, z: f32, w: f32 }

fn both(a: Vec4, b: Vec4) -> Vec4
{
	return a & b;
}
```

```anti refused
//! error: `+%` does not apply to a `simd struct`
simd struct I32x4 { a: i32, b: i32, c: i32, d: i32 }

fn wrapped(a: I32x4, b: I32x4) -> I32x4
{
	return a +% b;
}
```

```anti refused
//! error: `Vec4` is a `simd struct`, whose operators are built in
simd struct Vec4 { x: f32, y: f32, z: f32, w: f32 }

operator fn add(a: Vec4, b: Vec4) -> Vec4
{
	return a;
}
```

```anti refused
//! error: a condition has type `bool`, found `mask(Vec4)`
simd struct Vec4 { x: f32, y: f32, z: f32, w: f32 }

fn below(a: Vec4, b: Vec4) -> bool
{
	if a < b {
		return true;
	}
	return false;
}
```

### How antic builds it

A simd struct is an aggregate of the IR with the mark `simd`, and its mask is a second one with a lane of one byte.

```ir excerpt=lanes target=linux-x86_64 mode=release
type lanes.Vec4 = simd struct { x: f32, y: f32, z: f32, w: f32 }
type lanes.F32x8 = simd struct { a: f32, b: f32, c: f32, d: f32, e: f32, f: f32, g: f32, h: f32 }
...
type lanes.mask(Vec4) = simd struct { x: i8, y: i8, z: i8, w: i8 }
...
    vbinary fadd f32 %3, %0, %1, lanes.F32x8
    memcopy %2, %3, lanes.F32x8
...
    vbinary flt f32 %2, %0, %1, lanes.Vec4
    vselect f32 %3, %2, %0, %1, lanes.Vec4
...
    %1 = vreduce fadd f32 %0, lanes.Vec4
```

An operation on a whole simd struct is one instruction of the IR that works on values in memory. `vbinary fadd` adds two `F32x8` into the slot `%3`, and names the simd struct but no width of a register. `vbinary flt` writes a mask, `vselect` chooses by it, and `vreduce fadd` is `a.sum()`.

The LLVM text types a simd struct as a vector, `<N x T>`.

```llvm excerpt=lanes target=linux-x86_64 mode=dev
define internal <4 x float> @add4(<4 x float> %p0, <4 x float> %p1) #0 {
b0:
...
  %a0 = alloca [16 x i8], align 16
...
  %v3 = load <4 x float>, ptr %v1, align 16
  %v4 = load <4 x float>, ptr %v2, align 16
  %v5 = fadd <4 x float> %v3, %v4
  store <4 x float> %v5, ptr %v0, align 16
...
define internal void @add8(ptr noundef nonnull dereferenceable(32) %p0, ptr noundef nonnull dereferenceable(32) %p1, ptr noundef nonnull dereferenceable(32) %p2) #0 {
...
  %v5 = fadd <8 x float> %v3, %v4
```

- `add4` takes and returns `<4 x float>`. A simd struct of 16 bytes is the vector type of C and passes in one vector register. The Windows x64 convention passes a pointer to a copy and returns the vector in `xmm0`.
- The slot of a `Vec4` is 16 bytes aligned to 16.
- Each operation loads its operands as vectors, computes with one instruction of LLVM and stores the result. `opt` keeps the values in registers.
- An `F32x8` is 32 bytes, so it passes as the struct of its lanes, here behind three pointers. Its add is one `fadd <8 x float>`.

The code generator of LLVM splits a vector at the native width of the target and its processor level.

```asm excerpt=lanes target=linux-x86_64 mode=release
<add4>:
vaddps %xmm1, %xmm0, %xmm0
retq
...
<add8>:
vmovups (%rdi), %ymm0
vaddps (%rsi), %ymm0, %ymm0
vmovups %ymm0, (%rdx)
vzeroupper
retq
...
<clamp>:
vminps %xmm1, %xmm0, %xmm0
retq
...
<total>:
vshufpd $0x1, %xmm0, %xmm0, %xmm1
vaddps %xmm1, %xmm0, %xmm0
vmovshdup %xmm0, %xmm1
vaddss %xmm1, %xmm0, %xmm0
```

At x86-64-v3, the default level of linux-x86_64, the eight lanes of `add8` are one `vaddps` on a `ymm` register. `clamp` is a comparison and a `simd.select` in the source, and `opt` reduces the pair to one `vminps`. `total` folds the upper two lanes onto the lower two and then the second lane onto the first.

```asm excerpt=lanes target=linux-arm64 mode=release
<add8>:
ldp q3, q0, [x1]
ldp q2, q1, [x0]
fadd v0.4s, v1.4s, v0.4s
fadd v1.4s, v2.4s, v3.4s
stp q1, q0, [x2]
ret
```

A register of arm64 holds 16 bytes, so the same `fadd <8 x float>` is two `fadd` instructions there. The lane count belongs to the program and the instruction count to the target.

An operation on `f16` lanes, an integer division and a shift are one scalar operation per lane. Lowering writes them that way.

Where it lives: `check_simd_struct` of `src/antic/sema_declare.c` and `src/antic/sema_simd.c` check the declaration and the operations. `lower_simd`, `lower_simd_binary` and `lower_simd_unary` of `src/antic/lower_simd.c` write the IR. `compute` of `src/antic/layout.c` gives the alignment, and `CPU_VECTOR_BYTE_CAP` of `src/antic/cpu.h` is the cap.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of a simd struct | the lanes, no padding: `Vec4` 16, `F32x8` 32 | the same |
| Alignment | the size, at most 16 | the same |
| An add of 16 bytes | a load of each operand, one `fadd`, a store | one instruction |
| An add of 32 bytes | the same | one `vaddps` at x86-64-v3, two `fadd` on arm64 |
| A comparison | one compare and a mask of one byte per lane in memory | a mask in a register |
| `v.sum()` of four lanes | two folds | four instructions in `total` |
| An argument of 16 bytes | one vector register, a pointer to a copy on Windows x64 | the same |
| An argument of another size | as the struct of its lanes | the same |
| Heap allocations, calls into the runtime | 0 | 0 |
| Checks inserted | the bounds of `load` and `store`, the checks of `/` and the shifts | none |

A dev build tests the first and the last element that `T.load(slice, i)` and `v.store(slice, i)` reach, and ends the program with `index out of bounds`. A release build has no such test, as it has none for an index into a slice.

### Pitfalls

For a Java programmer:

- A simd struct is a value of fixed size in the frame or in a register. Nothing is allocated, and no virtual machine decides whether the vector instruction is used.
- A comparison gives a mask of the same lane count, to be reduced with `simd.any` or `simd.all`.

For a C programmer:

- The type names the lanes and no instruction set. The same source compiles for x86_64 and for arm64.
- An argument of 32 bytes is no `__m256`. It passes as a struct, in memory.
- Integer lanes wrap on `+`, `-` and `*` in a dev build too, where a scalar `+` would trap on overflow.

For a C++ programmer:

- An `operator fn` on a simd struct is refused. The operators are built in.
- `a == b` gives a mask. `simd.all(a == b)` is the test for equal vectors.

## Enums

### What you know

An `enum` of C is an `int` with names, and any integer converts to it without a test. C++ adds `enum class`, whose values are scoped and convert only by a cast. An enum of Java is a class with one object per value, with fields and methods of its own.

### The feature

`enum` declares a named integer type and its values.

- The values count from 0. A value without `=` is one more than the value before it.
- The underlying type follows the name after a colon, `enum Mode: u8`. Without one it is `c_int`. Every value fits the underlying type.
- A value is written with its type, `Kind.Circle`, and is a constant expression.
- `==` and `!=` compare two values of one enum. `<` and the other order comparisons are refused.
- `as` converts an enum to a number and an integer to an enum. A float and a value of another enum are refused, and the program converts them through an integer, `x as int as Mode`. A dev build tests a conversion to an enum against the declared values. Any other number ends the program with `value not declared by Mode`. A release build does not test.
- `switch` takes an enum. Its arms name the values with their type, and a `switch` without `else` covers every value.
- `k.hash()` hashes the value as its integer.
- A class body may declare an enum, which only that class names.

```anti
//! name: kinds
import anti.io;
import anti.text;

export enum Kind { Circle, Square, Empty }

export enum Mode: u8 { Read = 1, Write = 2, Both }

enum Level: i16 { Low = -1, Mid, High = 100 }

export fn next(k: Kind) -> Kind
{
	switch k {
		Kind.Circle => { return Kind.Square; },
		Kind.Square => { return Kind.Empty; },
		Kind.Empty => { return Kind.Circle; },
	}
}

export fn writes(m: Mode) -> bool
{
	return m == Mode.Write || m == Mode.Both;
}

export fn mode_of(n: int) -> Mode
{
	return n as Mode;
}

fn main() -> int
{
	let k = next(Kind.Square);
	let m = mode_of(3);
	let l = Level.Mid;
	let line = f"kind {k as int}, mode {m as int}, writes {writes(m)} {writes(Mode.Read)}, level {l as int} {Level.High as int}, same {k == Kind.Empty}, hash {k.hash() == Kind.Empty.hash()}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Kind {size_of(Kind)}, Mode {size_of(Mode)}, Level {size_of(Level)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```
```text output
kind 2, mode 3, writes true false, level 0 100, same true, hash true
sizes: Kind 4, Mode 1, Level 2
```

`Both` follows `Write` and is 3. `Mid` follows `Low` and is 0.

The checker refuses each of the following.

```anti refused
//! error: expected `int`, found `Kind`
enum Kind { Circle, Square }

fn number(k: Kind) -> int
{
	return k;
}
```

```anti refused
//! error: expected `Kind`, found an integer literal
enum Kind { Circle, Square }

fn first() -> Kind
{
	let k: Kind = 0;
	return k;
}
```

```anti refused
//! error: `<` needs numeric, `char` or `str` operands, found `Kind`
enum Kind { Circle, Square }

fn before(a: Kind, b: Kind) -> bool
{
	return a < b;
}
```

```anti refused
//! error: the operands of `==` have the types `Kind` and `Mode`
enum Kind { Circle, Square }

enum Mode { Read, Write }

fn mixed(k: Kind) -> bool
{
	return k == Mode.Read;
}
```

```anti refused
//! error: this `switch` on `Kind` has no arm for `Square`
enum Kind { Circle, Square }

fn sides(k: Kind) -> int
{
	switch k {
		Kind.Circle => { return 0; },
	}
	return 4;
}
```

```anti refused
//! error: `300` does not fit `u8`
enum Mode: u8 { Read = 300 }
```

```anti refused
//! error: the underlying type of an enum is an integer type, found `f32`
enum Mode: f32 { Read }
```

```anti refused
//! error: enum `Kind` has two values named `Circle`
enum Kind { Circle, Circle }
```

### How antic builds it

An enum is its underlying integer from lowering on. The IR has no enum type, and a value is a constant. What remains of the type is the range of its values, which antic hands to LLVM.

```llvm excerpt=kinds target=linux-x86_64 mode=dev
define internal noundef zeroext range(i8 0, 4) i8 @mode_of(i64 noundef %p0) #0 {
...
  %v1 = icmp eq i64 %v0, 1
...
  %v4 = icmp eq i64 %v3, 2
...
  %v10 = icmp eq i64 %v9, 3
...
  br i1 %v16, label %b2, label %b1
```

- The result of `mode_of` is an `i8` with `range(i8 0, 4)`: the optimizer may assume a `Mode` is below 4.
- The three `icmp` are the dev check of `n as Mode`, one compare per declared value. The branch goes to `b1` when none matched, which calls `anti_rt_check_failed` with the text `value not declared by Mode`.

```asm excerpt=kinds target=linux-x86_64 mode=release
<next>:
testl %edi, %edi
je 0xf <next+0xf>
cmpl $0x1, %edi
jne 0x15 <next+0x15>
movl $0x2, %eax
retq
...
<writes>:
andb $-0x2, %dil
cmpb $0x2, %dil
sete %al
retq
...
<mode_of>:
movq %rdi, %rax
retq
```

`next` is the `switch` over `Kind`. Its arms name every value and each returns, so the `switch` ends the path of the function and no `return` follows it. antic writes a chain of compares and marks the path after the last one as never reached, so `opt` keeps two compares: a value that is neither `Circle` nor `Square` is `Empty`. `writes` tests two values with one mask and one compare. `mode_of` of a release build returns its argument: the conversion is free and unchecked.

The C header of a library writes an exported enum as an `enum` of C.

Where it lives: `sema_declare_enums_and_variants` of `src/antic/sema_declare.c` gives each value its number. `enum_check` of `src/antic/lower_check.c` writes the dev check, and `lower_switch` of `src/antic/lower_stmt.c` the chain of compares.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of an enum | its underlying type: `Kind` 4, `Mode` 1, `Level` 2 | the same |
| A value | a constant | a constant |
| `a == b` | one integer compare | the same |
| Enum to number with `as` | a widening or nothing | the same |
| Number to enum with `as` | one compare per declared value and a branch | nothing |
| A `switch` over an enum | one compare per arm, in the order written | a table or a compare tree, as `opt` chooses |
| Heap allocations | 0 | 0 |
| Calls into the runtime | 0 on the passing path | 0 |
| Checks inserted | the test of `as` to an enum | none |

A release build trusts a number converted to an enum. A value outside the declared ones then breaks the `range` that LLVM was told, which is undefined behaviour.

### Pitfalls

For a Java programmer:

- An enum is an integer. It has no fields, no functions and no `values()`. A variant holds data per case.
- `switch` over an enum writes `Kind.Circle` in its arms, with the type.

For a C programmer:

- An enum does not convert to `int` on its own, and an integer does not convert to an enum. Both directions take `as`.
- The default underlying type is `c_int`, as in C. `enum Mode: u8` makes it one byte.
- Two enums are two types, and `==` between them is refused.

For a C++ programmer:

- Every enum is scoped, as `enum class` is.
- A dev build checks `n as Mode`. A release build checks nothing, as a `static_cast` to an enum checks nothing.

## Variants

### What you know

C writes a sum type by hand: a struct with a tag and a union, and a `switch` on the tag that nothing checks. C++ has `std::variant`, a class template that stores an index and the largest alternative, read through `std::visit` or `std::get`. Java has sealed interfaces with record classes, each value an object on the heap, and `switch` patterns over them.

### The feature

`variant` declares a type that holds exactly one of its cases. A case is a name, and optionally fields in braces.

- A literal names the variant and the case, `Shape.Circle { r: 2.0 }`, and names every field of the case. A case without fields is written `Shape.Empty`.
- `v.tag` reads the tag. The tag numbers the cases from 0 in the order of the declaration, converts to a number with `as` and is read-only.
- `v is Shape.Circle` tests the case and gives a `bool`.
- The fields of a case are reached only through `switch` and `if let` of the next section. `v.r` is refused.
- A variant is a value. `=` copies it, and `==` compares two of them: equal tags, and equal fields of that case. `v.hash()` follows the same parts.
- A field of a case has a name and a type, with no default and no bitfield width.
- A variant has one case or more, and a variant that holds itself is refused.
- A declaration takes `packed` and `align(N)`. `packed` reaches the fields of the cases as well.
- Another module names a case as `geo.Shape.Circle { r: 2.0 }` and tests it as `v is geo.Shape.Circle`.
- A variant with a case that owns something follows the value rules of [chapter 11]({{% relref "11-ownership" %}}). It is torn down by its tag, and `=` of an existing one is refused.

```anti
//! name: shapes
import anti.io;
import anti.text;

export variant Shape
{
	Circle { r: f32 },
	Box { w: f32, h: f32 },
	Empty,
}

variant Token
{
	Number { value: f64 },
	Word { text: str },
	End,
}

variant Signal { Red, Amber, Green }

packed variant Tight { Wide { n: u64 }, Nothing }

struct Drawing { shape: Shape, layer: int }

export fn unit() -> Shape
{
	return Shape.Box { w: 1.0, h: 1.0 };
}

export fn round(s: *Shape) -> bool
{
	return *s is Shape.Circle;
}

export fn tag_of(s: *Shape) -> int
{
	return s.tag as int;
}

fn main() -> int
{
	let c = Shape.Circle { r: 2.0 };
	let b = unit();
	let e = Shape.Empty;
	let copy = c;
	let d = Drawing { shape: b, layer: 1 };
	let t = Token.Word { text: "let" };
	let line = f"round {round(&c)} {round(&b)}, tag {tag_of(&c)} {tag_of(&b)} {tag_of(&e)}, equal {c == copy} {c == b} {d.shape == b}, hash {c.hash() == copy.hash()}, word {t is Token.Word}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Shape {size_of(Shape)}, Token {size_of(Token)}, Signal {size_of(Signal)}, Tight {size_of(Tight)}, Drawing {size_of(Drawing)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```
```text output
round true false, tag 0 1 2, equal true false true, hash true, word true
sizes: Shape 12, Token 24, Signal 1, Tight 9, Drawing 24
```

`Shape` is one byte of tag, three of padding and the 8 bytes of `Box`, its largest case. `Token` holds a `str` of 16 bytes behind a tag that is padded to 8. `Signal` has no case with fields and is its tag alone. `Tight` is packed, so its `u64` follows the tag at offset 1.

The checker refuses each of the following.

```anti refused
//! error: a variant has the field `tag` alone, and `switch` reads the fields of its cases
variant Shape { Circle { r: f32 }, Empty }

fn radius(s: Shape) -> f32
{
	return s.r;
}
```

```anti refused
//! error: the literal of `Shape.Circle` misses the field `r`
variant Shape { Circle { r: f32 }, Empty }

fn dot() -> Shape
{
	return Shape.Circle { };
}
```

```anti refused
//! error: `Shape` has no case `Square`
variant Shape { Circle { r: f32 }, Empty }

fn square() -> Shape
{
	return Shape.Square;
}
```

```anti refused
//! error: the `tag` of a variant is read-only
variant Shape { Circle { r: f32 }, Empty }

fn clear(s: *Shape)
{
	s.tag = 1;
}
```

```anti refused
//! error: `is` on `Shape` names one of its cases, as `Shape.Circle`
variant Shape { Circle { r: f32 }, Empty }

fn round(s: Shape) -> bool
{
	return s is Circle;
}
```

```anti refused
//! error: variant `Shape` has no case
variant Shape { }
```

```anti refused
//! error: variant `Tree` contains itself
variant Tree { Leaf { value: int }, Pair { left: Tree, right: Tree } }
```

### How antic builds it

The checker builds four kinds of types for a variant. Each case with fields is a struct, and a union holds those structs. The tags are an enum. The variant is a struct of the tag and the union.

```ir excerpt=shapes target=linux-x86_64 mode=release
type shapes.Shape.Circle = struct { r: f32 }
type shapes.Shape.Box = struct { w: f32, h: f32 }
type shapes.Shape.union = union { Circle: shapes.Shape.Circle, Box: shapes.Shape.Box }
type shapes.Shape = struct { tag: i8, u: shapes.Shape.union }
...
type shapes.Signal = struct { tag: i8 }
...
type shapes.Tight = packed struct { tag: i8, u: shapes.Tight.union }
...
export fn shapes.unit() -> agg shapes.Shape {
b0:
    %0 = slot shapes.Shape
    store i8 1, %0
    %1 = ptradd %0, offset_of shapes.Shape.u
    store f32 1, %1
    %2 = ptradd %1, offset_of shapes.Shape.Box.h
    store f32 1, %2
```

- `Shape` is a struct of two fields, `tag` and `u`. The tag is a `u8` up to 255 cases, a `u16` up to 65535 and a `u32` above.
- `Empty` has no fields and no member in the union. `Signal`, whose cases all lack fields, has no `u`.
- The literal `Shape.Box { w: 1.0, h: 1.0 }` in `unit` is a store of the tag 1, and one store per field at the offset of `u`.

From here a variant is a struct, and the layout of the first section applies. The union aligns to 4 for its `f32` fields, so `u` stands at offset 4.

```llvm excerpt=shapes target=linux-x86_64 mode=dev
define internal { i64, float } @unit() #0 {
b0:
...
  %s0 = alloca [12 x i8], align 4
...
  store i8 1, ptr %v0, align 1
...
  %v2 = getelementptr i8, ptr %v1, i64 4
...
  store float 0x3FF0000000000000, ptr %v3, align 4
...
define internal noundef zeroext range(i8 0, 2) i8 @round(ptr noundef nonnull dereferenceable(12) %p0) #0 {
...
  %v1 = load i8, ptr %v0, align 1, !range
...
  %v3 = icmp eq i8 %v2, 0
```

- The slot of a `Shape` is `[12 x i8]` aligned to 4, the type the LLVM module gives it.
- `unit` returns the 12 bytes as `{ i64, float }`, the two registers the System V convention gives a struct of an integer part and a float part.
- `round` is `*s is Shape.Circle`: one load of the tag and one compare with 0, the number of `Circle`. The load carries `!range`, here 0 to 3, so the optimizer knows the tag names a case.

```asm excerpt=shapes target=linux-x86_64 mode=release
<unit>:
movabsq $0x3f80000000000001, %rax
vmovss (%rip), %xmm0
retq
...
<round>:
cmpb $0x0, (%rdi)
sete %al
retq
...
<tag_of>:
movzbl (%rdi), %eax
retq
```

`unit` builds its result in registers. The register `rax` holds the tag 1 in its low byte and the bits of `1.0` for `w` in its high half, and `xmm0` holds `h`. `round` is one compare of a byte in memory, and `tag_of` one load.

The C header of a library writes `enum Shape_tag` with `Shape_Circle = 0` and the following values, and then the struct with the tag and the union.

Where it lives: `declare_cases` of `src/antic/sema_declare.c` and `types_set_cases` of `src/antic/types.c` build the parts. `build_variant`, `lower_load_tag` and `lower_case_address` of `src/antic/lower_expr.c`. `each_case` of `src/antic/lower_owning.c` tears down, copies and clears a variant by its tag. `variant_view` of `src/antic/header.c` writes the header.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of a variant | the tag, the padding to the alignment of the union, the largest case: `Shape` 12, `Token` 24 | the same |
| Bytes of a variant without fields | the tag: `Signal` 1 | the same |
| A literal | one store of the tag, one store per field | the same, or constants in registers |
| `v is Case`, `v.tag` | one load, one compare | one or two instructions |
| A copy | one `memcpy` of the size | moves through registers |
| `a == b` | a compare of the tags, then of the fields of the case | the same |
| Heap allocations | 0 | 0 |
| Calls into the runtime | 0 | 0 |
| Checks inserted | none | none |

Every value of a variant has the size of its largest case. A variant with one large case and many small ones pays for the large one in each value. A pointer in that case moves the large part elsewhere.

### Pitfalls

For a Java programmer:

- A variant is a value in place, with no object per case. Assigning it copies it.
- A case is no type. `Shape.Circle` cannot be the type of a parameter or a variable.

For a C programmer:

- The union inside a variant cannot be read past its tag. `v.u` and `v.r` are refused, and the tag cannot be written.
- A literal sets the tag and every field of the case at once, so a variant never holds a tag without its fields.

For a C++ programmer:

- A variant always holds one of its declared cases. It has no state like `valueless_by_exception`.
- The cases are named, so two cases may hold the same types. `std::variant<float, float>` needs an index for that.
- The tag is one byte for up to 255 cases and stands first in the value.

## Pattern matching

### What you know

C takes a tagged union apart with a `switch` on the tag and reads the member the programmer believes is there. C++ uses `std::visit` with an overload set, or `std::get_if`, and `std::get` throws on the wrong alternative. Java matches a sealed interface with `switch` and type patterns, and the compiler checks that the patterns cover every permitted class.

### The feature

`switch` on a variant value has one arm per case. An arm names the case alone, without the variant, also for a variant of another module.

- `Circle c => ...` binds `c` to a copy of the fields of the case. `c.r` reads a field. The name lives in that arm alone.
- A change of `c` changes the copy and leaves the variant as it was.
- `Circle => ...` names the case and binds nothing. A case without fields takes only this form.
- A `switch` without `else` has an arm for every case, and the message names the missing ones. A case has one arm at most.
- `fallthrough` never enters an arm that binds.
- An arm is one statement or a block, followed by a comma. `switch` is a statement and gives no value.

`if let Circle c = v { ... }` is a `switch` of one arm. It takes an `else`, and `else if let` continues the chain. `if let Empty = v` tests a case without binding.

`switch`, `if let` and `is` take a variant value. A pointer `p` to a variant is written `*p`, which reads through the pointer and copies only the case an arm binds.

An arm that binds a case with an owning field gets a copy of what the field owns. The arm tears the copy down at its end. [Chapter 11]({{% relref "11-ownership" %}}) covers that copy.

```anti
//! name: match
import anti.io;
import anti.text;

export variant Shape
{
	Circle { r: f32 },
	Box { w: f32, h: f32 },
	Empty,
}

export fn area(s: *Shape) -> f32
{
	let a: f32 = 0.0;
	switch *s {
		Circle c => a = 3.0 * c.r * c.r,
		Box b => a = b.w * b.h,
		Empty => { },
	}
	return a;
}

export fn width(s: *Shape) -> f32
{
	if let Box b = *s {
		return b.w;
	} else if let Circle c = *s {
		return 2.0 * c.r;
	}
	return 0.0;
}

fn doubled(s: Shape) -> f32
{
	switch s {
		Circle c => {
			c.r = c.r * 2.0;
			return c.r;
		},
		else => { },
	}
	return 0.0;
}

fn flat(s: Shape) -> bool
{
	let empty = false;
	switch s {
		Empty => empty = true,
		Circle => { },
		Box => { },
	}
	return empty;
}

fn main() -> int
{
	let c = Shape.Circle { r: 2.0 };
	let b = Shape.Box { w: 3.0, h: 4.0 };
	let e = Shape.Empty;
	let twice = doubled(c);
	let still = 0.0 as f32;
	if let Circle same = c {
		still = same.r;
	}
	let line = f"area {area(&c)} {area(&b)} {area(&e)}, width {width(&c)} {width(&b)} {width(&e)}, doubled {twice}, still {still}, flat {flat(e)} {flat(b)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```
```text output
area 12 12 0, width 4 3 0, doubled 4, still 2, flat true false
```

`doubled` writes to `c.r` and returns 4. The `Shape` that `main` passed still holds 2, which `still` shows.

The checker refuses each of the following.

```anti refused
//! error: this `switch` on `Shape` has no arm for `Box`, `Empty`
variant Shape { Circle { r: f32 }, Box { w: f32, h: f32 }, Empty }

fn radius(s: Shape) -> f32
{
	let r: f32 = 0.0;
	switch s {
		Circle c => r = c.r,
	}
	return r;
}
```

```anti refused
//! error: case `Empty` of `Shape` has no fields to bind
variant Shape { Circle { r: f32 }, Empty }

fn radius(s: Shape) -> f32
{
	let r: f32 = 0.0;
	switch s {
		Circle c => r = c.r,
		Empty e => { },
	}
	return r;
}
```

```anti refused
//! error: this case already has an arm
variant Shape { Circle { r: f32 }, Empty }

fn radius(s: Shape) -> f32
{
	let r: f32 = 0.0;
	switch s {
		Circle c => r = c.r,
		Circle d => r = d.r,
		Empty => { },
	}
	return r;
}
```

```anti refused
//! error: `switch` takes an enum, an integer, a `str` or a variant, found `*Shape`
variant Shape { Circle { r: f32 }, Empty }

fn radius(s: *Shape) -> f32
{
	let r: f32 = 0.0;
	switch s {
		Circle c => r = c.r,
		Empty => { },
	}
	return r;
}
```

```anti refused
//! error: unknown name `c`
variant Shape { Circle { r: f32 }, Empty }

fn radius(s: Shape) -> f32
{
	if let Circle c = s {
		return c.r;
	}
	return c.r;
}
```

```anti refused
//! error: `fallthrough` into an arm that binds the fields of `Circle`
variant Shape { Circle { r: f32 }, Empty }

fn radius(s: Shape) -> f32
{
	let r: f32 = 0.0;
	switch s {
		Empty => { fallthrough; },
		Circle c => r = c.r,
	}
	return r;
}
```

### How antic builds it

A `switch` on a variant loads the tag once and compares it with the number of each arm's case, in the order of the arms. An arm that binds copies the struct of its case into a local of the arm before its body runs.

```ir excerpt=match target=linux-x86_64 mode=release
export fn match.area(%0: ptr nonnull deref(size_of match.Shape)) -> f32 {
b0:
    %1 = slot match.Shape.Circle
    %2 = slot match.Shape.Box
    %3 = copy f32 0
    %4 = load i8 %0 range 0, 3
    %5 = eq i8 %4, 0
    branch %5, b1, b2
b1:
    %6 = ptradd %0, offset_of match.Shape.u
    memcopy %1, %6, match.Shape.Circle
    %7 = load f32 %1 !type match.Shape.Circle.r
...
b2:
    %11 = eq i8 %4, 1
    branch %11, b4, b5
```

- `%1` and `%2` are the locals `c` and `b`, one slot per binding arm, each as large as its case.
- `load i8 %0 range 0, 3` reads the tag once. `eq i8 %4, 0` tests for `Circle`, and `b2` goes on to test for `Box`.
- In `b1` the address of `u` is the address of the fields. `memcopy` copies the 4 bytes of `Circle` into `c`, and `c.r` reads from the copy.
- The arm `Empty => { }` has no code of its own.

The switch has no test for a tag outside the cases. The `range` on the load says there is none.

```asm excerpt=match target=linux-x86_64 mode=release
<area>:
movzbl (%rdi), %eax
cmpl $0x1, %eax
je 0x22 <area+0x22>
vxorps %xmm0, %xmm0, %xmm0
testl %eax, %eax
jne 0x21 <area+0x21>
vmovss 0x4(%rdi), %xmm0
vmulss (%rip), %xmm0, %xmm1
vmulss %xmm1, %xmm0, %xmm0
retq
vmovss 0x4(%rdi), %xmm0
vmulss 0x8(%rdi), %xmm0, %xmm0
retq
```

A release build keeps the one load of the tag and two compares. The copies are gone: `vmovss 0x4(%rdi)` reads `r` and `w` from the variant itself, and `vmulss 0x8(%rdi)` reads `h`. `opt` removes a copy that is only read.

Where it lives: `check_variant_arm` of `src/antic/sema_stmt.c` checks an arm and the coverage. `lower_switch`, `arm_test` and `bind_case_fields` of `src/antic/lower_stmt.c` write the tests and the copies. `if_let` of `src/antic/parser_stmt.c` turns `if let` into a `switch` of one arm.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| The tag | one load per `switch` | one load |
| An arm | one compare and one branch, in the order written | a compare tree or a table, as `opt` chooses |
| A binding | one `memcpy` of the fields of the case | nothing when the arm only reads |
| Frame bytes | one slot per binding arm, as large as its case | none when the copy is removed |
| `if let` | a `switch` of one arm: one load, one compare | the same |
| Heap allocations | 0 for a case that owns nothing | 0 |
| Calls into the runtime | 0 for a case that owns nothing | 0 |
| Checks inserted | none, coverage is a rule of the checker | none |

### Pitfalls

For a Java programmer:

- The bound name is a copy. Writing to it does not change the variant, where a pattern variable of Java refers to the object itself.
- `switch` is a statement. It assigns to a variable declared before it and gives no value of its own.
- A missing case is an error at compile time, as for a sealed interface. An `else` arm takes the cases that have no arm.

For a C programmer:

- No arm can read the fields of another case. The binding is the only way to the fields.
- An arm does not fall into the next one. `fallthrough;` asks for it and is refused before an arm that binds.

For a C++ programmer:

- No form reads a case without testing the tag, so nothing throws as `std::get` does.
- `if let Circle c = v` is the counterpart of `std::get_if`, with a copy of the alternative where C++ gives a pointer.
- Adding a case to a variant breaks every `switch` without `else` at compile time. `std::visit` with a generic lambda keeps compiling.
