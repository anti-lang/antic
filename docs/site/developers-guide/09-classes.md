---
title: "Classes"
description: "Classes and the object in memory, construct and destruct, fields and methods, dispatch through interfaces, static fields, singletons, operators and reflection."
summary: "Classes and the object in memory, `construct` and `destruct`, fields and methods, interfaces as inline sub-objects with thunks and the dispatch through them, static fields and singletons, operators, and reflection over the descriptors, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T20:24:00+02:00
draft: true
weight: 90
tags: [compilers, programming-languages]
keywords: [classes, objects, constructors, destructors, virtual dispatch, interfaces, vtable, singletons, operators, reflection]
---

A class is a struct whose first field is a pointer to a table of functions. The fields follow that pointer in C layout, the base class first. A call through a pointer loads the table and one entry. A release build turns that call into a direct one wherever the whole program has a single function for the entry.

## Classes and the object in memory

### What you know

A Java object lives on the heap behind a reference. It starts with a header of 12 or 16 bytes that holds its class and its lock and hash state, and the garbage collector frees it. A C++ object with a virtual function starts with a vtable pointer and lives wherever its variable does. `delete` or the end of its scope frees it. C writes the same thing by hand: a struct whose first member points at a struct of function pointers.

### The feature

`class Name { }` declares a class. Its body holds fields, constants, static fields and functions, and may hold `implements` lines, `use` lines and nested types.

- `class Circle inherits Shape` names the one base in the header. A class without `inherits` has the root `anti.lang.Object` as its base. A base of another module is written `shapes.Shape`.
- `abstract class` is a class with a function that has no body. It has no complete value: no literal, no `alloc`, no variable of its type. It stands behind a pointer, as a base and as an interface.
- `final class` cannot be inherited.
- A field is private to its class unless it is `pub` or `protected`. `pub` is visible everywhere, and `protected` in the class and the classes below it.
- A derived class adds fields after those of its base. It redeclares no field and no function of its chain, apart from a `concrete fn`.

A class is used as a value or behind a pointer, and the program chooses.

- `Circle { r: 2.0 }` is a value. As a local it lives in the frame, and as a field it lies inline in its owner. The literal names fields of the whole chain in any order, and defaults fill the rest. Outside the class it names `pub` fields only.
- `alloc Circle { r: 2.0 }` makes the object on the heap and gives a `*Circle`. `delete(p)` tears it down and frees it. Nothing frees a heap object but `delete`.
- `&c` is a `*Circle`. It converts to `*Shape` and to `*Object` without a cast and without a change of the address, since the base lies at offset 0.
- A value never converts. A `Circle` is no `Shape`, and `c.super` is its base part.

A pointer to a class says what the object is at run time.

- `p is *Box` gives a `bool`.
- `p as *Circle` is checked and ends the program on a mismatch, in dev mode and in release mode.
- `p as? *Box` gives a `?*Box`, `none` on a mismatch.
- `==` on two class pointers compares the identity of the objects.
- `p.type_name()` gives the name of the class. Every class has the seven functions of `Object`: `type_name`, `to_text`, `equals`, `hash`, `serialize`, `copy` and `destruct`.

```anti
//! name: objects
import anti.io;
import anti.text;

export abstract class Shape
{
	pub x: f32 = 0.0,
	pub y: f32 = 0.0,
	protected sides: u8 = 0,

	abstract fn area(self) -> f32;

	pub fn move(self, dx: f32, dy: f32)
	{
		self.x = self.x + dx;
		self.y = self.y + dy;
	}
}

export final class Circle inherits Shape
{
	pub r: f32 = 1.0,
	hits: int = 0,

	concrete fn area(self) -> f32
	{
		self.hits += 1;
		return 3.0 * self.r * self.r;
	}
}

export class Box inherits Shape
{
	pub w: f32 = 1.0,
	pub h: f32 = 1.0,

	concrete fn area(self) -> f32
	{
		return self.w * self.h;
	}
}

class Dot { }

export fn fresh(r: f32) -> *Circle
{
	return alloc Circle { r: r };
}

export fn radius(c: *Circle) -> f32
{
	return c.r;
}

export fn drop(own c: *Circle)
{
	delete(c);
}

export fn is_box(s: *Shape) -> bool
{
	return s is *Box;
}

fn main() -> int
{
	let c = Circle { r: 2.0, x: 1.0 };
	let b = Box { w: 3.0 };
	let p = fresh(4.0);
	let s: *Shape = p;
	let again = s as *Circle;
	let maybe = s as? *Box;
	c.move(1.0, 1.0);
	let line = f"area {c.area()} {b.area()} {s.area()}, x {c.x}, r {radius(p)}, same {s == again}, is_box {is_box(s)} {is_box(&b)}, maybe {maybe == none}, name {s.type_name()}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Dot {size_of(Dot)}, Circle {size_of(Circle)}, Box {size_of(Box)}, *Circle {size_of(*Circle)}";
	io.println(line);
	free(line.ptr);
	drop(p);
	return 0;
}
```
```text output
area 12 3 48, x 2, r 4, same true, is_box false true, maybe true, name Circle
sizes: Dot 8, Circle 40, Box 32, *Circle 8
```

`Dot` has no field and is 8 bytes, its table pointer. `Shape` is the pointer, two `f32` and one `u8`, padded to 24 bytes. `Circle` nests the whole `Shape`, padding included, so `r` stands at offset 24 and `hits` at 32.

The checker refuses each of the following.

```anti refused
//! error: `Shape` is abstract and has no complete value, and a literal needs one
abstract class Shape
{
	abstract fn area(self) -> f32;
}

class Circle inherits Shape
{
	concrete fn area(self) -> f32 { return 3.0; }
}

fn blank() -> f32
{
	let s = Shape { };
	return s.area();
}
```

```anti refused
//! error: `Big` cannot inherit `final` class `Dot`
final class Dot { pub x: int = 0, }

class Big inherits Dot { }
```

```anti refused
//! error: `secret` is private to `Safe`
class Safe { secret: int = 0, }

fn peek(s: *Safe) -> int
{
	return s.secret;
}
```

```anti refused
//! error: `x` is protected in `Base`
class Base { protected x: int = 0, }

class Derived inherits Base { }

fn peek(d: *Derived) -> int
{
	return d.x;
}
```

```anti refused
//! error: the literal of `Point` misses the field `x`
class Point { pub x: int, pub y: int = 0, }

fn origin() -> Point
{
	return Point { };
}
```

```anti refused
//! error: `x` is already a field of `Base`
class Base { pub x: int = 0, }

class Derived inherits Base { pub x: int = 1, }
```

```anti refused
//! error: expected `Base`, found `Derived`
class Base { pub x: int = 0, }

class Derived inherits Base { pub y: int = 0, }

fn slice(d: Derived) -> Base
{
	return d;
}
```

```anti refused
//! error: inherits belongs in the class header: class Circle inherits Shape
class Shape { pub x: int = 0, }

class Circle { pub r: int = 0, inherits Shape, }
```

### How antic builds it

A class is a struct in the IR. Its first field is `super`, the base, and the root is one pointer named `table`.

```ir excerpt=objects target=linux-x86_64 mode=release
type anti.lang.Object = struct { table: ptr }
...
type objects.Shape = struct { super: anti.lang.Object, x: f32, y: f32, sides: i8 }
type objects.Circle = struct { super: objects.Shape, r: f32, hits: i64 }
...
export fn objects.fresh(%0: f32) -> ptr {
b0:
    %1 = call ptr @malloc(size_of objects.Circle)
    %2 = eq i8 %1, 0
    branch %2, b1, b2
b1:
    call void @anti_rt_out_of_memory(size_of objects.Circle)
    unreachable
b2:
    %3 = addr @objects.anti_Circle_vtable
    store ptr %3, %1 !table
    %4 = ptradd %1, offset_of objects.Circle.r
    store f32 %0, %4
```

- The three `type` lines are the whole layout: the table pointer, then the fields of each class down the chain.
- `alloc Circle { r: r }` is a call of `malloc` with the size of the class. A null result ends the program in `anti_rt_out_of_memory`, which is why `alloc` gives a `*Circle` and no `?*Circle`.
- `store ptr %3, %1 !table` writes the address of the table of `Circle` into the first 8 bytes. That store is what makes the memory an object.
- The stores that follow write the named field and then every default of the chain. The last instruction of the function, not shown, is `call void @anti_rt_hook(%1, 256)`, the `created` hook of [chapter 15]({{% relref "15-hooks" %}}). The hook is 0. The bit 256 marks an object at the start of memory of its own on the heap. The count of the objects of a plugin reads it.

The table is one read-only global per class. Its entries are pointers.

```llvm excerpt=objects target=linux-x86_64 mode=dev
define hidden noundef i64 @objects.main() noinline #0 {
...
  %s0 = alloca [40 x i8], align 8
...
  store ptr @anti_Circle_vtable, ptr
...
<{ ptr @anti_Circle_descriptor, ptr @anti_lang_Object_type_name, ptr @anti_lang_Object_to_text, ptr @objects.Circle.equals, ptr @objects.Circle.hash, ptr @anti_lang_Object_serialize, ptr @objects.Circle.destroy, ptr @objects.Circle.copy, ptr @anti_lang_Object_created,
```

The last line is cut from the global `@anti_Circle_vtable`, which holds 19 pointers.

