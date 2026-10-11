---
title: "Tests"
description: "tests and fixtures blocks, anti test, expected output and memory checks in tests."
summary: "The `tests` and `fixtures` blocks of a module with how antic builds them and what they cost, `anti test`, how a test states the expected output, and `--memory-checks` in tests."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T18:40:00+02:00
draft: true
weight: 190
tags: [compilers, programming-languages]
keywords: [unit tests, fixtures, test runner, expected output, memory checks]
---

A module holds its tests beside its code, in a `tests` block, and the helpers of those tests in a `fixtures` block. `anti test` compiles a module with both blocks, writes a runner that calls every test, links it and runs it. Every other build drops the two blocks after parsing, so a program and a library file hold nothing of them. Every command of this chapter was run on macos-arm64 with Anti 0.2.0, and the page shows its output as the tool printed it.

## tests and fixtures blocks

### What you know

JUnit keeps a test in a class of its own, in a second source tree, and finds the methods marked `@Test` by reflection. Such a test reaches the package-private members of the class under test and none of its private ones. C has no test form in the language. A test is a second program that links the code, and it reaches a `static` function only by including the source file. The frameworks of C++ register each test from a macro before `main` runs, and a test reaches a private member through a `friend` declaration.

### The feature

`tests { }` and `fixtures { }` are blocks at module level. A module has at most one of each, and both hold functions and nothing else.

```anti
//! name: purse
import anti.io;

pub class Purse
{
	coins: [8]int,
	count: int = 0,

	pub fn new() -> Purse
	{
		return Purse { coins: [0; 8], count: 0 };
	}

	pub fn add(self, coin: int)
	{
		self.coins[self.count] = coin;
		self.count += 1;
	}

	pub fn total(self) -> int
	{
		return self.sum(self.count);
	}

	fn sum(self, n: int) -> int
	{
		let t = 0;
		for i in 0..n {
			t += self.coins[i];
		}
		return t;
	}
}

fn largest(a: int, b: int) -> int
{
	if a > b {
		return a;
	}
	return b;
}

fixtures
{
	fn filled(n: int) -> Purse
	{
		let p = Purse.new();
		for i in 0..n {
			p.add(i + 1);
		}
		return p;
	}

	fn setup()
	{
		io.println("setup");
	}

	fn teardown()
	{
		io.println("teardown");
	}
}

tests
{
	fn total_adds_every_coin()
	{
		let p = filled(4);
		let t = p.total();
		assert(t == 10);
	}

	fn sum_stops_at_n()
	{
		let p = filled(4);
		assert(p.count == 4);
		let first_two = p.sum(2);
		assert(first_two == 3);
	}

	fn largest_takes_the_second()
	{
		let m = largest(2, 7);
		assert(m == 7, "largest gives the larger one");
	}
}
```

The rules of the two blocks follow.

- Every `fn` of `tests` is a test. It takes no parameters and returns nothing, and its name is the name the report prints. The tests of a module run in the order the block writes them.
- Every `fn` of `fixtures` is a helper. It takes any parameters and returns any type. A test and another fixture of the module call it by its name. The runner never calls one by itself.
- `setup` and `teardown` are two names of `fixtures`. The runner calls `setup` before every test of the module and `teardown` after every test that returned. Both take no arguments, since the runner passes none.
- Both blocks see every private item of their module. `sum_stops_at_n` reads the private field `count` and calls the private function `sum` of `Purse`. `largest` is private to the module, and its test calls it. A class of another module keeps its four levels.
- A test states what it expects with `assert`. A condition that calls a function is the warning `assert-call`, so a test binds the result to a local first, as `t`, `first_two` and `m` above.
- A test calls another test as it calls any function of the module.

The checker refuses a second block of either kind.

```anti refused
//! error: a module has at most one `tests` block
tests
{
	fn first()
	{
	}
}

tests
{
	fn second()
	{
	}
}
```

