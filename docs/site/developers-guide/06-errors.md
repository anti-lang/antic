---
title: "Errors"
description: "may fail, fail, try and handlers, how an error travels back, error origins and stack traces and their cost, assert and the dev checks, warnings with allow and unchecked."
summary: "`may fail`, `fail`, `try` and the handlers, how an error travels back from `fail` to its handler, error origins and stack traces and their cost, `assert` and the checks of dev mode, and the warnings and safety checks with `allow` and `unchecked`, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-26T12:00:00+01:00
draft: true
weight: 60
tags: [compilers, programming-languages]
keywords: [errors, may fail, fail, try, catch, stack trace, error origin, assert, dev checks, warnings, allow, unchecked]
---

An error of Anti is an object on the heap, and a function that fails returns a pointer to it. Nothing unwinds the stack. Every caller tests the pointer with one compare and one branch, and the path without an error pays that test and nothing else.

## Failing functions

### What you know

Java and C++ throw an exception, which leaves the function and every caller until a handler catches it. The compiler writes tables that an unwinder reads, so the path without an exception costs no instruction and a throw costs a search. C returns a code, sets `errno` and leaves the test to the caller, who may forget it.

### The feature

`may fail` after the result marks a function that can fail. Such a function leaves on one of two channels. `return v;` leaves with its result. `fail e;` leaves with an error, a `*Error`, and gives no result.

`fail "text";` is `fail lang.Error.new(0, "text");`. `fail` takes any other `str` as well, as `fail f"no file {path}";` or `fail reason;`. That error has code 0 too and owns a copy of the text, so the bytes it came from may be freed or changed before the error is read. `Error` is a class of `anti.lang`, and a file names it `lang.Error`. It holds a `code`, a `message`, the error that caused it and the position of its first `fail`. `Error.new(code, message)` makes one, and `Error.wrap(code, message, cause)` makes one that takes over the error below it. A program declares a class of its own that inherits `lang.Error` and adds fields, and a handler tests for it with `is`. [Chapter 9]({{% relref "09-classes" %}}) describes classes, inheritance and `is`.

A call of a function that may fail is handled where it stands. The next section holds the forms: `try` forwards the error, and `catch` handles it. A call with neither is refused.

A function fails by its marking alone. One that returns `*Error` or `?*Error` without `may fail` gives the error as an ordinary value, and its call needs no handler.

```anti
//! name: errs
import anti.io;
import anti.text;

class Odd inherits lang.Error
{
	pub value: int = 0,

	pub fn of(n: int) -> *Odd
	{
		return alloc Odd { code: 7, message: "odd value", value: n };
	}
}

fn half(n: int) -> int may fail
{
	if n < 0 {
		fail "negative";
	}
	if (n & 1) != 0 {
		fail Odd.of(n);
	}
	return n >> 1;
}

fn quarter(n: int) -> int may fail
{
	let h = try half(n);
	return try half(h);
}

fn report(n: int)
{
	let q = quarter(n) catch e {
		let line = f"quarter({n}) failed: code {e.code}, {e.message}, odd {e is *Odd}";
		io.println(line);
		free(line.ptr);
		yield -1;
	};
	let line = f"quarter({n}) = {q}";
	io.println(line);
	free(line.ptr);
}

fn main() -> int
{
	report(12);
	report(6);
	report(-4);
	let line = f"sizes: Error {size_of(lang.Error)}, Odd {size_of(Odd)}, *Error {size_of(*lang.Error)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
quarter(12) = 3
quarter(6) failed: code 7, odd value, odd true
quarter(6) = -1
quarter(-4) failed: code 0, negative, odd false
quarter(-4) = -1
sizes: Error 128, Odd 136, *Error 8
```

`quarter(6)` fails in its second call of `half`, with 3. The `Odd` travels through `quarter` to the handler in `report`, which reads its fields and gives -1 in place of the result.

An error made by `Error.new` or `Error.wrap` does not own its message. `message` is a `str` that points at a literal, or at memory the program keeps alive as long as the error. `fail "literal"` makes such an error over the bytes of the literal. `fail` of any other `str` makes an error that owns a copy of the text, and `fail f"..."` makes one that takes over the memory of the `f"..."`, which is a copy made for it already. The error frees what it owns when it is deleted.

`anti.error` adds four parts to the convention.

- `SystemError` is the error of a failing system call, a class that inherits `lang.Error` and adds the field `errno`.
- `SystemError.from_errno()` builds one from `errno` right after a C call that set it. Its code and `errno` hold the number, and its message holds the text of the system.
- `SystemError.from_win32()` does the same with the last error of Win32. On every other system it gives an error of code 0 with an empty message.
- `error.on_fatal(f)` registers the one function that `fatal` runs before it ends the program, and `error.check(e)` calls `fatal` on the result of a function written by hand as `-> ?*Error`, such as a binding of C, when it holds an error.

```anti
//! name: syserr
import anti.io;
import anti.lang;
import anti.text;
import anti.error;

extern fn fopen(path: ?*byte, mode: ?*byte) -> ?*byte;

fn open(path: str) -> ?*byte may fail
{
	let f = fopen(path.ptr, "r".ptr);
	if f == none {
		fail error.SystemError.from_errno();
	}
	return f;
}

fn main() -> int
{
	let f = open("/no/such/file") catch e {
		let system = e as? *error.SystemError;
		let number = 0;
		if let s = system {
			number = s.errno;
		}
		let line = f"code {e.code}, errno {number}, a SystemError {system != none}";
		io.println(line);
		free(line.ptr);
		yield none;
	};
	let nothing: ?*lang.Error = none;
	error.check(nothing);
	let line = f"sizes: SystemError {size_of(error.SystemError)}, Error {size_of(lang.Error)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
code 2, errno 2, a SystemError true
sizes: SystemError 136, Error 128
```

`fopen` of a missing file sets `errno` to `ENOENT`, which is 2 on all six targets. The handler finds the class with `as?` and reads the field. `check` of `none` returns.

The checker refuses each of the following.

```anti refused
//! error: `half` may fail and its error is not handled
fn half(n: int) -> int may fail
{
	if n < 0 {
		fail "negative";
	}
	return n >> 1;
}

fn caller() -> int
{
	return half(1);
}
```

```anti refused
//! error: `fail` outside a function that may fail
fn caller() -> int
{
	fail "stopped";
}
```

```anti refused
//! error: expected `*Error`, found an integer literal
fn half(n: int) -> int may fail
{
	if n < 0 {
		fail 5;
	}
	return n >> 1;
}
```

```anti refused
//! error: `half` may fail and never does [never-fails]
fn half(n: int) -> int may fail
{
	return n >> 1;
}

fn main() -> int
{
	let h = half(2) catch { yield 0; };
	return h;
}
```

```anti refused
//! error: a function that returns `never` cannot fail
fn stop(n: int) -> never may fail
{
	fail "stopped";
}
```

