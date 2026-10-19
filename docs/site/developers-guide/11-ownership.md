---
title: "Ownership"
description: "own, lent, dup, moves, teardown, temporaries, the rules at a call, Shared<T> and its counts, and what the checker refuses and why."
summary: "`own` and `lent`, `dup` and moves, teardown and temporaries, the rules at a call, `Shared<T>` of `anti.mem` with its atomic count, and what the checker refuses and why, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T20:23:44+02:00
draft: true
weight: 110
tags: [compilers, programming-languages]
keywords: [ownership, borrowing, move semantics, dup, teardown, reference counting, Shared]
---

Every value that holds memory has one owner, and the owner frees it. A value on the stack is torn down at the end of its block, and an object on the heap is freed by `delete`. The checker refuses a copy that would give memory two owners, so a copy is always written as `dup`. Ownership costs no byte per value. It costs one store per move and one test per teardown of a local that may have moved.

## own and lent

### What you know

Java frees an object when no reference reaches it, so no reference owns anything. In C the owner of a block is a convention, and a comment says who calls `free`. C++ writes the owner as `std::unique_ptr<T>`, whose destructor frees. A reference and a raw pointer of C++ borrow, and nothing checks that the borrower stops before the owner frees.

### The feature

`own` before a pointer field or a slice field of a class says that the object owns the memory behind the field. The teardown and `dup` follow the word.

- The teardown of the object frees the memory. An `own` pointer to a class is deleted through the table of the object it points at. An object of a derived class therefore runs its own teardown. The elements of an `own` slice are torn down last to first, and then the buffer is freed.
- `dup` of the object gives the copy memory of its own, with a copy of the contents.
- `=` to the field tears down what the field held before the new value goes in, as `reset` of a `unique_ptr` does. A field given the pointer it holds keeps it.

A pointer field without `own` is an address. The teardown leaves it alone, and `dup` copies the address.

A class value or a struct value held inline in a field is part of the object and needs no word. It is torn down with the object.

`own` stands on a pointer or a slice field of a class and nowhere else among fields. A `str` is immutable and shared, an inline value is owned already, and a struct takes no `own` field.

The checker does not follow where the pointer of an `own` field came from. A program that stores the address of a local there, or one pointer in two `own` fields, compiles. Its teardown then frees memory that it must not free.

`lent` before a pointer parameter or a slice parameter says that the pointer is valid during the call alone. The function reads and changes through it. It hands the pointer on to another `lent` parameter and to nothing else.

- A lent pointer is not stored in a field, in a global or in a local of a plain pointer type.
- It is not returned.
- It does not go to a plain parameter, an `own` parameter or `delete`.
- A `snapshot fn` does not capture it. A closure by reference does, since such a closure never outlives the call.
- A pointer or a slice derived from a lent one is lent too: `&p.field`, `s.ptr`, `&s[i]` and `s[a..b]`.
- `let q = p;` gives a lent local, which follows the same rules.

The one exit of a lent pointer is an argument of an `extern fn`. C cannot be checked, and whether C keeps the pointer is the contract of that function.

Any pointer goes where a lent one is expected. `fn(lent *T)` and `fn(*T)` are two function types, and neither converts to the other. An anonymous function takes `lent` from the parameter it is passed to. `for p in &s` gives each element as a pointer lent for one turn of the loop. `dup(p)` of a lent pointer gives a new object, which belongs to the function that made it.

```anti
//! name: owners
import anti.io;
import anti.text;

class Texture
{
	pub id: int = 0,
}

class Sprite
{
	pub own pixels: []byte,
	pub own shadow: ?*Sprite = none,
	pub atlas: *Texture,
	pub name: str = "",
	pub static atomic live: int = 0;

	fn destruct(self)
	{
		Sprite.live.sub(1);
	}
}

fn sprite(atlas: *Texture, n: int) -> *Sprite
{
	let raw = alloc(byte, n) catch fatal;
	for i in 0..n {
		raw[i] = 0;
	}
	Sprite.live.add(1);
	return alloc Sprite { pixels: []byte { ptr: raw, len: n }, atlas: atlas };
}

fn brighten(lent s: *Sprite, by: byte)
{
	for p in &s.pixels {
		*p += by;
	}
}

fn sum(lent pixels: []byte) -> int
{
	let total = 0;
	for b in pixels {
		total += b as int;
	}
	return total;
}

fn each(s: *Sprite, f: fn(lent *Sprite))
{
	f(s);
	let shadow = s.shadow;
	if shadow != none {
		f(shadow);
	}
}

fn drop(own s: *Sprite)
{
	delete(s);
}

fn main() -> int
{
	let atlas = Texture { id: 7 };
	let s = sprite(&atlas, 4);
	s.shadow = sprite(&atlas, 2);
	brighten(s, 3);
	each(s, fn(p) { brighten(p, 1); });
	let shadow = s.shadow catch fatal;
	let line = f"sum {sum(s.pixels)}, shadow {sum(shadow.pixels)}, live {Sprite.live.load()}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Sprite {size_of(Sprite)}, *Sprite {size_of(*Sprite)}, []byte {size_of([]byte)}";
	io.println(line);
	free(line.ptr);
	drop(s);
	line = f"live {Sprite.live.load()}, atlas {atlas.id}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
sum 16, shadow 2, live 2
sizes: Sprite 56, *Sprite 8, []byte 16
live 0, atlas 7
```

`drop(s)` deletes one sprite, and the count of live sprites falls from 2 to 0. The teardown of `s` deleted the shadow through the `own` field and freed both pixel buffers. The texture is a plain pointer field and stays.

The checker refuses each of the following.

```anti refused
//! error: `own` needs a pointer or a slice, and `text` has type `str`
class Label
{
	own text: str,
}
```

```anti refused
//! error: `own` needs a pointer or a slice, and `atlas` has type `Texture`
class Texture
{
	pub id: int = 0,
}

class Sprite
{
	own atlas: Texture,
}
```

```anti refused
//! error: `p` is lent for the call and cannot be stored
class Person
{
	pub age: int = 0,
}

class Room
{
	pub who: ?*Person = none,
}

fn enter(lent p: *Person, r: *Room)
{
	r.who = p;
}
```

```anti refused
//! error: `p` is lent for the call and cannot be returned
class Person
{
	pub age: int = 0,
}

fn same(lent p: *Person) -> *Person
{
	return p;
}
```

```anti refused
//! error: `p` is lent for the call and passes on to a `lent` parameter alone
class Person
{
	pub age: int = 0,
}

fn plain(p: *Person)
{
}

fn pass(lent p: *Person)
{
	plain(p);
}
```

```anti refused
//! error: the object is lent for the call and stays with its owner, so `delete` does not take it
class Person
{
	pub age: int = 0,
}

fn end(lent p: *Person)
{
	delete(p);
}
```

```anti refused
//! error: the pointer is lent for the call and cannot be returned
class Person
{
	pub age: int = 0,
}

fn age_of(lent p: *Person) -> *int
{
	return &p.age;
}
```

```anti refused
//! error: the slice is lent for the call and cannot be returned
fn head(lent xs: []int) -> []int
{
	return xs[0..1];
}
```

```anti refused
//! error: `own` and `lent` do not stand together, since a lent pointer stays with its owner
class Person
{
	pub age: int = 0,
}

fn both(own lent p: *Person)
{
}
```

```anti refused
//! error: `lent` marks a pointer or slice parameter, and this one is `int`
fn count(lent n: int) -> int
{
	return n;
}
```

```anti refused
//! error: an `extern fn` takes no `lent` parameter, since C may keep the pointer
extern fn c_take(lent p: ?*byte);
```

```anti refused
//! error: expected `fn(lent *Person)`, found `fn(*Person)`
class Person
{
	pub age: int = 0,
}

fn run(f: fn(lent *Person))
{
	let one = Person { };
	f(&one);
}

fn grow(p: *Person)
{
	p.age += 1;
}

fn main() -> int
{
	run(grow);
	return 0;
}
```

### How antic builds it

`own` and `lent` are facts of the checker. Neither changes the layout of a value, and `lent` leaves no trace in the code.