A block holds functions alone. A type that a test needs stands at module level.

```anti refused
//! error: `tests` holds functions and nothing else
tests
{
	struct Point { x: int, y: int }
}
```

A test takes no parameter and returns nothing. The same message refuses a test marked `may fail`, since such a function returns an error.

```anti refused
//! error: a `tests` function takes no parameters and returns nothing, because the runner calls it
tests
{
	fn adds(n: int)
	{
	}
}
```

```anti refused
//! error: a `tests` function takes no parameters and returns nothing, because the runner calls it
fn half(n: int) -> int may fail
{
	if n % 2 != 0 {
		fail "odd";
	}
	return n / 2;
}

tests
{
	fn halves() may fail
	{
		let h = try half(4);
		assert(h == 2);
	}
}
```

A test of a failing function takes a handler. `let h = half(4) catch fatal;` ends the run with the error when `half` fails.

`setup` and `teardown` belong to `fixtures`.

```anti refused
//! error: `setup` and `teardown` belong to `fixtures`, which runs them around every test
tests
{
	fn setup()
	{
	}
}
```

Code outside the two blocks cannot call into them. The test build accepts the module below, and every other build refuses it, since the fixture is gone before the checker runs.

```anti refused
//! error: unknown name `two`
fixtures
{
	fn two() -> int
	{
		return 2;
	}
}

fn doubled() -> int
{
	return two() * 2;
}
```

Three more cases follow.

- A `setup` or a `teardown` with a parameter compiles. The runner then fails to compile, with "`setup` takes 1 argument, found 0" at a line of the runner.
- A block inside a function is a syntax error, "expected an expression", since `tests` is a keyword.
- `pub` before a function of a block is accepted and changes nothing.

### How antic builds it

The parser turns each function of a block into an ordinary function of the module and records the block it came from. The driver then takes one of two paths.

- Without `--tests` it removes every function that carries a block from the item list, before the checker runs. The `allow` and `unchecked` clauses of those functions go with them. No later pass sees a test.
- With `--tests` it keeps them and makes each one public, so that a runner in another module calls them by name.

`anti test` passes `--tests` on every compile of a run. A test is then a function like any other. The dev module of the block `purse` holds this one.

```llvm excerpt=purse target=linux-x86_64 mode=dev option=tests
define hidden void @purse.largest_takes_the_second() #0 {
b0:
...
  %v0 = call i64 (i64, i64) @purse.largest(i64 2, i64 7)
...
  %v2 = icmp eq i64 %v1, 7
...
  br i1 %v5, label %b2, label %b1
...
b1:
...
  call void (ptr, i64) @anti_rt_assert_failed(ptr %v6, i64 61)
  unreachable
...
b2:
  ret void
```

- The test is `purse.largest_takes_the_second`, its module path and its name. `hidden` is the visibility of a function of a dev module that other objects of the program call.
- `%v0` is the call of `largest`, and `%v2` compares its result with 7.
- The branch goes to `b2` when the assertion holds and to `b1` when it fails.
- `b1` passes the text of the assertion and its length, 61 bytes, to `anti_rt_assert_failed`. That function never returns.

The runner is Anti source that `anti test` writes, one file per module. The runner of the module `com.example.change` of the next section follows. It is `anti/test/com_example_change.anti` under the work directory.

```text
//! The runner `anti test` wrote for one module.
import com.example.change;

extern fn printf(format: ?*byte, ...) -> c_int;
extern fn anti_rt_test_running(name: ?*byte, length: int);

fn main() -> int
{
	anti_rt_test_running("com.example.change.ten_is_two_fives".ptr, 35);
	change.ten_is_two_fives();
	printf("ok %.*s\n".ptr, 35 as c_int, "com.example.change.ten_is_two_fives".ptr);
	anti_rt_test_running("com.example.change.seven_is_three_twos".ptr, 38);
	change.seven_is_three_twos();
	printf("ok %.*s\n".ptr, 38 as c_int, "com.example.change.seven_is_three_twos".ptr);
	anti_rt_test_running("com.example.change.nine_is_three_threes".ptr, 39);
	change.nine_is_three_threes();
	printf("ok %.*s\n".ptr, 39 as c_int, "com.example.change.nine_is_three_threes".ptr);
	anti_rt_test_running(none, 0);
	return 0;
}
```