- Entry 0 points at the descriptor of the class, which the section on reflection describes.
- Entries 1 to 7 are the seven functions of `Object`. `equals`, `hash`, `destroy` and `copy` are functions the compiler writes for each class. `type_name`, `to_text` and `serialize` are the root's, in the runtime.
- Entries 8 to 16 are the nine hooks of [chapter 15]({{% relref "15-hooks" %}}), and the first is `created`.
- Entries 17 and 18, at the end of the line, are `Circle_area` and `Shape_move`, the `pub` functions of the chain in the order of their declaration.

`%s0` is the local `c`, 40 bytes in the frame of `main`. A class value is its bytes, with no header before them and no allocation, and the literal stores the same table pointer into it. A class that is not exported names its table `objects.Dot.table`. An exported one has the name its C header declares.

```asm excerpt=objects target=linux-x86_64 mode=release
<fresh>:
...
movl $0x28, %edi
callq 0x15 <fresh+0x15>
testq %rax, %rax
je 0x5c <fresh+0x5c>
leaq (%rip), %rcx
movq %rcx, (%rax)
vmovss -0xc(%rbp), %xmm0
vmovss %xmm0, 0x18(%rax)
...
<is_box>:
movq (%rdi), %rax
movq (%rax), %rax
cmpq $0x2, 0x20(%rax)
jl 0x20 <is_box+0x20>
movq 0x28(%rax), %rax
leaq (%rip), %rcx
cmpq %rcx, 0x10(%rax)
```

`fresh` asks `malloc` for `0x28` bytes, 40, and tests the result. It stores the address of the table at offset 0 and `r` at offset `0x18`, which is 24. The stores of the defaults follow.

`is_box` is `s is *Box` on a `*Shape`. It loads the table, then entry 0, the descriptor. The descriptor holds the depth of the class in its chain at offset `0x20` and an array of its ancestors at `0x28`. `Box` has depth 2, so the test reads ancestor 2 of the object's class and compares it with the descriptor of `Box`. The cost is the same for a chain of any length.

A dev build adds one test before a dispatch, an `is` and an `as`. A table pointer of zero ends the program in `anti_rt_table_unset` with the class name. Memory from `alloc(T, n)` that was never filled has such a pointer.

Where it lives: `lower_class_record` and `table_of` of `src/antic/lower_desc.c` write the tables and the descriptors. `lower_init_function` of `src/antic/lower_class.c` writes the stores of a new object. `anti_rt_delete`, `anti_rt_destroy_from` and `checked_object` of `src/rt/object.c`. `anti_rt_hook` of `src/rt/hooks.c`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of an object | 8 for the table pointer, then the fields of the chain in C layout: `Dot` 8, `Circle` 40 | the same |
| Bytes per class in read-only data | a table of 8 bytes per entry, 17 entries before the first `pub` function, and a descriptor | the same |
| A class value as a local or a field | no allocation, one store of the table pointer, one store per field, one call of `anti_rt_hook` | the same |
| `alloc T { }` | one `malloc`, one test, the same stores, one call of `anti_rt_hook` | the same |
| `delete(p)` | one call of `anti_rt_delete` | the same |
| `&d` to a base pointer | nothing | nothing |
| `p is *T`, `p as? *T` | five loads and two compares, and the test of the table | five loads and two compares |
| `p as *T` | the same and a branch to `anti_rt_cast_failed` | the same |
| Atomic operations | two loads in each call of `anti_rt_hook`: the installed trace handler and the count of loaded libraries | the same |
| Locks | 0 | 0 |
| Checks inserted | the zero table before `is`, `as` and a dispatch | none of those, the check of `as` stays |

The call of `anti_rt_hook` runs once per object made and once per teardown. `--no-hooks` drops it, with the hooks and the leak tracking of [chapter 15]({{% relref "15-hooks" %}}).

### Pitfalls

For a Java programmer:

- `let c = Circle { };` is a value in the frame. `let d = c;` copies it, and the two do not share state. `alloc` gives the object that a Java reference would name.
- Nothing collects a heap object. Each `alloc` has one `delete`.
- An object has 8 bytes before its fields, the table pointer, and no lock word and no hash.

For a C programmer:

- The table pointer is the first field of every class, and the header of a library declares it. A `memset` over an object clears it.
- `as` on a class pointer is a checked conversion. It is no reinterpretation of the bits.

For a C++ programmer:

- A class value passed where its base is expected is refused. Nothing slices.
- Every class has a table, also one with no `pub` function. A type without behaviour is a struct.
- `==` on two pointers compares identity even when one points at an interface part of the object.

## construct and destruct

### What you know

A constructor of C++ runs the constructors of the bases and members first. A destructor runs in the reverse order, also during the unwinding of an exception. A constructor of Java calls `super(...)` as its first statement, and Java has no destructor: `close` is called by hand or by `try` with resources. C writes `init` and `free` functions and calls them by hand.

### The feature

`fn construct(self)` runs after every literal and every `alloc` of the class. The fields hold their defaults and the named values by then.

- A `construct` without arguments cannot fail. The one of a base runs before the one of a derived class, without a call in the source.
- `fn construct(self, args)` takes arguments, and the class is then made with `Circle(args)` as a value or `alloc Circle(args)` on the heap.
- A `construct` that can fail is written `may fail`. It leaves with `fail` and succeeds at its end. The call takes a handler like any failing call. A failure tears down what was built before it, and a failed `alloc` frees the object.
- A class has one `construct`. Another way to make an object is a static function with a name.
- A derived `construct` that wants a base with arguments initialised calls `self.super.construct(args)` as its first statement. `try` forwards a failure of the base.
- Every path that succeeds assigns each field that has no default.

`fn destruct(self)` is the destructor. It cannot fail, and it never calls the one of its base, since the compiler chains them.

`delete(p)` tears a heap object down in this order:

1. the `destroyed` hook,
2. `destruct` of the class, then of each base up the chain,
3. the `own` fields of [chapter 11]({{% relref "11-ownership" %}}) and the inline class fields, each with its own teardown,
4. the memory of the object.

`destroy(&v)` does the first three for a value. A local of a class whose chain declares `destruct` or owns something is torn down at the end of its block, on every exit. The `destruct` of a class does not run on an object whose `construct` failed. What was built before the failure is torn down: every field already set, and each base whose `construct` finished, with its `destruct`.

```anti
//! name: build
import anti.io;
import anti.lang;
import anti.text;

class Base
{
	pub id: int = 7,

	fn construct(self)
	{
		io.println("construct Base");
	}

	fn destruct(self)
	{
		io.println("destruct Base");
	}
}

class Part
{
	pub tag: int = 0,

	fn destruct(self)
	{
		let line = f"destruct Part {self.tag}";
		io.println(line);
		free(line.ptr);
	}
}

class Named inherits Base
{
	pub name: str,
	pub part: Part,
	own buffer: ?*Part = none,

	fn construct(self, name: str) may fail
	{
		if name.len == 0 {
			fail "a name is not empty";
		}
		self.name = name;
		self.part.tag = 1;
		self.buffer = alloc Part { tag: 2 };
		io.println("construct Named");
	}

	fn destruct(self)
	{
		io.println("destruct Named");
	}
}

export class Meter
{
	pub total: int = 0,

	fn construct(self, start: int)
	{
		self.total = start;
	}

	fn destruct(self)
	{
		self.total = 0;
	}
}

export fn meter(start: int) -> *Meter
{
	return alloc Meter(start);
}

export fn end(own m: *Meter)
{
	delete(m);
}

fn scoped()
{
	let n = Named("local") catch fatal;
	io.println("end of scope");
}

fn main() -> int
{
	let p = alloc Named("heap") catch fatal;
	delete(p);
	io.println("--");
	scoped();
	io.println("--");
	let bad = alloc Named("") catch e {
		let line = f"failed: {e.message}";
		io.println(line);
		free(line.ptr);
		return 0;
	};
	delete(bad);
	return 1;
}
```
```text output
construct Base
construct Named
destruct Named
destruct Base
destruct Part 2
destruct Part 1
--
construct Base
construct Named
end of scope
destruct Named
destruct Base
destruct Part 2
destruct Part 1
--
construct Base
destruct Part 0
destruct Base
failed: a name is not empty
```

The third run constructs `Base` and fails in `Named`. The `destruct` of `Named` does not run, since its `construct` did not finish. What was built before it is torn down in reverse order: the fields of `Named` first, of which `part` holds a `Part` and `buffer` holds `none`, and then the base, whose `construct` finished, so `destruct Base` runs. A field of `self` that the `construct` set before its `fail` goes the same way. What a `construct` holds in a local at its `fail` is released by an `undo` of [chapter 4]({{% relref "04-flow" %}}).

The checker refuses each of the following.

```anti refused
//! error: `Size` has one `construct`, and every other maker is a static function
class Size
{
	pub w: int = 0,

	fn construct(self, w: int) { self.w = w; }

	fn construct(self) { }
}
```

```anti refused
//! error: `construct` of `Size` returns `none` before it sets `w`
class Size
{
	pub w: int,

	fn construct(self, w: int)
	{
		if w > 0 {
			self.w = w;
		}
	}
}
```

```anti refused
//! error: `self.super.construct` is called at the top of the body of `construct`
class Base
{
	pub id: int = 0,

	fn construct(self, id: int) { self.id = id; }
}

class Derived inherits Base
{
	pub extra: int = 0,

	fn construct(self, id: int)
	{
		self.extra = id;
		self.super.construct(id);
	}
}
```

