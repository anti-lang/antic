---
title: "Collections"
description: "The layout, the growth and the cost of each operation of List, Deque, Ring, Grid, the maps and the sets, the sorted ones, Pool, Tree and PriorityQueue."
summary: "The collections of `anti.collection` with their layout and their growth, walking, hashing and its seed, order and equality, and one table with the cost of each operation, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T18:00:00+02:00
draft: true
weight: 160
tags: [compilers, programming-languages]
keywords: [collections, List, Deque, Map, HashMap, Set, SortedMap, Pool, Tree, PriorityQueue, iteration, hashing]
---

A collection is a generic class of the standard library that owns its elements. It is written in Anti and takes its memory from one allocator. Every collection is 96 bytes of shared header and the fields of its own layout. It holds its elements by value in blocks on the heap. The last section holds one table with the cost of each operation.

## List and Deque

### What you know

`ArrayList` of Java holds references to objects on the heap, and `std::vector` of C++ holds the elements themselves in one block that grows by a factor. `ArrayDeque` and `std::deque` add and take at both ends. C has a pointer, a length and a capacity that the programmer keeps by hand, and `realloc`.

### The feature

`List<T>` of `anti.collection.list` is a sequence in one block. `Deque<T>` of `anti.collection.deque` is a ring in one block that takes and gives at both ends. Both hold their elements by value.

`List<T>.new()` makes an empty list. `new(capacity, from)` takes room for `capacity` elements in one allocation and takes every block from the `Allocator` `from`. Both arguments are optional, and the default allocator is `mem.LibcAllocator`. Every growing collection of this chapter is made this way.

A collection is a class value. It lives in a variable, a field or another collection, and its teardown runs at the end of its block. The teardown tears down every element and gives the block back. A function takes a collection by pointer, `l: *List<int>`.

Every collection has the operations of its base `Collection<T>`.

| Operation | Result |
|---|---|
| `c.count` | the number of elements, a public field |
| `c.is_empty()` | whether the count is 0 |
| `c.find_all(test)` | a `Copies<T>`, a collection of copies of every element `test` accepts |
| `c.find_first(test)` | a `?T`, a copy of the first match |
| `c.find_one(test)` | a copy of the one match. It fails with `NotOne` unless exactly one element matches |
| `c.update_all(test, change)` | runs `change` on every match in place and gives their number |
| `c.update_first(test, change)` | the same for the first match, and gives a `bool` |
| `c.update_one(test, change)` | the same for the one match, and fails with `NotOne` otherwise |
| `c.remove_all(test)` | removes every match and gives their number |
| `c.remove_first(test)` | removes the first match and gives it as a `?T` |
| `c.remove_one(test)` | removes the one match and gives it, and fails with `NotOne` otherwise |
| `c.clear()` | removes every element |
| `c.to_text()` | the elements as text, `[1, 2, 3]`, in memory the program frees with `free(s.ptr)` |
| `c.serialize(out)` | the elements as JSON into a `text.Builder` |
| `dup(c)` | a copy with a block of its own and a copy of each element |
| `a == b`, `c.hash()` | equality and the hash, when the elements have them |
| `for x in c` | a walk of the elements |

`test` is a `fn(T) -> bool` and receives a copy of the element. `change` is a `fn(lent *T)` and receives a pointer that is valid for the call.

A list adds the operations of a sequence.

| Operation | Result |
|---|---|
| `l.push(x)` | adds `x` at the end. The list owns it from then on |
| `l.pop()` | takes the last element off, a `?T` |
| `l.insert(i, x)` | puts `x` before the element at `i`. `i` may be `count` |
| `l.remove_at(i)` | takes the element at `i` out and gives it |
| `l[i]` | a copy of the element at `i` |
| `l[i] = x` | replaces the element at `i` and tears the old one down |
| `l.get(i)`, `l.first()`, `l.last()` | a `?T`, which is `none` outside the list |
| `l.read(i, f)`, `l.modify(i, f)` | lend the element at `i` to `f`, and give `false` outside the list |
| `l.lend_slice(f)` | lends the whole block to `f` as one `[]T` |
| `l.reverse()` | reverses the order in place |
| `l.filter(test)` | a new list of copies of the matches |
| `l.map(f)` | a new list of what `f` gives for each element |
| `l.sort()` | sorts in place, smallest first. `T` needs `<` and `==` |
| `l.sort_by(key)`, `l.sort_with(less)` | sort by a key and by a comparison |
| `l.reserve(n)` | room for `n` more elements in one allocation |
| `l.shrink()` | gives back the room past the last element |

A deque has `push_front`, `push_back`, `pop_front`, `pop_back`, `front`, `back`, `reserve` and `shrink`. The two `pop` forms and `front` and `back` give a `?T`. A deque has no `[]`.

```anti
//! name: lists
import anti.io;
import anti.collection.list.{List};
import anti.collection.deque.{Deque};

fn show(label: str, l: *List<int>)
{
	let t = l.to_text();
	let line = f"{label}: {t}, count {l.count}";
	io.println(line);
	free(line.ptr);
	free(t.ptr);
}

fn main() -> int
{
	let l = List<int>.new();
	for i in 0..5 {
		l.push(i * i);
	}
	show("pushed", &l);
	l.insert(1, 100);
	let gone = l.remove_at(3);
	l[0] = 7;
	show("changed", &l);
	let last = l.pop() ?? -1;
	let far = l.get(99) ?? -1;
	let line = f"removed {gone}, popped {last}, get(99) {far}, l[1] {l[1]}";
	io.println(line);
	free(line.ptr);

	l.sort();
	show("sorted", &l);
	let even = l.filter(fn(x) {
		return x % 2 == 0;
	});
	show("even", &even);
	let copy = dup(l);
	copy.push(1);
	line = f"copy {copy.count}, original {l.count}, equal {copy == l}";
	io.println(line);
	free(line.ptr);

	let d = Deque<int>.new();
	d.push_back(2);
	d.push_back(3);
	d.push_front(1);
	let first = d.pop_front() ?? -1;
	let back = d.back() ?? -1;
	line = f"deque gave {first}, back {back}, count {d.count}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: List {size_of(List<int>)}, Deque {size_of(Deque<int>)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
pushed: [0, 1, 4, 9, 16], count 5
changed: [7, 100, 1, 9, 16], count 5
removed 4, popped 16, get(99) -1, l[1] 100
sorted: [1, 7, 9, 100], count 4
even: [100], count 1
copy 5, original 4, equal false
deque gave 1, back 3, count 2
sizes: List 136, Deque 136
```

Reading gives a copy. `l[i]`, `get`, `first` and `last` copy the element as `dup` does, so a change to the result does not reach the list. A change in place goes through `modify`, an update form, or `for x in &l`.

The checker refuses the following.

```anti refused
//! error: `List<int>` has a `destruct`, use `dup` instead of `=`
import anti.collection.list.{List};

fn twice()
{
	let a = List<int>.new();
	let b = a;
}
```

A collection owns its block, so `=` between two collections would leave two owners of one block. `dup(a)` makes the copy.

```anti refused
//! error: unary `&` needs a place
import anti.collection.list.{List};

fn inside(l: *List<int>) -> *int
{
	return &l[0];
}
```

`l[0]` is a call that gives a copy, and a copy has no address. No operation of a list gives a pointer that outlives a call.

```anti refused
//! error: `items` is protected in `Indexed<int>`
import anti.collection.list.{List};

fn block(l: *List<int>) -> ?*int
{
	return l.items;
}
```

```anti refused
//! error: `P` has no `lt`, which `sort` needs for `T`. A struct or a class has no default order and declares `operator fn lt`
import anti.collection.list.{List};

struct P
{
	x: int,
}

fn order(l: *List<P>)
{
	l.sort();
}
```

```anti refused
//! error: `x` is a copy of each element of `l`. Walk with `&l` to change the elements
import anti.collection.list.{List};

fn zero(l: *List<int>)
{
	for x in l {
		x = 0;
	}
}
```

```anti refused
//! error: `v` is lent for the call and cannot be stored
import anti.collection.list.{List};

fn keep(l: *List<int>) -> ?*int
{
	let p: ?*int = none;
	l.read(0, fn(v) {
		p = v;
	});
	return p;
}
```

An index outside the list stops a dev build with `index out of bounds: index 5, length 2`. A release build checks nothing there, as for an array. `get`, `first` and `last` test the index in every build and give `none`.

### How antic builds it

`List<T>` inherits `Indexed<T>`, which inherits `Collection<T>`. A `List<int>` is one object of 136 bytes.

| Offset | Field | Class |
|---:|---|---|
| 0 | the pointer to the table of the class | `Object` |
| 8 | `count` | `Collection<T>` |
| 16 | `from`, the allocator | `Collection<T>` |
| 24 | `changes`, a count of 8 bytes and the `SourceLocation` of the last change of the size, 64 bytes | `Collection<T>` |
| 96 | `items`, the block | `Indexed<T>` |
| 104 | `capacity` | `Indexed<T>` |
| 112 | `head`, the slot of the first element | `Indexed<T>` |
| 120 | `spare` | `Indexed<T>` |
| 128 | `ring`, one byte and 7 of padding | `Indexed<T>` |

A list and a deque have the same fields. A list keeps `head` at 0 and `ring` false, so the element at position `i` stands in slot `i`. A deque sets `ring`, and position `i` stands in slot `head + i`, less `capacity` when the sum reaches it. That is one compare and no division.

The block grows when `count + 1` passes `capacity`. The new capacity is the old one doubled, starting from 8. `resize` allocates the new block, zeroes it, copies the elements with `memcpy` and frees the old block. A deque copies its ring in order in at most two runs and sets `head` to 0.

The program below counts the allocations of a list through an allocator of its own.

```anti
//! name: growth
import anti.io;
import anti.mem;
import anti.collection.list.{List};
import anti.collection.deque.{Deque};

class Counting inherits mem.Allocator
{
	pub taken: int = 0,
	pub last: int = 0,

	concrete fn alloc(self, size: int, align: int) -> ?*byte
	{
		self.taken += 1;
		self.last = size;
		return mem.LibcAllocator.get().alloc(size, align);
	}

	concrete fn free(self, p: ?*byte)
	{
		mem.LibcAllocator.get().free(p);
	}
}

fn main() -> int
{
	let c = Counting { };
	let l = List<int>.new(0, &c);
	let seen = 0;
	for i in 0..100 {
		l.push(i);
		if c.taken != seen {
			seen = c.taken;
			let line = f"push {i + 1}: allocation {c.taken} of {c.last} bytes";
			io.println(line);
			free(line.ptr);
		}
	}
	l.shrink();
	let line = f"shrink: allocation {c.taken} of {c.last} bytes";
	io.println(line);
	free(line.ptr);

	seen = c.taken;
	let r = List<int>.new(100, &c);
	for i in 0..100 {
		r.push(i);
	}
	line = f"new(100): {c.taken - seen} allocation for 100 pushes";
	io.println(line);
	free(line.ptr);

	let d = Deque<i32>.new(0, &c);
	seen = c.taken;
	for i in 0..20 {
		d.push_front(i as i32);
	}
	line = f"deque of 20: {c.taken - seen} allocations, the last of {c.last} bytes";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
push 1: allocation 1 of 64 bytes
push 9: allocation 2 of 128 bytes
push 17: allocation 3 of 256 bytes
push 33: allocation 4 of 512 bytes
push 65: allocation 5 of 1024 bytes
shrink: allocation 6 of 800 bytes
new(100): 1 allocation for 100 pushes
deque of 20: 3 allocations, the last of 128 bytes
```

A hundred pushes into an empty list allocate five times. `new(100)` and `reserve(100)` allocate once.

`push` of the copy for `int` in a release build:

```llvm excerpt=lists target=linux-x86_64 mode=release
define internal void @"anti.collection.list.List$3cint$3e.push"(ptr
b0:
  %v1.i = getelementptr inbounds nuw i8, ptr %p0, i64 8
  %v3.i = load i64, ptr %v1.i, align 8
  %v6.i = add i64 %v3.i, 1
  %v8.i = getelementptr inbounds nuw i8, ptr %p0, i64 104
  %v10.i = load i64, ptr %v8.i, align 8
  %v13.i = icmp sgt i64 %v6.i, %v10.i
...
  tail call fastcc void @"anti.collection.Indexed$3cint$3e.resize"(
...
  store i64 %p1, ptr %v25, align 8
...
  %v10.i5 = getelementptr inbounds nuw i8, ptr %p0, i64 24
...
  call void @llvm.memcpy.p0.p0.i64(ptr noundef nonnull align 1 dereferenceable(64) %v20.i, ptr noundef nonnull align 8 dereferenceable(64) %p2, i64 64, i1 false)
```

- The symbol is the copy of the class for `int`. `$3c` and `$3e` are the escaped brackets, as chapter 10 describes.
- The two loads read `count` at offset 8 and `capacity` at offset 104, and the compare decides whether the block grows.
- The call of `resize` is the growth. opt inlined `make_room` and `grown` around it.
- The `store` writes the element into its slot.
- The address at offset 24 is the count of `changes`, which the function raises by one. A walk compares that count, as "Walking a collection" describes.
- The `memcpy` of 64 bytes writes the place of the call into `changes`. The third parameter `%p2` is the hidden argument `at: SourceLocation = here` of `push`, passed by address.