The module path of a runner is `anti.test.<module>`, with each dot of the module written as `_`. antic refuses a library of a program under the root `anti.`, so the two never clash. A module with `setup` gets the call `<module>.setup();` before each test and `<module>.teardown();` after it.

`anti_rt_test_running` stores the address and the length of the name in two words of the runtime. `anti_rt_assert_failed` reads them. With a name set it prints `FAIL <name>` and then the text of the assertion to the standard error, and calls `abort`. A test that returns reaches the `printf` that prints `ok <name>`.

The two modes build the runner in two ways.

| | Dev run | Run with `--release` |
|---|---|---|
| The module under test | its library file and its object | its library file |
| The modules it imports | one object per library file, the imports of the imports included | read from their library files |
| The runner | one object, linked with the objects above | one whole program of the runner and every module it reaches |
| Assertions and dev checks | on | off |

Where it lives: `test_block` and `parser_parse` of `src/antic/parser.c`, `drop_test_blocks` of `src/antic/driver.c`, `sema_check_test_block` of `src/antic/sema_stmt.c` and `from_test_block` of `src/antic/sema_call.c`. `write_runner`, `compile_imports` and `test_run` of `src/anti/test.c` build a run. `anti_rt_test_running` and `anti_rt_assert_failed` are in `src/rt/assert.c`.

### What it costs

A build without `--tests` pays nothing for the two blocks. The module `purse` was compiled with `-S` for linux-x86_64, once as written and once with both blocks deleted. The two assembly files are equal byte for byte in dev mode and in release mode. So are the two library files that `antic -c` writes, at 18212 bytes each. The test `anti_test` of the suite checks that no name of a block reaches the assembly or the library file.

A test build holds what the excerpt and the runner show.

| Cost | Count |
|---|---|
| Code of a test | one function per test and per fixture, as for any function of the module |
| An `assert` of a dev run | one compare, one branch and one text in the read-only data. The call of `anti_rt_assert_failed` runs on the failing path alone |
| An `assert` of a run with `--release` | nothing. The condition is removed with it |
| The runner, per test | one call of `anti_rt_test_running`, one call of the test and one `printf`. One call of `setup` and one of `teardown` where the module declares them |
| The runtime | two words, 16 bytes, for the name of the running test |
| Heap allocations of the runner | none of its own. `printf` belongs to the C library |
| Atomic operations and locks | none |
| Processes | one per module that has a test |

The text of an assertion holds the file, the line and the condition or the message. The one of the excerpt is 61 bytes and a terminator.

A dev run calls antic once for the library file of each module and once for its object. For each module with a test it adds one call per imported library file and one call that compiles and links the runner. A dev run keeps no cache: every run compiles each of these again. The page states no time for a run, since `tests/bench` holds no program for one.

### Pitfalls

For a Java programmer:

- No annotation marks a test and no reflection finds one. Every function of `tests` is a test, and the runner calls each by its name in the order written.
- A failed assertion ends the process of its module. The later tests of that module do not run, and `teardown` does not run after the failure.
- `assert` prints the text of its condition and not the two values. `assert(n == 4)` reports `n == 4`, without what `n` held.
- A test reaches every private member of its module without a second class in the same package.

For a C programmer:

- A test calls a function that is private to its module without including a source file, since the block is part of the module.
- An assertion with a call in its condition is a warning, and the page of a test binds the result first. Under `--release` the assertion is gone, as under `NDEBUG`.
- No test is linked into a program by accident. A build that is not a test build holds none of the code.

