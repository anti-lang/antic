---
title: "Hooks"
description: "Hooks and tracing, injection, runtime configuration, plugins, and what --no-hooks and --no-trace remove."
summary: "The nine hooks of `anti.lang.Object` and the tracing built on them, injection and the standard interfaces, the runtime configuration, plugins, and what `--no-hooks` and `--no-trace` remove, each with how antic builds it and what it costs."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T20:23:44+02:00
draft: true
weight: 150
tags: [compilers, programming-languages]
keywords: [hooks, tracing, dependency injection, runtime configuration, plugins, no-hooks, no-trace]
---

A hook is a function of every class that the compiler calls at a fixed moment in the life of an object. A hook site is one call of the runtime, in every build, and `--no-hooks` removes it. Tracing adds two such calls around a function. Injection is one load and one indirect call per object made. A plugin is a shared library that the runtime checks against the program before it loads it.

## Hooks

### What you know

Java has `finalize` and `Cleaner` for the end of an object, and agents that rewrite bytecode for everything else. C++ has the constructor and the destructor, and a program that wants to count its objects writes the counting into each class. C has neither, and instruments by hand or with a tool that wraps `malloc`.

### The feature

`anti.lang.Object`, the root of every class, declares nine hooks with empty bodies. A class replaces one with `concrete fn`.

| Hook | Signature in a class | When it runs | Built in |
|---|---|---|---|
| `created` | `concrete fn created(self)` | after the object is made and its `construct` has run | every build |
| `destroyed` | `concrete fn destroyed(self)` | first in the teardown of the object | every build |
| `copied` | `concrete fn copied(self, from: *Object)` | on the copy, after `dup` | every build |
| `dispatched` | `concrete fn dispatched(self)` | after `dispatch` gave the object to the pool | every build |
| `joined` | `concrete fn joined(self)` | after `join` of the job of the object | every build |
| `enter` | `concrete fn enter(self, name: str)` | first in a traced function | under tracing |
| `leave` | `concrete fn leave(self, name: str)` | at every exit of a traced function | under tracing |
| `failed` | `concrete fn failed(self, name: str, e: *lang.Error)` | before `leave`, on an exit with an error | under tracing |
| `changed` | `concrete fn changed(self, field: *FieldDescriptor)` | after a write to a field | under `--trace writes` |

The next section describes tracing and the last four hooks. This section describes the five that every build compiles.

The rules of the five:

- `created` runs for an object on the heap and for a class value in a variable. A `construct` that fails leaves no object and runs no `created`.
- `dup` runs `copied` on the new object and no `created`. A plain copy of a class value, `let b = a;`, runs no hook.
- `destroyed` runs where a teardown runs: at `delete`, and at the end of the block of a value whose class has a `destruct`, owns something or replaces `destroyed`. A class that replaces it therefore has a teardown and follows the value rules of a class that owns something: `let b = a;` is refused and `dup` copies.
- `dispatched` runs on the thread that wrote `dispatch`, also when the object is in flight and the job is empty. `joined` runs on the thread that wrote `join`.
- A hook is an ordinary function. It runs on the thread of its site, and a hook of an object that two threads use needs the care any shared function needs.

`--no-hooks` drops every site from a build, so no hook of any class runs. A program that loads plugins cannot be built with it, and neither can a plugin. The section on plugins gives the reason.

```anti
//! name: hooks
import anti.io;
import anti.text;

class Texture
{
	pub id: int = 0,

	pub static atomic live: int = 0;
	pub static atomic copies: int = 0;
	pub static atomic flights: int = 0;

	concrete fn created(self)
	{
		Texture.live.add(1);
	}

	concrete fn destroyed(self)
	{
		Texture.live.sub(1);
	}

	concrete fn copied(self, from: *Object)
	{
		Texture.live.add(1);
		Texture.copies.add(1);
	}

	concrete fn dispatched(self)
	{
		Texture.flights.add(1);
	}

	concrete fn joined(self)
	{
		Texture.flights.sub(1);
	}
}

export class Plain
{
	pub id: int = 0,
}

worker fn load(t: *Texture) -> int
{
	return t.id;
}

export fn make(id: int) -> *Plain
{
	return alloc Plain { id: id };
}

export fn drop(p: *Plain)
{
	delete(p);
}

fn show(what: str)
{
	let line = f"{what}: live {Texture.live.load()}, copies {Texture.copies.load()}, in flight {Texture.flights.load()}";
	io.println(line);
	free(line.ptr);
}

fn main() -> int
{
	let a = alloc Texture { id: 1 };
	show("alloc");
	let b = dup(a);
	show("dup");
	{
		let local = Texture { id: 3 };
		show("a value in a block");
	}
	show("after the block");
	let job = dispatch a -> load;
	show("dispatch");
	let id = join(job);
	show("join");
	delete(b);
	delete(a);
	show("delete");
	let p = make(id);
	drop(p);
	let line = f"sizes: Texture {size_of(Texture)}, Plain {size_of(Plain)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
alloc: live 1, copies 0, in flight 0
dup: live 2, copies 1, in flight 0
a value in a block: live 3, copies 1, in flight 0
after the block: live 2, copies 1, in flight 0
dispatch: live 2, copies 1, in flight 1
join: live 2, copies 1, in flight 0
delete: live 0, copies 1, in flight 0
sizes: Texture 16, Plain 16
```

A count of live objects adds `copied` to `created`, since the copy of `dup` runs `copied` and no `created`. The stock `LeakTracker` of the next section counts the same way. The value in the block is destroyed at its end, because `Texture` replaces `destroyed` and so has a teardown.

The five hooks add no field: a `Texture` with five hooks is 16 bytes, as a `Plain` with none.

The checker refuses each of the following.

```anti refused
//! error: `concrete fn created` takes 1 parameter besides `self`, and `Object.created` takes 0
class Texture
{
	pub id: int = 0,

	concrete fn created(self, extra: int) { }
}
```

```anti refused
//! error: `Texture.created` replaces `Object.created` and needs `concrete`
class Texture
{
	pub id: int = 0,

	fn created(self) { }
}
```

```anti refused
//! error: `Texture.made` is `concrete` and replaces nothing
class Texture
{
	pub id: int = 0,

	concrete fn made(self) { }
}
```

```anti refused
//! error: a struct holds fields alone, and a function belongs to a class
struct Pixel
{
	r: u8,

	concrete fn created(self) { }
}
```

### How antic builds it

The table of every class holds its descriptor at entry 0 and the seven functions of the root at entries 1 to 7. The nine hooks are entries 8 to 16. A class that replaces no hook has the address of the root's empty function in each of the nine.

