---
title: "Concurrent classes"
description: "Synchronized and concurrent classes, the hidden lock, guarded fields, sync blocks, the lock order checks of dev mode, and the thread-safe collections."
summary: "`synchronized class` with its hidden lock, `concurrent class` with its guarded fields, `sync` blocks, the lock order checks of dev mode and the thread-safe collections of `anti.collection`, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T20:23:44+02:00
draft: true
weight: 140
tags: [compilers, programming-languages]
keywords: [synchronized class, concurrent class, hidden lock, guarded fields, sync block, lock order, thread-safe collections]
---

A class is thread-safe when it is declared so, and the checker holds it to the declaration. A `synchronized class` has one hidden lock of 24 bytes per object, and every function that is not private runs under it. A `concurrent class` has no hidden lock. Each of its fields is guarded by a `Mutex` the class names, is atomic, or is written by `construct` alone.

## Synchronized classes

### What you know

Java marks a method `synchronized`, and the method runs under the monitor of its object, which the same thread enters again. Nothing stops a field from being read by a method without the word. C++ puts a `std::mutex` member in the class and a `std::lock_guard` in each member function. C does the same by hand with a `pthread_mutex_t` in the struct.

### The feature

`synchronized` stands directly before `class`, after `abstract`, `final` or `singleton`.

The rules of a synchronized class:

- Every function with `self` that is not private takes the hidden lock of the object as it starts and gives it back on every exit. `pub fn` and `protected fn` are such functions.
- A private function takes no lock. `construct` and `destruct` take none, since no other thread sees the object then.
- A thread that holds the lock takes it again without waiting. A public function may call another on the same object.
- The fields are reached by the functions with `self` of the class and of the classes below it, and by `construct` and `destruct`. Code outside is refused, for a `pub` field as well.
- A function that is not private gives out no pointer and no slice into the fields. It may not return one, and it may not pass one to a function value.
- A synchronized class inherits the root, an abstract class without fields, or another synchronized class. A class that inherits a synchronized class is written `synchronized class` as well.
- An object cannot be copied or assigned as a whole, since it holds a lock.
- A worker takes a pointer to an object of the class, the exception of [chapter 13]({{% relref "13-threads" %}}).

The default `==` of two objects runs under the locks of both, and the default hash under the lock of its object. The section on `sync` blocks describes the order of two locks.

```anti
//! name: tally
import anti.io;
import anti.text;

class Plain
{
	pub count: int = 0,
	pub calls: int = 0,
}

synchronized class Tally
{
	count: int = 0,
	calls: int = 0,

	pub fn add(self, n: int)
	{
		self.count += n;
		self.note();
	}

	fn note(self)
	{
		self.calls += 1;
	}

	pub fn add_twice(self, n: int)
	{
		self.add(n);
		self.add(n);
	}

	pub fn take(self, n: int) -> bool
	{
		if self.count < n {
			return false;
		}
		self.count -= n;
		return true;
	}

	pub fn total(self) -> int
	{
		return self.count;
	}

	pub fn noted(self) -> int
	{
		return self.calls;
	}
}

worker fn fill(chunk: []int, t: *Tally) -> int
{
	for n in chunk {
		t.add_twice(n);
	}
	return chunk.len;
}

fn main() -> int
{
	let t = alloc Tally { };
	let ones: [1000]int = [1; 1000];
	unchecked(escaping-local, "until the step sweep")
	let parts = parallel ones[0..1000] by 8 -> fill(t);
	free(parts.ptr);
	let line = f"total {t.total()}, noted {t.noted()}";
	io.println(line);
	free(line.ptr);
	let got = t.take(500);
	let missed = t.take(5000);
	line = f"take {got} {missed}, total {t.total()}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Plain {size_of(Plain)}, Tally {size_of(Tally)}";
	io.println(line);
	free(line.ptr);
	free(t);
	return 0;
}
```

```text output
total 2000, noted 2000
take true false, total 1500
sizes: Plain 24, Tally 48
```

Eight chunks call `add_twice` on one object from the threads of the pool, and no addition is lost. `add_twice` holds the lock and calls `add`, which takes it again. `note` is private and takes none.

The checker refuses each of the following.

```anti refused
//! error: `count` is a field of the synchronized class `Tally`, which its own functions reach alone
synchronized class Tally
{
	pub count: int = 0,

	pub fn add(self, n: int) { self.count += n; }
}

fn peek(t: *Tally) -> int
{
	return t.count;
}
```

```anti refused
//! error: `at` of the thread-safe class `Tally` gives out a pointer into its fields, return a copy
synchronized class Tally
{
	count: int = 0,

	pub fn at(self) -> *int { return &self.count; }
}
```

```anti refused
//! error: `all` of the thread-safe class `Board` gives out a pointer into its fields, return a copy
synchronized class Board
{
	cells: [4]int = [0; 4],

	pub fn all(self) -> []int { return self.cells[0..4]; }
}
```

```anti refused
//! error: `each` of the thread-safe class `Tally` passes a pointer into its fields to a function value, pass a copy
synchronized class Tally
{
	count: int = 0,

	pub fn each(self, f: fn(*int)) { f(&self.count); }
}
```

```anti refused
//! error: `Wide` inherits the synchronized class `Tally`, and is written `synchronized class`
synchronized class Tally { count: int = 0, }

class Wide inherits Tally { }
```

```anti refused
//! error: the synchronized class `Tally` inherits `Base`, which is neither of its kind nor an abstract class without fields
class Base { pub n: int = 0, }

synchronized class Tally inherits Base { }
```

```anti refused
//! error: `Tally` holds a `Mutex` and cannot be copied, pass a pointer to it
synchronized class Tally
{
	count: int = 0,

	pub fn total(self) -> int { return self.count; }
}

fn snapshot(t: *Tally) -> int
{
	let copy = *t;
	return copy.total();
}
```

```anti refused
//! error: `Tally` holds a `Mutex` and cannot be assigned
synchronized class Tally { count: int = 0, }

fn overwrite(a: *Tally, b: *Tally)
{
	*a = *b;
}
```

```anti refused
//! error: `note` is private to `Tally`
synchronized class Tally
{
	count: int = 0,

	fn note(self) { self.count += 1; }
}

fn poke(t: *Tally)
{
	t.note();
}
```

For a synchronized class the checker proves the following, each by the rules above.

- Every access of a field stands in `construct`, in `destruct` or in a function with `self` of the class or of a class below it.
- Every such function that code outside the class can call holds the lock of `self` from its first statement to its exit.
- No such function hands a pointer or a slice into the fields to its caller or to a function value.

It leaves the following to the programmer.

- A function reaches the fields of another object of its class, `other.count`, under the lock of `self` alone. `sync self, other { }` takes both.
- A function without `self` may call a private function on an object. That call runs with no lock.
- A private function may return a pointer into the fields, and a pointer stored in a field of the object leads out of it. What it points at is not under the lock. A function that is not private gives neither out, also inside a tuple or a struct. It does not return the result of such a private function either. A function that is not private stores no pointer into the fields through an out parameter or in another object.
- Two objects locked in opposite orders by two threads wait for each other. The dev build reports the two orders at run time, as a later section describes.

### How antic builds it

The hidden lock is the last field of the first synchronized class of a chain. It is a struct of 24 bytes on every target.

| Part | Bytes | Holds |
|---|---|---|
| The word | 4 on Linux and macOS, 8 on Windows, padded to 8 | the `Mutex` of [chapter 13]({{% relref "13-threads" %}}) |
| The owner | 8 | a value of the thread that holds the lock, or 0 |
| The depth | 8 | how many times that thread took it |

A `Tally` is the table pointer at offset 0, `count` at 8, `calls` at 16 and the lock at 24. The field has no name a program can spell. It is `transient`, so `serialize`, the field list of reflection and a literal pass over it.