For a C++ programmer:

- A fixture is a function and not a class. `setup` and `teardown` hold no members, so state that two tests share stands in a module-level variable or is returned by a fixture.
- No `friend` declaration is needed for a private member of a class of the same module.
- Nothing unwinds when an assertion fails. A `defer` and a `destruct` of the test do not run.

## anti test

`anti test` takes the source files of the modules to test and the roots that give each file its module path.

```sh
anti test -I src src/com/example/purse.anti
```

| Argument | Meaning |
|---|---|
| `<file.anti>...` | the modules of the run. Without a file the command prints "anti: test takes one or more .anti files" and exits with 2 |
| `-I <dir>` | a root. The module path of a file is its path under the root, so `src/com/example/purse.anti` under `-I src` is `com.example.purse` |
| `--release` | builds each runner as a whole program with the assertions and the dev checks off |
| `--memory-checks` | builds every module with `--memory-checks` and reads each report through the symbolizer |
| `--work <dir>` | the directory of the files of the run, `build/tests` by default |
| `--runtime <dir>`, `--llvm-mc <path>` | the runtime archive and the assembler, for a build of Anti itself |

The root is compared with the path as written. `-I src` matches `src/com/example/purse.anti` and does not match `./src/com/example/purse.anti`, whose module is then `purse` alone.

The command reads `anti.toml` of the current directory when there is one. It takes the package name from it, and the providers of the `[inject]` table with those of `[inject.test]` laid over them, which chapter 15 describes. It reads no `[layout]`: the files of a run are the ones the command line names.

### A run

The module of the first section stands in a project as `src/com/example/purse.anti`. The run prints the lines of `setup` and `teardown`, one `ok` line per test and one line of totals.

```text
setup
teardown
ok com.example.purse.total_adds_every_coin
setup
teardown
ok com.example.purse.sum_stops_at_n
setup
teardown
ok com.example.purse.largest_takes_the_second
anti test: 3 tests in 1 modules, 0 modules failed
```

The `ok` lines and the totals go to the standard output. The exit status is 0 when no module failed and 1 otherwise. The work directory holds these files after the run.

| File under `build/tests` | Contents |
|---|---|
| `com/example/purse.antl` | the library file of the module, with its tests in it |
| `com.example.purse.o` | the object of the module |
| `anti/test/imports/com_example_purse/1.o` | the object of `anti.io`, which the module imports |
| `anti/test/com_example_purse.anti` | the runner |
| `com_example_purse.runner.o`, `com_example_purse.runner` | the object of the runner and the linked program |

Every module that a module of the run imports is named in the same run, or stands as a library file under a root. The standard library is found in the runtime archive. A run of `tally.anti` alone, which imports `com.example.purse`, stops with "cannot find module `com.example.purse`".

### A failed assertion

The module `src/com/example/change.anti` holds three tests, and the second one states a wrong result.

```anti
fn coins(amount: int, coin: int) -> int
{
	return amount / coin;
}

tests
{
	fn ten_is_two_fives()
	{
		let n = coins(10, 5);
		assert(n == 2);
	}

	fn seven_is_three_twos()
	{
		let n = coins(7, 2);
		assert(n == 4);
	}

	fn nine_is_three_threes()
	{
		let n = coins(9, 3);
		assert(n == 3);
	}
}
```

```sh
anti test -I src src/com/example/change.anti
```

The standard output of the run holds two lines.

```text
ok com.example.change.ten_is_two_fives
anti test: 3 tests in 1 modules, 1 module failed
```

The standard error holds three lines.

```text
FAIL com.example.change.seven_is_three_twos
com/example/change.anti:17: assertion failed: n == 4
antic: build/tests/com_example_change.runner ended by signal 6
```

The first line names the test and the second the file, the line and the condition. The file is the path under the root. The third line reports the `abort` of the runner, and its prefix is `antic` although the tool is `anti`. The third test did not run. The totals count the tests the modules declare, three here, and not the tests that ran.