The fourth is a warning, `never-fails`. A release build refuses every warning, and a dev build prints it and continues.

### How antic builds it

A function `f(args) -> R may fail` is compiled as the C function `?*Error f(args, R *out)`. The result register holds the error, or null for none. The value goes through `out`, a pointer the caller passes as the last argument, to a slot of the caller's frame.

The three functions below carry one error from `fail` to its handler.

```anti
//! name: travel
extern fn note(code: c_int);

fn half(n: int) -> int may fail
{
	if (n & 1) != 0 {
		fail "odd";
	}
	return n >> 1;
}

fn quarter(n: int) -> int may fail
{
	let h = try half(n);
	return try half(h);
}

export fn report(n: int) -> int
{
	let q = quarter(n) catch e {
		note(e.code as c_int);
		yield -1;
	};
	return q;
}
```

The path starts at the `fail`.

```ir excerpt=travel target=linux-x86_64 mode=dev
fn travel.half(%0: i64, %1: ptr nonnull deref(size_of i64)) -> ptr {
...
    %7 = call ptr @anti.lang.Error.new(0, %4)
    %8 = copy ptr %7
    %9 = ptradd %8, offset_of anti.lang.Error.at
    %10 = ptradd %9, offset_of anti.lang.SourceLocation.line
    %11 = load i64 %10
    %12 = eq i8 %11, 0
    branch %12, b3, b5
...
b3:
    %13 = addr @travel.4
    memcopy %9, %13, anti.lang.SourceLocation
    %14 = call i8 @anti_rt_backtrace_on()
    branch %14, b4, b5
b4:
    %15 = call ptr @anti.lang.StackTrace.capture(0)
...
b5:
    ret ptr %8
```

`half` has two parameters: `n` and the out pointer `%1`. `Error.new` allocates the error and returns its address. The next four lines load the line of its origin, `at.line`, and compare it with 0. An error that no `fail` has seen yet has line 0. `b3` then writes the origin by copying the constant location of this `fail` into the error. `b3` also asks the runtime whether backtraces are on, and `b4` captures one when they are. A later section describes both. `b5` is the failing exit, `ret ptr %8`: the function returns the address of the error and writes nothing through `%1`.

The successful exit of `half` stores the result through `%1` and returns null, `store i64 %N, %1` and `ret ptr 0`.

The error next passes through `quarter`, which forwards it twice.

```ir excerpt=travel target=linux-x86_64 mode=dev
fn travel.quarter(%0: i64, %1: ptr nonnull deref(size_of i64)) -> ptr {
b0:
    %2 = slot i64
...
    %3 = call ptr @travel.half(%0, %2)
    %4 = eq i8 %3, 0
    branch %4, b2, b1
b1:
    ret ptr %3
b2:
    %6 = load i64 %2 !type i64
    %7 = call ptr @travel.half(%6, %5)
    %8 = eq i8 %7, 0
    branch %8, b4, b3
b3:
    ret ptr %7
b4:
    %9 = load i64 %5
    store i64 %9, %1
    ret ptr 0
```

`%2` is the slot of `h`, which `quarter` passes to `half` as the out pointer. `%4` compares the returned pointer with null. `b1` is the `try`: it returns the same pointer to its own caller. No copy of the error is made and nothing is recorded. `b2` goes on with the value that `half` wrote into `%2`. The second `try` is the same three lines.

The handler is the end of the path.

```ir excerpt=travel target=linux-x86_64 mode=dev
export fn travel.report(%0: i64) -> i64 {
b0:
    %1 = slot i64
    lifestart %1
    %2 = call ptr @travel.quarter(%0, %1)
    %3 = eq i8 %2, 0
    branch %3, b2, b1
b1:
    %4 = copy ptr %2
    %5 = ptradd inbounds %4, offset_of anti.lang.Error.code
    %6 = load i64 %5 !type anti.lang.Error.code
...
    store i64 -1, %1
    %12 = addr @anti.lang.Error.descriptor
    call void @anti_rt_delete(%4, %12)
    jump b2
```

`%1` is the slot of `q`. The call and the compare are those of a `try`. `b1` is the handler: `%4` is `e`, and `e.code` is a load at the offset of the field. `yield -1` is the `store` of -1 into the slot of `q`. The handler ends with `anti_rt_delete`, which runs the teardown of the error through its table and frees the object. `b2`, which both paths reach, reads `q` from its slot.

In machine code the test after each call is two instructions. The next section shows them in a release build.

`SystemError.from_errno()` is a function of `anti.error`, written in Anti. It reads `errno` through `anti_rt_errno` of the runtime, allocates the error and asks the runtime for the text of the number. The system writes that text into a buffer that its next call may write over, so `anti_rt_text_kept` copies it once, from its first use to the end of the program. Every error of the same text shares the copy, and the error owns no byte of its message.

Where it lives: `lower_fail` and `write_origin` of `src/antic/lower_error.c` write a `fail`, and `lower_handle_error` of the same file the test after a call. `lower_function` of `src/antic/lower_function.c` adds the out pointer. `Error` and its functions are in `src/std/anti/lang.anti`, and `SystemError`, `on_fatal` and `check` in `src/std/anti/error.anti`, with `anti_rt_text_kept` in `src/rt/text.c`. `anti_rt_delete` is in `src/rt/object.c`, and `src/antic/header.c` writes the C form of a failing function.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A call that does not fail | one more argument, one compare with null, one branch not taken, one store of the result through the out pointer | the same, or nothing when `opt` inlines the callee |
| `fail` of a new error | one `malloc` of the error and one call of `anti_rt_hook` for its `created` hook, one load and one compare of its line, one copy of 64 bytes, one call of `anti_rt_backtrace_on` | the same |
| `fail` with backtraces on | one capture of the stack as well, see the later section | off by default |
| `fail` of a `str` that is no literal | one `malloc` and one copy of its bytes, none for an `f"..."`, whose memory the error takes over | the same |
| Each frame the error passes | one compare, one branch taken, one return | the same |
| Handling it | one call of `anti_rt_delete`: the teardown with its `destroyed` hook and one `free` | the same |
| Bytes of an error | 128 on the heap, and what a class of the program adds | the same |
| `SystemError.from_errno()` | one `malloc` of 136 bytes, the calls `anti_rt_errno` and `anti_rt_errno_text`, and one copy of each distinct text of the system, kept to the end of the program | the same |
| Bytes in a frame | 8 for a result of 8 bytes that a failing call writes | the same or none |
| Locks | 0 | 0 |
| Atomic operations | one load in `anti_rt_backtrace_on`, one load of the installed trace handler in each call of `anti_rt_hook` for `created` or `destroyed`, and one load of the count of loaded libraries when the error is made and again when it is deleted | the same |
| Checks inserted | none | none |

