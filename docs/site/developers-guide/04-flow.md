---
title: "Flow"
description: "if and while, for over ranges and collections, switch on values and on str, x in lo..hi, break and continue, defer and undo, and the paths that end a function."
summary: "`if` and `while`, `for` over a range and over a collection, `switch` on an integer, a `char`, a `str` and a variant, `x in lo..hi`, `break` and `continue`, `defer` and `undo`, and the paths that end a function, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-26T12:00:00+01:00
draft: true
weight: 40
tags: [compilers, programming-languages]
keywords: [control flow, if, while, for loop, switch, break, continue, defer, undo, missing return, noreturn]
---

A statement of this chapter becomes basic blocks and branches in the IR of antic, and nothing else. No statement here allocates or takes a lock. A `for` over a collection calls its iterator, and a `switch` on a `str` calls `anti.text.equal`. A dev build calls the runtime when a check fails.

An excerpt marked `ir` shows the IR of antic after lowering, which `antic --dump-ir` prints and [chapter 2]({{% relref "02-how-antic-works" %}}) explains. A block is `b0`, `b1` and on, `branch %c, b1, b2` goes to `b1` when `%c` is true, and `jump` goes unconditionally. Lowering writes the checks of dev mode into the IR in both modes, and a release build drops their blocks afterwards.

## if and while

### What you know

C and C++ take any scalar as a condition: zero and the null pointer are false, everything else is true. Java takes a `boolean` alone. All three write the condition in parentheses and make the braces optional. All three have the conditional expression `c ? a : b`, and in all three an assignment is an expression that a condition may hold.

### The feature

`if` takes a condition and a block. `else if` continues the chain and `else` ends it. The braces are part of the statement, and parentheses around the condition are optional.

A condition has type `bool`. No integer and no pointer converts to it, so a test of a number is written `n != 0` and a test of a pointer `p != none`. The one other type a condition takes is `?Match`, the match of a pattern, which is true when the pattern matched.

`&&` and `||` evaluate the left operand first and the right one only when the left does not decide. `!` negates a `bool`.

`while cond do { }` tests before each turn and runs its block zero or more times. `do { } while cond` runs its block once before the first test. No `;` follows the condition of a `do` loop, so the condition ends with its line. It goes on to the next line after an operator, `n < 3 &&`, and inside a bracket it opens. An operator, a call or an index that stands first on the next line starts the next statement instead.

`if` is a statement. Anti has no conditional expression and no `if` that gives a value. An assignment is a statement as well, so a condition cannot hold one.

A name declared in a block ends with the block. The condition of a `while` therefore reads names of the enclosing block alone.

```anti
//! name: branch
import anti.io;
import anti.text;

export fn sign(x: int) -> int
{
	if x > 0 {
		return 1;
	} else if x < 0 {
		return -1;
	} else {
		return 0;
	}
}

export fn odd_part(n: int) -> int
{
	while n > 1 && (n & 1) == 0 do {
		n = n >> 1;
	}
	return n;
}

export fn digits(n: int) -> int
{
	let count = 0;
	do {
		count += 1;
		n = n / 10;
	} while n != 0
	return count;
}

export fn length(p: ?*int) -> int
{
	if p != none && *p > 0 {
		return *p;
	}
	return 0;
}

fn say(line: str)
{
	io.println(line);
	free(line.ptr);
}

fn main() -> int
{
	say(f"sign {sign(7)} {sign(-7)} {sign(0)}");
	say(f"odd_part {odd_part(48)} {odd_part(7)}");
	say(f"digits {digits(0)} {digits(1234)}");
	let n = 12;
	say(f"length {length(&n)} {length(none)}");
	return 0;
}
```

```text output
sign 1 -1 0
odd_part 3 7
digits 1 4
length 12 0
```

A condition that compares a local or a parameter with `none` narrows that name. In `length` the parameter `p` has type `?*int`. Right of `p != none &&` and inside the block it has type `*int`, so `*p` compiles. A test `p == none` whose block leaves narrows `p` for the rest of the enclosing block. `&&` narrows what follows it, and `||` narrows nothing inside the block, since either side may have been the true one. An assignment to the name ends the narrowing. A field is never narrowed, because any call may write it. [Chapter 7]({{% relref "07-optionals" %}}) holds the full rule, with `if let` and `let ... else`, which bind the checked value to a new name.

A function with a result ends every path in a `return`, and the checker reads the form of the last statement to decide it. An `if` counts when it has an `else` and every branch leaves. "Paths that end a function" at the end of this chapter lists every form and every kind of function and handler.

The checker refuses each of the following.

```anti refused
//! error: a condition has type `bool`, found `int`
let n = 3;
if n {
	n = 0;
}
```

```anti refused
//! error: a condition has type `bool`, found `?*int`
fn length(p: ?*int) -> int
{
	if p {
		return 1;
	}
	return 0;
}
```