antic writes the lock into the function itself. The call is the first instruction of the body, and the unlock is an exit action of the scope around the body. The module below is `take` of the example after opt.

```llvm excerpt=tally target=linux-x86_64 mode=release
define internal noundef zeroext range(i8 0, 2) i8 @tally.Tally.take(ptr noundef nonnull dereferenceable(48) %p0, i64 noundef %p1) #0 {
b0:
  %v1 = getelementptr inbounds nuw i8, ptr %p0, i64 24
  tail call void @anti_rt_object_lock(ptr nonnull %v1)
  %v4 = getelementptr inbounds nuw i8, ptr %p0, i64 8
  %v6 = load i64, ptr %v4, align 8
  %v9 = icmp slt i64 %v6, %p1
  br i1 %v9, label %common.ret, label %b2

common.ret:
  %common.ret.op = phi i8 [ 1, %b2 ], [ 0, %b0 ]
  tail call void @anti_rt_object_unlock(ptr nonnull %v1)
  ret i8 %common.ret.op

b2:
  %v20 = sub i64 %v6, %p1
  store i64 %v20, ptr %v4, align 8
  br label %common.ret
```

- `%v1` is the address of the hidden lock, `self` plus 24.
- `anti_rt_object_lock` takes it before the first read of `count`.
- `take` has two exits in the source. opt joined them in `common.ret`, which gives the lock back once and returns the flag.
- `b2` is the subtraction and the store, between the lock and the unlock.

The machine code is the same shape: one call before the body and one after it.

```asm excerpt=tally target=linux-x86_64 mode=release
<tally.Tally.take>:
...
leaq 0x18(%rdi), %rbx
movq %rbx, %rdi
callq
movq 0x8(%r14), %rax
subq %r15, %rax
...
movq %rax, 0x8(%r14)
movb $0x1, %r14b
movq %rbx, %rdi
callq
movl %r14d, %eax
```

`leaq 0x18(%rdi)` is the lock at offset 24, kept in `%rbx` for the unlock. The first `callq` is `anti_rt_object_lock` and the second `anti_rt_object_unlock`. The disassembly of an object prints a call without the name of its target.

A lock taken again is a call like the first. `add_twice` holds three pairs: its own, and one around each inlined `add`.

```llvm excerpt=tally target=linux-x86_64 mode=release
define internal void @tally.Tally.add_twice(ptr noundef nonnull dereferenceable(48) %p0, i64 noundef %p1) #0 {
b0:
  %v1 = getelementptr inbounds nuw i8, ptr %p0, i64 24
  tail call void @anti_rt_object_lock(ptr nonnull %v1)
  tail call void @anti_rt_object_lock(ptr nonnull %v1)
...
  tail call void @anti_rt_object_unlock(ptr nonnull %v1)
  tail call void @anti_rt_object_lock(ptr nonnull %v1)
...
  tail call void @anti_rt_object_unlock(ptr nonnull %v1)
  tail call void @anti_rt_object_unlock(ptr nonnull %v1)
  ret void
```

The compiler removes none of the inner pairs. The private `note` is inlined into `add` between its pair, with no call of its own.

`anti_rt_object_lock` reads the owner with one atomic load. When the owner is the calling thread, it adds one to the depth and returns. Otherwise it takes the word as a `Mutex` is taken, stores the thread as the owner and sets the depth to 1. `anti_rt_object_unlock` subtracts one from the depth. At zero it stores 0 as the owner and gives the word back.

The value of a thread is the address of a variable that each thread has its own copy of.

A dev build calls `anti_rt_object_lock_at` with the place of the function, `file:line`, and records the order of the locks.

```llvm excerpt=tally target=linux-x86_64 mode=dev
define hidden noundef zeroext range(i8 0, 2) i8 @tally.Tally.take(ptr noundef nonnull dereferenceable(48) %p0, i64 noundef %p1) #0 {
...
  %v1 = getelementptr i8, ptr %v0, i64 24
...
  call void (ptr, ptr) @anti_rt_object_lock_at(ptr %v2, ptr %v3)
```

The excerpts show the module before the link. The link of a release program runs LTO over the program and the runtime, as [chapter 2]({{% relref "02-how-antic-works" %}}) describes, and may inline the two calls.

Where it lives: `sema_safety_declare`, `sema_hidden_lock` and `sema_check_reach` of `src/antic/sema_safety.c`, `lower_object_lock_call` and `lower_hold_lock` of `src/antic/lower_sync.c`, and `anti_rt_object_lock` and `anti_rt_object_unlock` of `src/rt/lock.c`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes per object | 24 for the hidden lock: `Tally` is 48 and `Plain` 24 | the same |
| Allocations | 0 | 0 |
| A call of a function that is not private | one call of `anti_rt_object_lock_at` and one of `anti_rt_object_unlock_at` | one call of `anti_rt_object_lock` and one of `anti_rt_object_unlock` |
| Taking a free lock | one atomic load, the word as a `Mutex`, one atomic store | the same |
| Taking a lock the thread holds | one atomic load and one add | the same |
| Giving the lock back | one subtract, and at depth zero one atomic store and the word | the same |
| A call of a private function | no lock | no lock |
| The default `==` | the locks of both objects | the same |
| The default hash | the lock of the object | the same |
| The record of the order | per lock that is not taken again | 0 |

No flag turns the lock off. A class that does not need it is declared without `synchronized`.

The copies of code are one unlock call per exit of a function that opt keeps apart. `take` has two exits in the source and one unlock after opt.

### Pitfalls

For a Java programmer:

- Every function that is not private is synchronized. No function of the class opts out, and a getter takes the lock like the rest.
- A field is closed to code outside the class, `pub` or not. Java lets any method read a field of a class with synchronized methods.
- No `wait` and no `notify` exist on the hidden lock. A thread that waits for a state uses a channel.

For a C programmer:

- The lock is 24 bytes of every object, in a class with one `int` field as well. A struct with a `Mutex` field of 4 bytes is the smaller form, with the locking written by hand.
- `size_of` counts the hidden lock, and the layout of the class is no C layout a header can declare by hand.

For a C++ programmer:

- The lock is recursive, as `std::recursive_mutex` is. A plain `Mutex` field blocks a thread that takes it twice.
- A function that returns a reference into the object has no form here. It returns a copy, or takes a function that receives a lent pointer inside the lock.
- A function reads `other.count` of a second object of its class without the lock of `other`. The checker accepts it.

## Concurrent classes

### What you know

Java mixes `synchronized` blocks on chosen locks, `volatile` fields, the classes of `java.util.concurrent.atomic` and `final` fields in one class. C++ mixes `std::mutex` members, `std::atomic` members and `const` members, and C mixes the same by hand. In all three a comment says which lock guards which field, and no compiler reads the comment.

### The feature

`concurrent` stands directly before `class`, where `synchronized` stands. A concurrent class has no hidden lock. The programmer writes the locking, and the checker asks every field to be one of three kinds.

| Kind | Written | Rule |
|---|---|---|
| Guarded | `units: int = 0 guarded by lock,` | every read and write stands inside a `sync` on the `Mutex` field `lock` of the same object |
| Atomic | `atomic reads: int = 0,` | every access is an atomic operation of [chapter 13]({{% relref "13-threads" %}}) |
| Fixed | `limit: int,` | `construct` and `destruct` write it, and nothing else does |

`guarded by` follows the type of a field or its default. It names a `Mutex` field of the same class. In a type nested in the class, `guarded by Outer.lock` names a `Mutex` field of the class around it.

`construct` and `destruct` reach a guarded field without a `sync`, since no other thread sees the object then.