```llvm excerpt=owners target=linux-x86_64 mode=dev
define hidden void @owners.brighten(ptr noundef nonnull dereferenceable(56) %p0, i8 zeroext noundef %p1) #0 {
...
define hidden noundef i64 @owners.sum(i64 %p0.0, i64 %p0.1) #0 {
...
define hidden void @owners.drop(ptr noalias noundef nonnull dereferenceable(56) %p0) #0 {
```

`brighten` takes a `lent *Sprite` and `drop` an `own *Sprite`, since a function deletes only what it received as `own`. Both parameters are the same `ptr`. The `own` of `drop` adds `noalias`, since the function took the object over and no other pointer of it reaches the object. The `lent []byte` of `sum` is the two words of every slice, the address in `%p0.0` and the length in `%p0.1`.

`own` reaches the code through two functions that antic writes for every class: a teardown named `<Class>.destroy` and a copy named `<Class>.copy`. The table of the class holds both. `delete(s)` is one call, `anti_rt_delete(s, descriptor)`. The runtime reads the teardown from the table of the object and calls it, then frees the object.

```llvm excerpt=owners target=linux-x86_64 mode=release
define internal void @owners.Sprite.destroy(ptr noundef %p0, ptr noundef %p1) #1 {
b0:
  tail call void @anti_rt_hook(ptr %p0, i64 1)
  %v1.i = atomicrmw sub ptr @owners.Sprite.live, i64 1 seq_cst, align 8
  %v3 = getelementptr i8, ptr %p0, i64 24
  %v5 = load ptr, ptr %v3, align 8
  tail call void @anti_rt_delete_from(ptr %v5, ptr nonnull @owners.Sprite.descriptor, ptr %p1)
  store ptr null, ptr %v3, align 8
  %v11 = getelementptr i8, ptr %p0, i64 8
  %v13 = load ptr, ptr %v11, align 8
  tail call void @anti_rt_give(ptr %p1, ptr %v13)
  store ptr null, ptr %v11, align 8
  ret void
}
```

- `%p0` is the object. `%p1` is the allocator that the memory goes back to, and null stands for the C library.
- `anti_rt_hook(%p0, 1)` is the `destroyed` hook of [chapter 15]({{% relref "15-hooks" %}}).
- The `atomicrmw sub` is the body of `destruct`, which the release build inlined. The `destruct` of every class of the chain runs before any field goes, so a body still reads what the object owns.
- The fields go last to first. `shadow` stands at offset 24. `anti_rt_delete_from` tears down the object behind it through that object's own table and frees it. A pointer that holds `none` does nothing there.
- `pixels` stands at offset 8, and the load reads the address of its buffer. `anti_rt_give` frees the buffer: with a null allocator it calls `free`.
- Each `own` field holds `none` after its teardown, so a second teardown of the same object frees nothing.
- `atlas` at offset 32 and `name` at offset 40 are not `own`, and no instruction touches them.

A dev build writes the same calls in the same order and calls `destruct` where the release build inlined it. Dev mode adds no check here, and a release build removes no call.

Where it lives: `sema_lent_form` of `src/antic/sema.c`. `sema_require` and `through_lent` of `src/antic/sema_expr.c` refuse the uses of a lent pointer. `lower_class_teardown` and `destroy_buffer` of `src/antic/lower_owning.c` write the teardown. `anti_rt_delete`, `anti_rt_delete_from` and `anti_rt_give` of `src/rt/object.c`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes `own` adds to a field | 0 | 0 |
| Bytes of an `own` pointer field | 8 | 8 |
| Bytes of an `own` slice field | 16 | 16 |
| Bytes and instructions `lent` adds | 0 | 0 |
| Teardown of an `own` pointer to a class | one `anti_rt_delete_from`, one indirect call of the teardown, one `free`, one store of `none` | the same |
| Teardown of an `own` slice of bytes | one `anti_rt_give`, one `free`, one store of `none` | the same |
| Checks | the test for a zero table in `anti_rt_delete_from` | the same |
| Atomic operations | two loads in the `anti_rt_hook` of each object torn down, and what a `destruct` of the program does | the same |
| Locks | 0 | 0 |

`size_of(Sprite)` is 56: the table pointer, 16 bytes of `pixels`, 8 of `shadow`, 8 of `atlas` and 16 of `name`. The same class without the two words `own` has the same 56 bytes.

The example allocates four blocks, two sprites and two pixel buffers. `drop(s)` frees all four with one `delete` in the source. No option turns the test for a zero table off.

### Pitfalls

For a Java programmer:

- A pointer field without `own` keeps nothing alive. The object behind it may be deleted while the field still holds its address.
- `delete` is written for every `alloc`, either by the program or by the `own` field of an owner. Nothing collects an object that no pointer reaches.

For a C programmer:

- `own` frees with the object. A `free` of the same field in `destruct` frees it twice. Memory that a class gives back its own way, with `fclose` or a function of a library, stands in a field without `own`.
- `=` to an `own` field frees what the field held. A `delete` or a `free` of the old value before the `=` frees it twice.
- An `own` field takes memory of `alloc`. The address of a local stored there compiles, and the teardown frees a stack address.
- A `lent` parameter cannot be saved for later, not even in a local of the plain pointer type.

For a C++ programmer:

- An `own` field is a `unique_ptr` without the wrapper type: the field is 8 bytes, the class teardown frees it, and `=` deletes the old object as `reset` does.
- `lent` is checked. A reference parameter of C++ may be stored by the callee, and the checker refuses the same store of a `lent` pointer.
- A plain `*T` parameter is not lent. The function may store it, and the caller keeps the object alive.

## dup and moves

### What you know

`b = a` in Java copies a reference, and both names reach one object. In C it copies the bytes, and a struct that holds a pointer then has two holders of one block. C++ calls the copy constructor, which may allocate, and `std::move` leaves the source in a valid state that the program may still name.

### The feature

A value **owns something** when its teardown has work to do. That holds for a class value whose chain declares `destruct` or an `own` field, and for a collection. It holds for a struct, a tuple, an array and a `?T` with such a part, and for a variant with such a case. An `int`, a `str`, a pointer and a struct of those own nothing.

A value that owns nothing is copied by `=` and `let`, byte for byte. A value that owns something has three operations.

- `=` and `let` refuse to copy an existing one: a local, a field, an element or `*p`. The bytes would give its memory two owners.
- A fresh value moves. A literal, a call result and the result of `dup` have no other owner. A `let`, an `=`, a `return` and an argument therefore take them without a copy of what they own.
- `dup` copies. `dup(x)` of a value gives a value. For a pointer to a class, `dup(p)` gives a new object on the heap and a pointer to it. Each `own` field of the copy gets memory of its own, and a class value held inline copies itself the same way. Other pointer fields copy the address. `dup` of a value that owns nothing gives its bytes.

A local moves in four places. It moves as an argument of an `own` parameter and as a part of a tuple, struct, class or variant literal. It moves by `return` and by `destroy(&x)`. After a move the name is dead. The checker refuses every mention that follows the move in the text, an assignment to the name included.

The order of the text is the order of execution except in three cases, and the checker refuses the move in each.

- A move inside a loop that began after the `let` would run again.
- A `defer` or an `undo` that names the local runs after the move.
- A closure that captured the local may read it after the move.

A move on one path of an `if` is accepted. The local is then dead after the `if` on both paths. The code tears it down at the end of its block only when it did not move.

What moves at an `own` parameter follows the type. A value that owns something moves. So does a value that holds a pointer, such as a `*T`, a slice or a struct with a pointer field. An `int`, a `str` and a struct of numbers are copied, and the caller keeps its local. A value that stays where it is, a field or an element, does not move, and the checker asks for `dup`.

`*dup(p)` and `*alloc T { }` are refused. The dereference would drop the only pointer to a new object, and nothing could free it.