A site is one call of `anti_rt_hook` with the object and the number of the hook: 0 for `created`, 1 for `destroyed`, 3 for `dispatched`. `copied` has a call of its own, `anti_rt_hook_copied`, which also takes the object the copy was made from. The module below is `make` of the example after opt.

```llvm excerpt=hooks target=linux-x86_64 mode=release
define internal noundef nonnull ptr @make(i64 noundef %p0)
b0:
  %v0 = tail call dereferenceable_or_null(16) ptr @malloc(i64 16)
...
b2:
  store ptr @anti_Plain_vtable, ptr %v0, align 8
  %v9 = getelementptr i8, ptr %v0, i64 8
  store i64 %p0, ptr %v9, align 8
  tail call void @anti_rt_hook(ptr nonnull %v0, i64 256)
  ret ptr %v0
```

- The two `store` lines write the table pointer and `id`.
- `anti_rt_hook` with 256 is the site of `created`, 0, with the bit 256 of an object that stands at the start of memory of its own on the heap. The runtime counts such an object for a loaded library its class belongs to, and a local value of a class gives 0 alone. `Plain` declares no hook, and the call stands there all the same. The compiler does not know at this site whether a handler is installed, so it writes no test of its own.

The same function built with `--no-hooks` ends after the stores.

```llvm excerpt=hooks target=linux-x86_64 mode=release option=no-hooks
b2:
  store ptr @anti_Plain_vtable, ptr %v0, align 8
  %v9 = getelementptr i8, ptr %v0, i64 8
  store i64 %p0, ptr %v9, align 8
  ret ptr %v0
```

`delete(p)` calls `anti_rt_delete`, which calls the teardown of the class through its table. The teardown of `Plain` holds the site of `destroyed` and nothing else.

```llvm excerpt=hooks target=linux-x86_64 mode=release
define internal void @hooks.Plain.destroy(
b0:
  tail call void @anti_rt_hook(ptr %p0, i64 1)
  ret void
```

`anti_rt_hook` does the following, in this order.

1. It loads the installed handler, one word of the runtime, with an atomic load.
2. It loads entry 8 plus the hook number from the table of the object and compares it with the root's empty function.
3. For `created` and `destroyed` it loads the count of open plugins. With a plugin open it finds the library the class belongs to and counts the object.
4. It calls the handler's function when a handler is installed.
5. It calls the object's own hook when the compare found one.

The site of `joined` is in the runtime, since a job holds no type of its object. `join` therefore calls `anti_rt_join_hooked`, and under `--no-hooks` the plain `anti_rt_join`.

arm64 has the same call with the object in `x0` and the number in `x1`.

Where it lives: `lower_hook_object` and `lower_hook_copied` of `src/antic/lower_hook.c`, `root_names` of `src/antic/lower_desc.c`, `enum anti_hook` of `src/rt/object.h`, `anti_rt_hook` of `src/rt/hooks.c`, and the count of `src/rt/loaded.c`.

### What it costs

The costs are the same in dev and in release.

| Cost | Count |
|---|---|
| Bytes per object | 0 |
| Bytes per class | 9 table entries of 8 bytes |
| A site | one call of the runtime |
| Inside the call, nothing attached | one atomic load, two loads and one compare. For `created` and `destroyed` one more atomic load and one compare |
| Inside the call, a handler installed | the same, and one load and one indirect call |
| Inside the call, a hook of the class | the same, and one indirect call |
| Allocations and locks | 0. With a plugin open, `created` and `destroyed` take the lock of the loaded libraries |
| Sites per object | one `created` and one `destroyed` at most, one `copied` per `dup`, one `dispatched` and one `joined` per job |
| With `--no-hooks` | 0 sites, 0 calls |

`--no-hooks` gives up the five hooks of every class of the program, every `TraceHandler`, the stock handlers of `anti.trace` and the loading of plugins.

The excerpts show the module before the link. The LTO of a release link may inline `anti_rt_hook` into a site, as [chapter 2]({{% relref "02-how-antic-works" %}}) describes.

### Pitfalls

For a Java programmer:

- `destroyed` runs at a known place, the teardown, on the thread that runs it. It is no finalizer and never runs late.
- A hook of a class is a function of its table. A subclass replaces it again, and the hook of the base does not run unless the subclass calls it.

For a C programmer:

- A class value in a variable runs `created`. It runs `destroyed` only when its class has a teardown.
- The site is a call even when no class of the program declares a hook. `--no-hooks` is the one way to remove it.

For a C++ programmer:

- `copied` is no copy constructor. It runs after `dup` has made the copy, and a plain copy of a value runs nothing.
- A hook cannot veto. `created` runs after the object exists, and a `construct` that may fail is the form that refuses an object.
- A hook that allocates an object of a class runs `created` again. A hook that counts uses atomic fields and allocates nothing.

## Tracing

### What you know

Java profilers attach through JVM TI or rewrite bytecode at load, and the program pays when an agent is attached. C and C++ compile with `-finstrument-functions`, which calls two functions of the program around every function. Both are switched outside the source.

### The feature

Tracing is four more hooks, a mark that says which code calls them, and one handler that receives every hook of every object.

`trace` is a contextual word in two places.

| Written | Meaning |
|---|---|
| `trace class Renderer { }` | every `pub` function with `self` of the class is traced |
| `pub trace fn marked(self)` | the one function `marked` of a class is traced |

A traced function calls `enter` of its object first and `leave` at every exit: `return`, `fail`, the error a `try` forwards and the closing brace. An exit with an error calls `failed` before `leave`. The three receive the name of the function as a `str`, `module.Class.function`. The nine hooks themselves are never traced.

Marked code is compiled with its calls in a dev build and without them in a release build. Four options of antic decide otherwise.

| Option | Effect |
|---|---|
| `--trace` | marked code is traced, in any mode |
| `--no-trace` | marked code is not traced, in any mode |
| `--trace <pattern>` | a class the pattern names is traced as well: a class, its module path or a package above it |
| `--trace writes` | every write to a field of a traced class calls `changed` |

The build of the module that holds a function decides for that function. A library compiled without tracing stays untraced in a program built with `--trace`.

`anti.lang.TraceHandler` is an abstract class that declares the same nine hooks with the object after `self`. A class that inherits it is a handler, and it declares all nine. `lang.Trace.install(h)` stores one handler for the whole program, and `lang.Trace.install(none)` removes it.

Every hook site calls the installed handler and then the object's own hook. `leave` runs the two in the other order, so the handler and the hook of the object nest around the call. A class that replaced a hook does not silence the handler, and the handler does not replace the hook of the class.

`anti.trace` holds seven handlers.