Between the growth and the store, the function tests `items` against null and ends the program through `Error.fatal` when it is null. The test stands in both modes, since the source writes `self.items catch fatal`.

Where it lives: `src/std/anti/collection/list.anti` and `src/std/anti/collection/deque.anti`. `Indexed<T>` with `slot`, `make_room`, `resize` and `take_place`, and `Collection<T>` with `grown`, `room` and `close_gap`, stand in `src/std/anti/collection.anti`.

### What it costs

The counts are the same in dev and in release, apart from the checks of the last rows.

| Cost | Count |
|---|---|
| `size_of(List<int>)`, `size_of(Deque<int>)` | 136 each |
| Bytes per element | `size_of(T)`, in one block of `capacity` elements |
| Heap allocations | none for an empty collection. One per growth, and the old block is freed |
| Growth | from 8 elements by doubling. The block holds between `count` and twice `count` elements after it grew |
| `push` with room | one compare, one null test, one store, two adds and a copy of 64 bytes for the place of the call |
| `pop`, and `pop_front` and `push_front` of a deque | no element moves |
| `insert(i, x)` and `remove_at(i)` of a list | one `memmove` of the `count - i` elements behind |
| `l[i]`, `get`, `first`, `last` | one copy of the element as `dup` makes it |
| `sort`, `sort_by`, `sort_with` | no allocation. The sort is stable and merges in place |
| `find`, `update` and `remove` forms | one walk, and one call of `test` through a function value per element |
| `remove_all` of a list | one `memmove` per element removed |
| `clear` of a list of `n` elements | `n` removals from the front, which move `n (n - 1) / 2` elements in all |
| Calls into the runtime | `anti_rt_mem_alloc` and `anti_rt_mem_free` on a growth, through `alloc` and `free` of the default allocator. `to_text` and `serialize` call `anti_rt_element_text` and `anti_rt_element_serialize` per element |
| Atomic operations and locks | none |
| Index check of `l[i]` and `remove_at(i)` | dev mode. `--no-checks` drops it and `--checks` keeps it in a release build |
| Overflow checks of the arithmetic inside | dev mode, under the same two options |
| Copies of code | one copy of `List<T>`, `Indexed<T>` and `Collection<T>` per element type: 44 functions for `List<int>` in the module of `growth` |

The checks follow the build that compiles the program, since a copy of a generic is compiled with the module that uses it.

A list of 200,000 elements that is cleared with `clear` moves about 20 billion elements. A loop of `pop` moves none.

### Pitfalls

For a Java programmer:

- A `List<Point>` holds the points themselves and no references. `l[i]` gives a copy, and a change to the copy is lost.
- `l.count` is a field and no call. The checker does not refuse a write to it, and a write breaks the list.
- A `for` over a list that the body changes stops a dev build with a message. A release build goes on over the new state.

For a C programmer:

- No pointer into the block leaves the list. `lend_slice` lends the block for one call, for C and simd code.
- A release build does not check `l[i]`. `get(i)` checks in every build.
- `free(t.ptr)` is the program's job for the text of `to_text`. The list frees its own block.

For a C++ programmer:

- `auto b = a;` has no counterpart. `let b = a;` is refused, and `dup(a)` is the copy.
- `l[i]` is no reference. `l[i].x = 1` changes a copy, and `l.modify(i, f)` changes the element.
- `clear` of a long list is quadratic. `shrink` after `clear`, or a new list, is the release of the room.
- A deque is one ring and no list of blocks. Growth moves every element, and no element address is stable.

## Ring and Grid

### What you know

A ring buffer in C is an array, a head and a count, with a modulo per access. Java has `ArrayBlockingQueue` for a bounded queue and arrays of arrays for a grid. C++ has `std::array` and, since C++23, `std::mdspan` over one block.

### The feature

`Ring<T, N>` of `anti.collection.ring` is a queue of at most `N` elements. `N` is a constant of the type. The ring takes its room when it is made and never allocates again.

| Operation | Result |
|---|---|
| `Ring<T, N>.new(from)` | an empty ring with room for `N` elements |
| `r.push(x)` | adds `x` after the newest element and gives `true`. A full ring gives `false` and tears `x` down |
| `r.pop()` | takes the oldest element off, a `?T` |
| `r.peek()` | a copy of the oldest element, a `?T` |
| `r.is_full()` | whether the ring holds `N` elements |

`Grid<T>` of `anti.collection.grid` is a rectangle of `width` columns and `height` rows in one block, row by row. It never changes its size.

| Operation | Result |
|---|---|
| `Grid<T>.new(width, height, fill, from)` | a grid whose every cell is a copy of `fill` |
| `g[x, y]` | a copy of the element in column `x` of row `y` |
| `g[x, y] = v` | replaces that element |
| `g.get(x, y)` | a `?T`, which is `none` outside the grid |
| `g.lend_slice(f)` | lends the block to `f` as one `[]T` of `width * height` elements |
| `g.width`, `g.height` | the two sizes, public fields |

A remove form and `clear` of a grid put a copy of `fill` into each cell they empty, and `count` stays `width * height`.

```anti
//! name: rings
import anti.io;
import anti.mem;
import anti.collection.ring.{Ring};
import anti.collection.grid.{Grid};

class Counting inherits mem.Allocator
{
	pub taken: int = 0,
	pub last: int = 0,

	concrete fn alloc(self, size: int, align: int) -> ?*byte
	{
		self.taken += 1;
		self.last = size;
		return mem.LibcAllocator.get().alloc(size, align);
	}

	concrete fn free(self, p: ?*byte)
	{
		mem.LibcAllocator.get().free(p);
	}
}

fn main() -> int
{
	let c = Counting { };
	let r = Ring<int, 4>.new(&c);
	let took = 0;
	for i in 0..6 {
		if r.push(i * 10) {
			took += 1;
		}
	}
	let oldest = r.pop() ?? -1;
	let next = r.peek() ?? -1;
	let again = r.push(99);
	let t = r.to_text();
	let line = f"ring took {took} of 6, gave {oldest}, peek {next}, push {again}, now {t}, full {r.is_full()}";
	io.println(line);
	free(line.ptr);
	free(t.ptr);
	line = f"ring: {c.taken} allocation of {c.last} bytes";
	io.println(line);
	free(line.ptr);

	let g = Grid<i32>.new(3, 2, 0, &c);
	g[2, 1] = 7;
	g[0, 0] = 5;
	let sum = 0;
	for x in g {
		sum += x as int;
	}
	let outside = g.get(3, 0) ?? -1;
	t = g.to_text();
	line = f"grid {g.width} by {g.height}: {t}, sum {sum}, g[2, 1] {g[2, 1]}, get(3, 0) {outside}";
	io.println(line);
	free(line.ptr);
	free(t.ptr);
	let emptied = g.remove_all(fn(x) {
		return x == 7;
	});
	line = f"emptied {emptied}, count {g.count}, g[2, 1] {g[2, 1]}";
	io.println(line);
	free(line.ptr);
	line = f"grid: allocation {c.taken} of {c.last} bytes";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Ring {size_of(Ring<int, 4>)}, Grid {size_of(Grid<i32>)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
ring took 4 of 6, gave 0, peek 10, push true, now [10, 20, 30, 99], full true
ring: 1 allocation of 32 bytes
grid 3 by 2: [5, 0, 0, 0, 0, 7], sum 12, g[2, 1] 7, get(3, 0) -1
emptied 1, count 6, g[2, 1] 0
grid: allocation 2 of 28 bytes
sizes: Ring 136, Grid 152
```

The checker refuses the following.

```anti refused
//! error: `Ring` takes 2 type arguments, found 1
import anti.collection.ring.{Ring};

fn make()
{
	let r = Ring<int>.new();
}
```

```anti refused
//! error: `n` must be a constant for `N`
import anti.collection.ring.{Ring};

fn make(n: int)
{
	let r = Ring<int, n>.new();
}
```

```anti refused
//! error: `Grid.index` takes 2 arguments, found 1
import anti.collection.grid.{Grid};

fn cell(g: *Grid<int>) -> int
{
	return g[1];
}
```

```anti refused
//! error: `Grid.new` takes at least 3 arguments, found 2
import anti.collection.grid.{Grid};

fn make()
{
	let g = Grid<int>.new(2, 2);
}
```

A dev build stops at `g[x, y]` outside the grid, with `x` checked against the width and `y` against the height. A release build checks neither.

### How antic builds it

Both inherit `Indexed<T>` and have the fields of a list. A ring sets `ring` and a capacity of `N`. A grid adds `width` and `height`, 16 bytes, and sets `spare` to 1. Its block holds one slot past the cells, which keeps the copy of `fill` that an emptied cell takes. The grid of 3 by 2 cells of `i32` in the example therefore takes 7 slots, 28 bytes.

`push` of a `Ring<int, 4>` in a release build:

```llvm excerpt=rings target=linux-x86_64 mode=release
@"anti.collection.ring.Ring$3cint$2c$204$3e.push"(ptr
b0:
  %v1 = getelementptr inbounds nuw i8, ptr %p0, i64 8
  %v3 = load i64, ptr %v1, align 8
  %v5 = icmp eq i64 %v3, 4
...
  %v6.i = add i64 %p0.val1, %v3
  %v13.not.i = icmp slt i64 %v6.i, %p0.val
  %v23.i = select i1 %v13.not.i, i64 0, i64 %p0.val
  %spec.select.i = sub i64 %v6.i, %v23.i
  %v33 = shl i64 %spec.select.i, 3
  %v36 = getelementptr i8, ptr %v12, i64 %v33
  store i64 %p1, ptr %v36, align 8
```

- `$2c$20` in the symbol is the comma and the blank of `Ring<int, 4>`. Each pair of type arguments has a copy of its own.
- The compare of `count` with 4 is the test for a full ring. `N` is a constant in the code of the copy.
- The `add` is `head + count`, the position after the newest element.
- The compare, the `select` and the `sub` take `capacity` off when the sum reaches it. The slot costs no division.
- The shift by 3 is the size of an `int`, and the `store` writes the element.

The function contains no call of an allocator. `pop` clears the slot of the oldest element with zero bytes and moves `head` on by one slot.

`g[x, y]` is the call `g.index(x, y)`, which reads the cell at `y * width + x`.

Where it lives: `src/std/anti/collection/ring.anti` and `src/std/anti/collection/grid.anti`. `cell` of `Grid<T>` holds the index and its checks, and `take_place` of `Grid<T>` the refill of an emptied cell.

### What it costs

| Cost | Count |
|---|---|
| `size_of(Ring<int, 4>)` | 136 |
| `size_of(Grid<i32>)` | 152 |
| Heap allocations of a ring | one of `N * size_of(T)` bytes when it is made, and none after it |
| Heap allocations of a grid | one of `(width * height + 1) * size_of(T)` bytes when it is made, and none after it |
| `push` and `pop` of a ring | no allocation, no call and no division. `push` copies 64 bytes for the place of the call |
| `g[x, y]` and `g[x, y] = v` | one multiply, one add and one copy of the element |
| The index checks of `g[x, y]` | dev mode, two compares. `--no-checks` drops them and `--checks` keeps them in a release build |
| `find_all` and `to_text` of a ring | allocate, since each makes a new collection or a new text |
| Atomic operations and locks | none |
| Copies of code | one copy per pair of `T` and `N` of a ring, and one per `T` of a grid |

### Pitfalls

For a Java programmer:

- A full ring refuses the element and gives `false`. It does not block, and it does not overwrite the oldest element.
- `Ring<int, 4>` and `Ring<int, 8>` are two types. A function that takes one refuses the other.

For a C programmer:

- The index of a grid is `g[x, y]`, column first. The block is stored row by row, so the cell is at `y * width + x`.
- A ring is for one thread. `SpscRing<T, N>` of chapter 14 is the ring between two threads.

For a C++ programmer:

- `clear` of a grid does not empty it. Every cell takes the fill value, and `count` stays.
- A grid has no `resize`. A new grid and a copy of the cells is the way to another size.
- `N` of a ring is part of the type, and a large `N` costs no stack. The elements stand on the heap.

## Map and HashMap

### What you know

`HashMap` of Java walks in an order that depends on the hashes, and `LinkedHashMap` keeps insertion order with a linked list beside the table. `std::unordered_map` of C++ allocates one node per entry. C has no map, and a program writes its table by hand or takes a library.

### The feature

`anti.collection.map` holds two maps. `Map<K, V>` walks its entries in the order their keys were first added, on every run and every machine. `HashMap<K, V>` keeps no order and walks in a new random order on every walk. Both find a key by its hash. The key type needs `==` and `hash`, which every type of chapter 3 and every struct of them has by default.