```anti
//! name: moves
import anti.io;
import anti.text;

class Box
{
	pub own data: *int,
	pub n: int = 0,

	pub fn make(n: int) -> Box
	{
		let cell = alloc(int, 1) catch fatal;
		*cell = n * 10;
		return Box { data: cell, n: n };
	}

	fn destruct(self)
	{
		let line = f"  destruct {self.n}";
		io.println(line);
		free(line.ptr);
	}
}

fn take(own b: Box) -> int
{
	return b.n;
}

fn pass(own b: Box) -> Box
{
	return b;
}

fn next(own n: int) -> int
{
	return n + 1;
}

fn end(own p: *Box)
{
	delete(p);
}

fn main() -> int
{
	let a = Box.make(1);
	io.println("take(a)");
	let n = take(a);
	io.println("pass(b)");
	let b = Box.make(2);
	let c = pass(b);
	io.println("dup(c)");
	let d = dup(c);
	d.n = 3;
	*d.data = 33;
	let k = 5;
	let line = f"n {n}, c {c.n} {*c.data}, d {d.n} {*d.data}, next {next(k)} of {k}";
	io.println(line);
	free(line.ptr);
	let cell = alloc(int, 1) catch fatal;
	let p = alloc Box { data: cell, n: 4 };
	let q = dup(p);
	q.n = 5;
	io.println("end(p)");
	end(p);
	io.println("delete(q)");
	delete(q);
	line = f"sizes: Box {size_of(Box)}, *Box {size_of(*Box)}";
	io.println(line);
	free(line.ptr);
	io.println("end of main");
	return 0;
}
```

```text output
take(a)
  destruct 1
pass(b)
dup(c)
n 1, c 2 20, d 3 33, next 6 of 5
end(p)
  destruct 4
delete(q)
  destruct 5
sizes: Box 24, *Box 8
end of main
  destruct 3
  destruct 2
```

`a` moves into `take`, which tears it down at its exit. `b` moves into `pass` and out again into `c`, and no `destruct` runs. `d` is a copy with a cell of its own, so the write of 33 leaves the 20 of `c`. `k` is an `int`, is copied into `next`, and is read after the call. At the end of `main` the two locals that still hold a value go, last to first. `a` and `b` moved and print nothing.

The checker refuses each of the following.

```anti refused
//! error: `Box` has `own` fields, use `dup` instead of `=`
class Box
{
	pub own data: ?*int = none,
}

fn main() -> int
{
	let a = Box { };
	let b = a;
	return 0;
}
```

```anti refused
//! error: `a` was moved into `take`
class Box
{
	pub own data: ?*int = none,
	pub n: int = 0,
}

fn take(own b: Box)
{
}

fn main() -> int
{
	let a = Box { };
	take(a);
	return a.n;
}
```

```anti refused
//! error: `a` was moved into `take`
class Box
{
	pub own data: ?*int = none,
	pub n: int = 0,
}

fn take(own b: Box)
{
}

fn main() -> int
{
	let a = Box { };
	take(a);
	a = Box { n: 2 };
	return 0;
}
```

```anti refused
//! error: `a` was moved into `take`
class Box
{
	pub own data: ?*int = none,
	pub n: int = 0,
}

fn take(own b: Box)
{
}

fn main() -> int
{
	let a = Box { };
	if a.n == 0 {
		take(a);
	} else {
		take(a);
	}
	return 0;
}
```

The last block moves `a` on both paths. The second `take(a)` follows the first in the text, so the checker refuses it. The accepted form moves once, after the `if`.

```anti refused
//! error: `a` moves into `take` inside a loop, which would move it again
class Box
{
	pub own data: ?*int = none,
}

fn take(own b: Box)
{
}

fn main() -> int
{
	let a = Box { };
	for i in 0..2 {
		take(a);
	}
	return 0;
}
```

```anti refused
//! error: `a` moves into `take` while a `defer` or an `undo` names it
class Box
{
	pub own data: ?*int = none,
	pub n: int = 0,
}

fn take(own b: Box)
{
}

fn show(n: int)
{
}

fn main() -> int
{
	let a = Box { };
	defer show(a.n);
	take(a);
	return 0;
}
```

```anti refused
//! error: `a` is captured by a closure, which may read it after it moves into `take`
class Box
{
	pub own data: ?*int = none,
	pub n: int = 0,
}

fn take(own b: Box)
{
}

fn main() -> int
{
	let a = Box { };
	let read = fn() -> int { return a.n; };
	take(a);
	return read();
}
```

```anti refused
//! error: `p` was moved into `end`
class Box
{
	pub own data: ?*int = none,
	pub n: int = 0,
}

fn end(own p: *Box)
{
	delete(p);
}

fn main() -> int
{
	let p = alloc Box { };
	end(p);
	return p.n;
}
```

```anti refused
//! error: `a` was moved into `Pair`
class Box
{
	pub own data: ?*int = none,
	pub n: int = 0,
}

struct Pair
{
	a: Box,
	n: int,
}

fn main() -> int
{
	let a = Box { };
	let p = Pair { a: a, n: 2 };
	return a.n;
}
```

```anti refused
//! error: `Box` has `own` fields, and a value that stays where it is does not move into `take`, use `dup`
class Box
{
	pub own data: ?*int = none,
}

fn take(own b: Box)
{
}

fn main() -> int
{
	let boxes: [2]Box = [Box { }, Box { }];
	take(boxes[0]);
	return 0;
}
```

```anti refused
//! error: `Box` has `own` fields, and the repeat form copies one value into every element
class Box
{
	pub own data: ?*int = none,
}

fn main() -> int
{
	let a = Box { };
	let boxes: [2]Box = [a; 2];
	return 0;
}
```

```anti refused
//! error: `*` of a new object drops its only pointer, so nothing could free it. Copy the value with `dup(*p)` or `dup(x)`, or keep the pointer in a `let`
class Box
{
	pub own data: ?*int = none,
}

fn main() -> int
{
	let p = alloc Box { };
	let v = *dup(p);
	delete(p);
	return 0;
}
```

The message of a refused copy names what makes the type owning. A class with a `destruct` and no `own` field reads "has a `destruct`". A struct or a tuple reads "owns what its parts own".

### How antic builds it

A move is a copy of the bytes and one store. The store writes zero into the table pointer of the local, which is the first word of every class value.

```ir excerpt=moves target=linux-x86_64 mode=dev
    memcopy %11, %0, moves.Box
    store ptr 0, %0
    %12 = call i64 @moves.take(%11)
```

- `%0` is the slot of `a` in the frame of `main`, and `%11` is a second slot of the same frame.
- `memcopy` copies the 24 bytes of the `Box` into the second slot, which the call takes.
- `store ptr 0, %0` clears the table pointer of `a`. The local now holds no value.

A struct or a tuple has no table of its own. A move of one clears the table of each part that owns something, and the flag of each `?T` among them.

The LLVM module shows the same three steps.

```llvm excerpt=moves target=linux-x86_64 mode=dev
  call void @llvm.memcpy.p0.p0.i64(ptr %v12, ptr %v13, i64 24, i1 false)
  %v14 = load ptr, ptr %t0, align 8
  store ptr null, ptr %v14, align 8
  %v15 = load ptr, ptr %t7, align 8
  %v16 = call i64 (ptr) @moves.take(ptr byval([24 x i8]) align 8 %v15)
```

`byval([24 x i8])` is how linux-x86_64 passes a class value of 24 bytes: in the memory of the stack. On arm64 the same parameter is a plain pointer to the bytes in the frame of the caller.

The function that took the value tears it down at its exit, and tests the table first, since the value may have moved on.

```ir excerpt=moves target=linux-x86_64 mode=dev
fn moves.take(%0: agg moves.Box) -> i64 {
b0:
    %1 = ptradd inbounds %0, offset_of moves.Box.n
    %2 = load i64 %1 !type moves.Box.n
    %3 = copy i64 %2
    %4 = load ptr %0
    %5 = ne i8 %4, 0
    branch %5, b1, b2
b1:
    call void @moves.Box.destroy(%0, 0)
    jump b2
b2:
    ret i64 %3
}
```

- The first three lines read `b.n` into `%3`, the result.
- `load ptr %0` reads the table pointer of the parameter, and `ne` tests it against zero.
- `b1` calls the teardown of the class with a null allocator.
- `pass`, which returns its parameter, has neither the test nor the call. Its whole body is `ret ptr %0`.

The end of `main` calls the teardown of a local outright, or tests the local first.