Every object calls the `created` hook when it is made and the `destroyed` hook when it is torn down, and an error is an object. [Chapter 15]({{% relref "15-hooks" %}}) describes the hooks and `--no-hooks`, which drops those calls.

The executable holds no unwind tables for errors and no table of handlers. A function that may fail and a function that may not are compiled alike, except for the out pointer and the test.

An error that does occur costs one heap allocation, where a call that succeeds costs one compare. A function that fails as a matter of routine is better written with a `?T` result of [chapter 7]({{% relref "07-optionals" %}}), which allocates nothing. A parser that tries one form after another is such a function.

### Pitfalls

For a Java programmer:

- A failing call is visible in the source. Every one carries `try` or a handler, and no error passes a function that does not say so.
- An error is deleted when its handler ends. A handler that wants to keep it wraps it or hands it on.
- A class of errors is tested with `e is *Odd`, not chosen by the type in the `catch`.

For a C programmer:

- The test cannot be forgotten: a failing call without a handler does not compile.
- The value arrives through the out pointer only when the result is null. After an error the slot is not written.
- From C the function is called as `Error *f(args, R *out)`. [Chapter 17]({{% relref "17-c-interop" %}}) shows the header and the C side of an error.

For a C++ programmer:

- `may fail` is part of the type of the function, as `noexcept` is, with the meaning reversed.
- No destructor runs during unwinding, since nothing unwinds. Each frame runs its `defer` and `undo` statements at the failing exit, as [chapter 4]({{% relref "04-flow" %}}) shows.
- An error is always an object on the heap, never a value thrown by copy.

## try and handlers

### What you know

Java and C++ wrap a region in `try` and list handlers by type after it. A call outside any `try` lets the exception pass, without a mark at the call. C tests the return code after each call with an `if`, and shares the cleanup through a `goto`.

### The feature

A call of a function that may fail takes one of these forms.

| Form | What it does |
|---|---|
| `try f()` | forwards the error to the caller, which itself may fail |
| `f() catch e { }` | runs the handler with the error bound to `e` |
| `f() catch { }` | runs the handler and binds no name |
| `f() catch fatal` | prints the error and ends the program |
| `f() catch none` | gives `none` in place of a result that can be `none` |
| `try { } catch e { }` | handles every failing call of the block in one handler |

`try` stands wherever the call stands, inside an expression as well: `return try half(a) + try half(b);`. It is refused outside a function that may fail.

A handler follows a `let`, an assignment or a call statement. It does not follow a call inside a larger expression. Where the call gives a result, the handler supplies one with `yield v;`, or leaves the block with `return`, `break`, `continue` or `fail`. A handler of a call without a result ends with `yield;` or at its brace.

The error a handler binds is deleted at every exit of the handler. `fail e;` takes it away first by handing it to the caller. So does passing it to an `own` parameter, as `Error.wrap` does with its cause, and so does `delete(e)`. The handler names the error no more after either. The name of the error ends with the handler.

`yield v` gives the value as `let` gives a variable its value. A fresh value and an `own` parameter move into the result. An existing value that owns something, such as a local of a class with an `own` field, is refused with the message of `=`, since the local would stay a second owner. `yield dup(a)` gives a copy.

`catch fatal` ends the whole program, in a worker as well, and no `defer` runs. A worker that may fail is called directly with a handler. `dispatch` and `parallel` refuse it, since no caller stands there to take its error, and [chapter 13]({{% relref "13-threads" %}}) describes both.

A `try` block runs its statements until one fails. The rest of the block is skipped, its `defer` statements run, and the handler runs. A call inside the block needs no mark of its own. A call there that carries `try` forwards to the handler of the block as well, and never to the caller. In that handler `yield;` ends the handler, and the program goes on after the block.

```anti
//! name: hnd
import anti.io;
import anti.text;

fn half(n: int) -> int may fail
{
	if (n & 1) != 0 {
		fail "odd";
	}
	return n >> 1;
}

fn find(n: int) -> ?*int may fail
{
	if n < 0 {
		fail "negative";
	}
	return none;
}

fn both(n: int) -> int may fail
{
	return try half(n) + try half(n + 2);
}

fn wrapped(n: int) -> int may fail
{
	let h = half(n) catch e {
		fail lang.Error.wrap(9, "wrapped", e);
	};
	return h;
}

fn main() -> int
{
	let b = half(3) catch { yield 7; };
	let c = 5;
	c = half(3) catch { yield 8; };
	let d = find(-1) catch none;
	let s = both(4) catch { yield -1; };
	let w = wrapped(3) catch e { yield e.code; };
	let total = 0;
	try {
		total += half(8);
		total += half(5);
		total += half(100);
	} catch e {
		total += 1000;
	}
	half(1) catch { };
	let line = f"b {b}, c {c}, d {d == none}, s {s}, w {w}, total {total}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
b 7, c 8, d true, s 5, w 9, total 1004
```

In the `try` block the first call adds 4, the second fails, and the third never runs. `wrapped` moves the error into a new one with code 9, which the handler in `main` reads.

The checker refuses each of the following.

```anti refused
//! error: `try` outside a function that may fail
fn half(n: int) -> int may fail
{
	if n < 0 {
		fail "negative";
	}
	return n >> 1;
}

fn caller() -> int
{
	let h = try half(1);
	return h;
}
```

```anti refused
//! error: `catch none` needs a result that can be `none`, and this call gives `int`
fn half(n: int) -> int may fail
{
	if n < 0 {
		fail "negative";
	}
	return n >> 1;
}

fn caller() -> int
{
	let h = half(1) catch none;
	return h;
}
```

```anti refused
//! error: expected `int`, found `str`
fn half(n: int) -> int may fail
{
	if n < 0 {
		fail "negative";
	}
	return n >> 1;
}

fn caller() -> int
{
	let h = half(1) catch { yield "none"; };
	return h;
}
```

```anti refused
//! error: the call writes no result, so `yield` takes no value
fn log() may fail
{
	fail "full";
}

fn caller()
{
	log() catch { yield 1; };
}
```

```anti refused
//! error: unknown name `e`
fn half(n: int) -> int may fail
{
	if n < 0 {
		fail "negative";
	}
	return n >> 1;
}

fn caller() -> int
{
	let h = half(1) catch e { yield 0; };
	return e.code;
}
```

```anti refused
//! error: `e` shadows the outer `e` [shadowed-catch]
fn half(n: int) -> int may fail
{
	if n < 0 {
		fail "negative";
	}
	return n >> 1;
}

fn caller() -> int
{
	let e = 1;
	let h = half(1) catch e { yield 0; };
	return h + e;
}
```

```anti refused
//! error: `Key` has `own` fields, use `dup` instead of `=`
class Leaf
{
	pub n: int = 0,
}

class Key
{
	pub n: int = 0,
	pub own leaf: ?*Leaf = none,
}

fn key(n: int) -> Key
{
	return Key { n: n, leaf: alloc Leaf { n: n } };
}

fn failing(n: int) -> Key may fail
{
	if n > 0 {
		fail "refused";
	}
	return key(n);
}

fn caller()
{
	let a = key(1);
	let b = failing(1) catch e {
		yield a;
	};
}
```