| Operation | Result |
|---|---|
| `Map<K, V>.new(capacity, from)` | an empty map. Both arguments are optional |
| `m.get(k)` | a `?V`, a copy of the value under `k` |
| `m.contains(k)` | whether the map holds `k` |
| `m.set(k, v)` | writes `v` under `k` and replaces the value there |
| `m.add(k, v)` | writes `v` when the map does not hold `k`. Gives `false` and changes nothing when it does |
| `m.remove(k)` | removes the entry and gives its value as a `?V` |
| `m.update(k, change)` | runs `change` on the value in place, and gives `false` without the key |
| `m.read(k, f)`, `m.modify(k, f)` | lend the value under `k` to `f` |
| `m.keys()`, `m.values()` | an iterator over copies of the keys, and one that lends the values |
| `m.capacity()` | the number of entries the room holds before it grows |
| `m.reserve(n)`, `m.shrink()` | room for `n` more entries in one allocation, and the room without the unused part |
| `for (k, v) in m` | a walk of the entries |

Both maps have the same operations and those of `Collection<T>`, whose element is the tuple `(K, V)`. `set` and `add` copy the key and take the value itself. A map has no `[]`.

A key never changes in place. The forms that lend a whole entry put back a key that the function replaced.

A key that is removed from a `Map` and added again goes to the end of the order. A replaced value keeps its place.

```anti
//! name: maps
import anti.io;
import anti.mem;
import anti.collection.map.{Map, HashMap};

class Counting inherits mem.Allocator
{
	pub taken: int = 0,
	pub last: int = 0,

	concrete fn alloc(self, size: int, align: int) -> ?*byte
	{
		self.taken += 1;
		self.last = size;
		return mem.LibcAllocator.get().alloc(size, align);
	}

	concrete fn free(self, p: ?*byte)
	{
		mem.LibcAllocator.get().free(p);
	}
}

fn main() -> int
{
	let ages = Map<str, int>.new();
	ages.set("Ann", 41);
	ages.set("Bob", 29);
	ages.set("Cy", 35);
	ages.set("Ann", 42);
	let added = ages.add("Bob", 1);
	let bob = ages.remove("Bob") ?? -1;
	ages.set("Bob", 30);
	ages.update("Cy", fn(v) {
		*v += 1;
	});
	let t = ages.to_text();
	let dee = ages.get("Dee") ?? -1;
	let cy = ages.contains("Cy");
	let line = f"{t}, add gave {added}, removed {bob}, Dee {dee}, has Cy {cy}";
	io.println(line);
	free(line.ptr);
	free(t.ptr);
	for (name, age) in ages {
		line = f"{name} is {age}";
		io.println(line);
		free(line.ptr);
	}

	let c = Counting { };
	let squares = HashMap<int, int>.new(0, &c);
	let seen = 0;
	for i in 0..100 {
		squares.set(i, i * i);
		if c.taken != seen {
			seen = c.taken;
			line = f"HashMap set {i + 1}: {c.last} bytes, capacity {squares.capacity()}";
			io.println(line);
			free(line.ptr);
		}
	}
	let sum = 0;
	for k in squares.keys() {
		sum += k;
	}
	let nine = squares.get(9) ?? -1;
	line = f"keys add up to {sum}, 9 squared is {nine}, count {squares.count}";
	io.println(line);
	free(line.ptr);

	let ordered = Map<int, int>.new(0, &c);
	seen = c.taken;
	for i in 0..100 {
		ordered.set(i, i);
		if c.taken != seen {
			seen = c.taken;
			line = f"Map set {i + 1}: {c.last} bytes, capacity {ordered.capacity()}";
			io.println(line);
			free(line.ptr);
		}
	}
	line = f"sizes: Map {size_of(Map<int, int>)}, HashMap {size_of(HashMap<int, int>)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
{"Ann": 42, "Cy": 36, "Bob": 30}, add gave false, removed 29, Dee -1, has Cy true
Ann is 42
Cy is 36
Bob is 30
HashMap set 1: 384 bytes, capacity 8
HashMap set 9: 768 bytes, capacity 16
HashMap set 17: 1536 bytes, capacity 32
HashMap set 33: 3072 bytes, capacity 64
HashMap set 65: 6144 bytes, capacity 128
keys add up to 4950, 9 squared is 81, count 100
Map set 1: 448 bytes, capacity 8
Map set 9: 896 bytes, capacity 16
Map set 17: 1792 bytes, capacity 32
Map set 33: 3584 bytes, capacity 64
Map set 65: 7168 bytes, capacity 128
sizes: Map 160, HashMap 160
```

`for (k, v) in m` gives a copy of the key and a copy of the value. `for (k, v) in &m` gives a copy of the key and lends the value, so `*v += 1` changes the map.

The checker refuses the following.

```anti refused
//! error: `C` has no `eq`, which `Map` needs for `K`
import anti.collection.map.{Map};

concurrent class C
{
	atomic n: int = 0,
}

fn make()
{
	let m = Map<C, int>.new();
}
```

A concurrent class has no default `==` and no default hash, so it is no key.

```anti refused
//! error: `Map` takes 2 type arguments, found 1
import anti.collection.map.{Map};

fn make()
{
	let m = Map<int>.new();
}
```

```anti refused
//! error: `v` is a copy of each element of `m`. Walk with `&m` to change the elements
import anti.collection.map.{Map};

fn zero(m: *Map<str, int>)
{
	for (k, v) in m {
		v = 0;
	}
}
```

```anti refused
//! error: `k` is a copy that the iterator of `m` gives, so a change reaches no element
import anti.collection.map.{Map};

fn rename(m: *Map<str, int>)
{
	for (k, v) in &m {
		k = "x";
	}
}
```

```anti refused
//! error: expected `List<int>`, found `Map<int, int>`
import anti.collection.list.{List};
import anti.collection.map.{Map};

fn same(l: *List<int>, m: *Map<int, int>) -> bool
{
	return *l == *m;
}
```

### How antic builds it

The two maps have two layouts. Each is one block from the allocator, so `new(capacity)` and `reserve(n)` allocate once.

A `Map<K, V>` inherits `EntryTable<(K, V)>`. Its block holds three arrays one after the other.

| Part | Contents | Bytes |
|---|---|---|
| Entries | the tuples `(K, V)` in the order they were added | `capacity * size_of((K, V))` |
| Hashes | the marked hash of each entry, or 0 for a removed one | `capacity * 8` |
| Buckets | two words per bucket: the marked hash and the position of the entry | `buckets * 16` |

The number of buckets is a power of two, at least 16 and at least twice the capacity. A `Map<int, int>` of capacity 8 is 128 + 64 + 256 bytes, the 448 of the output. A lookup probes the buckets from `hash & (buckets - 1)` in steps of one and compares the stored hash before it compares the key. A walk reads the entries in order and passes over the removed ones.

A removed entry keeps its position with a hash of 0, and its bucket is marked removed. No other entry moves, so the order stays. The room is rebuilt when it is full. It is rebuilt in the same size when a quarter of it holds removed entries, and in twice the size otherwise.

A `HashMap<K, V>` inherits `SlotTable<(K, V)>`. Its block holds a mark of 8 bytes per slot and then the slots, which hold the entries themselves.

| Part | Contents | Bytes |
|---|---|---|
| Marks | 0 for an empty slot, 1 for a removed one, the marked hash otherwise | `slots * 8` |
| Slots | the tuples `(K, V)` | `slots * size_of((K, V))` |

The number of slots is a power of two and starts at 16. The table grows before the live and the removed entries pass half the slots, so `capacity()` is half the slots. It grows to twice the slots when more than a quarter are live, and to the same number without the removed entries otherwise. A `HashMap<int, int>` of 16 slots is 16 times 24 bytes, the 384 of the output.

A marked hash is the hash of the key mixed with the seed of the map, with the top bit set. The top bit keeps it apart from the marks 0 and 1. The map reads the seed once, when it is made.

The object of each map is 160 bytes: the 96 bytes of `Collection<T>` and eight fields of 8 bytes. `Map` holds the entries, the hashes, the count of entries written, the seed, the block, the buckets, the capacity and the count of buckets. `HashMap` holds the marks, the slots, the count of slots, the seed and the block. Its last three fields are the count of removed slots, the count of walks and the random number of the walks.

`get` of a `HashMap<int, int>` calls `find`, which opt kept as one function of the copy:

```llvm excerpt=maps target=linux-x86_64 mode=release
@"anti.collection.map.HashMap$3cint$2c$20int$3e.find"(ptr
b0:
  %v4 = lshr i64 %p1, 33
  %v7 = xor i64 %v4, %p1
  %v9 = mul i64 %v7, -49064778989728563
...
  %v8.i = or i64 %v46.i.i, -9223372036854775808
...
  %v27.i = add i64 %v25.i, -1
  %t15.01.i = and i64 %v27.i, %v8.i
...
  %v54.i = icmp eq i64 %v387.i, %v8.i
...
  %v60.i = add i64 %t15.06.i, 1
  %t15.0.i = and i64 %v60.i, %v27.i
...
  %v40.not.i = icmp eq i64 %v38.i, 0
```

- The shift, the `xor` and the multiply open the hash of the `int` key, written inline. "Hashing and its seed" below holds the whole function.
- The `or` sets the top bit, which makes the marked hash.
- `add -1` turns the count of slots into the mask, and the `and` is the first slot of the probe.
- The compare with the marked hash is the test of a slot. Only a slot with an equal mark reaches the compare of the keys.
- The `add` of 1 and the `and` step to the next slot, and the compare with 0 ends the probe at an empty slot.

The function mixes three times: the key, the seed, and the two together. That is six multiplies for a lookup of an `int` key, and no call.

Where it lives: `src/std/anti/collection/map.anti`. `EntryTable<E>` holds `position`, `append`, `place`, `forget`, `make_room` and `rebuild`. `SlotTable<E>` holds `slot_of`, `put`, `free_slot`, `resize` and `shuffle`. `seeded` of `src/std/anti/lang.anti` mixes a hash with a seed.

### What it costs

The counts are the same in dev and in release, apart from the checks of dev mode in the arithmetic.

| Cost | Count |
|---|---|
| `size_of(Map<int, int>)`, `size_of(HashMap<int, int>)` | 160 each |
| Bytes per entry of capacity, `Map<K, V>` | `size_of((K, V))` and 8, and at least 32 for its two buckets. 56 for `Map<int, int>` |
| Bytes per entry of capacity, `HashMap<K, V>` | two slots: twice `size_of((K, V))` and 16. 48 for `HashMap<int, int>` |
| Heap allocations | none for an empty map. One block per growth, and the old one is freed. No allocation per entry |
| Growth | by doubling, from a capacity of 8. Every entry moves into the new block, a `Map` in its order |
| `get`, `contains`, `set`, `add`, `remove`, `update` | one hash of the key, one mix with the seed, one probe |
| The probe | one compare of 8 bytes per slot passed, and one compare of the keys per slot with an equal mark |
| `set` and `add` of a new key | a copy of the key as `dup` makes it, and a copy of 64 bytes for the place of the call |
| `remove` | marks the slot. No entry moves |
| A walk of a `HashMap` | every slot, live or not. The first walk draws 8 bytes with `anti_rt_entropy` |
| A walk of a `Map` | every entry written, removed ones included |
| Calls into the runtime | `anti_rt_hash_seed` when a map is made, and `anti_rt_hash_bytes` per hash of a `str` key |
| Atomic operations | one load, inside `anti_rt_hash_seed`, when a map is made |
| Locks | none |
| Copies of code | one copy of the map, its table and `Collection<T>` per pair of `K` and `V` |

`tests/bench/map_work.anti` fills a `HashMap<int, int>` with 100,000 keys, answers 500,000 lookups and removes 50,000 keys, 20 times over. `tests/bench/map_work.c` is the same program in C with a table laid out and grown the same way.

| Program | C twin | Anti |
|---|---:|---:|
| `map_work` | 127.9 ms | 137.8 ms |

| Condition | Value |
|---|---|
| Machine | Apple M4 Pro, 14 cores, 64 GB, macOS 26.6 |
| Target | macos-arm64 |
| Commit | `1fb19e61` |
| Mode | release, linked with full LTO. The C twin is built by the pinned clang at `-O2` |
| Command | `cmake --build build/host --target bench`, which runs `tests/run_bench.cmake` |
| Runs | the median of 5 runs of each program |
| Load | another job ran on the machine during the run |

### Pitfalls

For a Java programmer:

- `m.get(k)` gives a `?V` and never a null reference. `m.get(k) ?? 0` names the value for a missing key.
- `set` is `put`, and `add` is `putIfAbsent`. `add` gives a `bool` and not the old value.
- `Map` is the ordered one, as `LinkedHashMap`. `HashMap` changes its order from one walk to the next, also in one run.

For a C programmer:

- A `str` key is copied as a view of two words. The map does not copy the bytes, so the text must outlive the entry.
- The value `get` gives is a copy. `update` and `modify` change the value in place.

For a C++ programmer:

- No `operator[]` exists, and no lookup inserts a default value.
- No pointer and no reference to an entry is stable. A growth moves every entry, and `read` and `modify` lend an entry for one call.
- `==` of two maps compares the entries whatever their order, also for `Map`.
- `m.count` is a field, and `m.capacity()` is a call.

## Set, HashSet and BitSet

### What you know

`HashSet` and `LinkedHashSet` of Java are maps without values. `std::unordered_set` of C++ allocates a node per element, and `std::bitset` and `std::vector<bool>` hold one bit per number. C has bit masks in an integer and tables written by hand.

### The feature

`anti.collection.set` holds three sets. `Set<T>` keeps insertion order, and `HashSet<T>` keeps no order and walks in a new random order each time. They stand on the two tables of the maps. `BitSet` holds numbers from 0 up, one bit per number, and walks them in increasing order.

| Operation | Result |
|---|---|
| `Set<T>.new(capacity, from)` | an empty set. Both arguments are optional |
| `s.add(x)` | adds `x` and gives `true`. Gives `false` when the set holds it already |
| `s.remove(x)` | removes `x`, and gives `false` when the set does not hold it |
| `s.contains(x)` | whether the set holds `x` |
| `a.union(b)`, `a.intersect(b)`, `a.minus(b)` | a new set of the elements in either, in both, and in `a` alone |
| `a.is_subset(b)` | whether `b` holds every element of `a` |
| `s.capacity()`, `s.reserve(n)`, `s.shrink()` | as for a map |

The three have the same operations and those of `Collection<T>`. The element type of a `Set` and a `HashSet` needs `==` and `hash`. The element of a `BitSet` is an `int`. `capacity` of a `BitSet` is the number of bits, and `reserve(n)` makes room for the numbers below `n`.

An element of a set never changes in place. The update forms and the forms that lend an element lend a copy, and the set drops the change. A walk gives copies.

```anti
//! name: sets
import anti.io;
import anti.collection.set.{Set, HashSet, BitSet};

fn main() -> int
{
	let a = Set<str>.new();
	a.add("red");
	a.add("green");
	a.add("blue");
	let twice = a.add("red");
	let b = Set<str>.new();
	b.add("blue");
	b.add("black");
	let both = a.intersect(b);
	let all = a.union(b);
	let rest = a.minus(b);
	let t1 = both.to_text();
	let t2 = all.to_text();
	let t3 = rest.to_text();
	let line = f"add again {twice}, both {t1}, all {t2}, rest {t3}, subset {both.is_subset(a)}";
	io.println(line);
	free(line.ptr);
	free(t1.ptr);
	free(t2.ptr);
	free(t3.ptr);

	let h = HashSet<int>.new();
	for i in 0..10 {
		h.add(i % 4);
	}
	let gone = h.remove(3);
	let sum = 0;
	for x in h {
		sum += x;
	}
	line = f"hash set of {h.count}, sum {sum}, removed {gone}, has 3 {h.contains(3)}";
	io.println(line);
	free(line.ptr);

	let bits = BitSet.new();
	bits.add(3);
	bits.add(200);
	bits.add(64);
	let below = bits.add(-1);
	let t = bits.to_text();
	line = f"bits {t}, count {bits.count}, capacity {bits.capacity()}, add(-1) {below}";
	io.println(line);
	free(line.ptr);
	free(t.ptr);
	line = f"sizes: Set {size_of(Set<int>)}, HashSet {size_of(HashSet<int>)}, BitSet {size_of(BitSet)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
add again false, both ["blue"], all ["red", "green", "blue", "black"], rest ["red", "green"], subset true
hash set of 3, sum 3, removed true, has 3 false
bits [3, 64, 200], count 3, capacity 256, add(-1) false
sizes: Set 160, HashSet 160, BitSet 112
```

The checker refuses the following.

```anti refused
//! error: `for x in &e` takes each element in place, and the `operator fn value` of `SetWalk<int>` gives a copy of `int`
import anti.collection.set.{Set};

fn zero(s: *Set<int>)
{
	for x in &s {
		*x = 0;
	}
}
```

```anti refused
//! error: expected `int`, found `str`
import anti.collection.set.{BitSet};

fn mark(b: *BitSet)
{
	b.add("x");
}
```

`add` of a `BitSet` gives `false` for a number below 0 and adds nothing.

### How antic builds it

A `Set<T>` inherits `EntryTable<T>` and a `HashSet<T>` inherits `SlotTable<T>`. They have the blocks, the growth and the probe of the two maps, with the element in the place of the tuple.

A `BitSet` inherits `Collection<int>` and adds two fields, the block of words and the count of words, 112 bytes in all. The number `n` is bit `n % 64` of word `n / 64`. `add` of a number past the room doubles the words until the number fits, starting from one word. The numbers 3, 64 and 200 of the example need 4 words, the capacity of 256.

`contains` of a `BitSet` in a release build:

```llvm excerpt=sets target=linux-x86_64 mode=release
define internal noundef zeroext range(i8 0, 2) i8 @anti.collection.set.BitSet.contains(ptr
b0:
  %v1 = icmp slt i64 %p1, 0
...
  %v214 = lshr i64 %p1, 6
  %v26 = getelementptr inbounds nuw i8, ptr %p0, i64 104
  %v28 = load i64, ptr %v26, align 8
  %v31.not = icmp slt i64 %v214, %v28
...
  %v55 = load i64, ptr %v53, align 8
  %v7.i = and i64 %p1, 63
  %0 = lshr i64 %v55, %v7.i
```

- `BitSet` is no generic, so the symbol carries no brackets and the program holds one copy of it.
- The first compare refuses a number below 0.
- The shift by 6 is the division by 64, the index of the word. The load at offset 104 reads the count of words, and the compare refuses a number past the room.
- The last load reads the word, the `and` with 63 is the bit in it, and the shift brings that bit down.

`union` of two `BitSet` values combines word by word with `|`, `intersect` with `&`, and `minus` with `& ~`. `count` of the result comes from a count of the set bits of each word.

Where it lives: `src/std/anti/collection/set.anti`, with `resize`, `combined`, `next_bit` and `ones` of `BitSet`. The two tables stand in `src/std/anti/collection/map.anti`.

### What it costs

| Cost | Count |
|---|---|
| `size_of(Set<int>)`, `size_of(HashSet<int>)` | 160 each |
| `size_of(BitSet)` | 112 |
| Bytes per element of capacity, `Set<T>` | `size_of(T)` and 8, and at least 32 for its two buckets |
| Bytes per element of capacity, `HashSet<T>` | two slots: twice `size_of(T)` and 16 |
| Bytes of a `BitSet` | one bit per number up to the largest, in words of 8 bytes |
| `add`, `remove`, `contains` of a `Set` and a `HashSet` | one hash, one mix with the seed, one probe |
| `add`, `remove`, `contains` of a `BitSet` | one shift, one load and one mask. `add` copies 64 bytes for the place of the call |
| `union`, `intersect`, `minus` | one new set and one lookup per element. A `BitSet` takes one operation per word |
| A walk of a `BitSet` | one test per word, and one step per set bit |
| Heap allocations | one block per growth. A `BitSet` doubles its words |
| Atomic operations and locks | one atomic load when a `Set` or a `HashSet` is made, for the seed. None for a `BitSet` |
| Checks | none of its own. The checks of dev mode stand in the arithmetic, under `--checks` and `--no-checks` |
| Copies of code | one copy per element type of a `Set` and a `HashSet`, and one `BitSet` |

### Pitfalls

For a Java programmer:

- `union`, `intersect` and `minus` give a new set. Neither operand changes, where `addAll` and `retainAll` change the receiver.
- A `BitSet` of the one number 1,000,000 takes 16,384 words, 131,072 bytes. The room follows the largest number and not the count.

For a C programmer:

- A `BitSet` has no fixed width. It grows on `add`, and that `add` allocates.
- A `Set<str>` compares the bytes of the texts and stores the views. The bytes must outlive the set.

For a C++ programmer:

- `update_all` of a set compiles and changes nothing, since the set lends a copy. `remove` and `add` replace an element.
- `==` of two `Set<T>` values ignores the order, although a walk has one.

## SortedMap and SortedSet

### What you know

`TreeMap` and `TreeSet` of Java and `std::map` and `std::set` of C++ are red-black trees with one node on the heap per element. Each lookup follows a pointer per level. C has `qsort` and `bsearch` over an array that the program keeps sorted.

### The feature

`anti.collection.sorted` holds `SortedMap<K, V>` and `SortedSet<T>`. Both walk their elements in the order of their keys. The key type meets `Ordered`, which is `==` and `<`. The numbers and `str` have both, and a struct declares `operator fn lt`, as "Order and equality" below shows.

A sorted map has the operations of a map: `get`, `contains`, `set`, `add`, `remove`, `update`, `read`, `modify`, `keys`, `values`, `reserve` and `shrink`. A sorted set has those of a set: `add`, `remove`, `contains`, `union`, `intersect`, `minus` and `is_subset`. Both add the operations of the order.

| Operation | Result |
|---|---|
| `m.first()`, `m.last()` | the entry of the smallest and of the largest key, a `?(K, V)`. A set gives a `?T` |
| `m.floor(k)` | the entry of the largest key that is not above `k` |
| `m.ceiling(k)` | the entry of the smallest key that is not below `k` |
| `m.range(lo, hi)` | an iterator over the entries from `lo` up to and without `hi` |
| `m.pop_first()`, `m.pop_last()` | remove the first and the last entry and give it. The map alone has them |

```anti
//! name: sorted
import anti.io;
import anti.mem;
import anti.collection.sorted.{SortedMap, SortedSet};

class Counting inherits mem.Allocator
{
	pub taken: int = 0,
	pub last: int = 0,

	concrete fn alloc(self, size: int, align: int) -> ?*byte
	{
		self.taken += 1;
		self.last = size;
		return mem.LibcAllocator.get().alloc(size, align);
	}

	concrete fn free(self, p: ?*byte)
	{
		mem.LibcAllocator.get().free(p);
	}
}

fn main() -> int
{
	let c = Counting { };
	let m = SortedMap<int, str>.new(0, &c);
	m.set(30, "thirty");
	m.set(10, "ten");
	m.set(20, "twenty");
	m.set(40, "forty");
	let t = m.to_text();
	io.println(t);
	free(t.ptr);
	let (fk, fv) = m.floor(25) ?? (0, "none");
	let (ck, cv) = m.ceiling(25) ?? (0, "none");
	let (lk, lv) = m.last() ?? (0, "none");
	let line = f"floor(25) {fk}, ceiling(25) {ck}, last {lk}, ceiling(41) {m.ceiling(41) != none}";
	io.println(line);
	free(line.ptr);
	for (k, v) in m.range(15, 40) {
		line = f"in range: {k} {v}";
		io.println(line);
		free(line.ptr);
	}
	let (pk, pv) = m.pop_first() ?? (0, "none");
	line = f"popped {pk}, count {m.count}";
	io.println(line);
	free(line.ptr);

	let s = SortedSet<int>.new(0, &c);
	let seen = c.taken;
	for i in 0..100 {
		s.add((i * 37) % 100);
		if c.taken != seen {
			seen = c.taken;
			line = f"add {i + 1}: {c.last} bytes";
			io.println(line);
			free(line.ptr);
		}
	}
	let low = s.first() ?? -1;
	let high = s.last() ?? -1;
	let sum = 0;
	for x in s.range(10, 15) {
		sum += x;
	}
	line = f"set of {s.count}: first {low}, last {high}, range(10, 15) adds up to {sum}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: SortedMap {size_of(SortedMap<int, str>)}, SortedSet {size_of(SortedSet<int>)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
{10: "ten", 20: "twenty", 30: "thirty", 40: "forty"}
floor(25) 20, ceiling(25) 30, last 40, ceiling(41) false
in range: 20 twenty
in range: 30 thirty
popped 10, count 3
add 1: 200 bytes
add 12: 800 bytes
add 25: 1600 bytes
add 51: 3200 bytes
set of 100: first 0, last 99, range(10, 15) adds up to 60
sizes: SortedMap 160, SortedSet 160
```

A value of a sorted map is lent and changed in place, as in a map. A key and an element of a sorted set are given as copies. The update forms of the base lend a whole entry. A function that moves the key out of its order ends the program. The message is "a lent element of a sorted collection was moved out of its order".

The checker refuses a key without an order.

```anti refused
//! error: `P` has no `lt`, which `SortedMap` needs for `K`. A struct or a class has no default order and declares `operator fn lt`
import anti.collection.sorted.{SortedMap};

struct P
{
	x: int,
}

fn make()
{
	let m = SortedMap<P, int>.new();
}
```

### How antic builds it

Both are B-trees over `BTree<T>`. A node holds up to 11 elements side by side and up to 12 children, and every node but the root holds at least 5 elements. A full node splits into two nodes of 5 and the element between them, which moves up.

Every node of a tree stands in one block. A node is an index into the block and no pointer of its own. The block holds the elements of all nodes, 11 slots each, and then the links of all nodes, 14 words each. The links of a node are the count of its elements, its parent and 12 children.