A run of two modules starts one process per module, so the failure of one module leaves the tests of the other running. The run below reports the failure of `change` and all three tests of `purse`.

```sh
anti test -I src src/com/example/change.anti src/com/example/purse.anti
```

```text
anti test: 6 tests in 2 modules, 1 module failed
```

### Other ends of a test

A test also ends its module by a trap of a dev check and by an error that reaches `fatal`. The module `src/com/example/shelf.anti` shows both.

```anti
import anti.lang;

fn pick(items: []int, i: int) -> int
{
	return items[i];
}

fn half(n: int) -> int may fail
{
	if n % 2 != 0 {
		fail "odd";
	}
	return n / 2;
}

tests
{
	fn half_of_four()
	{
		let h = half(4) catch fatal;
		assert(h == 2);
	}

	fn fourth_of_three()
	{
		let a: [3]int = [1, 2, 3];
		let v = pick(a[0..3], 3);
		assert(v == 0);
	}
}
```

The dev run stops at the bounds check of `pick`. The standard error holds the message of the check and no `FAIL` line, since the check does not read the name of the test. The last `ok` line of the standard output shows which test ran before it.

```text
com/example/shelf.anti:5: index out of bounds: index 3, length 3
antic: build/tests/com_example_shelf.runner ended by signal 6
```

With `half(3)` in the first test, the error reaches `fatal`. The run prints the origin and the text of the error, `com/example/shelf.anti:11:3: odd`. A dev run adds the raw frames of the stack trace, which `anti symbols resolve` of chapter 18 reads. The runner exits with a status other than 0, and the module counts as failed.

| End of a test | Standard error of a dev run | Later tests of the module |
|---|---|---|
| It returns | nothing | run |
| A failed `assert` | `FAIL <module>.<test>`, then the file, the line and the text | do not run |
| A trap of a dev check | the message of the check | do not run |
| An error that reaches `fatal` | the origin and the text of the error, then the raw frames | do not run |

### Runs with --release

`anti test --release` builds each runner as a release program with the assertions and the dev checks off. Both modules above pass.

```sh
anti test --release -I src src/com/example/change.anti src/com/example/shelf.anti
```

```text
ok com.example.change.ten_is_two_fives
ok com.example.change.seven_is_three_twos
ok com.example.change.nine_is_three_threes
ok com.example.shelf.half_of_four
ok com.example.shelf.fourth_of_three
anti test: 5 tests in 2 modules, 0 modules failed
```

`seven_is_three_twos` passes because its assertion is removed. `fourth_of_three` reads the word after its array, since the bounds check is removed as well. A release run therefore shows that the optimized code of every test runs to its end, and it checks no assertion. An error that reaches `fatal` still fails the module.

### Modules with main

A dev run compiles each module into an object. For a module that declares `main`, antic goes on to link that object as a program, without the objects of its imports. The link fails with an undefined symbol for each function of another module, and the run ends with status 1 and no line of totals.

A run with `--release` tests such a module. The runner is the program, and the `main` of the module under test is a function that nothing calls. A project whose tests run in dev mode keeps `main` in a module of its own, with the code under test in the modules it imports.

Where it lives: `test_command` of `src/anti/main.c` reads the command line, and `test_run` of `src/anti/test.c` drives the run. `unit_read` of `src/anti/units.c` gives each file its module path through `modpath_of_source` of `src/antic/modpath.c`.

## Expected output

`anti test` compares no output. A test states an expected value with `assert`, and a text is a value. `==` on two `str` compares their bytes.

```anti
//! name: labels
fn label(total: int) -> str
{
	if total > 10 {
		return "full";
	}
	return "light";
}

tests
{
	fn label_of_ten_is_light()
	{
		let same = label(10) == "light";
		assert(same);
	}
}
```

The test of a function that prints is a run of the program. `anti run` writes the standard output of the program and nothing else to its own standard output, so a shell compares it with a file.

