---
title: "The tool"
description: "Modules and imports, visibility across modules, library files, anti.toml, dependencies and the lock file, anti build and its cache, anti check, anti fmt, anti doc, anti license and anti symbols."
summary: "Modules, imports and direct imports, the four levels of visibility, library files, the manifest `anti.toml`, dependencies and `anti.lock`, `anti build` and its cache, `anti check`, `anti fmt`, `anti doc`, `anti license` and `anti symbols`."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-26T12:00:00+01:00
draft: true
weight: 180
tags: [compilers, programming-languages]
keywords: [modules, imports, visibility, library files, build tool, manifest, lock file, dependency resolution, build cache, formatter, documentation generator, licence notice, symbols]
---

A program of Anti is a set of modules, one per source file, and a module reaches another through its library file. The first three sections hold the modules, their visibility and their library files. The rest of the chapter holds `anti`, the tool of a project. It reads the manifest `anti.toml`, resolves and fetches the dependencies, and compiles through the driver of antic, which is linked into it.

Every command of this chapter was run on macos-arm64 by antic and anti 0.2.0 of `c2806b61`, and the page shows its output as the tool printed it. A path of the machine is shortened to `/home/ann`. The commands ran in a tree of the repository with `--runtime`. An installed package does not need the option, and the page leaves it out.

[Chapter 1]({{% relref "01-getting-started" %}}) holds `anti new`, a first `anti build` and `anti run`. [Chapter 17]({{% relref "17-c-interop" %}}) holds `anti bind`, and [chapter 19]({{% relref "19-tests" %}}) `anti test`.

## Modules and imports

### What you know

A Java source file names its package, and the directories mirror the packages. `import` names a class, which the class loader finds on the class path at run time. C pastes the text of a header with `#include`, and the linker joins the objects by the names of their symbols. C++ adds namespaces for names, and the modules of C++20 compile an interface into a binary file that an importer reads.

### The feature

One source file is one module. Its module path is the path of the file below the search root that holds it, with dots for the slashes and without `.anti`. Under the root `src`, the file `src/com/example/shapes.anti` is the module `com.example.shapes`. antic makes the file and the root absolute and normalises both first, so `./src/com/example/../example/shapes.anti` names the same module. A file under no root takes its file name alone, `shapes`. `anti build` gives antic the source directory of `[layout]` as the root.

The segments of a module path follow four rules.

- Each segment is a lowercase identifier. `anti build` refuses another file name with `` `bad-name` in ./src/com/example/bad-name.anti is not a lowercase identifier ``.
- No segment is a keyword. antic refuses the file `src/app/here.anti` with `` antic: `here` in src/app/here.anti is a keyword ``, and `import app.here;` does not parse.
- A path under `anti.` belongs to the standard library. A library with such a path is refused with `the module path ... is reserved for the language's own libraries`.
- A library whose path has one segment gets the warning `single-segment-path`, since such a path is for the files of a program. A third party starts its paths with a root it owns, as `com.example`.

An `import` line makes a module visible in the file. It has three forms.

| Form | The file names |
|---|---|
| `import com.example.shapes;` | `shapes.Square`, by the last segment |
| `import com.example.shapes as sh;` | `sh.Square`, by the alias alone |
| `import com.example.shapes.{Square, CORNERS};` | `Square` and `CORNERS` without a prefix, and `shapes.Square` as well |

The rules of the checker:

- The `import` lines stand first in a file, before every item. A clause of the whole file, `allow` or `unchecked`, may stand among them.
- A module is imported once in a file. A plain import and a direct import of one module are refused. An alias and a direct import of one module compile together.
- A name of a direct import is a `pub` item of the module, or an `internal` one inside its package. A name that clashes with a name of the file is refused, naming both. `anti fmt` sorts the list.
- An expression reaches a module by its last segment or its alias. A dotted path such as `com.example.shapes.Square` stands in no type and no expression.
- A module the file does not import is unknown, and so are its types.
- Two modules that import each other are refused, and `anti build` names the cycle: `anti: the imports form a cycle: com.example.a -> com.example.b -> com.example.a`.

Some forms of the language need a module of the standard library, and the message of each names the import. `may fail`, `here` and `catch` on a `?*T` need `anti.lang`. `f"..."` and a `switch` on a `str` need `anti.text`. A pattern literal needs `anti.regex`, and `Object.deserialize` and an allocator argument of `destroy` and `delete` need `anti.mem`.

The program below uses the three forms on modules of the standard library.

```anti
//! name: imports
import anti.io as out;
import anti.text as t;
import anti.text.{Builder, equal};

pub fn same(a: str, b: str) -> bool
{
	return t.equal(a, b);
}

export fn same_c(a: ?*byte, b: ?*byte) -> bool
{
	return same(t.from_c(a), t.from_c(b));
}

fn main() -> int
{
	let b = Builder.new();
	b.append("one");
	b.append(" two");
	let whole = b.text();
	let x = "a";
	let y = "b";
	let line = f"{whole}: same {same(whole, whole)}, equal {equal(x, y)}";
	out.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
one two: same true, equal false
```

The checker refuses the following.

```anti refused
//! error: `text` is already declared
import anti.text;
import anti.text.{equal};
```

```anti refused
//! error: `equal` of `anti.text` clashes with `equal` declared on line
import anti.text.{equal};

fn equal(a: int) -> int
{
	return a;
}
```

```anti refused
//! error: `anti.regex` has no public item `Regex`
import anti.regex.{Regex};
```

`Regex` is declared in `anti.lang` by the compiler, and is no item of `anti.regex`.

```anti refused
//! error: cannot find module `com.example.nowhere`
import com.example.nowhere;
```

```anti refused
//! error: `f"..."` builds its text with `anti.text.Builder`, so the module imports `anti.text`
fn name(n: int) -> str
{
	return f"n is {n}";
}
```

An import reads the library file of the module, never its source. antic looks for `com/example/shapes.antl` under each `-I` root in order, and under `std/` of the runtime archive last. It then reads the imports of each file it found, until every module the program reaches has its file. A library file comes from `antic -c`, so the imported module is compiled first. `anti build` compiles the modules of a project in the order of their imports. A session by hand:

```sh
antic -I src -c src/com/example/shapes.anti
antic -I src src/com/example/prog.anti -o prog
```

A library file named on the command line that holds the module antic compiles is refused: `` antic: com/example/lib.antl holds module `com.example.lib`, which this command compiles ``.

### How antic builds it

A module is a namespace of symbols. Every function has the symbol of its module path and its name, `anti.text.equal`, with the dot, so two modules may declare the same name and no C program can call either by accident. An alias and a direct import change what the file writes and never the symbol: `t.equal` and `equal` above both call `anti.text.equal`.

An import runs no code. It hands the checker the interface of the library file and nothing else. Two kinds of function run before `main`. One is the start of the runtime. The other is the constructor of a module that holds pattern literals, which [chapter 3]({{% relref "03-values" %}}) describes.