| Part | Contents | Bytes |
|---|---|---|
| Slots | 11 elements per node | `nodes * 11 * size_of(T)`, rounded up to 8 |
| Links | 14 words per node | `nodes * 112` |

A node of a `SortedSet<int>` is 88 and 112 bytes, the 200 of the output. The block grows by doubling its nodes, from 1. A removed node joins a list of free nodes that the next split takes first. `reserve(n)` takes room for one node per 5 elements, which is the most a tree of `n` elements needs.

The object is 160 bytes: the 96 of `Collection<T>`, the block, the slots, the links, the root and four counts of nodes.

A lookup walks from the root. In each node it compares the key with the elements in a line, from the first. It goes down the child at the first element that is not below the key. `locate` of a `SortedSet<int>` in a release build:

```llvm excerpt=sorted target=linux-x86_64 mode=release
@"anti.collection.sorted.SortedSet$3cint$3e.locate"(ptr
...
  %v19.i.i = mul i64 %t5.051, 112
...
  %v4.i = mul i64 %t5.051, 11
...
  %v21.i = load i64, ptr %v20.i.i, align 8
  %v26.i = icmp slt i64 %v21.i, %v23.i
```

- The multiply by 112 finds the links of the node, 14 words of 8 bytes, whose first word is the count of its elements.
- The multiply by 11 is the first slot of the node.
- The load and the compare are one step of the search in the node. For an `int` key the `<` is one instruction, inlined into the loop.

A key type with `operator fn lt` of its own costs one call of that function per step, unless opt inlines it.

Where it lives: `src/std/anti/collection/sorted.anti`. `BTree<T>` holds `take_node`, `split`, `rebalance`, `merge`, `compact` and `resize`. `lower`, `locate`, `floor_place` and `ceiling_place` of each class hold the search.

### What it costs

| Cost | Count |
|---|---|
| `size_of(SortedMap<int, str>)`, `size_of(SortedSet<int>)` | 160 each |
| Bytes per node | `11 * size_of(T)` and 112. 200 for a `SortedSet<int>`, 376 for a `SortedMap<int, str>` |
| Bytes per element | between one eleventh and one fifth of a node, apart from the root: 18 to 40 for a `SortedSet<int>` |
| Heap allocations | one block per growth of the nodes, by doubling. No allocation per element |
| `get`, `contains`, `floor`, `ceiling` | one node per level, and up to 11 compares in a node |
| Levels | a tree of 1, 2, 3 and 4 levels holds up to 11, 143, 1,727 and 20,735 elements |
| `set` and `add` of a new key | the search, a move of up to 10 elements in the leaf, and a split of each full node on the way |
| `remove` | the search, a move in the leaf, and a merge or a loan from a neighbour when a node falls below 5 |
| `first`, `last` | one node per level |
| A walk, `range` | one step per element, in the order of the keys |
| `shrink` | moves the nodes down to the start of the block and allocates once |
| Atomic operations and locks | none |
| Checks | the order check after a lent entry changed, two compares, in every build |
| Copies of code | one copy of the class and of `BTree<T>` per set of type arguments |

A sorted collection reads no seed and calls no hash.

### Pitfalls

For a Java programmer:

- The order comes from `<` of the key type. No comparator is passed when the map is made.
- `floor` and `ceiling` give a `?(K, V)`, a copy of the entry. `floorKey` and `floorEntry` have this one form.
- `range(lo, hi)` leaves `hi` out, as `subMap` does.

For a C programmer:

- A struct key needs `operator fn lt` at module level. Without it the map is refused, and no `memcmp` order stands in.
- A `str` key is compared by its bytes at every step of every lookup.

For a C++ programmer:

- No iterator stays valid across a change. A growth moves every node, and a split moves elements between nodes.
- `==` of the key decides whether `set` replaces. A `<` that disagrees with `==` gives a map with two entries the program holds equal.
- A `lent` entry that changes its key ends the program. `remove` and `set` move an entry to another key.

## Pool and Handle

### What you know

A Java program holds a reference to an object, and the collector keeps the object alive. C and C++ hold a pointer or an index, and both go stale. The pointer does when the block moves or is freed, and the index when the slot is reused. Game engines pair the index with a generation for that reason.

### The feature

`Pool<T>` of `anti.collection.pool` keeps each element in a slot that never moves. `add` gives a `Handle<T>`, a struct of the index of the slot and the generation of the slot. A handle is two integers. It may be copied, stored in a field and compared with `==`.

| Operation | Result |
|---|---|
| `Pool<T>.new(capacity, from)` | an empty pool. Both arguments are optional |
| `p.add(x)` | puts `x` into a free slot and gives its `Handle<T>` |
| `p.remove(h)` | takes the element out and gives it as a `?T` |
| `p.get(h)` | a `?T`, a copy of the element |
| `p.set(h, x)` | replaces the element, and gives `false` when it is gone |
| `p.contains(h)` | whether `h` finds an element |
| `p.read(h, f)`, `p.modify(h, f)` | lend the element to `f` |
| `p.get_versioned(h)` | a `?(T, int)`, the copy and the version of the element |
| `p.set_if_version(h, x, version)` | replaces the element when its version still stands, and fails with `Changed` otherwise |

`remove` frees the slot and raises its generation. A handle of the removed element then finds nothing, also after the slot holds another element. Every form above gives `none` or `false` for such a handle.

The version of an element counts its changes since it was added: `set`, `set_if_version`, `modify`, the update forms, and each turn of a `for` that reaches it.

`for x in &p` walks the elements in the order of their slots and lends each.

```anti
//! name: pools
import anti.io;
import anti.mem;
import anti.collection.pool.{Pool, Handle};

class Counting inherits mem.Allocator
{
	pub taken: int = 0,
	pub last: int = 0,

	concrete fn alloc(self, size: int, align: int) -> ?*byte
	{
		self.taken += 1;
		self.last = size;
		return mem.LibcAllocator.get().alloc(size, align);
	}

	concrete fn free(self, p: ?*byte)
	{
		mem.LibcAllocator.get().free(p);
	}
}

struct Unit
{
	hp: int,
	x: f32,
	y: f32,
}

fn main() -> int
{
	let c = Counting { };
	let units = Pool<Unit>.new(0, &c);
	let a = units.add(Unit { hp: 10, x: 0.0, y: 0.0 });
	let b = units.add(Unit { hp: 20, x: 1.0, y: 0.0 });
	units.modify(a, fn(u) {
		u.hp -= 3;
	});
	let gone = units.remove(b);
	let d = units.add(Unit { hp: 30, x: 2.0, y: 2.0 });
	let stale = units.get(b);
	let fresh = units.get(d) ?? Unit { hp: 0, x: 0.0, y: 0.0 };
	let first = units.get(a) ?? Unit { hp: 0, x: 0.0, y: 0.0 };
	let line = f"a has {first.hp}, the handle of b finds {stale != none}, d has {fresh.hp}, count {units.count}";
	io.println(line);
	free(line.ptr);
	line = f"b is slot {b.index} of generation {b.generation}, d is slot {d.index} of generation {d.generation}";
	io.println(line);
	free(line.ptr);
	let seen = c.taken;
	for i in 0..100 {
		units.add(Unit { hp: i, x: 0.0, y: 0.0 });
		if c.taken != seen {
			seen = c.taken;
			line = f"add {i + 3}: block {c.taken} of {c.last} bytes";
			io.println(line);
			free(line.ptr);
		}
	}
	line = f"sizes: Pool {size_of(Pool<Unit>)}, Handle {size_of(Handle<Unit>)}, Unit {size_of(Unit)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
a has 7, the handle of b finds false, d has 30, count 2
b is slot 1 of generation 0, d is slot 1 of generation 1
add 9: block 2 of 640 bytes
add 25: block 3 of 1280 bytes
add 57: block 4 of 2560 bytes
sizes: Pool 520, Handle 16, Unit 16
```

`d` took the slot that `b` left. The two handles differ in the generation, so `b` finds nothing.

The checker refuses a handle of another element type.

```anti refused
//! error: expected `Handle<int>`, found `Handle<str>`
import anti.collection.pool.{Pool, Handle};

fn read(p: *Pool<int>, h: Handle<str>) -> ?int
{
	return p.get(h);
}
```

The type of a handle names the element type alone. Nothing in it names the pool. A handle of one `Pool<int>` passes the checker at another `Pool<int>`, where it finds another element or nothing.

### How antic builds it

A pool grows by adding blocks and never copies one. The first block holds 8 slots, or the capacity it was made with, rounded up to a power of two. Each further block holds twice the slots of the one before. Slot `i` lies in the block that the highest set bit of `i + 8` names, so the address of a slot is a few shifts and one load.

The memory of a block holds its elements and then three words per slot.

| Word | Contents |
|---|---|
| Generation | raised by each `remove` of the slot |
| Link | -2 for a slot that holds an element, the next free slot otherwise, or -1 at the end of the free list |
| Version | the count of changes of the element |

A `Unit` is 16 bytes, so a slot is 40 bytes and the second block of 16 slots is the 640 bytes of the output. `add` takes the slot freed last, and a new slot when no slot is free.

The object is 520 bytes: the 96 of `Collection<T>`, an array of 48 block pointers and five words. 48 blocks reach past 2^50 slots.

`contains` in a release build holds the whole test of a handle:

```llvm excerpt=pools target=linux-x86_64 mode=release
@"anti.collection.pool.Slots$3cpools.Unit$3e.contains"(ptr
b0:
  %v4.i = icmp slt i64 %p1.0, 0
...
@"anti.collection.pool.Slots$3cpools.Unit$3e.word"(ptr nonnull readonly dereferenceable(520) %p0, i64 %p1.0, i64 1)
  %v12.not.i = icmp eq i64 %v10.i, -2
...
  %v17.i = getelementptr inbounds nuw i8, ptr %p0, i64 496
  %v19.i = load i64, ptr %v17.i, align 8
  %v22.not.i = icmp slt i64 %p1.0, %v19.i
...
@"anti.collection.pool.Slots$3cpools.Unit$3e.word"(ptr nonnull readonly dereferenceable(520) %p0, i64 %p1.0, i64 0)
  %v36.not.i = icmp eq i64 %v29.i, %p1.1
```

- The handle arrives as two integers, `%p1.0` the index and `%p1.1` the generation. The symbol names `Slots<T>`, the base that a pool and a tree share.
- The first compare and the compare with the word at offset 496 refuse an index below 0 and one past the slots handed out.
- The call of `word` with 1 reads the link of the slot, and the compare with -2 tests that the slot holds an element.
- The call of `word` with 0 reads the generation of the slot, and the last compare tests it against the generation of the handle.

Where it lives: `src/std/anti/collection/pool.anti`. `Slots<T>` holds `slot_of`, `claim`, `release`, `block_of` and `make_block`, and `slot_element`, `slot_words` and `highest_bit` find a slot.

### What it costs

| Cost | Count |
|---|---|
| `size_of(Pool<Unit>)` | 520 |
| `size_of(Handle<Unit>)` | 16 |
| Bytes per slot | `size_of(T)` and 24. 40 for a `Unit` of 16 bytes |
| Heap allocations | one per block of 8 slots, then of 16, then of 32, each twice the one before. No block is copied or freed before the pool ends |
| `add` | one slot from the free list or the next new one. No element moves |
| `get`, `set`, `contains`, `read`, `modify`, `remove` | two compares of the index, one of the link, one of the generation |
| The address of a slot | the highest bit of the index, found with six compares, and one load of the block pointer |
| `remove` | zeroes the slot and raises the generation. No element moves |
| A walk | every slot handed out, free ones included, and one more version per element reached |
| Atomic operations and locks | none. `SyncPool<T>` of chapter 14 is the pool for more than one thread |
| Checks | the test of the handle, in every build |
| Copies of code | one copy of `Pool<T>`, `Slots<T>` and `Collection<T>` per element type |

A pool never gives a block back before its teardown. A pool that held a million elements once keeps the room of a million.

### Pitfalls

For a Java programmer:

- A handle keeps nothing alive. `remove` ends the element, and every handle of it finds `none` from then on.
- `get` gives a copy. `modify` changes the element in its slot.

For a C programmer:

- A handle is checked at every use. A stale one gives `none` and never another element of the same pool.
- The slot of an element never moves, yet no operation gives its address past a call.

For a C++ programmer:

- A handle of one pool compiles at another pool of the same element type. Nothing in the handle names its pool.
- A pool gives no block back before it ends, and `dup` copies every block in its size. A handle of the pool finds the same element in the copy.
- `for x in &p` raises the version of every element it reaches, whether the body changes it or not.

## Tree and PriorityQueue

### What you know

A tree in Java is a node class with a list of children, one object per node. C and C++ write the same with pointers and pay an allocation per node. `PriorityQueue` of Java and `std::priority_queue` of C++ are binary heaps in an array. Java gives the smallest element first and C++ the largest.

### The feature

`Tree<T>` of `anti.collection.tree` is a hierarchy of elements on the slots of a pool. Every element is reached by a `Handle<T>`, and the reading and lending forms are those of a pool.

