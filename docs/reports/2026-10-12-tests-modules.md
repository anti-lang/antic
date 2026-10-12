# The modules area of the test matrix

The step `modules` of the test run, on 2026-10-12, on `main`. It wrote the
tests of the area modules of `docs/notes/test-matrix.md`. No file under
`src/` changed.

## What the step added

- `tests/matrix/` holds 137 sources of the area: 11 programs, each in dev,
  release and `--memory-checks`, and 126 refusals of one message each.
- Three programs hold their constructs in fifteen places each: the names
  an import gives, the calls of an `extern fn` and the exported items.
- Three programs import the whole standard library, by path, under an
  alias and with a direct import. One holds the clause of the whole file,
  the cell the note listed as open.
- `tests/modules/` holds twelve trees, and twelve tests of them run in dev
  and in release. They hold every kind of item across a library file and
  the internal level. They hold one function per kind of value and the
  defaults of fields. A diamond of four modules, generics that reach private items,
  the exported items and the constants of a library complete them.
- `tests/errors/` holds 55 programs `modules_*` for the refusals at the
  boundary. A private and an internal item of sixteen kinds are among
  them, with the members of a class of a library.
- `program_abi_ctypes` takes every C type to C and back in three modes.
  `clib_ctypes` takes them the other way, in a library built with
  `--lib static`, `--lib shared` and `--bundle-runtime`.
- `plugin_kinds` calls a function of a plugin per kind of value.
- The note marks the open cell of the area and the rows of decisions 25
  and 26. Five messages left `tests/diagnostics-open.txt`, and `antic
  --soname` and `antic --strip-docs` left `tests/options-open.txt`.

The triples: a class of a library, a field of type `?T` and its default.
A class of a library, `protected` and a function without `self`. A generic
variant, the name of a module and the type arguments of a case. A type
parameter `N: int`, a constant and the name of a module. A generic of a
library, a private item and a copy the client compiles. A plugin, a local
object and a class without `destruct`. "Modules" in the note names the
test of each.

## The findings

`docs/audit/tests-findings.md` holds the rows, and
`tests/known-failures.txt` 319 lines, 33 of them new.

- T085, major. antic cannot read the library file of a class with a field
  `?int = none`.
- T086, major. A protected static function of a class of a library is
  called from outside.
- T087, minor. `pub import`, an `extern fn` in a class body and an
  interface a module does not give get a second message.
- T088, minor. The driver reports a cycle of imports without a position.
- T089, major. Decision 26: a `str` in an `extern fn` compiles, five ways.
- T090, major. Decision 25: `count` of a collection is written from
  outside, on eight collections.
- T091, major. A case of a generic variant cannot be named with its type
  arguments, under the name of a module or after `is`.
- T092, severe. A local object without `destruct` in a plugin stays
  counted as alive, so `unload` refuses for ever.
- T093, major. A plugin cannot load when its host imports a module of the
  standard library and does not use it.
- T094, major. A constant of another module is refused as the argument of
  `N: int`.
- T095, minor. A message on a copy of a library generic names a line of
  the library in the file of the client.

## Where the task and the code differ

- The task asks for one program of `tests/matrix/` per cell. A matrix
  build reads no library file. The cells across a module therefore stand
  in `tests/modules/` and run in dev and release.
- The task asks for every C type against decision 26. `c_char` is an
  integer of 8 bits with a sign on every target, by "Numeric types" in
  `docs/decisions.md`. The tests pass it as `signed char`.
- The sign of `c_wchar` and the width of `c_long` follow the target. Both
  tests compare them with what C gives, so one expected file serves.
- A function of a class with a type parameter of its own does not cross a
  library file. Neither does a type nested in a generic class. The
  overview lists both as not built, and three refusals hold what antic
  says.

## Rules the documents do not state

- A plain and a direct import of one module in one file are refused.
- An alias with a direct import compiles, and so does a plain import
  with an alias.
- A clause of the whole file stands among the imports as well.
- An expression reaches a module by its last segment or its alias, never
  by its whole path.
- A module that names a type of a module it does not import is refused,
  although it holds values of that type.
- An `extern fn` takes a slice, a `char`, a tuple, an optional value and
  an `own` parameter.
- Two modules declare one `extern fn` with two signatures.
- An `f32` and a `c_long` are no variadic arguments, and a `c_size_t` is.
- A private function of a class of a library is reported as missing, and
  a private field as private.
- `--lib static` builds a library of a module without an exported item.
- `--lib shared --bundle-runtime`, `--lib static --no-runtime` and
  `--lib static --dev` are taken without a message.
- A wrong value of `--lib` ends antic with 2.
- `export` stands before an `operator fn`, and an `export fn` takes a
  function parameter without `keep`. No test holds their header.
- A plugin that uses a module its host does not import fails to load with
  the message of the loader.
- T086, T088, T089, T090 and T095 expect messages and positions of this
  step's wording.

## Counts and checks

- Tests: 3235 before and 3508 after, by `ctest --preset host -N`.
- Host suite: 3508 of 3508 passed in 451 s, 319 of them expected
  failures. Two skip, the `mimalloc_environment` tests.
  `build/drive/logs/modules-host-suite2.log` holds the run.
- ASan: 3507 tests in two parts, 1000 in 532 s and 2526 in 396 s, all
  passed. Nineteen fixture tests ran in both parts.
  `modules-asan-suite-a.log` and `modules-asan-suite-b.log` hold them.
- UBSan: the same two parts in 477 s and 266 s, all passed, in
  `modules-ubsan-suite-a.log` and `modules-ubsan-suite-b.log`.
- The three trees built with zero warnings, in `modules-host-build15.log`,
  `modules-asan-build.log` and `modules-ubsan-build.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no expected
  file of either changed.
- The expected files of `program_abi_ctypes`, `clib_ctypes`,
  `constants_modules` and `optional_default_modules` were written before
  the first run. The others were compared with a model line by line.
- The docs-style checker reports `0 error(s), 0 warning(s)` on the note,
  the findings, `CLAUDE.md` and this report.

## Not done

- Neither VM ran the new tests. The C files of `tests/abi/` and
  `tests/clib/` were compiled for macos-arm64 alone.
- The tests across a library file do not run under `--memory-checks`.
  Seven of them gave the same output there in a run by hand.
- `anti build --soname` and `anti build --strip-docs` stay in
  `tests/options-open.txt`, with the options of the `anti` tool.
- The scripts that wrote the sources are scratch files under
  `build/scratch/modules/` and are not committed.

## Proof

`git log --oneline -3` after the push of the commits of the step, before
the commit of this report:

```text
70e96c4f State the modules tests, the counts and the suite times in CLAUDE.md
0b7e207c Write the sources of the modules area in the form of anti fmt
a8d6458f Mark the modules cells of the test matrix
```

`git rev-parse HEAD origin/main` after that push:

```text
70e96c4fa08668fc312cc98de422192b81a6bcab
70e96c4fa08668fc312cc98de422192b81a6bcab
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds the three outputs after the last push, since
the hash of this commit is not known inside it.