A dev build compiles one module into one object. A function of the module is a global symbol with `hidden` visibility. The objects of the program link to it, and nothing outside the program sees it. A call into another module is a direct call of a declared symbol:

```llvm excerpt=imports target=linux-x86_64 mode=dev
define hidden noundef zeroext range(i8 0, 2) i8 @imports.same(i64 %p0.0, i64 %p0.1, i64 %p1.0, i64 %p1.1)
...
  %v10 = call range(i8 0, 2) i8 (i64, i64, i64, i64) @anti.text.equal(i64 %v3, i64 %v5, i64 %v7, i64 %v9)
...
declare noundef zeroext range(i8 0, 2) i8 @anti.text.equal(i64, i64, i64, i64)
```

- `same` is `hidden` and carries its module, `imports`, in its symbol.
- The call passes the two `str` values as four integers, a pointer and a length each, by the C convention of [chapter 5]({{% relref "05-functions" %}}).
- The `declare` line is all the object knows of `anti.text.equal`. Its body is in the object of `anti.text`, so a dev build never inlines across modules.

A release build reads the IR of every module from its library file and compiles the whole program as one module. Every function is `internal` there, and opt inlines across the modules:

```llvm excerpt=imports target=linux-x86_64 mode=release
define internal noundef zeroext range(i8 0, 2) i8 @same_c(ptr noundef %p0, ptr noundef %p1)
b0:
  %v1.i = tail call { i64, i64 } @anti_rt_text_from_c(ptr %p0)
...
  %v12.not.i.i = icmp eq i64 %v3, %v8
...
  %bcmp.i.i = tail call i32 @bcmp(ptr %0, ptr %1, i64 %v3)
...
imports.same.exit:
```

- `same_c` calls the runtime for each C string. `from_c` of `anti.text` is gone, inlined.
- The compare of the two lengths and the `bcmp` of the bytes are the body of `anti.text.equal`, inlined into `same`, which is inlined into `same_c`. The label `imports.same.exit` is where the inlined `same` ends.

Where it lives: `modpath_of_source` and `modpath_reserved` of `src/antic/modpath.c` give the module path, and `module_name` of `src/antic/driver.c` checks it. `search_roots` and `driver_find_libraries` of `src/antic/driver_search.c` find the library files. `sema_declare_import` and `sema_declare_direct_names` of `src/antic/sema_declare.c` declare the names, and `src/anti/units.c` orders the modules of a project and names a cycle.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| An import at run time | nothing: no code, no data, no start | the same |
| A call into another module | one direct call. The body stands in another object and is never inlined | one call, or none after opt inlines it |
| The symbol of a function | global, `hidden` | `internal` |
| An import at compile time | one read of a library file, and of each file it imports in turn | the same, and the IR of every function of the file joins the program |
| A copy of a generic of another module | compiled in each module that uses it, and merged at the link, as [chapter 10]({{% relref "10-generics" %}}) describes | compiled once |

The three forms of an import give the same code. A plain import keeps the module visible at each use, which a reader of a long file follows. An alias resolves two modules with one last segment. A direct import suits names used on many lines, as the types of `anti.collection`.

### Pitfalls

For a Java programmer:

- An import names a module and never a class. `import com.example.shapes.Square;` is no import form, and `import com.example.shapes.{Square};` is.
- Nothing is found at run time. A module missing at compile time is an error, and the program holds every module it reaches.

For a C programmer:

- An import pastes no text. Two modules may declare the same name, and a module sees what it imports alone, not what its imports import.
- A dev build calls every function of another module through a symbol, as C calls a function of another file. A release build inlines across modules.

For a C++ programmer:

- No namespace is opened. `using namespace` has no counterpart, and a direct import lists each name.
- An import does not compile its module. The library file must exist, and `anti build` writes it.

## Visibility across modules

### What you know

Java has four levels: `public`, `protected`, the package level without a keyword, and `private`, and the virtual machine checks them again when it links. C has two: a name with external linkage that every object sees, and `static`, which keeps a name in its file. C++ adds `public`, `protected` and `private` for the members of a class, and `friend` to reach past them.

### The feature

Anti has four levels. Each applies where it makes sense.

| Marker | A module item | A function of a class | A field of a class |
|---|---|---|---|
| `pub` | every module | everywhere, through the table | everywhere |
| `internal` | the modules of the same package | refused | refused |
| `protected` | refused | the class and every class below it | the class and every class below it |
| none | the module | the class | the class |

The package of a module is the `name` of `[package]` in `anti.toml`. `anti build`, `anti check`, `anti test` and `anti doc` hand it to antic with every compile, and antic takes it as `--package-name`. The library file marks an `internal` item, and the checker gives it to an importer of the same package alone.

Five more rules complete the table.

- `abstract fn`, `concrete fn` and `operator fn` are `pub`.
- A class and the classes nested in it reach the private members of each other, in both directions.
- An `operator fn` at module level reaches the private and protected members of the classes of its module, and gives them their `==`, hash and order. An ordinary function of the module does not.
- A class of another module that inherits a class reaches its protected members.
- Visibility is checked by name. `reflect` reaches every field, as [chapter 9]({{% relref "09-classes" %}}) describes.

The program below holds a private function, a protected field, a nested class that reads both, and a class below that writes the field.

```anti
//! name: visible
import anti.io;
import anti.text;

pub class Counter
{
	pub count: int = 0,
	protected step: int = 1,

	pub fn add(self)
	{
		self.bump(self.step);
	}

	fn bump(self, by: int)
	{
		self.count += by;
	}

	class Probe
	{
		seen: int = 0,

		fn read(self, c: *Counter) -> int
		{
			return c.count + c.step;
		}
	}

	pub fn probed(self) -> int
	{
		let p = Probe { seen: 5 };
		return p.read(self) + p.seen;
	}
}

class Double inherits Counter
{
	pub fn twice(self)
	{
		self.step = 2;
		self.add();
	}
}

fn main() -> int
{
	let c = alloc Double { };
	c.add();
	c.twice();
	let line = f"count {c.count}, probed {c.probed()}";
	io.println(line);
	free(line.ptr);
	delete(c);
	return 0;
}
```

```text output
count 3, probed 10
```

The checker refuses the following.

```anti refused
//! error: `secret` is private to `Square`
class Square
{
	pub side: int = 0,

	fn secret(self) -> int
	{
		return 7;
	}
}

fn peek(s: *Square) -> int
{
	return s.secret();
}
```

```anti refused
//! error: `cached` is protected in `Square`
class Square
{
	pub side: int = 0,
	protected cached: int = 0,
}

fn peek(s: *Square) -> int
{
	return s.cached;
}
```

```anti refused
//! error: `internal` marks a module item, and a class member is `pub`, `protected` or neither
class Square
{
	internal fn area(self) -> int
	{
		return 1;
	}
}
```