```anti refused
//! error: `Size.construct` may fail and its error is not handled
class Size
{
	pub w: int = 0,

	fn construct(self, w: int) may fail
	{
		if w < 0 {
			fail "negative";
		}
		self.w = w;
	}
}

fn make() -> int
{
	let s = Size(1);
	return s.w;
}
```

```anti refused
//! error: `Size` has no `construct` with arguments, so a literal builds it
class Size { pub w: int = 0, }

fn make() -> int
{
	let s = Size(1);
	return s.w;
}
```

```anti refused
//! error: `destruct` cannot fail
class Size
{
	pub w: int = 0,

	fn destruct(self) may fail { }
}
```

### How antic builds it

`alloc Meter(start)` is the allocation and the stores of the last section, with a direct call of `construct` before the `created` hook.

```ir excerpt=build target=linux-x86_64 mode=release
export fn build.meter(%0: i64) -> ptr {
b0:
    %1 = call ptr @malloc(size_of build.Meter)
...
    %3 = addr @build.anti_Meter_vtable
    store ptr %3, %1 !table
    %4 = ptradd %1, offset_of build.Meter.total
    store i64 0, %4
    call void @build.Meter.construct(%1, %0)
    call void @anti_rt_hook(%1, 256)
    ret ptr %1
}
export fn build.end(%0: ptr nonnull deref(size_of build.Meter) own) {
b0:
    %1 = addr @build.anti_Meter_descriptor
    call void @anti_rt_delete(%0, %1)
```

`construct` is an ordinary function with `self` as its first parameter. It is called directly: the class of a new object is known where it is made. The default 0 of `total` is stored before the call, although this `construct` overwrites it.

`delete(m)` is one call, `anti_rt_delete` with the object and the descriptor of its static class. The runtime reads the table of the object, which may belong to a derived class, and calls entry 6. That entry holds a function the compiler writes for every class, here `Named.destroy` of the same program.

```ir excerpt=build target=linux-x86_64 mode=release
fn build.Named.destroy(%0: ptr, %1: ptr) {
b0:
    call void @anti_rt_hook(%0, 1)
    call void @build.Named.destruct(%0)
    call void @build.Base.destruct(%0)
    %2 = ptradd %0, offset_of build.Named.buffer
    %3 = load ptr %2
    %4 = addr @build.Part.descriptor
    call void @anti_rt_delete_from(%3, %4, %1)
    store ptr 0, %2
    %5 = ptradd %0, offset_of build.Named.part
...
    call void @build.Part.destroy(%5, %1)
```

- `anti_rt_hook(%0, 1)` is the `destroyed` hook.
- The two `destruct` calls are the chain, the class first and its base second. Both are direct.
- The `own` field `buffer` goes through `anti_rt_delete_from`, which tears the `Part` down and gives its memory back. `%1` is the allocator, null for the C library.
- The inline field `part` is torn down in place by the `destroy` function of its class.

The same function serves a local. At the end of the block of `scoped`, the compiler calls the teardown of `n` with no free at the end.

Where it lives: `lower_run_construct_bodies` of `src/antic/lower_class.c` calls the chain of `construct`. `teardown_field`, `lower_destroy_owned` and `lower_class_copy` of `src/antic/lower_owning.c` write the functions `destroy` and `copy` of each class and the teardown of locals. `anti_rt_delete_from` and `anti_rt_destroy_from` of `src/rt/object.c`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A `construct` | one direct call per class of the chain that declares one | the same, or inlined |
| `alloc T(args)` | one `malloc`, the stores, the calls of `construct`, one `anti_rt_hook` | the same |
| A failed `alloc T(args)` | one `free` and the path of the error | the same |
| `delete(p)` | one `anti_rt_delete`, one indirect call of the class teardown, one `anti_rt_hook`, one direct call per `destruct` of the chain, one `free` | the same |
| Each `own` pointer field | one `anti_rt_delete_from` | the same |
| Each inline class field | one direct call of its teardown | the same |
| The end of a block with a class local that owns something | the teardown without the `free` | the same |
| Heap allocations of a class value | 0 | 0 |
| Checks inserted | `anti_rt_delete` ends the program on a zero table, in both modes | the same |

A class whose chain declares no `destruct` and owns nothing has no teardown at the end of a block, and its local costs nothing there.

### Pitfalls

For a Java programmer:

- `destruct` runs at a known point: at `delete`, or at the end of the block of a value. No finalizer runs later.
- A `construct` with arguments is called as `Circle(2.0)`, without `new`. `alloc Circle(2.0)` is the heap form.
- A base `construct` with arguments is not called unless the derived one calls it.

For a C programmer:

- `delete(p)` frees what the object owns through its `own` fields. A second `free` of such a field is a double free.
- `alloc T { }` never gives null, so no test follows it.

For a C++ programmer:

- A failed `construct` runs the destructor of each base whose `construct` completed and tears down the fields already set, as in C++. The destructor of the failing class itself does not run. An `undo` releases what the failing path holds in a local.
- The teardown runs `destruct` of the class and its bases first and the fields after them. C++ destroys the members of a class before its bases.
- No copy constructor exists. `=` copies a class value that owns nothing and is refused for one that owns something, where `dup` copies.

## Fields and methods

### What you know

A method of Java is virtual unless it is `private`, `static` or `final`, and the virtual machine removes the dispatch where it sees one target. A member function of C++ is direct unless it is declared `virtual`. In both, `this` is implicit, and a field is named without it. C passes the struct as an explicit first argument.

### The feature

A function of a class body takes `self`, a `*Class`, as its first parameter. A field is always written `self.x`: a bare `x` in a body is a local, a parameter, a constant of the class or an item of the module.

- `pub fn` has an entry in the table of the class and is visible everywhere.
- `fn` without a marker is private to the class, and `protected fn` reaches the classes below. Neither has a table entry, and both are called directly.
- `abstract fn f(self) -> R;` has no body and is `pub`. It takes `self`, since an object reaches it through its table. A class with one is an `abstract class`.
- `concrete fn f(self)` replaces an inherited entry. The word is required: a function that matches an inherited one without it is refused, and so is a `concrete fn` that matches nothing.
- `pub final fn` cannot be replaced, so every call of it is direct.
- A function without `self` is static and is called as `Circle.unit()`.
- `const NAME: T = e;` is a constant of the class. Inside the class its bare name `UNIT` reaches it: in another constant, the type and the default of a field, a type nested in the class and every function. Outside the class, and in a class that inherits it, it is `Shape.UNIT`. A local or a parameter of the same name wins, and the constant hides an item of the module of the same name. `self.UNIT` is refused.
- `self.super.f(args)` calls the body of the base, directly.
- `let f = c.scaled;` is a bound function, two words, the object and the entry. `f()` calls it.

A field takes a default, `pub r: f32 = 1.0`, a constant expression that every literal applies when it omits the field. A field may be a bitfield. `own`, `atomic` and `transient` are markers of chapters [11]({{% relref "11-ownership" %}}) and [13]({{% relref "13-threads" %}}).

`v.f(args)` looks in the class, then up its chain, then in its `use` fields, then in the module. `use part: T,` is a field whose `pub` members are reachable on the class, so `car.start()` is `car.engine.start()`. The class does not convert to `*T`.

A class body may declare a `struct`, an `enum` or a `class`. Only the class names the nested type, and a public signature that names it is refused. The class and the classes nested in it reach the private and the protected members of each other. Java gives the classes inside one class the same.

```anti
class Engine
{
	pub power: int = 90,

	pub fn start(self) -> int { return self.power; }
}

class Base
{
	pub fn describe(self) -> int { return 1; }
}

class Car inherits Base
{
	struct Wheel { size: int }

	use engine: Engine,
	spare: Wheel = Wheel { size: 16 },

	concrete fn describe(self) -> int
	{
		return self.super.describe() + self.spare.size;
	}
}

fn drive(c: *Car) -> int
{
	return c.start() + c.power + c.describe();
}
```

`Car` reaches `start` and `power` of its `use` field by their bare names. `Wheel` is `Car.Wheel` in the symbols, and no other class names it. `self.super.describe()` calls the body of `Base`.

```anti
//! name: calls
import anti.io;
import anti.text;

export abstract class Shape
{
	pub scale: f32 = 1.0,
	protected const UNIT: f32 = 1.0;

	abstract fn area(self) -> f32;

	pub fn scaled(self) -> f32
	{
		return self.area() * self.scale;
	}

	pub final fn doubled(self) -> f32
	{
		return 2.0 * self.scale;
	}
}

export class Circle inherits Shape
{
	pub r: f32 = 1.0,

	concrete fn area(self) -> f32
	{
		return 3.0 * self.square();
	}

	fn square(self) -> f32
	{
		return self.r * self.r;
	}

	pub fn unit() -> Circle
	{
		return Circle { r: Shape.UNIT };
	}
}

export class Box inherits Shape
{
	pub w: f32 = 1.0,
	pub h: f32 = 1.0,

	concrete fn area(self) -> f32
	{
		return self.w * self.h;
	}
}

export abstract class Counter
{
	pub count: int = 0,

	abstract fn step(self) -> int;
}

export class Up inherits Counter
{
	concrete fn step(self) -> int
	{
		self.count += 1;
		return self.count;
	}
}

export fn any_area(s: *Shape) -> f32
{
	return s.area();
}

export fn twice(s: *Shape) -> f32
{
	return s.doubled();
}

export fn circle_area(c: *Circle) -> f32
{
	return c.area();
}

export fn advance(c: *Counter) -> int
{
	return c.step();
}

fn main() -> int
{
	let c = Circle.unit();
	c.r = 2.0;
	c.scale = 0.5;
	let b = Box { w: 3.0, h: 4.0 };
	let u = Up { };
	let bound = c.scaled;
	let first = advance(&u);
	let line = f"area {any_area(&c)} {any_area(&b)} {circle_area(&c)}, scaled {c.scaled()} {bound()}, twice {twice(&b)}, step {first} {advance(&u)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```