```anti refused
//! error: expected `{`
let n = 3;
if n > 0
	n = 0;
```

```anti refused
//! error: expected `do`
let n = 3;
while n > 0 {
	n -= 1;
}
```

```anti refused
//! error: `p` may be `none`, check it or use `?*T`
fn length(p: ?*int, always: bool) -> int
{
	if p != none || always {
		return *p;
	}
	return 0;
}
```

```anti refused
//! error: `n.next` may be `none`, check it or use `?*T`
struct Node { next: ?*Node, value: int }

fn second(n: *Node) -> int
{
	if n.next != none {
		return n.next.value;
	}
	return 0;
}
```

```anti refused
//! error: `forever` can reach its end without `return`
fn forever() -> int
{
	while true do {
		break;
	}
}
```

### How antic builds it

Lowering turns an `if` into one conditional branch per condition and one block per body. A loop is a block for the test, a block for the body and a jump back. `&&` and `||` are branches too: the right operand stands in a block that the left operand jumps over.

```ir excerpt=branch target=linux-x86_64 mode=dev
export fn branch.odd_part(%0: i64) -> i64 {
b0:
    jump b1
b1:
    %1 = sgt i8 %0, 1
    branch %1, b4, b3
...
b3:
    ret i64 %0
b4:
    %2 = and i64 %0, 1
    %3 = eq i8 %2, 0
    branch %3, b2, b3
...
b6:
    %7 = sshr i64 %0, 1
    %0 = copy i64 %7
    jump b1
}
```

`b1` is the test of the `while`. It holds the left operand of `&&`, `n > 1`, and leaves to `b3` when that is false. `b4` holds the right operand and runs only when the left one was true. `b6` is the body, which shifts, assigns `n` and jumps back to the test. `b3` is the exit, which both operands reach. The left-out blocks check the shift count, which [chapter 3]({{% relref "03-values" %}}) describes.

A dev build writes this IR into the LLVM module one instruction at a time and runs no optimizer. Every local and every temporary is a slot of the frame.

```llvm excerpt=branch target=linux-x86_64 mode=dev
define internal noundef i64 @sign(i64 noundef %p0) #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i8, align 1
  %t2 = alloca i8, align 1
  store i64 %p0, ptr %t0, align 8
  %v0 = load i64, ptr %t0, align 8
  %v1 = icmp sgt i64 %v0, 0
  %v2 = zext i1 %v1 to i8
  store i8 %v2, ptr %t1, align 1
  %v3 = load i8, ptr %t1, align 1
  %v4 = trunc i8 %v3 to i1
  br i1 %v4, label %b1, label %b2
```

The three `alloca` lines are the slots of `x` and of the two conditions. `icmp sgt` compares, `zext` widens the result to the `i8` that a `bool` is, and the `store` and `load` pass it through its slot. `trunc` gives the `i1` that `br` takes. A condition in a dev build is therefore one compare, one store, one load and one branch.

A release build runs `opt` over the module, which removes the slots and rewrites the branches.

```llvm excerpt=branch target=linux-x86_64 mode=release
define internal noundef range(i64 -1, 2) i64 @sign(i64 noundef %p0) #4 {
b0:
  %common.ret.op = tail call i64 @llvm.scmp.i64.i64(i64 %p0, i64 0)
  ret i64 %common.ret.op
}
```

The whole `if` chain of `sign` is one three-way compare, `llvm.scmp`, which gives -1, 0 or 1. The function holds no branch.

```llvm excerpt=branch target=linux-x86_64 mode=release
b0:
  %v1.not = icmp eq ptr %p0, null
  br i1 %v1.not, label %common.ret, label %b3
...
b3:                                               ; preds = %b0
  %v9 = load i64, ptr %p0, align 8, !tbaa !18
  %spec.select = tail call i64 @llvm.smax.i64(i64 %v9, i64 0)
  br label %common.ret
```

This is `length`. The test of `none` is a compare of the pointer with null. It stays a branch, because the `load` in `b3` must not run for a null pointer. The second condition and both `return` statements became `smax`, the larger of `*p` and 0.

```asm excerpt=branch target=linux-x86_64 mode=release
<length>:
testq %rdi, %rdi
je 0x15 <length+0x15>
movq (%rdi), %rax
movq %rax, %rcx
sarq $0x3f, %rcx
andnq %rax, %rcx, %rax
retq
xorl %eax, %eax
retq
```

`testq` and `je` are the test of `none`. The three instructions after the load compute the larger of the value and 0 without a branch. On arm64 the test of `none` is one `cbz` instruction.

Where it lives: `if_statement` and `statement` of `src/antic/parser_stmt.c` parse the forms. `check_condition`, `checked_against_none` and `stmt_leaves` of `src/antic/sema_stmt.c` check the condition, the narrowing and the missing `return`. `lower_if` and `lower_loop` of `src/antic/lower_stmt.c` write the blocks, and `src/antic/llvm_emit_function.c` writes each `branch` as a `br`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A condition | one compare, one store and one load of an `i8` slot, one conditional branch | one compare and one branch, or a branch-free form that `opt` chooses |
| `&&` and `||` | one conditional branch per operand | one branch per operand, or one `and` where both sides are safe to evaluate |
| A turn of a `while` | the condition and one jump back | the condition |
| Narrowing | nothing: it is a rule of the checker | nothing |
| Heap allocations | 0 | 0 |
| Calls into the runtime | 0 | 0 |
| Locks and atomic operations | 0 | 0 |
| Checks inserted | none | none |

An `if` or a loop inserts no check of its own. The checks inside `odd_part` and `digits` belong to `>>`, `/` and `+=`, and [chapter 3]({{% relref "03-values" %}}) gives their cost.

The forms that do the same job:

| Job | Form | Cost |
|---|---|---|
| a counted loop | `for i in lo..hi` | one compare per turn, no overflow at the end of the type, a trip count `opt` reads |
| a loop whose end the body decides | `while cond do { }` | the condition per turn |
| a body that runs once before the test | `do { } while cond` | the condition per turn, none before the first |
| one value against a list of constants | `switch` | in release, a table or a jump where `opt` finds one |
| a value against a range | `x in lo..hi` | two compares, one with constant bounds in release |

A `while` that counts with `i += 1` carries the overflow check of `+=` in a dev build. The `for` over the same range carries none, as its excerpt in the next section shows, so it is the form for a count.

### Pitfalls

For a Java programmer:

- The loop is written `while cond do { }`. A `while` without `do` is refused.
- Anti has no `c ? a : b`. An `if` with two assignments takes its place.
- A test of `none` narrows a local or a parameter. It does not narrow a field, so a field is copied into a local first.

For a C programmer:

- `if n` and `if p` are refused. The condition is a `bool`.
- `if x = 1` does not parse, since an assignment is a statement.
- The braces are required, so no `else` attaches to the wrong `if`.
- `do { } while cond` takes no `;` at its end.

For a C++ programmer:

- No class converts to `bool` in a condition. `explicit operator bool` has no counterpart.
- `if` holds no declaration of its own, as `if (auto x = f())` does. `if let` binds the value of an optional, and [chapter 7]({{% relref "07-optionals" %}}) describes it.
- A name narrowed by `p != none` is a plain `*T` in the block. No `*p` there carries a hidden check.

## for over ranges and collections

### What you know

C has the three-clause `for (init; test; step)`, and Java and C++ keep it. Java adds `for (T x : items)` over an array or an `Iterable`, which calls `iterator()`, `hasNext()` and `next()`. C++ adds `for (auto x : items)`, which calls `begin()` and `end()` and copies each element unless the variable is a reference.

### The feature

Anti has no three-clause `for`. A `for` walks a range or a sequence, and every other loop is a `while`.

A range is `lo..hi`. It starts at `lo` and stops before `hi`. Both bounds have one integer type, which is the type of the variable. Each bound is evaluated once, before the first turn. A range whose `lo` is not below `hi` runs no turn. `for lo..hi { }` names no variable and counts the turns.

`by k` sets the step, and `k` is a constant expression. `by -k` visits the values that `by k` visits, in reverse order: `0..10 by -3` gives 9, 6, 3 and 0. A range never computes a value past its ends, so a range that ends at the largest value of its type stops as any other does.

`for x in e` walks a slice, an array, a collection or an iterator, and `x` is a copy of each element. `for x in &e` gives `x` as a pointer to each element in its place, for one turn. `for i, x in e` gives the index and the element of a slice or an array. `for (k, v) in m` takes apart the tuple that each element of a collection is, such as the key and the value of a map.

The variable of a `for` is read-only and ends with the loop.

```anti
//! name: loops
import anti.io;
import anti.text;
import anti.collection;
import anti.collection.list.{List};

export fn sum_to(n: int) -> int
{
	let total = 0;
	for i in 0..n {
		total +%= i;
	}
	return total;
}

fn total_of(items: *List<int>) -> int
{
	let sum = 0;
	for x in items {
		sum +%= x;
	}
	return sum;
}

fn say(line: str)
{
	io.println(line);
	free(line.ptr);
}

fn main() -> int
{
	let b = text.Builder.new();
	for i in 0..10 by 3 {
		b.append_int(i);
		b.append(" ");
	}
	for i in 0..10 by -3 {
		b.append_int(i);
		b.append(" ");
	}
	for 0..2 {
		b.append("tick ");
	}
	say(b.take());

	let squares = [1, 4, 9];
	for p in &squares {
		*p += 1;
	}
	for i, x in squares {
		say(f"{i}: {x}");
	}

	let numbers = List<int>.new();
	numbers.push(3);
	numbers.push(4);
	say(f"sum_to {sum_to(5)}, total_of {total_of(&numbers)}");
	say(f"iterator {size_of(collection.IndexedWalk<int>)} bytes");
	return 0;
}
```

```text output
0 3 6 9 9 6 3 0 tick tick 
0: 2
1: 5
2: 10
sum_to 10, total_of 7
iterator 64 bytes
```

`List<int>` of the example is the growing sequence of `anti.collection`, which [chapter 16]({{% relref "16-collections" %}}) describes. A collection is any type with `operator fn iter(self)`, which gives a new iterator on each call. An iterator is any type with `operator fn next(self) -> bool` and `operator fn value(self) -> T`. `for x in c` calls `iter` once, then `next` before each turn and `value` at the head of each turn. `for x in it` walks an iterator in its place. A class of the program takes part by declaring the three hooks, and [chapter 9]({{% relref "09-classes" %}}) shows one.

`for x in e` copies each element, and the loop tears the copy down at the end of each turn, on every exit of the body. `for x in &e` copies nothing. Over a collection it binds what the `value` of the iterator returns, a `lent *T` for a list. [Chapter 11]({{% relref "11-ownership" %}}) describes lent pointers.

A collection must not change its size while a loop walks it. A dev build stops the program when it does, with a message that names the collection, the loop and the place of the change. `remove_all(test)` of a collection removes elements while it walks.

The checker refuses each of the following.

```anti refused
//! error: `by 0` never advances
let total = 0;
for i in 0..10 by 0 {
	total += i;
}
```

```anti refused
//! error: a variable is not a constant expression
fn sum(step: int) -> int
{
	let total = 0;
	for i in 0..10 by step {
		total += i;
	}
	return total;
}
```

```anti refused
//! error: `i` is the variable of a `for` and is read-only
for i in 0..10 {
	i = 5;
}
```

```anti refused
//! error: `x` is a copy of each element of `items`. Walk with `&items` to change the elements
let items = [1, 2, 3];
for x in items {
	x = 5;
}
```

```anti refused
//! error: a `for` range counts over an integer, found `float`
let turns = 0;
for t in 0.0..1.0 {
	turns += 1;
}
```

```anti refused
//! error: `for` walks a range, a slice, an array, a collection or an iterator, found `int`
let total = 0;
for x in 5 {
	total += x;
}
```

```anti refused
//! error: `for i, x` binds the index and the element of a slice or an array
let total = 0;
for i, x in 0..10 {
	total += i;
}
```

```anti refused
//! error: unknown name `i`
fn last() -> int
{
	for i in 0..3 { }
	return i;
}
```

### How antic builds it

A range loop is a counter, one test before the loop and one test per turn.

```ir excerpt=loops target=linux-x86_64 mode=dev
export fn loops.sum_to(%0: i64) -> i64 {
b0:
    %1 = copy i64 0
    %2 = copy i64 0
    %3 = copy i64 %0
    %4 = copy i64 %2
    %5 = slt i8 %2, %3
    branch %5, b1, b3
b1:
    %6 = copy i64 %4
    %7 = add i64 %1, %6
...
b2:
    %8 = sub i64 %3, %4
    %9 = ule i8 %8, 1
    branch %9, b3, b4
...
b4:
    %10 = add nuw nsw i64 %4, 1
```

`b0` copies the two bounds into `%2` and `%3`, so that the body cannot change them, and starts the counter `%4`. `slt` is the test of an empty range, the one compare by the sign of the type. `b1` is the body: `%6` is `i`, a copy of the counter. `b2` is the step. It measures the distance to the end, `hi - counter`, and leaves when the distance is at most the step. Only then does `b4` move the counter. The `add` carries `nuw nsw`, because the moved counter lies inside the range and cannot overflow. `continue` jumps to `b2`.

The step never produces a value past `hi`. A loop written `i < hi; i += k` wraps when `hi` is near the end of the type. The distance test stops before that. The counter needs no overflow check in dev mode for the same reason.

In a release build `opt` knows the trip count of this loop, and here it replaces the loop by its closed form.

```asm excerpt=loops target=linux-x86_64 mode=release
<sum_to>:
testq %rdi, %rdi
jle 0x1f <sum_to+0x1f>
leaq -0x1(%rdi), %rdx
leaq -0x2(%rdi), %rax
mulxq %rax, %rcx, %rax
shrdq $0x1, %rax, %rcx
leaq (%rdi,%rcx), %rax
decq %rax
retq
xorl %eax, %eax
retq
```

`testq` and `jle` are the test of an empty range. The rest multiplies `n - 1` by `n - 2`, halves the product and adds `n - 1`, which is the sum of 0 to `n - 1`. No instruction of the loop is left.

A loop over a slice or an array reads the pointer and the length once and indexes with a counter. It has no bounds check, since the counter stays below the length it was read from. Each turn copies one element.

A loop over a collection is the three calls.

```ir excerpt=loops target=linux-x86_64 mode=release
fn loops.total_of(%0: ptr nonnull deref(size_of anti.collection.list.List<int>)) -> i64 {
b0:
    %2 = slot anti.collection.IndexedWalk<int>
...
    memcopy %2, %7, anti.collection.IndexedWalk<int>
    jump b1
b1:
...
    %14 = ne i8 %10, %13
    branch %14, b4, b5
b2:
    %31 = call ptr @anti.collection.IndexedWalk<int>.value(%2)
    %32 = load i64 %31 !type i64
...
    call void @anti_rt_walk_changed(%15, 56, %19, %24, %29)
    unreachable
b5:
    %30 = call i8 @anti.collection.IndexedWalk<int>.next(%2)
    branch %30, b2, b3
```

`%2` is the iterator, a slot of the frame of 64 bytes. `b0` calls `iter` through the table of the list, since `items` is a pointer, and copies its result into the slot. A dev build also tests the table pointer before that call, which [chapter 9]({{% relref "09-classes" %}}) describes. `b1` is the test of each turn. It loads the pointer to the `Changes` of the list and compares two counts. One is the count of changes now, and the other the count when the walk began. When they differ, the block `b4` calls `anti_rt_walk_changed`, which prints the message and ends the program. `b5` calls `next`, and `b2` calls `value` and loads the element through the pointer it returns.

The iterator of a list holds a `Watch`, which is a pointer to the `Changes` of the list and the count at the start. "Walking a collection" of [chapter 16]({{% relref "16-collections" %}}) describes both, and what every change of a collection stores. An iterator without a field of type `Watch` gets no such test.

A release build drops `b4` and the compare before it. `next` and `value` are ordinary functions of the module, and `opt` inlines them where its threshold allows.

Where it lives: `check_for`, `check_step` and the read-only rule of `src/antic/sema_stmt.c`, and `sema_iterate` of `src/antic/sema_expr.c` for the hooks. `lower_for`, `range_start` and `range_step` of `src/antic/lower_stmt.c` write a range and a slice loop, and `lower_for_hooks` and `walk_check` a collection loop. `anti_rt_walk_changed` is in `src/rt/check.c`. `Changes` and `Watch` are in `src/std/anti/lang.anti`, and `IndexedWalk` in `src/std/anti/collection.anti`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A range loop, before the first turn | two copies of the bounds, one compare | one compare |
| A range loop, per turn | one subtract, one compare, one add | at most that, and none where `opt` finds a closed form or vectorizes |
| A slice or array loop, per turn | one compare, one add, one copy of the element | the same or less |
| A collection loop, before the first turn | one call of `iter`, one copy of the iterator into the frame | the same, inlined where `opt` chooses |
| A collection loop, per turn | three loads and one compare of the change counts, one call of `next`, one call of `value`, one copy of the element | `next` and `value`, with no compare of the counts |
| `for x in &e`, per turn | no copy of the element | no copy |
| Heap allocations | 0 | 0 |
| Locks and atomic operations | 0 | 0 |
| Checks inserted | the change check of a collection loop | none |

The iterator lives in the frame of the function that holds the loop. For a `List<int>` it is the `IndexedWalk<int>` of 64 bytes that the last line of the program prints.

`--no-checks` drops the change check from a dev build, and `--checks` keeps it in a release build. Without it, a loop over a list that grows in the body reads through a pointer into memory the list has freed.

The copy of `for x in e` has the size of the element. For an `int` it is one load. For a struct of 200 bytes it is 200 bytes per turn, and `for x in &e` avoids it.

### Pitfalls

For a Java programmer:

- `0..10` stops before 10, and `by -1` walks the same ten values down from 9. It does not start at 10.
- A change of a collection inside its loop stops a dev build, as `ConcurrentModificationException` does. A release build carries no such test.
- `for x in items` gives a copy of a struct. A change of the copy is refused, and `for x in &items` reaches the element.

For a C programmer:

- The bounds are evaluated once. A body that changes `n` does not change the number of turns of `for i in 0..n`.
- The variable cannot be assigned, so a turn is skipped with `continue` and never with `i += 1`.
- A range from 10 down to 0 is `0..10 by -1`. `10..0` runs no turn.
- The variable does not exist after the loop. A search keeps its result in a variable declared before the loop.

For a C++ programmer:

- `for x in items` copies, as `for (auto x : items)` does. `for x in &items` is `for (auto& x : items)`.
- An iterator is one object with `next` and `value`. No pair of `begin` and `end` exists, and no iterator is compared with another.
- An iterator is not invalidated without notice in a dev build. The program stops at the next turn.

## switch

### What you know

`switch` in C, C++ and Java jumps to the `case` whose constant equals the value. Execution runs on into the next `case` unless a `break` ends it. C and C++ switch on integers and enums. Java also switches on strings and, in its arrow form, runs one arm without fallthrough.

### The feature

`switch` takes a value of an integer type, an enum, a `char`, a `str` or a variant. Each arm names one value, then `=>`, then a body. A comma separates the arms. One arm runs, and none falls into the next unless it says so.

The body of an arm is a call, an assignment or a block. A call is any call that stands as a statement: `1 => free(p)`, and a call that may fail with its handler, `1 => save(n) catch { }`. Anything else, such as a `return`, stands in a block.

The value of an arm is a constant expression of the type of the `switch`, and an enum value is written with its type, `Kind.Circle`. Each value has one arm. The checker compares the values, not their spelling, so an arm `size_of(i64)` and an arm `8` name one value. Two values of an enum may share a number, and a `switch` names at most one of them. That arm covers both. `else` takes every value that no arm names, and a `switch` has at most one.

A `switch` on an enum without `else` names every value of the enum. A `switch` on a variant without `else` names every case. A `switch` on an integer, a `char` or a `str` without `else` does nothing when no arm matches. A `switch` on a `char` takes `char` literals as its arms, `'a' =>` or `'\n' =>`, and compares the scalar values as integers. A `bool` and a value of a type parameter are refused.

`fallthrough;` as the last statement of a block arm enters the body of the next arm without testing its value.

An arm of a `switch` on a variant names a case. `Circle c =>` binds `c` to a copy of the fields of the case, and `Empty =>` binds nothing. [Chapter 8]({{% relref "08-structs-enums" %}}) describes variants.

```anti
//! name: arms
import anti.io;
import anti.text;

enum Kind { Circle, Square, Rect }

variant Shape { Circle { r: int }, Box { w: int, h: int }, Empty }

export fn days(month: int) -> int
{
	let n = 31;
	switch month {
		2 => n = 28,
		4 => n = 30,
		6 => n = 30,
		9 => n = 30,
		11 => n = 30,
	}
	return n;
}

export fn in_month(n: int) -> bool
{
	return n in 1..13;
}

fn level(name: str) -> int
{
	switch name {
		"debug" => { return 0; },
		"info" => { return 1; },
		"warn" => { return 2; },
	}
	return -1;
}

fn area(s: Shape) -> int
{
	let a = 0;
	switch s {
		Circle c => a = 3 * c.r * c.r,
		Box b => a = b.w * b.h,
		Empty => { },
	}
	return a;
}

fn corners(k: Kind) -> int
{
	let n = 0;
	switch k {
		Kind.Circle => { },
		Kind.Square => {
			fallthrough;
		},
		Kind.Rect => n = 4,
	}
	return n;
}

fn say(line: str)
{
	io.println(line);
	free(line.ptr);
}

fn main() -> int
{
	say(f"days {days(2)} {days(4)} {days(12)}");
	say(f"in_month {in_month(12)} {in_month(13)}");
	let info = level("info");
	let trace = level("trace");
	say(f"level {info} {trace}");
	say(f"area {area(Shape.Circle { r: 2 })} {area(Shape.Box { w: 2, h: 5 })} {area(Shape.Empty)}");
	say(f"corners {corners(Kind.Circle)} {corners(Kind.Square)} {corners(Kind.Rect)}");
	return 0;
}
```

```text output
days 28 30 31
in_month true false
level 1 -1
area 12 10 0
corners 0 4 4
```

`x in lo..hi` is an expression of type `bool`, written for `x >= lo && x < hi`, so the range excludes `hi` as the range of a `for` does. It computes `x` once, then `lo`, and `hi` only when `x` reaches `lo`, as the `&&` does. The three have one type, which the first of them that is no literal names: an integer, a `char`, a float, or a type with an `operator fn lt`, which both comparisons call. With three constant operands the expression is a constant. The right side is a range and takes no `by`, and `in` without a range is refused. `in` binds as `<` does, so `i + 1 in 0..n * 2` needs no parentheses. The expression is no value of an arm, so a `switch` has no arm for a range of values, and an `if` chain of `in` tests takes its place.

The checker refuses each of the following.

```anti refused
//! error: `in` takes a range
fn has(x: int, y: int) -> bool
{
	return x in y;
}
```

```anti refused
//! error: `switch` takes an enum, an integer, a `char`, a `str` or a variant, found `float`
let x = 1.5;
switch x {
	1.5 => x = 0.0,
}
```

```anti refused
//! error: `switch` takes an enum, an integer, a `char`, a `str` or a variant, found `bool`
let b = true;
let n = 0;
switch b {
	true => n = 1,
}
```

```anti refused
//! error: a variable is not a constant expression
let x = 1;
let y = 1;
switch x {
	y => x = 0,
}
```

```anti refused
//! error: this value already has an arm
let x = 1;
switch x {
	1 => x = 0,
	1 => x = 2,
}
```

```anti refused
//! error: this value already has an arm
let n = 8;
let r = 0;
switch n {
	size_of(i64) => r = 1,
	8 => r = 2,
}
```

```anti refused
//! error: this value already has an arm
enum Code { Ok = 0, Fine = 0, Bad = 1 }

fn rank(c: Code) -> int
{
	let r = 0;
	switch c {
		Code.Ok => r = 1,
		Code.Fine => r = 2,
		Code.Bad => r = 3,
	}
	return r;
}
```

```anti refused
//! error: this `switch` on `Kind` has no arm for `Rect`
enum Kind { Circle, Square, Rect }

fn corners(k: Kind) -> int
{
	let n = 0;
	switch k {
		Kind.Circle => n = 0,
		Kind.Square => n = 4,
	}
	return n;
}
```

```anti refused
//! error: an arm of a `switch` is a call, an assignment or a block
let x = 1;
switch x {
	1 => x + 1,
}
```

```anti refused
//! error: `fallthrough` in the last arm
let x = 1;
switch x {
	1 => { fallthrough; },
}
```

```anti refused
//! error: `fallthrough` is allowed as the last statement of a `switch` arm only
let x = 1;
switch x {
	1 => {
		fallthrough;
		x = 3;
	},
	2 => { },
}
```

```anti refused
//! error: a `switch` has one `else`
let x = 1;
switch x {
	else => x = 2,
	else => x = 3,
}
```

`fallthrough` into an arm that binds the fields of a variant case is refused as well, since the fields of that case are not there.

### How antic builds it

Lowering writes a `switch` as a chain of compares in the order of the arms. The LLVM text that antic writes holds no `switch` instruction and no jump table, in either mode.

```ir excerpt=arms target=linux-x86_64 mode=dev
export fn arms.days(%0: i64) -> i64 {
b0:
    %1 = copy i64 31
    %2 = copy i64 %0
    %3 = eq i8 %2, 2
    branch %3, b1, b2
b1:
    %1 = copy i64 28
    jump b3
b2:
    %4 = eq i8 %2, 4
    branch %4, b4, b5
b3:
    ret i64 %1
b4:
    %1 = copy i64 30
    jump b3
```

`%2` is a copy of the value, taken once. `b0` compares it with 2, the value of the first arm, and `b1` is that arm's body, which jumps to the join block `b3`. `b2` is the test of the second arm. A value that matches the last arm passes through every compare before it. A dev build runs this chain as written.

In a release build `opt` reads the chain and chooses another form. For `days` it is a table.

```llvm excerpt=arms target=linux-x86_64 mode=release
@switch.table.days = private unnamed_addr constant [10 x i8] c"\1C\1F\1E\1F\1E\1F\1F\1E\1F\1E", align 8
...
b0:
  %switch.tableidx = add i64 %p0, -2
  %0 = icmp ult i64 %switch.tableidx, 10
  br i1 %0, label %switch.lookup, label %b3
...
switch.lookup:                                    ; preds = %b0
  %switch.gep = getelementptr inbounds nuw i8, ptr @switch.table.days, i64 %switch.tableidx
  %switch.load = load i8, ptr %switch.gep, align 1
  %switch.ext = zext i8 %switch.load to i64
```

The table holds one byte per month from 2 to 11: `\1C` is 28, `\1E` is 30 and `\1F` is 31. `b0` subtracts 2 and makes one unsigned compare, which tests both ends of the table. `switch.lookup` loads the byte. The five arms became one compare, one branch and one load, whatever the value.

```asm excerpt=arms target=linux-x86_64 mode=release
<days>:
addq $-0x2, %rdi
movl $0x1f, %eax
cmpq $0x9, %rdi
ja 0x1a <days+0x1a>
leaq (%rip), %rax
movzbl (%rdi,%rax), %eax
retq
```

`movl $0x1f` loads 31, the value when no arm matches. `ja` skips the lookup for a month outside 2 to 11.

A `switch` on a `str` binds the text once and calls `anti.text.equal` for each arm in turn.

```ir excerpt=arms target=linux-x86_64 mode=dev
fn arms.level(%0: agg str) -> i64 {
b0:
    %2 = slot str
...
    %1 = copy ptr %0
    %3 = addr @arms.0
    store ptr %3, %2
    %4 = ptradd %2, offset_of str.len
    store i64 5, %4
    %5 = call i8 @anti.text.equal(%1, %2)
    branch %5, b1, b2
b1:
    ret i64 0
b2:
```

`%2` is a `str` in the frame for the literal `"debug"`: the address of its bytes and the length 5. The call compares it with the text of the `switch`. `equal` compares the lengths first and the bytes only when the lengths agree. The arms are not hashed and not sorted, so a text that matches no arm costs one call per arm.

`x in lo..hi` is the two compares of its `&&`, a branch between them in a dev build. With constant bounds a release build folds them into one.

```llvm excerpt=arms target=linux-x86_64 mode=release
define internal noundef zeroext range(i8 0, 2) i8 @in_month(i64 noundef %p0)
b0:
  %0 = add i64 %p0, -1
  %spec.select = icmp ult i64 %0, 12
```

`add` moves the range 1 to 12 down to 0 to 11, and the one unsigned compare `ult` with 12 tests both ends: a value below 1 becomes a large unsigned number. The object holds `decq`, `cmpq $12` and `setb`, with no branch.

A `switch` on a variant loads the tag once and compares it with the number of each arm's case. An arm that binds copies the fields of its case into a slot of the frame first. "Pattern matching" of [chapter 8]({{% relref "08-structs-enums" %}}) shows that code.

Where it lives: the `TOKEN_SWITCH` case of `statement` and `arm_body` of `src/antic/parser_stmt.c`. `check_switch`, `check_switch_covers` and `refuse_same_value` of `src/antic/sema_stmt.c`. `lower_switch`, `arm_test` and `bind_case_fields` of `src/antic/lower_stmt.c`. `equal` is in `src/std/anti/text.anti`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| An integer, `char` or enum `switch` of n arms | up to n compares and n branches, in the order of the arms | what `opt` chooses: a lookup table, a jump table or a shorter chain |
| A `str` `switch` of n arms | up to n calls of `anti.text.equal`, and two stores per call | the same calls, inlined where `opt` chooses |
| A variant `switch` of n arms | one load of the tag, up to n compares | one load, and what `opt` chooses |
| An arm that binds | one copy of the fields of the case | the copy, where `opt` cannot remove it |
| `fallthrough` | one jump | none |
| Heap allocations | 0 | 0 |
| Calls into the runtime | 0 | 0 |
| Checks inserted | none | none |

The completeness of a `switch` on an enum or a variant is a rule of the checker. No arm for an impossible value and no trap is added at run time.

The order of the arms is the order of the tests in a dev build. In a release build the order does not decide the code of an integer `switch`.

| Cost of `x in lo..hi` | Dev | Release |
|---|---|---|
| Compares | two, with a branch between them | two, or one unsigned compare for constant bounds |
| Calls | none, or two calls of `operator fn lt` for a class | the same |

The choice among the forms that pick one path:

- A `switch` on an integer, a `char` or an enum of many arms is the cheaper form in a release build, where `opt` may make it a table. A chain of `if` tests the same values in the order written.
- A `switch` on an enum or a variant without `else` lets a release build drop the test of the last arm, as the last section shows. The checker also refuses it when a new value or case has no arm.
- A `switch` on a `str` of n arms costs up to n calls of `equal`. A text matched against many values is a lookup in a `HashMap<str, V>` of [chapter 16]({{% relref "16-collections" %}}), which hashes the text once.
- A test of a range of values is an `in` in an `if` chain, since an arm holds one value.

### Pitfalls

For a Java programmer:

- An arm holds one value. `case 4, 6, 9, 11 ->` is four arms, or one `if` with `||`.
- A `switch` gives no value. An arm assigns a variable declared before it.
- A `switch` on a `str` compares arm by arm. It does not hash the text first.

For a C programmer:

- An arm does not fall into the next one. No `break` ends it, and `fallthrough;` asks for the old behaviour.
- `break` inside an arm leaves the enclosing loop, as the next section shows.
- A `switch` on a `char` takes `'a'` as an arm and refuses the number `97`. The arms are compared as integers all the same.
- A `switch` on an enum that misses a value is an error, not a warning.

For a C++ programmer:

- A `return` in every arm ends the function for the checker when the `switch` has an `else`, or names every value of an enum or a variant. A `switch` on an integer without `else` goes on for the other values, so a `return` after it is required.
- `Circle c =>` copies the fields. Writing `c.r` changes the copy.
- A new case of a variant, or a new value of an enum, breaks every `switch` without `else` at compile time.

## break and continue

### What you know

`break` leaves the innermost loop or `switch` in C, C++ and Java, and `continue` starts the next turn of the innermost loop. Java labels a loop to leave an outer one, and C and C++ use `goto` for that.

### The feature

`break;` leaves the innermost loop: a `while`, a `do` or a `for`. `continue;` ends the current turn. In a `while` and a `do` loop it goes to the condition. In a `for` it goes to the step, so the loop advances.

A `switch` is not a target of `break`. Inside an arm, `break` and `continue` act on the loop around the `switch`.

Both run the `defer` statements of every block they leave, innermost first, and tear down the locals of those blocks. The next section describes `defer`.

Anti has no labels and no `goto`. An outer loop is left through a flag that its condition reads, or by a `return` from a function that holds both loops.

```anti
//! name: jumps
import anti.io;
import anti.text;

export fn first_negative(p: *int, n: int) -> int
{
	let at = -1;
	for i in 0..n {
		if p[i] >= 0 {
			continue;
		}
		at = i;
		break;
	}
	return at;
}

fn say(line: str)
{
	io.println(line);
	free(line.ptr);
}

fn main() -> int
{
	let items = [4, 0, 7, -2, 9];
	say(f"first negative at {first_negative(&items[0], 5)}");

	let count = 0;
	for x in items {
		switch x {
			0 => { continue; },
			7 => { break; },
			else => count += 1,
		}
	}
	say(f"count {count}");

	let i = 0;
	while true do {
		i += 1;
		defer say(f"left turn {i}");
		if i == 1 {
			continue;
		}
		if i == 2 {
			break;
		}
	}
	return 0;
}
```

```text output
first negative at 3
count 1
left turn 1
left turn 2
```

The second loop counts 4, skips 0 and leaves at 7, so `count` is 1. In the third loop the `defer` runs at the `continue` of the first turn and at the `break` of the second.

The checker refuses each of the following.

```anti refused
//! error: `break` outside a loop
let n = 3;
if n > 0 {
	break;
}
```

```anti refused
//! error: `break` outside a loop
let x = 1;
switch x {
	1 => { break; },
}
```

```anti refused
//! error: `continue` outside a loop
let n = 3;
if n > 0 {
	continue;
}
```

Lowering skips every statement that follows a `break`, a `continue` or a `return` in its block, since nothing can reach it.

### How antic builds it

Each of the two is one jump. Before the jump stand the deferred statements of the blocks it leaves, written out again at that place. The example calls `note`, an `extern fn`. That is a function of C which the module declares and does not define, so no pass removes a call of it. [Chapter 17]({{% relref "17-c-interop" %}}) describes the form.

```anti
//! name: scan
extern fn note(n: c_int);

export fn scan(p: *int, n: int)
{
	for i in 0..n {
		defer note(2);
		if p[i] == 0 {
			continue;
		}
		if p[i] < 0 {
			break;
		}
		note(1);
	}
}
```

```ir excerpt=scan target=linux-x86_64 mode=dev
b2:
    %15 = sub i64 %3, %4
    %16 = ule i8 %15, 1
    branch %16, b3, b8
b3:
    ret
b4:
    call void @note(2)
    jump b2
...
b6:
    call void @note(2)
    jump b3
b7:
    call void @note(1)
    call void @note(2)
    jump b2
```

`b2` is the step of the `for` and `b3` the exit of the loop. `b4` is the `continue`: the deferred `note(2)` and a jump to the step. `b6` is the `break`: the deferred call again and a jump to the exit. `b7` is the end of the body, with the third copy of the deferred call. The function holds `note(2)` three times, once per exit of the block.

A release build leaves it to `opt` to merge the copies. In the object of this example the `continue` and the end of the body share one call, and the `break` keeps its own.

```asm excerpt=scan target=linux-x86_64 mode=release
<scan>:
...
movl $0x2, %edi
callq 0x2a <scan+0x2a>
incq %r15
...
movl $0x1, %edi
callq 0x45 <scan+0x45>
jmp 0x20 <scan+0x20>
...
movl $0x2, %edi
...
jmp 0x66 <scan+0x66>
```

The first `movl $0x2, %edi` stands at offset `0x20`, and it and its `callq` are the deferred call of both the `continue` and the end of the body. The `jmp 0x20` after `note(1)` goes back to it. The second `movl $0x2, %edi` is the copy of the `break`. It leaves through the last `jmp`, a tail call of `note`.

Where it lives: the `STMT_BREAK` and `STMT_CONTINUE` cases of `check_stmt` in `src/antic/sema_stmt.c` refuse one outside a loop. The same cases of `lower_stmt_kind` in `src/antic/lower_stmt.c` call `lower_run_defers_to` and write the jump, and `lower_block` skips the statements after it.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| `break` | one jump | one jump or none |
| `continue` | one jump | one jump or none |
| Deferred statements on the way | each runs once, from a copy of its code at this exit | the same, and `opt` may merge copies |
| Heap allocations | 0 | 0 |
| Calls into the runtime | 0 | 0 |
| Checks inserted | none | none |

The code size grows with the exits. A block with d deferred statements and e exits holds d times e copies in the IR.

The forms that leave more than one loop:

- A flag that the outer condition reads costs one store where it is set. A dev build adds one load and one compare per outer turn.
- A function that holds both loops and leaves with `return` costs a call where `opt` does not inline it, and no flag. The function also gives the search result a name.

### Pitfalls

For a Java programmer:

- No label names a loop. `break outer;` has no counterpart, and a function with a `return` takes its place.
- `break` in a `switch` arm leaves the loop around it. A Java `break` there leaves the `switch` alone.

For a C programmer:

- A `switch` arm needs no `break`, and one written from habit ends the loop.
- `continue` in a `for` runs the step, as in C. In a `while` the body advances its own counter before it.
- No `goto` exists. Cleanup at one label is written with `defer`.

For a C++ programmer:

- A local of a class type is destroyed at `break` and `continue`, as a destructor runs there in C++.
- The deferred code is copied to each exit. It is not one function called from each.

## defer and undo

### What you know

C frees what a function took at the end, often behind a `goto cleanup` label. C++ ties the release to a destructor, which runs at every exit of the scope. Java writes it in `finally` or opens the resource in `try (...)`. None of the three separates the release that always runs from the one that runs after a failure alone.

### The feature

`defer stmt;` records one statement. It runs at every exit of the block that holds the `defer`. The exits are the closing brace, a `return`, a `break`, a `continue` and an error that leaves the block. The statements of one block run in the reverse order of their declaration.

`undo stmt;` records one statement that runs only when the block is left by an error: a `fail`, or a `try` that forwards one. On that exit every `undo` of the block runs, last declared first, and after them every `defer` of the block.

[Chapter 6]({{% relref "06-errors" %}}) describes errors. This section needs three of its forms. A function marked `may fail` can end with an error, which `fail` raises. `try` before a call passes the error of that call on to the caller. `catch { }` after a call takes the error and goes on.

Both take effect where they stand. An exit above a `defer` does not run it. A deferred statement reads its variables when it runs, not when it is declared.

The statement is a call, an assignment or a block. A call that may fail carries its handler, `defer fs.close(f) catch fatal;`.

A local of a class type with a `destruct` is torn down at the end of its block. It behaves as a `defer` written after its declaration. [Chapter 11]({{% relref "11-ownership" %}}) describes that order.

```anti
//! name: exits
import anti.io;
import anti.text;

fn step(name: str, ok: bool) may fail
{
	io.println(name);
	if !ok {
		fail "stopped";
	}
}

fn build(ok: bool) may fail
{
	io.println("open a");
	defer io.println("defer: close a");
	undo io.println("undo: remove a");
	io.println("open b");
	defer io.println("defer: close b");
	undo io.println("undo: remove b");
	try step("write", ok);
	io.println("done");
}

fn order() -> int
{
	let n = 1;
	defer {
		let line = f"deferred, n is {n}";
		io.println(line);
		free(line.ptr);
	}
	{
		defer io.println("inner block ends");
		n = 2;
	}
	n = 3;
	return n;
}

fn main() -> int
{
	build(true) catch { };
	io.println("--");
	build(false) catch { };
	io.println("--");
	let n = order();
	let line = f"order gave {n}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
open a
open b
write
done
defer: close b
defer: close a
--
open a
open b
write
undo: remove b
undo: remove a
defer: close b
defer: close a
--
inner block ends
deferred, n is 3
order gave 3
```

The first call of `build` succeeds and runs the two `defer` statements. The second fails in `step`. Both `undo` statements run before either `defer`. In `order` the deferred block prints 3, the value of `n` at the `return`, and the `defer` of the inner block runs at that block's brace.

`return v;` with a result that fits a register, such as an `int` or a pointer, computes `v` before the deferred statements run. A deferred statement that assigns the local afterwards does not change the result. A result that is a struct, a tuple or a `str` and names a local is read from that local after the deferred statements ran.

```anti
//! name: results
import anti.io;
import anti.text;

struct Pair { a: int, b: int }

fn scalar() -> int
{
	let n = 1;
	defer n = 99;
	return n;
}

fn aggregate() -> Pair
{
	let p = Pair { a: 1, b: 2 };
	defer p.a = 99;
	return p;
}

fn main() -> int
{
	let p = aggregate();
	let line = f"scalar {scalar()}, aggregate {p.a}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
scalar 1, aggregate 99
```

The parser refuses a `defer` directly in a `defer` and an `undo` directly in an `undo`.

```anti refused
//! error: a `defer` holds one statement, not another `defer`
let n = 0;
defer defer n = 1;
```

```anti refused
//! error: an `undo` holds one statement, not another `undo`
let n = 0;
undo undo n = 1;
```

```anti refused
//! error: `step` may fail and its error is not handled
fn step() may fail
{
	fail "stopped";
}

fn build() -> int
{
	defer step();
	return 0;
}
```

A deferred statement runs on the way out of its block, so nothing inside it may leave it. The checker refuses `return`, `break`, `continue`, `fail`, `try` and `yield` inside a `defer` or an `undo`. A `break` or a `continue` of a loop that stands inside the same statement stays allowed.

```anti refused
//! error: `return` inside a `defer`
fn run(n: int) -> int
{
	defer return n + 1;
	return n;
}
```

An `undo` runs on a failing exit, so it belongs in a function that can fail. In any other function it never runs, and the checker gives the warning `undo-never-runs`. A release build refuses every warning, as [chapter 6]({{% relref "06-errors" %}}) describes, so the block below fails there.

```anti refused
//! error: an `undo` in a function that cannot fail never runs [undo-never-runs]
extern fn note(n: c_int);

fn plain()
{
	undo note(1);
	note(2);
}
```

An `undo` inside a `defer` or inside another `undo` is refused. The statement around it runs on the way out of its block, after the failure that an `undo` waits for, so the inner `undo` would never run. A `defer` inside an `undo` is allowed and runs at the end of the block of the `undo`.

```anti refused
//! error: `undo` inside a `defer`
extern fn note(n: c_int);

fn risky() may fail
{
	fail "stopped";
}

fn build() may fail
{
	defer {
		undo note(1);
	}
	try risky();
}
```

A local that a `defer` or an `undo` names cannot move into an `own` parameter, since the deferred statement would read it after the move. [Chapter 11]({{% relref "11-ownership" %}}) shows that refusal.

### How antic builds it

A deferred statement has no run-time record. Lowering keeps a list of exit actions per block while it walks the function. At every exit it writes the statements of that list into the IR again, in reverse order. A failing exit writes the `undo` statements of a block first.

```anti
//! name: paths
extern fn close_a();
extern fn remove_a();

fn step(ok: bool) may fail
{
	if !ok {
		fail "stopped";
	}
}

fn build(ok: bool) may fail
{
	defer close_a();
	undo remove_a();
	try step(ok);
}
```

```ir excerpt=paths target=linux-x86_64 mode=dev
fn paths.build(%0: i8 zeroext) -> ptr {
b0:
    %1 = call ptr @paths.step(%0)
    %2 = eq i8 %1, 0
    branch %2, b2, b1
b1:
    call void @remove_a()
    call void @close_a()
    ret ptr %1
b2:
    call void @close_a()
    ret ptr 0
}
```

`build` may fail, so it returns a pointer: the error, or 0 for none. [Chapter 6]({{% relref "06-errors" %}}) describes that form. `b0` calls `step` and compares the pointer it returns with 0. `b1` is the failing exit of the `try`. It runs the `undo`, then the `defer`, and returns the error. `b2` is the closing brace. It runs the `defer` alone and returns 0.

The `defer` stands twice and the `undo` once. Neither is called through a pointer, neither is recorded in the frame, and no table of the executable describes them. A function without an exit by error holds no code for its `undo` statements at all.

Where it lives: the `TOKEN_DEFER` and `TOKEN_UNDO` cases of `statement` in `src/antic/parser_stmt.c`. `push_exit_action`, `lower_run_defers` and `lower_run_defers_to` of `src/antic/lower_stmt.c` keep the list and write the copies, and `lower_block` runs the list at the closing brace.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| Declaring a `defer` or an `undo` | no instruction | no instruction |
| A `defer` at run time | its statement, once per exit taken | the same |
| An `undo` at run time | its statement on a failing exit, nothing otherwise | the same |
| Code size | one copy of a `defer` per exit of its block, one copy of an `undo` per failing exit | the same, and `opt` may merge copies |
| Bytes in the frame | 0 | 0 |
| Heap allocations | 0 | 0 |
| Calls into the runtime | 0 | 0 |
| Checks inserted | none | none |

The cost is the size of the code. A block with three deferred calls and four `try` expressions holds each call five times: four failing exits and the closing brace. A long deferred statement is better written as one function that the `defer` calls.

### Pitfalls

For a Java programmer:

- A `defer` belongs to its block, not to its function. One inside the block of an `if` or a loop runs when that block ends.
- A `defer` runs only when control passed it. A `return` above it skips it, where `finally` covers its whole `try`.
- An error is no exception, and nothing unwinds. A `defer` runs because its code stands at each exit.

For a C programmer:

- The deferred statement reads the variable at the exit. `defer free(p);` frees what `p` holds then, so `p` is not reassigned in between.
- `undo` replaces the `if (failed)` before a cleanup label. It runs on the error exit alone.
- A deferred statement cannot jump. `return`, `fail`, `try` and a `break` or `continue` of a loop outside it are refused, so cleanup code never leaves its function early.

For a C++ programmer:

- The order is that of destructors, last declared first, with one difference: every `undo` of a block runs before its `defer` statements.
- A `defer` is not an object. It cannot be dismissed, as a scope guard can. `undo` is the form that runs on failure alone.
- A deferred statement that changes a struct local changes the result of a `return` that names it. A scalar result is fixed before the deferred statements run.

## Paths that end a function

### What you know

Java refuses a method with a result that can complete normally, with "missing return statement", and treats `while (true)` without a `break` as a loop that never completes. C and C++ accept a function that falls off its end. The value the caller reads is then undefined, and a compiler warns with `-Wreturn-type` where it sees the path. C++ marks a function that never returns `[[noreturn]]`, and C11 `_Noreturn`.

### The feature

Each kind of function and of handler requires something at the end of every path, and the checker refuses a path that ends without it.

| Kind | Every path ends in | Message of the refusal |
|---|---|---|
| a function with a result, `may fail` or not | `return`, `fail` or a call that never returns | `` `f` can reach its end without `return` `` |
| an anonymous function with a result | the same | `` the anonymous function can reach its end without `return` `` |
| a function that returns `never` | `fail` or a call that never returns | `` `f` returns `never` and can reach its end `` |
| the handler of a value, `let v = f() catch { }` | `yield`, `return`, `fail`, `break`, `continue` or a call that never returns | `` the handler of a value can reach its end without `yield` `` |
| the `else` of a `let` | the same | `` the `else` of a `let` leaves the block it stands in `` |

A `may fail` function without a result may reach its end, which returns no error. The handler of a call statement, whose result nothing takes, may reach its end as well: `save(n) catch { };` goes on after the call. [Chapter 6]({{% relref "06-errors" %}}) describes handlers, and [chapter 7]({{% relref "07-optionals" %}}) the `else` of a `let`.

The checker reads the form of the last statement of a block, never the values. These forms end the path when the parts named end it:

- `return`, `fail`, and a call of a function that returns `never`, which [chapter 5]({{% relref "05-functions" %}}) describes.
- A block, a `sync` block and the block of a `let` that ends a handler, through their last statement.
- An `if` with an `else`, through every branch and the `else`. An `if let` through its block and its `else`.
- A `switch` through every arm and its `else`. Without an `else` it ends the path on an enum or a variant alone, where it names every value. An arm that ends in `fallthrough` ends where the next arm does.
- `while true do { }` whose body holds no `break` of its own. The condition is the literal `true`.
- `do { } while c` whose body ends the path and holds no `break` or `continue` of its own, since the body runs before the condition.
- A `select` through every arm, since it runs one arm and waits until a channel is ready. [Chapter 13]({{% relref "13-threads" %}}) describes it.
- A `try` block whose body ends the path and whose handler ends the path of the function. A `yield;` in the handler goes on after the block, so such a handler does not end it.

A `break` or a `continue` ends the path of a handler and of the `else` of a `let`, since both leave the statement. Inside the handler of a call in the condition of a `while` or a `do` loop they belong to that loop: `break` leaves it, and `continue` tests the condition again. The bounds and the step of a `for` run once before its first turn, so a `break` in a handler there belongs to the loop around the `for`, and is refused without one.

```anti
//! name: ends
import anti.io;
import anti.text;

enum Dir { North, East, South, West }

export fn turn(d: Dir) -> Dir
{
	switch d {
		Dir.North => { return Dir.East; },
		Dir.East => { return Dir.South; },
		Dir.South => { return Dir.West; },
		Dir.West => { return Dir.North; },
	}
}

export fn first_zero(p: *int) -> int
{
	let i = 0;
	while true do {
		if p[i] == 0 {
			return i;
		}
		i += 1;
	}
}

fn checked(n: int) -> int may fail
{
	if n < 0 {
		fail "negative";
	}
	return n;
}

fn doubled(n: int) -> int
{
	try {
		let v = checked(n);
		return v * 2;
	} catch e {
		return -1;
	}
}

fn halve(n: int) -> int
{
	let h = checked(n) catch {
		yield 0;
	};
	return h / 2;
}

fn first_of(p: ?*int) -> int
{
	let v = p else {
		return -1;
	};
	return *v;
}

fn count_ok(items: []int) -> int
{
	let ok = 0;
	for x in items {
		checked(x) catch {
			continue;
		};
		ok += 1;
	}
	return ok;
}

fn stop(code: int) -> never
{
	io.println("stopping");
	io.exit(code);
}

fn say(line: str)
{
	io.println(line);
	free(line.ptr);
}

fn main() -> int
{
	let zs = [3, 1, 0, -4];
	say(f"turn {turn(Dir.West) as int}, first_zero {first_zero(&zs[0])}");
	say(f"doubled {doubled(4)} {doubled(-4)}, halve {halve(9)} {halve(-9)}");
	say(f"first_of {first_of(&zs[1])} {first_of(none)}, count_ok {count_ok(zs[0..4])}");
	stop(0);
}
```

```text output
turn 0, first_zero 2
doubled 8 -1, halve 4 0
first_of 1 -1, count_ok 3
stopping
```

`turn` ends in a `switch` that names every value of `Dir`, `first_zero` in a `while true` without a `break`, and `doubled` in a `try` block whose body and handler both return. `main` ends in a call of `stop`, which returns `never`, so it needs no `return`.

The checker refuses each of the following. Each could reach its end on some run, or the checker cannot tell from the form.

```anti refused
//! error: `sign` can reach its end without `return`
fn sign(x: int) -> int
{
	if x > 0 {
		return 1;
	}
	if x <= 0 {
		return 0;
	}
}
```

```anti refused
//! error: `name` can reach its end without `return`
fn name(n: int) -> int
{
	switch n {
		0 => { return 10; },
		1 => { return 11; },
	}
}
```

```anti refused
//! error: `spin` can reach its end without `return`
fn spin(n: int) -> int
{
	while n >= 0 do {
		if n == 5 {
			return n;
		}
		n += 1;
	}
}
```

```anti refused
//! error: `again` can reach its end without `return`
fn again(n: int) -> int
{
	do {
		if n > 3 {
			continue;
		}
		return n;
	} while n > 0
}
```

```anti refused
//! error: `parsed` can reach its end without `return`
fn checked(n: int) -> int may fail
{
	if n < 0 {
		fail "negative";
	}
	return n;
}

fn parsed(n: int) -> int
{
	try {
		let v = checked(n);
		return v;
	} catch e {
		yield;
	}
}
```

```anti refused
//! error: `stop` returns `never` and can reach its end
extern fn exit(status: c_int) -> never;

fn stop(n: int) -> never
{
	if n > 0 {
		exit(n as c_int);
	}
}
```

```anti refused
//! error: the handler of a value can reach its end without `yield`
fn checked(n: int) -> int may fail
{
	if n < 0 {
		fail "negative";
	}
	return n;
}

fn halve(n: int) -> int
{
	let h = checked(n) catch {
		let zero = 0;
	};
	return h / 2;
}
```

```anti refused
//! error: the `else` of a `let` leaves the block it stands in
fn first_of(p: ?*int) -> int
{
	let v = p else {
		let missing = -1;
	};
	return *v;
}
```

```anti refused
//! error: `break` outside a loop
fn count() -> int may fail
{
	fail "no count";
}

fn total() -> int
{
	let t = 0;
	for i in 0..count() catch { break; } {
		t += i;
	}
	return t;
}
```

```anti refused
//! error: the anonymous function can reach its end without `return`
fn apply(f: fn(int) -> int) -> int
{
	return f(1);
}

fn run() -> int
{
	return apply(fn(x) {
		if x > 0 {
			return x;
		}
	});
}
```

### How antic builds it

The checks cost nothing at run time: they decide which programs compile. What a program gains is that every path that remains ends in a `ret`, a call that never returns or a jump to code that runs.

Lowering ends the block after a form that ends the path, the exit of a `while true` or the join after the arms of a `switch`, with `unreachable`. No path reaches that block, so it needs no `return` and runs no deferred statement.

```ir excerpt=ends target=linux-x86_64 mode=dev
export fn ends.turn(%0: i32) -> i32 range 0, 4 {
...
b6:
    %5 = eq i8 %1, 3
    branch %5, b7, b8
b7:
    ret i32 0
b8:
    jump b9
b9:
    unreachable
}
```

`b6` is the test of the last arm, `Dir.West`, and `b7` its `return`. `b8` is the path for a value that no arm names, and it jumps to `b9`, the join after the `switch`, which is `unreachable`. A dev build writes no instruction for that block, and no trap.

A release build reads `unreachable` as a fact: no value other than the four reaches the `switch`.

```llvm excerpt=ends target=linux-x86_64 mode=release
define internal noundef range(i32 0, 4) i32 @turn(i32 noundef %p0)
b0:
  %0 = icmp ult i32 %p0, 3
  br i1 %0, label %switch.lookup, label %b6
...
b6:                                               ; preds = %b0
  %v16 = icmp eq i32 %p0, 3
  tail call void @llvm.assume(i1 %v16)
  br label %common.ret
```

The first three arms became `p0 + 1`. The last compare is no longer a branch: `llvm.assume` records that `p0` is 3 on that path, and the return is 0. The code for a value outside `Dir` is gone.

A call of a function that returns `never` ends its block with `unreachable` in the same way, and the function carries the LLVM attributes `noreturn` and `cold`.

```llvm excerpt=ends target=linux-x86_64 mode=dev
define hidden void @ends.stop(i64 noundef %p0) noreturn cold #0 {
...
  call void (i64) @ends.stop(i64 0)
  unreachable
```

`noreturn` lets LLVM drop the code after each call, and `cold` places the paths that lead to it away from the hot ones.

The fact holds as long as no value outside the enum reaches the `switch`. A value of an enum comes from a literal or from `n as Dir`. A dev build checks the `as` and stops the program with `value not declared by Dir: value 7`. A release build drops that check, and a `switch` that receives such a value then runs code that assumes another, with no defined result.

Where it lives: `stmt_returns`, `stmt_leaves`, `switch_ends`, `loops_forever`, `do_ends`, `select_ends` and `try_ends` of `src/antic/sema_stmt.c` read the forms. `sema_check_function` of the same file gives the messages of a function, the `else` of a `let` is checked in `check_let`, and `refuse_handler_end` of `src/antic/sema_call.c` checks the handler of a value. `end_path` of `src/antic/lower_stmt.c` writes `unreachable`, and `src/antic/llvm_emit_function.c` writes `noreturn cold` on a function that returns `never`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A path the checker proves is never taken | a block with no instruction | nothing, and the compare before it may go |
| A call of a `never` function | the call | the call, on a path placed as cold |
| Heap allocations | 0 | 0 |
| Calls into the runtime | 0 | 0 |
| Checks inserted | none of their own. `n as Dir` checks the value | none |

The check of `as` on an enum is the one that keeps the fact true. `--no-checks` drops it from a dev build, and a release build has it only with `--checks`.

The forms that end a path without a `return`:

- A `switch` on an enum or a variant without `else` ends the path and lets a release build drop the test of the last arm. A `switch` with an `else` that returns ends it too and keeps a path for every value.
- `while true` ends the path where the loop leaves by `return` alone. A `while` with a condition needs a `return` after it, which costs nothing when no path reaches it.
- A call of a `never` function ends a path that stops the program, as `io.exit`, `Error.fatal` or a function of the program that calls one of them.

### Pitfalls

For a Java programmer:

- The rule reads the form alone, as Java's does. `while n >= 0` that never ends still needs a `return` after it.
- `do { } while` whose body returns ends the path, where Java asks for a `return` after it.
- A `switch` on an enum that names every value ends the path. Java's switch statement needs a `default` for that.

For a C programmer:

- A function with a result that can reach its end does not compile. No path returns an undefined value.
- `if (x > 0) return 1; if (x <= 0) return 0;` is refused. The second test becomes an `else`.
- A function that ends the program declares `-> never`, and every caller's path ends at the call.

For a C++ programmer:

- `-> never` is part of the type, not an attribute. The checker refuses a body of such a function that can reach its end.
- A value of an enum outside its values exists only through `as`, which a dev build checks. A release build assumes the values of a complete `switch`, as `std::unreachable()` after it would.