A field that is none of the three fails the safety check `unguarded-field`. A safety check is an error that a clause may overrule. `unchecked(unguarded-field, "reason")` after the type of a field takes that field out of the check. The same clause in the header of the class takes every field out. The clause before a statement overrules the check for that statement. A clause that overrules nothing is itself an error, `unused-unchecked`.

`compare_swap` takes a plain field of one word that `unchecked` marks. That is the form of a lock-free structure.

A concurrent class has no default `==` and no default hash. It declares `concrete fn equals` and `concrete fn hash`, or `operator fn eq` and `operator fn hash`, when it needs them. `=` and `let` copy no object of a concurrent class, also when it holds atomic fields alone and no lock. A fresh value moves into a place, and `dup` copies one.

It shares three rules with a synchronized class. A function that is not private gives out no pointer into the fields. The class inherits its own kind or an abstract class without fields. A worker takes a pointer to an object.

```anti
//! name: stock
import anti.io;
import anti.text;

concurrent class Stock
{
	lock: Mutex,
	units: int = 0 guarded by lock,
	orders: int = 0 guarded by lock,
	atomic reads: int = 0,
	limit: int,
	ticket: int = 0 unchecked(unguarded-field, "claimed with compare_swap"),

	fn construct(self, limit: int)
	{
		self.limit = limit;
	}

	pub fn put(self, n: int) -> bool
	{
		sync self.lock {
			if self.units + n > self.limit {
				return false;
			}
			self.units += n;
			self.orders += 1;
		}
		return true;
	}

	pub fn level(self) -> int
	{
		self.reads.add(1);
		sync self.lock {
			return self.units;
		}
	}

	pub fn claim(self) -> int
	{
		let seen = self.ticket;
		while !self.ticket.compare_swap(seen, seen + 1) do {
			seen = self.ticket;
		}
		return seen;
	}

	pub fn most(self) -> int
	{
		return self.limit;
	}

	pub fn asked(self) -> int
	{
		return self.reads.load();
	}
}

worker fn fill(chunk: []int, s: *Stock) -> int
{
	let taken = 0;
	for n in chunk {
		if s.put(n) {
			taken += 1;
		}
		s.claim();
	}
	return taken;
}

fn main() -> int
{
	let s = alloc Stock(600);
	let ones: [1000]int = [1; 1000];
	unchecked(escaping-local, "until the step sweep")
	let parts = parallel ones[0..1000] by 8 -> fill(s);
	let taken = 0;
	for p in parts {
		taken += p;
	}
	free(parts.ptr);
	let line = f"taken {taken} of 1000, level {s.level()} of {s.most()}, next ticket {s.claim()}, asked {s.asked()}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Stock {size_of(Stock)}";
	io.println(line);
	free(line.ptr);
	delete(s);
	return 0;
}
```

```text output
taken 600 of 1000, level 600 of 600, next ticket 1000, asked 1
sizes: Stock 56
```

`units` and `orders` change together under `lock`. `reads` is counted without it. `limit` is fixed, so `most` reads it with no lock. `ticket` is claimed by `compare_swap`, and 1000 claims from eight chunks give the tickets 0 to 999 once each.

A type nested in a concurrent class is checked as the class is. Its field names the lock of its own object or the lock of the class around it.

```anti
struct Person { age: int }

concurrent class People
{
	struct Node
	{
		lock: Mutex,
		person: Person guarded by lock,
		next: ?*Node guarded by People.lock,
	}

	lock: Mutex,
	head: ?*Node = none guarded by lock,

	pub fn add(self, p: Person)
	{
		let n = alloc Node { person: p, next: none };
		sync self.lock {
			n.next = self.head;
			self.head = n;
		}
	}

	pub fn birthday(self)
	{
		sync self.lock {
			let n = self.head;
			while n != none do {
				sync n.lock {
					n.person.age += 1;
				}
				n = n.next;
			}
		}
	}
}
```

`birthday` holds the lock of the list while it walks the links, and the lock of each node while it changes the person.

The checker refuses each of the following.

```anti refused
//! error: `units` is guarded by `lock`, and no `sync` holds it here [unguarded-field]
concurrent class Stock
{
	lock: Mutex,
	units: int = 0 guarded by lock,

	pub fn level(self) -> int { return self.units; }
}
```

```anti refused
//! error: `units` is guarded by `a`, and no `sync` holds it here [unguarded-field]
concurrent class Stock
{
	a: Mutex,
	b: Mutex,
	units: int = 0 guarded by a,

	pub fn level(self) -> int
	{
		sync self.b {
			return self.units;
		}
	}
}
```

```anti refused
//! error: `units` is guarded by `lock`, and no `sync` holds it here [unguarded-field]
concurrent class Stock
{
	lock: Mutex,
	units: int = 0 guarded by lock,

	pub fn level(self, other: *Stock) -> int
	{
		sync other.lock {
			return self.units;
		}
	}
}
```

```anti refused
//! error: `units` is guarded by `lock`, and no `sync` holds it here [unguarded-field]
concurrent class Stock
{
	lock: Mutex,
	units: int = 0 guarded by lock,

	fn peek(self) -> int { return self.units; }

	pub fn level(self) -> int
	{
		sync self.lock {
			return self.peek();
		}
	}
}
```

```anti refused
//! error: `limit` is not guarded, atomic or fixed [unguarded-field]
concurrent class Stock
{
	limit: int = 9,

	pub fn raise(self) { self.limit += 1; }
}
```

```anti refused
//! error: `guarded by` marks a field of a concurrent class or of a type nested in one
class Stock
{
	lock: Mutex,
	units: int = 0 guarded by lock,
}
```

```anti refused
//! error: `gate` is no `Mutex` field of `Stock`
concurrent class Stock
{
	lock: Mutex,
	units: int = 0 guarded by gate,
}
```

```anti refused
//! error: `hits` is atomic and needs no lock
concurrent class Stock
{
	lock: Mutex,
	atomic hits: int = 0 guarded by lock,
}
```

```anti refused
//! error: `unchecked(unguarded-field)` overrules nothing [unused-unchecked]
concurrent class Stock
{
	limit: int = 3 unchecked(unguarded-field, "never written"),

	pub fn most(self) -> int { return self.limit; }
}
```

```anti refused
//! error: `==` is not defined on `Stock`. A concurrent class has no default `==`
concurrent class Stock
{
	limit: int = 3,

	pub fn most(self) -> int { return self.limit; }
}

fn same(a: *Stock, b: *Stock) -> bool
{
	return *a == *b;
}
```

```anti refused
//! error: `hash` is not defined on `Stock`. A concurrent class has no default hash
concurrent class Stock
{
	limit: int = 3,

	pub fn most(self) -> int { return self.limit; }
}

fn key(a: *Stock) -> u64
{
	return a.hash();
}
```

```anti refused
//! error: `at` of the thread-safe class `Stock` gives out a pointer into its fields, return a copy
concurrent class Stock
{
	lock: Mutex,
	units: int = 0 guarded by lock,

	pub fn at(self) -> *int
	{
		sync self.lock {
			return &self.units;
		}
	}
}
```

```anti refused
//! error: `compare_swap` takes an atomic field, and `n` is a plain one
concurrent class Stock
{
	n: int = 0,

	pub fn take(self) -> bool { return self.n.compare_swap(0, 1); }
}
```

The checker reads one function at a time and compares places as they are written. For a concurrent class it proves the following.

- Every read and write of a guarded field stands inside a `sync` block of the same function. The operand of that `sync` is the `Mutex` field the guard names, on the same place the field is read from.
- A variable that a `let` or a `=` gave `&` of a place in a guarded field, or a slice of it, inside the `sync`, is used in that block alone. A later block on the same lock is after it. A `=` of another value in the block that declares the variable frees it.
- A field that is neither guarded nor atomic is written by `construct` and `destruct` of the class alone. A write from another module is reported at the write.
- An atomic field is reached by an atomic operation alone.
- No function that is not private returns a pointer or a slice into the fields, or passes one to a function value.