```text output
area 12 12 12, scaled 6 6, twice 2, step 1 2
```

The checker refuses each of the following.

```anti refused
//! error: unknown name `r`
class Circle
{
	pub r: f32 = 1.0,

	pub fn twice(self) -> f32 { return r * 2.0; }
}
```

```anti refused
//! error: `Loud.speak` replaces `Animal.speak` and needs `concrete`
class Animal
{
	pub fn speak(self) -> int { return 1; }
}

class Loud inherits Animal
{
	pub fn speak(self) -> int { return 2; }
}
```

```anti refused
//! error: `Loud.run` is `concrete` and replaces nothing
class Animal { pub legs: int = 4, }

class Loud inherits Animal
{
	concrete fn run(self) -> int { return 2; }
}
```

```anti refused
//! error: `Loud.speak` replaces `final` function `Animal.speak`
class Animal
{
	pub final fn speak(self) -> int { return 1; }
}

class Loud inherits Animal
{
	concrete fn speak(self) -> int { return 2; }
}
```

```anti refused
//! error: `helper` is private to `Animal`
class Animal
{
	fn helper(self) -> int { return 1; }
}

fn poke(a: *Animal) -> int
{
	return a.helper();
}
```

```anti refused
//! error: expected `*Engine`, found `*Car`
class Engine
{
	pub fn start(self) -> int { return 1; }
}

class Car { use engine: Engine, }

fn engine_of(c: *Car) -> *Engine
{
	return c;
}
```

### How antic builds it

A field is an offset in the struct of the class, read as a struct field is. A call takes one of two forms.

```ir excerpt=calls target=linux-x86_64 mode=dev
export fn calls.any_area(%0: ptr nonnull deref(size_of calls.Shape)) -> f32 {
b0:
    %1 = load ptr %0 !table
    %2 = eq i8 %1, 0
    branch %2, b1, b2
b1:
    %3 = addr @calls.4
    call void @anti_rt_table_unset(%3, 5)
    unreachable
b2:
    %4 = mul i64 17, size_of ptr
    %5 = ptradd %1, %4
    %6 = load ptr %5 !entry
    %7 = call f32 %6 via @calls.fn.0(%0) table @calls.anti_Shape_descriptor 17
    ret f32 %7
}
export fn calls.twice(%0: ptr nonnull deref(size_of calls.Shape)) -> f32 {
b0:
    %1 = call f32 @calls.Shape.doubled(%0)
```

`any_area` calls `s.area()` on a `*Shape`. `area` is `pub` and not final, so the call goes through the table.

- `load ptr %0 !table` reads the table pointer from the object.
- The compare with 0 and the block `b1` are the dev check of the last section.
- `17 * size_of ptr` is the offset of the entry. `area` is the first `pub` function of the chain. The descriptor, the seven functions of the root and the nine hooks come before it.
- `load ptr %5 !entry` reads the function, and the call passes the object as `self`. The words after the arguments record the class and the slot for the whole-program pass.

`twice` calls a `final` function. It is one direct call in dev mode too, with no load.

A release build runs the whole-program pass over the IR of every module. A call through a table becomes direct when every table that can stand behind the static class holds one function at that slot.

```asm excerpt=calls target=linux-x86_64 mode=release
<any_area>:
movq (%rdi), %rax
jmpq *0x88(%rax)
...
<twice>:
vmovss 0x8(%rdi), %xmm0
vaddss %xmm0, %xmm0, %xmm0
retq
...
<circle_area>:
vmovss 0x10(%rdi), %xmm0
vmulss %xmm0, %xmm0, %xmm0
vmulss (%rip), %xmm0, %xmm0
retq
...
<advance>:
movq 0x8(%rdi), %rax
incq %rax
movq %rax, 0x8(%rdi)
retq
```

- `any_area` keeps its dispatch, since `Circle` and `Box` both fill `area`. It is two instructions: the load of the table and a jump through the entry at `0x88`, which is 17 times 8.
- `twice` has the body of `doubled` in place.
- `circle_area` calls `area` on a `*Circle`. No class below `Circle` replaces it, so the call is direct, and `opt` inlines `area` and the private `square` into three instructions.
- `advance` calls `step` on a `*Counter`, an abstract class. `Up` is the only class of the program that fills `step`, so the pass binds the call to `Up.step`, and the increment stands in place.

The pass does not run in dev mode, which compiles one module at a time and relies on `final`. It also leaves the calls alone in a plugin and in a program that loads a plugin or injects a provider. A class the compiler never saw may fill an entry there. `--closed` builds a program that loads no plugin, and the pass runs for it.

Where it lives: `lower_load_table` of `src/antic/lower_expr.c` loads the table and writes the dev check. `devirtualise` of `src/antic/whole.c`. `docs/notes/whole-program.md` holds the choices of the pass.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A field read or write | one load or store at a constant offset | one instruction |
| A call of a private, protected, static or `final` function | one direct call | one direct call, or inlined |
| A call of a `pub` function through a pointer | two loads, one test of the table, one indirect call | two loads and one indirect call, or a direct call when one function fills the slot |
| A call of a `pub` function on a value | one direct call, since the class of a value is known | the same, or inlined |
| A bound function `c.scaled` | two words in the frame | the same |
| Bytes per `pub` function | 8 in the table of every class of the chain below | the same |
| Heap allocations, calls into the runtime | 0 | 0 |
| Checks inserted | the zero table before a dispatch | none |

### Pitfalls

For a Java programmer:

- `self` is written, in the parameter list and before every field.
- A function is private unless it says `pub`. Java's default is visible in the package.
- Replacing a function takes `concrete`. A function with the name of an inherited one and no `concrete` is an error, where Java overrides without a word.
- A constant of the base is `Shape.UNIT` in a derived class. Java reaches an inherited `static final` field by its bare name.

For a C programmer:

- The first parameter `self` is the struct pointer a C function would take. `Class.f(&v, args)` is the same call without the method form.

For a C++ programmer:

- Every `pub` function is virtual in dev mode. `final` makes a call direct there, and a release build finds the direct calls itself.
- A `use` field gives the names of a member without a conversion to it. Private inheritance is the nearest C++ form.
- A release build may bind a call through an abstract class to its one implementation. Adding a second implementation brings the dispatch back.

## Interfaces and dispatch

### What you know

A Java class implements any number of interfaces, and a call through an interface reference searches the class for the method, with caches in front. A C++ class inherits more than one base, each with its own vtable pointer inside the object. A pointer to a second base points into the middle of the object, and a thunk adjusts `this` on a virtual call. C has no interfaces, and a struct of function pointers with a context pointer stands in.

### The feature

An interface is an abstract class. It may hold fields, constants and functions with bodies beside its abstract functions, and Anti has no other interface form.

`implements name: Interface,` in a class body places a sub-object of the interface inside the class, at a field with that name. A class implements any number of interfaces, each once.

- `concrete fn draw(self)` fills the entry `draw` of every table of the class that has one.
- `concrete fn Drawable::draw(self)` fills the table of that interface alone. Two interfaces that declare one name with different signatures take two qualified bodies.
- The fields of the interface are reached as `self.dr.layer`, and its `pub` members also directly on the class, as `c.above(1)`.
- `&c` converts to `*Drawable` without a cast.
- `d is *Circle`, `d as *Circle` and `d as? *Circle` lead from an interface pointer back to the class.
- `==` on two pointers into one object is true, whichever parts they point at.

```anti
//! name: faces
import anti.io;
import anti.text;

export abstract class Drawable
{
	pub layer: int = 0,

	abstract fn draw(self) -> int;

	pub fn above(self, other: int) -> bool
	{
		return self.layer > other;
	}
}

export abstract class Named
{
	abstract fn rank(self) -> int;
}

export class Circle
{
	implements dr: Drawable,
	implements nm: Named,

	pub r: int = 1,

	concrete fn draw(self) -> int
	{
		return self.r * 2;
	}

	concrete fn rank(self) -> int
	{
		return self.r + 100;
	}
}

export class Square
{
	implements dr: Drawable,

	pub side: int = 1,

	concrete fn draw(self) -> int
	{
		return self.side * 4;
	}
}

export fn render(d: *Drawable) -> int
{
	return d.draw();
}

export fn rank_of(n: *Named) -> int
{
	return n.rank();
}

export fn as_named(c: *Circle) -> *Named
{
	return c;
}

fn main() -> int
{
	let c = Circle { r: 5 };
	let s = Square { side: 3 };
	c.dr.layer = 2;
	let d: *Drawable = &c;
	let n = as_named(&c);
	let back = d as *Circle;
	let line = f"render {render(d)} {render(&s)}, rank {rank_of(n)}, direct {c.draw()}, above {d.above(1)} {c.above(5)}, same {d == n}, back {back.r}, is {d is *Square}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Drawable {size_of(Drawable)}, Named {size_of(Named)}, Circle {size_of(Circle)}, Square {size_of(Square)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```
```text output
render 10 12, rank 105, direct 10, above true false, same true, back 5, is false
sizes: Drawable 16, Named 8, Circle 40, Square 32
```