| Handler | Reads | Keeps |
|---|---|---|
| `LeakTracker` | `created`, `copied`, `destroyed` | `made()`, `freed()`, `live()` |
| `Profiler` | `enter`, `leave` | calls and nanoseconds per function name, for 64 names |
| `CallLogger` | `enter`, `leave`, `failed` | a line per call, indented by depth |
| `ErrorMonitor` | `failed` | the count, and the code, message and function of the last failure |
| `ThreadMonitor` | `dispatched`, `joined` | jobs sent, taken, running and the peak |
| `ChangeJournal` | `changed` | the last 64 writes, as class and field |
| `Composite` | all nine | up to 8 handlers, called in order |

Each has `new()` and, apart from `Composite` and `CallLogger`, a `report()` that prints to standard error. `trace.install("leaks,profile")` builds the handlers a text names and installs them. The names are `leaks`, `profile`, `calls`, `errors`, `threads` and `writes`. A name past the eighth of a list writes a line to standard error and is left out, as an unknown name is. `trace.start()` does the same with the runtime key `trace`, so `--anti.trace=leaks` chooses the handler when the program starts. The program makes the call, and no handler is installed without it.

```anti
//! name: traced
import anti.io;
import anti.text;
import anti.trace;

trace class Renderer
{
	pub frames: int = 0,

	pub fn draw(self, n: int) -> int
	{
		self.frames += n;
		return self.scale(self.frames);
	}

	pub fn check(self, n: int) -> int may fail
	{
		if n < 0 {
			fail "below zero";
		}
		return n;
	}

	fn scale(self, n: int) -> int
	{
		return n * 2;
	}
}

class Quiet
{
	pub n: int = 0,

	pub fn plain(self) -> int
	{
		return self.n;
	}

	pub trace fn marked(self) -> int
	{
		return self.n + 1;
	}
}

class Count inherits lang.TraceHandler
{
	pub atomic made: int = 0,
	pub atomic entered: int = 0,
	pub atomic left: int = 0,
	pub atomic failures: int = 0,
	pub atomic writes: int = 0,

	concrete fn created(self, o: *Object) { self.made.add(1); }
	concrete fn destroyed(self, o: *Object) { }
	concrete fn copied(self, o: *Object, from: *Object) { }
	concrete fn dispatched(self, o: *Object) { }
	concrete fn joined(self, o: *Object) { }
	concrete fn enter(self, o: *Object, name: str) { self.entered.add(1); }
	concrete fn leave(self, o: *Object, name: str) { self.left.add(1); }
	concrete fn failed(self, o: *Object, name: str, e: *lang.Error) { self.failures.add(1); }
	concrete fn changed(self, o: *Object, field: *FieldDescriptor) { self.writes.add(1); }
}

fn main() -> int
{
	let count = alloc Count { };
	lang.Trace.install(count);
	let r = alloc Renderer { };
	let q = alloc Quiet { };
	let total = r.draw(1) + r.draw(2) + q.plain() + q.marked();
	let bad = 0;
	r.check(-1) catch {
		bad = 1;
	};
	lang.Trace.install(none);
	let line = f"total {total}, bad {bad}";
	io.println(line);
	free(line.ptr);
	line = f"made {count.made.load()}, entered {count.entered.load()}, left {count.left.load()}, failed {count.failures.load()}, changed {count.writes.load()}";
	io.println(line);
	free(line.ptr);

	let leaks = trace.LeakTracker.new();
	lang.Trace.install(leaks);
	let extra = alloc Quiet { };
	let copy = dup(extra);
	delete(copy);
	lang.Trace.install(none);
	line = f"leaks: made {leaks.made()}, freed {leaks.freed()}, live {leaks.live()}";
	io.println(line);
	free(line.ptr);
	delete(extra);
	delete(leaks);
	delete(q);
	delete(r);
	delete(count);
	return 0;
}
```

```text output
total 9, bad 1
made 3, entered 0, left 0, failed 0, changed 0
leaks: made 2, freed 1, live 1
```

The test runs the release build, where marked code is not traced: `entered` is 0. The handler still receives `created` three times, for the `Renderer`, the `Quiet` and the error of `check`. The five hooks of the last section are in every build.

The same file built with other options prints another second line. Each line below is the output of that build.

| Build | Second line |
|---|---|
| `antic --trace` | `made 3, entered 4, left 4, failed 1, changed 0` |
| `antic --trace --trace writes` | `made 3, entered 4, left 4, failed 1, changed 2` |
| `anti run`, a dev build | `made 4, entered 4, left 4, failed 1, changed 0` |

The four calls are two of `draw`, one of `marked` and one of `check`. `scale` is private and `plain` is unmarked, so neither is traced. The two writes are `self.frames += n`. The dev build makes one object more, the stack trace of the error, which [chapter 6]({{% relref "06-errors" %}}) describes.

The checker refuses each of the following.

```anti refused
//! error: `Watch` lacks `concrete fn destroyed`
class Watch inherits lang.TraceHandler
{
	concrete fn created(self, o: *Object) { }
}
```

```anti refused
//! error: expected `?*TraceHandler`, found an integer literal
fn run()
{
	lang.Trace.install(5);
}
```

```anti refused
//! error: `trace` marks a class or a function of one
trace fn run() { }
```

### How antic builds it

A traced function holds one call of `anti_rt_hook_call` before its first statement and one as an exit action of a scope around its body. The module below is `draw` of the example built in release mode with `--trace`.

```llvm excerpt=traced target=linux-x86_64 mode=release option=trace
define internal noundef range(i64 0, -1) i64 @traced.Renderer.draw(ptr noundef nonnull dereferenceable(16) %p0, i64 noundef %p1)
b0:
  tail call void @anti_rt_hook_call(ptr nonnull %p0, i64 5, ptr nonnull @traced.59, i64 20)
  %v3 = getelementptr inbounds nuw i8, ptr %p0, i64 8
  %v5 = load i64, ptr %v3, align 8
  %v8 = add i64 %v5, %p1
  store i64 %v8, ptr %v3, align 8
  %v2.i = shl i64 %v8, 1
  tail call void @anti_rt_hook_call(ptr nonnull %p0, i64 6, ptr nonnull @traced.59, i64 20)
  ret i64 %v2.i
```

- The first call is `enter`, hook 5. It passes the object, and the name as a pointer and a length: the 20 bytes of `traced.Renderer.draw`, a constant of the read-only data.
- The four lines after it are the body. The private `scale` is inlined as the shift, with no call of its own.
- The second call is `leave`, hook 6, with the same name.

The release build without the option holds the body alone.