```anti refused
//! error: `run` may fail, and `dispatch` runs a worker that cannot fail
class Task
{
	pub n: int = 0,
}

worker fn run(t: *Task) -> int may fail
{
	if t.n > 2 {
		fail "too large";
	}
	return t.n;
}

fn caller() -> int
{
	let t = alloc Task { };
	let job = dispatch t -> run();
	let got = join(job);
	delete(t);
	return got;
}
```

The sixth is the warning `shadowed-catch`, which a release build refuses. The last section of this chapter describes warnings.

### How antic builds it

Every form is the compare of the last section, followed by another block.

| Form | The block after a failed call |
|---|---|
| `try` | the `undo` and `defer` statements of the blocks left, then `ret` with the error |
| `catch e { }` | the handler, then `anti_rt_delete` of the error at each exit |
| `catch fatal` | a call of `Error.fatal`, which does not return |
| `catch none` | a store of null into the result, then `anti_rt_delete` |
| `try { } catch` | a jump to the one handler block, which every call of the block shares |

A release build keeps the compare and the branch after every call it does not inline.

```anti
//! name: thr
extern fn note(code: c_int);

export fn twice(f: fn(int) -> int may fail, n: int) -> int may fail
{
	let h = try f(n);
	return try f(h);
}

export fn guarded(f: fn(int) -> int may fail, n: int) -> int
{
	let v = f(n) catch e {
		note(1);
		yield -1;
	};
	return v;
}
```

```asm excerpt=thr target=linux-x86_64 mode=release
leaq -0x28(%rbp), %rsi
movq %rdx, %rdi
movq %r15, %rdx
callq *%r14
testq %rax, %rax
jne 0x44 <twice+0x44>
movq -0x28(%rbp), %rdi
leaq -0x20(%rbp), %rsi
movq %r15, %rdx
callq *%r14
testq %rax, %rax
jne 0x44 <twice+0x44>
movq -0x20(%rbp), %rax
movq %rax, (%rbx)
xorl %eax, %eax
addq $0x18, %rsp
```

This is `twice`. `leaq -0x28(%rbp), %rsi` is the out pointer, the address of the slot of `h`. After the call `rax` holds the error or null. `testq` and `jne` are the `try`: both jump to the one exit of the function at `0x44`, the `addq`, with the error still in `rax`. The path without an error loads `h` from its slot and calls again. It stores the final result through the out pointer of `twice` in `rbx` and clears `rax` to return null.

```asm excerpt=thr target=linux-x86_64 mode=release
callq *%rcx
testq %rax, %rax
je 0x41 <guarded+0x41>
movq %rax, %rbx
movl $0x1, %edi
callq 0x2a <guarded+0x2a>
movq $-0x1, -0x10(%rbp)
leaq (%rip), %rsi
movq %rbx, %rdi
callq 0x41 <guarded+0x41>
movq -0x10(%rbp), %rax
```

This is `guarded`. `je` skips the handler when no error came back. The handler keeps the error in `rbx`, calls `note` and stores -1 into the slot of `v`. It then calls `anti_rt_delete` with the error and the address of the descriptor of `Error`. The linker fills in the targets of the two direct calls. The last line, which both paths reach, loads `v`.

On arm64 the test is one instruction, `cbnz` or `cbz` on `x0`.

Where it lives: `sema_call.c` of `src/antic/` checks the forms and the `yield`, and `check_try` of `src/antic/sema_stmt.c` the block. `lower_handle_error` and `lower_handler` of `src/antic/lower_error.c` write the branch and the handler. `lower_push_error_action` of `src/antic/lower_stmt.c` records the delete that each exit of a handler runs.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| `try` | one compare and one branch per call | the same, two instructions on x86-64 and one on arm64 |
| A handler that does not run | one compare and one branch | the same |
| A handler that runs | its body, and one `anti_rt_delete` | the same |
| `catch none` | one store of null and one `anti_rt_delete` on failure | the same |
| A `try` block | one compare and one branch per failing call in it | the same |
| Code size | the `defer` statements of the blocks left, once per `try` | the same, and `opt` may merge the exits |
| Heap allocations | 0 | 0 |
| Checks inserted | none | none |

A function with four `try` expressions and three `defer` statements holds each deferred statement five times, as [chapter 4]({{% relref "04-flow" %}}) counts.

The forms differ in where the error goes, and their cost on the path without an error is the same compare and branch:

| Form | Where the error goes | Cost when the call fails |
|---|---|---|
| `try f()` | to the caller | the deferred statements, one return |
| `f() catch e { }` | into the handler, deleted at its end | the handler and one `anti_rt_delete` |
| `try { } catch e { }` | into one handler for the block | the same, with one handler block in the code |
| `f() catch none` | dropped, the result is `none` | one store and one `anti_rt_delete` |
| `f() catch fatal` | printed, and the program ends | the print and the exit |

`try` is the form where the caller can decide better. A handler is the form where the function has the answer, such as a default value. A `try` block is the form for a group of calls that share one answer. It holds the handler once, where a `catch` per call holds one copy per call.

### Pitfalls

For a Java programmer:

- `try` is not a block that guards a region. It marks one call and forwards its error. The guarding form is `try { } catch e { }`.
- A handler has no type list. One handler takes every error, and tests the class inside.
- A handler whose value a `let`, an assignment or a guard takes ends every path in `yield`, `fail`, `return` or a call that returns `never`. The checker refuses one that can reach its end, since the name would hold no defined value. The handler of a call that stands as a statement may reach its end.

For a C programmer:

- `catch { }` with an empty block drops the error on purpose, and deletes it. Nothing leaks.
- The error is freed at the end of the handler. The checker refuses a bare `e` stored by `let`, `=` or `yield`, or into a literal, which would dangle. `dup(e)` keeps a copy.
- `catch fatal` is the `perror` and `exit` pair: it prints the error and ends the program with its code, or with 1 when the code is 0.

For a C++ programmer:

- A handler cannot rethrow with a bare statement. `fail e;` hands the same error on, with its origin unchanged.
- `Error.wrap` takes the error over as its cause. The handler does not delete it then, and the new error deletes it later.
- A handler does not follow a call in the middle of an expression. The call is bound with `let` first.

## Error origins and stack traces

### What you know

A Java exception captures its stack trace when it is constructed, in every build, and prints it with the names of the methods. A C++ exception carries no position. `std::stacktrace` of C++23 and `backtrace()` of glibc capture addresses on request, and the names come from debug information. C has `errno`, which names no place.

### The feature

Every error carries its origin. The first `fail` that gives an error writes the position of that `fail` into the field `at`. The field is a `SourceLocation` with the file, the line, the column, the function and the module. A later `fail` of the same error, such as `fail e;` in a handler, leaves the origin as it is.