```ir excerpt=moves target=linux-x86_64 mode=dev
    call void @moves.Box.destroy(%2, 0)
    %122 = load ptr %1
    %123 = ne i8 %122, 0
    branch %123, b11, b12
b11:
    call void @moves.Box.destroy(%1, 0)
    jump b12
```

`%2` is `c`, which never moves, and its teardown is called outright. `%1` is `b`, which moved into `pass`. The test finds zero at run time, and the branch passes over the call.

`dup(c)` copies the bytes and then lets the copy function of the class replace what the class owns.

```ir excerpt=moves target=linux-x86_64 mode=dev
    memcopy %24, %2, moves.Box
...
    call void @moves.Box.copy(%2, %24)
    call void @anti_rt_hook_copied(%24, %2)
    memcopy %3, %24, moves.Box
```

```llvm excerpt=moves target=linux-x86_64 mode=release
define internal void @moves.Box.copy(ptr nofree noundef readonly captures(none) %p0, ptr nofree noundef writeonly captures(none) initializes((0, 24)) %p1) #7 {
b0:
  tail call void @llvm.memcpy.p0.p0.i64(ptr noundef nonnull align 1 dereferenceable(24) %p1, ptr noundef nonnull align 1 dereferenceable(24) %p0, i64 24, i1 false)
  %v3 = getelementptr i8, ptr %p0, i64 8
  %v5 = getelementptr i8, ptr %p1, i64 8
  %v7 = load ptr, ptr %v3, align 8
  %v9 = tail call ptr @anti_rt_copy_buffer(ptr %v7, i64 8)
  store ptr %v9, ptr %v5, align 8
  ret void
}
```

- The `memcpy` copies all 24 bytes, the table pointer and the address in `data` among them.
- The two `getelementptr` lines give the address of `data` at offset 8 in the source and in the copy.
- `anti_rt_copy_buffer(%v7, 8)` allocates 8 bytes with `malloc` and copies the `int` into them. When no memory is left, the program ends with a message, as `dup(p)` below does.
- The `store` puts the new address into the copy, which now owns a cell of its own.
- `anti_rt_hook_copied`, in the lines before, is the `copied` hook of [chapter 15]({{% relref "15-hooks" %}}).

`dup(p)` of a pointer is one call, `anti_rt_dup(p, descriptor)`. The runtime reads the size of the object from its descriptor, allocates it and calls the copy function through the table. A pointer to a base class therefore copies the whole derived object.

A move of a pointer writes no code. `end(p)` passes the address, and no teardown waits for a pointer local at the end of its block.

Where it lives: `sema_refuse_owned_copy`, `sema_move_local` and `sema_note_move` of `src/antic/sema_value.c`. `lower_move_argument` of `src/antic/lower_stmt.c` and `lower_clear_owned`, `lower_copy_owned` and `lower_class_copy` of `src/antic/lower_owning.c`. `anti_rt_dup` and `anti_rt_copy_buffer` of `src/rt/object.c`.

### What it costs

The lowering is the same in dev and in release. The table gives what it writes for the `Box` of the example, 24 bytes with one `own` pointer to an `int`.

| Operation | Bytes copied | Heap allocations | Calls into the runtime | Other |
|---|---|---|---|---|
| A local moves into an `own` parameter | 24 into a slot of the frame | 0 | 0 | one store of zero |
| A fresh value goes to an `own` parameter | 0 | 0 | 0 | none |
| A pointer moves | 0 | 0 | 0 | none |
| `return` of a local | the bytes of the result, as every `return` | 0 | 0 | none |
| `dup(x)` of a `Box` | three times 24 and once 8 | 1 | `anti_rt_copy_buffer`, `anti_rt_hook_copied` | one test for a zero table |
| `dup(p)` of a `*Box` | 24 and 8 | 2 | `anti_rt_dup`, `anti_rt_copy_buffer`, `anti_rt_hook_copied` | one indirect call |
| Teardown of a local that moves somewhere | 0 | 0 | 0 | one load, one compare, one branch |

A move costs no atomic operation and takes no lock. The test before the teardown of a local stands in both modes, and no option removes it. The checks that `dup` makes are the tests for a zero table, in both modes.

In the release build of the example opt inlined `take` and `pass` into `main`, and neither function stands in the module.

### Pitfalls

For a Java programmer:

- `let b = a;` of a value that owns something is refused. No form makes two names of one value. `dup` makes a second value, and `&a` makes a pointer.
- `dup(p)` copies deep along `own` fields and shallow along plain pointers. The copy and the original share what a plain pointer field points at.

For a C programmer:

- A struct with an owning part is no plain data. `=` refuses it, and a `memcpy` of it gives two owners of one block.
- A moved local is dead. Its bytes are still in the frame, and a pointer taken with `&a` before the move still reaches them. The checker follows the name and not the pointer.

For a C++ programmer:

- A move is not a member function. No move constructor runs, and a class cannot replace what a move does.
- A moved local is not reusable. An assignment to it after the move is refused, where C++ accepts one.
- A move on both paths of an `if` is refused. The move is written once, after the `if`.
- No copy constructor runs by itself. Every copy of an owning value stands in the source as `dup`.
- A class replaces the copy with `concrete fn copy(self, to: *Object)`, which fills `to` and allocates no object.

## Teardown and temporaries

### What you know

C++ runs the destructors of the locals at the end of their scope, last to first. Those of the temporaries run at the end of the full expression. Java has no destructor. A `finally` block or try-with-resources closes the files and locks that the collector leaves open. C frees by hand on every path out of a function, which is where its leaks come from.

### The feature

A local that owns something is torn down at the end of its block. That holds on every exit: the closing brace, `return`, `break`, `continue`, `fail` and a `try` that forwards an error. The effect is that of `defer destroy(&x);` written after the `let`. The locals and the `defer` statements of a block run in one order, the last one first.

The teardown of a value goes part by part.

- A class value runs the `destruct` of every class of its chain, the concrete class first. Then its fields go, last to first.
- A struct and a tuple tear down their parts last to first, and an array its elements last to first.
- A `?T` is torn down when it holds a value, and a variant by the case its tag names.
- A part that owns nothing costs nothing.

An object on the heap is never torn down by itself. `delete(p)` runs the teardown and frees the memory. `destroy(p)` runs the teardown and keeps the memory, which `free(p)` then gives back. `delete` takes a pointer to a class. A struct on the heap that owns something ends with `destroy(p)` and `free(p)`.

`destroy(&x)` tears a local down before the end of its block. It counts as a move, so the name is dead after it and the block tears nothing down a second time. A part of a local cannot be destroyed alone.

`=` into a place that holds a value destroys the old value first and then moves the new one in. `=` to an `own` field deletes the object the field held, or tears down the elements of its slice and frees the buffer.

An `own` parameter is a local of its function. It is torn down at every exit, unless it moved on.

A **temporary** is a fresh value that no local holds: a call result or a literal that an expression uses and nothing binds. It is torn down by the function that made it, at one of three places.

| The temporary | Where it is torn down |
|---|---|
| An argument of a parameter that is not `own` | right after the call that took it |
| A value read through a field, `make().n`, or used as the object of a call, `make().f()` | at the end of the statement |
| A call result that a statement drops | at the end of the statement |

A condition of `if` or of a loop ends its temporaries before its branch runs. A `return` ends them before it leaves.

A temporary that goes to an `own` parameter, into a `let`, into a field or out by `return` moves there and is no temporary any more.

```anti
//! name: teardown
import anti.io;
import anti.text;

class Res
{
	pub n: int = 0,

	fn destruct(self)
	{
		let line = f"  destruct {self.n}";
		io.println(line);
		free(line.ptr);
	}
}

struct Pair
{
	first: Res,
	second: Res,
}

fn res(n: int) -> Res
{
	return Res { n: n };
}

fn look(r: Res) -> int
{
	return r.n;
}

fn early(stop: bool) -> int
{
	let a = res(1);
	if stop {
		return 0;
	}
	let b = res(2);
	return a.n + b.n;
}

fn main() -> int
{
	io.println("block");
	{
		let a = res(1);
		let p = Pair { first: res(2), second: res(3) };
		io.println("end of block");
	}
	io.println("early(true)");
	early(true);
	io.println("early(false)");
	early(false);
	io.println("arguments");
	let n = look(res(4)) + look(res(5));
	io.println("field reads");
	n += res(6).n + res(7).n;
	io.println("after the statement");
	io.println("dropped result");
	res(8);
	io.println("loop");
	for i in 0..3 {
		let r = res(10 + i);
		if i == 1 {
			break;
		}
	}
	io.println("assignment");
	let s = res(20);
	s = res(21);
	io.println("heap");
	let h = alloc Res { n: 7 };
	let k = alloc Res { n: 8 };
	destroy(k);
	free(k);
	delete(h);
	let line = f"sizes: Res {size_of(Res)}, Pair {size_of(Pair)}";
	io.println(line);
	free(line.ptr);
	io.println("end of main");
	return n - 22;
}
```