`protected fn helper()` at module level is refused the other way round, with `` `protected` marks a class member, and a module item is `pub`, `internal` or neither ``.

Across modules the messages are the same for the members of a class. A private function of a class of a library is reported as private, since the library file carries its name. An item of a module that another module may not see is reported as missing. The module `com.example.shapes` below holds an `internal` and a private function, and was compiled with `--package-name com.example`.

```text
src/com/example/use_private.anti:5:9: error: `shapes` has no public item `private_here`
src/com/example/use_secret.anti:5:9: error: `secret` is private to `Square`
src/com/example/use_internal.anti:5:9: error: `shapes` has no public item `shared_inside`
```

The third line comes from a compile with `--package-name com.other`. With `--package-name com.example` the same file compiles.

### How antic builds it

Visibility exists in the checker alone. A private function has the same symbol and the same linkage as a `pub` one: global and `hidden` in a dev object, `internal` in a release build. What changes is how a function of a class is called. A `pub` function of a class has an entry in the table of the class, and a call through a pointer to a class that is not `final` dispatches through it. A private or protected function has no entry, and every call of it is direct.

```llvm excerpt=visible target=linux-x86_64 mode=dev
  call void (ptr, i64) @visible.Counter.bump(ptr %v4, i64 %v5)
...
define hidden void @visible.Counter.bump(ptr noundef nonnull dereferenceable(24) %p0, i64 noundef %p1)
...
ptr @anti_lang_Object_changed, ptr @visible.Counter.add, ptr @visible.Counter.probed }>
```

- `add` calls `bump` by its symbol, with `self` and the step as the two arguments. No load of the table precedes the call.
- `bump` is `hidden`, as every function of the module is.
- The table of `Counter` ends with `add` and `probed` after the nine hooks of `Object`. It has 19 entries, and none is `bump`.

The interface in the library file of a module carries its `pub` items, its `internal` items with their mark, and the `pub` and protected functions of each class. It carries the name alone of each private function of a class, so a message names it as private. A generic carries every function of its body, private ones included, since a module that makes a copy compiles them. The IR of the file holds the body of every function, for the release build that compiles the whole program. The C header of `--lib` declares every field of an exported class, with `/* private */` on the private ones, and no private or protected function.

Where it lives: `sema_call.c` and `sema_expr.c` of `src/antic/` check a member, and `sema_declare_direct_names` of `src/antic/sema_declare.c` an item of a library. `carried_member` and `is_private_member` of `src/antic/antl_write.c` decide what the library file carries, and `src/antic/header.c` what the header declares.

### What it costs

| Cost | Dev | Release |
|---|---|---|
| A check of visibility | none at run time | none |
| A call of a private or protected function | one direct call | one direct call, or none after inlining |
| A call of a `pub` function of a class that is not `final`, through a pointer | two loads and an indirect call | a direct call where the whole-program pass devirtualises it |
| A table entry | one pointer, 8 bytes, per `pub` function of the class | the same |

A helper of a class that no other class needs is private. It costs no table entry and is never dispatched. `protected` is for a member a class below needs. `internal` is for an item the modules of one package share and a user of the package does not see. An item a user calls is `pub`.

### Pitfalls

For a Java programmer:

- No level without a keyword is shared by a package. An item without a marker is private to its module, and `internal` is the package level.
- `protected` gives nothing to the rest of the module. A function of the module that is no `operator fn` reads no protected field.

For a C programmer:

- A private function is no `static` function. Its symbol is global and `hidden` in a dev object, so two private functions of two modules never clash, and the linker sees both.
- The fields of a class are all in the C header. Private is a promise of the checker, and C code reads every field.

For a C++ programmer:

- No `friend` exists. An `operator fn` of the module and the nested classes are the two ways past a private member.
- A private function is never in the table, so a class below cannot replace it. A function that a class below replaces is `pub`.

## Library files

### What you know

A C library ships a header, which every user compiles again, and an archive or a shared object of machine code for one target. A Java library ships class files of bytecode, which hold the interface and the code at once and run on every machine. A C++ library adds templates, whose bodies stand in the header, since each user compiles its own instances.

### The feature

A library file, `<module>.antl`, holds the public interface of one module, the checked tree of each of its generics and the IR of every function. It holds no machine code. `antic -c` writes it, beside the source or at the path of `-o`. An import reads it, as "Modules and imports" describes, and `anti doc` writes the pages of a module from it.

| Option of `antic -c` | Effect |
|---|---|
| `--package-name <p>`, `--package-version <v>` | the package header of the file. Without them the file names its module and `0.0.0` |
| `--license <spdx>`, `--license-text <file>`, `--attribution <line>` | the licence fields of the header, which a notice reads |
| `--strip-docs` | leave the doc text out |

`anti build` writes the library file of every module of a project under `build/<target>/<mode>/lib/`, and a library project copies them into `dist/`. A dependency is a set of library files, whatever its source. The library file of the module `com.example.units` of "anti.toml" below is 828 bytes, and 676 with `--strip-docs`.

A library file serves every target. Its IR holds no size and no offset, only symbolic values that the back end lays out for the target it compiles for. The file of `com.example.units` that `antic -c` writes for macos-arm64, linux-x86_64 and windows-arm64 is the same 825 bytes, byte for byte.

The interface in the file carries what another module may name, and what the checker of that module needs:

- every `pub` item, and every `internal` one with its mark, with its type as the source spells it, so `anti doc` writes `f64` where the source writes `f64`;
- the parameter names of each function, its defaults, its `own` parameters and its `may fail` flag;
- the value of each constant, and the default of each field, a function it names included;
- each class with its `pub` and protected functions, its constants and static fields, the mark of a singleton, and the name alone of each private function;
- the checked tree of each generic function and type, with every function of its body. A function of a class with type parameters of its own, and a type nested in a generic class, are not carried yet;
- the locks each function takes, the edges of the lock order its code adds and the first wait each function may reach. `lock-order` of [chapter 14]({{% relref "14-concurrent-classes#lock-order-at-compile-time" %}}) and `blocking-under-lock` of [chapter 13]({{% relref "13-threads#waits-under-a-lock" %}}) follow them across modules;
- the imports, the `link` lines and the doc text of the module.

A file of another format version is refused when a module imports it.

```text
antic: src/com/example/shapes.antl has format version 92, and antic reads version 93
```

### How antic builds it

`antic -c` runs the front end and the lowering, and stops before the back end. What it writes is what the back end would have read: the IR of each function as lowering left it, before any optimization. The program below holds a struct whose layout differs between the targets, since `c_long` is 8 bytes on Linux and 4 on Windows.

```anti
//! name: sizes
export struct Entry
{
	key: c_long,
	value: c_int,
}

export fn second(e: *Entry) -> int
{
	return e.value as int + size_of(Entry);
}
```

The IR of `second`, as the library file holds it:

```ir excerpt=sizes target=linux-x86_64 mode=release
export fn sizes.second(%0: ptr nonnull deref(size_of sizes.Entry)) -> i64 {
b0:
    %1 = ptradd inbounds %0, offset_of sizes.Entry.value
    %2 = load i32 %1 !type sizes.Entry.value
```

- `size_of sizes.Entry` and `offset_of sizes.Entry.value` are symbols and no numbers. The IR is the same for every target.
- The load reads an `i32`, the `c_int` field, at the offset the back end will give it.

The back end lays out `Entry` for its target and folds the symbols. On linux-x86_64 the field stands at 8 and the struct is 16 bytes:

```llvm excerpt=sizes target=linux-x86_64 mode=release
  %v1 = getelementptr inbounds nuw i8, ptr %p0, i64 8
  %v3 = load i32, ptr %v1, align 4
  %v5 = sext i32 %v3 to i64
  %v7 = add nsw i64 %v5, 16
```

On windows-x86_64 the same IR gives 4 and 8:

```llvm excerpt=sizes target=windows-x86_64 mode=release
  %v1 = getelementptr inbounds nuw i8, ptr %p0, i64 4
  %v3 = load i32, ptr %v1, align 4
  %v5 = sext i32 %v3 to i64
  %v7 = add nsw i64 %v5, 8
```

A dev build compiles the object of a module from its library file, for one target. A release build reads the IR of every library file the program reaches and compiles the whole program, as [chapter 2]({{% relref "02-how-antic-works" %}}) describes. Both lay out every type once per target, in the back end.

The file starts with the magic `ANTL` and the version as a 32-bit number, 93. The package header, the module path, the imports, the frameworks, the Linux libraries and the doc text of the module follow. Then come the table of types, the items, the trees of the generics, the IR and the lock facts. Its bytes depend on the source and the options alone, so every host writes the same file.

Where it lives: `antl_write` of `src/antic/antl_write.c` writes the file, `src/antic/antl_read.c` and `src/antic/antl_read_ir.c` read it, `src/antic/antl_tree.c` holds the trees of the generics and `src/antic/antl_locks.c` the lock facts. `src/antic/layout.c` lays out the types for a target.

### What it costs

| Cost | Count |
|---|---|
| Bytes of the library file of `com.example.units` | 828, and 676 with `--strip-docs` |
| Files per module | one for all six targets |
| Work of an import at compile time | one read of the file and of each file it imports. The checker reads the interface, and a release build adds the IR of every function to the program |
| A generic of the file | its checked tree, compiled again in each module that makes a copy |
| A dev object | compiled from the library file, and cached by its digest, as "anti build and its cache" below describes |
| Cost at run time | none. A program holds no library file |

A library file is the form in which Anti code reaches other Anti code. A project imports it, a repository serves it and a release build compiles from it. A library for C of [chapter 17]({{% relref "17-c-interop" %}}) is the form for C, compiled for one target. `--strip-docs` suits a file that ships to users who read the pages elsewhere. The pages that `anti doc` writes from such a file lose their text.

### Pitfalls

For a Java programmer:

- A library file is no bytecode that runs. It is compiled to machine code by the build of the program that imports it, for that program's target.
- Two versions of one module in one program do not exist. The resolver of "Dependencies and the lock file" picks one.

For a C programmer:

- No header is written by hand and none is compiled again. The interface comes from the source, and an import reads it in one pass.
- One file serves every target, so a `c_long` field has its offset per target in the back end, never in the file.

For a C++ programmer:

- A generic ships as its checked tree, as a template ships in a header, and each module compiles the copies it uses. No module takes a copy from the file of another.
- A file of another format version is refused. A compiler of another version cannot read it, and its source must be compiled again.

## anti.toml

The manifest is the file `anti.toml` in the root of a project. It is TOML: tables, strings, integers, booleans, arrays and inline tables. `anti new com.example.hello` writes the smallest one.

```toml
[package]
name = "com.example.hello"
version = "0.1.0"
```

The tool reads these tables.

| Table | Key | Value |
|---|---|---|
| `[package]` | `name` | the package name, a module path. It is the root of every module of the package |
| | `version` | the version, `major.minor.patch` |
| | `antic` | the lowest compiler version the project builds with |
| | `license` | the SPDX identifier of the licence |
| | `license_text` | the file that holds the licence text |
| | `attribution` | one line the notice of a program carries |
| `[layout]` | `src`, `test`, `build`, `dist` | the four directories, with those names by default |
| `[targets]` | `default` | the targets of `anti build` without `--target`. The host by default |
| | `all` | the targets of `anti build --target all`. All six by default |
| `[repositories]` | an alias | the URL prefix of a repository |
| `[dependencies]` | a package name, quoted | an inline table with `version`, `repo` or `path` |
| `[inject]`, `[inject.test]` | the path of an interface | its provider, as [chapter 15]({{% relref "15-hooks" %}}) describes |

A source file is one module. Its module path is its path under `src/` with dots for the slashes, so `src/com/example/hello.anti` is `com.example.hello`. The module with `fn main` is the program, and a project without one is a library project.

The manifest below is that of the library of this chapter.

```toml
[package]
name = "com.example.units"
version = "1.2.0"
license = "MIT"
```

Its one module is `src/com/example/units.anti`.

```anti
//! name: units
//! Conversions between metres and feet.

/// The feet in one metre.
pub const FEET_PER_METRE: f64 = 3.28084;

/// The length `metres` in feet.
///
/// ```anti
/// let f = units.feet(2.0);
/// ```
pub fn feet(metres: f64) -> f64
{
	return metres * FEET_PER_METRE;
}

/// The length `feet` in metres.
pub fn metres(feet: f64) -> f64
{
	return feet / FEET_PER_METRE;
}
```

`anti build` refuses a manifest it cannot follow, with one line.

| Manifest | Message |
|---|---|
| `antic = "9.0"` | `anti: ./anti.toml asks for antic 9.0 and this one is 0.2.0` |
| `default = ["amiga-68k"]` | `anti: anti.toml: [targets] names amiga-68k, which is no target of Anti` |
| `src = "source"`, with no such directory | `anti: ./source holds no .anti file, so this project has no module` |
| `"com.example.Units" = { path = "../units" }` | `anti: ./anti.toml: the dependency com.example.Units is no package name` |
| a path that holds a control byte | `anti: ./anti.toml: the path <path> of the dependency com.example.units holds a control byte`, with the path as written |

A library project without the module whose path is the package name builds, with a warning: `anti: ./anti.toml: warning: the library project com.example.app has no module com.example.app`.

A key the tool does not know is not reported. `colour = "red"` under `[package]` builds as if it were not there.

Where it lives: `src/anti/manifest.c` reads the manifest with the TOML parser of `src/rt/toml.c`, which the runtime configuration shares.

## Dependencies and the lock file

A dependency is a package of library files, the form that "Library files" above describes. A project that depends on a package never sees its source.

`[dependencies]` maps a package name to an inline table of one of three shapes.

| Shape | Source |
|---|---|
| `{ version = "1.2.0", repo = "local" }` | the repository with that alias |
| `{ version = "1.2.0" }` | every repository of the manifest. One of them must hold a version that fits |
| `{ path = "../units" }` | a project on disk, which is built first, or a directory of library files |

A version constraint has three forms.

| Form | Meaning |
|---|---|
| `"1.2.0"` | 1.2.0 or newer, below 2.0.0 |
| `"=1.2.0"` | exactly 1.2.0 |
| `">=1.2.0"` | 1.2.0 or newer, with no upper bound |

### Path dependencies

The program of this chapter is `com.example.hello`. Its manifest names the library by its directory.

```toml
[package]
name = "com.example.hello"
version = "0.1.0"