`e.text()` gives the error as text: `file:line:column: error N: message`, or the message alone after the position when the code is 0. Each cause follows on a line of its own after `caused by:`. `e.print()` writes that text to the standard error.

An error may also carry a stack trace, the return addresses of the calls that led to the `fail`. The trace is captured at the first `fail`, together with the origin, when backtraces are on. They are on in a dev build and off in a release build. A release program turns them on with the option `--anti.backtrace` on its command line or with `backtrace = true` in its runtime configuration, without being built again. [Chapter 15]({{% relref "15-hooks" %}}) describes the runtime configuration.

A trace holds raw frames: an address, and the module it lies in with its build id and its base address. `e.text()` appends them after the causes in that raw form. `anti symbols resolve` of [chapter 18]({{% relref "18-the-tool" %}}) turns it into names and lines from the symbols archive of a release.

`lang.StackTrace.capture()` takes a trace anywhere, in any build. `debug.backtrace(skip)` of `anti.debug` is its short form, the trace of its caller without the `skip` innermost frames. Its `frames` are the raw frames, and `text()` gives the raw form. `symbolize()` reads the name of the function of each frame from the symbol table of the program. In a build with `-g` it reads the file and the line as well.

```anti
//! name: origin
import anti.io;
import anti.text;

fn half(n: int) -> int may fail
{
	if (n & 1) != 0 {
		fail "odd";
	}
	return n >> 1;
}

fn load(n: int) -> int may fail
{
	let h = half(n) catch e {
		fail lang.Error.wrap(2, "cannot load", e);
	};
	return h;
}

fn main() -> int
{
	let v = load(3) catch e {
		io.println(e.text());
		let line = f"at {e.at.line}:{e.at.column} in {e.at.function}";
		io.println(line);
		free(line.ptr);
		yield 0;
	};
	let line = f"sizes: SourceLocation {size_of(lang.SourceLocation)}, StackTrace {size_of(lang.StackTrace)}, RawFrame {size_of(lang.RawFrame)}";
	io.println(line);
	free(line.ptr);
	return v;
}
```

```text output
origin.anti:15:3: error 2: cannot load
  caused by: origin.anti:8:3: odd
at 15:3 in origin.load
sizes: SourceLocation 64, StackTrace 40, RawFrame 48
```

The page builds this program in release mode, so the text holds two origins and no trace. The line numbers count the lines of the module that the test of this page builds, which has no blank lines between its items.

A dev build of the same program, made with `anti run` on macos-arm64, prints the trace of the outer error after the causes. The addresses differ per run, and the path of the first module is shortened here.

```text
main.anti:15:3: error 2: cannot load
  caused by: main.anti:8:3: odd
module 0 823e53a59f5364714242c3e349f7ab966c09a195209bd80a3c09a18e9733f9bc 0x0000000100dd4000 dist/macos-arm64/dev/origin
module 1 - 0x000000018a9d8000 /usr/lib/dyld
0x0000000100dd49d8 0+0x9d8
0x0000000100dd4a80 0+0xa80
0x0000000100ddfaac 0+0xbaac
0x000000018a9f84e4 1+0x204e4
```

A `module` line gives a number, the build id, the base address and the path. A frame line gives the address, then the number of its module and the offset in it. The four frames are `load`, `main`, the entry of the runtime and the loader of the system.

The `function` of an origin is the full name of the function that holds the `fail`. An anonymous function is named after the function around it and its number. A copy of a generic carries its type arguments, and a function of a nested type names each type around it.

```anti
//! name: fnames
import anti.io;

class Outer
{
	class Inner
	{
		pub fn check(self, n: int) -> int may fail
		{
			if n < 0 {
				fail "nested";
			}
			return n;
		}
	}

	pub fn run(self, n: int) -> int may fail
	{
		let i = Inner { };
		return try i.check(n);
	}
}

fn first<T>(x: T) -> T may fail
{
	fail "generic";
}

fn call(f: fn() -> int may fail) -> int may fail
{
	return try f();
}

fn show(e: *lang.Error)
{
	io.println(e.at.function);
}

fn main() -> int
{
	let a = first(3) catch e {
		show(e);
		yield 0;
	};
	let b = call(fn() -> int { fail "anonymous"; }) catch e {
		show(e);
		yield 0;
	};
	let o = Outer { };
	let c = o.run(-1) catch e {
		show(e);
		yield 0;
	};
	return a + b + c;
}
```

```text output
fnames.first<int>
fnames.main.0
fnames.Outer.Inner.check
```

### How antic builds it

The code after each `fail` writes the origin, as the first section shows. It is a load of `at.line`, a compare with 0 and, for a new error, a copy of a constant `SourceLocation` of 64 bytes. That constant is the same kind of record as a `here` of [chapter 5]({{% relref "05-functions" %}}). An error that already has a line skips the copy and the rest.

Whether a trace follows is decided at run time by `anti_rt_backtrace_on`. It reads the option `backtrace` with one atomic load. When the option is unset it reads the constant `anti_rt_backtrace_default`. The whole-program pass writes it into every program that can fail: 1 in a dev build, 0 in a release build. A library is therefore compiled once and serves both.

The capture is `StackTrace.capture`, a function of `anti.lang`.

- It reserves 64 addresses, 512 bytes, in its frame.
- It calls `anti_rt_trace_walk`, which fills them with return addresses. On Linux and macOS the walk follows the chain of frame records from its own frame. Every function that calls another keeps a frame record, in both modes, so the walk needs no unwind tables. On Windows it unwinds through the unwind data of each function.
- It allocates the `StackTrace`, 40 bytes, and an array of one `RawFrame` of 48 bytes per frame.
- It calls `anti_rt_trace_frame` once per frame, which finds the module of the address, its base and its build id.

No name is looked up at the capture. `symbolize` and `anti symbols resolve` do that later, and only for a trace that is read.

```llvm excerpt=travel target=linux-x86_64 mode=dev
declare noundef ptr @anti.lang.StackTrace.capture(i64 noundef) #1
...
declare noundef i8 @anti_rt_backtrace_on() #1
```

The module of a function that fails declares the two functions it calls. A function that walks the stack and skips frames by count stays out of line, as [chapter 5]({{% relref "05-functions" %}}) says. The first frame of the trace is therefore the function with the `fail`.

In a release build `opt` inlines small functions, and an inlined function has no frame. A trace of a release build lists the functions that still have one.

Where it lives: `lower_fail` of `src/antic/lower_error.c` writes the origin and the question. `anti_rt_backtrace_on` is in `src/rt/backtrace.c`, and `whole_write_backtrace_default` of `src/antic/whole_tables.c` writes the default. `StackTrace` is in `src/std/anti/lang.anti`. `anti_rt_trace_walk` is in `src/rt/platform_posix.c` and `src/rt/platform_windows.c`, and `anti_rt_trace_frame`, `anti_rt_trace_text` and `anti_rt_trace_symbolize` are in `src/rt/trace.c`.