```text output
block
end of block
  destruct 3
  destruct 2
  destruct 1
early(true)
  destruct 1
early(false)
  destruct 2
  destruct 1
arguments
  destruct 4
  destruct 5
field reads
  destruct 7
  destruct 6
after the statement
dropped result
  destruct 8
loop
  destruct 10
  destruct 11
assignment
  destruct 20
heap
  destruct 8
  destruct 7
sizes: Res 16, Pair 32
end of main
  destruct 21
```

- The block tears down `p` before `a`, and inside `p` the part `second` before `first`.
- `early(true)` leaves by its first `return` with one local, and `early(false)` by its second with two.
- The argument `res(4)` goes right after `look` returns, before `res(5)` is made.
- The values of `res(6).n` and `res(7).n` both live to the end of the statement and go last to first.
- The local of the loop body goes at the end of each turn, and on the `break`.
- `s = res(21)` tears down the 20 that `s` held.

The checker refuses each of the following.

```anti refused
//! error: `k` was moved into `destroy`
class Res
{
	pub n: int = 0,

	fn destruct(self)
	{
	}
}

fn main() -> int
{
	let k = Res { };
	destroy(&k);
	return k.n;
}
```

```anti refused
//! error: `q.first` is a part of `q`. Destroy the whole of `q`, or give the part a class with its own teardown
class Res
{
	pub n: int = 0,

	fn destruct(self)
	{
	}
}

struct Pair
{
	first: Res,
	second: Res,
}

fn main() -> int
{
	let q = Pair { first: Res { }, second: Res { } };
	destroy(&q.first);
	return 0;
}
```

```anti refused
//! error: `delete` needs a class pointer, found `*Pair`
class Res
{
	pub n: int = 0,

	fn destruct(self)
	{
	}
}

struct Pair
{
	first: Res,
	second: Res,
}

fn end(p: *Pair)
{
	delete(p);
}
```

```anti refused
//! error: `Pair` owns what its parts own, use `dup` instead of `=`
class Res
{
	pub n: int = 0,

	fn destruct(self)
	{
	}
}

struct Pair
{
	first: Res,
	second: Res,
}

fn main() -> int
{
	let p = Pair { first: Res { }, second: Res { } };
	let q = p;
	return 0;
}
```

```anti refused
//! error: `Res` has a `destruct`, use `dup` instead of `=`
class Res
{
	pub n: int = 0,

	fn destruct(self)
	{
	}
}

fn main() -> int
{
	let a = Res { };
	let b = Res { n: 2 };
	b = a;
	return 0;
}
```

### How antic builds it

antic writes the teardown of a local once per exit of its block. No table of cleanups exists at run time, and no code runs to register a local.

```ir excerpt=teardown target=linux-x86_64 mode=dev
fn teardown.early(%0: i8 zeroext) -> i64 {
...
    branch %0, b1, b2
b1:
    %4 = copy i64 0
...
b4:
    call void @teardown.Res.destroy(%1, 0)
    ret i64 %4
...
b8:
    call void @teardown.Res.destroy(%2, 0)
...
b10:
    call void @teardown.Res.destroy(%1, 0)
    ret i64 %15
}
```

- `%1` is the slot of `a` and `%2` the slot of `b`.
- `b1` is the first `return`. It computes the result into `%4`, and `b4` tears down `a` and returns.
- `b8` and `b10` belong to the second `return`. They tear down `b` and then `a`, the last local first.
- The call of the teardown of `a` stands in the function twice, once per exit.

A test for a zero table stands before each of those calls.

```ir excerpt=teardown target=linux-x86_64 mode=dev
    %5 = load ptr %1
    %6 = eq i8 %5, 0
    branch %6, b3, b4
...
b3:
    %7 = addr @teardown.2
    call void @anti_rt_table_unset(%7, 3)
    unreachable
```

`a` never moves, so a zero table there is an error of the program, such as a value that C code cleared. `anti_rt_table_unset` ends the program with the name of the class. A local that moves somewhere takes the other form of the test, which passes over the teardown.

An argument that is a temporary is torn down right after its call.

```ir excerpt=teardown target=linux-x86_64 mode=dev
    %36 = call agg @teardown.res(4)
    %37 = call i64 @teardown.look(%36)
...
    call void @teardown.Res.destroy(%36, 0)
    %41 = call agg @teardown.res(5)
```

A temporary that lives to the end of its statement may be made on one path of the statement alone. A slot of the frame therefore holds its address, and zero before it is made.

```ir excerpt=teardown target=linux-x86_64 mode=dev
    %52 = call agg @teardown.res(6)
    store ptr %52, %53
...
    %64 = load ptr %57
    %65 = ne i8 %64, 0
    branch %65, b17, b18
...
    call void @teardown.Res.destroy(%64, 0)
    store ptr 0, %57
```

- `store ptr %52, %53` records the address of the value of `res(6)` in the slot `%53`.
- At the end of the statement the code loads each slot, the last one first. `%57` is the slot of `res(7)`.
- A slot that holds an address gets its teardown and then zero.

The teardown of the class is the function of the section on `own`. For `Res` it is two calls.

```llvm excerpt=teardown target=linux-x86_64 mode=release
define internal void @teardown.Res.destroy(ptr noundef %p0, ptr nofree readnone captures(none) %p1) #0 {
b0:
  tail call void @anti_rt_hook(ptr %p0, i64 1)
  tail call void @teardown.Res.destruct(ptr %p0)
  ret void
}
```

`destroy(k)` of a heap object is `anti_rt_destroy(k, descriptor)`, which calls the same function through the table. `delete(h)` is `anti_rt_delete(h, descriptor)`, which does the same and then calls `free`.

Where it lives: `destroy_local`, `push_exit_action`, `drop_result`, `lower_keep_temp` and `lower_end_temps` of `src/antic/lower_stmt.c`. `fresh_argument` of `src/antic/lower_expr.c` finds the arguments that are temporaries. `lower_destroy_owned` of `src/antic/lower_owning.c` is the one teardown of a value of any type. `anti_rt_destroy` and `anti_rt_delete` of `src/rt/object.c`.

### What it costs

The lowering is the same in dev and in release, and both keep the test for a zero table.

| Cost | Dev and release |
|---|---|
| Bytes of a `Res` | 16: the table pointer and `n` |
| Bytes of a `Pair` | 32, with no word for the teardown |
| Code per local that owns something | one teardown sequence per exit of its block |
| Teardown of a class local | one load, one compare, one branch, one direct call of `<Class>.destroy` |
| Inside `<Class>.destroy` | one `anti_rt_hook`, one direct call per `destruct` of the chain, the teardown of each owning field |
| A temporary argument | the same sequence, once, after the call |
| A statement temporary | one slot of 8 bytes in the frame, one store when made, one load and one test at the end of the statement |
| `delete(p)` | one `anti_rt_delete`, one indirect call of the teardown, one `free` |
| Atomic operations | two loads in each `anti_rt_hook` |
| Heap allocations, locks | 0 |

A value that owns nothing has no teardown, and its block emits no code for it. `--no-hooks` drops the call of `anti_rt_hook`, with the hooks of [chapter 15]({{% relref "15-hooks" %}}).

`early` has two exits and two locals, and its body holds three calls of `teardown.Res.destroy`. A function with many exits and many owning locals grows by that product.

### Pitfalls

For a Java programmer:

- The teardown runs at a known place, the end of the block. A `destruct` may close a file or release a lock, and no `finally` is needed for it.
- An object of `alloc` has no such place. Without `delete` it stays until the program ends.