`Circle` is its own table pointer, the 16 bytes of `Drawable`, the 8 bytes of `Named` and its field `r`. It holds three table pointers.

The checker refuses each of the following.

```anti refused
//! error: `Circle` lacks `concrete fn draw` of `Drawable`
abstract class Drawable
{
	abstract fn draw(self) -> int;
}

class Circle
{
	implements dr: Drawable,

	pub r: int = 1,
}
```

```anti refused
//! error: `Point` is not abstract and cannot be implemented
class Point { pub x: int = 0, }

class Circle { implements at: Point, }
```

```anti refused
//! error: `Circle` implements `Drawable` twice
abstract class Drawable
{
	abstract fn draw(self) -> int;
}

class Circle
{
	implements first: Drawable,
	implements second: Drawable,

	concrete fn draw(self) -> int { return 1; }
}
```

```anti refused
//! error: `concrete fn go` returns `int`, and `Flag.go` returns `bool`
abstract class Step
{
	abstract fn go(self) -> int;
}

abstract class Flag
{
	abstract fn go(self) -> bool;
}

class Both
{
	implements s: Step,
	implements f: Flag,

	concrete fn go(self) -> int { return 1; }
}
```

```anti refused
//! error: `Both` fills `go` only for `Step` and `Stride`, so the call is ambiguous, reach it through an interface
abstract class Step
{
	abstract fn go(self) -> int;
}

abstract class Stride
{
	abstract fn go(self) -> int;
}

class Both
{
	implements s: Step,
	implements t: Stride,

	concrete fn Step::go(self) -> int { return 1; }

	concrete fn Stride::go(self) -> int { return 2; }
}

fn walk(b: *Both) -> int
{
	return b.go();
}
```

### How antic builds it

The sub-object of an interface is a field of the class struct, with the layout of the interface: its table pointer and its fields.

```ir excerpt=faces target=linux-x86_64 mode=release
type faces.Circle = struct { super: anti.lang.Object, dr: faces.Drawable, nm: faces.Named, r: i64 }
...
export fn faces.render(%0: ptr nonnull deref(size_of faces.Drawable)) -> i64 {
b0:
    %1 = load ptr %0 !table
    %2 = mul i64 17, size_of ptr
    %3 = ptradd %1, %2
    %4 = load ptr %3 !entry
    %5 = call i64 %4 via @faces.fn.0(%0) table @faces.anti_Drawable_descriptor 17
...
export fn faces.as_named(%0: ptr nonnull deref(size_of faces.Circle)) -> ptr {
b0:
    %1 = ptradd %0, offset_of faces.Circle.nm
    ret ptr %1
...
fn faces.Circle.dr.draw.thunk(%0: ptr nonnull) -> i64 {
b0:
    %1 = sub i64 0, offset_of faces.Circle.dr
    %2 = ptradd %0, %1
    %3 = call i64 @faces.Circle.draw(%2)
```

- `Circle` holds `dr` at offset 8 and `nm` at offset 24, each a complete `Drawable` or `Named` with a table pointer of its own.
- `as_named` is the conversion of `*Circle` to `*Named`: the address of the object plus the offset of `nm`. The pointer points into the middle of the object.
- `render` dispatches through a `*Drawable` exactly as the last section dispatches through a class pointer. The caller knows nothing of `Circle`. It loads the table that the sub-object points at and calls entry 17.
- That table belongs to `Circle`, and its entry 17 is the thunk. The thunk subtracts the offset of `dr` from the pointer, which gives the address of the `Circle`, and calls `Circle.draw` with it. `draw` is compiled once and serves the direct call and the interface call.

Each class has one table per interface it implements, beside its own.

```llvm excerpt=faces target=linux-x86_64 mode=dev
define hidden noundef i64 @faces.Circle.dr.draw.thunk(ptr noundef nonnull %p0) #0 {
...
  %v1 = getelementptr i8, ptr %v0, i64 -8
...
  %v3 = call i64 (ptr) @Circle_draw(ptr %v2)
...
@Circle_dr_table = dso_local constant
...
@Circle_nm_table = dso_local constant
```

`Circle_dr_table` has the 17 entries every table starts with and the two `pub` functions of `Drawable`. Its first 16 function entries point at thunks as well, so that `d.type_name()` or the teardown of an object through a `*Drawable` reaches the whole `Circle`. Its last entry is `Drawable_above`, the body the interface itself supplies, which takes the sub-object and needs no thunk. Entry 0 points at a descriptor of the sub-object that records the offset 8, which `is`, `as` and `==` use to find the enclosing object.

```asm excerpt=faces target=linux-x86_64 mode=release
<render>:
movq (%rdi), %rax
jmpq *0x88(%rax)
...
<rank_of>:
movq 0x8(%rdi), %rax
addq $0x64, %rax
retq
...
<as_named>:
leaq 0x18(%rdi), %rax
retq
...
<faces.Circle.dr.draw.thunk>:
movq 0x18(%rdi), %rax
addq %rax, %rax
retq
```

- `render` is the two instructions of any dispatch, since `Circle` and `Square` both implement `Drawable`.
- The thunk of a release build has the body of `draw` inlined. It reads `r` at `0x18` from the sub-object, which is offset 32 of the `Circle`, and doubles it. The interface call costs one indirect jump and no second call.
- `Circle` is the only class that implements `Named`, so the whole-program pass binds `n.rank()` in `rank_of` to its thunk. `opt` then inlines it into one load at offset 8 of the sub-object and one add.
- `as_named` is one `leaq`.

On arm64 `render` is `ldr x8, [x0]`, `ldr x1, [x8, #0x88]` and `br x1`.

Where it lives: `lower_interface_thunk` of `src/antic/lower_class.c` writes a thunk, and `lower_class_record` of `src/antic/lower_desc.c` the table of each interface. `devirtualise` of `src/antic/whole.c`. `checked_object` of `src/rt/object.c` goes from a sub-object to its object.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes per implemented interface, per object | 8 for its table pointer, and its fields | the same |
| Bytes per implemented interface, per class | one table, one descriptor, and one thunk per function entry that is not the interface's own body | the same, less what nothing reaches |
| `&c` to an interface pointer | one add of a constant | one instruction, or folded |
| A call through an interface pointer | two loads, one test, one indirect call to the thunk, one direct call from it | two loads and one indirect jump into a thunk with the body inlined |
| The same call with one implementing class in the program | the same as above | a direct call, or inlined |
| A call of a function the interface supplies | two loads, one test, one indirect call, no thunk | the same without the test, or direct |
| Heap allocations, calls into the runtime | 0 | 0 |
| Checks inserted | the zero table before the dispatch | none |

A call through an interface pointer takes two loads and an indirect transfer, as a call through a class pointer does. The work that tells the classes apart was done when the table was written.

### Pitfalls

For a Java programmer:

- An interface pointer is a different address than the object pointer. `==` still answers whether two pointers name one object.
- An interface may hold fields, and each implementing object carries them.
- Each implemented interface adds 8 bytes to every object of the class.

For a C programmer:

- A `*Drawable` cannot be cast to its class with pointer arithmetic by hand. `d as *Circle` reads the offset from the descriptor.

For a C++ programmer:

- `implements` is a named member. `self.dr.layer` says which sub-object a field belongs to, so two interfaces with a field of one name do not clash.
- No virtual inheritance exists. Implementing one interface twice, directly or through the chain, is refused.
- A release build of a whole program devirtualises across modules. A plugin host keeps every dispatch.

## Static fields and singletons

### What you know

A `static` field of Java or C++ is one variable per class, shared by every thread, and safe only when the program guards it. C++ initialises a function-local static on first use behind a guard, and statics at namespace level in an order that is unspecified across files. A singleton is written by hand in both, with a private constructor and an accessor.

### The feature

`static atomic count: int = 0;` declares a field of the class itself, reached as `Class.count`. A static field is always `atomic`: Anti has no mutable global that a thread could write without one.

An atomic field has no `=` and no `+=`, and it is not read by its name alone. It has these operations, all sequentially consistent.

| Operation | Result |
|---|---|
| `load()` | the value |
| `store(v)` | nothing |
| `add(v)`, `sub(v)`, `and(v)`, `or(v)`, `swap(v)` | the value the field held before |
| `compare_swap(expected, new)` | `true` when the field held `expected` and now holds `new` |

`add` and `sub` take an integer field. `and` and `or` take an integer or a `bool`. A pointer field takes `load`, `store`, `swap` and `compare_swap` alone.

`atomic` also marks an instance field and a local, which [chapter 13]({{% relref "13-threads" %}}) covers.

`singleton class Config { }` declares a class with one instance. `Config.get()` gives a pointer to it and creates it at the first call, from the defaults and from `construct` when the class declares one.

- The program writes no literal and no `alloc` of a singleton, and `delete` of it is refused.
- Its `construct` takes no parameters, and the class declares no `get`, which the compiler writes.
- No class inherits a singleton, and a singleton is never copied: `dup(Config.get())` and `*Config.get()` are refused.
- A field without a marker is read-only after creation. `construct` sets it, and nothing else does.
- An `atomic` field has the operations above.
- A `mutable` field is an ordinary field that the program writes anywhere, except in a `worker fn` of [chapter 13]({{% relref "13-threads" %}}) and in what one calls. The whole-program pass checks that in every build.