### What it costs

| Cost | Dev | Release | Release with `--anti.backtrace` |
|---|---|---|---|
| The origin, at the first `fail` | one load, one compare, a copy of 64 bytes | the same | the same |
| The question | one call of `anti_rt_backtrace_on`, one atomic load | the same | the same |
| A later `fail` of the same error | one load, one compare, one branch | the same | the same |
| The capture | one per new error | none | one per new error |
| Stack used by a capture | 512 bytes | 0 | 512 bytes |
| Runtime calls of a capture | one `anti_rt_trace_walk`, one `anti_rt_trace_frame` per frame | 0 | the same as dev |
| Heap allocations of a capture | 2: 40 bytes, and 48 bytes per frame | 0 | 2 |
| Frames kept | at most 64, the innermost | 0 | at most 64 |
| Bytes in the error | 64 for the origin, 8 for the pointer to the trace | the same | the same |
| Read-only data | 64 bytes per `fail` statement | the same | the same |

An error with a trace of 10 frames takes three heap blocks: 128 bytes for the error, 40 for the trace and 480 for the frames. Without a trace it takes one.

The walk reads two words per frame, and `anti_rt_trace_frame` asks the loader for the module of each address. A program that fails in a loop pays the capture at every new error in a dev build. Passing `--anti.backtrace=false` to a dev program turns the capture off and leaves the origin.

The text of an error is formed at the first `text()` or `print()`, in one heap block that the error keeps and frees. An error that is handled without being printed never forms it.

The choice is a setting of the run, not a form of the source:

- The origin costs 64 bytes of read-only data per `fail` and a copy at each new error, in every build. It names the place, which is enough for an error that a handler reads.
- A trace costs a walk of the stack and two heap blocks per new error. A dev build pays it by default. `--anti.backtrace=false` drops it from a dev run that fails in a loop.
- A release run pays no trace until `--anti.backtrace` or `backtrace = true` asks for it, without a new build.
- `lang.StackTrace.capture()` takes a trace at a place of the program's choosing, in any build, at the same cost as a capture of a `fail`.

### Pitfalls

For a Java programmer:

- A release build captures no stack trace by default. The origin, one position, is always there.
- A trace is raw addresses. The names come from `symbolize()` in the program, or from `anti symbols resolve` and the symbols archive afterwards.
- The origin is where the error was first failed, not where it was constructed. `Error.new` on one line and `fail` on another gives the line of the `fail`.

For a C programmer:

- The position costs no macro. `fail` writes it.
- Frame records are kept in release builds as well, so a trace works without debug information. A leaf function keeps none.
- A trace ends at the first C frame that has no frame record.

For a C++ programmer:

- `fail e;` in a handler keeps the first origin and the first trace. The path of the rethrow is not recorded.
- A wrapped error has an origin and a trace of its own, and `text()` prints the trace of the outermost error alone.
- Inlining removes frames from the trace of a release build. A build with `-g` records the inlined calls for a debugger, and [chapter 20]({{% relref "20-targets" %}}) describes it.

## assert and the dev checks

### What you know

`assert(c)` of C and C++ is a macro that vanishes under `NDEBUG`, with its argument. Java's `assert` runs only under `-ea`. Java checks every array index and every cast in every build. C and C++ check nothing, and a wrong index or a signed overflow is undefined behaviour.

### The feature

`assert(cond);` and `assert(cond, "message");` test a `bool`. The message is a string literal. When the condition is false the program prints the file, the line and the message, and aborts. Without a message it prints the text of the condition.

A dev build keeps every assertion and a release build drops every one, with its condition. A condition that calls a function is therefore the warning `assert-call`, since a release build would not run the call.

A dev build also inserts checks around operations that can go wrong. Each prints the file, the line, the operation and the values, and aborts.

| Check | Fails when | Chapter |
|---|---|---|
| Bounds | an index is not below the length of an array, a slice or a `str`, or a part `xs[lo..hi]` reaches past its length | 12 |
| Overflow | a signed `+`, `-` or `*` leaves the range of its type, the least value is divided by -1 or its remainder by -1 taken, or unary `-` negates it | 3 |
| Narrowing | a conversion with `as` loses the value | 3 |
| A `char` | a conversion to `char` gives a surrogate or a value past `0x10FFFF` | 3 |
| Division | the divisor of `/` or `%` is zero | 3 |
| Shift | a shift count is below zero or not below the width of the type | 3 |
| Bitfield | a write or a compound assignment gives a bitfield a value its width does not hold | 8 |
| Enum | a number converted to an enum names none of its values | 8 |
| Changed collection | a collection changed its size while a `for` walks it | 4 |
| Count of chunks | the count of chunks of a `parallel` is below zero | 13 |
| Second `join` | a job is joined a second time, where the checker cannot see it | 13 |
| A held `Mutex` | a thread takes a `Mutex` it holds, where the checker cannot see it | 14 |

A message names the file and the line, the check and the values. Four of them, after the file and the line, read:

```text
overflow in /: left -9223372036854775808, right -1
value out of range for char: value 55296
value out of range for the 3 bits of a: value 9
slice end out of bounds: index 5, length 3
```

Later chapters add checks of their own, such as the zero table pointer before a dispatch and the order of the locks. "Checks of a release build" of [chapter 21]({{% relref "21-performance" %}}) lists the checks with what a release build does with each.

Four options of antic override the mode. `--checks` keeps the checks in a release build, and `--no-checks` drops them from a dev build. `--asserts` and `--no-asserts` do the same for assertions. The build of the program decides for every module in it, the standard library included.

```anti
//! name: asr
import anti.io;
import anti.text;

export fn checked(n: int) -> int
{
	assert(n >= 0, "n is not negative");
	assert(n < 1000);
	return n +% 1;
}

fn main() -> int
{
	let line = f"checked {checked(-5)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
checked -4
```

The page builds the program in release mode, where the first assertion is gone and `checked(-5)` returns -4. A dev build of it prints `asr.anti:7: assertion failed: n is not negative` and aborts, and so does a release build with `--asserts`.

The checker refuses each of the following.

```anti refused
//! error: this `assert` condition calls a function, which a release build does not run [assert-call]
fn next(n: int) -> int { return n + 1; }

fn main() -> int
{
	assert(next(1) > 0);
	return 0;
}
```

```anti refused
//! error: the message of an `assert` is a string literal
let reason = "positive";
assert(1 > 0, reason);
```

```anti refused
//! error: expected `bool`, found an integer literal
assert(1);
```

The first is a warning, which a release build refuses and a dev build prints.

### How antic builds it

An assertion and a check are a compare and a branch. The branch leads to a block that calls the runtime, which prints the message and aborts. A program that passes runs the compare and a branch that is not taken. Where the mode drops them, nothing of either is left in the program.