```llvm excerpt=traced target=linux-x86_64 mode=release
define internal noundef range(i64 0, -1) i64 @traced.Renderer.draw(
b0:
  %v1 = getelementptr inbounds nuw i8, ptr %p0, i64 8
  %v3 = load i64, ptr %v1, align 8
  %v6 = add i64 %v3, %p1
  store i64 %v6, ptr %v1, align 8
  %v2.i = shl i64 %v6, 1
  ret i64 %v2.i
```

A dev build holds the two calls, as the build with `--trace` does.

```llvm excerpt=traced target=linux-x86_64 mode=dev
define hidden noundef i64 @traced.Renderer.draw(ptr noundef nonnull dereferenceable(16) %p0, i64 noundef %p1) #0 {
...
  call void (ptr, i64, ptr, i64) @anti_rt_hook_call(
```

`anti_rt_hook_call` loads the handler and the entry of the object as `anti_rt_hook` does. For `enter` it calls the handler and then the hook of the object, and for `leave` the hook of the object and then the handler. `failed` is `anti_rt_hook_failed`, which also takes the error. `changed` is `anti_rt_hook_changed` after the store, with the record of the field from the descriptor of the class.

`leave` is an exit action and no call at each `return`, so every exit runs it. Its scope is opened around the body, so it runs after the locals of the body are torn down.

The handler is one word of the runtime, written and read atomically. `Trace.install` is a call of `anti_rt_trace_install`, which stores the pointer. The functions of a handler are entries 17 to 25 of its table, after the sixteen of the root.

The stock handlers allocate nothing inside a hook. Each table they keep is a field of the handler with a size fixed at compile time. The counts of `LeakTracker`, `CallLogger` and `ThreadMonitor` and the totals of `ErrorMonitor` and `ChangeJournal` are atomic fields. The tables of the `Profiler` and of the `ChangeJournal` and the last error of the `ErrorMonitor` are plain fields, and no lock guards them.

Where it lives: `traced_function` of `src/antic/lower_function.c`, `lower_traced_class`, `lower_hook_call`, `lower_hook_failed` and `lower_hook_changed` of `src/antic/lower_hook.c`, `anti_rt_hook_call` and `anti_rt_trace_install` of `src/rt/hooks.c`, and `src/std/anti/trace.anti`.

### What it costs

| Cost | Marked code traced | Marked code not traced |
|---|---|---|
| Calls per call of a traced function | two of `anti_rt_hook_call`, and one of `anti_rt_hook_failed` on an exit with an error | 0 |
| Inside each call, nothing attached | one atomic load, two loads and one compare | 0 |
| Inside each call, a handler installed | the same, and one load and one indirect call | 0 |
| A write under `--trace writes` | one call of `anti_rt_hook_changed` per store to a field | 0 |
| Bytes in the program | the name of each traced function, once | 0 |
| Bytes per object | 0 | 0 |
| Allocations and locks | 0 | 0 |

Marked code is traced in a dev build and with `--trace`. It is not traced in a release build and with `--no-trace`.

A traced function cannot be inlined across its two calls of the runtime as freely as a plain one. Its object must be in memory at both calls. `--no-trace` on a dev build gives the plain code back and gives up `enter`, `leave` and `failed` of every class.

`--no-hooks` removes these sites as well, with the five of the last section.

The `Profiler` adds a read of the clock in `enter` at depth 0 and one in `leave`, and a search of its 64 names by comparing texts.

### Pitfalls

For a Java programmer:

- Tracing is compiled in or out. No agent attaches to a running release build, and a handler installed there receives the five lifecycle hooks alone.
- One handler is installed at a time. A second `install` replaces the first, and `Composite` is the form for more than one.

For a C programmer:

- `trace class` traces the `pub` functions with `self`. A private function and a function without `self` are not traced unless `trace fn` marks them.
- A library keeps the tracing of its own build. `--trace` on the program does not reach the bodies of a library file.

For a C++ programmer:

- `leave` runs after the locals of the function are gone. A handler that reads the object in `leave` reads it after the teardowns of the body.
- The name a hook receives is a `str` into read-only data and lives as long as the program. The error `failed` receives is deleted when its `catch` ends, so a handler copies what it keeps.
- A handler runs on every thread that reaches a site, so a handler of the program counts with atomic fields. The tables of the stock `Profiler` are plain fields.

## Injection

### What you know

Java frameworks such as Spring and Guice fill annotated fields at run time by reflection, from a configuration read at start. C and C++ pass a pointer to an interface by hand, or choose an implementation at link time. The first costs a start-up scan, and the second writes the wiring into every constructor.

### The feature

A class marks a field `inject`, and the provider of the interface fills it when an object is made.

| Written | Meaning |
|---|---|
| `inject log: *Logger,` | the provider of `Logger` fills the field before `construct` runs |
| `inject final from: *Allocator,` | the same, and the runtime configuration may not replace the provider |

The type of the field is a pointer to an abstract class, the interface. The field takes no default and no `own`, and a literal of the class does not name it.

A provider is a function without parameters that gives a pointer of the interface. A build finds the provider of an interface in this order.

1. `--inject Interface=Provider` on the command line of antic. `anti build` and `anti test` pass one per line of the `[inject]` table of `anti.toml`, and `anti test` lays `[inject.test]` over it.
2. A static function `default` of the interface itself.

`Provider` is the path of a function of the program, or the path of a singleton, which names its `get`. An interface with neither is refused at the link, with a message that names the class and the field.

```toml
[inject]
"anti.log.Logger" = "net.example.ConsoleLogger"

[inject.test]
"anti.log.Logger" = "net.example.tests.FakeLogger"
```

One slot per interface holds the provider for the whole program. Every object asks the slot, so two objects that inject one interface receive what the same provider gives.

Six interfaces of the standard library carry a `default`, so a field of one needs no line in the manifest.

| Interface | Default implementation |
|---|---|
| `anti.log.Logger` | `SinkLogger` |
| `anti.time.Clock` | `SystemClock` |
| `anti.random.Source` | `SharedRandom` |
| `anti.fs.FileSystem` | `SystemFileSystem` |
| `anti.mem.Allocator` | `LibcAllocator` |
| `anti.config.Config` | `FileConfig` |

The runtime configuration may replace a provider with a class of a plugin when the program starts. The section on plugins describes it. An `inject final` field refuses that replacement.

A program may assign an `inject` field after the object is made, as any pointer field.