For a C programmer:

- A `free` by hand of what a local owns frees it twice, since the block frees it again.
- A `return` in the middle of a function tears down the locals made so far. No `goto cleanup` is written.
- `destroy(p)` does not free. The memory of `p` is still allocated after it.

For a C++ programmer:

- An argument temporary dies right after its call, before the rest of the full expression runs. A pointer that the call returned into what the temporary owns dangles in the same statement.
- A value read through a field or used as the object of a call lives to the end of the statement, as in C++.
- A constructor that fails runs the `destruct` of each base that was built and no `destruct` of its own class, as [chapter 9]({{% relref "09-classes" %}}) shows.
- `delete` of a pointer to a struct is refused. `destroy(p)` and `free(p)` are the two steps.

## The rules at a call

### What you know

Java passes every object as a reference by value, so the callee and the caller reach one object and neither owns it. C passes bytes, and a comment says whether the callee frees a pointer it receives. C++ chooses per parameter: by value with a copy, by reference, or by value of a `unique_ptr` with a move at the call.

### The feature

The word before a parameter says who owns the argument after the call. No word means that the argument stays with the caller.

| Parameter | What the call passes | Who tears the value down | What the function may do with it |
|---|---|---|---|
| `b: Box` | the bytes of the value | the caller | read it, change its own bytes, pass it to a parameter of the same kind, copy it with `dup(b)` |
| `p: *Box` | the address | no one, at this call | read and change the object, store the pointer, return it, but neither `delete` nor `destroy` it |
| `lent p: *Box` | the address | no one, at this call | read and change the object, pass the pointer to a `lent` parameter |
| `own b: Box` | the value itself, by a move | the function, at every exit | everything a local allows: bind it, store it, return it, pass it on |
| `own p: *Box` | the address, and the caller's name is dead | no one by itself | everything, and `delete(p)` is the function's to write |

A parameter without `own` **belongs to the caller**. Where its value owns something, the function gets the bytes, and the ownership stays with the caller. The checker refuses every way that would give the value a second owner. Those are `let`, `=`, a tuple, struct, class or variant literal, an `own` argument and `return`. The message names the two fixes, `own` on the parameter and `dup(b)`. In generic code the same holds for a parameter whose type is a type parameter, since the `T` of a copy may own something.

A function deletes or destroys only what it received as `own`. `delete(p)` and `destroy(p)` of a parameter without `own` are refused, and so is `destroy(&b)`, with a message that names `own`. A part of a singleton lives as long as the program, so `delete` and `destroy` refuse it too. `free` stays for memory from C.

A write to a field of such a parameter changes the bytes the function received. The caller's value keeps its field. What the value owns is shared for the length of the call, so a write through an `own` pointer of the parameter reaches the caller's memory.

An `own` parameter takes the argument over.

- A local passed there moves, by the rules of the last section, and a fresh value needs nothing.
- Inside the function the parameter moves by `let`, `=`, `return`, a literal and another `own` argument. After any of them the name is dead.
- At every exit where it has not moved, the function tears it down.
- A value that holds no pointer and owns nothing is copied, and the caller keeps its local.

`own` before a pointer parameter hands over the pointer and nothing more. The caller's name is dead after the call. The object is on the heap, and no block tears a heap object down. The function ends it with `delete(p)` or stores the pointer in an `own` field.

A plain pointer owns nothing. In a generic, `own x: T` whose `T` is a plain pointer such as `*Key` owns the pointer value, one word, and not the object it names. A copy of the generic that does not hand `x` on deletes nothing, and the object stays with whoever else holds it. Only `own p: *Key`, written with the pointer, owns the object.

The object of a call, `self`, is a pointer to the object. A function of a class reads and changes the object in place and takes no ownership of it.

`keep` belongs to parameters of function type. A function may store such a parameter only when it is marked `keep`, and `keep own` takes a `snapshot fn` with its memory. [Chapter 5]({{% relref "05-functions" %}}) holds both.

```anti
//! name: calls
import anti.io;
import anti.text;

class Box
{
	pub own data: ?*int = none,
	pub n: int = 0,

	fn destruct(self)
	{
		let line = f"  destruct {self.n}";
		io.println(line);
		free(line.ptr);
	}
}

fn look(b: Box) -> int
{
	b.n += 100;
	return b.n;
}

fn bump(p: *Box)
{
	p.n += 1;
}

fn grow(lent p: *Box)
{
	p.n += 10;
}

fn take(own b: Box) -> int
{
	return b.n;
}

fn end(own p: *Box)
{
	delete(p);
}

fn larger(own a: Box, own b: Box) -> Box
{
	if a.n > b.n {
		return a;
	}
	return b;
}

fn main() -> int
{
	let a = Box { n: 1 };
	let seen = look(a);
	bump(&a);
	grow(&a);
	let line = f"look {seen}, a.n {a.n}";
	io.println(line);
	free(line.ptr);
	io.println("take(a)");
	let n = take(a);
	io.println("take of a literal");
	n += take(Box { n: 2 });
	io.println("larger");
	let big = larger(Box { n: 3 }, Box { n: 4 });
	let p = alloc Box { n: 5 };
	bump(p);
	io.println("end(p)");
	end(p);
	io.println("end of main");
	return n + big.n - 18;
}
```

```text output
look 101, a.n 12
take(a)
  destruct 12
take of a literal
  destruct 2
larger
  destruct 3
end(p)
  destruct 6
end of main
  destruct 4
```

`look` adds 100 to its own bytes and returns 101, and `a.n` is still 1 after it. `bump` and `grow` write through a pointer and bring `a.n` to 12. `larger` takes two values, returns one and tears down the other, the 3. The 4 moved out into `big` and goes at the end of `main`.

The checker refuses each of the following.

```anti refused
//! error: `b` belongs to the caller and does not move. Mark it `own` to take it over, or copy it with `dup(b)`
class Box
{
	pub own data: ?*int = none,
}

fn same(b: Box) -> Box
{
	return b;
}
```

```anti refused
//! error: `b` belongs to the caller and does not move. Mark it `own` to take it over, or copy it with `dup(b)`
class Box
{
	pub own data: ?*int = none,
}

class Shelf
{
	pub slot: Box,
}

fn put(s: *Shelf, b: Box)
{
	s.slot = b;
}
```

```anti refused
//! error: `b` belongs to the caller and does not move. Mark it `own` to take it over, or copy it with `dup(b)`
class Box
{
	pub own data: ?*int = none,
}

fn take(own b: Box)
{
}

fn hand_on(b: Box)
{
	take(b);
}
```

```anti refused
//! error: `b` was moved into `take`
class Box
{
	pub own data: ?*int = none,
}

fn take(own b: Box)
{
}

fn twice(own b: Box)
{
	take(b);
	take(b);
}
```

```anti refused
//! error: `b` was moved into `c`
class Box
{
	pub own data: ?*int = none,
	pub n: int = 0,
}

fn bind(own b: Box) -> int
{
	let c = b;
	return b.n;
}
```

```anti refused
//! error: `Box` has `own` fields, and a value that stays where it is does not move into `take`, use `dup`
class Box
{
	pub own data: ?*int = none,
}

fn take(own b: Box)
{
}

fn from_pointer(p: *Box)
{
	take(*p);
}
```

```anti refused
//! error: `xs` was moved into `total`
fn total(own xs: []int) -> int
{
	return xs.len;
}

fn main() -> int
{
	let numbers: [3]int = [1, 2, 3];
	let xs = numbers[0..2];
	let n = total(xs);
	return xs.len - n;
}
```

### How antic builds it

The kind of a parameter shows in the signature of the function.

```llvm excerpt=calls target=linux-x86_64 mode=dev
define hidden noundef i64 @calls.look(ptr byval([24 x i8]) align 8 %p0) #0 {
...
define hidden void @calls.bump(ptr noundef nonnull dereferenceable(24) %p0) #0 {
...
define hidden void @calls.grow(ptr noundef nonnull dereferenceable(24) %p0) #0 {
...
define hidden noundef i64 @calls.take(ptr byval([24 x i8]) align 8 %p0) #0 {
...
define hidden void @calls.end(ptr noalias noundef nonnull dereferenceable(24) %p0) #0 {
...
define hidden void @calls.larger(ptr sret([24 x i8]) align 8 %sret, ptr byval([24 x i8]) align 8 %p0, ptr byval([24 x i8]) align 8 %p1) #0 {
```