[dependencies]
"com.example.units" = { path = "../units" }
```

Its source imports the module and calls it. `anti run` builds the library project first and prints `2 m is 6.56 ft`.

```sh
anti run
```

`anti build` writes `anti.lock` beside the manifest on its first run.

```toml
# Written by `anti build`. Commit it for an application.
version = 1

[[package]]
name = "com.example.units"
version = "1.2.0"
path = "./../units/dist/macos-arm64/dev"
modules = [
    { path = "com.example.units", sha256 = "02870f25d74c191993b270b3c8d7683ba0288bc2bef6999b49937f13e85dd4ac" },
]
```

The lock file names every package of the graph with its version, its source and the SHA-256 of each library file. A package whose version needs other packages names them in `needs`, as `needs = ["com.example.shapes"]`. A later build takes the locked versions and compares each digest before it compiles.

### Repositories

A repository is a URL prefix over static files, and no server code stands behind it. The URL starts with `https://` or `file://`. `http://` is taken for `127.0.0.1`, `localhost` and `[::1]` alone.

```text
<prefix>/<name>/index.toml
<prefix>/<name>/<version>/<module>.antl
```

`index.toml` lists every version of a package with the digest of each module and the dependencies of the version.

```toml
name = "com.example.units"
revision = 1

[[version]]
version = "1.2.0"
modules = [{ path = "com.example.units", sha256 = "02870f25d74c191993b270b3c8d7683ba0288bc2bef6999b49937f13e85dd4ac" }]
dependencies = []
```

A version with `yanked = true` keeps its entry. A lock file that names it still resolves, and a new resolution passes over it.

The program takes the library from a repository with this manifest.

```toml
[package]
name = "com.example.hello"
version = "0.1.0"

[repositories]
local = "file:///home/ann/repo"

[dependencies]
"com.example.units" = { version = "1.2.0", repo = "local" }
```

`anti build` then writes this lock file.

```toml
# Written by `anti build`. Commit it for an application.
version = 1

[[package]]
name = "com.example.units"
version = "1.2.0"
repo = "file:///home/ann/repo"
modules = [
    { path = "com.example.units", sha256 = "02870f25d74c191993b270b3c8d7683ba0288bc2bef6999b49937f13e85dd4ac" },
]
```

### Resolution and the cache

A build resolves in four steps.

1. It reads the manifest. When a lock file stands and every locked entry meets the manifest, it takes the lock file as it is. The dependencies of the manifest must also reach every locked package through `needs`.
2. Otherwise it reads the index of each dependency and walks the dependencies of the versions until the graph is closed. It picks the highest version that is not yanked and meets every constraint on the package, and writes `anti.lock`.
3. It fetches every library file that is not in the cache and compares its digest with the index.
4. It hands the sources of the project and every library file of the graph to the compiler.

Fetched files go to the cache of the user, which [chapter 1]({{% relref "01-getting-started" %}}) places: `~/.cache/anti` on macOS and Linux unless `XDG_CACHE_HOME` names another, and `%LOCALAPPDATA%\anti\cache` on Windows. After the build above it holds three files.

```text
index/<sha256 of the prefix>/com.example.units/index.toml
index/<sha256 of the prefix>/com.example.units/index.toml.checked
pkg/com.example.units/1.2.0/com.example.units.antl
```

The file `index.toml.checked` is the stamp of the last check. The tool reads an index from its repository again at most once per hour. A library file in the cache is never fetched again. An `https://` fetch runs the `curl` of the system. A redirect of the server is followed when it leads to `https://`, or to `http://` of the machine itself, and refused otherwise.

`anti build --offline` contacts no repository and fails when a file it needs is not in the cache.

| Situation | Message |
|---|---|
| `--offline`, empty cache | `anti: --offline and no cached com.example.units of com.example.units 1.2.0` |
| the fetched file differs from the index | `anti: com.example.units of com.example.units 1.2.0 has the digest 6108... and the index names 0287...` |
| `version = "2.0.0"`, and the repository has 1.2.0 | `anti: file:///home/ann/repo has no version of com.example.units that satisfies every constraint on it` |
| no `repo`, and no repository of the manifest has the package | `anti: no repository of the manifest has a version of com.example.units that satisfies every constraint on it` |
| `path = "../nowhere"` | `anti: ./../nowhere holds no library file of the package com.example.units` |
| `version = "=2.0.0"` beside a path that holds 1.2.0 | `anti: ./../../units/dist/macos-arm64/dev holds com.example.units 1.2.0, which no constraint on the package takes` |
| no `repo`, the first repository holds 1.1.0 alone and the second 1.2.0 | the second answers. A file missing there is named: `anti: file:///home/ann/second/com.example.units/1.2.0/com.example.units.antl: the repository holds no such file` |
| no `repo`, and two repositories hold a version that fits | `anti: file:///home/ann/repo and file:///home/ann/second both hold com.example.units, so the dependency names which one` |
| `local = "file:///C:/repo"` on macOS or Linux | `anti: file:///C:/repo/com.example.units/index.toml: a file:// URL names C:/repo/com.example.units/index.toml, which is a path of another system` |
| two path dependencies that name each other | `anti: the path dependencies form a cycle: com.example.app -> com.example.one -> com.example.app` |
| an `anti.lock` that is no TOML | `anti: ./anti.lock is no TOML that anti reads` |
| an `anti.lock` whose entries no longer answer the manifest | `anti: ./anti.lock no longer answers the manifest, so the build writes it again`, and the build goes on |

The two digests of the second row are shortened here. The tool prints all 64 digits of each. A cycle of path dependencies is refused at any depth, and the message names the two packages that close it.

Where it lives: `src/anti/deps.c` resolves the graph and writes the lock file, and `src/anti/repo.c` fetches into the cache.

## anti build and its cache

`anti build` reads `anti.toml` of the current directory, resolves, fetches and compiles. It prints nothing when it succeeds. `anti run` does the same for the host and runs the program.