Lowering writes both in both modes, each as a branch into a block that ends in `unreachable`.

```ir excerpt=asr target=linux-x86_64 mode=dev
export fn asr.checked(%0: i64) -> i64 {
b0:
    %1 = sge i8 %0, 0
    branch %1, b2, b1
b1:
    %2 = addr @asr.0
    call void @anti_rt_assert_failed(%2, 47)
    unreachable
b2:
    %3 = slt i8 %0, 1000
    branch %3, b4, b3
b3:
    %4 = addr @asr.1
    call void @anti_rt_assert_failed(%4, 38)
    unreachable
b4:
    %5 = add i64 %0, 1
    ret i64 %5
}
```

`b0` is the first condition. `b1` is its failure block: the address of the text, which the compiler built with the file and the line in it, and the length 47. `anti_rt_assert_failed` writes the text to the standard error and calls `abort`. A check has the same form with `anti_rt_check_failed`, which takes the text, a kind and the two values it prints.

The driver then decides per build. Where assertions are off it replaces each branch into an assertion block with a jump to the other side. The ordinary passes then remove the block, the call, the text and the condition. It does the same for the check blocks where checks are off.

```llvm excerpt=asr target=linux-x86_64 mode=release
define internal noundef i64 @checked(i64 noundef %p0) #4 {
b0:
  %v1 = add i64 %p0, 1
  ret i64 %v1
}
```

The release function is the addition alone. The module holds no compare of `n`, no text and no declaration of `anti_rt_assert_failed`.

A failed check or assertion ends the program with `abort`, which raises `SIGABRT` on Linux and macOS. No `defer` runs and no error is made, so a handler cannot catch it.

Where it lives: `assert_branch` and the `STMT_ASSERT` case of `src/antic/lower_stmt.c` write an assertion, and `src/antic/lower_check.c` the checks. `optimize_drop_failures` of `src/antic/optimize.c` drops them, called from `back_end` of `src/antic/driver.c`. `anti_rt_assert_failed` is in `src/rt/assert.c` and `anti_rt_check_failed` in `src/rt/check.c`.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| An `assert` | its condition, one branch | nothing: the condition is removed too |
| A check | one compare or one overflow flag, one branch | nothing |
| A failed one | one call that does not return | none exists |
| Read-only data | one text per assertion and per check site | none |
| With `--asserts` or `--checks` in release | none | the dev cost, in optimized code |
| Heap allocations | 0 | 0 |
| Locks and atomic operations | 0 | 0 |

Each failure block is `cold`, so the compare and a branch that is not taken are all that the passing path runs.

Turning the checks off gives up the stop. Without the bounds check an index past the end reads or writes other memory. Without the overflow check a signed overflow wraps. [Chapter 3]({{% relref "03-values" %}}) states what each arithmetic operation does without its check, and [chapter 12]({{% relref "12-memory" %}}) what an index does.

A release build is tested with its checks on by passing `--checks --asserts`. That build runs the optimized code and still stops at the first violation.

The forms that state a condition:

- `assert(c)` states a fact of the program's own logic. It costs a compare and a branch in a dev build and nothing in a release build.
- `if !c { fail "..."; }` tests a condition that a run can break, such as its input. It stays in every build, costs the same compare and branch, and hands the caller an error to handle.
- The checks of dev mode cover the operations themselves. A program that needs the bounds check in production keeps it with `--checks`. It pays a compare and a branch per index that `opt` does not prove.

### Pitfalls

For a Java programmer:

- A release build checks no index and no cast. The checks belong to dev mode and to tests.
- A failed check ends the program. It is no exception, and no handler sees it.
- An assertion is on by default in a dev build, without a flag such as `-ea`.

For a C programmer:

- An assertion with a call in its condition is refused in a release build, so a side effect cannot vanish with it.
- The condition of an `assert` is a `bool`. `assert(p)` on a pointer is written `assert(p != none)`.
- A dev build defines what C leaves undefined: a wrong index or an overflow stops with the line and the values.

For a C++ programmer:

- The checks are instructions of the compiled function, and one option of the build removes all of them. No checked library stands behind them.
- An assertion that fails does not unwind. `defer` statements and destructors do not run.
- `assert` takes no formatted message. The message is one literal, built into the read-only data.

## Warnings, allow and unchecked

### What you know

C and C++ compilers warn under flags such as `-Wall`, turn warnings into errors with `-Werror`, and silence one with `#pragma GCC diagnostic` or `-Wno-<name>` and a name that differs per compiler. Java's `javac` warns under `-Xlint` and silences with `@SuppressWarnings("name")` on a declaration, without a reason. In all three a warning is advice, and a build goes on.

### The feature

antic reports two kinds of finding with a name. A warning points at code that compiles and is likely wrong. A safety check refuses code that would be wrong at run time, unless the program states why it is not. Each prints its name in brackets at the end of its message: `` `e` shadows the outer `e` [shadowed-catch] ``. An error has no name, and nothing silences it.

| Name | Kind | Reports | Chapter |
|---|---|---|---|
| `never-fails` | warning | a `may fail` function with no `fail` and no `try` | 6 |
| `shadowed-catch` | warning | the name a `catch` binds hides a variable of an outer scope | 6 |
| `assert-call` | warning | an `assert` condition calls a function, which a release build does not run | 6 |
| `unused-allow` | warning | an `allow` that silences nothing | 6 |
| `unused-unchecked` | warning | an `unchecked` that overrules nothing | 6 |
| `undo-never-runs` | warning | an `undo` that no failing exit runs | 4 |
| `unused-result` | warning | a dropped result of a call without effects | 5 |
| `above-vector-cap` | warning | a simd struct above 256 bytes, which is an array and a loop | 8 |
| `unfilled-abstract` | warning | an abstract class that no class of the program fills | 9 |
| `blocking-under-lock` | warning | a wait while a lock is held | 13 |
| `nothing-exported` | warning | `--lib static` of a module that exports nothing | 17 |
| `single-segment-path` | warning | a library whose module path has one segment | 18 |
| `doc-markup`, `doc-unresolved`, `doc-note-only`, `doc-dropped`, `undocumented` | warning | the doc comments, under `--doc-warnings`, and `undocumented` under `--warn-undocumented` as well | 18 |
| `exponential-pattern` | safety check | nested repeats over overlapping text in a pattern literal | 3 |
| `escaping-local` | safety check | a reference to a local that would outlive it | 11 |
| `use-after-free` | safety check | a value used after `delete`, `destroy` or a move | 11 |
| `uninitialized-read` | safety check | an element of `alloc(T, n)` read before it is written | 12 |
| `index-out-of-range` | safety check | an index out of range on every path that reaches it | 12 |
| `unguarded-field` | safety check | a field of a concurrent class that nothing guards | 14 |
| `lock-order` | safety check | two places that take the same locks in opposite orders | 14 |