```anti
//! name: wiring
import anti.io;
import anti.text;
import anti.mem;

abstract class Greeter
{
	abstract fn greeting(self) -> str;

	pub fn default() -> *Greeter
	{
		return Console.get();
	}
}

singleton class Console inherits Greeter
{
	concrete fn greeting(self) -> str
	{
		return "console";
	}
}

class Plain
{
	pub calls: int = 0,
}

class Service
{
	pub inject out: *Greeter,
	inject final from: *mem.Allocator,
	pub calls: int = 0,

	pub fn run(self) -> str
	{
		self.calls += 1;
		return self.out.greeting();
	}

	pub fn room(self, n: int) -> bool
	{
		let p = self.from.alloc(n, 8) else {
			return false;
		};
		self.from.free(p);
		return true;
	}
}

fn main() -> int
{
	let s = alloc Service { };
	let t = Service { calls: 5 };
	let line = f"greeting {s.run()}, room {s.room(64)}, same provider {s.out == t.out}";
	io.println(line);
	free(line.ptr);
	line = f"sizes: Plain {size_of(Plain)}, Service {size_of(Service)}";
	io.println(line);
	free(line.ptr);
	delete(s);
	return 0;
}
```

```text output
greeting console, room true, same provider true
sizes: Plain 16, Service 32
```

`Service` names neither `Console` nor `LibcAllocator`. `Greeter.default` gives the one `Console`, and `anti.mem.Allocator` carries its own default. Both objects hold the same `Greeter`, since both asked the same slot.

The compiler refuses each of the following. The first three are refused by the checker and the last two at the link.

```anti refused
//! error: `inject` needs a pointer to an abstract class, and `p` has type `*Plain`
class Plain { pub n: int = 0, }

class Service
{
	inject p: *Plain,
}
```

```anti refused
//! error: `out` is `inject`, so its provider fills it and it has no default
abstract class Greeter { abstract fn greeting(self) -> str; }

class Service
{
	inject out: *Greeter = none,
}
```

```anti refused
//! error: `out` is `inject`, and its provider fills it
abstract class Greeter
{
	abstract fn greeting(self) -> str;

	pub fn default() -> *Greeter { return Console.get(); }
}

singleton class Console inherits Greeter
{
	concrete fn greeting(self) -> str { return "console"; }
}

class Service
{
	pub inject out: *Greeter,
}

fn build(g: *Greeter) -> int
{
	let s = Service { out: g };
	return 0;
}
```

```anti refused
//! error: has no provider
abstract class Greeter { abstract fn greeting(self) -> str; }

singleton class Console inherits Greeter
{
	concrete fn greeting(self) -> str { return "console"; }
}

class Service
{
	inject out: *Greeter,
}

fn main() -> int
{
	let s = alloc Service { };
	delete(s);
	return 0;
}
```

```anti refused
//! error: the providers make a cycle
abstract class First
{
	abstract fn one(self) -> int;

	pub fn default() -> *First { return A.get(); }
}

abstract class Second
{
	abstract fn two(self) -> int;

	pub fn default() -> *Second { return B.get(); }
}

singleton class A inherits First
{
	inject other: *Second,

	concrete fn one(self) -> int { return 1; }
}

singleton class B inherits Second
{
	inject other: *First,

	concrete fn two(self) -> int { return 2; }
}

class Service
{
	inject first: *First,
}

fn main() -> int
{
	let s = alloc Service { };
	delete(s);
	return 0;
}
```

### How antic builds it

The slot is a global of the runtime module, one pointer, named `anti.rt.inject.` and the path of the interface. The pass over the whole program writes the provider into it at the link.

```llvm excerpt=wiring target=linux-x86_64 mode=release
@anti.rt.inject.wiring.Greeter = global <{ ptr }> <{ ptr @wiring.Greeter.default }>, align 8
@anti.rt.inject.anti.mem.Allocator = global <{ ptr }> <{ ptr @anti.mem.Allocator.default }>, align 8
```

An `inject` field is filled where the defaults of the class are written: in a literal, and in the function that prepares an object for `construct`. The lines below are `alloc Service { }` in `main` of the example.

```llvm excerpt=wiring target=linux-x86_64 mode=release
  store ptr @wiring.Service.table, ptr %v0, align 8
  %v9 = getelementptr i8, ptr %v0, i64 8
  %v11 = load ptr, ptr @anti.rt.inject.wiring.Greeter, align 8
  %v13 = tail call ptr %v11()
  store ptr %v13, ptr %v9, align 8
  %v17 = getelementptr i8, ptr %v0, i64 16
  %v19 = load ptr, ptr @anti.rt.inject.anti.mem.Allocator, align 8
  %v21 = tail call ptr %v19()
  store ptr %v21, ptr %v17, align 8
  %v25 = getelementptr i8, ptr %v0, i64 24
  store i64 0, ptr %v25, align 8
  tail call void @anti_rt_hook(ptr nonnull %v0, i64 256)
```

- The first `load` reads the slot of `Greeter`, the call runs the provider through it, and the `store` writes the result into `out` at offset 8.
- The next three lines do the same for `from` at offset 16.
- `calls` at offset 24 is a plain default, and the hook `created` follows.

The call is indirect although the slot is filled at the link. The slot is a writable global, so the runtime may store another provider before `main`.

The field is an ordinary pointer after that. `self.out.greeting()` is the call through an interface of [chapter 9]({{% relref "09-classes" %}}), with no further load of the slot.

`get` of a singleton makes its object at the first call, with a `compare_swap`, and gives the same object after it.

The link checks the graph of the providers. It walks the direct calls of each provider and draws an edge to every interface whose slot they read. A cycle is refused. A call through a table is not followed, so the check misses a cycle through one.

Every program carries `anti_rt_injectable`, a list of the interfaces it injects with the class and the field of each. The runtime reads it when the program starts, for `--anti.inspect` and for the replacements.

Where it lives: the `inject` marks in `src/antic/parser_item.c`, the fill in the lowering of a literal, `whole_write_injections` and `resolve_provider` of `src/antic/whole_tables.c`, and the `[inject]` table in `src/anti/manifest.c`.

### What it costs

The costs are the same in dev and in release.

| Cost | Count |
|---|---|
| Bytes per `inject` field | 8, one pointer. `Service` is 32: the table pointer, two fields and `calls` |
| Bytes per interface | one slot of 8 bytes, and one record of the list the runtime reads |
| Per object made | one load of the slot, one indirect call of the provider and one store, per `inject` field |
| Inside a provider that is `get` of a singleton | one atomic load, after the first call |
| A call through the field | the dispatch of an interface, as for any pointer to an abstract class |
| Allocations | 0 by the injection. The provider decides its own |
| At the start of the program | one read of the key `plugins` under the lock of the configuration, and one pass over the list of the injectable interfaces. A program without replacements stores nothing |

A class with three `inject` fields runs three providers for every object. A provider that builds a new object each time builds one per field and per object, and nothing frees it for the class.

### Pitfalls

For a Java programmer:

- The wiring is fixed when the program is linked. No container exists at run time, and no scope such as request or session.
- A provider returns what it wants, each time it is asked. A provider that gives one shared object is `get` of a singleton.

For a C programmer:

- The field is a plain pointer. The class does not own what it holds and never frees it.
- The slot is per interface, for the whole program. Two classes cannot have two providers of one interface.

For a C++ programmer:

- The provider runs before `construct`, for every object, also for one that `construct` then refuses.
- A value of the class in a variable runs the providers as an object on the heap does.
- An interface with no provider fails the link of the program, with a message. A library that injects compiles without one.

## Runtime configuration

### What you know

Java reads `-D` properties and the environment, and each library adds a file of its own. A C or C++ program parses `argv` with `getopt` and calls `getenv`. The settings of the runtime of a language, such as the thread count of a pool, are scattered over environment variables.

### The feature

The runtime of a program has six keys, and three layers give a key its value.

| Key | Value | Read by |
|---|---|---|
| `backtrace` | `true` or `false` | the capture of the frames of an error, [chapter 6]({{% relref "06-errors" %}}) |
| `hash_seed` | a number | the hashing collections, [chapter 16]({{% relref "16-collections" %}}) |
| `logger` | a path | `anti.log`, the file of its sinks |
| `plugins` | directories, separated by `:` | the discovery of plugins |
| `threads` | a count from 1 | the worker pool, [chapter 13]({{% relref "13-threads" %}}) |
| `trace` | names of handlers | `trace.start()` of `anti.trace` |

The layers, the strongest first:

1. The command line of the program. `--anti.threads=4` sets the key `threads`. The runtime takes every argument that starts with `--anti.` before `main`, so `main` does not see it.
2. The configuration file. `--anti.conf=<path>` names it, or the environment variable `ANTI_CONF`, or a call of `rt.configure(path)` of `anti.runtime`.
3. The build. A key that no layer set has no value, and its reader keeps its own default.

The file is TOML with three parts.

```toml
include = ["base.toml"]

[runtime]
threads = 2

[injections]
"anti.log.Logger" = "plugins/libfancy.so"
```

`include` names files that are read first, in order, relative to the file that names them. The including file wins per key. `[runtime]` holds the six keys. `[injections]` replaces the provider of an interface with a library, which the section on plugins describes.

Three more options are the runtime's own.

| Option | Effect |
|---|---|
| `--anti.help` | prints the options and ends the program |
| `--anti.inspect` | prints the value of each key with the layer it came from, the injectable interfaces and the loaded plugins, and ends |
| `--anti.inject=<interface>=<path>` | replaces one provider, over the file |

`rt.get(key)` gives the effective value of a key as a `?str`, or `none` where no layer set it.

The runtime refuses each of the following with a message on standard error and the exit status 70.

- An option under `--anti.` that it does not know. The message lists the ones it has.
- A value a key does not take, such as `--anti.threads=many`.
- A key in the file outside `include`, `[runtime]` and `[injections]`, and a key of `[runtime]` that is none of the six.
- A file that cannot be read, and a cycle of includes.
- `[injections]` in a file that `rt.configure` reads, or in a file it includes. The providers are fixed before `main`, and `rt.configure` runs after it.
- A line of `[injections]` that names no injectable interface of the program, or an `inject final` one.

```anti
//! name: settings
import anti.io;
import anti.text;
import anti.runtime as rt;

fn show(key: str)
{
	let value = rt.get(key) ?? "(not set)";
	let line = f"{key}: {value}";
	io.println(line);
	free(line.ptr);
}

fn main() -> int
{
	show("threads");
	show("trace");
	show("backtrace");
	show("colour");
	let line = f"sizes: ?str {size_of(?str)}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
threads: (not set)
trace: (not set)
backtrace: (not set)
colour: (not set)
sizes: ?str 24
```

The test runs the program with no argument and no file, so no key has a value. `colour` is no key, and `rt.get` gives `none` for it as well.

The same program with two options prints the values the command line set.

```sh
./settings --anti.threads=4 --anti.trace=leaks
```

```text
threads: 4
trace: leaks
backtrace: (not set)
colour: (not set)
sizes: ?str 24
```

With the file `conf/app.toml` above, a `conf/base.toml` that sets `threads = 8` and `trace = "leaks"`, and one option, `--anti.inspect` prints where each value came from.

```sh
./settings --anti.conf=conf/app.toml --anti.threads=3 --anti.inspect
```

```text
anti runtime 0.2.0
backtrace =  (build)
hash_seed =  (build)
logger =  (build)
plugins =  (build)
threads = 3 (command line)
trace = leaks (conf/base.toml)
injectable interfaces: none
loaded plugins: none
```

`threads` is 3 from the command line, over the 2 of `app.toml` and the 8 of `base.toml`. `trace` comes from the included file, which the including file did not overrule.

### How antic builds it

The compiler builds nothing for the configuration. It is code of the runtime that runs before `main`, in every program.

The start of the runtime walks the arguments once. An argument that does not start with `--anti.` moves down into the slice `main` receives. Each other one goes to the table of the keys with its name and the text after the first `=`. The file of `--anti.conf` or of `ANTI_CONF` is read after the walk, with the TOML reader of the runtime, the includes depth first.

The table holds one entry per key: the value, the layer that set it and the file of a value from a file. A write is dropped when the entry already stands at a stronger layer. Two writes of the file layer overwrite each other, so a later include wins over an earlier one and the including file over both.

`rt.get` is a call of `anti_rt_conf_get`. It takes one lock of the runtime, finds the key by comparing its name with the six, and gives the text of the value.

```llvm excerpt=settings target=linux-x86_64 mode=release
  %v9.i = tail call { i64, i64 } @anti_rt_conf_get(ptr %0, i64 range(i64 5, 10) %p0.1)
```

The line stands in `show` of the example, with `rt.get` inlined. The call passes the key as a pointer and a length and returns the value as a `str` in two registers. `range(i64 5, 10)` is what opt knows of the length: the four keys of the example have 5 to 9 bytes. The text of the value belongs to the runtime and stays valid until the program ends.

Each reader asks at its own time. The pool reads `threads` when it starts, at the first `dispatch` or `parallel`. `anti.log` reads `logger` when its default logger is built. `trace.start()` reads `trace` when the program calls it.

Where it lives: `src/rt/start.c` for the walk of the arguments, `src/rt/conf.c` for the table, the file and `--anti.inspect`, `src/rt/toml.c` for the reader, and `src/std/anti/runtime.anti`.

### What it costs

The costs are the same in dev and in release.

