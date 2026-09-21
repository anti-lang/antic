# `Error` in anti.lang: blocked

The step moves `Error` and `NoneDereference` from anti.error to a new
module anti.lang, which imports nothing, so that anti.io and anti.text
can write `may fail` without an import cycle. It stopped before any
change, on two questions that the move raises and the instruction does
not answer.

## First question: the makers that leave the class

The instruction keeps `from_errno` and `from_win32` in anti.error. Today
they are static functions of `Error`. In anti.error they become free
functions, `error.from_errno() -> *lang.Error`.

A `[provisional]` entry under "Object model" in `docs/decisions.md` says
that a static function of the error class builds an error, and every
other function that returns `*Error` reports a failure. `src/sema.c`
applies it at each call, through `is_failing` and `makes_error`. A free
function has no owner class, so every call of it must be handled. A
scratch program in `build/drive/scratch/maker/` declares a free function
that returns `*error.Error` and writes `fail from_errno_free()`. It
stops at "`from_errno_free` may fail and its error is not handled", in
`build/drive/logs/maker.log`. anti.log writes `return
failure.Error.from_errno()` twice, and the test of anti.error writes
`fail error.Error.from_errno()`.

The ways I see:

1. `from_errno` and `from_win32` stay static functions of `Error`, and
   anti.lang declares the four `extern fn` lines of errno and Win32.
   anti.error keeps `on_fatal`, `check` and the subclasses.
2. They become static functions of a subclass in anti.error, such as
   `error.SystemError.from_errno()`. The makers rule already covers a
   class below `Error` through its chain, and a caller can test `e is
   error.SystemError`.
3. The makers rule widens to the free functions of anti.error that
   return `*Error`. The compiler then knows anti.error by its path
   again, for this one rule.

## Second question: what `Error.text()` gives, and who owns it

`text(self, out: *text.Builder)` writes into a builder of anti.text,
which anti.lang cannot name. The specifications write the form
`e.text()`, which gives the text. `own` is refused on a `str`, and in
the standard library the bytes of a built `str` belong to the builder
that holds them. Nothing says where the bytes of `e.text()` live. Its
one caller is `tests/std/builder.anti`.

The ways I see:

1. The error owns them. `Error` gains a private buffer that `text()`
   fills and `destruct` frees. The text lives until the error is
   deleted or `text()` runs again. That adds a field to the layout of
   `Error`.
2. anti.lang declares a builder of its own and `text` takes it. The
   standard library then has two builders, or `text.Builder` moves to
   anti.lang, which the previous report listed as its third way out of
   the cycle.
3. `text()` gives bytes on the heap that nothing frees, as
   `Object.deserialize` does for its `str` fields under a
   `[provisional]` entry.

## What I take as settled

Unless you say otherwise, the step goes on with these:

- A module imports anti.lang like any other, with `import anti.lang`.
  That answers the question of the report on anti.error.
- The hook that `fatal` reads moves to anti.lang as an `internal`
  singleton, and `error.on_fatal` stores into it. Every module of
  `std/` is built with `--package-name anti`, so the item reaches
  anti.error. `print` and `fatal` stay on `Error` and write through
  `anti_rt_write` and `anti_rt_exit`, as anti.io does.
- `at` and `frames` wait for error origins and stack traces, which
  "Timing" places after `may fail` with tuples. This step moves
  `SourceLocation` and `StackTrace` to anti.lang in the documents and
  adds neither type nor field to the code.
- `Object` stays `anti.rt.Object` in the compiler. The documents name
  it `anti.lang.Object`, as "Namespaces" asks.

## What changed

Nothing in code, in the specifications or in `docs/decisions.md`.

## Gates

No code changed, so the build and the suites did not run. The
docs-style checker reports nothing on this report, in
`build/drive/logs/docs-style.log`.