The fourth refusal above follows from the first rule. `peek` reads `units` with no `sync` of its own, and the checker does not know that every caller holds the lock. A `Mutex` is not taken twice, so `peek` cannot take it again. The clause `unchecked(unguarded-field, "the caller holds the lock")` before the statement is the form for such a helper.

It leaves the following to the programmer.

- The order of the locks. Two functions that take two locks in opposite orders compile, and the dev build reports them when they run.
- Which fields belong together. Two fields with two locks may each be right and wrong as a pair.
- A sequence of atomic operations. `load` and then `store` are two operations, and another thread may run between them.
- A guard of the class around a nested type. `guarded by People.lock` accepts a `sync` on the `lock` of any `People` object, and the checker does not know which list holds the node.
- What a pointer leads to. A pointer read under the lock is a plain value after the block, and the checker follows it no further. It follows a pointer to the guarded field itself through the variable that took it, and through no field, tuple or result.
- A closure. Its body is checked with no `sync` held, so a guarded field inside it needs a `sync` of its own.
- Every field that `unchecked` marks.
- The time of `construct`. The checker assumes that no other thread has the object yet.

### How antic builds it

A concurrent class has the layout of a plain class. A `Stock` is the table pointer at offset 0, `lock` at 8, `units` at 16, `orders` at 24, `reads` at 32, `limit` at 40 and `ticket` at 48. A guard adds no field and no code of its own. The code is the `sync` the programmer wrote, the atomic instruction and the plain load.

```llvm excerpt=stock target=linux-x86_64 mode=release
define internal noundef i64 @stock.Stock.level(ptr noundef nonnull dereferenceable(56) %p0) #0 {
b0:
  %v1 = getelementptr inbounds nuw i8, ptr %p0, i64 32
  %v3 = atomicrmw add ptr %v1, i64 1 seq_cst, align 8
  %v5 = getelementptr inbounds nuw i8, ptr %p0, i64 8
  tail call void @anti_rt_mutex_lock(ptr nonnull %v5)
  %v8 = getelementptr inbounds nuw i8, ptr %p0, i64 16
  %v10 = load i64, ptr %v8, align 8
  tail call void @anti_rt_mutex_unlock(ptr nonnull %v5)
  ret i64 %v10
```

- `atomicrmw add` is `self.reads.add(1)` on the field at offset 32, outside the lock.
- `anti_rt_mutex_lock` takes the `Mutex` at offset 8. No lock is taken at the entry of the function, as a synchronized class would take one.
- The `load` reads `units` at offset 16 between the two calls.

`claim` holds no call at all.

```llvm excerpt=stock target=linux-x86_64 mode=release
define internal noundef i64 @stock.Stock.claim(ptr nofree noundef nonnull captures(none) dereferenceable(56) %p0)
b0:
  %v1 = getelementptr inbounds nuw i8, ptr %p0, i64 48
  br label %b1

b1:
  %t2.0 = load i64, ptr %v1, align 8
  %v7 = add i64 %t2.0, 1
  %v11 = cmpxchg ptr %v1, i64 %t2.0, i64 %v7 seq_cst seq_cst, align 8
  %v12 = extractvalue { i64, i1 } %v11, 1
  br i1 %v12, label %b3, label %b1

b3:
  ret i64 %t2.0
```

- The `load` is the plain read of `ticket`, which `unchecked` allows.
- `cmpxchg` writes the next ticket when the field still holds what the load saw. The loop tries again otherwise.

A fixed field is one load, `most` of the example, with nothing around it.

The table of a concurrent class without `equals` and `hash` of its own holds two functions that read no field. One compares the addresses of the two objects, and the other hashes the address.

Where it lives: `check_guards_of`, `sema_check_reach`, `sema_note_field_write` and `sema_report_unfixed` of `src/antic/sema_safety.c`, `unchecked_swap` of `src/antic/sema_thread.c`, and `lower_sync` of `src/antic/lower_sync.c`.

### What it costs

The costs are the same in dev and in release, apart from the record of the order of [chapter 13]({{% relref "13-threads" %}}).

| Cost | Count |
|---|---|
| Bytes per object | the fields alone. `Stock` is 56: the table pointer, a `Mutex` in one word and five fields of 8 |
| Bytes of a guard | 0 |
| An access of a guarded field | the `sync` around it: one lock call and one unlock call per block, however many fields the block reaches |
| An access of an atomic field | one atomic instruction |
| A read of a fixed field | one load |
| Allocations | 0 |
| Checks at run time | 0 |

The check `unguarded-field` runs in the compiler. `unchecked` removes the check for a field and adds no code and no cost. It gives up the proof for that field.

### Pitfalls

For a Java programmer:

- A fixed field is no `final`. The word is not written, and the checker finds the rule by reading every write.
- `volatile` has no form. A field that one thread writes and others read is `atomic`, with `load` and `store`.
- A private helper that reads a guarded field is refused, although its callers hold the lock. The annotation `@GuardedBy` of Java tools accepts it.

For a C programmer:

- `guarded by` names a field of the same object. One lock for a group of objects has no form, since a class has no plain static field.
- The `Mutex` of a guard is not recursive. A function that holds it and calls another function that takes it waits for itself in a release build, and a dev build stops it.

For a C++ programmer:

- The class has no `operator==` until it declares one. A container that needs `==` or a hash refuses the class.
- A concurrent class is not movable or copyable as a whole, since it holds a `Mutex`.
- `unchecked` on a field is the whole escape. The checker then reads nothing of that field, in the module of the class and in every module that imports it.

## sync blocks

### What you know

Java writes `synchronized (obj) { }` on any object, and the block gives the monitor back on every exit. C++ writes `std::lock_guard` in a scope, and `std::scoped_lock a_and_b(m1, m2)` takes two mutexes without a deadlock. C pairs `pthread_mutex_lock` and `pthread_mutex_unlock` by hand and orders two locks by a rule of the program.

### The feature

`sync` takes three kinds of operand.

| Form | Holds |
|---|---|
| `sync m { }` | a `Mutex`, or a pointer to one, as [chapter 13]({{% relref "13-threads" %}}) describes |
| `sync obj { }` | the hidden lock of a synchronized object, or of a pointer to one |
| `sync a, b { }` | the hidden locks of two synchronized objects |

`sync obj { }` makes a run of calls on one object one step. The functions inside take the lock again without waiting, and no other thread comes between them.

`sync a, b { }` takes the two locks in the order of the addresses of the two objects, whatever order the source names them in. Two threads that write `sync a, b` and `sync b, a` therefore take them in one order. The same object twice is one lock, taken once.

A `sync` gives its locks back on every exit of the block: its end, `return`, `break`, `continue` and `fail`.

`sync a, b` takes synchronized objects alone. Two `Mutex` values are refused, since a `Mutex` is not taken again by its thread and the program orders two of them itself. No form takes three.

A `sync obj` inside a `sync obj` is accepted, since the hidden lock counts. A `sync m` inside a `sync m` on a `Mutex` is refused.

A concurrent class has no hidden lock, so `sync` on an object of one is refused. Its `Mutex` fields are private to it unless it marks them `pub`.