| Option | Effect |
|---|---|
| `--release` | release mode. Dev mode is the default of the tool |
| `--target <t>`, `--target all` | build for that target, or for the list `all` of the manifest |
| `--cpu <level>` | the processor level, which [chapter 20]({{% relref "20-targets" %}}) lists |
| `--offline` | contact no repository |
| `--lib static`, `--lib shared` | write a library for C with its header, as [chapter 17]({{% relref "17-c-interop" %}}) describes |
| `--bundle-runtime`, `--soname` | the two options of a library for C |
| `--memory-checks` | build with the memory checks of [chapter 12]({{% relref "12-memory" %}}) |
| `--strip-docs` | leave the doc text out of the library files |
| `--lto full`, `thin`, `none` | the link time optimization of a release build |
| `--profile-generate`, `--profile-use <file>` | the two steps of a build with a profile, which [chapter 21]({{% relref "21-performance" %}}) describes |
| `--runtime <dir>` | the package to build with |
| `--llvm-ar <path>` | the llvm-ar that writes a static library, in place of the one of the package |

A wrong use of a command ends with the exit status 2 and one line, before anything builds.

| Command line | Message |
|---|---|
| `anti build --soname` | `anti: --soname builds a library, with --lib` |
| `anti build --bundle-runtime` | `anti: --bundle-runtime builds a library, with --lib` |
| `anti build --target all --cpu v1` | `anti: v1 is no processor level of linux-arm64`, after a check of every target and before a build of any |
| `anti run --target linux-x86_64` | `anti: run builds a program for this machine, so it takes no --target`, and the same for `--lib`, `--bundle-runtime`, `--soname`, `--strip-docs` and `--llvm-ar` |
| `anti run` in a library project | ``anti: com.example.one is a library project, which has no `main` to run`` |

A build that finds a file where a directory belongs names it and ends with 1: `anti: ./dist is a file where a directory belongs`.

### A dev build

Dev mode compiles one module at a time. A dev build of `com.example.hello` writes these files.

```text
build/macos-arm64/dev/lib/com/example/hello.antl
build/macos-arm64/dev/lib/com/example/hello.antl.key
build/macos-arm64/dev/obj/anti/io.o
build/macos-arm64/dev/obj/anti/io.o.key
build/macos-arm64/dev/obj/anti/lang.o
build/macos-arm64/dev/obj/anti/lang.o.key
build/macos-arm64/dev/obj/anti/mem.o
build/macos-arm64/dev/obj/anti/mem.o.key
build/macos-arm64/dev/obj/anti/text.o
build/macos-arm64/dev/obj/anti/text.o.key
build/macos-arm64/dev/obj/com/example/units.o
build/macos-arm64/dev/obj/com/example/units.o.key
build/macos-arm64/dev/hello.o
build/macos-arm64/dev/hello
dist/macos-arm64/dev/hello
dist/macos-arm64/dev/NOTICE.txt
```

The build writes a library file per module of the project, and an object per module the program reaches. That covers the modules of the project, of the dependencies and of the standard library below them. The module with `main` is compiled from its source and linked in the same call.

Every cached output has a key file beside it. Its first line names the input and the flags.

```text
02870f25d74c191993b270b3c8d7683ba0288bc2bef6999b49937f13e85dd4ac 0.2.0 macos-arm64 armv8.5 g
```

| Part | Contents |
|---|---|
| The digest | the SHA-256 of the input: the source for a library file, the library file for an object |
| `0.2.0` | the version of antic |
| `macos-arm64`, `armv8.5` | the target and the processor level |
| `g` or `no-g` | whether the output carries the lines of `-g`. A dev object does, a library file does not |
| `strict`, `memory-checks` | present when warnings are errors, and when the object carries the memory checks |

The lines after it hold what else reaches the output. They name the package with its version and its licence, with the digest of the licence file. The dependencies, the attribution lines, the injections and `strip-docs` follow. The last lines hold the digest of every library file the input reaches through its imports, and of every library file of the dependencies. A module whose import changed its interface is checked again, and its object follows a struct of the import that grew.

A build compiles an output again when it is missing or its key differs from the one the build would write. It removes the key before the compile and writes it after, so a build that stops halfway leaves no key to trust.

The key of an object is over the library file and not over the source. A comment added to a source writes the library file again with the same bytes, and the object stays.

What two builds of `com.example.hello` wrote again, read from the times of the files:

| Build | Files written again |
|---|---|
| after a change of `hello.anti` | `hello.antl` and its key, `hello.o`, `hello`, and the two files of `dist/` |
| with no change | `hello.o`, `hello`, and the two files of `dist/` |

The objects of `anti.io`, `anti.lang`, `anti.mem`, `anti.text` and `com.example.units` stayed in both builds. The module with `main` is the one module every build compiles again, and its compile carries the link.

### A release build

`anti build --release` compiles the whole program in one call of the compiler, as [chapter 2]({{% relref "02-how-antic-works" %}}) describes, and caches no object.

```text
build/macos-arm64/release/lib/com/example/hello.antl
build/macos-arm64/release/lib/com/example/hello.antl.key
build/macos-arm64/release/hello.o
build/macos-arm64/release/hello
build/macos-arm64/release/hello.debug
build/macos-arm64/release/hello.debug.o
build/macos-arm64/release/hello.debug.lto/0.arm64.lto.o
build/macos-arm64/release/hello.map
dist/macos-arm64/release/hello
dist/macos-arm64/release/hello-symbols.zip
dist/macos-arm64/release/NOTICE.txt
```

The program of `dist/` carries no symbol data. `hello-symbols.zip` holds `hello.debug`, the same link with its debug sections kept, and `hello.map`, the map of the functions of the program. "anti symbols" below reads both.

| Program | Bytes |
|---|---:|
| `dist/macos-arm64/dev/hello` | 137,008 |
| `dist/macos-arm64/release/hello` | 107,008 |

The symbol table of a dev program for macOS names each of its objects by the absolute path of the file. The size of the dev program therefore changes with the directory of the project: the same build under a directory name 43 characters longer is 137,216 bytes. The release program holds no such path.

### Targets and libraries

`--target <t>` writes `build/<t>/` and `dist/<t>/` beside those of the host, and the cache of each target is its own. A manifest with `default = ["macos-arm64", "linux-x86_64"]` makes a plain `anti build` write both.

```text
dist/linux-x86_64/dev/NOTICE.txt
dist/linux-x86_64/dev/probe
dist/macos-arm64/dev/NOTICE.txt
dist/macos-arm64/dev/probe
```

A library project has no `main`. Its build writes the library file of each module into `dist/`, which is what a path dependency and a repository take.

```text
build/macos-arm64/dev/lib/com/example/units.antl
build/macos-arm64/dev/lib/com/example/units.antl.key
dist/macos-arm64/dev/com/example/units.antl
```

A library file holds no machine code. One file serves all six targets, and the build of the program that uses it writes the object.

Where it lives: `src/anti/build.c` holds `anti build`, `anti run` and `anti new`, with the key in `cache_key`. `src/anti/units.c` collects the modules and the link facts of a program.

## anti check

`anti check` runs four classes of checks over the sources of a project and writes no program. It prints one line per class.