| Operation | Result |
|---|---|
| `t.add_root(x)` | adds `x` at the top, after the other roots, and gives its handle |
| `t.add_child(parent, x)` | adds `x` below `parent`, after its other children. A `?Handle<T>`, `none` when the parent is gone |
| `t.parent(h)` | the handle of the parent, a `?Handle<T>`. `none` for a root |
| `t.children(h)` | the handles of the children in order, a `Copies<Handle<T>>` |
| `t.depth_first(h)`, `t.breadth_first(h)` | the handles of `h` and of everything below it, in the two orders |
| `t.remove(h)` | removes the element with its whole subtree and gives the number removed |

`for x in &t` walks the elements in the order of their slots, which is no order of the tree. A `remove` form of the base takes out the matches and leaves their children in their place, under the parent of each.

`PriorityQueue<T>` of `anti.collection.queue` gives the smallest element first. `T` meets `Ordered`.

| Operation | Result |
|---|---|
| `q.push(x)` | adds `x` |
| `q.pop()` | removes the smallest element and gives it, a `?T` |
| `q.peek()` | a copy of the smallest element, a `?T` |

`for x in q` walks copies in the order of the array. `pop` gives another order, smallest first. An update form changes elements in place and restores the order after it.

```anti
//! name: trees
import anti.io;
import anti.collection.tree.{Tree};
import anti.collection.queue.{PriorityQueue};

fn main() -> int
{
	let menu = Tree<str>.new();
	let file = menu.add_root("File");
	let edit = menu.add_root("Edit");
	let open = menu.add_child(file, "Open") catch fatal;
	menu.add_child(file, "Save");
	menu.add_child(open, "Recent");
	menu.add_child(edit, "Undo");
	let out = "";
	for h in menu.depth_first(file) {
		let name = menu.get(h) ?? "?";
		let next = f"{out} {name}";
		if out.len > 0 {
			free(out.ptr);
		}
		out = next;
	}
	let up = menu.parent(open) ?? edit;
	let above = menu.get(up) ?? "?";
	let removed = menu.remove(open);
	let line = f"below File:{out}. Open stands under {above}. Removing it took {removed}, {menu.count} left";
	io.println(line);
	free(line.ptr);
	free(out.ptr);

	let q = PriorityQueue<int>.new();
	q.push(5);
	q.push(1);
	q.push(9);
	q.push(3);
	let smallest = q.peek() ?? -1;
	let one = q.pop() ?? -1;
	let two = q.pop() ?? -1;
	line = f"peek {smallest}, pops {one} {two}, {q.count} left";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Tree {size_of(Tree<str>)}, PriorityQueue {size_of(PriorityQueue<int>)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
below File: File Open Recent Save. Open stands under File. Removing it took 2, 4 left
peek 1, pops 1 3, 2 left
sizes: Tree 528, PriorityQueue 136
```

The checker refuses the following.

```anti refused
//! error: `P` has no `lt`, which `PriorityQueue` needs for `T`. A struct or a class has no default order and declares `operator fn lt`
import anti.collection.queue.{PriorityQueue};

struct P
{
	x: int,
}

fn make()
{
	let q = PriorityQueue<P>.new();
}
```

```anti refused
//! error: `for x in &e` takes each element in place, and the `operator fn value` of `QueueWalk<int>` gives a copy of `int`
import anti.collection.queue.{PriorityQueue};

fn zero(q: *PriorityQueue<int>)
{
	for x in &q {
		*x = 0;
	}
}
```

A walk of a queue lends nothing, since a change in place would break the order of the heap.

### How antic builds it

A `Tree<T>` inherits `Slots<T>`, the storage of a pool, and keeps five more words per slot. They are the parent, the first child, the last child, the next sibling and the previous sibling. Each is the index of a slot, or -1. A slot of a tree is `size_of(T)` and 64 bytes. The object is the 520 bytes of a pool and the index of the last root.

`add_child` claims a slot and links it after the last child of its parent. `remove` unlinks the top of the subtree and then frees the slots from the leaves up, so it uses no stack and no list of its own. `children`, `depth_first` and `breadth_first` follow the links and fill a `Copies<Handle<T>>` from the allocator of the tree.

A `PriorityQueue<T>` inherits `Indexed<T>` and has the 136 bytes and the growth of a list. Its block holds one spare slot past the capacity, which a swap goes through. The element at `i` is no larger than those at `2i + 1` and `2i + 2`, so the smallest stands at 0.

`push` writes the element at the end and moves it up. The loop of a `PriorityQueue<int>` in a release build:

```llvm excerpt=trees target=linux-x86_64 mode=release
@"anti.collection.queue.PriorityQueue$3cint$3e.push"(ptr
...
  %v20.i8 = add nsw i64 %t7.08.i, -1
  %v266.i = lshr i64 %v20.i8, 1
...
  %v39.i = load i64, ptr %v37.i, align 8
...
  %v49.i = load i64, ptr %v47.i, align 8
  %v52.i = icmp slt i64 %v39.i, %v49.i
...
  tail call fastcc void @"anti.collection.queue.PriorityQueue$3cint$3e.swap"(
  %v15.not.i = icmp eq i64 %v266.i, 0
```

- The `add` of -1 and the shift by 1 are `(at - 1) / 2`, the position of the parent.
- The two loads read the element and its parent, and the compare is the `<` of `int`.
- The call swaps the two when the element is smaller, and the last compare ends the loop at the root.

`pop` moves the last element to position 0 and moves it down, with up to two compares per level.

Where it lives: `src/std/anti/collection/tree.anti` with `attach`, `detach` and `lift`, and `src/std/anti/collection/queue.anti` with `sift_up`, `sift_down`, `fix` and `heapify`.

### What it costs

| Cost | Count |
|---|---|
| `size_of(Tree<str>)` | 528 |
| `size_of(PriorityQueue<int>)` | 136 |
| Bytes per slot of a tree | `size_of(T)` and 64: the three words of a pool and five links |
| Heap allocations of a tree | one per block, as for a pool. No allocation per node |
| `add_root`, `add_child` | one slot, and the links of the new element and of its neighbours |
| `parent` | the test of the handle and one read |
| `children`, `depth_first`, `breadth_first` | one new `Copies<Handle<T>>`, which allocates as a list does, and one read per step |
| `remove` of a subtree of `n` elements | `n` slots freed, and no allocation |
| `push` of a queue of `n` elements | up to `log2 n` compares and swaps, and the growth of a list |
| `pop` | up to `2 log2 n` compares and `log2 n` swaps |
| `peek` | one copy of the element at 0 |
| `update_all` and `remove_all` of a queue | one pass, and one rebuild of the heap over all elements |
| Atomic operations and locks | none |
| Checks | the test of each handle of a tree, in every build |

### Pitfalls

For a Java programmer:

- A node of a tree is no object with fields `parent` and `children`. It is a handle, and the tree answers the questions about it.
- `PriorityQueue<T>` takes no comparator. A queue of the largest first needs a type whose `<` is reversed.

For a C programmer:

- `children(h)` allocates a collection for its answer. A loop that calls it per node allocates per node.
- `remove(h)` of a tree ends the whole subtree. A `remove_first` of the base ends one element and keeps its children.

For a C++ programmer:

- The queue gives the smallest element first, where `std::priority_queue` gives the largest.
- `for x in q` is no sorted walk. Repeated `pop` is the sorted order, and it empties the queue.
- A walk of a tree goes by slot. `depth_first` gives the order of the tree.

## Walking a collection

### What you know

`for (T x : c)` of Java calls `iterator()`, `hasNext()` and `next()`, and a change during the loop throws `ConcurrentModificationException`. The range `for` of C++ calls `begin()` and `end()`, and a change that moves the elements leaves the iterators dangling without a message. C walks with an index or a pointer.

### The feature

`for` walks a range, a slice, an array, a collection or an iterator. Chapter 4 holds the loop itself. This section holds what a collection adds.

| Form | What the body receives |
|---|---|
| `for x in c` | a copy of each element. The body may not assign to `x` |
| `for x in &c` | a pointer to each element, lent for one turn of the loop |
| `for (k, v) in m` | a copy of each key and of each value of a map |
| `for (k, v) in &m` | a copy of each key, and a lent pointer to each value |
| `for k in m.keys()`, `for v in m.values()` | copies of the keys, and the values |
| `for x in m.range(lo, hi)` | the elements of a sorted collection from `lo` up to and without `hi` |

A collection lends an element when a change in place cannot break it.

| Collection | `for x in c` | `for x in &c` |
|---|---|---|
| `List`, `Deque`, `Ring`, `Grid`, `Copies`, `Pool`, `Tree` | a copy | a lent `*T` |
| `Map`, `HashMap`, `SortedMap` | a copy of `(K, V)` | the key as a copy and the value as a lent `*V` |
| `Set`, `HashSet`, `SortedSet`, `BitSet`, `PriorityQueue` | a copy | refused |

A type is walked through three hooks. `operator fn iter` gives an iterator. `operator fn next` moves an iterator on and gives `false` after the last element. `operator fn value` gives the element the iterator stands at. A type with `next` and `value` is an iterator, and a `for` over a type with `iter` walks the iterator it gives. `it.to_slice()` collects what is left of any iterator into a slice on the heap, which the program frees with `free(s.ptr)`.

`Iterable<T>` and `Iterator<T>` of `anti.collection` are the constraints of a generic function that takes any collection or any iterator of an element type.

A dev build stops the program when the size of a collection changes while a `for` walks it. The message names the loop and the place of the change: `` `l` was changed while `for` walked it: changed at app.anti:10 ``. The remove forms of the collection remove while they walk. A release build makes no such test.

```anti
//! name: walks
import anti.io;
import anti.collection.{Iterable};
import anti.collection.list.{List};
import anti.collection.map.{Map};
import anti.collection.set.{Set};

struct Countdown
{
	left: int,
}

operator fn iter(c: Countdown) -> Countdown
{
	return c;
}

operator fn next(c: *Countdown) -> bool
{
	c.left -= 1;
	return c.left >= 0;
}

operator fn value(c: *Countdown) -> int
{
	return c.left;
}

fn total<C: Iterable<int>>(c: *C) -> int
{
	let sum = 0;
	for x in c {
		sum += x;
	}
	return sum;
}

fn sum(l: *List<int>) -> int
{
	let total = 0;
	for x in l {
		total += x;
	}
	return total;
}

fn main() -> int
{
	let l = List<int>.new();
	for i in 1..5 {
		l.push(i);
	}
	for x in &l {
		*x *= 10;
	}
	let s = Set<int>.new();
	s.add(5);
	s.add(6);
	let line = f"list {total(&l)}, set {total(&s)}, sum {sum(&l)}";
	io.println(line);
	free(line.ptr);

	let stock = Map<str, int>.new();
	stock.set("nuts", 3);
	stock.set("bolts", 8);
	for (name, n) in &stock {
		*n += 1;
	}
	for (name, n) in stock {
		line = f"{name}: {n}";
		io.println(line);
		free(line.ptr);
	}

	let down = Countdown { left: 3 };
	for n in down {
		line = f"countdown {n}";
		io.println(line);
		free(line.ptr);
	}
	let all = Countdown { left: 4 }.iter().to_slice();
	line = f"to_slice gave {all.len} elements, the first {all[0]}";
	io.println(line);
	free(line.ptr);
	free(all.ptr);
	return 0;
}
```

```text output
list 100, set 11, sum 100
nuts: 4
bolts: 9
countdown 2
countdown 1
countdown 0
to_slice gave 4 elements, the first 3
```

The checker refuses the following.

```anti refused
//! error: `for i, x` binds the index and the element of a slice or an array
import anti.collection.list.{List};

fn numbered(l: *List<int>)
{
	for i, x in l {
	}
}
```

```anti refused
//! error: `x` is lent for one turn of the loop and cannot be stored
import anti.collection.list.{List};

fn keep(l: *List<int>) -> ?*int
{
	let p: ?*int = none;
	for x in &l {
		p = x;
	}
	return p;
}
```

```anti refused
//! error: `for` walks a range, a slice, an array, a collection or an iterator, found `S`
struct S
{
	n: int,
}

fn each(s: S)
{
	for x in s {
	}
}
```

```anti refused
//! error: `List<str>` has no `Iterable<int>`, which `total` needs for `C`
import anti.collection.{Iterable};
import anti.collection.list.{List};

fn total<C: Iterable<int>>(c: *C) -> int
{
	return 0;
}

fn words(l: *List<str>) -> int
{
	return total(l);
}
```

### How antic builds it

The checker turns `for x in c` into three calls: `c.iter()` once before the loop, `next()` in the test and `value()` at the head of the body. The iterator is a hidden local of the frame, and no walk of a collection allocates.

The iterator of a list is an `IndexedWalk<T>` of 64 bytes. It holds the block, the capacity, the head and the count of the list as they were when the loop began, a position, and a `Watch`. A `Watch` is a pointer to the `changes` of the collection and the count of changes it saw when the walk began.

Every operation that changes the size of a collection raises the count in `changes` and writes its place there. In a dev build the test of the loop compares the two counts before each `next()`.