```anti
//! name: pairs
import anti.io;
import anti.text;

export synchronized class Account
{
	balance: int = 0,

	fn construct(self, start: int)
	{
		self.balance = start;
	}

	pub fn add(self, n: int)
	{
		self.balance += n;
	}

	pub fn get(self) -> int
	{
		return self.balance;
	}
}

export fn transfer(from: *Account, to: *Account, n: int)
{
	sync from, to {
		from.add(-n);
		to.add(n);
	}
}

export fn raise(a: *Account, floor: int) -> bool
{
	sync a {
		if a.get() >= floor {
			return false;
		}
		a.add(floor - a.get());
	}
	return true;
}

worker fn shuffle(chunk: []int, a: *Account, b: *Account) -> int
{
	for n in chunk {
		transfer(a, b, n);
		transfer(b, a, n);
	}
	return chunk.len;
}

fn main() -> int
{
	let x = alloc Account(100);
	let y = alloc Account(100);
	let ones: [4000]int = [1; 4000];
	unchecked(escaping-local, "until the step sweep")
	let parts = parallel ones[0..4000] by 8 -> shuffle(x, y);
	free(parts.ptr);
	let line = f"after 8000 transfers: {x.get()} {y.get()}, equal {*x == *y}";
	io.println(line);
	free(line.ptr);

	transfer(x, y, 30);
	transfer(x, x, 5);
	let low = raise(x, 90);
	let high = raise(x, 90);
	line = f"x {x.get()}, y {y.get()}, raised {low} then {high}, equal {*x == *y}";
	io.println(line);
	free(line.ptr);
	free(x);
	free(y);
	return 0;
}
```

```text output
after 8000 transfers: 100 100, equal true
x 90, y 130, raised true then false, equal false
```

Eight chunks transfer from `x` to `y` and from `y` to `x` at once. Both directions take the two locks in the order of the addresses, so no two threads wait for each other. `raise` reads and writes under one lock, so the test and the change are one step. `transfer(x, x, 5)` takes one lock once.

The checker refuses each of the following.

```anti refused
//! error: `sync a, b` takes two synchronized objects or pointers to them, found `Plain`
synchronized class Account
{
	balance: int = 0,

	pub fn add(self, n: int) { self.balance += n; }
}

class Plain { pub n: int = 0, }

fn run(a: *Account, p: *Plain)
{
	sync a, p {
		a.add(1);
	}
}
```