```anti
//! name: statics
import anti.io;
import anti.text;

export class Ticket
{
	pub number: int = 0,
	pub static atomic issued: int = 0;

	fn construct(self)
	{
		self.number = Ticket.issued.add(1) + 1;
	}
}

singleton class Config
{
	pub path: str = "config.toml",
	pub atomic requests: int = 0,
	pub mutable title: str = "untitled",
	pub limit: int = 0,

	fn construct(self)
	{
		self.limit = 10;
	}
}

export fn issued() -> int
{
	return Ticket.issued.load();
}

export fn issue() -> int
{
	return Ticket.issued.add(1);
}

fn limit() -> int
{
	return Config.get().limit;
}

fn main() -> int
{
	let a = Ticket { };
	let b = Ticket { };
	let before = issue();
	let swapped = Ticket.issued.compare_swap(3, 10);
	let c = Config.get();
	c.requests.add(2);
	c.title = "guide";
	let line = f"numbers {a.number} {b.number}, before {before}, issued {issued()}, swapped {swapped}, limit {limit()}, requests {Config.get().requests.load()}, title {c.title}, one {c == Config.get()}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Ticket {size_of(Ticket)}, Config {size_of(Config)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```
```text output
numbers 1 2, before 2, issued 10, swapped true, limit 10, requests 2, title guide, one true
sizes: Ticket 16, Config 56
```

The static field adds nothing to a `Ticket`, which is its table pointer and `number`.

The checker refuses each of the following.

```anti refused
//! error: `count` is `static` and must be `atomic`
class Ticket
{
	pub number: int = 0,
	static count: int = 0;
}
```

```anti refused
//! error: `count` is atomic, so it is read with `load()` and written with `store(v)`
class Ticket
{
	pub number: int = 0,
	pub static atomic count: int = 0;
}

fn total() -> int
{
	return Ticket.count;
}
```

```anti refused
//! error: `Config` is a singleton, and `Config.get()` gives its one instance
singleton class Config { pub limit: int = 0, }

fn second() -> int
{
	let c = Config { };
	return c.limit;
}
```

```anti refused
//! error: `limit` of singleton `Config` is read-only after creation, and `mutable` marks a field the program writes
singleton class Config { pub limit: int = 0, }

fn raise()
{
	Config.get().limit = 1;
}
```

```anti refused
//! error: `Config` is a singleton and outlives the program
singleton class Config { pub limit: int = 0, }

fn close()
{
	delete(Config.get());
}
```

### How antic builds it

A static field is a global of the module, named after its class, in the writable data of the program. An operation on it is one atomic instruction of LLVM.

```llvm excerpt=statics target=linux-x86_64 mode=dev
define internal noundef i64 @issued() #0 {
...
  %v1 = load atomic i64, ptr %v0 seq_cst, align 8
...
define internal noundef i64 @issue() #0 {
...
  %v1 = atomicrmw add ptr %v0, i64 1 seq_cst, align 8
...
@statics.Ticket.issued = hidden global <{ [8 x i8] }> zeroinitializer, align 8
...
@statics.Config.instance = hidden global <{ [8 x i8] }> zeroinitializer, align 8
```

- `Ticket.issued` is 8 bytes, zero at start, the one mutable global that `Ticket` adds.
- `load()` is `load atomic` and `add(1)` is `atomicrmw add`, both `seq_cst`. The IR of antic writes the two as calls of `anti_rt_atomic_load` and `anti_rt_atomic_add`, which the emitter replaces with the instructions. The runtime keeps the functions for C code.
- A singleton has a second kind of global, `Config.instance`, one pointer that is null until the first `Config.get()`.

```asm excerpt=statics target=linux-x86_64 mode=release
<issued>:
movq (%rip), %rax
retq
...
<issue>:
movl $0x1, %eax
lock
xaddq %rax, (%rip)
retq
```

On x86-64 an aligned load is atomic as it stands, so `load()` is a plain `movq`. `add(1)` is `lock xaddq`, which adds and gives back the old value. On linux-arm64, whose default level is `armv8.0`, the add is a loop of `ldaxr` and `stlxr`.

The compiler writes `Config.get()` for the singleton.

```ir excerpt=statics target=linux-x86_64 mode=release
fn statics.Config.get() -> ptr {
b0:
    %0 = addr @statics.Config.instance
    %1 = call ptr @anti_rt_atomic_load(%0, size_of ptr)
    %2 = copy ptr %1
    %3 = eq i8 %1, 0
    branch %3, b1, b3
b1:
    %4 = call ptr @malloc(size_of statics.Config)
...
b2:
    call void @free(%4)
    %16 = call ptr @anti_rt_atomic_load(%0, size_of ptr)
...
    call void @statics.Config.construct(%4)
    call void @anti_rt_hook(%4, 256)
    %2 = copy ptr %4
    %15 = call i8 @anti_rt_atomic_compare_swap(%0, size_of ptr, 0, %4)
    branch %15, b3, b2
```

- Every call starts with an atomic load of the instance pointer. When it is set, the function returns it: `b3`.
- The first call takes `b1`. It allocates the object, stores the table and the defaults, runs `construct` and the `created` hook, and then publishes the pointer with one `compare_swap` against null.
- A thread that loses that race takes `b2`: it frees its own object and loads the winner's. Two threads may therefore both run `construct`, and one of the two objects is dropped without a `destruct`.

The instance is never freed. The read-only rule of the plain fields is a rule of the checker and has no code.

Where it lives: `lower_static_global` of `src/antic/lower_place.c` and `lower_singleton_get` of `src/antic/lower_class.c`. `check_singletons` of `src/antic/whole.c` checks the `mutable` fields against the workers. `src/rt/atomic.c` holds the functions for C.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of a static field | its type, once per program, 0 per object | the same |
| `load()` | one atomic load | one `movq` on x86-64 |
| `store(v)`, `add(v)`, `swap(v)` and the like | one atomic instruction | one locked instruction on x86-64 |
| `compare_swap` | one `cmpxchg` | one `lock cmpxchg` on x86-64 |
| `Config.get()` after the first call | one call, one atomic load, one compare | the same, or inlined |
| The first `Config.get()` | one `malloc`, the stores, `construct`, one `anti_rt_hook`, one `compare_swap` | the same |
| A read of a singleton field | `Config.get()` and one load | the same |
| Locks | 0 | 0 |
| Checks inserted | none | none |

Every access of a static field is an atomic operation, also in a program with one thread. A locked instruction orders the memory of all cores and costs more than a plain one. No time for it stands here, since `tests/bench` holds no program that measures it.

### Pitfalls

For a Java programmer:

- A static field is always atomic, with the meaning of `AtomicLong`. `Ticket.issued += 1` does not compile, and `add(1)` is the form.
- `add` gives the value before the addition, as `getAndAdd` does.
- A singleton is created at the first `get()`. No class loading runs a static initialiser.

For a C programmer:

- Anti has no plain global variable that a function writes. A static atomic field or a `mutable` field of a singleton takes its place.

For a C++ programmer:

- No static initialisation order exists, since a static field starts from a constant.
- `Config.get()` does not lock. A `construct` of a singleton may run twice when two threads race for the first call, so it must have no effect outside the object.
- A singleton has no destructor run at exit.

## Operators

### What you know

C++ overloads nearly every operator, as a member or as a free function, and finds the function by the types of both operands. Java overloads none: `a.add(b)` and `a.equals(b)` are the forms, and `==` on two references compares identity. C has no operators on structs beside `=`.

### The feature

`operator fn` maps an operator to a function with a fixed name. It is `pub`.

| Operator | Function | Note |
|---|---|---|
| `a + b`, `a - b`, `a * b`, `a / b`, `a % b` | `add`, `sub`, `mul`, `div`, `rem` | |
| `-a`, `~a` | `neg`, `not` | one operand |
| `a & b`, `a \| b`, `a ^ b`, `a << b`, `a >> b` | `and`, `or`, `xor`, `shl`, `shr` | |
| `a == b` | `eq` | `!=` is its negation |
| `a < b` | `lt` | `>` swaps the operands, `<=` and `>=` negate |

Only the type of the left operand is looked up. `a + b` is `a.add(b)`, and the function is also an ordinary method under that name. `=`, `&&`, `||`, `.`, a call and `as` have no function.

For a class the function stands in the class body, with `self` as the left operand. For a struct it is a free function of the module that declares the struct, with the struct as its first parameter. Two such functions may share a name when their first parameters differ in type.

The same word declares the language hooks `iter`, `next`, `value`, `index`, `set_index` and `hash`, which [chapter 16]({{% relref "16-collections" %}}) covers.

A class or a struct without `operator fn eq` has a default `==` that compares the fields in order. A class without `concrete fn hash` has a default hash over the same fields. Neither has a default `<`, so a type that a constraint asks to be ordered declares `lt`. `==` on two class pointers stays the identity of the objects and calls nothing.