```llvm excerpt=walks target=linux-x86_64 mode=dev
define hidden noundef i64 @walks.sum(ptr noundef nonnull dereferenceable(136) %p0)
...
  %v21 = icmp ne i64 %v19, %v20
...
  %v26 = call ptr (ptr) @anti.collection.IndexedWalk$3cint$3e.value(ptr %v25)
...
  call void (ptr, ptr) %v40(ptr sret([64 x i8]) align 8 %a0, ptr %v41)
...
  call void (ptr, i64, ptr, i64, i64) @anti_rt_walk_changed(ptr
...
  %v77 = call range(i8 0, 2) i8 (ptr) @anti.collection.IndexedWalk$3cint$3e.next(ptr %v76)
```

- The compare is the walk check. Its operands are the count in `changes` of the list and the count in the `Watch` of the iterator.
- The call of `value` gives the lent pointer to the element, which the loop reads into `x`.
- The indirect call is `iter`, dispatched through the table of the list. It writes the 64 bytes of the iterator into the frame.
- `anti_rt_walk_changed` prints the message and ends the program. Its arguments are the text that names the loop and the file and the line of the last change.
- The call of `next` moves the position on.

A release build drops the compare and the call of `anti_rt_walk_changed`, with the other checks of dev mode. The whole-program pass replaces the dispatch of `iter` by a direct call, and opt inlines the three functions into the loop. `sum` of the example is gone from the release module, inlined into `main`.

`value` of the iterator in a release build finds the slot as `slot` of the list does:

```llvm excerpt=walks target=linux-x86_64 mode=release
define internal noundef ptr @"anti.collection.IndexedWalk$3cint$3e.value"(ptr
...
  %v23 = add i64 %v20, %v16
...
  %v30.not = icmp slt i64 %v23, %v27
  %v40 = select i1 %v30.not, i64 0, i64 %v27
  %spec.select = sub i64 %v23, %v40
  %v45 = shl i64 %spec.select, 3
```

- The `add` is `head + at`, and the compare, the `select` and the `sub` take the capacity off when the sum reaches it.
- The shift by 3 turns the slot into the offset of an `int`.

A walk of a `Map` steps over the entries in order and passes over the removed ones. A walk of a `HashMap` starts at a random slot and steps by a random odd stride. Such a stride reaches every slot of a table of a power of two once. The random number comes from 8 bytes that `anti_rt_entropy` draws at the first walk of the map. Each walk mixes them with the count of walks so far.

`to_slice` writes each value into memory from `anti_rt_grow`, which is `realloc` of the C library.

Where it lives: `sema_iterate` and `watch_changes` of `src/antic/sema_expr.c`, `lower_for_hooks` and `walk_check` of `src/antic/lower_stmt.c`, and `anti_rt_walk_changed` of `src/rt/check.c`. `IndexedWalk<T>` stands in `src/std/anti/collection.anti`, and `Stride` with `advance` in `src/std/anti/collection/map.anti`. `Changes` and `Watch` stand in `src/std/anti/lang.anti`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Bytes of the iterator of a list, in the frame | 64 | 64, or none after inlining |
| Bytes of `changes` in every collection | 72 | 72 |
| Heap allocations of a walk | none | none |
| Calls per element | `next()` and `value()` | none after inlining |
| The call of `iter` | one dispatch through the table | one direct call, or none after inlining |
| The walk check | one compare per turn | none |
| `for x in c` | one copy of the element per turn, as `dup` makes it | the same |
| `for x in &c` | no copy | no copy |
| Every `push`, `pop`, `add` and `remove` | one add, and a copy of 64 bytes for the place of the change | the same |
| The first walk of a `HashMap` or a `HashSet` | one call of `anti_rt_entropy` | the same |
| `to_slice` | one `realloc` per growth of the slice | the same |
| Atomic operations and locks | none | none |

`--no-checks` drops the walk check from a dev build, and `--checks` keeps it in a release build. Without it, a loop over a collection that the body grows reads the block the list had when the loop began. That block is freed by the growth.

The record of the changes is paid in every build, since the fields and the stores stand in the library code. A release build writes the place of each change and never reads it.

### Pitfalls

For a Java programmer:

- A release build has no `ConcurrentModificationException`. The check is a tool of dev mode.
- The check sees a change of the size. `l[i] = x` inside a `for` over `l` is no change of the size and passes.
- `for x in c` copies each element. For a struct of 200 bytes that is 200 bytes per turn, and `for x in &c` copies nothing.

For a C programmer:

- The loop has no index. `for i, x in s` exists for a slice and an array, and `lend_slice` gives a list as a slice.
- The pointer of `for x in &c` is valid for one turn. The checker refuses to store it.

For a C++ programmer:

- No `begin()` and `end()` pair exists. An iterator knows its own end, and `next()` gives `false` there.
- Two walks of one `HashMap` give two orders. A test that prints a `HashMap` in a loop fails on its second run, by design of the walk.
- A `for` over a set never lends. The element is the key of its slot, and a change would move it.

## Hashing and its seed

### What you know

Every Java object has `hashCode()`, an `int` of 32 bits, and `String.hashCode()` is the same on every run. C++ has `std::hash<T>` for the built-in types and none for a struct until the program writes one. C has no hash. An attacker who knows the hash of a server's table can pick keys that all land in one bucket.

### The feature

`x.hash()` gives a `u64` for a value of every type. The hash of a value is the same on every run and on every target.

| Type | Hash |
|---|---|
| An integer, a `bool`, a `char`, a float, a pointer | the 64 bits of the value through one mixing function |
| A `str` and a slice of bytes | the bytes, eight at a time |
| A struct, a tuple, an array, a slice | the parts in order, each mixed into the hash so far |
| A `?T` and a variant | the flag or the tag, then the value or the fields of the case |
| A class value | the fields of its chain, unless the class declares `concrete fn hash(self) -> u64` |
| A collection | the elements: in order for a sequence, whatever the order for a map and a set |

A struct replaces the default with `operator fn hash(a: S) -> u64` at module level. A type that declares `==` of its own declares the hash with it, since two values that are equal must hash alike. The default `==` and the default hash agree.

The hashing collections do not use `x.hash()` as it is. Each mixes it with a seed, a `u64` that the runtime draws from the random source of the system when the program starts. `lang.hash_seed()` gives the seed. A collection reads it once, when it is made.

The option `--anti.hash_seed=<n>` of every program and the key `hash_seed` of the runtime configuration fix the seed. A test that needs the same buckets on every run sets one of them. Chapter 15 holds the runtime configuration.

The seed decides the buckets and never the order a program sees. A `Map` walks in insertion order, and a `HashMap` takes its order from another random number.

```anti
//! name: hashes
import anti.io;
import anti.collection.map.{HashMap};
import anti.collection.list.{List};

export struct Point
{
	x: int,
	y: int,
}

struct Name
{
	text: str,
	shown: str,
}

operator fn eq(a: Name, b: Name) -> bool
{
	return a.text == b.text;
}

operator fn hash(a: Name) -> u64
{
	return a.text.hash();
}

export fn point_hash(p: *Point) -> u64
{
	return p.hash();
}

fn main() -> int
{
	let p = Point { x: 3, y: 4 };
	let q = Point { x: 3, y: 4 };
	let same = p.hash() == q.hash();
	let tuple = (3, 4).hash() == p.hash();
	let line = f"equal points hash alike: {same}, a tuple of the same parts: {tuple}";
	io.println(line);
	free(line.ptr);
	line = f"7.hash() is {7.hash()}";
	io.println(line);
	free(line.ptr);

	let a = Name { text: "ann", shown: "Ann" };
	let b = Name { text: "ann", shown: "ANN" };
	let seen = HashMap<Name, int>.new();
	seen.set(a, 1);
	seen.set(b, 2);
	let found = seen.get(a) ?? 0;
	line = f"two names with one key: count {seen.count}, value {found}";
	io.println(line);
	free(line.ptr);

	let l = List<int>.new();
	l.push(1);
	l.push(2);
	let m = List<int>.new();
	m.push(1);
	m.push(2);
	line = f"equal lists hash alike: {l.hash() == m.hash()}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
equal points hash alike: true, a tuple of the same parts: true
7.hash() is 8360697188923789789
two names with one key: count 1, value 2
equal lists hash alike: true
```

The program prints the same number for `7.hash()` under every seed, since the seed enters in the collection and not in `hash`.

The checker refuses the following.

```anti refused
//! error: `operator fn hash` is written `operator fn hash(a: S) -> u64`
struct S
{
	n: int,
}

operator fn hash(a: S) -> int
{
	return a.n;
}
```

```anti refused
//! error: a class replaces `hash` of `Object` with `concrete fn hash(self) -> u64`
class K
{
	pub n: int = 0,

	operator fn hash(self) -> u64
	{
		return 1;
	}
}
```

### How antic builds it

antic writes the default hash inline, from the type. A scalar goes through the finalizer of MurmurHash3. It is a shift by 33 and an `xor`, a multiply, the same again with a second multiplier, and a last shift and `xor`. A value of parts starts at the offset basis of FNV-1a. It takes each part as the mix of the hash so far `xor` the hash of the part.

The hash of a `Point` of two `int` fields in a release build:

```llvm excerpt=hashes target=linux-x86_64 mode=release
define dso_local noundef i64 @point_hash(ptr
b0:
  %v1 = load i64, ptr %p0, align 8
  %v4 = lshr i64 %v1, 33
  %v7 = xor i64 %v4, %v1
  %v9 = mul i64 %v7, -49064778989728563
  %v12 = lshr i64 %v9, 33
  %v15 = xor i64 %v12, %v9
  %v17 = mul i64 %v15, -4265267296055464877
  %v20 = lshr i64 %v17, 33
  %0 = xor i64 %v17, %v20
  %v25 = xor i64 %0, -3750763034362895579
...
  %v51 = load i64, ptr %v49, align 8
...
  ret i64 %v98
```

- The first load reads `x`.
- The lines up to `%0` are one mix: shift, `xor`, multiply by `0xff51afd7ed558ccd`, shift, `xor`, multiply by `0xc4ceb9fe1a85ec53`, shift, `xor`. LLVM prints the two multipliers as signed numbers.
- The `xor` with -3750763034362895579 folds that hash into the start value `0xcbf29ce484222325`. A second mix follows.
- The second load reads `y`, which takes two more mixes.

The function is four mixes, eight multiplies, and no call. The two constants and the start value stand in `src/rt/hash.h`, which antic and the runtime both read. Text hashed by the runtime and a value hashed inline therefore follow one definition.

A `str` is hashed by `anti_rt_hash_bytes` of the runtime, which takes eight bytes at a time through the same mix.

The seed is one atomic word of the runtime. `anti_rt_init` fills it first, from `arc4random_buf`, `getrandom` or `rand_s` by the platform. A system that gives no random bytes gets a seed from its two clocks. A table computes `seeded(h, seed)`, which is the mix of `h` `xor` the mix of the seed, and sets the top bit.

Where it lives: `src/antic/sema_hash.c` and `src/antic/lower_hash.c` for the default, `src/rt/hash.h` and `src/rt/hash.c` for the constants, the bytes and the seed, and `hash_seed` and `seeded` of `src/std/anti/lang.anti`.

### What it costs

antic writes the default hash at the place of `x.hash()` in both modes, so the counts are the same in dev and in release.

| Cost | Count |
|---|---|
| The hash of a scalar | one mix: two multiplies, three shifts and three `xor`, no call |
| The hash of a struct of `n` scalar fields | `2n` mixes, no call |
| The hash of a `str` of `n` bytes | one call of `anti_rt_hash_bytes`, and two mixes per 8 bytes |
| The hash of a class value | one call of the `hash` of its class |
| The hash of a list of `n` elements | one walk, the hash of each element and one mix per element |
| The seed of a program | 8 bytes, drawn once at start |
| Reading the seed | one call of `anti_rt_hash_seed` with one atomic load, when a hashing collection is made |
| A lookup in a hashing collection | the hash of the key and two more mixes for the seed |
| Heap allocations | none |
| Locks | none |

No option turns the seed off. `--anti.hash_seed=<n>` fixes it and gives up the protection: the buckets of every table are then the same on every run.

### Pitfalls

For a Java programmer:

- A hash is 64 bits and unsigned. `x.hash()` is no `int`.
- A class overrides `hash` with `concrete fn hash`, and it overrides `equals` with it. One without the other gives two equal objects in two buckets.
- No program may rely on the order of a `HashMap`, not even inside one run.

For a C programmer:

- The hash of a struct reads its fields and never its padding. Two structs with equal fields hash alike.
- The hash of a `str` reads the bytes, and the hash of a `*byte` reads the address.

For a C++ programmer:

- A struct has a hash without a line of code. `std::hash` needs a specialization for it.
- `x.hash()` is not seeded. A program that stores hashes in a file may read them back in another run.
- A fixed seed in production gives every user of the program the same buckets.

## Order and equality

### What you know