A dev build prints a warning and goes on, and antic exits with 0. A release build refuses every warning as an error with the same message and name, so a program that compiles in release mode has none. `--warnings-as-errors` gives a dev build the same rule, and `anti check` passes it. The five doc warnings come with `--doc-warnings` alone, which `anti check` passes, and they stay warnings in every build. A safety check refuses the program in every build.

`allow(name, "reason")` silences a warning, and `unchecked(name, "reason")` overrules a safety check. The reason is a text that is not empty, and it stays in the source for the next reader. A clause covers a range of the source and stands at one of four levels.

| Level | Where it stands | What it covers |
|---|---|---|
| statement | on the line before a statement | that statement, a block statement with its body |
| declaration | last in the header of a function, a class or a struct, before its brace | the declaration, from its doc comment to its closing brace |
| field | after the type of a field of a class, `unchecked` alone | that field |
| file | at the top of the module, among the imports, ended by `;`, as `allow(assert-call, "reason");` | every line of the file |

A clause silences a finding of its name whose position it covers. A clause that covers none is the warning `unused-allow` or `unused-unchecked`, reported only when the check of its name ran in this build. The doc warnings run under `--doc-warnings`, `unfilled-abstract` in the build of a program, and `nothing-exported` under `--lib static`. A clause in a `tests` block goes with the block, so a build that drops the block reports nothing of it.

Both words are contextual. Before a statement, `allow(...)` followed by `;` or by more of an expression, such as `.` or an operator, is a call of a function named `allow`.

```anti
//! name: clauses
import anti.io;
import anti.text;
import anti.regex;

fn close_it(n: int) may fail
	allow(never-fails, "an implementation of the contract may fail")
{
	if n < 0 {
		return;
	}
}

fn reuse(e: int) -> int
{
	allow(shadowed-catch, "the handler reads the error alone")
	close_it(e) catch e {
		return e.code;
	};
	return e;
}

fn repeated(s: str) -> bool
{
	unchecked(exponential-pattern, "the texts are a few bytes long")
	if s.matches(re"(a+)+b") {
		return true;
	}
	return false;
}

fn main() -> int
{
	let yes = repeated("aab");
	let no = repeated("aa");
	let line = f"reuse {reuse(4)}, repeated {yes} {no}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
reuse 4, repeated true false
```

The program builds in release mode, which refuses every warning, so each clause silences a finding that is there. The clause in the header of `close_it` covers the function, the one in `reuse` the call statement, and the one in `repeated` the `if` with its pattern.

A release build refuses each of the following. The first three are warnings that a dev build prints.

```anti refused
//! error: `allow(never-fails)` silences nothing [unused-allow]
fn half(n: int) -> int
{
	allow(never-fails, "nothing here fails")
	let h = n / 2;
	return h;
}
```

```anti refused
//! error: `close_it` may fail and never does [never-fails]
fn close_it(n: int) may fail
{
	if n < 0 {
		return;
	}
}
```

```anti refused
//! error: the repeat `a+` inside `(a+)+` can take exponential time
import anti.regex;

fn repeated(s: str) -> bool
{
	if s.matches(re"(a+)+b") {
		return true;
	}
	return false;
}
```

```anti refused
//! error: `escaping-local` is no warning, and `allow` silences a warning alone
fn half(n: int) -> int
{
	allow(escaping-local, "a safety check needs unchecked")
	let h = n / 2;
	return h;
}
```

```anti refused
//! error: `never-fails` is no safety check, and `unchecked` overrules a safety check alone
fn half(n: int) -> int
{
	unchecked(never-fails, "a warning needs allow")
	let h = n / 2;
	return h;
}
```

```anti refused
//! error: the reason of `allow(never-fails)` is empty
fn close_it(n: int) may fail
	allow(never-fails, "")
{
	if n < 0 {
		return;
	}
}
```

```anti refused
//! error: `allow` stands before a statement, last in a declaration's header or at the top of the file
class Box
{
	n: int allow(never-fails, "a field takes unchecked alone"),
}
```

A name that is neither a warning nor a safety check gets the message of the wrong kind, as `no-such-name` is no warning. A clause of the whole file that stands after an item is refused with `a clause of the whole file stands at the top of the module`.

### How antic builds it

A clause changes no code, and neither does a warning. The checker writes each finding with its position and its name. A pass after the checker compares the positions with the ranges of the clauses and drops each finding that a clause of its name covers. A release build then turns every warning that is left into an error. What runs is the code that the program says, with or without the clause.

The function `close_it` keeps the form of a function that may fail, although no path fails.

```ir excerpt=clauses target=linux-x86_64 mode=dev
fn clauses.close_it(%0: i64) -> ptr {
b0:
    %1 = slt i8 %0, 0
    branch %1, b1, b2
b1:
    ret ptr 0
b2:
    ret ptr 0
}
```

Both paths return a null error pointer, `ret ptr 0`. Every caller still tests that pointer after the call, as the first section of this chapter shows. The warning `never-fails` therefore points at a compare and a branch per call that never finds an error. The `allow` keeps that cost where an implementation of an interface may fail and another does not.

`unchecked(exponential-pattern, ...)` keeps the pattern as written. PCRE2 still backtracks through it, and its match limit stops a search that runs too long, as [chapter 3]({{% relref "03-values" %}}) describes.

Where it lives: `src/antic/warnings.c` holds the one table of the names, the pass that applies the clauses, `warnings_apply`, and `warnings_promote`, which turns the warnings into errors. The parser records each clause and its range. `diagnostics_warn` of `src/antic/diagnostic.c` writes a warning, and `docs/notes/warnings.md` gives the meaning and the fix of each name.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A warning | a line on the standard error, and antic exits with 0 | the build is refused |
| A safety check | the build is refused | the same |
| A clause | no code and no data | the same |

A clause costs nothing at run time. The finding it silences may cost something, as `never-fails` does. The choice for each finding:

- Fix the code where the finding is right. Each row of the table names a defect or a cost.
- `allow` or `unchecked` where the program is right and the check cannot see why, with the reason that it can.
- `--warnings-as-errors` on a dev build, as `anti check` does, finds before a release build what that build would refuse.

### Pitfalls

For a Java programmer:

- A warning stops a release build. No release program carries a warning.
- `allow` takes a reason, and an empty one is refused. `@SuppressWarnings` takes none.
- A clause that silences nothing is itself a warning, so a stale `allow` does not stay.

For a C programmer:

- No flag selects the warnings. Every warning is on in every build, and the doc warnings come with `--doc-warnings`.
- A clause names one finding and covers one statement, declaration, field or file. No pragma turns a warning off for the rest of the file from where it stands.

For a C++ programmer:

- A safety check is not a warning. It refuses the build until an `unchecked` with a reason overrules it.
- The names are the same on every target and in every build, since one compiler gives them.