| Cost | Count |
|---|---|
| At the start of every program | one walk of the arguments, and one read of the environment for `ANTI_CONF` |
| With a file | one read and one parse per file, includes among them |
| `rt.get(key)` | one call of the runtime, one lock, up to six comparisons of text |
| A reader of a key | one such call, when it starts |
| Bytes in the program | the table of six keys, the option texts and the TOML reader, in every program |
| Checks | every option and every key of a file, when the program starts |

No option of antic builds a program without the configuration.

### Pitfalls

For a Java programmer:

- A key has one name in every layer. `threads` is `--anti.threads` on the command line and `threads` under `[runtime]`, and no environment variable per key exists.
- An unknown key is an error at the start. A mistyped key does not pass silently.

For a C programmer:

- `main` never sees an `--anti.` argument. A program cannot define an option of its own under that prefix.
- `rt.configure` after the pool has started does not change the pool. Each reader reads its key once.

For a C++ programmer:

- `ANTI_CONF` is the one environment variable the runtime reads. Every other setting is a key.
- The command line wins over the file, also for a file that `rt.configure` reads later in the run.

## Plugins

### What you know

Java loads classes at run time, finds implementations with `ServiceLoader` and checks a class when it links it. C and C++ call `dlopen` and `dlsym` and cast the result. Nothing checks that the library was built against the same header, and a struct that grew by a field corrupts memory at the first call.

### The feature

A plugin is a shared library that provides classes for interfaces of the program that loads it. That program is the host.

The library says what it offers with one line at module level per interface.

```anti
import anti.io;
import anti.log;
import anti.text;

provides anti.log.Logger as FancyLogger;

pub class FancyLogger
{
	implements logger: log.Logger,
	lines: int = 0,

	pub concrete fn log(self, level: log.Level, message: str)
	{
		self.lines += 1;
		let line = f"fancy {self.lines}: {message}";
		io.println(line);
		free(line.ptr);
	}
}
```

The interface of a `provides` line is an abstract class. The class is declared in the same module, inherits the interface or implements it, and is neither abstract nor a singleton. An interface is provided once per library.

`antic --lib shared --no-runtime` writes the library. It holds the code of its own module alone. It carries no runtime and no copy of the standard library. Every name it uses of them is left for the host to supply when it loads the library.

```sh
antic --lib shared --no-runtime -I . -o plugins/libfancy.so acme/fancy.anti
```

antic writes `anti-plugins.toml` beside the library: the path, the runtime version, the SHA-256 digest and the interfaces of each library of the directory.

```toml
[[library]]
path = 'libfancy.so'
runtime = '0.2.0'
digest = 'a458b6eeb5aced91928e77c6556353561f4af4506be5f76d51a345fd9aa40b3f'
interfaces = ['anti.log.Logger']
```

A host reaches a library by hand or as a provider.

| Way | Written |
|---|---|
| By hand | `plugin.load(path)` of `anti.plugin`, then `lib.instance(Interface)` |
| As a provider, by path | `"anti.log.Logger" = "plugin:plugins/libfancy.so"` under `[inject]` of the manifest |
| As a provider, by discovery | `"anti.log.Logger" = "discover"` under `[inject]`, and the key `plugins` names the directories |

`[injections]` of the configuration file and `--anti.inject` replace a provider with a library when the program starts, without a rebuild.

The functions of a `Library`:

| Function | Gives |
|---|---|
| `plugin.load(path)` | a `*Library`, or fails |
| `lib.instance(Interface)` | a new object of the class the library provides, as a `?*Interface` |
| `lib.supports(Interface, "f")` | whether the class of the library has the function `f` |
| `lib.live()` | the count of objects of the library on the heap that are alive |
| `lib.unload()` | closes the library, or fails with `IN_USE` while an object of it is alive |

```anti
//! name: host
import anti.io;
import anti.log;
import anti.plugin;
import anti.text;

fn speak(path: str) may fail
{
	let lib = try plugin.load(path);
	let logger = lib.instance(log.Logger) else {
		fail "the library provides no Logger";
	};
	logger.log(log.Level.Info, "first");
	logger.log(log.Level.Info, "second");
	let known = lib.supports(log.Logger, "log");
	let line = f"supports log: {known}, alive: {lib.live()}";
	io.println(line);
	free(line.ptr);
	lib.unload() catch e {
		io.println(e.message);
	};
	delete(logger);
	try lib.unload();
	delete(lib);
}
```

A host that calls `speak` with the path of the library above prints the four lines below. This is the output of the two builds on macos-arm64, with the library as `plugins/libfancy.dylib`.

```text
fancy 1: first
fancy 2: second
supports log: true, alive: 1
1 object of the library is alive
```

The first `unload` fails, since `logger` is alive. The second one closes the library.

The load checks the library before any code of it runs.

- The runtime version of the library equals the host's exactly.
- Every interface the library provides is the host's own. The library was bound against this host and carries no copy of the interface.
- The version of each interface fits. Every class carries the version of its package, `--package-version` of the build and `0.0.0` without one. An abstract class carries one hash per prefix of its table. The load compares the hashes of the host and of the library at the length of the shorter table.
- No field was added to the interface between the two versions.
- No slot that a call of the host reaches is missing in the library. A slot that `reflect.call` alone may reach is filled with a stub that names the plugin.

`compatible 1.1;` in the body of an abstract class names the oldest version of a plugin the interface accepts.

The classes of a loaded library join the registry of the host, so `reflect.new` builds one by name.

A program that can load a library exports its own functions and data, so that the library can bind against them. `--closed` builds a program without those exports. A closed program loads no plugin, and `--inject` of a library path is refused for it.

On Linux a host links dynamically against glibc, since a static program of musl has no loader. On Windows the link of a host writes an import library `<program>.lib`, and a plugin is linked against the import library of its host.

The compiler refuses each of the following.

```anti refused
//! error: `Plain` is not abstract, and a `provides` line names an interface
class Plain { pub n: int = 0, }

provides Plain as Plain;
```

```anti refused
//! error: the module declares no class `Missing`
abstract class Greeter { pub abstract fn hi(self) -> str; }

class Loud inherits Greeter
{
	pub concrete fn hi(self) -> str { return "HI"; }
}

provides Greeter as Missing;
```

```anti refused
//! error: `Plain` neither inherits `Greeter` nor implements it
abstract class Greeter { pub abstract fn hi(self) -> str; }

class Loud inherits Greeter
{
	pub concrete fn hi(self) -> str { return "HI"; }
}

class Plain { pub n: int = 0, }

provides Greeter as Plain;
```

```anti refused
//! error: `Console` is a singleton, and a `provides` line names a class the host builds
abstract class Greeter { pub abstract fn hi(self) -> str; }

singleton class Console inherits Greeter
{
	pub concrete fn hi(self) -> str { return "hi"; }
}

provides Greeter as Console;
```