```sh
anti run > out.txt
cmp out.txt expected.txt
```

`anti new` writes an empty `test/` directory, and `[layout]` of the manifest names it. `anti fmt` and `anti check` read the sources under it. `anti test` does not read it and runs no program against an expected output.

### The suite of Anti

The suite of the compiler states the expected output of a program in a file. A test of `tests/programs/` is a pair: `<name>.anti` and `<name>.expected`. The first line of the expected file is `exit N`, the exit status. Every byte after that line is the expected standard output. `by_reverse.expected` holds three lines.

```text
exit 0
constant: 9 6 3 0
forward: 0 3 6 9
```

`tests/run_program.cmake` builds the program in release mode, runs it and compares both. It reads the output as the bytes of a file, so a CR and a NUL count. antic must print nothing during the build: a warning fails the test unless the test names it. An optional `<name>.args` holds one command-line argument per line.

The pages of this guide use a third form. A block that declares `main` is built and run, and the `text output` block after it holds its whole output. The test `guide_examples` compares the two through `tests/run_document_examples.cmake`.

## Memory checks in tests

`anti test --memory-checks` compiles every module of the run with the option of chapter 12 and with the lines of `-g`. Each runner then reports a use after free, a double free, an access outside a heap block and, at its exit, the blocks it lost. The module `src/com/example/squares.anti` loses one block.

```anti
import anti.lang;

fn squares(n: int) -> *int
{
	let p = alloc(int, n) catch fatal;
	for i in 0..n {
		p[i] = i * i;
	}
	return p;
}

tests
{
	fn last_square_is_nine()
	{
		let p = squares(4);
		let v = p[3];
		assert(v == 9);
	}

	fn first_square_is_zero()
	{
		let p = squares(4);
		let v = p[0];
		free(p);
		assert(v == 0);
	}
}
```

Without the option both tests pass and the run exits with 0. With it the run prints both `ok` lines, and the totals name one failed module.

```sh
anti test --memory-checks -I src src/com/example/squares.anti
```

```text
ok com.example.squares.last_square_is_nine
ok com.example.squares.first_square_is_zero
anti test: 2 tests in 1 modules, 1 module failed
```

The standard error holds the report of the leak checker. The addresses differ from run to run, and the page leaves out the frames of the C runtime.

```text
==12093==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 32 byte(s) in 1 object(s) allocated from:
    #1 0x0001023cc9cc in com.example.squares.squares squares.anti:7
    #2 0x0001023ccad0 in com.example.squares.last_square_is_nine squares.anti:18
    #3 0x0001023cc8f0 in anti.test.com_example_squares.main com_example_squares.anti:10

SUMMARY: AddressSanitizer: 32 byte(s) leaked in 1 allocation(s).
```

The report comes at the exit of the runner, after every test of the module. It carries no `FAIL` line. The frames name the test: frame 2 is `last_square_is_nine` at the line of its call of `squares`, and frame 1 is the `alloc`. Frame 3 is the runner.

The facts of a checked run follow.

- A use after free and a double free end the runner at the access, with the frames of the access, of the `free` and of the `alloc`. The later tests of the module do not run.
- The leak check runs once per runner, so it reports the leaks of all tests of a module together.
- `--release --memory-checks` checks the optimized code. The optimizer inlines a small test into the runner, and frame 2 above then reads `anti.test.com_example_squares.main squares.anti:18`.
- A dev run keeps its bounds checks and its assertions beside the memory checks.
- The limits of the option hold: windows-arm64 refuses it, and the Windows runtime reports no leaks. The five programs of the measured table of chapter 12 ran 8 to 53 times slower with the option.

Where it lives: `base_options` of `src/anti/test.c` sets the option and `-g` for every compile, and `run_unit` runs the program through `memreport_run` of `src/anti/memreport.c`. The instrumentation is in `src/antic/memcheck.c`.