- `look` and `take` receive a `Box` by value. A value of 24 bytes passes in the memory of the stack on linux-x86_64, which `byval` says. The plain parameter and the `own` one have the same type.
- `bump` and `grow` receive a pointer. `nonnull` and `dereferenceable(24)` come from the type `*Box`, and `lent` adds nothing.
- `end` receives an `own` pointer, which is `noalias`: the optimizer may assume that no other pointer of the function reaches the object.
- `larger` returns a `Box` through the pointer `%sret`, which the caller passes.

A class value of 16 bytes passes in two registers. On arm64 a value above 16 bytes passes as a pointer to a copy that the caller makes with a `memcpy` before the call.

The difference between the plain and the `own` parameter is in the code around the call and at the exits. The caller passes a plain argument where it stands and clears nothing.

```ir excerpt=calls target=linux-x86_64 mode=dev
    %6 = call i64 @calls.look(%0)
    %7 = copy i64 %6
    call void @calls.bump(%0)
    call void @calls.grow(%0)
...
    memcopy %26, %0, calls.Box
    store ptr 0, %0
    %27 = call i64 @calls.take(%26)
```

`%0` is the slot of `a`. `look` receives its bytes and `bump` its address, and the IR of antic writes both as `%0`. The call of `take` has the copy and the store of a move.

A function with two `own` parameters tears down on each exit the one that did not move.

```ir excerpt=calls target=linux-x86_64 mode=dev
b1:
    %7 = load ptr %1
    %8 = ne i8 %7, 0
    branch %8, b3, b4
b2:
    %9 = load ptr %0
    %10 = ne i8 %9, 0
    branch %10, b5, b6
b3:
    call void @calls.Box.destroy(%1, 0)
    jump b4
b4:
    ret ptr %0
b5:
    call void @calls.Box.destroy(%0, 0)
    jump b6
b6:
    ret ptr %1
```

`b1` is `return a;`. It tears down `b`, in `%1`, and returns `a`. `b2` is `return b;` and tears down `a`. The returned parameter is not cleared and not tested: the checker knows that it moved on this path.

Where it lives: `sema_refuse_caller_value` and `sema_note_move` of `src/antic/sema_value.c`. `lower_push_own_action` and `lower_move_argument` of `src/antic/lower_stmt.c`. `param_facts` of `src/antic/llvm_emit_call.c` writes `noalias`, and `abi_classify` of `src/antic/abi.c` decides between registers and memory.

### What it costs

The lowering is the same in dev and in release. The table gives what it writes for the `Box` of 24 bytes at a call that opt does not inline.

| Parameter | At the call | In the function |
|---|---|---|
| `b: Box` | one copy of 24 bytes | nothing |
| `p: *Box` | one register | nothing |
| `lent p: *Box` | one register | nothing |
| `own b: Box` from a local | one copy of 24 bytes into a slot, one store of zero, one copy of 24 bytes | per exit one load, one compare, one branch, one call of `Box.destroy` |
| `own b: Box` from a fresh value | one copy of 24 bytes | the same |
| `own p: *Box` | one register | nothing: `delete` is written by hand |

No kind of parameter allocates, calls the runtime at the call, or takes a lock. The 24 bytes are the cost of passing a class value of that size and fall to two registers for a value of 16 bytes. A pointer parameter avoids the copy for a larger value.

### Pitfalls

For a Java programmer:

- A parameter of a class type is the value. A write to its field stays in the function. A parameter of type `*Box` is the reference.
- After `take(a)` the name `a` is gone. Java has no call that ends the caller's variable.

For a C programmer:

- A class value passed without `own` is not torn down by the callee. A `destroy(&b)` there would end what the caller still owns.
- `own p: *Box` frees nothing by itself. A function that takes it and returns without `delete(p)` leaks the object.
- A plain `p: *Box` may be stored by the function. The signature does not say so, and `lent` is the word that forbids it.

For a C++ programmer:

- The move is at the call site without a word. `take(a)` moves, where C++ writes `take(std::move(a))`.
- A plain value parameter is the bytes of the argument. No copy constructor runs, and what the bytes own stays with the caller.
- `return b;` of a plain parameter is refused, where C++ copies or moves. The parameter needs `own`, or the function returns `dup(b)`.

## Shared objects

### What you know

`std::shared_ptr<T>` of C++ counts its owners in a control block, and every copy of the pointer changes the count without a word in the source. Java's collector finds the objects that nothing reaches, cycles included, at a time of its own choosing. C counts by hand with a pair of functions such as `retain` and `release`.

### The feature

`anti.mem.Shared<T>` gives one object more than one owner. Each owner holds a **handle**, and the object holds the number of its handles.

| Operation | What it does |
|---|---|
| `Shared<T>.new(value)` | takes `value`, moves it to the heap and gives the first handle, with the count 1 |
| `h.share()` | gives another handle of the same object and adds 1 to the count |
| `dup(h)` | the same as `h.share()` |
| `h.count()` | the number of handles at this moment |
| `h.read(f)` and `h.modify(f)` | call `f` with the object as a `lent *T` |
| the teardown of a handle | subtracts 1, and destroys and frees the object when the handle was the last |

A handle is a class value that owns something. Every rule of the chapter holds for it. `=` refuses to copy one, a handle moves into an `own` parameter, and a local handle is torn down at the end of its block. A collection of handles, `List<Shared<T>>`, counts one for each element and gives each count back when the element goes.

No pointer to the object leaves a handle. `read` and `modify` lend it for one call, so the pointer cannot be stored. The two functions differ in name alone, and both lend a pointer that may change the object.

The count is atomic. The handles of one object may be made, used and torn down on more than one thread at once. The object itself is not locked: `T` must be a thread-safe type of [chapter 14]({{% relref "14-concurrent-classes" %}}) for more than one thread to change it.

Two objects that each hold a handle of the other are never freed. Each keeps the count of the other at 1 or above. The fix is a plain pointer for one of the two links.

```anti
//! name: shared
import anti.io;
import anti.text;
import anti.collection.list.{List};
import anti.mem.{Shared};

class Circle
{
	pub r: int = 0,

	fn destruct(self)
	{
		let line = f"  destruct circle {self.r}";
		io.println(line);
		free(line.ptr);
	}
}

fn radius(c: *Shared<Circle>) -> int
{
	let r = 0;
	c.read(fn(p) { r = p.r; });
	return r;
}

fn main() -> int
{
	let c = Shared<Circle>.new(Circle { r: 2 });
	let shapes = List<Shared<Circle>>.new();
	shapes.push(c.share());
	shapes.push(c.share());
	let line = f"count {c.count()}, radius {radius(&c)}";
	io.println(line);
	free(line.ptr);
	c.modify(fn(p) { p.r = 3; });
	{
		let other = dup(c);
		line = f"count {other.count()} in the block";
		io.println(line);
		free(line.ptr);
	}
	shapes.clear();
	line = f"count {c.count()}, radius {radius(&c)}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Shared<Circle> {size_of(Shared<Circle>)}, Circle {size_of(Circle)}";
	io.println(line);
	free(line.ptr);
	io.println("end of main");
	return 0;
}
```

```text output
count 3, radius 2
count 4 in the block
count 1, radius 3
sizes: Shared<Circle> 16, Circle 16
end of main
  destruct circle 3
```

The count reaches 4 inside the block and falls back when `other` is torn down. `shapes.clear()` tears down two handles. The circle is destroyed once, when `c`, the last handle, goes at the end of `main`.

The next program builds a cycle. `a` and `b` each hold a handle of the other, and neither `destruct` runs.