```anti refused
//! error: expected `{`
synchronized class Account
{
	balance: int = 0,

	pub fn add(self, n: int) { self.balance += n; }
}

fn run(a: *Account, b: *Account, c: *Account)
{
	sync a, b, c {
		a.add(1);
	}
}
```

```anti refused
//! error: `sync` takes a `Mutex`, a synchronized object or a pointer to either, found `Plain`
class Plain { pub n: int = 0, }

fn run(p: *Plain)
{
	sync p {
		p.n += 1;
	}
}
```

```anti refused
//! error: `sync` takes a `Mutex`, a synchronized object or a pointer to either, found `Stock`
concurrent class Stock
{
	lock: Mutex,
	units: int = 0 guarded by lock,
}

fn run(s: *Stock)
{
	sync s { }
}
```

```anti refused
//! error: `balance` is private to `Account`
synchronized class Account { balance: int = 0, }

fn run(a: *Account)
{
	sync a {
		a.balance += 1;
	}
}
```

### How antic builds it

`sync obj` is the call a function of the class makes at its entry, on the same address. `sync a, b` is one call with both addresses.

```llvm excerpt=pairs target=linux-x86_64 mode=release
define internal void @transfer(ptr noundef nonnull dereferenceable(40) %p0, ptr noundef nonnull dereferenceable(40) %p1, i64 noundef %p2)
b0:
  %v1 = getelementptr inbounds nuw i8, ptr %p0, i64 16
  %v3 = getelementptr inbounds nuw i8, ptr %p1, i64 16
  tail call void @anti_rt_object_lock_pair(ptr nonnull %v1, ptr nonnull %v3)
  tail call void @anti_rt_object_lock(ptr nonnull %v1)
...
  tail call void @anti_rt_object_unlock(ptr nonnull %v1)
  tail call void @anti_rt_object_lock(ptr nonnull %v3)
...
  tail call void @anti_rt_object_unlock(ptr nonnull %v3)
  tail call void @anti_rt_object_unlock_pair(ptr nonnull %v1, ptr nonnull %v3)
  ret void
```

- `%v1` and `%v3` are the hidden locks of `from` and `to`, each at offset 16 of an `Account` of 40 bytes.
- `anti_rt_object_lock_pair` compares the two addresses and takes the lower lock first. It takes one lock when both are the same.
- The pairs inside are the inlined `add` on each object, each a lock taken again.
- `anti_rt_object_unlock_pair` is the exit action of the block.

The compare of the addresses is in the runtime, so the module holds no branch for it.

`raise` shows `sync a` with a call inside.

```llvm excerpt=pairs target=linux-x86_64 mode=release
define internal noundef zeroext range(i8 0, 2) i8 @raise(ptr noundef nonnull dereferenceable(40) %p0, i64 noundef %p1)
b0:
  %v1 = getelementptr inbounds nuw i8, ptr %p0, i64 16
  tail call void @anti_rt_object_lock(ptr nonnull %v1)
  tail call void @anti_rt_object_lock(ptr nonnull %v1)
  %v4.i = getelementptr inbounds nuw i8, ptr %p0, i64 8
  %v6.i = load i64, ptr %v4.i, align 8
  tail call void @anti_rt_object_unlock(ptr nonnull %v1)
```

The first lock is the `sync a`, and the second is the entry of the inlined `get`.

The default `==` of a synchronized class takes its two locks with the same pair call. It first returns for one object compared with itself and for two objects of different classes, before it locks.

A dev build calls `anti_rt_object_lock_pair_at` with the place of the `sync` and records the order it took.

Where it lives: `check_sync` and `check_sync_pair` of `src/antic/sema_stmt.c`, `lower_sync` and `lower_sync_pair` of `src/antic/lower_sync.c`, `lower_class_equals` of `src/antic/lower_eq.c`, and `anti_rt_object_lock_pair` of `src/rt/lock.c`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| `sync obj { }` | one call of `anti_rt_object_lock_at`, and one of `anti_rt_object_unlock_at` per exit taken | `anti_rt_object_lock` and `anti_rt_object_unlock` |
| `sync a, b { }` | one call of `anti_rt_object_lock_pair_at` and one of `anti_rt_object_unlock_pair_at` | `anti_rt_object_lock_pair` and `anti_rt_object_unlock_pair` |
| Inside the pair call | one compare of two addresses, and two locks or one | the same |
| A call on the object inside the block | a lock taken again: one atomic load and one add, and the reverse | the same |
| Allocations | 0 | 0 |

A block of five calls on one object under `sync obj` takes the word of the lock once and counts the depth five times. Without the block, each call takes the word.

### Pitfalls

For a Java programmer:

- `sync` takes a `Mutex` or a synchronized object. An object of a plain class has no monitor to take.
- Two nested `sync` blocks on two objects take them in the order written. `sync a, b` is the form that orders them.

For a C programmer:

- The order of `sync a, b` is the order of the addresses at run time. No rule of the program, such as an account number, enters it.
- The unlock is not written. A block that should give the lock back early ends early.

For a C++ programmer:

- `sync a, b` inside a public function of the class of `a` cannot reorder. The function holds the lock of `self` from its entry, so the pair call takes `self` again and then the other. A transfer between two objects belongs in a function that holds neither.
- `sync a, b` has no form for a `Mutex`, where `std::scoped_lock` takes any two.
- No form takes three locks. A third object is a nested `sync` in an order the program chooses.

## Lock order checks

### What you know

A Java thread dump names the threads of a deadlock after it has happened. C and C++ have no check in the language. Tools such as ThreadSanitizer find a lock order inversion in a run that takes the two orders, deadlock or not.

### The feature

A dev build records the order in which each thread takes its locks. When one thread takes lock B while it holds lock A, the runtime keeps the pair A then B. A later thread that takes A while it holds B meets the reverse pair. The runtime then writes one line to standard error and goes on.

The check covers a `Mutex` in a `sync`, the hidden lock of a synchronized class and both locks of `sync a, b`. A lock that its thread takes again is not recorded.

The report comes before the thread waits, so a run that would hang prints its report first. The two orders need not meet in time. One thread that takes both orders, one after the other, is reported as well.

Each pair of locks is reported once. The line names four places, each as `file:line`. Two are where the second lock is taken and where the first was taken. The other two are the same for the reverse order.

A release build records nothing and reports nothing. No option of antic adds the check to a release build or drops it from a dev build.

```anti
//! name: orders
import anti.io;
import anti.text;

synchronized class Account
{
	balance: int = 0,

	fn construct(self, start: int)
	{
		self.balance = start;
	}

	pub fn add(self, n: int)
	{
		self.balance += n;
	}

	pub fn get(self) -> int
	{
		return self.balance;
	}

	pub fn pay(self, to: *Account, n: int)
	{
		self.balance -= n;
		to.add(n);
	}
}

class Vault
{
	pub a: Mutex,
	pub b: Mutex,
	pub n: int = 0,
}

fn main() -> int unchecked(lock-order, "until the step sweep")
{
	let x = alloc Account(100);
	let y = alloc Account(100);
	x.pay(y, 10);
	y.pay(x, 30);
	let line = f"after pay: {x.get()} {y.get()}";
	io.println(line);
	free(line.ptr);

	let v = alloc Vault { };
	sync v.a {
		sync v.b {
			v.n += 1;
		}
	}
	sync v.b {
		sync v.a {
			v.n += 1;
		}
	}
	let p = alloc Account(1);
	let q = alloc Account(1);
	sync p, q {
		v.n += 1;
	}
	sync q, p {
		v.n += 1;
	}
	line = f"vault {v.n}";
	io.println(line);
	free(line.ptr);
	v.a.destroy();
	v.b.destroy();
	free(v);
	free(p);
	free(q);
	free(x);
	free(y);
	return 0;
}
```

```text output
after pay: 120 80
vault 4
```

The test runs the release build, which prints the two lines alone. The program has one thread, so it cannot deadlock. It takes two pairs of locks in both orders all the same. The safety check `lock-order` of the next section refuses the two orders of `v.a` and `v.b`, so the header of `main` overrules it.

`anti run` builds the same file in dev mode as `src/main.anti` of a project. The run then writes two more lines to standard error.

```text
anti: two locks are taken in opposite orders and can deadlock: main.anti:14 takes one while holding the one of main.anti:24, and main.anti:14 takes that one while holding the one of main.anti:24
anti: two locks are taken in opposite orders and can deadlock: main.anti:55 takes one while holding the one of main.anti:54, and main.anti:50 takes that one while holding the one of main.anti:49
```

- Line 24 is `pay` and line 14 is `add`. `x.pay(y, 10)` takes the lock of `y` while it holds the lock of `x`, and `y.pay(x, 30)` takes them the other way round. Two threads that run the two calls at once can each hold one lock and wait for the other.
- Lines 49 and 50 take `v.a` and then `v.b`, and lines 54 and 55 take `v.b` and then `v.a`.
- `sync p, q` and `sync q, p` give no report. Both take the two locks in the order of their addresses.

### The check at compile time

antic refuses an order it can prove in every build, before a run takes it. The safety check `lock-order` builds a graph of the orders in which the program takes its locks. An edge leads from one lock to another wherever code that holds the first takes the second. The code holds the lock of each `sync` block around it, and a public function of a synchronized class holds the hidden lock of its object. A call takes every lock its callee takes, and the graph follows the calls the program names, into the functions of a library as well. A cycle is refused, and the message names both places.

A lock is named by its class and its field, since antic cannot know the objects. `v.a` of every `Vault` is one lock, and so is the hidden lock of every `Account`. A lock that the code holds already adds no edge, so `x.pay(y, 10)` and `y.pay(x, 30)` of the example above pass the check, and the dev check reports them. `sync a, b` adds no edge between its two locks.

```anti refused
//! error: `Vault.a` is locked here while `Vault.b` is held, and line 12 locks them in the other order [lock-order]
class Vault
{
	pub a: Mutex,
	pub b: Mutex,
	pub n: int = 0,
}

fn forward(v: *Vault)
{
	sync v.a {
		sync v.b {
			v.n += 1;
		}
	}
}

fn backward(v: *Vault)
{
	sync v.b {
		sync v.a {
			v.n -= 1;
		}
	}
}

fn main() -> int
{
	let v = alloc Vault { };
	forward(v);
	backward(v);
	free(v);
	return 0;
}
```

The check follows no call through an interface, a function value or a plugin, since the function that runs is not known. The dev check finds an order such a call takes. `unchecked(lock-order, "reason")` overrules the check.

### A wait under a lock

A thread that waits while it holds a lock makes every other thread that needs the lock wait as well, and a thread that would end the wait may need that lock. The warning `blocking-under-lock` names a `recv`, a `send`, a `select`, a `join` or a `join_all` inside a `sync` block or in a function of a synchronized class that holds the hidden lock. It names a call of a function that reaches one as well, through the calls the lock orders follow. Every channel has room for a bounded number of values, so any `send` may find it full, and a `select` waits until one of its channels is ready.

```anti refused
//! error: `recv` may wait while the `sync` of line 10 holds its lock [blocking-under-lock]
struct Inbox
{
	lock: Mutex,
	count: int,
}

fn take(box: *Inbox, c: chan int)
{
	sync box.lock {
		let v = recv(c) else {
			return;
		};
		box.count += *v;
	}
}

fn main() -> int
{
	let box = Inbox { count: 0 };
	let c = chan int(1);
	send(c, 4);
	take(&box, c);
	delete(c);
	return box.count - 4;
}
```

A dev build prints the warning and goes on, and a release build refuses it. The fix takes the value first and holds the lock for what it guards alone. Some waits cannot block for long, such as a `recv` on a channel local to the block that holds its values. There `allow(blocking-under-lock, "reason")` silences the warning, and the reason says why.

### How antic builds it

The check is in the lock calls of a dev build. Each takes the place of the lock as a text.

```llvm excerpt=orders target=linux-x86_64 mode=dev
  call void (ptr, ptr) @anti_rt_mutex_lock_at(ptr %v182, ptr %v183)
...
  call void (ptr, ptr) @anti_rt_mutex_lock_at(ptr %v186, ptr %v187)
...
  call void (ptr, ptr, ptr) @anti_rt_object_lock_pair_at(ptr %v280, ptr %v281, ptr %v282)
```

- The first two calls are `sync v.a` and `sync v.b` of the example. The second argument of each is the address of a constant such as `main.anti:49`.
- The pair call passes one place for both locks.

The release build of the same lines calls `anti_rt_mutex_lock` and `anti_rt_object_lock_pair` with the lock alone, and the module holds no text of a place.

The runtime keeps two structures.

| Structure | Holds | Size |
|---|---|---|
| A stack per thread | the locks the thread holds, each with its place | 32 entries of 16 bytes, in thread-local memory |
| One table of the program | one record per ordered pair of locks, with the two places and a flag | open addressed by the two addresses, 64 records at first, doubled when half full |

A lock call of a dev build first looks at the stack of its thread. With an empty stack it pushes the lock and takes it. With locks on the stack it takes the lock of the table. For each lock held it looks up the reverse pair, reports it once, and records its own pair. It then pushes the lock and takes it. The unlock pops the entry.

A thread that holds more than 32 locks takes the next ones without a record.

`m.destroy()` clears the records that name the `Mutex`. Nothing clears the records of a hidden lock. An object that is freed leaves its records, and a later object at the same address may meet an order it never took.

Where it lives: `order_take`, `order_give` and `order_forget` of `src/rt/lock.c`, and `lock_site` and `lower_hold_lock` of `src/antic/lower_sync.c`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A lock taken while the thread holds none | one push of 16 bytes on the stack of the thread | 0 |
| A lock taken while the thread holds `n` others | the lock of the table, and `n` times two lookups in it | 0 |
| A new pair | one record of 40 bytes in the table | 0 |
| An unlock | one search of the stack from its top, and a pop | 0 |
| Bytes in the program | one text per `sync` and per function of a synchronized class that is not private, `file:line` | 0 |
| Allocations | the table: one `calloc` of 64 records, and one per doubling | 0 |

Every thread that takes a second lock goes through the one lock of the table. A dev build of a program with nested locks on many threads therefore runs them one after the other at that point.

The check is given up in a release build. An inversion that no dev run took is found by no build.

### Pitfalls

For a Java programmer:

- The report is no exception and no dump. It is one line on standard error, and the program runs on.
- A program that never takes the two orders in one dev run gets no report. The check sees the orders a run took.

For a C programmer:

- The record is by address. A freed object and a new one at the same address share their records. A report may name an order the new object never took.
- The record stays after the locks are given back. Two orders an hour apart are reported.

For a C++ programmer:

- The check is in dev builds alone and has no flag. ThreadSanitizer runs where it is asked to.
- It finds inversions of locks and nothing else. A data race on a plain field is the checker's to refuse at compile time, by the rules of the sections above.

## Thread-safe collections

### What you know

Java has `ConcurrentHashMap`, `CopyOnWriteArrayList` and the wrappers of `Collections.synchronizedList`. An iterator of the wrapper needs a `synchronized` block that the programmer writes. The C++ standard library has no thread-safe container, and C has no container.

### The feature

Two modules of the standard library hold six collections for more than one thread.

| Collection | Module | Kind | Locking |
|---|---|---|---|
| `SyncList<T>` | `anti.collection.synchronized` | `synchronized class` | one hidden lock around a `List` |
| `SyncMap<K, V>` | `anti.collection.synchronized` | `synchronized class` | one hidden lock around a `Map` |
| `SyncSet<T>` | `anti.collection.synchronized` | `synchronized class` | one hidden lock around a `Set` |
| `SyncPool<T>` | `anti.collection.synchronized` | `synchronized class` | one hidden lock around a `Pool` |
| `ConcurrentMap<K, V>` | `anti.collection.concurrent` | `concurrent class` | 16 parts by the hash of the key, each with a `Mutex` |
| `SpscRing<T, N>` | `anti.collection.concurrent` | `concurrent class` | no lock, two atomic counters |

Each is made with `new`, takes an optional `Allocator` there, and is torn down as the plain collections of [chapter 16]({{% relref "16-collections" %}}) are. A worker takes a pointer to one.

They have the operations of the plain collections less two groups. No operation takes a position, since a position can go stale between two calls. No operation lends the whole buffer, since a slice of it would be reached without the lock.

The operations that all six share:

- `count()` and `is_empty()`.
- `find_all(test)`, `find_first(test)` and `find_one(test)`, which give copies. `find_one` fails unless exactly one element passes.
- `update_all(test, change)`, `update_first` and `update_one`, where `change` receives a `lent` pointer to the element.
- `remove_all(test)`, `remove_first(test)`, `remove_one(test)` and `clear()`.
- `for x in c`, which walks copies of the elements.

The operations of each:

| Collection | Its own operations |
|---|---|
| `SyncList<T>` | `push`, `pop`, `first`, `last`, `reserve`, `shrink`, `reverse`, `filter`, `sort_with`, and `sort`, `sort_by` and `map` of the module |
| `SyncMap<K, V>` and `ConcurrentMap<K, V>` | `get`, `contains`, `set`, `add`, `remove`, `update`, `read`, `modify`, `keys`, `values`, `capacity`, `reserve`, `shrink` |
| `SyncSet<T>` | `add`, `remove`, `contains`, `union`, `intersect`, `minus`, `is_subset`, `to_set` |
| `SyncPool<T>` | `add`, which gives a `Handle<T>`, and `get`, `set`, `remove`, `contains`, `read` and `modify` by handle |
| `SpscRing<T, N>` | `push`, which gives `false` when the ring is full, `pop`, `peek` and `is_full` |

Reading gives a copy. `get` gives a `?V`, and `pop` a `?T`. A pointer to an element leaves a collection through a `lent` parameter alone: `read(k, f)`, `modify(k, f)` and the `change` of an update.

The functions a program passes to these operations run inside the lock. Such a function may read the collection again, since the hidden lock counts. It must not change the collection, which is walking its elements.

Every element of the list, the maps, the set and the pool carries a version. It is a number that changes with each change of the element. `get_versioned(k)` of a map gives the value and its version. `set_if_version(k, v, version)` writes when the version still stands and fails with `anti.collection.Changed` otherwise. The list gives a version with `find_first_versioned`, the set with `version_of` and the pool with `get_versioned` of a handle. A thread can therefore read, compute outside the lock and write back only if nothing changed.

`==` and `x.hash()` of the four synchronized collections are operators of their module over pointers.

An `SpscRing` is for one producing thread and one consuming thread. The checker does not count the threads. `push` from two threads at once loses elements, and so does `pop` from two.

```anti
//! name: shared
import anti.io;
import anti.text;
import anti.collection.synchronized.{SyncList, SyncMap};
import anti.collection.concurrent.{ConcurrentMap, SpscRing};

class Feeder
{
	pub count: int = 0,
}

worker fn record(chunk: []int, seen: *SyncList<int>, last: *SyncMap<int, int>, hits: *ConcurrentMap<int, int>) -> int
{
	for n in chunk {
		seen.push(n);
		last.set(n % 4, n);
		if !hits.add(n % 10, 1) {
			hits.update(n % 10, fn(v) {
				*v += 1;
			});
		}
	}
	return chunk.len;
}

worker fn feed(f: *Feeder, ring: *SpscRing<int, 64>) -> int
{
	let sent = 0;
	for i in 0..f.count {
		if ring.push(i) {
			sent += 1;
		}
	}
	return sent;
}

fn main() -> int
{
	let seen = SyncList<int>.new();
	let last = SyncMap<int, int>.new();
	let hits = ConcurrentMap<int, int>.new();
	let numbers: [1000]int = [0; 1000];
	for i in 0..1000 {
		numbers[i] = i;
	}
	unchecked(escaping-local, "until the step sweep")
	let parts = parallel numbers[0..1000] by 8 -> record(&seen, &last, &hits);
	free(parts.ptr);

	let sum = 0;
	for x in seen {
		sum += x;
	}
	let sevens = hits.get(7) ?? 0;
	let line = f"list {seen.count()} with sum {sum}, map {last.count()} keys, hits of 7: {sevens}, keys {hits.count()}";
	io.println(line);
	free(line.ptr);

	let ring = SpscRing<int, 64>.new();
	let feeder = alloc Feeder { count: 100 };
	let job = dispatch feeder -> feed(&ring);
	let sent = join(job);
	let got = 0;
	while true do {
		let x = ring.pop() else {
			break;
		};
		got += 1;
	}
	line = f"ring of 64 took {sent} of 100 and gave {got}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: SyncList {size_of(SyncList<int>)}, SyncMap {size_of(SyncMap<int, int>)}, ConcurrentMap {size_of(ConcurrentMap<int, int>)}, SpscRing {size_of(SpscRing<int, 64>)}";
	io.println(line);
	free(line.ptr);
	free(feeder);
	return 0;
}
```

```text output
list 1000 with sum 499500, map 4 keys, hits of 7: 100, keys 10
ring of 64 took 64 of 100 and gave 64
sizes: SyncList 184, SyncMap 368, ConcurrentMap 40, SpscRing 56
```

Eight chunks push into one list, set keys of one map and count into one `ConcurrentMap`. `add` gives `false` for a key that is there, and `update` then counts under the lock of the part, so every one of the 1000 counts lands. The ring takes 64 elements and refuses the other 36, since nothing pops while the producer runs.

The checker refuses each of the following.

```anti refused
//! error: `items` is private to `SyncList<int>`
import anti.collection.synchronized.{SyncList};

fn inside(l: *SyncList<int>) -> int
{
	return l.items.count();
}
```

```anti refused
//! error: `SyncList<int>` has no function `get`
import anti.collection.synchronized.{SyncList};

fn third(l: *SyncList<int>) -> ?int
{
	return l.get(2);
}
```

```anti refused
//! error: an argument of a worker has type `*List<int>`, which holds a pointer. A worker takes and returns values alone
import anti.collection.list.{List};

worker fn fill(chunk: []int, into: *List<int>) -> int
{
	return chunk.len;
}

fn run(data: []int, into: *List<int>)
{
	let parts = parallel data -> fill(into);
	free(parts.ptr);
}
```

### How antic builds it

The four synchronized collections are synchronized classes of the standard library, written in Anti. Each holds the plain collection as a field, with the version of each element beside it, and the hidden lock of the class. `push` of a `SyncList<int>` is the lock, the body of the plain `push` and the unlock.

```llvm excerpt=shared target=linux-x86_64 mode=release
define internal void @"anti.collection.synchronized.SyncList$3cint$3e.push"(ptr noundef nonnull dereferenceable(184) %p0, i64 noundef %p1)
b0:
  %v1 = getelementptr inbounds nuw i8, ptr %p0, i64 160
  tail call void @anti_rt_object_lock(ptr nonnull %v1)
  %v4 = getelementptr inbounds nuw i8, ptr %p0, i64 152
  %v6 = load i64, ptr %v4, align 8
  %v8 = add i64 %v6, 1
  store i64 %v8, ptr %v4, align 8
```

- The symbol is the copy of the generic for `int`. `$3c` and `$3e` are the escaped brackets of `SyncList<int>`, as [chapter 10]({{% relref "10-generics" %}}) describes.
- The hidden lock is at offset 160 of the 184 bytes, the last 24.
- The add and the store raise the clock of the list at offset 152, which gives the new element its version.

The elements of a `SyncList<T>` are stored as tuples `(T, int)` of the value and its version. An element of a `SyncList<int>` takes 16 bytes, where one of a `List<int>` takes 8. A smaller `T` also takes the padding before the `int`.

A `ConcurrentMap` holds a pointer to 16 parts. Each part is a `Mutex`, a `HashMap` of the entries, a `HashMap` of their versions and a clock. An operation hashes the key with the seed of the map, picks the part by the top four bits of the hash and runs inside `sync part.lock`. The table of the part hashes the key again for each of its own operations. The count of the entries is an atomic field, so `count()` takes no lock. An operation over the whole map, such as `find_all` or a walk, takes the 16 locks one after the other and holds one at a time.

An `SpscRing<T, N>` holds a block of `N` elements and two atomic counters: `given`, which the producer writes, and `taken`, which the consumer writes. Each side keeps a plain copy of the counter of the other side. It reads the atomic one only when the copy says the ring is full or empty.

```llvm excerpt=shared target=linux-x86_64 mode=release
define internal noundef zeroext range(i8 0, 2) i8 @"anti.collection.concurrent.SpscRing$3cint$2c$2064$3e.push"(
b0:
  %v1 = getelementptr inbounds nuw i8, ptr %p0, i64 24
  %v3 = load atomic i64, ptr %v1 seq_cst, align 8
  %v5 = getelementptr inbounds nuw i8, ptr %p0, i64 40
  %v7 = load i64, ptr %v5, align 8
  %v10 = sub i64 %v3, %v7
  %v12 = icmp sgt i64 %v10, 63
...
  %v52 = srem i64 %v3, 64
...
  store i64 %p1, ptr %v58, align 8
  %v64 = add i64 %v3, 1
  store atomic i64 %v64, ptr %v1 seq_cst, align 8
```

- The atomic load reads `given` at offset 24, and the plain load reads the producer's copy of `taken` at offset 40.
- The compare with 63 is the test for a full ring of 64. `N` is a constant of the copy.
- `srem` by 64 is the slot, the plain `store` writes the element, and the atomic store publishes the new count.

A `push` into a ring with room is one atomic load, one atomic store and no call.

`for x in c` calls the hook `iter` of the collection, which takes the lock, copies every element into a `Snapshot<T>` on the heap and gives the lock back. The loop then walks the snapshot. A `ConcurrentMap` copies part by part, so its snapshot is no single moment of the whole map.

Where it lives: `src/std/anti/collection/synchronized.anti` and `src/std/anti/collection/concurrent.anti`. `part_of` and `locked_parts` there choose and order the parts of a `ConcurrentMap`.

### What it costs

The costs are the same in dev and in release, apart from the record of the order of the locks.

| Cost | Count |
|---|---|
| `size_of(SyncList<int>)` | 184, of which 24 are the hidden lock |
| `size_of(SyncMap<int, int>)` | 368: two maps, a clock and the hidden lock |
| `size_of(ConcurrentMap<int, int>)` | 40, and one block of 16 parts on the heap |
| `size_of(SpscRing<int, 64>)` | 56, and one block of 64 elements on the heap |
| An operation of a synchronized collection | one hidden lock taken and given back, around the operation of the plain collection |
| A version | 8 bytes per element and the padding of the pair, and one add per change |
| `get` of a `ConcurrentMap` | one `Mutex` of 16 and two hashes of the key: one to pick the part and one in the table of the part |
| `set`, `add` and `update` of a `ConcurrentMap` | one `Mutex` of 16 and one atomic add when the count changes. Four hashes of the key for `set`, and three for an `add` or an `update` that changes the map |
| `count()` of a `ConcurrentMap` | one atomic load |
| An operation over a whole `ConcurrentMap` | 16 locks, one after the other |
| `push` and `pop` of an `SpscRing` with room and with elements | one atomic load and one atomic store, no lock and no call |
| `for x in c` | one lock, and one copy of every element into a snapshot on the heap |
| Copies of code | one copy of the class per set of type arguments, as [chapter 10]({{% relref "10-generics" %}}) counts |

[Chapter 16]({{% relref "16-collections" %}}) holds the cost of each operation of the plain collections inside.

### Pitfalls

For a Java programmer:

- `ConcurrentMap` locks one of 16 parts for a read as well. It has no read without a lock, where `ConcurrentHashMap` has one.
- `for x in c` never fails on a change by another thread and never sees one. It walks a copy made when the loop began.
- A function given to `update_all` or `find_all` runs inside the lock. A slow one holds up every other thread of the collection.

For a C programmer:

- No element is reached by index. `first`, `last`, the find forms, a key or a handle address an element.
- The value a `get` gives is a copy. A change to it does not reach the collection until `set` writes it back.

For a C++ programmer:

- `if !m.contains(k) { m.set(k, v); }` is two steps with two locks. `add(k, v)` is the one step, and `set_if_version` the one for a value computed outside.
- An `SpscRing` with two producers compiles. The class is `concurrent`, its fields are `unchecked`, and the rule of one thread per side is a comment.
- A synchronized collection inside a synchronized class takes two locks per call, one of each object, in the same order every time.