```sh
anti check
```

```text
anti check: front end: 1 file, 1 target, 0 warnings
anti check: doc blocks: 1 block
anti check: doc warnings: 0
anti check: formatting: 1 file
```

| Class | What it does |
|---|---|
| Front end | the lexer, the parser and the checker on every source, with every warning as an error |
| Doc blocks | compiles each `anti` block of a doc comment |
| Doc warnings | reports what a doc comment gets wrong. With `--warn-undocumented` it reports every `pub` item without a `///` comment |
| Formatting | compares each file with what `anti fmt` writes |

The classes run in this order. The first class that fails ends the run, and the exit status is 1. The doc warnings fail nothing. A fence of a doc comment that nothing closes holds no block. The doc warnings report it:

```text
src/com/example/open.anti:3:1: warning: the doc comment of `one` holds a fence that nothing closes [doc-markup]
```

| Option | Effect |
|---|---|
| `--targets all` | run the front end once per target. The first line then reads `1 file, 6 targets` |
| `--warn-undocumented` | add the warning for a `pub` item without a doc comment |
| `<file.anti>...` | check those files. Without one, every source under the directories of `[layout]` |
| `--work <dir>` | the directory of the modules it writes for the doc blocks. `check` in the build directory of `[layout]` without it, `build/check` in the example |

With `--warn-undocumented`, a run over a `pub` function without a doc comment that is not in the canonical form prints:

```text
src/com/example/units.anti:22:8: warning: the pub item `yards` has no `///` comment [undocumented]
src/com/example/units.anti:22:1: error: the line is not the one `anti fmt` writes
anti check: front end: 1 file, 1 target, 0 warnings
anti check: doc blocks: 1 block
anti check: doc warnings: 1
anti check: formatting: 1 file, 1 finding
```

A doc block that does not compile ends the run at the second class. The block of a function `miles` gives its `f64` result to an `int`, `let n: int = units.miles(1.0);`.

```text
build/check/check/docs/com_example_units_1.anti:5:14: error: expected `int`, found `float`
anti: the `anti` block of `miles` at src/com/example/units.anti:28 failed, and its module is build/check/check/docs/com_example_units_1.anti
anti check: front end: 1 file, 1 target, 0 warnings
anti check: doc blocks: 2 blocks, 1 failed
```

An error of the front end ends the run at the first class. Here a function divides an `f64` by a `str`.

```text
src/com/example/units.anti:41:9: error: the operands of `/` have the types `f64` and `str`
anti check: front end: 1 file, 1 target, 0 warnings, 1 failed
```

A doc block compiles in one of two contexts. A block of a `///` comment becomes a module of its own that imports the module of the comment, so it reaches the public items alone. A `//#` note is a doc comment for the developers of a library, which the user pages leave out. A block of such a note is compiled inside a copy of the module, where the private items are in reach. A block without `fn main` is wrapped in a function. The example `let f = units.feet(2.0);` of `feet` is therefore checked against the real function.

Where it lives: `src/anti/check.c`. The front end is `antic --front-end --warnings-as-errors`, called through the driver.

## anti fmt

`anti fmt` writes every source in the canonical form. It prints the path of each file it changed. `anti fmt --check` writes nothing, prints the files that differ and exits with 1 when one does.

The canonical form:

- one tab per level of nesting
- the brace of an item body on a line of its own
- the brace of a statement block on the line of its statement
- no parentheses around a whole condition
- one statement per line
- a doc comment wrapped at 80 columns
- the names of a direct import in sorted order

The line breaks of the author stand where no rule moves them. A source before `anti fmt`:

```anti
import anti.text.{equal, Builder};
import anti.io;

/// Whether `n` is small. The doc comment of this function is longer than eighty columns, so the formatter wraps it.
fn small(n: int) -> bool {
    if (n < 10) { return true; }
    let a = 1; let b = 2;
    return (a + b) > n;
}
```

The same source after it:

```anti
import anti.text.{Builder, equal};
import anti.io;

/// Whether `n` is small. The doc comment of this function is longer than eighty
/// columns, so the formatter wraps it.
fn small(n: int) -> bool
{
	if n < 10 { return true; }
	let a = 1;
	let b = 2;
	return (a + b) > n;
}
```

The formatter sorted the two names of the import list and left the order of the two import lines. It dropped the parentheses around the condition of `if` and kept those around `a + b`, which are no whole condition. The block `{ return true; }` stays on its line, since the author wrote it there.

`anti fmt` works on the tokens of a file and not on its syntax tree. It uses the lexer of the compiler, so it formats a file that does not parse. An ordinary comment stays where it stands between two tokens.

A second run changes nothing. `anti fmt` then prints no path, and `anti fmt --check` exits with 0. The sources of the standard library and of the tests of Anti stand in this form.

Where it lives: `src/anti/fmt.c`.

## anti doc

`anti doc` writes one page per module and an index of them, from the doc comments. A `//!` comment at the top of a file documents the module, and a `///` comment documents the item that follows it.

```sh
anti doc -o docs
```

```text
doc: 1 module into docs
```

The command writes `docs/index.html` and `docs/com.example.units.html`. The page of the module:

```html
<!DOCTYPE html>
<meta charset="utf-8">
<title>com.example.units</title>
<main class="doc">
<h1>com.example.units</h1>
<p>Conversions between metres and feet.</p>
<section class="item" id="FEET_PER_METRE">
<h2>FEET_PER_METRE</h2>
<pre class="signature">pub const FEET_PER_METRE: f64 = 3.28084</pre>
<p>The feet in one metre.</p>
</section>
<section class="item" id="feet">
<h2>feet</h2>
<pre class="signature">pub fn feet(metres: f64) -&gt; f64</pre>
<p>The length <a href="#metres"><code>metres</code></a> in feet.</p>
<pre class="code">let f = units.feet(2.0);
</pre>
</section>
<section class="item" id="metres">
<h2>metres</h2>
<pre class="signature">pub fn metres(feet: f64) -&gt; f64</pre>
<p>The length <a href="#feet"><code>feet</code></a> in metres.</p>
</section>
</main>
```

The page is plain HTML with eight class names and no styling: `doc`, `index`, `item`, `signature`, `code`, `fields`, `members` and `internals`. A site gives them a style of its own.

Three facts of the page follow from how the tool reads a module.

- A signature is written from the checked interface, with each type as the source spells it. `f64` stays `f64`, and `float`, its other name, stays `float`.
- A code span that names an item of the module becomes a link to it. The parameter `metres` of `feet` links to the function `metres`, since the tool matches the name alone.
- An `anti` block of a doc comment becomes a `pre` of class `code`.

| Option | Effect |
|---|---|
| `-o <dir>` | the directory of the pages, `doc` in the build directory of `[layout]` without it |
| `--markdown` | write Markdown, with each signature and each example as an `anti` block |
| `--private` | keep the items that are not `pub` |
| `--dev` | the developer's pages: the private items and the `//#` notes of the source |
| `<file.anti>`, `<file.antl>` | document those modules. Without one, the modules of the project |