```anti
//! name: cycle
import anti.io;
import anti.text;
import anti.mem.{Shared};

class Node
{
	pub name: str = "",
	pub peer: ?Shared<Node> = none,

	fn destruct(self)
	{
		io.println(self.name);
	}
}

fn main() -> int
{
	let a = Shared<Node>.new(Node { name: "  destruct a" });
	let b = Shared<Node>.new(Node { name: "  destruct b" });
	let lone = Shared<Node>.new(Node { name: "  destruct lone" });
	a.modify(fn(p) { p.peer = b.share(); });
	b.modify(fn(p) { p.peer = a.share(); });
	let line = f"counts {a.count()} {b.count()} {lone.count()}";
	io.println(line);
	free(line.ptr);
	io.println("end of main");
	return 0;
}
```

```text output
counts 2 2 1
end of main
  destruct lone
```

The checker refuses each of the following.

```anti refused
//! error: `Shared<Circle>` has `own` fields, use `dup` instead of `=`
import anti.mem.{Shared};

class Circle
{
	pub r: int = 0,
}

fn main() -> int
{
	let a = Shared<Circle>.new(Circle { r: 1 });
	let b = a;
	return 0;
}
```

```anti refused
//! error: `p` is lent for the call and cannot be stored
import anti.mem.{Shared};

class Circle
{
	pub r: int = 0,
}

class Keeper
{
	pub last: ?*Circle = none,
}

fn main() -> int
{
	let a = Shared<Circle>.new(Circle { r: 1 });
	let k = Keeper { };
	a.read(fn(p) { k.last = p; });
	return 0;
}
```

### How antic builds it

`Shared<T>` is a class of the standard library, written in Anti. A handle is the table pointer and one `own` pointer to a cell. The cell is an object on the heap that holds the count and the value.

```anti
class Cell<T>
{
	pub atomic count: int,
	pub value: T,
}
```

`share()` adds 1 to the count of the cell and builds a second handle that points at the same cell.

```llvm excerpt=shared target=linux-x86_64 mode=release
define internal { i64, i64 } @"anti.mem.Shared$3cshared.Circle$3e.share"(ptr nofree noundef nonnull readonly captures(none) dereferenceable(16) %p0) #1 {
b0:
  %s2 = alloca [16 x i8], align 8
  %v1 = getelementptr inbounds nuw i8, ptr %p0, i64 8
  %v3 = load ptr, ptr %v1, align 8
  %v7 = icmp eq ptr %v3, null
  br i1 %v7, label %b1, label %b2
...
b2:
  %v16 = getelementptr inbounds nuw i8, ptr %v3, i64 8
  %v18 = atomicrmw add ptr %v16, i64 1 seq_cst, align 8
  store ptr @"anti.mem.Shared$3cshared.Circle$3e.table", ptr %s2, align 8
  %v22 = getelementptr inbounds nuw i8, ptr %s2, i64 8
  store ptr %v3, ptr %v22, align 8
  call void @anti_rt_hook(ptr nonnull %s2, i64 0)
```

- The name is `Shared<shared.Circle>.share` with the two brackets escaped. It is the copy of the generic for `Circle`, in the module of the program.
- `%v3` is the pointer to the cell, loaded from offset 8 of the handle.
- The compare with null guards a handle that was torn down already. The branch `b1`, not shown, ends the program with a `NoneDereference`.
- `atomicrmw add` at offset 8 of the cell is the count going up, one atomic instruction with the order `seq_cst`.
- The two stores build the new handle in `%s2`: its table pointer and the same cell pointer. No memory is allocated.
- `anti_rt_hook(%s2, 0)` is the `created` hook of the new handle.

The teardown of a handle is its `destruct`.

```llvm excerpt=shared target=linux-x86_64 mode=release
b2:
  store ptr null, ptr %v1, align 8
  %v13 = getelementptr inbounds nuw i8, ptr %v3, i64 8
  %v15 = atomicrmw sub ptr %v13, i64 1 seq_cst, align 8
  %v17 = icmp eq i64 %v15, 1
  br i1 %v17, label %b3, label %common.ret

b3:
  tail call void @anti_rt_delete(ptr nonnull %v3, ptr nonnull @"anti.mem.Cell$3cshared.Circle$3e.descriptor")
```

- The `store` writes `none` into the handle, so the teardown of its `own` field, which follows `destruct`, frees nothing.
- `atomicrmw sub` takes 1 from the count and gives the value the count held before.
- When that value was 1, the handle was the last. `anti_rt_delete` then tears down the cell, which runs the `destruct` of the `Circle`, and frees it.

The two count operations are one instruction each on x86-64.

```asm excerpt=shared target=linux-x86_64 mode=release
<anti.mem.Shared$3cshared.Circle$3e.destruct>:
movq %rdi, %rax
movq 0x8(%rdi), %rdi
testq %rdi, %rdi
je 0x27 <anti.mem.Shared$3cshared.Circle$3e.destruct+0x27>
movq $0x0, 0x8(%rax)
lock
decq 0x8(%rdi)
jne 0x27 <anti.mem.Shared$3cshared.Circle$3e.destruct+0x27>
```

`lock decq 0x8(%rdi)` is the count going down, and `jne` reads its zero flag. `share()` holds `lock incq 0x8(%rax)` at the same place.

linux-arm64 defaults to the level `armv8.0`, which has no atomic add in one instruction. The count is a loop there.

```asm excerpt=shared target=linux-arm64 mode=release
<anti.mem.Shared$3cshared.Circle$3e.destruct>:
mov x8, x0
ldr x0, [x0, #0x8]
cbz x0, 0x34 <anti.mem.Shared$3cshared.Circle$3e.destruct+0x34>
str xzr, [x8, #0x8]
add x8, x0, #0x8
ldaxr x9, [x8]
subs x9, x9, #0x1
stlxr w10, x9, [x8]
cbnz w10, 0x14 <anti.mem.Shared$3cshared.Circle$3e.destruct+0x14>
```

`ldaxr` loads the count and marks the address, `stlxr` stores the new count if no other thread wrote in between, and `cbnz` tries again otherwise. [Chapter 20]({{% relref "20-targets" %}}) holds the levels at which arm64 has the one instruction.

Where it lives: `Shared` and `Cell` of `src/std/anti/mem.anti`. The atomic operations of a field are in [chapter 13]({{% relref "13-threads" %}}).

### What it costs

The costs are the same in dev and in release.

| Cost | Count |
|---|---|
| Bytes of a handle, `size_of(Shared<Circle>)` | 16: the table pointer and the cell pointer |
| Bytes of the cell on the heap | 32 for a `Circle` of 16: the table pointer, the count and the value |
| `Shared<T>.new(value)` | one `malloc` of the cell, plain stores of the count and the value, two `anti_rt_hook`, no atomic operation on the count |
| `share()` | one atomic add, one `anti_rt_hook` for the new handle, no allocation |
| `dup(h)` | one atomic add, one `anti_rt_hook_copied`, no allocation |
| `count()` | one atomic load |
| `read(f)` and `modify(f)` | one test for `none`, one indirect call of `f`, no atomic operation, no lock |
| The teardown of a handle | one `anti_rt_hook`, one atomic subtract, one `anti_rt_delete_from` of a field that holds `none` |
| The teardown of the last handle | one `anti_rt_delete` on top: the teardown of the value and one `free` |
| Atomic loads inside the hooks | two in each `anti_rt_hook`, one in `anti_rt_hook_copied` |
| Locks | 0 |
| Copies of code | one copy of `Shared<T>` and of `Cell<T>` per type argument, as [chapter 10]({{% relref "10-generics" %}}) counts |

A plain pointer and an `own` field cost none of this. A program pays for the count where it writes `Shared`, and nowhere else.

The cell holds the value in place, so the object and its count are one allocation.

### Pitfalls

For a Java programmer:

- A cycle of handles leaks. No collector finds it.
- `let b = a;` of a handle is refused. A second handle is written as `a.share()`.
- The object is destroyed at the moment the last handle goes, on the thread that held it.

For a C programmer:

- No `release` is written. The teardown of the handle at the end of its block subtracts.
- The object is reached through `read` and `modify` alone, and the pointer they lend cannot be kept.

For a C++ programmer:

- No weak handle exists. A back link is a plain pointer, and the program keeps it valid.
- Every change of the count stands in the source as `share()` or `dup`. Passing a handle to an `own` parameter moves it and changes no count.
- The atomic count makes the handles safe to share between threads. The object is as thread-safe as its type, as with `shared_ptr`.