Java compares references with `==` and values with `equals`, and orders with `Comparable` or a `Comparator`. C++ has `operator==` and `operator<` that the program writes, and since C++20 a defaulted `operator<=>`. C compares scalars, and `memcmp` compares bytes with the padding.

### The feature

`==` compares values. It has a default on a struct, a class value, a tuple, a variant, a `?T` and an array, which compares part by part, each part by its own `==`. `!=` is its negation. A struct replaces the default with `operator fn eq(a: S, b: S) -> bool` at module level.

`<` is built in for the numbers, `char` and `str`. A struct and a class have no default order. `operator fn lt(a: S, b: S) -> bool` at module level gives a type `<`, and `>`, `<=` and `>=` follow from it.

`Ordered` of `anti.lang` is the constraint `eq + lt`. `sort`, the sorted collections and `PriorityQueue` ask for it.

Every collection has `==` when its elements have it.

| Collection | Two values are equal when |
|---|---|
| `List`, `Deque`, `Ring`, `PriorityQueue`, `Pool`, `Tree` | they hold equal elements in the same order of their places |
| `Grid` | the widths and the heights are equal and the cells are |
| `Map`, `HashMap`, `Set`, `HashSet`, `BitSet` | they hold equal entries, whatever the order |
| `SortedMap`, `SortedSet` | they hold equal entries, compared in key order |

Both operands have one type. A `List<int>` and a `Deque<int>` with the same elements do not compare.

`sort` of a list is stable: elements that are equal under `<` keep their order.

```anti
//! name: orders
import anti.io;
import anti.collection.list.{List};
import anti.collection.map.{Map, HashMap};
import anti.collection.sorted.{SortedSet};

export struct Version
{
	major: int,
	minor: int,
}

operator fn lt(a: Version, b: Version) -> bool
{
	if a.major != b.major {
		return a.major < b.major;
	}
	return a.minor < b.minor;
}

fn newest<T: Ordered>(a: T, b: T) -> T
{
	if a < b {
		return dup(b);
	}
	return dup(a);
}

export fn same(a: *Version, b: *Version) -> bool
{
	return *a == *b;
}

export fn before(a: *Version, b: *Version) -> bool
{
	return *a < *b;
}

fn main() -> int
{
	let l = List<Version>.new();
	l.push(Version { major: 2, minor: 1 });
	l.push(Version { major: 1, minor: 9 });
	l.push(Version { major: 2, minor: 0 });
	l.sort();
	let low = l.first() ?? Version { major: 0, minor: 0 };
	let top = newest(l[0], l[2]);
	let line = f"lowest {low.major}.{low.minor}, newest {top.major}.{top.minor}";
	io.println(line);
	free(line.ptr);
	l.sort_by<Version, int>(fn(v) {
		return v.minor;
	});
	let t = "";
	for v in l {
		line = f"{t} {v.major}.{v.minor}";
		if t.len > 0 {
			free(t.ptr);
		}
		t = line;
	}
	line = f"by minor:{t}";
	io.println(line);
	free(line.ptr);
	free(t.ptr);

	let s = SortedSet<Version>.new();
	s.add(Version { major: 3, minor: 0 });
	s.add(Version { major: 1, minor: 2 });
	let twice = s.add(Version { major: 3, minor: 0 });
	line = f"set of {s.count}, add again {twice}";
	io.println(line);
	free(line.ptr);

	let a = Map<str, int>.new();
	a.set("x", 1);
	a.set("y", 2);
	let b = Map<str, int>.new();
	b.set("y", 2);
	b.set("x", 1);
	let ha = HashMap<str, int>.new();
	ha.set("x", 1);
	let hb = dup(ha);
	let copy = dup(l);
	line = f"maps in another order equal: {a == b}, a copy of a list: {copy == l}, of a hash map: {ha == hb}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
lowest 1.9, newest 2.1
by minor: 2.0 2.1 1.9
set of 2, add again false
maps in another order equal: true, a copy of a list: true, of a hash map: true
```

`Version` declares `lt` alone. Its `==` is the default, and the two together meet `Ordered`.

The checker refuses the following.

```anti refused
//! error: `<` is not defined on `S`. A struct or a class has no default order and declares `operator fn lt`
struct S
{
	n: int,
}

fn before(a: S, b: S) -> bool
{
	return a < b;
}
```

```anti refused
//! error: `==` is not defined on `S`, whose field `u` has no `eq`
union U
{
	a: int,
	b: f64,
}

struct S
{
	u: U,
}

fn same(a: S, b: S) -> bool
{
	return a == b;
}
```

A union has no default `==`, since nothing says which of its fields holds the value.

```anti refused
//! error: `==` is not defined on `[]int`
fn same(a: []int, b: []int) -> bool
{
	return a == b;
}
```

```anti refused
//! error: `<` needs numeric, `char` or `str` operands, found `(int, str)`
fn before(a: (int, str), b: (int, str)) -> bool
{
	return a < b;
}
```

```anti refused
//! error: expected `HashMap<int, int>`, found `Map<int, int>`
import anti.collection.map.{Map, HashMap};

fn same(m: *HashMap<int, int>, n: *Map<int, int>) -> bool
{
	return *m == *n;
}
```

### How antic builds it

The default `==` is code of the function that uses it. antic writes one compare per part and leaves at the first part that differs. `same` of the example in a release build:

```llvm excerpt=orders target=linux-x86_64 mode=release
define dso_local noundef zeroext range(i8 0, 2) i8 @same(ptr
b0:
  %v1 = load i64, ptr %p0, align 8
  %v3 = load i64, ptr %p1, align 8
  %v6 = icmp eq i64 %v1, %v3
...
  %v16 = load i64, ptr %v12, align 8
  %v18 = load i64, ptr %v14, align 8
  %v21 = icmp eq i64 %v16, %v18
```

- The first two loads and the compare are the field `major` of both values.
- The second pair of loads and the compare are `minor`, at offset 8. The function reaches them only when the first compare held.

`a < b` on a type with `operator fn lt` is a call of that function. A dev build of `before` holds it as a call:

```llvm excerpt=orders target=linux-x86_64 mode=dev
  %v8 = call range(i8 0, 2) i8 (i64, i64, i64, i64) @orders.lt(i64 %v1, i64 %v3, i64 %v5, i64 %v7)
```

The two structs of 16 bytes pass as four integers, by the calling convention of chapter 5. A release build inlines `lt` into `before`:

```llvm excerpt=orders target=linux-x86_64 mode=release
define dso_local noundef zeroext range(i8 0, 2) i8 @before(ptr
...
  %v8.not.i = icmp eq i64 %a0.sroa.0.0.copyload, %a1.sroa.0.0.copyload
  %v18.i = icmp slt i64 %a0.sroa.0.0.copyload, %a1.sroa.0.0.copyload
  %v31.i = icmp slt i64 %a0.sroa.2.0.copyload, %a1.sroa.2.0.copyload
  %common.ret.op.in.i = select i1 %v8.not.i, i1 %v31.i, i1 %v18.i
```

- The three compares are `major` equal, `major` less and `minor` less.
- The `select` picks the answer of `minor` when the two `major` fields are equal, and no branch is left.

`==` of two lists calls `equal` of `anti.collection`, which walks both by place and compares each pair of elements through a function value. `==` of two maps walks one map and looks each key up in the other.

`sort_with` sorts runs of 20 elements by insertion and merges runs that double in length. Each merge works in place by rotations, so the sort takes no second block.

Where it lives: `sema_equals` and `sema_default_eq` of `src/antic/sema_hash.c`, `src/antic/lower_eq.c`, and `Ordered` of `src/std/anti/lang.anti`. `equal` and `sort_stable` stand in `src/std/anti/collection.anti`, and the `operator fn eq` of each collection in its module.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| The default `==` of a struct of `n` scalar fields | up to `n` compares, inline | the same |
| `==` of a class value | one call of the `equals` of its class | the same, or inlined |
| `a < b` with `operator fn lt` | one call | one call, or inlined |
| `>`, `<=`, `>=` on such a type | one call of `lt` each | the same |
| `==` of two sequences of `n` elements | up to `n` compares, each through a function value | the same |
| `==` of two maps of `n` entries | up to `n` lookups in the second map | the same |
| `sort` of `n` elements | no allocation. A merge in place takes a factor of `log n` more moves than a merge into a second block | the same |
| Heap allocations, atomic operations and locks | none | none |

`==` and `<` insert no check in either mode.

### Pitfalls

For a Java programmer:

- `==` on two structs or two class values compares the fields. Two pointers compare as addresses.
- No `Comparator` is passed to a sorted collection. `sort_by` and `sort_with` of a list take the key or the comparison.
- A collection has `==` only when its elements do. A list of unions has none, and the checker says which part lacks it.

For a C programmer:

- The default `==` compares field by field and never the padding, so it differs from `memcmp`.
- Two `str` values compare by their bytes. Two slices of another type have no `==`.

For a C++ programmer:

- `operator fn lt` gives all four order operators. No `<=>` exists, and `==` stays the default unless `eq` is declared.
- A float field compares by `==` of floats. A struct that holds a NaN is not equal to itself, and `0.0` and `-0.0` are equal and hash alike.
- `sort` is stable and allocates nothing. It is slower by a factor of `log n` in its moves than a sort with a buffer.

## The cost of each operation

The table holds every plain collection of `anti.collection`. A count that holds on average over a run of operations is marked "amortized". `n` is the count of elements.

| Collection | Layout | Growth | Read | Add | Remove | Walk |
|---|---|---|---|---|---|---|
| `List<T>` | one block of `T` | doubles from 8 and copies the block | `l[i]`: one copy | `push`: 1 store, amortized. `insert(i)`: moves `n - i` | `pop`: none moved. `remove_at(i)`: moves `n - i` | in order |
| `Deque<T>` | one block, a ring | doubles from 8 and copies the ring | `front`, `back`: one copy | both ends: 1 store, amortized | both ends: none moved | from the front |
| `Ring<T, N>` | one block of `N` | none | `peek`: one copy | `push`: 1 store, or `false` when full | `pop`: none moved | from the oldest |
| `Grid<T>` | one block of `width * height + 1` | none | `g[x, y]`: one multiply, one copy | none | a cell takes the fill value | row by row |
| `Map<K, V>` | one block: entries, hashes, buckets | doubles from 8 and rebuilds | 1 hash, 1 probe | 1 hash, 1 probe, 1 append, amortized | marks the entry, none moved | insertion order |
| `HashMap<K, V>` | one block: marks, slots | doubles from 16 slots and rebuilds | 1 hash, 1 probe | 1 hash, 1 probe, amortized | marks the slot, none moved | a new random order each time |
| `Set<T>` | as `Map` | as `Map` | 1 hash, 1 probe | as `Map` | as `Map` | insertion order |
| `HashSet<T>` | as `HashMap` | as `HashMap` | 1 hash, 1 probe | as `HashMap` | as `HashMap` | a new random order each time |
| `BitSet` | one block of words | doubles its words | 1 shift, 1 load | 1 shift, 1 store | 1 shift, 1 store | increasing numbers |
| `SortedMap<K, V>` | one block of B-tree nodes of 11 | doubles its nodes | `log n` nodes, up to 11 compares each | the search, a move in the leaf, splits | the search, a move, merges | key order |
| `SortedSet<T>` | as `SortedMap` | as `SortedMap` | as `SortedMap` | as `SortedMap` | as `SortedMap` | key order |
| `Pool<T>` | blocks of 8, 16, 32 slots and on | adds a block, copies none | 4 compares, 1 slot address | 1 slot from the free list | frees the slot, none moved | slot order |
| `Tree<T>` | as `Pool`, with 5 links per slot | as `Pool` | as `Pool`. `parent`: 1 read | 1 slot, its links | the subtree, 1 slot per element | slot order |
| `PriorityQueue<T>` | one block, a binary heap | doubles from 8 and copies the block | `peek`: one copy | `push`: up to `log2 n` swaps | `pop`: up to `log2 n` swaps | array order |

The operations of `Collection<T>` cost the same walk on every collection.

| Operation | Work | Allocations |
|---|---|---|
| `is_empty`, `count` | one load | none |
| `find_first`, `update_first`, `remove_first` | a walk up to the first match, one call of `test` per element | none |
| `find_one`, `update_one`, `remove_one` | a walk of all elements, since a second match fails | none, or one `NotOne` error |
| `find_all` | a walk of all elements, and a copy of each match | the growth of a `Copies<T>` |
| `update_all` | a walk of all elements | none |
| `remove_all` | a walk of all elements. A `List` moves the tail per removal | none |
| `clear` | one removal per element. A `List` of `n` moves `n (n - 1) / 2` elements | none. The room stays until `shrink` |
| `to_text`, `serialize` | a walk, a copy of each element and one call of the runtime per element | the growth of the text |
| `dup` | a copy of each element | one block of the size of `count`, or the blocks of a pool |
| `==` | a walk of both, or a lookup per entry for a map and a set | none |
| `hash` | a walk and the hash of each element | none |

The one time of this chapter is that of `map_work` under "Map and HashMap". `tests/bench` holds no program for the other collections, so their costs stand as counts.