```anti
//! name: ops
import anti.io;
import anti.text;

export class Vec2
{
	pub x: int = 0,
	pub y: int = 0,

	operator fn add(self, o: Vec2) -> Vec2
	{
		return Vec2 { x: self.x + o.x, y: self.y + o.y };
	}

	operator fn neg(self) -> Vec2
	{
		return Vec2 { x: -self.x, y: -self.y };
	}

	operator fn lt(self, o: Vec2) -> bool
	{
		return self.x * self.x + self.y * self.y < o.x * o.x + o.y * o.y;
	}
}

class Name
{
	pub text: str = "",
	pub uses: int = 0,

	operator fn eq(self, o: Name) -> bool
	{
		return self.text == o.text;
	}

	concrete fn hash(self) -> u64
	{
		return self.text.hash();
	}
}

export struct Money { cents: i64 }

operator fn add(a: Money, b: Money) -> Money
{
	return Money { cents: a.cents + b.cents };
}

operator fn lt(a: Money, b: Money) -> bool
{
	return a.cents < b.cents;
}

fn largest<T: lt>(own a: T, own b: T) -> T
{
	if a < b {
		return b;
	}
	return a;
}

export fn sum(a: *Vec2, b: *Vec2) -> int
{
	let c = *a + *b;
	return c.x + c.y;
}

export fn same(a: *Vec2, b: *Vec2) -> bool
{
	return *a == *b;
}

export fn pay(a: Money, b: Money) -> Money
{
	return a + b;
}

fn main() -> int
{
	let a = Vec2 { x: 1, y: 2 };
	let b = Vec2 { x: 3, y: 4 };
	let c = a + b;
	let d = -a;
	c = c + a;
	let n = Name { text: "id", uses: 1 };
	let m = Name { text: "id", uses: 2 };
	let total = pay(Money { cents: 150 }, Money { cents: 275 });
	let top = largest(Money { cents: 5 }, Money { cents: 9 });
	let line = f"c {c.x} {c.y}, d {d.x} {d.y}, sum {sum(&a, &b)}, same {same(&a, &b)} {a == a}, order {a < b} {a >= b} {a.lt(b)}, names {n == m} {n != m} {n.hash() == m.hash()}, pointers {&n == &m}, money {total.cents} {top.cents}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```
```text output
c 5 8, d -1 -2, sum 10, same false true, order true false true, names true false true, pointers false, money 425 9
```

`n == m` calls the `eq` of `Name`, which compares the texts and ignores `uses`. `&n == &m` compares two addresses.

The checker refuses each of the following.

```anti refused
//! error: `operator fn` takes one of `add sub mul div rem neg eq lt and or xor shl shr not iter next value index set_index hash`
class Vec2
{
	pub x: int = 0,

	operator fn plus(self, o: Vec2) -> Vec2 { return Vec2 { x: self.x + o.x }; }
}
```

```anti refused
//! error: `<` is not defined on `Vec2`. A struct or a class has no default order and declares `operator fn lt`
class Vec2 { pub x: int = 0, }

fn before(a: Vec2, b: Vec2) -> bool
{
	return a < b;
}
```

```anti refused
//! error: expected `Vec2`, found an integer literal
class Vec2
{
	pub x: int = 0,

	operator fn add(self, o: Vec2) -> Vec2 { return Vec2 { x: self.x + o.x }; }
}

fn shifted(a: Vec2) -> Vec2
{
	return 1 + a;
}
```

```anti refused
//! error: `+=` does not apply to `Vec2`
class Vec2
{
	pub x: int = 0,

	operator fn add(self, o: Vec2) -> Vec2 { return Vec2 { x: self.x + o.x }; }
}

fn grow(a: *Vec2, by: Vec2)
{
	*a += by;
}
```

```anti refused
//! error: `eq` is already declared
struct Money { cents: i64 }

fn eq(a: Money, b: Money) -> bool { return true; }

operator fn eq(a: Money, b: Money) -> bool { return a.cents == b.cents; }
```

### How antic builds it

The checker rewrites an operator into a call of its function, and lowering sees an ordinary call.

```ir excerpt=ops target=linux-x86_64 mode=release
export fn ops.sum(%0: ptr nonnull deref(size_of ops.Vec2), %1: ptr nonnull deref(size_of ops.Vec2)) -> i64 {
b0:
    %2 = slot ops.Vec2
    lifestart %2
    %3 = call agg @ops.Vec2.add(%0, %1)
    memcopy %2, %3, ops.Vec2
...
export fn ops.same(%0: ptr nonnull deref(size_of ops.Vec2), %1: ptr nonnull deref(size_of ops.Vec2)) -> i8 zeroext range 0, 2 {
b0:
    %2 = copy i8 1
    %3 = call i8 @ops.Vec2.equals(%0, %1)
...
export fn ops.pay(%0: agg ops.Money, %1: agg ops.Money) -> agg ops.Money {
b0:
    %2 = call agg @ops.add(%0, %1)
```

- `*a + *b` in `sum` is a direct call of `Vec2.add` with the two addresses. The result is a `Vec2`, an object, which the call returns through memory and `memcopy` puts into the local `c`.
- `*a == *b` in `same` calls `Vec2.equals`, the default the compiler writes for the class and stores in entry 3 of its table.
- `a + b` on two `Money` in `pay` is a call of the free function `ops.add`.

The default `equals` of a class tests more than the fields.

```ir excerpt=ops target=linux-x86_64 mode=release
fn ops.Vec2.equals(%0: ptr, %1: ptr) -> i8 {
b0:
    %2 = copy i8 1
    %3 = eq i8 %0, %1
    branch %3, b2, b3
...
b4:
    %5 = call ptr @anti_rt_descriptor(%0)
    %6 = call ptr @anti_rt_descriptor(%1)
    %7 = eq i8 %5, %6
    branch %7, b5, b1
b5:
    %8 = ptradd %0, offset_of ops.Vec2.x
    %9 = ptradd %1, offset_of ops.Vec2.x
    %10 = load i64 %8
    %11 = load i64 %9
    %12 = eq i8 %10, %11
```

An object equals itself, which the first compare answers. Two objects of different classes are not equal: `anti_rt_descriptor` reads the class of each, and the two must be one. Then the fields are compared in order, each by its kind, as for a struct. The default `hash` mixes the same fields in the same order.

```asm excerpt=ops target=linux-x86_64 mode=release
<sum>:
...
leaq (%rip), %rax
movq %rax, -0x18(%rbp)
vmovdqu 0x8(%rdi), %xmm0
vpaddq 0x8(%rsi), %xmm0, %xmm0
vmovdqu %xmm0, -0x10(%rbp)
leaq -0x18(%rbp), %rdi
xorl %esi, %esi
callq
...
<pay>:
leaq (%rdi,%rsi), %rax
retq
```

`pay` is one instruction: the call of `add` on the struct is inlined and the two values travel in registers. `sum` has `Vec2.add` inlined too, as one `vpaddq` over both fields. The result is an object, though. `leaq` and `movq` store its table pointer into the frame, and the `callq` is `anti_rt_hook` for its `created` hook. An operator that returns a class value pays for an object each time.

Where it lives: `src/antic/sema_operator.c` finds the function of an operator. `lower_equals` of `src/antic/lower_eq.c` and `lower_hash` of `src/antic/lower_hash.c` write the defaults, and `default_eq` of `src/antic/sema_hash.c` decides which types have one. `anti_rt_descriptor` of `src/rt/object.c`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| An operator on a struct | one direct call | inlined where `opt` chooses, one instruction in `pay` |
| An operator on a class value | one direct call | inlined where `opt` chooses |
| A class value as the result | one store of the table pointer and one call of `anti_rt_hook` | the same |
| Default `==` of a struct | one compare per field, in place | the same |
| Default `==` of a class value | one call of the class's `equals`, two calls of `anti_rt_descriptor`, one compare per field | the same |
| `==` on two class pointers | a compare of two addresses | the same |
| Heap allocations | 0 | 0 |
| Checks inserted | those of the arithmetic inside the function | none |

A small numeric type that is added in a loop costs least as a struct with free operators. A struct result has no table pointer and no hook.

### Pitfalls

For a Java programmer:

- `==` on two class values compares their fields, through `eq` or the default. `==` on two pointers is the identity that Java's `==` is.
- A class that replaces `hash` and keeps the default `==`, or the reverse, breaks the rule that equal values hash alike. Nothing checks it.

For a C programmer:

- `a + b` on a struct compiles only when the module of the struct declares `operator fn add` for it.

For a C++ programmer:

- The right operand does not choose the function. `1 + v` is refused where `v + 1` may compile.
- A compound assignment does not follow from the binary operator. `a = a + b` is the form.
- `<=`, `>`, `>=` and `!=` are derived from `lt` and `eq`, and no `operator fn` declares them.

## Reflection

### What you know

Java keeps the name, the fields and the methods of every class at run time. `java.lang.reflect` reads them, with a lookup by name and boxed arguments. C++ keeps `type_info` and what `dynamic_cast` needs, and no list of members. C keeps nothing.

### The feature

Every class has a descriptor: its name, its base, its size, its fields and its `pub` functions. The module `anti.reflect` reads it.

| Function | Result |
|---|---|
| `describe(obj as *byte)` | the descriptor of the object's class, a `?*byte` |
| `name(d)`, `parent(d)` | the class name, the descriptor of the base |
| `field_count(d)`, `field(d, i)` | the fields as `Field` values: `name`, `offset`, `type_id`, `element`, `owned` |
| `function_count(d)`, `function(d, i)` | the functions as `Function` values: `name`, `slot`, `param_count` |
| `get(obj, d, i)` | field `i` as a `Value` |
| `set(obj, d, i, v)` | writes field `i`, and may fail |
| `call(obj, i, args)` | calls function `i` with a slice of `Value`, gives a `Value`, and may fail |
| `new("Circle")` | a new object of the class of that name, a `?*Object` |

A `Value` holds a kind and one of an `int`, a `uint`, an `f64`, a `bool`, a `char`, a `str` or a pointer. `of_int`, `of_float` and their siblings build one, and `v.data.f` reads the float of one.