The user pages come from the public interface alone, which a library file carries. `anti doc units.antl` writes the same bytes as `anti doc` over the source, so a package without source has its documentation. `--private` and `--dev` need the source.

```sh
anti doc --dev -o docs dist/macos-arm64/dev/com/example/units.antl
```

```text
anti: dist/macos-arm64/dev/com/example/units.antl: --dev needs the source
```

A module with three `pub` items and two private ones gives 3 sections without an option and 5 with `--private` or `--dev`.

`anti build --strip-docs` leaves the doc text out of the library files, and a page from such a file holds the signatures alone.

A fence of a doc comment that nothing closes gives a warning, and the page is written:

```text
anti: one.anti: warning: the doc comment of `one` holds a fence that nothing closes
```

The Markdown page closes the open code block at the end of the comment, so the rest of the page stays text.

Where it lives: `src/anti/doc.c`, and `src/antic/docpage.c` for the page of one module.

## anti license

`anti license` prints licence notices. A notice is one line per package, with its version and its licence, and then each licence text once.

| Form | What it prints |
|---|---|
| `anti license` | the licence of antic and anti and of every component of the package: 4,016 lines for Anti 0.2.0 on macos-arm64 |
| `anti license --project` | the packages the project links, from `anti.lock` and the bundled modules it imports. It builds the project first |
| `anti license --project --notice` | writes that notice as `NOTICE.txt` in `dist/` and prints nothing |
| `anti license --from <binary>` | the notice a program or a shared library of Anti carries inside it |
| `anti license --from-archive <library>` | the licence fields of the package header in a static library of `--lib static` |

The first lines of the plain form:

```text
package antic 0.2.0 MIT
package anti 0.2.0 MIT
package anti.rt 0.2.0 0BSD
package apsl - APSL-2.0
package compiler-rt - Apache-2.0 WITH LLVM-exception
package glibc 2.35-0ubuntu3 LGPL-2.1-or-later
```

`anti license --project` in `com.example.hello` prints 19 lines. The first five:

```text
package anti.rt 0.2.0 0BSD
package com.example.units 1.2.0 MIT
package anti 0.2.0 0BSD
package com.example.hello 0.1.0 
text for anti.rt anti
```

The text of the BSD Zero Clause License follows. `com.example.units` names MIT in its manifest and no `license_text`, so the notice holds its line and no text for it. `com.example.hello` names no licence, and its line ends after the version.

`anti build` writes the same notice as `NOTICE.txt` beside every program and every shared library. The program carries it as well, in the data `anti_licenses`, and `anti license --from dist/macos-arm64/dev/hello` prints the same 19 lines from the binary alone.

```sh
anti license --from-archive libgeometry.a
```

```text
package geometry 0.0.0 
```

The archive is the library of [chapter 17]({{% relref "17-c-interop" %}}), built by antic without a package version, so its header names `0.0.0` and no licence.

Where it lives: `src/anti/licensing.c`, and `src/antic/notice.c` for the notice a build writes into a program.

## anti symbols

A release program carries no symbol data, and a trace it prints is a list of addresses. `anti symbols` keeps the symbols archives of a deployment together and turns such a trace into names and lines. It has three forms.

| Form | What it does |
|---|---|
| `anti symbols inventory --conf <config.toml> [--from <dir>] [--out <symbols.zip>]` | finds the binaries of a deployment and folds their archives into one |
| `anti symbols check --conf <config.toml> [--symbols <symbols.zip>]...` | reports per binary whether its symbols are present, stale or missing |
| `anti symbols resolve <trace.txt> --symbols <symbols.zip>...` | prints a raw trace with the function and the line of each frame |

The commands start from the runtime configuration of [chapter 15]({{% relref "15-hooks" %}}). They find the program in the directory of the configuration, and the libraries that its `plugins` key and its `[injections]` table name. Every binary is known by its build id, the 64 hex digits its notice carries. An archive belongs to a binary when the two ids are equal.

The deployment of the example is one directory with three files: the program `tracer` of `tests/anti-symbols`, its archive `tracer-symbols.zip` and the configuration `tracer.toml`.

```sh
anti symbols inventory --conf tracer.toml --out all.zip
```

```text
present ./tracer 34718ddb81a9babf1fb834873afead4b69879caa4366f7e16ed8458b99a382ed
wrote all.zip with 1 of 1 modules
```

`all.zip` holds the symbols of every binary under a directory named after its build id, and an index.

```toml
# The symbols archives of one deployment, by build id. Each entry stands
# under a directory named after its id.

[[module]]
module = 'tracer'
id = '34718ddb81a9babf1fb834873afead4b69879caa4366f7e16ed8458b99a382ed'
version = '1.2.0'
source = './tracer-symbols.zip'
```

`anti symbols check` prints the same `present` line and exits with 0. With the archive moved away it prints `missing` and exits with 1. `--symbols all.zip` gives it the inventory to read, and the binary is `present` again.

```text
missing ./tracer 34718ddb81a9babf1fb834873afead4b69879caa4366f7e16ed8458b99a382ed
```

The program prints the raw trace of `StackTrace.text()`. A `module` line names a binary with its build id and the address it was loaded at. A frame is an address, and the number of its module with the offset into it.

```text
module 0 34718ddb81a9babf1fb834873afead4b69879caa4366f7e16ed8458b99a382ed 0x000000010428c000 /home/ann/deploy/tracer
module 1 - 0x000000018a9d8000 /usr/lib/dyld
0x000000010428c714 0+0x714
0x0000000104291ee4 0+0x5ee4
0x000000018a9f84e4 1+0x204e4
```

```sh
anti symbols resolve trace.txt --symbols all.zip
```

```text
module 0 34718ddb81a9babf1fb834873afead4b69879caa4366f7e16ed8458b99a382ed 0x000000010428c000 /home/ann/deploy/tracer
module 1 - 0x000000018a9d8000 /usr/lib/dyld
0x000000010428c714 0+0x714 com.example.tracer.main tracer.anti:11
0x0000000104291ee4 0+0x5ee4 main
0x000000018a9f84e4 1+0x204e4
```

The first frame gains its function and its line from the debug link of the archive. The source calls `capture` in `inner`, three calls below `main`. The release build inlined those calls, so the frame is that of `main`, and the line is the one of the call of `capture`. The second frame is the `main` of the C runtime, which the map of the program names and no line covers. The third lies in `dyld`, whose build id no archive holds, and stays raw.

The trace is resolved by its offsets, so it may come from another machine and another run. The address a module was loaded at does not enter.

A shared library and a plugin have no symbols archive written by `anti build`. `resolve` leaves a frame of a Windows program raw, since it reads no PDB.

Where it lives: `src/anti/syms.c`, with `src/anti/symmap.c` for the map and `src/anti/zip.c` for the archives.