```anti refused
//! error: `Greeter` is provided twice
abstract class Greeter { pub abstract fn hi(self) -> str; }

class Loud inherits Greeter
{
	pub concrete fn hi(self) -> str { return "HI"; }
}

class Soft inherits Greeter
{
	pub concrete fn hi(self) -> str { return "hi"; }
}

provides Greeter as Loud;
provides Greeter as Soft;
```

```anti refused
//! error: `Plain` is not abstract, and a library provides an interface
import anti.plugin;

class Plain { pub n: int = 0, }

fn take(lib: *plugin.Library) -> bool
{
	let p = lib.instance(Plain) else {
		return false;
	};
	return true;
}
```

```anti refused
//! error: `compatible` names the versions a plugin may carry, and belongs to an abstract class
class Plain
{
	compatible 1.1;
	pub n: int = 0,
}
```

A host built with `--no-hooks` is refused at its link with the message below, and a plugin is refused the same way.

```text
host.anti: error: the program loads libraries, and `--no-hooks` drops the hooks that count their objects for `unload`; build it without `--no-hooks`, or with `--closed`
```

### How antic builds it

A build with `--no-runtime` emits the object of the module alone, as a dev build does, and links it with no runtime library. The pass over the whole program writes one table into it, `anti_rt_provides`. It holds the runtime version, the classes of the library and one entry per `provides` line. An entry is the path of the interface and the descriptors of the interface and of the class. It also holds the function that prepares an object and the offset of the interface inside the class.

The library reaches the host through the loader of the system. On Linux and Windows a plugin reaches every function and every datum of the host through an entry the loader fills. That entry is in the GOT on ELF, and among the `__imp_` pointers of the import library on COFF. On macOS the link leaves the names undefined with `-undefined dynamic_lookup`, and the loader binds them.

The host is built to be bound against. Its functions and data are global and none is hidden. The link passes `-export_dynamic` on macOS and `--export-dynamic` on Linux, and writes a `.def` file and an import library on Windows.

`plugin.load` opens the library, reads `anti_rt_provides` and runs the checks of the list above. That every interface descriptor lies in the image of the host is the proof that the library was bound against it.

`lib.instance(Interface)` is rewritten by the checker into a call that passes the address of the descriptor of the interface. The lines below are that call in `speak` of the host above, in a dev build.

```ir excerpt=host target=linux-x86_64 mode=dev
    %6 = addr @anti.log.Logger.descriptor
...
    %12 = load ptr %11 !entry
    %13 = call ptr %12 via @host.fn.0(%5, %6) table @anti.plugin.Library.descriptor 17
    %14 = copy ptr %13
    %15 = eq i8 %14, 0
    branch %15, b5, b6
```

- `addr @anti.log.Logger.descriptor` stands where the source names the type `log.Logger`. It is the descriptor of the interface in the host, the same address the loader compared at `plugin.load`.
- The call goes through entry 17 of the table of the `Library`. That entry is `instance_at` of `anti.plugin`, which hands the handle of the library and the descriptor to `anti_rt_plugin_instance`.
- The compare with 0 is the `else` of the `let`. The result is `none` when the library provides no class for the interface.

The runtime allocates the size the descriptor of the class gives, calls the function that prepares the object and adds the offset of the interface.

The count of live objects starts in the hooks. `created` of every object on the heap finds the image that the table of its class lies in, and adds to the count of that library. `delete` takes the object off once its `destroyed` hook, its `destruct` and the teardown of its fields have returned, since all three run code of the library. `unload` reads the count. A build without hooks could not keep it, which is why `--no-hooks` is refused for a host and for a plugin.

A provider that is a library fills a slot of the last section in another way than a function does. The slot holds a function, and what a library gives is an object. Every injectable interface therefore has one holder and one thunk that returns what the holder holds. The runtime stores the object in the holder and the thunk in the slot before `main`.

Discovery reads the `anti-plugins.toml` of each directory of the key `plugins`, in order, and opens no library to learn what is inside. An empty directory of the value, as in `--anti.plugins=:plugins`, is the working directory, as in `PATH`. A library built for another runtime version is passed over. One whose bytes do not match the digest of the index is refused. Two libraries that provide one interface are an error that names both.

Where it lives: `whole_write_provides` of `src/antic/whole_tables.c`, `whole_hosts_plugins` of `src/antic/whole.c`, `driver_write_plugin_index` of `src/antic/driver_index.c`, `src/rt/plugin.c` for the loader and its checks, `src/rt/inject.c` for the providers the start takes from libraries, `src/rt/loaded.c` for the open libraries and their counts, and `src/std/anti/plugin.anti`.

### What it costs

| Cost | Count |
|---|---|
| A call into a plugin | the dispatch of an interface, as a call into a class of the program |
| A call from a plugin into the host or the runtime | one call through an entry of the loader on Linux and Windows |
| `plugin.load` | one open of the library by the system, one read of its table, and the checks per interface |
| `lib.instance` | one allocation of the object, one call of the function that prepares it, the hook `created` |
| `created` and `destroyed` with a library open | one lookup of the image of the table by the system, one lock of the runtime and one atomic add, for every object of the program |
| `created` and `destroyed` with no library open | one atomic load and one compare on top of the hook |
| The exports of a host | the names of its functions and data stay in the executable |
| LTO | a host keeps every exported function, so the link removes less unused code |

The two programs of the example show the exports. The host measured here has a `main` that calls `speak` and prints the message of a failure. On macos-arm64 it is 255,344 bytes, and the same source built with `--closed` is 134,912 bytes. The library is 57,856 bytes. The closed program fails at `plugin.load` with the message of the loader, which names the first symbol it could not find.

### Pitfalls

For a Java programmer:

- A library is built against one host. It carries no code of its own for the interface, the runtime or the standard library, and loads into no other program.
- The version check is by the shape of the interface. A function added at the end of an abstract class keeps old plugins loading, and a function inserted in the middle refuses them.
- `unload` fails while one object of the library is alive. Nothing collects it later.

For a C programmer:

- No `dlsym` and no cast. `lib.instance` gives a typed pointer or `none`, and the checks have run before it.
- A static program of musl loads nothing. A Linux host is the dynamic link against glibc, with the system's `libc.so.6` at run time.

For a C++ programmer:

- The runtime version of host and plugin match exactly. A plugin is rebuilt for every release of the runtime.
- A class whose `construct` takes arguments cannot be provided through `instance`. The host has no arguments to give.
- A host cannot be built with `--no-hooks`, and a plugin neither. A program that wants no hook sites is built `--closed` and loads nothing.