`set` and `call` follow the error convention of [chapter 6]({{% relref "06-errors" %}}). They fail with the code `BAD_INDEX`, `NOT_CARRIED` for a type no `Value` carries, `WRONG_KIND` or `WRONG_COUNT`.

The functions of the root work over the same descriptor. `obj.serialize(&builder)` writes an object as JSON with its class in the member `type`, and `Object.deserialize(text, from)` builds the object back, with its memory from the allocator `from`. A field that the text cannot carry, such as a bitfield or a union, is written as `null` and keeps its default on the way back.

Reflection reaches every field, the private ones included. `--no-reflect` drops the field lists, the function lists and what `call` and `new` need. It keeps what `is` and `as` read.

```anti
//! name: mirror
import anti.io;
import anti.mem;
import anti.reflect;
import anti.text;

struct Size { w: int, h: int }

class Circle
{
	pub r: f64 = 1.0,
	pub label: str = "circle",
	size: Size = Size { w: 2, h: 3 },
	hits: u8 : 3 = 0,

	pub fn area(self) -> f64
	{
		return 3.0 * self.r * self.r;
	}

	pub fn grow(self, by: f64) -> f64
	{
		self.r = self.r + by;
		return self.r;
	}
}

fn main() -> int
{
	let obj = alloc Circle { };
	defer delete(obj);
	let d = reflect.describe(obj as *byte) else { return 1; };
	let line = f"{reflect.name(d)}: {reflect.field_count(d)} fields, {reflect.function_count(d)} functions";
	io.println(line);
	free(line.ptr);
	for i in 0..reflect.field_count(d) {
		let f = reflect.field(d, i);
		line = f"field {f.name} at {f.offset}, type id {f.type_id as int}";
		io.println(line);
		free(line.ptr);
	}
	let last = reflect.function_count(d) - 1;
	let grow = reflect.function(d, last);
	line = f"function {grow.name}, slot {grow.slot}, {grow.param_count} parameter";
	io.println(line);
	free(line.ptr);
	reflect.set(obj as *byte, d, 0, reflect.of_float(2.0)) catch fatal;
	let args: [1]reflect.Value = [reflect.of_float(0.5)];
	let grown = reflect.call(obj, last, args[0..1]) catch fatal;
	let r = reflect.get(obj as *byte, d, 0);
	let wrong = false;
	reflect.call(obj, last, args[0..0]) catch e {
		wrong = e.code == reflect.WRONG_COUNT;
	};
	line = f"grown {grown.data.f}, r {r.data.f}, wrong count {wrong}";
	io.println(line);
	free(line.ptr);
	let b = text.Builder.new();
	obj.serialize(&b);
	io.println(b.text());
	let arena = mem.ArenaAllocator.new(mem.LibcAllocator.get(), 4096);
	let back = Object.deserialize(b.text(), &arena);
	line = f"read back a {back.type_name()}, equal {obj.equals(back)}";
	io.println(line);
	free(line.ptr);
	destroy(back, &arena);
	arena.free_all();
	let fresh = reflect.new("Circle") else { return 1; };
	line = f"new {fresh.type_name()}";
	io.println(line);
	free(line.ptr);
	delete(fresh);
	return 0;
}
```
```text output
Circle: 4 fields, 18 functions
field r at 8, type id 15
field label at 16, type id 16
field size at 32, type id 21
field hits at 48, type id 0
function grow, slot 18, 2 parameter
grown 2.5, r 2.5, wrong count true
{"type":"Circle","r":2.5,"label":"circle","size":{"w":2,"h":3},"hits":null}
read back a Circle, equal true
new Circle
```

The 18 functions are the seven of the root, the nine hooks, `area` and `grow`. `grow` has index 17 in that list and slot 18 in the table, since entry 0 of the table is the descriptor. Its two parameters count `self`. The bitfield `hits` has the type id 0, `None`.

The checker refuses each of the following.

```anti refused
//! error: `set` may fail and its error is not handled
import anti.reflect;

class Circle { pub r: f64 = 1.0, }

fn widen(c: *Circle, d: *byte)
{
	reflect.set(c as *byte, d, 0, reflect.of_float(2.0));
}
```

```anti refused
//! error: expected `[]Value`, found a float literal
import anti.reflect;

class Circle
{
	pub r: f64 = 1.0,

	pub fn grow(self, by: f64) -> f64 { return self.r + by; }
}

fn grow(c: *Circle) -> f64
{
	let v = reflect.call(c, 17, 0.5) catch fatal;
	return v.data.f;
}
```

### How antic builds it

The descriptor is a constant record that the module of the class writes once per program, with two lists behind it.

```ir excerpt=mirror target=linux-x86_64 mode=release
type anti.rt.Descriptor = struct { name: ptr, name_length: i64, parent: ptr, size: i64, depth: i64, ancestors: ptr, field_count: i64, fields: ptr, destruct: ptr, offset: i64, function_count: i64, functions: ptr, version: ptr, version_length: i64, versions: ptr, type_arg_count: i64, type_args: ptr }
...
type anti.rt.Field = struct { name: ptr, name_length: i64, offset: i64, type: i64, owned: i64, descriptor: ptr }
...
type anti.rt.Function = struct { name: ptr, name_length: i64, slot: i64, param_count: i64, signature: ptr, signature_length: i64 }
```

- A descriptor has 17 words, 136 bytes. `depth` and `ancestors` serve `is` and `as`. `offset` is the distance from an interface part to its object. `version` and `versions` serve the plugin loader of [chapter 15]({{% relref "15-hooks" %}}).
- A field record has 6 words, 48 bytes: the name, the offset, a type id and the `own` mark. For an inline struct or class it also holds the descriptor of that type.
- A function record has 6 words, 48 bytes: the name, the slot in the table, the count of parameters and the signature with its length.

`get` and `set` read a field record and copy the bytes at its offset, by the type id. `call` needs code, since it must pass a `[]Value` as the arguments of a real call. The whole-program pass writes one trampoline per distinct signature of the program.

```llvm excerpt=mirror target=linux-x86_64 mode=dev
define hidden noundef i8 @anti.rt.trampoline.f64.f64(ptr noundef %p0, ptr noundef %p1, ptr noundef %p2, i64 noundef %p3, ptr noundef %p4) #0 {
b0:
...
  %v1 = icmp ne i64 %v0, 1
...
  %v8 = icmp ne i8 %v7, 3
...
  %v19 = call double (ptr, double) %v16(ptr %v17, double %v18)
```

`anti.rt.trampoline.f64.f64` serves every function that takes one `f64` after `self` and returns an `f64`, `grow` among them. It takes the entry, the object, the arguments, their count and a place for the result.

- `icmp ne i64 %v0, 1` tests that one argument came.
- `icmp ne i8 %v7, 3` tests that its kind is 3, `Float`.
- The `call` passes the object and the `f64` to the entry, and the result is stored back as a `Value`.

`reflect.call` itself is `anti_rt_reflect_call` of the runtime. It finds the function record by its index and reads the entry from the table of the object at the record's slot. It then searches the table `anti_rt_trampolines` for the record's signature text with `strcmp`. The call therefore dispatches as any call does: an object of a derived class runs its own replacement.

`reflect.new` reads `anti_rt_registry`, a list of every class of the program with its descriptor and a function that prepares an object. The whole-program pass writes it at link time.

A release build removes what nothing reaches, and a slot that only reflection may reach is kept. The pass records the slots that calls of the program use in `anti_rt_slots`, with a flag for a program that calls `reflect.call`.

Where it lives: `src/antic/lower_desc.c` writes descriptors, field lists and function lists. `whole_write_trampolines`, `whole_write_registry` and `whole_write_slots` of `src/antic/whole_tables.c`. `src/rt/reflect.c` holds the readers, `src/rt/call.c` holds `anti_rt_reflect_call`, and `src/std/anti/reflect.anti` is the module.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Read-only bytes per class | a descriptor of 136, 48 per field, 40 per `pub` function, 16 of them inherited, and the names | the same for a class that is reached |
| Read-only bytes per interface part of a class | a descriptor of 136 | the same |
| Code per distinct signature | one trampoline | the same |
| Bytes per object | 0 | 0 |
| `describe`, `field`, `get` | one to five calls into the runtime | the same |
| `set` | one call and the test of the kind | the same |
| `call` | one call into the runtime, one `strcmp` per trampoline until the match, the tests of count and kinds, one indirect call | the same |
| `new` | a search of the registry by name, one `malloc`, the defaults, `construct` | the same |
| With `--no-reflect` | no field list, no function list, no trampoline, no registry | the same |

The descriptor costs space in the program and nothing in an object. A program that never imports `anti.reflect` still carries the descriptors, because `is`, `as`, `delete` and the default `serialize` read them.

### Pitfalls

For a Java programmer:

- A field and a function are found by index. `reflect.field(d, i).name` gives the name, and a search by name is a loop the program writes.
- The index of `call` counts the function list, where the seven root functions and the nine hooks come first. The first `pub` function of a class has index 16.
- `reflect.call` does not throw. It fails with a code, and the call site handles it.

For a C programmer:

- The descriptor is ordinary read-only data whose struct `src/rt/` declares. C code that holds an object can read it.

For a C++ programmer:

- `is` and `as` do not search. They compare one entry of the ancestor array, at a cost that does not grow with the hierarchy.
- `--no-reflect` is the nearest form to `-fno-rtti`, and it keeps the checked conversions working.
