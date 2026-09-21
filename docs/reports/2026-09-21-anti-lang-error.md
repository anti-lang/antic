# `Error` in anti.lang

The step moves `Error` and `NoneDereference` from anti.error to a new
module anti.lang, which imports nothing, so that anti.io and anti.text
can write `may fail` without an import cycle. It stopped twice on
questions, which Eddie answered, and once for a verification of the
tree. `ee0c154` builds `transient` fields and `7141ff9` makes the move.

## What was done

- `ee0c154`. A `transient` field of type `?*T` or `?fn(...)` holds
  derived state. `dup` writes `none` into the copy, and the field list
  of the descriptor leaves the field out, so the default `equals`,
  `hash` and `serialize` skip it. `transient own` is refused. The
  library file carries the bit, and `ANTL_VERSION` rose to 32.
- `7141ff9`, `std/anti/lang.anti`. It holds `Error` with `code`,
  `message`, `protected own cause` and `transient text_cache: ?*byte`,
  `NoneDereference`, and the `internal singleton class Fatal` whose
  `hook` `fatal` reads. `text()` forms `error N: message`, or the
  message alone for code 0, with each cause after `  caused by: `, into
  `text_cache` on its first call, through its own `put_int` and
  `put_text`. `destruct` frees the buffer. `print` and `fatal` write
  through `anti_rt_write` and `anti_rt_exit`.
- `std/anti/error.anti` imports anti.lang alone. It holds `SystemError`
  (inherits `Error`, `pub errno: int`, `from_errno`, `from_win32`),
  `on_fatal` and `check`, and the errno and Win32 `extern fn` lines.
- The compiler names the module and its two classes once, as
  `LANG_MODULE`, `LANG_ERROR` and `LANG_NONE_DEREFERENCE` in
  `src/types.h`, with `types_is_lang_error`. The checker and the header
  writer read them there. The messages name anti.lang.
- anti.os, anti.reflect, anti.args and anti.toml import anti.lang, and
  anti.log also imports anti.error for `SystemError.from_errno`. lang
  heads `ANTIC_STD_MODULES`. Every test names `lang.Error`, and the
  module tests list `lang` for dev mode.
- New tests: `tests/std/lang.anti` for the text, the cache, `dup`,
  `equals`, `hash`, `serialize`, the lowest `int` and `NoneDereference`,
  and `tests/std/fatal.anti` for the hook across modules. The test of
  anti.error covers `SystemError` and a `dup` across the boundary.

## What the std-may-fail-error step left, and what was reversed

It treated anti.error as the base in four places, and the move reverses
each of them:

1. The module comment "The error convention of the language" moved to
   anti.lang.
2. The status lines of the syntax overview and `CLAUDE.md` name
   anti.lang.
3. The examples that spelled `error.Error` spell `lang.Error`.
4. anti.error no longer imports anti.io and anti.text, which that step
   left in place.

Nothing it did conflicted with the move.

## The link failure of `get`

`declare_get` gave the generated `get` the symbol `get`, not `T.get`.
Inside one module the call reached it by IR index, so it never showed.
From anti.error the call names `anti.lang.Fatal.get`, which did not
exist. `get` now has the symbol `T.get`, as every function of a body
does. `std_fatal` and `std_builder` failed on the link and now pass.

## The emit-identity manifest

60 lines changed, six targets each of `construct_args`, `may_fail`,
`may_fail_table`, `nullable`, `object_model`, `one_instance`,
`out_place_forms`, `out_slot`, `out_through_pointer` and `tuples`. The
old assembly came from `ee0c154` built in `build/`, with the move
stashed and then restored byte for byte. After mapping `anti.error.` and
`A4anti5error_` to anti.lang, the program symbols of every file change
in two ways alone:

- The slot of `fatal` moves from 112 to 96, two fewer, because
  `from_errno` and `from_win32` left the table of `Error`.
- `get` becomes `Config.get` in `object_model` and `one_instance`, and
  that is all of `one_instance`.

The anti.lang symbols change as the move gives: `Error` grows from 40
to 48 bytes, `new` stores `none` at 40, `copy` writes zero there, the
descriptor lists 12 functions and points at `Error.destruct`, and the
formatting routines replace the calls into anti.io and anti.text.

## A defect left in place

`lang.Fatal.get().hook.store(f)` is refused across modules with
"`hook` is atomic, so it is read with `load()` and written with
`store(v)`", and the same call inside one module is accepted. Its probe
in `atomic_call` fails on the chained call. `on_fatal` binds the
singleton to a local first, with a comment. The repro:

```anti
import anti.lang;

pub fn two(f: fn())
{
	lang.Fatal.get().hook.store(f);
}
```

compiled with `antic -c --anti-internal --package-name anti`.

## Verification against `7141ff9`

```console
$ git ls-tree --name-only 7141ff9 std/anti/lang.anti
std/anti/lang.anti
$ git show 7141ff9:std/anti/lang.anti | grep -n "^import"
(exit 1)
$ git grep -n "class Error" 7141ff9 -- std/
7141ff9:std/anti/lang.anti:25:pub class Error
$ git show 7141ff9:std/anti/error.anti | grep -n "^import"
8:import anti.lang;
$ git grep -nE "anti\.error\.|(error|failure)\.Error\b|[^m]Error\.from_(errno|win32)" \
    7141ff9 -- src std tests tools rt docs CLAUDE.md ':!docs/reports' \
    ':!docs/work-order-completion.md'
(exit 1)
```

`import anti.error` stands in `std/anti/log.anti` and in the tests of
`builder`, `error` and `fatal`, each for a convenience.
`SourceLocation` and `StackTrace` are named in the documents as
anti.lang and come into the code with error origins.

## Provisional entries

`transient` as a contextual word on `?*T` and `?fn(...)` alone, and
its form as an absent record. `errno` and `code` both hold the system's
number. The text format. The cached text read up to its NUL, and the
message alone when memory runs out. `Object` and `Job` still named
under `anti.rt` in the code.

## Gates

Each configured with `--fresh`, which keeps the pinned tools in
`build/`. Host: 519 of 519 passed. ASan: 518 of 518, with no
AddressSanitizer report. UBSan: 518 of 518, with no runtime error. The
build gives no warning of the compiler. ld prints the three
`-lto_library` notes it printed before the session. The docs-style
checker reports nothing on the documents, `CLAUDE.md` and the comments
of the changed `.anti` files. Logs under `build/drive/logs/`, `gate-*`.
